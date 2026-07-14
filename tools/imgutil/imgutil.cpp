/*
	imgutil — Disk image utility for classic Macintosh APM images

	Inspects, extracts partitions from, and assembles Apple Partition
	Map (APM) disk images used by the emulator.  Understands DDR,
	partition map entries, Apple_Driver43, and HFS MDB structures.
*/

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "driver43_data.h"

namespace fs = std::filesystem;

// GCC 13 lacks <print>, so use std::format + fputs
template<typename... Args>
static void println(std::FILE *stream, std::format_string<Args...> fmt, Args&&... args)
{
    auto s = std::format(fmt, std::forward<Args>(args)...);
    s += '\n';
    std::fputs(s.c_str(), stream);
}

template<typename... Args>
static void println(std::format_string<Args...> fmt, Args&&... args)
{
    println(stdout, fmt, std::forward<Args>(args)...);
}


/* ── Big-endian read/write helpers ─────────────────── */

static uint16_t get16(const uint8_t *p)
{
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}

static uint32_t get32(const uint8_t *p)
{
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8)  |
            static_cast<uint32_t>(p[3]);
}

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = static_cast<uint8_t>(v >> 8);
    p[1] = static_cast<uint8_t>(v);
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = static_cast<uint8_t>(v >> 24);
    p[1] = static_cast<uint8_t>(v >> 16);
    p[2] = static_cast<uint8_t>(v >> 8);
    p[3] = static_cast<uint8_t>(v);
}

/* ── APM constants and structures ──────────────────── */

static constexpr uint16_t kDDRSignature = 0x4552; // "ER"
static constexpr uint16_t kPMSignature  = 0x504D; // "PM"
static constexpr uint16_t kMDBSignature = 0x4244; // HFS MDB
static constexpr uint32_t kBlockSize    = 512;

// One driver descriptor from the DDR driver table.
struct DriverEntry {
    uint32_t block;    // physical start block
    uint16_t size;     // size in 512-byte blocks
    uint16_t type;     // OS type (1 = MacOS)
};

// A single partition map entry (name, type, location).
struct PartInfo {
    uint32_t    startBlock;
    uint32_t    blockCount;
    std::string name;
    std::string type;
};

// Parsed contents of block 0 (Device Driver Record).
struct DiskInfo {
    uint16_t    blockSize;
    uint32_t    blockCount;
    uint16_t    driverCount;
    DriverEntry drivers[8]; // max 8 driver entries in DDR
};

// HFS volume metadata extracted from the Master Directory Block.
struct VolumeInfo {
    std::string name;
    uint32_t    crDate = 0; // Mac epoch
    uint32_t    mdDate = 0;
    bool        valid  = false;
};

/* ── File I/O ──────────────────────────────────────── */

static bool read_bytes_at(std::ifstream &f, uint64_t offset,
                          uint8_t *buf, size_t len)
{
    f.seekg(static_cast<std::streamoff>(offset));
    f.read(reinterpret_cast<char *>(buf), static_cast<std::streamsize>(len));
    return f.good();
}

/* ── Format detection ──────────────────────────────── */

enum class ImageFormat { Unknown, APM, HFS };

/*
	Distinguish APM (DDR + partition map) from bare HFS (MDB at offset
	1024) by probing magic signatures.  Rewinds the stream.
*/
static ImageFormat detect_format(std::ifstream &f, uint64_t fileSize)
{
    if (fileSize < 1536) return ImageFormat::Unknown; // need at least 3 blocks

    uint8_t block0[512];
    if (!read_bytes_at(f, 0, block0, 512))
        return ImageFormat::Unknown;

    // Check for DDR signature at offset 0
    if (get16(block0) == kDDRSignature)
    {
        // Verify PM entry at block 1
        uint8_t block1[512];
        if (read_bytes_at(f, 512, block1, 512) && get16(block1) == kPMSignature)
            return ImageFormat::APM;
    }

    // Check for HFS MDB at offset 1024
    uint8_t mdb[2];
    if (read_bytes_at(f, 1024, mdb, 2) && get16(mdb) == kMDBSignature)
        return ImageFormat::HFS;

    return ImageFormat::Unknown;
}

/* ── DDR / Partition map parsing ───────────────────── */

// Decode the Device Driver Record from raw block 0 bytes.
static DiskInfo parse_ddr(const uint8_t *block0)
{
    DiskInfo di{};
    di.blockSize    = get16(block0 + 2);
    di.blockCount   = get32(block0 + 4);
    di.driverCount  = get16(block0 + 16);
    for (int i = 0; i < di.driverCount && i < 8; i++)
    {
        const uint8_t *e = block0 + 18 + i * 8;
        di.drivers[i].block = get32(e);
        di.drivers[i].size  = get16(e + 4);
        di.drivers[i].type  = get16(e + 6);
    }
    return di;
}

