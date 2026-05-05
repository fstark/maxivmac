# Icon Extraction Tool — Implementation Plan

Design: [ICON_TOOL_DESIGN.md](ICON_TOOL_DESIGN.md)
Spec: [ICON_TOOL.md](ICON_TOOL.md)

Build gate: `cmake --preset macos && cmake --build --preset macos`
Test gate:  `./bld/macos/tests && cd test && ./verify.sh`

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Mac system palette table + directory skeleton | |
| 2 | Resource fork parser + unit tests | |
| 3 | icl8 → RGBA decoder + unit tests | |
| 4 | Entry point, CLI, and CMake build | |
| 5 | Integration test with real sidecar | |

---

## Phase 1 — Mac System Palette + Directory Skeleton

Create the `tools/icon_extract/` directory with the palette table and
the STB implementation file.  These have zero dependencies and always
compile.

### 1.1 — Create `tools/icon_extract/mac_palette.h`

```cpp
#pragma once
#include <array>
#include <cstdint>

// Standard Macintosh 8-bit system palette (256 entries).
// Each entry is packed RGBA: R in bits 31–24, G 23–16, B 15–8, A 7–0.
// Alpha is always 0xFF in the table itself (mask applied separately).
//
// Source: Apple Technical Note TN1023, Color QuickDraw clut ID 8.
inline constexpr std::array<uint32_t, 256> kMacSystemPalette = {
    // ... 256 entries ...
};
```

The palette is 6×6×6 color cube (indices 0–214) followed by a
grayscale ramp (215–254) and black (255).  The 6×6×6 cube iterates:
- R: 0xFF, 0xCC, 0x99, 0x66, 0x33, 0x00  (6 levels)
- G: same 6 levels
- B: same 6 levels

Index formula for the color cube: `idx = r_step * 36 + g_step * 6 + b_step`
where each step is 0–5 (0 = brightest).

Grayscale ramp (indices 215–254): 40 entries from 0xEE down to 0x11
in equal steps (decrements of ~6.5 per entry; use the well-known
table values from the Mac system clut).

Index 255 = black (0x000000FF).

Provide the full 256-entry table with hex literals.

### 1.2 — Create `tools/icon_extract/stb_impl.cpp`

```cpp
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
```

### 1.3 — Create placeholder `tools/icon_extract/resource_fork.h`

Minimal header with the `rsrc` namespace and the `Resource` struct.
No implementation yet — just enough so Phase 2 builds incrementally.

```cpp
#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rsrc {

struct Resource {
    uint32_t type;
    int16_t  id;
    std::string name;
    std::vector<uint8_t> data;
};

std::vector<Resource> ParseResourceFork(std::span<const uint8_t> fork);

std::vector<const Resource*> FindByType(
    const std::vector<Resource>& resources, uint32_t type);

const Resource* FindByTypeAndId(
    const std::vector<Resource>& resources, uint32_t type, int16_t id);

} // namespace rsrc
```

### 1.4 — Create placeholder `tools/icon_extract/resource_fork.cpp`

Stub implementations that return empty vectors / nullptr.  Allows the
build to link in Phase 1.

### Fence

- [ ] `tools/icon_extract/mac_palette.h` exists with 256 entries
- [ ] `tools/icon_extract/stb_impl.cpp` compiles independently
- [ ] `tools/icon_extract/resource_fork.h` and `.cpp` exist (stubs)
- [ ] Commit: `"icon-extract: phase 1 — palette table and skeleton"`

---

## Phase 2 — Resource Fork Parser + Unit Tests

Implement the full resource fork binary parser and add unit tests
using a real sidecar that contains icon resources.

### 2.1 — Copy test fixture

Copy the file `macsrc/shareddrive/icons/._Macintosh 128K` (68111 bytes)
to `test/fixtures/icon_128k_sidecar.bin`.  This is a real AppleDouble
sidecar containing `icl8` and `ICN#` resources for the Macintosh 128K
icon.

### 2.2 — Implement `ParseResourceFork()` in `resource_fork.cpp`

Follow the algorithm from Design §5.1:

