#include "resource_fork.h"
#include "icon_decode.h"
#include "png_text.h"
#include "storage/appledouble.h"
#include "storage/appledouble_internal.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

namespace {

void PrintUsage()
{
    std::puts(
        "Usage: icon-extract [OPTIONS] FILE|DIR [FILE|DIR...]\n"
        "\n"
        "Extract icl8 icons from Mac resource forks (or AppleDouble sidecars)\n"
        "and write them as 32×32 RGBA PNG files.\n"
        "\n"
        "Options:\n"
        "  -o, --output-dir DIR    Write PNGs to DIR (default: .)\n"
        "  -r, --recursive         Recurse into directories\n"
        "  -v, --verbose           Print each extracted icon\n"
        "  -h, --help              Show this help\n");
}

std::vector<uint8_t> ReadFile(const fs::path &path)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto size = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    f.read(reinterpret_cast<char *>(buf.data()), size);
    return buf;
}

// Extract the raw resource fork from an AppleDouble sidecar blob.
std::vector<uint8_t> ForkFromSidecar(const std::vector<uint8_t> &blob)
{
    using appledouble::detail::ReadBE16;
    using appledouble::detail::ReadBE32;

    if (blob.size() < 26) return {};
    if (ReadBE32(blob.data()) != 0x00051607u) return {};

    uint16_t numEntries = ReadBE16(blob.data() + 24);
    for (uint16_t i = 0; i < numEntries; ++i) {
        const uint8_t *e = blob.data() + 26 + i * 12;
        if (e + 12 > blob.data() + blob.size()) break;
        if (ReadBE32(e) == 2) { // resource fork entry
            uint32_t off = ReadBE32(e + 4);
            uint32_t len = ReadBE32(e + 8);
            if (off + len <= blob.size())
                return {blob.begin() + off, blob.begin() + off + len};
        }
    }
    return {};
}

constexpr uint32_t kAppleDoubleMagic = 0x00051607u;
constexpr uint32_t kIcl8Type = 0x69636C38u; // 'icl8'
constexpr uint32_t kICNType = 0x49434E23u;  // 'ICN#'

} // namespace

int main(int argc, char *argv[])
{
    fs::path outputDir = ".";
    bool verbose = false;
    bool recursive = false;
    std::vector<fs::path> inputs;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "-h" || arg == "--help") {
            PrintUsage();
            return 0;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-r" || arg == "--recursive") {
            recursive = true;
        } else if (arg == "-o" || arg == "--output-dir") {
            if (++i >= argc) {
                std::fputs("Error: -o requires an argument\n", stderr);
                return 1;
            }
            outputDir = argv[i];
        } else {
            inputs.emplace_back(argv[i]);
        }
    }

    if (inputs.empty()) {
        PrintUsage();
        return 1;
    }

    // Expand directories when -r is set
    if (recursive) {
        std::vector<fs::path> expanded;
        for (const auto &input : inputs) {
            if (fs::is_directory(input)) {
                for (const auto &entry : fs::recursive_directory_iterator(input)) {
                    if (!entry.is_regular_file()) continue;
                    auto name = entry.path().filename().string();
                    if (name.starts_with("._"))
                        expanded.push_back(entry.path());
                }
            } else {
                expanded.push_back(input);
            }
        }
        inputs = std::move(expanded);
    }

    fs::create_directories(outputDir);
    int totalExtracted = 0;

    for (const auto &inputPath : inputs) {
        auto bytes = ReadFile(inputPath);
        if (bytes.empty()) {
            std::fprintf(stderr, "Error: cannot read '%s'\n",
                         inputPath.c_str());
            continue;
        }

        // Determine resource fork source
        std::vector<uint8_t> fork;
        using appledouble::detail::ReadBE32;

        if (bytes.size() >= 4 && ReadBE32(bytes.data()) == kAppleDoubleMagic) {
            // Input IS a sidecar
            fork = ForkFromSidecar(bytes);
        } else if (auto sidecar = appledouble::SidecarPathFor(inputPath);
                   fs::exists(sidecar)) {
            auto sidecarBytes = ReadFile(sidecar);
            fork = ForkFromSidecar(sidecarBytes);
        } else {
            // Treat as raw resource fork
            fork = std::move(bytes);
        }

        if (fork.empty()) {
            std::fprintf(stderr, "Warning: no resource fork in '%s'\n",
                         inputPath.c_str());
            continue;
        }

        auto resources = rsrc::ParseResourceFork(fork);
        auto icl8s = rsrc::FindByType(resources, kIcl8Type);

        if (icl8s.empty()) {
            std::fprintf(stderr, "Warning: no icl8 resources in '%s'\n",
                         inputPath.c_str());
            continue;
        }

        // Derive the original Mac filename from the input path.
        // If it's a sidecar (._Foo), strip the ._ prefix.
        std::string basename = inputPath.filename().string();
        if (basename.starts_with("._"))
            basename = basename.substr(2);
        std::string fullPath = inputPath.string();

        for (auto *icl8 : icl8s) {
            if (icl8->data.size() != 1024) {
                std::fprintf(stderr, "Warning: icl8 id=%d wrong size "
                                     "(%zu), skipping\n",
                             icl8->id, icl8->data.size());
                continue;
            }

            std::optional<std::span<const uint8_t, 128>> mask;
            auto *icn = rsrc::FindByTypeAndId(resources, kICNType, icl8->id);
            if (icn && icn->data.size() == 256) {
                mask = std::span<const uint8_t, 128>(
                    icn->data.data() + 128, 128);
            } else if (verbose) {
                std::fprintf(stderr, "Warning: no ICN# mask for icl8 "
                                     "id=%d, using opaque\n",
                             icl8->id);
            }

            std::span<const uint8_t, 1024> icl8Span(icl8->data.data(), 1024);
            auto icon = rsrc::DecodeIcl8(icl8Span, mask);

            std::string outName = basename;
            if (icl8s.size() > 1)
                outName += "_" + std::to_string(icl8->id);
            outName += ".png";
            auto outFile = outputDir / outName;

            for (int dup = 2; fs::exists(outFile); ++dup) {
                std::string dedupName = basename;
                if (icl8s.size() > 1)
                    dedupName += "_" + std::to_string(icl8->id);
                dedupName += " (" + std::to_string(dup) + ").png";
                outFile = outputDir / dedupName;
            }

            // Embed original filename and full path as iTXt metadata
            png::TextChunk textChunks[] = {
                {"Title", basename},
                {"Source", fullPath},
            };

            if (!png::WritePngWithText(outFile, 32, 32,
                                       icon.pixels, textChunks)) {
                std::fprintf(stderr, "Error: failed to write '%s'\n",
                             outFile.c_str());
                continue;
            }

            if (verbose)
                std::printf("  %s\n", outFile.c_str());
            ++totalExtracted;
        }
    }

    if (totalExtracted == 0) {
        std::fputs("No icons extracted.\n", stderr);
        return 1;
    }

    std::printf("Extracted %d icon(s).\n", totalExtracted);
    return 0;
}