/*
	Read all partition map entries starting at block 1.  The first
	entry's mapEntries field gives the total count.  Stops early if
	a block lacks the PM signature.
*/
static std::vector<PartInfo> parse_partition_map(std::ifstream &f)
{
    std::vector<PartInfo> parts;
    uint8_t buf[512];

    // Read first PM entry to get total count
    if (!read_bytes_at(f, 512, buf, 512)) return parts;
    if (get16(buf) != kPMSignature) return parts;

    uint32_t totalEntries = get32(buf + 4);

    for (uint32_t i = 0; i < totalEntries; i++)
    {
        if (!read_bytes_at(f, (1 + i) * 512, buf, 512)) break;
        if (get16(buf) != kPMSignature) break;

        PartInfo pi;
        pi.startBlock = get32(buf + 8);
        pi.blockCount = get32(buf + 12);
        pi.name.assign(reinterpret_cast<char *>(buf + 16), 32);
        pi.name.resize(std::strlen(pi.name.c_str())); // trim nulls
        pi.type.assign(reinterpret_cast<char *>(buf + 48), 32);
        pi.type.resize(std::strlen(pi.type.c_str())); // trim nulls
        parts.push_back(std::move(pi));
    }
    return parts;
}

/* ── HFS MDB reading ──────────────────────────────── */

// Convert Mac epoch (seconds since 1904-01-01 00:00:00) to YYYY-MM-DD
static std::string mac_date_str(uint32_t macTime)
{
    if (macTime == 0) return "unknown";
    // Mac epoch is 2082844800 seconds before Unix epoch
    constexpr int64_t kMacToUnixDelta = 2082844800;
    auto tp = std::chrono::system_clock::from_time_t(
        static_cast<time_t>(macTime) - kMacToUnixDelta);
    auto days = std::chrono::floor<std::chrono::days>(tp);
    std::chrono::year_month_day ymd{days};
    return std::format("{:%Y-%m-%d}", ymd);
}

/*
	Read HFS Master Directory Block at partitionOffset + 1024.
	Extracts volume name (Pascal string) and creation/modification dates.
*/
static VolumeInfo read_mdb(std::ifstream &f, uint64_t partitionOffset)
{
    VolumeInfo vi{};
    uint8_t mdb[64]; // only need first 64 bytes of MDB
    if (!read_bytes_at(f, partitionOffset + 1024, mdb, 64))
        return vi;
    if (get16(mdb) != kMDBSignature)
        return vi;

    vi.crDate = get32(mdb + 2);
    vi.mdDate = get32(mdb + 6);

    // Volume name: Pascal string at MDB offset +36
    uint8_t nameLen = mdb[36];
    if (nameLen > 27) nameLen = 27;
    vi.name.assign(reinterpret_cast<char *>(mdb + 37), nameLen);
    vi.valid = true;
    return vi;
}

/* ── Subcommands ───────────────────────────────────── */

// Print DDR, driver, partition map, and HFS volume info for an image.
static int cmd_info(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::fputs("Usage: imgutil info <file>\n", stderr);
        return 1;
    }
    fs::path path(argv[2]);
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) { println(stderr, "imgutil: cannot open {}", argv[2]); return 1; }
    uint64_t fileSize = static_cast<uint64_t>(f.tellg());

    auto fmt = detect_format(f, fileSize);

    if (fmt == ImageFormat::APM)
    {
        uint8_t block0[512];
        read_bytes_at(f, 0, block0, 512);
        auto di = parse_ddr(block0);
        auto parts = parse_partition_map(f);

        println("{}: APM disk ({} KB, {} blocks \u00d7 {})",
                     path.filename().string(),
                     fileSize / 1024, di.blockCount, di.blockSize);

        for (int i = 0; i < di.driverCount; i++)
        {
            auto &drv = di.drivers[i];
            const char *cpuType = (drv.type == 1) ? "68000" : "unknown";
            println("  Driver: Apple_Driver43 at block {} ({} blocks, {})",
                         drv.block, drv.size, cpuType);
        }

        println("  Partitions:");
        for (size_t i = 0; i < parts.size(); i++)
        {
            auto &p = parts[i];
            uint64_t sizeKB = static_cast<uint64_t>(p.blockCount) * 512 / 1024;
            println("    {}: \"{}\"{:<{}}  {:<20} block {}, {} blocks ({} KB)",
                         i + 1, p.name,
                         "", (p.name.size() < 12 ? 12 - p.name.size() : 0),
                         p.type, p.startBlock, p.blockCount, sizeKB);

            if (p.type == "Apple_HFS")
            {
                uint64_t partOffset = static_cast<uint64_t>(p.startBlock) * 512;
                auto vi = read_mdb(f, partOffset);
                if (vi.valid)
                {
                    println("       Volume: \"{}\" (created {}, modified {})",
                                 vi.name,
                                 mac_date_str(vi.crDate),
                                 mac_date_str(vi.mdDate));
                }
            }
        }
    }
    else if (fmt == ImageFormat::HFS)
    {
        auto vi = read_mdb(f, 0);
        println("{}: HFS partition ({} KB)",
                     path.filename().string(), fileSize / 1024);
        if (vi.valid)
        {
            println("  Volume: \"{}\" (created {}, modified {})",
                         vi.name,
                         mac_date_str(vi.crDate),
                         mac_date_str(vi.mdDate));
        }
    }
    else
    {
        println(stderr, "imgutil: {}: unknown format", argv[2]);
        return 1;
    }
    return 0;
}