```cpp
#include "resource_fork.h"
#include "storage/appledouble_internal.h"  // ReadBE32, ReadBE16

using appledouble::detail::ReadBE16;
using appledouble::detail::ReadBE32;

namespace rsrc {

std::vector<Resource> ParseResourceFork(std::span<const uint8_t> fork)
{
    std::vector<Resource> result;
    if (fork.size() < 16) return result;

    uint32_t dataOff = ReadBE32(fork.data());
    uint32_t mapOff  = ReadBE32(fork.data() + 4);

    if (mapOff + 28 > fork.size()) return result;

    const uint8_t* map = fork.data() + mapOff;
    uint16_t typeListOff = ReadBE16(map + 24);
    uint16_t nameListOff = ReadBE16(map + 26);

    const uint8_t* typeList = map + typeListOff;
    if (typeList + 2 > fork.data() + fork.size()) return result;

    uint16_t numTypes = ReadBE16(typeList) + 1;

    for (uint16_t i = 0; i < numTypes; ++i) {
        const uint8_t* entry = typeList + 2 + i * 8;
        if (entry + 8 > fork.data() + fork.size()) break;

        uint32_t type   = ReadBE32(entry);
        uint16_t numRes = ReadBE16(entry + 4) + 1;
        uint16_t refOff = ReadBE16(entry + 6);

        const uint8_t* refList = typeList + refOff;

        for (uint16_t j = 0; j < numRes; ++j) {
            const uint8_t* ref = refList + j * 12;
            if (ref + 12 > fork.data() + fork.size()) break;

            int16_t id = static_cast<int16_t>(ReadBE16(ref));
            // uint16_t nameOff = ReadBE16(ref + 2); // unused for now
            uint32_t dataOff3 = (static_cast<uint32_t>(ref[5]) << 16)
                              | (static_cast<uint32_t>(ref[6]) << 8)
                              |  static_cast<uint32_t>(ref[7]);

            size_t absDataOff = dataOff + dataOff3;
            if (absDataOff + 4 > fork.size()) continue;

            uint32_t len = ReadBE32(fork.data() + absDataOff);
            if (absDataOff + 4 + len > fork.size()) continue;

            Resource r;
            r.type = type;
            r.id   = id;
            r.data.assign(fork.data() + absDataOff + 4,
                          fork.data() + absDataOff + 4 + len);
            result.push_back(std::move(r));
        }
    }
    return result;
}

std::vector<const Resource*> FindByType(
    const std::vector<Resource>& resources, uint32_t type)
{
    std::vector<const Resource*> out;
    for (auto& r : resources)
        if (r.type == type) out.push_back(&r);
    return out;
}

const Resource* FindByTypeAndId(
    const std::vector<Resource>& resources, uint32_t type, int16_t id)
{
    for (auto& r : resources)
        if (r.type == type && r.id == id) return &r;
    return nullptr;
}

} // namespace rsrc
```

### 2.3 — Create `test/test_resource_fork.cpp`

