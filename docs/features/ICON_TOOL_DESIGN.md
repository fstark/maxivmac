# Icon Extraction Tool — Detailed Design

Implements the specification in [ICON_TOOL.md](ICON_TOOL.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

```
tools/
  icon_extract/
    icon_extract.cpp      # Entry point, arg parsing, orchestration (~80 lines)
    resource_fork.h       # Resource fork parser (header-only, ~60 lines)
    resource_fork.cpp     # Resource fork parser implementation (~120 lines)
    mac_palette.h         # constexpr Mac 256-color system palette table
    stb_impl.cpp          # STB_IMAGE_WRITE_IMPLEMENTATION define
```

A new top-level `tools/` directory keeps standalone utilities separate
from the emulator.  The icon extractor has its own STB implementation
file so its compilation unit is independent of the emulator's.

---

## 2. Public Interface

No public header is consumed by the emulator.  The tool is fully
standalone.  Internal interfaces:

### resource_fork.h

```cpp
#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rsrc {

struct Resource {
    uint32_t type;      // FourCC, e.g. 'icl8', 'ICN#'
    int16_t  id;
    std::string name;   // may be empty
    std::vector<uint8_t> data;
};

// Parse a raw resource fork blob into individual resources.
// Returns all resources found, or empty vector on parse failure.
std::vector<Resource> ParseResourceFork(std::span<const uint8_t> fork);

// Filter helpers
std::vector<const Resource*> FindByType(
    const std::vector<Resource>& resources, uint32_t type);

const Resource* FindByTypeAndId(
    const std::vector<Resource>& resources, uint32_t type, int16_t id);

} // namespace rsrc
```

### mac_palette.h

```cpp
#pragma once
#include <array>
#include <cstdint>

// Standard Macintosh 8-bit system palette (256 entries).
// Each entry is packed RGBA (R in high byte, A in low byte).
inline constexpr std::array<uint32_t, 256> kMacSystemPalette = { /* ... */ };
```

---

## 3. Integration Points

This tool has **zero integration with the emulator runtime**.  It reuses
three existing `.cpp` files (plus their headers) by compiling them into
its own binary:

| # | File | What it provides | Modification needed |
|---|------|-----------------|-------------------|
| 1 | `src/storage/appledouble.cpp` | `ReadResourceFork()`, `SidecarPathFor()` | None |
| 2 | `src/storage/appledouble_internal.h` | `ReadBE32()`, `ReadBE16()`, `ParseSidecar()` | None |
| 3 | `src/storage/filename_encoding.cpp` | Dependency of appledouble.cpp | None |
| 4 | `src/util/macroman.cpp` | Dependency of filename_encoding | None |
| 5 | `libs/stb/stb_image_write.h` | `stbi_write_png()` | None |

No existing files are modified.

---

## 4. Internal State

No persistent state.  The tool is a linear pipeline:

```
input path → read sidecar → raw fork bytes → parse resource map
  → for each icl8: find ICN# mask → decode palette → composite RGBA → write PNG
```

Working data per icon:

```cpp
struct IconRGBA {
    std::array<uint8_t, 32 * 32 * 4> pixels;  // 4096 bytes RGBA
};
```

---

## 5. Key Algorithms

### 5.1 Resource Fork Parsing

The Mac resource fork binary layout (all big-endian):

```
Offset 0:   uint32_t dataOffset      — offset to resource data area
Offset 4:   uint32_t mapOffset       — offset to resource map
Offset 8:   uint32_t dataLength
Offset 12:  uint32_t mapLength
...
Map + 24:   uint16_t typeListOffset  — relative to map start
Map + 26:   uint16_t nameListOffset  — relative to map start

Type list (at map + typeListOffset):
  uint16_t numTypes - 1
  For each type:
    uint32_t type           — FourCC
    uint16_t numResources - 1
    uint16_t refListOffset  — relative to type list start

Reference list entry (12 bytes each):
    int16_t  id
    uint16_t nameOffset     — relative to name list, or 0xFFFF
    uint8_t  attributes
    uint24_t dataOffset     — relative to data area start
    uint32_t reserved
```

Resource data entries are prefixed by a 4-byte big-endian length.

Algorithm:

```
fn ParseResourceFork(fork: bytes) -> Vec<Resource>:
    dataOff  = BE32(fork[0..4])
    mapOff   = BE32(fork[4..8])
    map      = fork[mapOff..]
    typeListOff = BE16(map[24..26])
    nameListOff = BE16(map[26..28])
    typeList = map[typeListOff..]
    numTypes = BE16(typeList[0..2]) + 1

    for i in 0..numTypes:
        entry    = typeList[2 + i*8 ..]
        type     = BE32(entry[0..4])
        numRes   = BE16(entry[4..6]) + 1
        refOff   = BE16(entry[6..8])  // relative to typeList start
        refList  = typeList[refOff..]

        for j in 0..numRes:
            ref      = refList[j*12..]
            id       = BE16_signed(ref[0..2])
            nameOff  = BE16(ref[2..4])
            dataOff3 = (ref[5] << 16) | (ref[6] << 8) | ref[7]
            // actual data at fork[dataOff + dataOff3]
            len      = BE32(fork[dataOff + dataOff3 .. +4])
            data     = fork[dataOff + dataOff3 + 4 .. + 4 + len]
            emit Resource{type, id, data}
```

### 5.2 icl8 → RGBA Conversion

```
fn DecodeIcl8(icl8: [u8; 1024], mask: Option<[u8; 128]>) -> IconRGBA:
    for row in 0..32:
        for col in 0..32:
            paletteIdx = icl8[row * 32 + col]
            rgba       = kMacSystemPalette[paletteIdx]

            if mask is Some(m):
                // mask is second 128 bytes of ICN# (the mask half)
                byteIdx = row * 4 + (col / 8)
                bitIdx  = 7 - (col % 8)
                alpha   = if (m[byteIdx] >> bitIdx) & 1 then 255 else 0
            else:
                alpha = 255

            pixels[(row*32 + col)*4 + 0] = (rgba >> 24) & 0xFF  // R
            pixels[(row*32 + col)*4 + 1] = (rgba >> 16) & 0xFF  // G
            pixels[(row*32 + col)*4 + 2] = (rgba >>  8) & 0xFF  // B
            pixels[(row*32 + col)*4 + 3] = alpha
```

### 5.3 ICN# Mask Extraction

An `ICN#` resource is 256 bytes: the first 128 bytes are the 1-bit
icon image, the second 128 bytes are the 1-bit mask.  Only the mask
half (bytes 128–255) is used for alpha compositing.

---

## 6. Reused Infrastructure

| Existing component | How it's used |
|---|---|
| `appledouble::ReadResourceFork()` | Reads raw resource fork bytes from AppleDouble sidecar given a host file path |
| `appledouble::SidecarPathFor()` | Computes `._filename` path — used when the input IS the data file (tool also accepts sidecar paths directly) |
| `appledouble::detail::ParseSidecar()` | Direct sidecar parsing when the user passes the `._` file directly |
| `appledouble::detail::ReadBE32/16()` | Big-endian helpers, reused in resource fork parser |
| `stbi_write_png()` | Writes RGBA buffer to PNG file |

No duplication.  The resource fork parser is new code — nothing in the
codebase currently parses the internal resource map structure.

---

## 7. Build Integration

Add to bottom of `CMakeLists.txt`, before the test section:

```cmake
# ---------------------------------------------------------------------------
# Icon extraction tool
# ---------------------------------------------------------------------------
add_executable(icon-extract
    tools/icon_extract/icon_extract.cpp
    tools/icon_extract/resource_fork.cpp
    tools/icon_extract/stb_impl.cpp
    src/storage/appledouble.cpp
    src/storage/filename_encoding.cpp
    src/util/macroman.cpp
)
target_include_directories(icon-extract PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/tools/icon_extract"
    "${CMAKE_SOURCE_DIR}/libs/stb"
)
target_compile_options(icon-extract PRIVATE -Wall -Wextra -Werror)
set_source_files_properties(tools/icon_extract/stb_impl.cpp PROPERTIES
    COMPILE_FLAGS "-Wno-error"
)
```

The tool uses C++23 (inherited from the project-level setting) and links
only the C++ standard library — no SDL, ImGui, or OpenGL.

---

## 8. Dependency Diagram

```
icon_extract.cpp
  ├── resource_fork.h / .cpp      (new: resource map parser)
  │     └── appledouble_internal.h  (ReadBE32/16 helpers)
  ├── mac_palette.h                (new: compile-time palette table)
  ├── appledouble.h / .cpp         (existing: sidecar reader)
  │     ├── appledouble_internal.h
  │     └── filename_encoding.cpp
  │           └── macroman.cpp
  └── stb_image_write.h           (existing: PNG encoder)
```

All arrows point downward — no cycles.

---

## 9. Input Handling

The tool accepts two kinds of input paths:

1. **AppleDouble sidecar** (`._Filename`) — detected by magic number
   `0x00051607` at offset 0.  Parsed directly via
   `appledouble::detail::ParseSidecar()`.

2. **Host file path** (`Filename`) — the tool computes the sidecar via
   `SidecarPathFor()` and reads from there.

3. **Raw resource fork file** — if the file doesn't match AppleDouble
   magic and has no corresponding sidecar, treat the entire file as a
   raw resource fork blob.  This supports resource forks extracted by
   other tools (e.g. `derez`, `macbinary`).

Detection logic:

```cpp
auto bytes = ReadFile(path);
if (bytes.size() >= 4 && ReadBE32(bytes.data()) == kAppleDoubleMagic) {
    // It's a sidecar — parse it, extract resource fork entry
} else if (auto sidecar = SidecarPathFor(path); std::filesystem::exists(sidecar)) {
    // It's a host file — read its sidecar
} else {
    // Treat as raw resource fork
}
```

---

## 10. Command-Line Interface

```
Usage: icon-extract [OPTIONS] FILE [FILE...]

Options:
  -o, --output-dir DIR    Write PNGs to DIR (default: current directory)
  -v, --verbose           Print each extracted icon
  -h, --help              Show this help

Output files: icon_<id>.png (e.g. icon_128.png)
When extracting from multiple input files, icons are prefixed with the
input filename stem: <stem>_icon_<id>.png
```

Argument parsing is hand-rolled (no library) — the interface is trivial
enough that `getopt`-style parsing in ~20 lines suffices.

---

## 11. Mac System Palette

The standard Macintosh 256-color system palette is defined in Apple
Technical Note TN1023 and the Color QuickDraw documentation.  It's a
fixed, well-known table used by all `icl8` resources.

The palette is structured as:

- Indices 0–214: a 6×6×6 color ramp (216 entries, not in RGB order)
- Indices 215–254: grayscale ramp (40 entries, white→black)
- Index 255: black

The table will be defined as a `constexpr std::array<uint32_t, 256>`
with pre-computed RGBA values (R in bits 24–31, G in bits 16–23,
B in bits 8–15, A in bits 0–7 = 0xFF for all entries).

Source: Inside Macintosh: Imaging With QuickDraw, Chapter 4 (Color
QuickDraw), and the `clut` resource ID 8 from the System file.

---

## 12. Testing

| Test | Location | Framework |
|------|----------|-----------|
| Resource fork parser unit tests | `test/test_resource_fork.cpp` | doctest (existing) |
| icl8 decode + mask composite | `test/test_resource_fork.cpp` | doctest |
| Integration: known sidecar → expected PNG | `test/test_icon_extract.sh` | shell script + `cmp` |

Test data: a small AppleDouble sidecar with a known `icl8` + `ICN#`
pair, committed to `test/data/`.  The expected PNG output is also
committed for byte-exact comparison.

The resource fork parser tests will be added to the existing `tests`
executable by adding `tools/icon_extract/resource_fork.cpp` and a new
test file to its source list.
