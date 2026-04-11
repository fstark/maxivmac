/*
	state_recorder.cpp

	Implementation of the golden-file record/verify system.
	See docs/TRACE.md for design details.
*/

#include "core/state_recorder.hpp"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cinttypes>

StateRecorder g_recorder;


// ── Wire format (private to this TU) ──────────────────
// All multi-byte fields stored in native (little-endian) byte order.

static constexpr uint32_t kGoldenMagic = 0x474F4C44; // "GOLD"
static constexpr uint32_t kGoldenVersion = 3;

struct GoldenHeader
{							   // 80 bytes
	uint32_t magic;			   // 0
	uint32_t version;		   // 4
	uint64_t maxInstructions;  // 8
	uint32_t snapshotInterval; // 16
	uint32_t reserved_sc;	   // 20  (derive count from file size)
	uint32_t modelId;		   // 24
	uint8_t speedValue;		   // 28
	uint8_t reserved0a;		   // 29
	uint16_t reserved0b;	   // 30
	uint8_t romHash[16];	   // 32
	uint8_t diskHash[16];	   // 48
	uint32_t ramSize;		   // 64
	uint16_t screenWidth;	   // 68
	uint16_t screenHeight;	   // 70
	uint8_t screenDepth;	   // 72
	uint8_t reserved1a;		   // 73
	uint16_t reserved1b;	   // 74
	uint32_t reserved2;		   // 76
};
static_assert(sizeof(GoldenHeader) == 80, "GoldenHeader must be 80 bytes");

struct CpuSnapshot
{							   // 88 bytes
	uint64_t instructionCount; // 0
	uint32_t pc;			   // 8
	uint16_t sr;			   // 12
	uint16_t pad;			   // 14
	uint32_t d[8];			   // 16
	uint32_t a[8];			   // 48
	uint32_t ioCrc;			   // 80
	uint32_t reserved;		   // 84  (pad to 8-byte alignment)
};
static_assert(sizeof(CpuSnapshot) == 88, "CpuSnapshot must be 88 bytes");


// ── CRC32 (table-based, used for I/O hashing) ─────────

static const uint32_t *crc32_table()
{
	static uint32_t t[256];
	static bool ready = false;
	if (!ready)
	{
		for (uint32_t i = 0; i < 256; i++)
		{
			uint32_t c = i;
			for (int j = 0; j < 8; j++)
				c = (c >> 1) ^ (0xEDB88320 & (-(c & 1)));
			t[i] = c;
		}
		ready = true;
	}
	return t;
}