```cpp
#include <doctest/doctest.h>
#include "resource_fork.h"
#include "storage/appledouble_internal.h"

#include <filesystem>
#include <fstream>
#include <vector>

namespace {

std::vector<uint8_t> ReadTestFile(const char* path)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    REQUIRE(f.is_open());
    auto size = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    f.read(reinterpret_cast<char*>(buf.data()), size);
    return buf;
}

// Extract raw resource fork from the AppleDouble sidecar
std::vector<uint8_t> ExtractForkFromSidecar(const std::vector<uint8_t>& sidecar)
{
    // AppleDouble: magic(4) + version(4) + filler(16) + numEntries(2)
    // then entries: id(4) + offset(4) + length(4)
    // Resource fork entry has id == 2
    REQUIRE(sidecar.size() >= 26);
    using appledouble::detail::ReadBE16;
    using appledouble::detail::ReadBE32;

    REQUIRE(ReadBE32(sidecar.data()) == 0x00051607u);
    uint16_t numEntries = ReadBE16(sidecar.data() + 24);
    for (uint16_t i = 0; i < numEntries; ++i) {
        const uint8_t* e = sidecar.data() + 26 + i * 12;
        uint32_t entryId = ReadBE32(e);
        uint32_t offset  = ReadBE32(e + 4);
        uint32_t length  = ReadBE32(e + 8);
        if (entryId == 2) { // resource fork
            REQUIRE(offset + length <= sidecar.size());
            return {sidecar.begin() + offset,
                    sidecar.begin() + offset + length};
        }
    }
    FAIL("No resource fork entry in sidecar");
    return {};
}

} // namespace

TEST_CASE("ParseResourceFork — empty input")
{
    std::vector<uint8_t> empty;
    auto res = rsrc::ParseResourceFork(empty);
    CHECK(res.empty());
}

TEST_CASE("ParseResourceFork — truncated input")
{
    std::vector<uint8_t> small(10, 0);
    auto res = rsrc::ParseResourceFork(small);
    CHECK(res.empty());
}

TEST_CASE("ParseResourceFork — real sidecar (Macintosh 128K)")
{
    auto sidecar = ReadTestFile("test/fixtures/icon_128k_sidecar.bin");
    REQUIRE(!sidecar.empty());

    auto fork = ExtractForkFromSidecar(sidecar);
    REQUIRE(!fork.empty());

    auto resources = rsrc::ParseResourceFork(fork);
    REQUIRE(!resources.empty());

    // Should find icl8 resources (type = 'icl8' = 0x69636C38)
    auto icl8s = rsrc::FindByType(resources, 0x69636C38u);
    CHECK(!icl8s.empty());

    // Each icl8 should be exactly 1024 bytes
    for (auto* r : icl8s) {
        CHECK(r->data.size() == 1024);
    }

    // Should find ICN# resources (type = 'ICN#' = 0x49434E23)
    auto icns = rsrc::FindByType(resources, 0x49434E23u);
    CHECK(!icns.empty());

    // Each ICN# should be exactly 256 bytes (icon + mask)
    for (auto* r : icns) {
        CHECK(r->data.size() == 256);
    }

    // For each icl8, a matching ICN# with same ID should exist
    for (auto* icon : icl8s) {
        auto* mask = rsrc::FindByTypeAndId(resources, 0x49434E23u, icon->id);
        CHECK_MESSAGE(mask != nullptr,
            "Missing ICN# mask for icl8 id=", icon->id);
    }
}

TEST_CASE("FindByTypeAndId — returns nullptr for missing")
{
    std::vector<rsrc::Resource> resources;
    resources.push_back({0x12345678u, 1, "", {}});
    CHECK(rsrc::FindByTypeAndId(resources, 0x12345678u, 99) == nullptr);
    CHECK(rsrc::FindByTypeAndId(resources, 0x12345678u, 1) != nullptr);
}
```

### 2.4 — Add test sources to CMakeLists.txt

Add `test/test_resource_fork.cpp` and `tools/icon_extract/resource_fork.cpp`
to the `tests` executable source list.  Also add the
`tools/icon_extract` directory to the test target's include directories.

### Fence

- [ ] `test/fixtures/icon_128k_sidecar.bin` exists (copy of the Mac 128K sidecar)
- [ ] `resource_fork.cpp` parses the real sidecar correctly
- [ ] Unit tests pass: `./bld/macos/tests --test-case="*ResourceFork*"`
- [ ] Full build clean
- [ ] Commit: `"icon-extract: phase 2 — resource fork parser + tests"`

---

## Phase 3 — icl8 → RGBA Decoder + Unit Tests

Implement the palette lookup and mask compositing algorithm.

### 3.1 — Create `tools/icon_extract/icon_decode.h`

```cpp
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace rsrc {

struct IconRGBA {
    std::array<uint8_t, 32 * 32 * 4> pixels;  // RGBA, row-major
};

// Decode an icl8 resource (1024 bytes, palette-indexed) into RGBA.
// If mask is provided, it's the second 128 bytes of the ICN# resource
// (the 1-bit mask half).  Mask bit=1 → opaque, bit=0 → transparent.
// If no mask, all pixels are fully opaque.
IconRGBA DecodeIcl8(
    std::span<const uint8_t, 1024> icl8,
    std::optional<std::span<const uint8_t, 128>> mask);

} // namespace rsrc
```

### 3.2 — Create `tools/icon_extract/icon_decode.cpp`