// Copy a single partition out of an APM image to a standalone file.
static int cmd_extract(int argc, char *argv[])
{
    if (argc < 4)
    {
        std::fputs("Usage: imgutil extract <file> <index> [-o output]\n", stderr);
        return 1;
    }

    fs::path inputPath(argv[2]);
    int index = 0;
    try { index = std::stoi(argv[3]); }
    catch (...) { println(stderr, "imgutil: invalid index '{}'", argv[3]); return 1; }
    if (index < 1) { std::fputs("imgutil: index must be >= 1\n", stderr); return 1; }

    // Parse optional -o
    fs::path outputPath;
    for (int i = 4; i < argc - 1; i++)
    {
        if (std::string_view(argv[i]) == "-o")
            outputPath = argv[i + 1];
    }

    // Open input, verify APM
    std::ifstream f(inputPath, std::ios::binary | std::ios::ate);
    if (!f) { println(stderr, "imgutil: cannot open {}", argv[2]); return 1; }
    uint64_t fileSize = static_cast<uint64_t>(f.tellg());

    if (detect_format(f, fileSize) != ImageFormat::APM)
    {
        std::fputs("imgutil: extract requires an APM disk image\n", stderr);
        return 1;
    }

    auto parts = parse_partition_map(f);
    if (index > static_cast<int>(parts.size()))
    {
        println(stderr, "imgutil: partition {} does not exist (disk has {})",
                     index, parts.size());
        return 1;
    }

    auto &part = parts[index - 1];
    uint64_t offset = static_cast<uint64_t>(part.startBlock) * 512;
    uint64_t size   = static_cast<uint64_t>(part.blockCount) * 512;

    // Default output name
    if (outputPath.empty())
    {
        std::string stem = inputPath.stem().string();
        outputPath = inputPath.parent_path() / (stem + "_p" + std::to_string(index) + ".hfs");
    }

    // Copy partition data to output
    std::ofstream out(outputPath, std::ios::binary);
    if (!out)
    {
        println(stderr, "imgutil: cannot create {}", outputPath.string());
        return 1;
    }

    constexpr size_t kBufSize = 64 * 1024;
    std::vector<uint8_t> buf(kBufSize);
    uint64_t remaining = size;
    f.seekg(static_cast<std::streamoff>(offset));

    while (remaining > 0)
    {
        size_t chunk = static_cast<size_t>(std::min<uint64_t>(remaining, kBufSize));
        f.read(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(chunk));
        out.write(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(chunk));
        remaining -= chunk;
    }

    println("Extracted partition {} (\"{}\", {}) \u2192 {} ({} bytes)",
                 index, part.name, part.type, outputPath.string(), size);
    return 0;
}

/* ── mkdisk — assemble a bootable APM disk from HFS partitions ── */

static constexpr uint32_t kPartMapStart   = 1;   // block 1
static constexpr uint32_t kPartMapBlocks  = 63;  // blocks 1–63
static constexpr uint32_t kDriverStart    = 64;  // block 64
// Boot args from PM entry +136; the driver validates boot_args[0] as a
// version signature (accepts 0x00010600 or 0x00010500) and crashes without it.
static constexpr uint8_t kDriverBootArgs[16] = {
    0x00, 0x01, 0x06, 0x00,  0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,  0x00, 0x07, 0x00, 0x00,
};
static constexpr uint32_t kDriverBlocks   = 32;  // blocks 64–95
static constexpr uint32_t kDriverUsed     = 19;  // actual code size in blocks
static constexpr uint32_t kDriverBootSize = 0x24B0; // boot code size (9392 bytes)
static constexpr uint32_t kDriverChecksum = 0xF624; // boot code checksum
static constexpr uint32_t kHFSStart       = 96;  // block 96
static constexpr uint32_t kFreeBlocks     = 32;  // trailing Apple_Free