static uint32_t crc32_update(uint32_t crc, const void *data, size_t len)
{
	const uint32_t *t = crc32_table();
	const uint8_t *p = static_cast<const uint8_t *>(data);
	crc = ~crc;
	for (size_t i = 0; i < len; i++)
		crc = t[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
	return ~crc;
}

// ── Helpers ────────────────────────────────────────────

static FILE *openTextOutput(const std::string &path)
{
	if (path.empty()) return stderr;
	FILE *f = std::fopen(path.c_str(), "w");
	if (!f)
	{
		std::fprintf(stderr, "StateRecorder: cannot open text log '%s'\n", path.c_str());
		return stderr;
	}
	return f;
}

static void hashToHex(const uint8_t h[16], char out[33])
{
	for (int i = 0; i < 16; i++)
		std::snprintf(out + i * 2, 3, "%02x", h[i]);
	out[32] = '\0';
}


// ── readHeader (static) ────────────────────────────────

bool StateRecorder::readHeader(const std::string &path, HeaderInfo &out)
{
	FILE *f = std::fopen(path.c_str(), "rb");
	if (!f)
	{
		std::fprintf(stderr, "StateRecorder: cannot open '%s' for reading\n", path.c_str());
		return false;
	}

	GoldenHeader h;
	if (std::fread(&h, sizeof(h), 1, f) != 1)
	{
		std::fprintf(stderr, "StateRecorder: failed to read header from '%s'\n", path.c_str());
		std::fclose(f);
		return false;
	}
	std::fclose(f);

	if (h.magic != kGoldenMagic)
	{
		std::fprintf(stderr, "StateRecorder: bad magic 0x%08X (expected 0x%08X)\n", h.magic,
					 kGoldenMagic);
		return false;
	}
	if (h.version != kGoldenVersion)
	{
		std::fprintf(stderr, "StateRecorder: unsupported version %u (expected %u)\n", h.version,
					 kGoldenVersion);
		return false;
	}

	out.modelId = h.modelId;
	out.speedValue = h.speedValue;
	out.ramSize = h.ramSize;
	out.screenWidth = h.screenWidth;
	out.screenHeight = h.screenHeight;
	out.screenDepth = h.screenDepth;
	out.snapshotInterval = h.snapshotInterval;
	out.maxInstructions = h.maxInstructions;
	std::memcpy(out.romHash, h.romHash, 16);
	std::memcpy(out.diskHash, h.diskHash, 16);
	return true;
}


// ── init ───────────────────────────────────────────────

/*
	Open golden file for record or verify, set up text logs,
	and validate header (model, snapshot count, interval).
*/
bool StateRecorder::init(const Config &cfg)
{
	mode_ = cfg.mode;
	onMismatch_ = cfg.onMismatch;
	textLog_ = cfg.textLog;
	snapshotInterval_ = cfg.snapshotInterval;
	maxInstructions_ = cfg.maxInstructions;
	nextSnapshot_ = snapshotInterval_; // first snapshot at instruction N
	snapshotIndex_ = 0;
	snapshotCount_ = 0;
	ioCrc_ = 0;
	finished_ = false;

	if (mode_ == RecorderMode::Off && textLog_ == TextLog::None) return true;

	// Open text log if requested
	if (textLog_ != TextLog::None)
	{
		textFile_ = openTextOutput(cfg.textPath);
	}

	if (mode_ == RecorderMode::Record)
	{
		goldenFile_ = std::fopen(cfg.goldenPath.c_str(), "wb");
		if (!goldenFile_)
		{
			std::fprintf(stderr, "StateRecorder: cannot open '%s' for writing\n",
						 cfg.goldenPath.c_str());
			return false;
		}

		// Build and write header
		GoldenHeader hdr = {};
		hdr.magic = kGoldenMagic;
		hdr.version = kGoldenVersion;
		hdr.snapshotInterval = snapshotInterval_;
		hdr.maxInstructions = maxInstructions_;
		hdr.modelId = cfg.modelId;
		hdr.speedValue = cfg.speedValue;
		std::memcpy(hdr.romHash, cfg.romHash, 16);
		std::memcpy(hdr.diskHash, cfg.diskHash, 16);
		hdr.ramSize = cfg.ramSize;
		hdr.screenWidth = cfg.screenWidth;
		hdr.screenHeight = cfg.screenHeight;
		hdr.screenDepth = cfg.screenDepth;

		std::fwrite(&hdr, sizeof(hdr), 1, goldenFile_);
		std::fflush(goldenFile_);

		std::fprintf(stderr, "StateRecorder: RECORD → %s (interval=%u, max=%" PRIu64 ")\n",
					 cfg.goldenPath.c_str(), snapshotInterval_, maxInstructions_);
	}
	else if (mode_ == RecorderMode::Verify)
	{
		FILE *f = std::fopen(cfg.goldenPath.c_str(), "rb");
		if (!f)
		{
			std::fprintf(stderr, "StateRecorder: cannot open '%s' for reading\n",
						 cfg.goldenPath.c_str());
			return false;
		}

		GoldenHeader h;
		if (std::fread(&h, sizeof(h), 1, f) != 1)
		{
			std::fprintf(stderr, "StateRecorder: failed to read header from '%s'\n",
						 cfg.goldenPath.c_str());
			std::fclose(f);
			return false;
		}

		// Validate header
		if (h.magic != kGoldenMagic)
		{
			std::fprintf(stderr, "StateRecorder: bad magic 0x%08X (expected 0x%08X)\n", h.magic,
						 kGoldenMagic);
			std::fclose(f);
			return false;
		}
		if (h.version != kGoldenVersion)
		{
			std::fprintf(stderr, "StateRecorder: unsupported version %u (expected %u)\n", h.version,
						 kGoldenVersion);
			std::fclose(f);
			return false;
		}
		if (h.modelId != cfg.modelId)
		{
			std::fprintf(stderr, "StateRecorder: model mismatch: golden=%u, current=%u\n",
						 h.modelId, cfg.modelId);
			std::fclose(f);
			return false;
		}
		if (h.speedValue != cfg.speedValue)
		{
			std::fprintf(stderr, "StateRecorder: speed mismatch: golden=%u, current=%u\n",
						 h.speedValue, cfg.speedValue);
			std::fclose(f);
			return false;
		}
		if (h.ramSize != cfg.ramSize)
		{
			std::fprintf(stderr, "StateRecorder: RAM size mismatch: golden=%u, current=%u\n",
						 h.ramSize, cfg.ramSize);
			std::fclose(f);
			return false;
		}
		if (h.screenWidth != cfg.screenWidth || h.screenHeight != cfg.screenHeight)
		{
			std::fprintf(stderr,
						 "StateRecorder: screen size mismatch: golden=%ux%u, current=%ux%u\n",
						 h.screenWidth, h.screenHeight, cfg.screenWidth, cfg.screenHeight);
			std::fclose(f);
			return false;
		}
		if (h.screenDepth != cfg.screenDepth)
		{
			std::fprintf(stderr, "StateRecorder: screen depth mismatch: golden=%u, current=%u\n",
						 h.screenDepth, cfg.screenDepth);
			std::fclose(f);
			return false;
		}
		if (std::memcmp(h.romHash, cfg.romHash, 16) != 0)
		{
			char expH[33], gotH[33];
			hashToHex(h.romHash, expH);
			hashToHex(cfg.romHash, gotH);
			std::fprintf(stderr, "StateRecorder: ROM hash mismatch:\n  golden: %s\n  actual: %s\n",
						 expH, gotH);
			std::fclose(f);
			return false;
		}
		// disk hash: only check if golden has a non-zero hash
		{
			uint8_t zero[16] = {};
			if (std::memcmp(h.diskHash, zero, 16) != 0 &&
				std::memcmp(h.diskHash, cfg.diskHash, 16) != 0)
			{
				char expH[33], gotH[33];
				hashToHex(h.diskHash, expH);
				hashToHex(cfg.diskHash, gotH);
				std::fprintf(stderr,
							 "StateRecorder: disk hash mismatch:\n  golden: %s\n  actual: %s\n",
							 expH, gotH);
				std::fclose(f);
				return false;
			}
		}

		snapshotInterval_ = h.snapshotInterval;
		maxInstructions_ = h.maxInstructions;
		nextSnapshot_ = snapshotInterval_;

		// Derive snapshot count from file size
		std::fseek(f, 0, SEEK_END);
		long fileSize = std::ftell(f);
		long dataSize = fileSize - static_cast<long>(sizeof(GoldenHeader));
		if (dataSize < 0 || (dataSize % sizeof(CpuSnapshot)) != 0)
		{
			std::fprintf(stderr,
						 "StateRecorder: file size inconsistent "
						 "(data region = %ld bytes, snapshot size = %zu)\n",
						 dataSize, sizeof(CpuSnapshot));
			std::fclose(f);
			return false;
		}
		snapshotCount_ = static_cast<uint32_t>(dataSize / sizeof(CpuSnapshot));

		// Load all snapshots into memory
		std::fseek(f, sizeof(GoldenHeader), SEEK_SET);
		auto *snaps = new CpuSnapshot[snapshotCount_];
		for (uint32_t i = 0; i < snapshotCount_; i++)
		{
			if (std::fread(&snaps[i], sizeof(CpuSnapshot), 1, f) != 1)
			{
				std::fprintf(stderr, "StateRecorder: failed to read snapshot %u/%u\n", i,
							 snapshotCount_);
				std::fclose(f);
				delete[] snaps;
				return false;
			}
		}
		goldenSnaps_ = snaps;
		std::fclose(f);

		std::fprintf(stderr, "StateRecorder: VERIFY ← %s (%u snapshots, interval=%u)\n",
					 cfg.goldenPath.c_str(), snapshotCount_, snapshotInterval_);
	}

	return true;
}


// ── Text output (file-static) ──────────────────────────

static void dumpCpuSnapshot(FILE *out, const CpuSnapshot &snap)
{
	std::fprintf(out,
				 "CPU @%" PRIu64 " PC=%08X SR=%04X"
				 " D=%08X %08X %08X %08X %08X %08X %08X %08X"
				 " A=%08X %08X %08X %08X %08X %08X %08X %08X"
				 " IO_CRC=%08X\n",
				 snap.instructionCount, snap.pc, snap.sr, snap.d[0], snap.d[1], snap.d[2],
				 snap.d[3], snap.d[4], snap.d[5], snap.d[6], snap.d[7], snap.a[0], snap.a[1],
				 snap.a[2], snap.a[3], snap.a[4], snap.a[5], snap.a[6], snap.a[7], snap.ioCrc);
	std::fflush(out);
}

static void dumpMismatch(FILE *out, uint32_t snapIdx, const CpuSnapshot &expected,
						 const CpuSnapshot &actual)
{
	std::fprintf(out, "\n=== MISMATCH at instruction %" PRIu64 " (snapshot #%u) ===\n",
				 actual.instructionCount, snapIdx);

	auto field32 = [&](const char *name, uint32_t exp, uint32_t act)
	{
		if (exp != act)
			std::fprintf(out, "  %-4s: expected %08X  actual %08X  ***\n", name, exp, act);
		else
			std::fprintf(out, "  %-4s: %08X\n", name, act);
	};
	auto field16 = [&](const char *name, uint16_t exp, uint16_t act)
	{
		if (exp != act)
			std::fprintf(out, "  %-4s: expected %04X      actual %04X      ***\n", name, exp, act);
		else
			std::fprintf(out, "  %-4s: %04X\n", name, act);
	};

	field32("PC", expected.pc, actual.pc);
	field16("SR", expected.sr, actual.sr);
	for (int i = 0; i < 8; i++)
	{
		char name[4];
		std::snprintf(name, sizeof(name), "D%d", i);
		field32(name, expected.d[i], actual.d[i]);
	}
	for (int i = 0; i < 8; i++)
	{
		char name[4];
		std::snprintf(name, sizeof(name), "A%d", i);
		field32(name, expected.a[i], actual.a[i]);
	}

	if (expected.ioCrc != actual.ioCrc)
	{
		std::fprintf(
			out,
			"  I/O CRC: expected %08X  actual %08X  *** (I/O diverged since previous snapshot)\n",
			expected.ioCrc, actual.ioCrc);
	}

	std::fprintf(out, "\n");
	std::fflush(out);
}


// ── cpuSlow ────────────────────────────────────────────

void StateRecorder::cpuSlow(uint32_t instructionCount, uint32_t pc, uint16_t sr,
							const uint32_t d[8], const uint32_t a[8])
{
	CpuSnapshot snap;
	snap.instructionCount = static_cast<uint64_t>(instructionCount);
	snap.pc = pc;
	snap.sr = sr;
	snap.pad = 0;
	std::memcpy(snap.d, d, sizeof(snap.d));
	std::memcpy(snap.a, a, sizeof(snap.a));
	snap.ioCrc = ioCrc_;

	FILE *out = textFile_ ? textFile_ : stderr;

	if (mode_ == RecorderMode::Record)
	{
		std::fwrite(&snap, sizeof(snap), 1, goldenFile_);
		snapshotCount_++;

		if (textLog_ >= TextLog::CpuOnly) dumpCpuSnapshot(out, snap);

		ioCrc_ = 0;
		nextSnapshot_ += snapshotInterval_;

		if (instructionCount >= maxInstructions_)
		{
			finish();
			std::exit(0);
		}
	}
	else if (mode_ == RecorderMode::Verify)
	{
		if (snapshotIndex_ >= snapshotCount_)
		{
			std::fprintf(stderr, "PASS (%u snapshots verified)\n", snapshotCount_);
			finish();
			std::exit(0);
		}

		auto *snaps = static_cast<CpuSnapshot *>(goldenSnaps_);
		const CpuSnapshot &expected = snaps[snapshotIndex_];
		bool cpuOk =
			(snap.instructionCount == expected.instructionCount && snap.pc == expected.pc &&
			 snap.sr == expected.sr && std::memcmp(snap.d, expected.d, sizeof(snap.d)) == 0 &&
			 std::memcmp(snap.a, expected.a, sizeof(snap.a)) == 0);
		bool ioOk = (snap.ioCrc == expected.ioCrc);

		if (textLog_ >= TextLog::CpuOnly) dumpCpuSnapshot(out, snap);

		if (!cpuOk)
		{
			dumpMismatch(out, snapshotIndex_, expected, snap);
			if (onMismatch_ == OnMismatch::ExitNonZero)
			{
				finish();
				std::exit(1);
			}
		}
		else if (!ioOk)
		{
			std::fprintf(out,
						 "IO_CRC warning at snapshot #%u (insn %" PRIu64 "): "
						 "expected %08X, actual %08X\n",
						 snapshotIndex_, snap.instructionCount, expected.ioCrc, snap.ioCrc);
		}

		ioCrc_ = 0;
		snapshotIndex_++;
		nextSnapshot_ += snapshotInterval_;

		if (snapshotIndex_ >= snapshotCount_)
		{
			std::fprintf(stderr, "PASS (%u snapshots verified)\n", snapshotCount_);
			finish();
			std::exit(0);
		}

		if (instructionCount >= maxInstructions_)
		{
			std::fprintf(stderr, "PASS (instruction budget reached, %u/%u snapshots verified)\n",
						 snapshotIndex_, snapshotCount_);
			finish();
			std::exit(0);
		}
	}

	// text-only mode (no golden file)
	if (mode_ == RecorderMode::Off)
	{
		if (textLog_ >= TextLog::CpuOnly) dumpCpuSnapshot(out, snap);
		ioCrc_ = 0;
		nextSnapshot_ += snapshotInterval_;
		if (instructionCount >= maxInstructions_)
		{
			finish();
			std::exit(0);
		}
	}
}


// ── ioSlow ─────────────────────────────────────────────

void StateRecorder::ioSlow(uint32_t instructionCount, uint32_t address, uint32_t data, bool write,
						   bool byteSize, const char *deviceName)
{
	struct
	{
		uint32_t ic;
		uint32_t addr;
		uint32_t data;
		uint8_t dir;
		uint8_t bs;
	} rec;
	rec.ic = instructionCount;
	rec.addr = address;
	rec.data = data;
	rec.dir = write ? 'W' : 'R';
	rec.bs = byteSize ? 1 : 0;
	ioCrc_ = crc32_update(ioCrc_, &rec, sizeof(rec));

	if (textLog_ >= TextLog::CpuAndIo)
		textIo(instructionCount, address, data, write, byteSize, deviceName);
}

void StateRecorder::textIo(uint32_t icount, uint32_t addr, uint32_t data, bool write, bool byteSize,
						   const char *device)
{
	FILE *out = textFile_ ? textFile_ : stderr;
	if (byteSize)
		std::fprintf(out, "IO  @%u %c.b %-4s %08X %02X\n", icount, write ? 'W' : 'R', device, addr,
					 data & 0xFF);
	else
		std::fprintf(out, "IO  @%u %c.w %-4s %08X %04X\n", icount, write ? 'W' : 'R', device, addr,
					 data & 0xFFFF);
}


// ── finish / destructor ────────────────────────────────

void StateRecorder::finish()
{
	if (finished_) return;
	finished_ = true;

	if (mode_ == RecorderMode::Record && goldenFile_)
	{
		std::fclose(goldenFile_);
		goldenFile_ = nullptr;
		std::fprintf(stderr, "StateRecorder: recorded %u snapshots\n", snapshotCount_);
	}

	if (goldenFile_)
	{
		std::fclose(goldenFile_);
		goldenFile_ = nullptr;
	}

	if (textFile_ && textFile_ != stderr)
	{
		std::fclose(textFile_);
	}
	textFile_ = nullptr;

	delete[] static_cast<CpuSnapshot *>(goldenSnaps_);
	goldenSnaps_ = nullptr;
}

StateRecorder::~StateRecorder()
{
	finish();
}


// ── Public interface ───────────────────────────────────

bool StateRecorder::active() const
{
	return mode_ != RecorderMode::Off || textLog_ != TextLog::None;
}

void StateRecorder::cpu(uint32_t instructionCount, uint32_t pc, uint16_t sr, const uint32_t d[8],
						const uint32_t a[8])
{
	if (instructionCount != nextSnapshot_) [[likely]]
		return;
	cpuSlow(instructionCount, pc, sr, d, a);
}

void StateRecorder::io(uint32_t instructionCount, uint32_t address, uint32_t data, bool write,
					   bool byteSize, const char *deviceName)
{
	if (textLog_ < TextLog::CpuAndIo && mode_ == RecorderMode::Off) [[likely]]
		return;
	ioSlow(instructionCount, address, data, write, byteSize, deviceName);
}