```cpp
#include "icon_decode.h"
#include "mac_palette.h"

namespace rsrc {

IconRGBA DecodeIcl8(
    std::span<const uint8_t, 1024> icl8,
    std::optional<std::span<const uint8_t, 128>> mask)
{
    IconRGBA icon{};

    for (int row = 0; row < 32; ++row) {
        for (int col = 0; col < 32; ++col) {
            uint8_t paletteIdx = icl8[row * 32 + col];
            uint32_t rgba = kMacSystemPalette[paletteIdx];

            uint8_t alpha = 255;
            if (mask) {
                int byteIdx = row * 4 + (col / 8);
                int bitIdx  = 7 - (col % 8);
                alpha = (((*mask)[byteIdx] >> bitIdx) & 1) ? 255 : 0;
            }

            size_t px = static_cast<size_t>((row * 32 + col) * 4);
            icon.pixels[px + 0] = static_cast<uint8_t>((rgba >> 24) & 0xFF);
            icon.pixels[px + 1] = static_cast<uint8_t>((rgba >> 16) & 0xFF);
            icon.pixels[px + 2] = static_cast<uint8_t>((rgba >>  8) & 0xFF);
            icon.pixels[px + 3] = alpha;
        }
    }
    return icon;
}

} // namespace rsrc
```

### 3.3 — Add decode tests to `test/test_resource_fork.cpp`

```cpp
#include "icon_decode.h"
#include "mac_palette.h"

TEST_CASE("DecodeIcl8 — uniform color, no mask")
{
    // Fill icl8 with palette index 0 (should be white: 0xFFFFFFFF)
    std::array<uint8_t, 1024> icl8;
    icl8.fill(0);

    auto icon = rsrc::DecodeIcl8(icl8, std::nullopt);

    // First pixel should be R=0xFF, G=0xFF, B=0xFF, A=0xFF
    CHECK(icon.pixels[0] == 0xFF);
    CHECK(icon.pixels[1] == 0xFF);
    CHECK(icon.pixels[2] == 0xFF);
    CHECK(icon.pixels[3] == 0xFF);
}

TEST_CASE("DecodeIcl8 — black (index 255)")
{
    std::array<uint8_t, 1024> icl8;
    icl8.fill(255);

    auto icon = rsrc::DecodeIcl8(icl8, std::nullopt);

    // Black: R=0, G=0, B=0, A=0xFF
    CHECK(icon.pixels[0] == 0x00);
    CHECK(icon.pixels[1] == 0x00);
    CHECK(icon.pixels[2] == 0x00);
    CHECK(icon.pixels[3] == 0xFF);
}

TEST_CASE("DecodeIcl8 — mask zeroes alpha")
{
    std::array<uint8_t, 1024> icl8;
    icl8.fill(0);

    // All-zero mask → all transparent
    std::array<uint8_t, 128> mask;
    mask.fill(0x00);

    auto icon = rsrc::DecodeIcl8(icl8,
        std::span<const uint8_t, 128>(mask));

    CHECK(icon.pixels[3] == 0x00);  // alpha = 0
}

TEST_CASE("DecodeIcl8 — mask ones preserve alpha")
{
    std::array<uint8_t, 1024> icl8;
    icl8.fill(0);

    // All-ones mask → all opaque
    std::array<uint8_t, 128> mask;
    mask.fill(0xFF);

    auto icon = rsrc::DecodeIcl8(icl8,
        std::span<const uint8_t, 128>(mask));

    CHECK(icon.pixels[3] == 0xFF);  // alpha = 255
}

TEST_CASE("DecodeIcl8 — real data from sidecar")
{
    auto sidecar = ReadTestFile("test/fixtures/icon_128k_sidecar.bin");
    auto fork = ExtractForkFromSidecar(sidecar);
    auto resources = rsrc::ParseResourceFork(fork);

    auto icl8s = rsrc::FindByType(resources, 0x69636C38u);
    REQUIRE(!icl8s.empty());

    auto* first = icl8s.front();
    REQUIRE(first->data.size() == 1024);

    // Get mask from ICN# (second 128 bytes)
    auto* icn = rsrc::FindByTypeAndId(resources, 0x49434E23u, first->id);
    REQUIRE(icn != nullptr);
    REQUIRE(icn->data.size() == 256);

    std::span<const uint8_t, 128> maskSpan(icn->data.data() + 128, 128);
    std::span<const uint8_t, 1024> icl8Span(first->data.data(), 1024);

    auto icon = rsrc::DecodeIcl8(icl8Span, maskSpan);

    // Sanity: at least some opaque pixels exist
    bool hasOpaque = false;
    for (size_t i = 3; i < icon.pixels.size(); i += 4) {
        if (icon.pixels[i] == 0xFF) { hasOpaque = true; break; }
    }
    CHECK(hasOpaque);
}
```

### 3.4 — Add `icon_decode.cpp` to test sources