/*
	Build a complete APM disk image from one or more bare HFS partition
	files.  Writes DDR, partition map, embedded Apple_Driver43, the HFS
	partition(s) verbatim, and trailing Apple_Free space.  The resulting
	image is bootable by the emulator or real hardware.
*/
static int cmd_mkdisk(int argc, char *argv[])
{
    std::vector<fs::path> inputs;
    fs::path outputPath;

    for (int i = 2; i < argc; i++)
    {
        if (std::string_view(argv[i]) == "-o" && i + 1 < argc)
        {
            outputPath = argv[++i];
        }
        else
        {
            inputs.emplace_back(argv[i]);
        }
    }

    if (inputs.empty())
    {
        std::fputs("Usage: imgutil mkdisk <part1> [part2 ...] [-o output]\n", stderr);
        return 1;
    }

    if (inputs.size() > 1 && outputPath.empty())
    {
        std::fputs("imgutil: -o required when multiple inputs given\n", stderr);
        return 1;
    }

    if (outputPath.empty())
    {
        std::string stem = inputs[0].stem().string();
        outputPath = inputs[0].parent_path() / (stem + "_disk.img");
    }

    // Validate inputs and read volume names
    struct InputPart {
        fs::path path;
        uint64_t size;        // file size in bytes
        uint32_t blocks;      // size in 512-byte blocks
        std::string volName;  // from MDB, or "Untitled N"
    };
    std::vector<InputPart> inputParts;

    for (size_t i = 0; i < inputs.size(); i++)
    {
        InputPart ip;
        ip.path = inputs[i];

        std::ifstream f(ip.path, std::ios::binary | std::ios::ate);
        if (!f)
        {
            println(stderr, "imgutil: cannot open {}", ip.path.string());
            return 1;
        }
        ip.size = static_cast<uint64_t>(f.tellg());
        ip.blocks = static_cast<uint32_t>(ip.size / 512);

        if (ip.size == 0 || ip.size % 512 != 0)
        {
            println(stderr, "imgutil: {}: size not a multiple of 512",
                         ip.path.string());
            return 1;
        }

        // Validate HFS MDB
        uint8_t mdb_sig[2];
        if (!read_bytes_at(f, 1024, mdb_sig, 2) || get16(mdb_sig) != kMDBSignature)
        {
            println(stderr, "imgutil: {}: no valid HFS MDB found",
                         ip.path.string());
            return 1;
        }

        // Read volume name
        auto vi = read_mdb(f, 0);
        ip.volName = vi.valid ? vi.name : ("Untitled " + std::to_string(i + 1));

        inputParts.push_back(std::move(ip));
    }

    // Calculate layout
    uint32_t totalHFSBlocks = 0;
    for (auto &ip : inputParts)
        totalHFSBlocks += ip.blocks;

    uint32_t totalBlocks = kHFSStart + totalHFSBlocks + kFreeBlocks;
    uint32_t numPMapEntries = 2 + static_cast<uint32_t>(inputParts.size()) + 1;
    // Entries: Apple_partition_map, Apple_Driver43, Apple_HFS ×N, Apple_Free

    // --- Write output ---
    std::ofstream out(outputPath, std::ios::binary);
    if (!out)
    {
        println(stderr, "imgutil: cannot create {}", outputPath.string());
        return 1;
    }

    // Block 0: DDR
    uint8_t ddr[512] = {};
    put16(ddr + 0, kDDRSignature);      // "ER"
    put16(ddr + 2, 512);                // block size
    put32(ddr + 4, totalBlocks);        // block count
    put16(ddr + 8, 1);                  // devType
    put16(ddr + 10, 1);                 // devId
    // offset 12–15: reserved (0)
    put16(ddr + 16, 1);                 // driverCount = 1
    // Driver entry 0:
    put32(ddr + 18, kDriverStart);      // block
    put16(ddr + 22, kDriverUsed);       // size (19 blocks)
    put16(ddr + 24, 1);                 // type (MacOS)
    out.write(reinterpret_cast<char *>(ddr), 512);

    // Helper to write a PM entry
    auto write_pm = [&](uint32_t startBlock, uint32_t blockCount,
                        const char *name, const char *type,
                        uint32_t lBlockStart, uint32_t lBlockCount,
                        uint32_t flags,
                        uint32_t bootSize = 0, uint32_t bootChecksum = 0,
                        const char *processor = nullptr,
                        const uint8_t *bootArgs = nullptr, size_t bootArgsLen = 0)
    {
        uint8_t pm[512] = {};
        put16(pm + 0, kPMSignature);
        put32(pm + 4, numPMapEntries);
        put32(pm + 8, startBlock);
        put32(pm + 12, blockCount);
        std::strncpy(reinterpret_cast<char *>(pm + 16), name, 32);
        std::strncpy(reinterpret_cast<char *>(pm + 48), type, 32);
        put32(pm + 80, lBlockStart);
        put32(pm + 84, lBlockCount);
        put32(pm + 88, flags);
        put32(pm + 96, bootSize);
        put32(pm + 116, bootChecksum);
        if (processor)
            std::strncpy(reinterpret_cast<char *>(pm + 120), processor, 16);
        if (bootArgs && bootArgsLen > 0)
            std::memcpy(pm + 136, bootArgs, bootArgsLen);
        out.write(reinterpret_cast<char *>(pm), 512);
    };

    // PM entry 1: Apple_partition_map
    write_pm(kPartMapStart, kPartMapBlocks,
             "Apple", "Apple_partition_map",
             0, kPartMapBlocks, 0x37);

    // PM entry 2: Apple_Driver43
    write_pm(kDriverStart, kDriverBlocks,
             "Macintosh", "Apple_Driver43",
             0, kDriverBlocks, 0x7F,
             kDriverBootSize, kDriverChecksum, "68000",
             kDriverBootArgs, sizeof(kDriverBootArgs));

    // PM entries 3..N: Apple_HFS partitions
    uint32_t currentBlock = kHFSStart;
    for (auto &ip : inputParts)
    {
        write_pm(currentBlock, ip.blocks,
                 ip.volName.c_str(), "Apple_HFS",
                 0, ip.blocks, 0xB7);
        currentBlock += ip.blocks;
    }

    // PM entry N+1: Apple_Free
    write_pm(currentBlock, kFreeBlocks,
             "Extra", "Apple_Free",
             0, kFreeBlocks, 0x37);

    // Pad remaining partition map blocks
    uint32_t pmBlocksWritten = numPMapEntries;
    for (uint32_t i = pmBlocksWritten; i < kPartMapBlocks; i++)
    {
        uint8_t zeros[512] = {};
        out.write(reinterpret_cast<char *>(zeros), 512);
    }

    // Blocks 64–95: Driver
    out.write(reinterpret_cast<const char *>(driver43_bin), driver43_bin_len);
    // If driver43_bin_len < 32*512, pad with zeros
    if (driver43_bin_len < kDriverBlocks * 512)
    {
        std::vector<uint8_t> pad(kDriverBlocks * 512 - driver43_bin_len, 0);
        out.write(reinterpret_cast<char *>(pad.data()),
                  static_cast<std::streamsize>(pad.size()));
    }

    // Blocks 96+: HFS partitions (copy verbatim)
    constexpr size_t kBufSize = 64 * 1024;
    std::vector<uint8_t> buf(kBufSize);
    for (auto &ip : inputParts)
    {
        std::ifstream in(ip.path, std::ios::binary);
        uint64_t remaining = ip.size;
        while (remaining > 0)
        {
            size_t chunk = static_cast<size_t>(std::min<uint64_t>(remaining, kBufSize));
            in.read(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(chunk));
            out.write(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(chunk));
            remaining -= chunk;
        }
    }

    // Trailing Apple_Free (32 blocks of zeros)
    {
        std::vector<uint8_t> zeros(kFreeBlocks * 512, 0);
        out.write(reinterpret_cast<char *>(zeros.data()),
                  static_cast<std::streamsize>(zeros.size()));
    }

    uint64_t totalSize = static_cast<uint64_t>(totalBlocks) * 512;
    println("Created {} ({} KB, {} blocks)",
                 outputPath.string(), totalSize / 1024, totalBlocks);
    return 0;
}

/* ── Main ──────────────────────────────────────────── */

static void usage()
{
    std::fputs(
        "Usage:\n"
        "  imgutil info <file>\n"
        "  imgutil extract <file> <index> [-o output]\n"
        "  imgutil mkdisk <part1> [part2 ...] [-o output]\n",
        stderr);
}

int main(int argc, char *argv[])
{
    if (argc < 2) { usage(); return 1; }
    std::string_view cmd = argv[1];
    if (cmd == "info")    return cmd_info(argc, argv);
    if (cmd == "extract") return cmd_extract(argc, argv);
    if (cmd == "mkdisk")  return cmd_mkdisk(argc, argv);
    usage();
    return 1;
}