Add `tools/icon_extract/icon_decode.cpp` to the `tests` executable.

### Fence

- [ ] `tools/icon_extract/icon_decode.h` and `.cpp` exist
- [ ] All decode unit tests pass
- [ ] Full build clean
- [ ] Commit: `"icon-extract: phase 3 — icl8 decoder + tests"`

---

## Phase 4 — Entry Point, CLI, and CMake Build

Wire up the standalone executable: argument parsing, orchestration
logic, and the CMake target.

### 4.1 — Create `tools/icon_extract/png_text.h`

A small helper that writes a PNG with embedded iTXt metadata chunks.
STB image write has no text chunk support, so we use
`stbi_write_png_to_mem()` to get raw PNG bytes, splice in iTXt chunks
between IHDR and IDAT, then write the final result to disk.

```cpp
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include <filesystem>

namespace png {

// A key/value pair to embed as a PNG iTXt chunk (UTF-8).
struct TextChunk {
    std::string_view keyword;  // 1–79 bytes, Latin-1 (ASCII in practice)
    std::string_view text;     // UTF-8 value
};

// Write a 32×32 RGBA PNG with optional iTXt text chunks.
// Returns true on success.
bool WritePngWithText(
    const std::filesystem::path& path,
    int width, int height,
    std::span<const uint8_t> rgba,
    std::span<const TextChunk> textChunks);

} // namespace png
```

### 4.2 — Create `tools/icon_extract/png_text.cpp`

```cpp
#include "png_text.h"
#include <stb_image_write.h>
#include <cstring>
#include <fstream>
#include <vector>

namespace png {
namespace {

// CRC-32 over [buf, buf+len).  Same algorithm as the PNG spec.
uint32_t Crc32(const uint8_t* buf, size_t len)
{
    // Use the same CRC table approach as stb_image_write internally.
    static constexpr uint32_t kCrcTable[256] = { /* standard table */ };
    // (In practice, reuse stbiw__crc32 or compute inline.)
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i)
        crc = (crc >> 8) ^ kCrcTable[(crc ^ buf[i]) & 0xFF];
    return ~crc;
}

void WriteBE32(std::vector<uint8_t>& out, uint32_t v)
{
    out.push_back(static_cast<uint8_t>(v >> 24));
    out.push_back(static_cast<uint8_t>(v >> 16));
    out.push_back(static_cast<uint8_t>(v >>  8));
    out.push_back(static_cast<uint8_t>(v));
}

// Build a single iTXt chunk (uncompressed, UTF-8).
// Layout: keyword \0 compressionFlag(0) compressionMethod(0)
//         languageTag \0 translatedKeyword \0 text
std::vector<uint8_t> BuildITxtChunk(std::string_view keyword,
                                     std::string_view text)
{
    std::vector<uint8_t> chunk;

    // 4-byte length placeholder (filled at end)
    size_t lengthPos = chunk.size();
    chunk.resize(chunk.size() + 4);

    // Chunk type: "iTXt"
    chunk.push_back('i'); chunk.push_back('T');
    chunk.push_back('X'); chunk.push_back('t');

    // Keyword + null separator
    chunk.insert(chunk.end(), keyword.begin(), keyword.end());
    chunk.push_back(0);

    // Compression flag = 0, compression method = 0
    chunk.push_back(0);
    chunk.push_back(0);

    // Language tag (empty) + null
    chunk.push_back(0);

    // Translated keyword (empty) + null
    chunk.push_back(0);

    // UTF-8 text (no null terminator needed)
    chunk.insert(chunk.end(), text.begin(), text.end());

    // Fill in data length (everything after type, before CRC)
    uint32_t dataLen = static_cast<uint32_t>(
        chunk.size() - lengthPos - 4 - 4);  // minus length field and type
    chunk[lengthPos + 0] = static_cast<uint8_t>(dataLen >> 24);
    chunk[lengthPos + 1] = static_cast<uint8_t>(dataLen >> 16);
    chunk[lengthPos + 2] = static_cast<uint8_t>(dataLen >>  8);
    chunk[lengthPos + 3] = static_cast<uint8_t>(dataLen);

    // CRC over type + data
    uint32_t crc = Crc32(chunk.data() + lengthPos + 4,
                         chunk.size() - lengthPos - 4);
    WriteBE32(chunk, crc);

    return chunk;
}

} // namespace

bool WritePngWithText(
    const std::filesystem::path& path,
    int width, int height,
    std::span<const uint8_t> rgba,
    std::span<const TextChunk> textChunks)
{
    // Generate raw PNG in memory
    int len = 0;
    unsigned char* png = stbi_write_png_to_mem(
        rgba.data(), width * 4, width, height, 4, &len);
    if (!png) return false;

    if (textChunks.empty()) {
        // No metadata — just write directly
        std::ofstream f(path, std::ios::binary);
        if (!f) { STBIW_FREE(png); return false; }
        f.write(reinterpret_cast<char*>(png), len);
        STBIW_FREE(png);
        return f.good();
    }

    // Find insertion point: after IHDR chunk (8 sig + 4 len + 4 type + 13 data + 4 crc = 33)
    // PNG signature is 8 bytes, IHDR is always the first chunk.
    constexpr size_t kAfterIHDR = 8 + 4 + 4 + 13 + 4;  // = 33
    if (static_cast<size_t>(len) < kAfterIHDR) {
        STBIW_FREE(png);
        return false;
    }

    // Build output: [sig + IHDR] [iTXt chunks...] [rest of PNG]
    std::vector<uint8_t> output;
    output.reserve(static_cast<size_t>(len) + textChunks.size() * 64);

    output.insert(output.end(), png, png + kAfterIHDR);

    for (const auto& tc : textChunks) {
        auto itxt = BuildITxtChunk(tc.keyword, tc.text);
        output.insert(output.end(), itxt.begin(), itxt.end());
    }

    output.insert(output.end(), png + kAfterIHDR, png + len);
    STBIW_FREE(png);

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(output.data()),
            static_cast<std::streamsize>(output.size()));
    return f.good();
}

} // namespace png
```

### 4.3 — Create `tools/icon_extract/icon_extract.cpp`

```cpp
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
        "Usage: icon-extract [OPTIONS] FILE [FILE...]\n"
        "\n"
        "Options:\n"
        "  -o, --output-dir DIR    Write PNGs to DIR (default: .)\n"
        "  -v, --verbose           Print each extracted icon\n"
        "  -h, --help              Show this help\n");
}

std::vector<uint8_t> ReadFile(const fs::path& path)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto size = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    f.read(reinterpret_cast<char*>(buf.data()), size);
    return buf;
}

// Extract the raw resource fork from an AppleDouble sidecar blob.
std::vector<uint8_t> ForkFromSidecar(const std::vector<uint8_t>& blob)
{
    using appledouble::detail::ReadBE16;
    using appledouble::detail::ReadBE32;

    if (blob.size() < 26) return {};
    uint16_t numEntries = ReadBE16(blob.data() + 24);
    for (uint16_t i = 0; i < numEntries; ++i) {
        const uint8_t* e = blob.data() + 26 + i * 12;
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
constexpr uint32_t kICNType  = 0x49434E23u; // 'ICN#'

} // namespace

int main(int argc, char* argv[])
{
    fs::path outputDir = ".";
    bool verbose = false;
    std::vector<fs::path> inputs;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "-h" || arg == "--help") {
            PrintUsage();
            return 0;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
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

    fs::create_directories(outputDir);
    int totalExtracted = 0;

    for (const auto& inputPath : inputs) {
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

        std::string prefix;
        if (inputs.size() > 1)
            prefix = inputPath.stem().string() + "_";

        for (auto* icl8 : icl8s) {
            if (icl8->data.size() != 1024) {
                std::fprintf(stderr, "Warning: icl8 id=%d wrong size "
                             "(%zu), skipping\n", icl8->id, icl8->data.size());
                continue;
            }

            std::optional<std::span<const uint8_t, 128>> mask;
            auto* icn = rsrc::FindByTypeAndId(resources, kICNType, icl8->id);
            if (icn && icn->data.size() == 256) {
                mask = std::span<const uint8_t, 128>(
                    icn->data.data() + 128, 128);
            } else if (verbose) {
                std::fprintf(stderr, "Warning: no ICN# mask for icl8 "
                             "id=%d, using opaque\n", icl8->id);
            }

            std::span<const uint8_t, 1024> icl8Span(icl8->data.data(), 1024);
            auto icon = rsrc::DecodeIcl8(icl8Span, mask);

            auto outFile = outputDir /
                (prefix + "icon_" + std::to_string(icl8->id) + ".png");

            // Embed original filename and full path as iTXt metadata
            png::TextChunk textChunks[] = {
                {"Title",  basename},
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
```

### 4.4 — Add CMake target

Insert the following block in `CMakeLists.txt` immediately before the
`# Unit tests` section (before line 286):

```cmake
# ---------------------------------------------------------------------------
# Icon extraction tool
# ---------------------------------------------------------------------------
add_executable(icon-extract
    tools/icon_extract/icon_extract.cpp
    tools/icon_extract/resource_fork.cpp
    tools/icon_extract/icon_decode.cpp
    tools/icon_extract/png_text.cpp
    tools/icon_extract/stb_impl.cpp
    src/storage/appledouble.cpp
    src/storage/filename_encoding.cpp
    src/util/macroman.cpp
)
target_include_directories(icon-extract PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/tools/icon_extract"
    "${CMAKE_SOURCE_DIR}/libs/stb"
    "${CMAKE_SOURCE_DIR}/libs"
)
target_compile_options(icon-extract PRIVATE -Wall -Wextra -Werror)
set_source_files_properties(tools/icon_extract/stb_impl.cpp PROPERTIES
    COMPILE_FLAGS "-Wno-error"
)
```

### Fence

- [ ] `icon-extract` binary builds: `cmake --build --preset macos --target icon-extract`
- [ ] `./bld/macos/icon-extract --help` prints usage
- [ ] Output PNGs contain iTXt "Title" (basename) and "Source" (full path)
- [ ] Full build clean (all targets)
- [ ] Commit: `"icon-extract: phase 4 — CLI entry point, PNG metadata, and CMake target"`

---

## Phase 5 — Integration Test with Real Sidecar

Verify the tool end-to-end with the real Mac 128K icon sidecar.

### 5.1 — Run tool on test fixture

```bash
./bld/macos/icon-extract -v -o /tmp/icon_test \
    test/fixtures/icon_128k_sidecar.bin
```

Expected: one or more `icon_<id>.png` files written to `/tmp/icon_test`.

### 5.2 — Create `test/test_icon_extract.sh`

A shell-script integration test:

```bash
#!/bin/bash
set -euo pipefail

TOOL="./bld/macos/icon-extract"
FIXTURE="test/fixtures/icon_128k_sidecar.bin"
OUTDIR=$(mktemp -d)

trap "rm -rf $OUTDIR" EXIT

"$TOOL" -v -o "$OUTDIR" "$FIXTURE"

# At least one PNG was produced
COUNT=$(find "$OUTDIR" -name '*.png' | wc -l)
if [ "$COUNT" -lt 1 ]; then
    echo "FAIL: no PNGs produced"
    exit 1
fi

# Each PNG starts with the PNG magic bytes
for f in "$OUTDIR"/*.png; do
    MAGIC=$(xxd -l 4 -p "$f")
    if [ "$MAGIC" != "89504e47" ]; then
        echo "FAIL: $f is not a valid PNG"
        exit 1
    fi

    # Verify iTXt chunk with "Title" keyword is present
    if ! grep -q "Title" "$f"; then
        echo "FAIL: $f missing Title iTXt chunk"
        exit 1
    fi

    # Verify iTXt chunk with "Source" keyword is present
    if ! grep -q "Source" "$f"; then
        echo "FAIL: $f missing Source iTXt chunk"
        exit 1
    fi
done

echo "PASS: extracted $COUNT icon(s) with metadata"
```

### 5.3 — Verify manually

Open one of the PNGs to visually confirm it looks like the classic
Macintosh 128K icon (a compact Mac with a smiling face).

### Fence

- [ ] `test/test_icon_extract.sh` passes
- [ ] At least one PNG is produced from the test fixture
- [ ] Full build and test suite still green
- [ ] Commit: `"icon-extract: phase 5 — integration test"`

---

## Notes

- The `test/fixtures/` directory is new.  Add it to `.gitignore`
  exclusions if necessary (binary test fixtures should be committed).
- The palette table in Phase 1 is the longest single piece of code
  (~256 hex literals).  Reference the well-documented Mac system
  palette from TN1023 / Inside Macintosh to ensure correctness.
- If `appledouble.cpp` has additional link dependencies beyond
  `filename_encoding.cpp` and `macroman.cpp`, discover and add them
  in Phase 4.
