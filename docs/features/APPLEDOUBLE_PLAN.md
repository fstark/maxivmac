# AppleDouble Storage Library — Implementation Plan

Design: [APPLEDOUBLE_DESIGN.md](APPLEDOUBLE_DESIGN.md)
Spec: [APPLEDOUBLE.md](APPLEDOUBLE.md)

This plan covers the **standalone library only** — no integration with
`extn_extfs.cpp`.  Integration is a separate, later plan.

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Constants, structs, FourCC, typemap.def loader | DONE |
| 2 | Filename escaping (HostNameFromMac / MacNameFromHost) | DONE |
| 3 | Text conversion (MacRomanFromUTF8File / UTF8FromMacRoman) | DONE |
| 4 | Sidecar binary format (parse / serialise) | DONE |
| 5 | Finder info (Get / Set / lazy sidecar lifecycle) | DONE |
| 6 | Resource fork sub-file access (read / write / resize) | DONE |
| 7 | Date handling, IsSidecar, Delete/Rename, GetFileInfo | DONE |
| 8 | Build integration (CMakeLists.txt) | DONE |

Build gate: `cmake --build bld/macos`
Test gate:  `./bld/macos/tests`

---

## Phase 1 — Constants, structs, FourCC, type/creator .def file

Set up the public header, the `assets/typemap.def` data file, and the
loader.  Everything here is pure data + a small file parser — no
filesystem sidecar logic yet.  This compiles on its own and gives
every later phase a foundation to build on.

### 1.1 — Create `src/storage/appledouble.h`

The public header, exactly as specified in Design §2.  For this phase,
only declare the parts that don't depend on later implementation:

```cpp
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace appledouble {

/* ── Constants ────────────────────────────────────── */

inline constexpr uint32_t kMacEpochOffset = 2082844800u; // 1904→1970

inline constexpr uint32_t kAppleDoubleMagic   = 0x00051607u;
inline constexpr uint32_t kAppleDoubleVersion  = 0x00020000u;
inline constexpr uint32_t kEntryIdResourceFork = 2u;
inline constexpr uint32_t kEntryIdFinderInfo   = 9u;
inline constexpr uint32_t kFinderInfoSize      = 32u;
inline constexpr uint32_t kHeaderSize          = 26u;  // magic+ver+filler+nEntries
inline constexpr uint32_t kEntryDescSize       = 12u;  // per entry descriptor

/* ── FourCC helper ────────────────────────────────── */

constexpr uint32_t FourCC(const char (&s)[5]) {
    return (static_cast<uint32_t>(static_cast<uint8_t>(s[0])) << 24) |
           (static_cast<uint32_t>(static_cast<uint8_t>(s[1])) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(s[2])) <<  8) |
            static_cast<uint32_t>(static_cast<uint8_t>(s[3]));
}

/* ── Finder info ──────────────────────────────────── */

struct FinderInfo {
    uint32_t type    = 0;
    uint32_t creator = 0;
    uint16_t flags   = 0;

    bool operator==(const FinderInfo &) const = default;
};

/* ── Per-file metadata snapshot ───────────────────── */

struct FileInfo {
    FinderInfo finder;
    uint32_t   dataForkSize = 0;
    uint32_t   rsrcForkSize = 0;
    uint32_t   crDate       = 0;
    uint32_t   modDate      = 0;
    bool       isText       = false;
};

/* ── Type/creator mapping ─────────────────────────── */

int LoadTypeMappings(const std::filesystem::path &defPath);
FinderInfo FinderInfoFromExtension(std::string_view extension);

/* ── Sidecar path ─────────────────────────────────── */

std::filesystem::path SidecarPathFor(const std::filesystem::path &hostPath);

/* ── Finder info access ───────────────────────────── */

FinderInfo GetFinderInfo(const std::filesystem::path &hostPath);
void SetFinderInfo(const std::filesystem::path &hostPath,
                   const FinderInfo &info);

/* ── Resource fork access ─────────────────────────── */

uint32_t ResourceForkSize(const std::filesystem::path &hostPath);

std::vector<uint8_t> ReadResourceFork(
    const std::filesystem::path &hostPath,
    uint32_t offset, uint32_t count);

void WriteResourceFork(const std::filesystem::path &hostPath,
                       uint32_t offset,
                       std::span<const uint8_t> data);

void SetResourceForkSize(const std::filesystem::path &hostPath,
                         uint32_t newSize);

/* ── Composite query ──────────────────────────────── */

FileInfo GetFileInfo(const std::filesystem::path &hostPath);

/* ── Date handling ────────────────────────────────── */

uint32_t MacDateFromFileTime(std::filesystem::file_time_type ft);
void SetModDate(const std::filesystem::path &hostPath, uint32_t macDate);

/* ── Text conversion (whole-file) ─────────────────── */

std::vector<uint8_t> MacRomanFromUTF8File(
    const std::filesystem::path &hostPath);
uint32_t MacRomanSizeFromUTF8File(
    const std::filesystem::path &hostPath);
std::string UTF8FromMacRoman(std::span<const uint8_t> macRoman);

/* ── Filename escaping ────────────────────────────── */

std::string HostNameFromMac(std::string_view macName);
std::string MacNameFromHost(std::string_view hostName);

/* ── Directory enumeration ────────────────────────── */

bool IsSidecar(std::string_view name);

/* ── Sidecar lifecycle ────────────────────────────── */

bool DeleteWithSidecar(const std::filesystem::path &hostPath);
bool RenameWithSidecar(const std::filesystem::path &oldPath,
                       const std::filesystem::path &newPath);

} // namespace appledouble
```

### 1.2 — Create `assets/typemap.def`

The extension → type/creator mapping data file, following the same
convention as `traps.def` and `errors.def`:

```
# Extension → type/creator mapping for the AppleDouble library.
# Format: <extension> <type> <creator>

.txt   TEXT ttxt
.text  TEXT ttxt
.c     TEXT KAHL
.h     TEXT KAHL
.p     TEXT KAHL
.r     TEXT KAHL
.cpp   TEXT KAHL
.hpp   TEXT KAHL
.s     TEXT KAHL
.asm   TEXT KAHL
.md    TEXT ttxt
.csv   TEXT ttxt
.htm   TEXT MOSS
.html  TEXT MOSS
.jpg   JPEG ogle
.jpeg  JPEG ogle
.gif   GIFf ogle
.bmp   BMPf ogle
.png   PNGf ogle
.bin   BINA hDmp
```

### 1.3 — Create `src/storage/appledouble.cpp` (type table loader + stubs)

Create the file with ① the internal `TypeMapping` struct and
module-level `s_typeMappings` vector, ② `LoadTypeMappings()` which
parses the `.def` file (skip `#` comments and blank lines, split each
line into 3 whitespace-separated fields: extension, type FourCC,
creator FourCC), ③ `FinderInfoFromExtension()` (case-insensitive
lookup into `s_typeMappings`, returns `????/????` if not found), and
④ `SidecarPathFor()`.  All other functions are stubs (empty body or
`return {}`) so the library compiles.

```cpp
#include "storage/appledouble.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace appledouble {
namespace {

struct TypeMapping {
    std::string ext;      // lowercased, e.g. ".txt"
    uint32_t    type;
    uint32_t    creator;
};

std::vector<TypeMapping> s_typeMappings;

constexpr FinderInfo kUnknownFinder = {
    FourCC("????"), FourCC("????"), 0
};

} // anonymous namespace
```

`LoadTypeMappings(path)`: open file, read line by line, skip lines
starting with `#` or that are blank, split into 3 tokens (extension,
type string, creator string), lowercase the extension, convert the
4-char type/creator strings to `uint32_t` via `FourCC`, push into
`s_typeMappings`.  Return count or -1 on file open failure.

`FinderInfoFromExtension`: lowercase the input, linear scan
`s_typeMappings`.  Return `kUnknownFinder` if not found or if
`s_typeMappings` is empty.

`SidecarPathFor`: `path.parent_path() / ("._" + path.filename().string())`.

All other declared functions: stub bodies that compile
(`return {};`, `return 0;`, `return false;`, or empty).

### 1.4 — Create empty `src/storage/filename_encoding.h/cpp`

Minimal files: header with the `HostNameFromMac` / `MacNameFromHost`
declarations (or just `#include "storage/appledouble.h"` — the
declarations are already there).  The `.cpp` includes the header and is
otherwise empty.  These files exist so the build system can reference
them from phase 1.

### 1.5 — Create empty `src/storage/text_convert.h/cpp`

Same pattern.  Header + empty `.cpp`.

### 1.6 — Tests

Create `test/test_appledouble.cpp`.  The `LoadTypeMappings` tests
write a temporary `.def` file, load it, then query
`FinderInfoFromExtension`.  Also test loading the real
`assets/typemap.def`.

```cpp
#include <doctest/doctest.h>
#include "storage/appledouble.h"
#include <fstream>

using namespace appledouble;

TEST_CASE("FourCC produces correct values") {
    CHECK(FourCC("TEXT") == 0x54455854u);
    CHECK(FourCC("ttxt") == 0x74747874u);
    CHECK(FourCC("????") == 0x3F3F3F3Fu);
}

TEST_CASE("LoadTypeMappings and FinderInfoFromExtension") {
    // Write a temp .def file
    auto p = std::filesystem::temp_directory_path() / "test_typemap.def";
    {
        std::ofstream f(p);
        f << "# test mappings\n"
          << ".txt  TEXT ttxt\n"
          << ".jpg  JPEG ogle\n"
          << "\n"
          << "# comment\n"
          << ".bin  BINA hDmp\n";
    }
    int count = LoadTypeMappings(p);
    CHECK(count == 3);

    auto fi = FinderInfoFromExtension(".txt");
    CHECK(fi.type == FourCC("TEXT"));
    CHECK(fi.creator == FourCC("ttxt"));

    fi = FinderInfoFromExtension(".jpg");
    CHECK(fi.type == FourCC("JPEG"));
    CHECK(fi.creator == FourCC("ogle"));

    std::filesystem::remove(p);
}

TEST_CASE("FinderInfoFromExtension is case insensitive") {
    // Assumes LoadTypeMappings already called in previous test
    auto lower = FinderInfoFromExtension(".txt");
    auto upper = FinderInfoFromExtension(".TXT");
    auto mixed = FinderInfoFromExtension(".Txt");
    CHECK(lower == upper);
    CHECK(lower == mixed);
}

TEST_CASE("FinderInfoFromExtension unknown returns ????") {
    auto fi = FinderInfoFromExtension(".xyz");
    CHECK(fi.type == FourCC("????"));
    CHECK(fi.creator == FourCC("????"));
}

TEST_CASE("LoadTypeMappings returns -1 for missing file") {
    CHECK(LoadTypeMappings("/nonexistent/typemap.def") == -1);
}

TEST_CASE("LoadTypeMappings loads actual assets/typemap.def") {
    int count = LoadTypeMappings("assets/typemap.def");
    CHECK(count >= 19);  // at least the 19 shipped mappings
    auto fi = FinderInfoFromExtension(".txt");
    CHECK(fi.type == FourCC("TEXT"));
}

TEST_CASE("SidecarPathFor basic") {
    namespace fs = std::filesystem;
    CHECK(SidecarPathFor("/dir/foo.txt") == fs::path("/dir/._foo.txt"));
    CHECK(SidecarPathFor("/a/b/README") == fs::path("/a/b/._README"));
    CHECK(SidecarPathFor("plain.bin") == fs::path("._plain.bin"));
}
```

### Fence

- [ ] `assets/typemap.def` exists with 19+ mappings
- [ ] `src/storage/appledouble.h` exists with all declarations from Design §2
- [ ] `src/storage/appledouble.cpp` exists with `LoadTypeMappings`,
      `FinderInfoFromExtension`, `SidecarPathFor`, all other functions as stubs
- [ ] `src/storage/filename_encoding.h` and `.cpp` exist (minimal)
- [ ] `src/storage/text_convert.h` and `.cpp` exist (minimal)
- [ ] `test/test_appledouble.cpp` exists with FourCC, type mapping, and
      SidecarPathFor tests
- [ ] Build does NOT yet compile (CMakeLists.txt updated in Phase 8) —
      but all files are syntactically valid
- [ ] Commit: `"appledouble: phase 1 — constants, structs, typemap.def"`

---

## Phase 2 — Filename escaping

Implement `HostNameFromMac`, `MacNameFromHost`, and `IsSidecar` in
`filename_encoding.cpp`.  These are pure string transformations with
no filesystem I/O — easy to test in isolation.  See Design §4.5.

### 2.1 — Implement `filename_encoding.cpp`

```cpp
#include "storage/appledouble.h"
#include <cctype>

namespace appledouble {
```

`HostNameFromMac(std::string_view macName) -> std::string`:
Iterate bytes.  If byte is in the escape set
`{ 0x22, 0x2A, 0x2F, 0x3A, 0x3C, 0x3E, 0x3F, 0x5C, 0x5E, 0x7C }`,
emit `'^'` + two uppercase hex digits.  Otherwise emit the byte
verbatim.

`MacNameFromHost(std::string_view hostName) -> std::string`:
Iterate.  On `'^'` followed by two hex digits, decode and emit the
byte.  Otherwise emit verbatim.

`IsSidecar(std::string_view name) -> bool`:
`return name.size() >= 2 && name[0] == '.' && name[1] == '_';`

The header `filename_encoding.h` can simply `#include "storage/appledouble.h"`
(declarations are there).  Or it can be a thin forwarding header — the
key thing is the `.cpp` provides the definitions.

### 2.2 — Tests

Add to `test/test_appledouble.cpp`:

```cpp
TEST_CASE("HostNameFromMac escapes POSIX-invalid characters") {
    CHECK(HostNameFromMac("My:File") == "My^3AFile");
    CHECK(HostNameFromMac("A/B") == "A^2FB");
    CHECK(HostNameFromMac("A^B") == "A^5EB");
    CHECK(HostNameFromMac("a\"b") == "a^22b");
    CHECK(HostNameFromMac("a*b") == "a^2Ab");
    CHECK(HostNameFromMac("a<b>c") == "a^3Cb^3Ec");
    CHECK(HostNameFromMac("a?b") == "a^3Fb");
    CHECK(HostNameFromMac("a\\b") == "a^5Cb");
    CHECK(HostNameFromMac("a|b") == "a^7Cb");
}

TEST_CASE("HostNameFromMac no-op on clean names") {
    CHECK(HostNameFromMac("readme.txt") == "readme.txt");
    CHECK(HostNameFromMac("") == "");
}

TEST_CASE("MacNameFromHost decodes ^XX sequences") {
    CHECK(MacNameFromHost("My^3AFile") == "My:File");
    CHECK(MacNameFromHost("A^5EB") == "A^B");
}

TEST_CASE("Filename escaping round-trips") {
    // All printable Mac Roman characters — verify round-trip
    for (int b = 0x20; b < 0x7F; ++b) {
        std::string mac(1, static_cast<char>(b));
        CHECK(MacNameFromHost(HostNameFromMac(mac)) == mac);
    }
    // High bytes
    for (int b = 0x80; b <= 0xFF; ++b) {
        std::string mac(1, static_cast<char>(b));
        CHECK(MacNameFromHost(HostNameFromMac(mac)) == mac);
    }
}

TEST_CASE("MacNameFromHost handles trailing caret gracefully") {
    CHECK(MacNameFromHost("abc^") == "abc^");
    CHECK(MacNameFromHost("abc^2") == "abc^2");
}

TEST_CASE("IsSidecar") {
    CHECK(IsSidecar("._foo.txt"));
    CHECK(IsSidecar("._"));
    CHECK_FALSE(IsSidecar("foo.txt"));
    CHECK_FALSE(IsSidecar(".hidden"));
    CHECK_FALSE(IsSidecar(""));
    CHECK_FALSE(IsSidecar("."));
}
```

### Fence

- [ ] `HostNameFromMac` / `MacNameFromHost` / `IsSidecar` implemented
- [ ] All filename escaping tests pass
- [ ] Commit: `"appledouble: phase 2 — filename escaping"`

---

## Phase 3 — Text conversion

Implement `MacRomanFromUTF8File`, `MacRomanSizeFromUTF8File`,
`UTF8FromMacRoman` in `text_convert.cpp`.  Depends on the existing
`mac_roman.h` functions.  See Design §4.6.

### 3.1 — Implement `text_convert.cpp`

```cpp
#include "storage/appledouble.h"
#include "platform/common/mac_roman.h"
#include <fstream>

namespace appledouble {
```

**UTF-8 decoding helper** (internal): a small function that consumes
bytes from a `string_view` / buffer and yields Unicode code points.
Handle 1–4 byte sequences.  Invalid sequences yield U+FFFD (which
will map to `'?'` in Mac Roman).

`MacRomanSizeFromUTF8File(path) -> uint32_t`:
Read entire file into memory.  Decode UTF-8 code points.  Count each
code point as 1 byte (Mac Roman is single-byte).

`MacRomanFromUTF8File(path) -> vector<uint8_t>`:
Same decode loop, but call `UniCodePoint2MacRoman(cp)` per code point.
If it returns 0 (unmappable), emit `'?'`.

`UTF8FromMacRoman(span<const uint8_t>) -> string`:
For each byte, call `MacRoman2UniCodeData` to get the UTF-8 form.
Append to output string.  (Use the existing `MacRoman2UniCodeSize`
to know how many bytes each character produces.)

### 3.2 — Tests

Add to `test/test_appledouble.cpp`.  The file-based tests create a
temporary file in `/tmp/`, write known UTF-8 content, then call
the conversion functions.

```cpp
#include <fstream>

namespace {
// Helper: write a temp file with given content, return its path
std::filesystem::path writeTempFile(std::string_view content,
                                    std::string_view name = "test.txt") {
    auto p = std::filesystem::temp_directory_path() / name;
    std::ofstream f(p, std::ios::binary);
    f.write(content.data(), content.size());
    return p;
}
} // namespace

TEST_CASE("MacRomanSizeFromUTF8File ASCII") {
    auto p = writeTempFile("hello");
    CHECK(MacRomanSizeFromUTF8File(p) == 5);
    std::filesystem::remove(p);
}

TEST_CASE("MacRomanSizeFromUTF8File multibyte") {
    // "café" = 63 61 66 C3 A9 = 5 UTF-8 bytes, 4 characters
    auto p = writeTempFile("caf\xC3\xA9");
    CHECK(MacRomanSizeFromUTF8File(p) == 4);
    std::filesystem::remove(p);
}

TEST_CASE("MacRomanSizeFromUTF8File empty") {
    auto p = writeTempFile("");
    CHECK(MacRomanSizeFromUTF8File(p) == 0);
    std::filesystem::remove(p);
}

TEST_CASE("MacRomanFromUTF8File content") {
    auto p = writeTempFile("caf\xC3\xA9");
    auto result = MacRomanFromUTF8File(p);
    REQUIRE(result.size() == 4);
    CHECK(result[0] == 'c');
    CHECK(result[1] == 'a');
    CHECK(result[2] == 'f');
    CHECK(result[3] == 0x8E); // é in Mac Roman
    std::filesystem::remove(p);
}

TEST_CASE("MacRomanFromUTF8File unmappable becomes ?") {
    // Emoji: U+1F600 (4 UTF-8 bytes) → '?'
    auto p = writeTempFile("\xF0\x9F\x98\x80");
    auto result = MacRomanFromUTF8File(p);
    REQUIRE(result.size() == 1);
    CHECK(result[0] == '?');
    std::filesystem::remove(p);
}

TEST_CASE("UTF8FromMacRoman ASCII") {
    std::vector<uint8_t> input = {'H', 'i'};
    CHECK(UTF8FromMacRoman(input) == "Hi");
}

TEST_CASE("UTF8FromMacRoman high byte") {
    // 0x8E = é in Mac Roman → UTF-8 C3 A9
    std::vector<uint8_t> input = {0x8E};
    CHECK(UTF8FromMacRoman(input) == "\xC3\xA9");
}

TEST_CASE("UTF8FromMacRoman empty") {
    CHECK(UTF8FromMacRoman({}) == "");
}

TEST_CASE("UTF8FromMacRoman round-trip all 256 bytes") {
    for (int b = 0; b < 256; ++b) {
        std::vector<uint8_t> input = {static_cast<uint8_t>(b)};
        auto utf8 = UTF8FromMacRoman(input);
        auto back = MacRomanFromUTF8File(
            writeTempFile(utf8, "rt_" + std::to_string(b) + ".txt"));
        REQUIRE(back.size() == 1);
        CHECK(back[0] == static_cast<uint8_t>(b));
    }
    // clean up temp files
    for (int b = 0; b < 256; ++b) {
        std::filesystem::remove(
            std::filesystem::temp_directory_path() /
            ("rt_" + std::to_string(b) + ".txt"));
    }
}
```

### Fence

- [ ] `MacRomanFromUTF8File`, `MacRomanSizeFromUTF8File`, `UTF8FromMacRoman`
      implemented in `text_convert.cpp`
- [ ] UTF-8 decoder handles 1–4 byte sequences and invalid input
- [ ] All text conversion tests pass
- [ ] Commit: `"appledouble: phase 3 — text conversion"`

---

## Phase 4 — Sidecar binary format

Implement the internal sidecar parser and serialiser.  These are the
building blocks for Finder info and resource fork access.  See
Design §3.1 and §4.7.

### 4.1 — Internal sidecar data structure

Add to `appledouble.cpp` (inside the anonymous namespace):

```cpp
struct SidecarEntry {
    uint32_t id     = 0;
    uint32_t offset = 0;  // from start of file
    uint32_t length = 0;
};

struct ParsedSidecar {
    bool valid = false;
    std::vector<SidecarEntry> entries;

    // Convenience: extracted data for the two entries we care about
    std::vector<uint8_t> finderInfoData;  // 32 bytes if present
    std::vector<uint8_t> resourceForkData;

    bool hasFinderInfo() const;
    bool hasResourceFork() const;
    uint32_t resourceForkOffset() const;  // file offset in sidecar
    uint32_t resourceForkLength() const;
};
```

### 4.2 — `parseSidecar(path) -> ParsedSidecar`

Internal function.  Reads the sidecar file, validates magic and
version, iterates entry descriptors, extracts Finder info and resource
fork data.  Returns `{.valid = false}` on any error (missing file,
bad magic, truncated data).  See Design §4.7 for the algorithm.

### 4.3 — `writeSidecar(path, optional<FinderInfo>, optional<span<uint8_t>> rsrcFork)`

Internal function.  Serialises a new sidecar with 0, 1, or 2 entries.
Lays out: header (26 bytes) → entry descriptors → Finder info data
(if present, 32 bytes) → resource fork data (if present).  All
multi-byte integers are big-endian.

If both arguments are empty/nullopt, does nothing (caller should
delete the sidecar instead).

### 4.4 — Big-endian helpers

Small `constexpr` functions in the anonymous namespace:

```cpp
constexpr uint32_t readBE32(const uint8_t *p);
constexpr uint16_t readBE16(const uint8_t *p);
void writeBE32(uint8_t *p, uint32_t v);
void writeBE16(uint8_t *p, uint16_t v);
```

### 4.5 — Tests

These tests create raw binary sidecar files in `/tmp/` and verify
parsing, or call `writeSidecar` and verify the output.

```cpp
TEST_CASE("parseSidecar rejects missing file") { ... }
TEST_CASE("parseSidecar rejects bad magic") { ... }
TEST_CASE("parseSidecar rejects truncated header") { ... }
TEST_CASE("parseSidecar reads Finder info entry") {
    // Manually construct a valid sidecar with just Finder info,
    // write to /tmp/, parse, verify finderInfoData matches.
}
TEST_CASE("parseSidecar reads resource fork entry") { ... }
TEST_CASE("parseSidecar reads both entries") { ... }
TEST_CASE("parseSidecar skips unknown entry IDs") { ... }
TEST_CASE("writeSidecar Finder info only round-trips") {
    // Write via writeSidecar, parse back, compare.
}
TEST_CASE("writeSidecar resource fork only round-trips") { ... }
TEST_CASE("writeSidecar both entries round-trips") { ... }
```

Because `parseSidecar` and `writeSidecar` are internal (anonymous
namespace), the tests need access.  Two options:

**Option A:** Move them to a `detail` namespace in a private header
`appledouble_internal.h` which only the test includes.

**Option B:** Make the tests go through the public API (`SetFinderInfo`
/ `GetFinderInfo` / `WriteResourceFork` / `ReadResourceFork`) which
aren't implemented yet.

→ Use **Option A**.  Create `src/storage/appledouble_internal.h` with
`parseSidecar` and `writeSidecar` declarations.  This header is not
part of the public API.

### Fence

- [ ] `appledouble_internal.h` exists with `parseSidecar` / `writeSidecar`
- [ ] `parseSidecar` correctly handles valid/invalid/truncated sidecars
- [ ] `writeSidecar` produces files that round-trip through `parseSidecar`
- [ ] Big-endian helpers tested implicitly via round-trip tests
- [ ] All sidecar format tests pass
- [ ] Commit: `"appledouble: phase 4 — sidecar binary format"`

---

## Phase 5 — Finder info (Get / Set / lazy creation)

Implement `GetFinderInfo` and `SetFinderInfo` with the lazy sidecar
creation/deletion logic.  See Design §4.1.

### 5.1 — `GetFinderInfo`

In `appledouble.cpp`:
1. Compute sidecar path via `SidecarPathFor`.
2. Call `parseSidecar`.  If valid and has Finder info entry, decode
   the 32-byte blob into a `FinderInfo` struct (type at offset 0,
   creator at offset 4, flags at offset 8 — all big-endian).
3. Otherwise, call `FinderInfoFromExtension` on the file's extension.

### 5.2 — `SetFinderInfo`

Implements Design §4.1 exactly:
1. Compute default from extension.
2. If new info equals default:
   - If sidecar exists with no resource fork → delete sidecar.
   - If sidecar exists with resource fork → rewrite sidecar keeping
     only resource fork.
   - Otherwise do nothing.
3. If new info differs from default:
   - If sidecar exists → rewrite with updated Finder info + existing
     resource fork (if any).
   - Otherwise → create new sidecar with Finder info only.

### 5.3 — Finder info serialisation helpers

Internal functions to convert between `FinderInfo` struct and the
32-byte blob (big-endian type, creator, flags + 22 zero bytes for
location, folder, and FXInfo).

### 5.4 — Tests

```cpp
TEST_CASE("GetFinderInfo with no sidecar returns extension default") {
    auto p = writeTempFile("hello", "test.txt");
    auto fi = GetFinderInfo(p);
    CHECK(fi.type == FourCC("TEXT"));
    CHECK(fi.creator == FourCC("ttxt"));
    std::filesystem::remove(p);
}

TEST_CASE("SetFinderInfo creates sidecar for non-default") {
    auto p = writeTempFile("hello", "test.txt");
    FinderInfo custom{FourCC("APPL"), FourCC("myap"), 0};
    SetFinderInfo(p, custom);
    CHECK(std::filesystem::exists(SidecarPathFor(p)));
    CHECK(GetFinderInfo(p) == custom);
    std::filesystem::remove(p);
    std::filesystem::remove(SidecarPathFor(p));
}

TEST_CASE("SetFinderInfo with default does not create sidecar") {
    auto p = writeTempFile("hello", "test_nosc.txt");
    auto def = FinderInfoFromExtension(".txt");
    SetFinderInfo(p, def);
    CHECK_FALSE(std::filesystem::exists(SidecarPathFor(p)));
    std::filesystem::remove(p);
}

TEST_CASE("SetFinderInfo back to default removes sidecar") {
    auto p = writeTempFile("hello", "test_rm.txt");
    FinderInfo custom{FourCC("APPL"), FourCC("myap"), 0};
    SetFinderInfo(p, custom);
    REQUIRE(std::filesystem::exists(SidecarPathFor(p)));
    SetFinderInfo(p, FinderInfoFromExtension(".txt"));
    CHECK_FALSE(std::filesystem::exists(SidecarPathFor(p)));
    std::filesystem::remove(p);
}

TEST_CASE("SetFinderInfo back to default keeps sidecar if resource fork exists") {
    // Setup: create sidecar with custom Finder info + resource fork
    // Then set Finder info to default → sidecar kept (has rsrc fork)
    // Verify GetFinderInfo returns default, sidecar still exists
}
```

### Fence

- [ ] `GetFinderInfo` / `SetFinderInfo` implemented
- [ ] Lazy creation: no sidecar for default Finder info
- [ ] Lazy deletion: sidecar removed when returning to default (no rsrc fork)
- [ ] Sidecar preserved when resource fork exists
- [ ] All Finder info tests pass
- [ ] Commit: `"appledouble: phase 5 — Finder info with lazy sidecar"`

---

## Phase 6 — Resource fork sub-file access

Implement `ReadResourceFork`, `WriteResourceFork`,
`SetResourceForkSize`, and `ResourceForkSize`.  See Design §4.2, §4.3.

### 6.1 — `ResourceForkSize`

Parse sidecar.  If valid and has resource fork entry, return its
length.  Otherwise return 0.

### 6.2 — `ReadResourceFork`

Implements Design §4.3: compute sidecar path, find the resource fork
entry, seek to `entry.dataOffset + offset`, read `min(count,
entry.length - offset)` bytes.  Uses direct file seek — does not load
the entire sidecar into memory.

### 6.3 — `WriteResourceFork`

Implements Design §4.2:
1. Parse sidecar (may not exist).
2. If the write is within existing fork bounds → seek and overwrite
   in place.
3. Otherwise → load existing fork data, resize, copy new data in,
   call `writeSidecar` to rewrite the file (preserving Finder info
   if present).

### 6.4 — `SetResourceForkSize`

1. Parse sidecar.
2. Load existing resource fork data (or start empty).
3. `resize(newSize, 0)` — truncates or zero-fills.
4. If `newSize == 0` and no Finder info override → delete sidecar.
5. If `newSize == 0` and Finder info override exists → rewrite sidecar
   with Finder info only.
6. Otherwise → rewrite sidecar with new fork + existing Finder info.

### 6.5 — Tests

```cpp
TEST_CASE("WriteResourceFork creates sidecar") {
    auto p = writeTempFile("data", "rf_create.txt");
    std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    WriteResourceFork(p, 0, data);
    CHECK(ResourceForkSize(p) == 10);
    // cleanup
}

TEST_CASE("ReadResourceFork matches written data") {
    auto p = writeTempFile("data", "rf_read.txt");
    std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
    WriteResourceFork(p, 0, data);
    auto result = ReadResourceFork(p, 0, 4);
    CHECK(result == data);
}

TEST_CASE("WriteResourceFork at offset grows fork") {
    auto p = writeTempFile("data", "rf_grow.txt");
    std::vector<uint8_t> data = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    WriteResourceFork(p, 20, data);
    CHECK(ResourceForkSize(p) == 25);
    // Bytes 0..19 should be zero
    auto gap = ReadResourceFork(p, 0, 20);
    CHECK(std::all_of(gap.begin(), gap.end(), [](uint8_t b){ return b == 0; }));
    // Bytes 20..24 should be our data
    CHECK(ReadResourceFork(p, 20, 5) == data);
}

TEST_CASE("ReadResourceFork clamps to fork size") {
    auto p = writeTempFile("data", "rf_clamp.txt");
    std::vector<uint8_t> data = {1, 2, 3};
    WriteResourceFork(p, 0, data);
    auto result = ReadResourceFork(p, 0, 100);
    CHECK(result.size() == 3);
}

TEST_CASE("ReadResourceFork beyond fork returns empty") {
    auto p = writeTempFile("data", "rf_beyond.txt");
    std::vector<uint8_t> data = {1, 2, 3};
    WriteResourceFork(p, 0, data);
    CHECK(ReadResourceFork(p, 100, 10).empty());
}

TEST_CASE("ReadResourceFork no sidecar returns empty") {
    auto p = writeTempFile("data", "rf_noscar.txt");
    CHECK(ReadResourceFork(p, 0, 10).empty());
    CHECK(ResourceForkSize(p) == 0);
}

TEST_CASE("WriteResourceFork in-place overwrite") {
    auto p = writeTempFile("data", "rf_inplace.txt");
    std::vector<uint8_t> initial(20, 0xFF);
    WriteResourceFork(p, 0, initial);
    std::vector<uint8_t> patch = {0x00, 0x00};
    WriteResourceFork(p, 5, patch);
    CHECK(ResourceForkSize(p) == 20); // size unchanged
    auto all = ReadResourceFork(p, 0, 20);
    CHECK(all[4] == 0xFF);
    CHECK(all[5] == 0x00);
    CHECK(all[6] == 0x00);
    CHECK(all[7] == 0xFF);
}

TEST_CASE("SetResourceForkSize truncate") {
    auto p = writeTempFile("data", "rf_trunc.txt");
    std::vector<uint8_t> data(100, 0xAA);
    WriteResourceFork(p, 0, data);
    SetResourceForkSize(p, 10);
    CHECK(ResourceForkSize(p) == 10);
    auto result = ReadResourceFork(p, 0, 10);
    CHECK(std::all_of(result.begin(), result.end(), [](uint8_t b){ return b == 0xAA; }));
}

TEST_CASE("SetResourceForkSize to zero removes sidecar") {
    auto p = writeTempFile("data", "rf_zero.txt");
    std::vector<uint8_t> data = {1, 2, 3};
    WriteResourceFork(p, 0, data);
    REQUIRE(std::filesystem::exists(SidecarPathFor(p)));
    SetResourceForkSize(p, 0);
    CHECK_FALSE(std::filesystem::exists(SidecarPathFor(p)));
}

TEST_CASE("SetResourceForkSize to zero keeps sidecar with Finder override") {
    auto p = writeTempFile("data", "rf_zero_fi.txt");
    std::vector<uint8_t> data = {1, 2, 3};
    WriteResourceFork(p, 0, data);
    FinderInfo custom{FourCC("APPL"), FourCC("myap"), 0};
    SetFinderInfo(p, custom);
    SetResourceForkSize(p, 0);
    CHECK(std::filesystem::exists(SidecarPathFor(p)));
    CHECK(ResourceForkSize(p) == 0);
    CHECK(GetFinderInfo(p) == custom);
}

TEST_CASE("Finder info + resource fork interaction") {
    auto p = writeTempFile("data", "rf_fi_both.txt");
    FinderInfo custom{FourCC("APPL"), FourCC("myap"), 0};
    SetFinderInfo(p, custom);
    std::vector<uint8_t> rsrc = {0xCA, 0xFE};
    WriteResourceFork(p, 0, rsrc);

    // Both present
    CHECK(GetFinderInfo(p) == custom);
    CHECK(ReadResourceFork(p, 0, 2) == rsrc);

    // Remove Finder info → resource fork stays
    SetFinderInfo(p, FinderInfoFromExtension(".txt"));
    CHECK(ReadResourceFork(p, 0, 2) == rsrc);
    CHECK(std::filesystem::exists(SidecarPathFor(p)));

    // Remove resource fork → sidecar gone (Finder is default now)
    SetResourceForkSize(p, 0);
    CHECK_FALSE(std::filesystem::exists(SidecarPathFor(p)));
}
```

### Fence

- [ ] `ReadResourceFork`, `WriteResourceFork`, `SetResourceForkSize`,
      `ResourceForkSize` all implemented
- [ ] In-place writes work for within-bounds cases
- [ ] Growth triggers full sidecar rewrite
- [ ] Sidecar lifecycle correct when resource fork + Finder info interact
- [ ] All resource fork tests pass
- [ ] Commit: `"appledouble: phase 6 — resource fork sub-file access"`

---

## Phase 7 — Date handling, lifecycle, GetFileInfo

Implement the remaining functions: `MacDateFromFileTime`, `SetModDate`,
`DeleteWithSidecar`, `RenameWithSidecar`, `GetFileInfo`.  These are
mostly thin wrappers around `std::filesystem` plus the sidecar
machinery built in earlier phases.  See Design §4.4.

### 7.1 — Date handling

`MacDateFromFileTime(file_time_type ft) -> uint32_t`:
Convert `file_time_type` to `time_t` (seconds since 1970), add
`kMacEpochOffset`.  Handle the clock conversion using
`std::chrono::file_clock::to_sys` (C++20) then
`std::chrono::system_clock::to_time_t`.

`SetModDate(path, macDate)`:
Subtract `kMacEpochOffset`, convert to `file_time_type`, call
`std::filesystem::last_write_time(path, ft)`.

### 7.2 — DeleteWithSidecar

1. If path is a directory → `std::filesystem::remove(path)` (fails if
   not empty, matching POSIX rmdir).  If `._<dirname>` sidecar exists
   in parent, remove it too.
2. If path is a file → remove the file and its sidecar (if any).
3. Return false on any `std::filesystem::filesystem_error`.

### 7.3 — RenameWithSidecar

1. `std::filesystem::rename(oldPath, newPath)`.
2. Compute old and new sidecar paths.  If old sidecar exists,
   `std::filesystem::rename(oldSidecar, newSidecar)`.
3. Return false on error.

### 7.4 — GetFileInfo

Implements Design §4.4: combines `last_write_time`, `GetFinderInfo`,
`ResourceForkSize`, `file_size`.  If type == `FourCC("TEXT")`, calls
`MacRomanSizeFromUTF8File` for `dataForkSize`.

### 7.5 — Tests

```cpp
TEST_CASE("MacDateFromFileTime round-trips through SetModDate") {
    auto p = writeTempFile("data", "date_rt.txt");
    uint32_t macDate = 3'600'000'000u; // some date in 2018
    SetModDate(p, macDate);
    auto ft = std::filesystem::last_write_time(p);
    uint32_t result = MacDateFromFileTime(ft);
    // Allow ±1 second for rounding
    CHECK(result >= macDate - 1);
    CHECK(result <= macDate + 1);
    std::filesystem::remove(p);
}

TEST_CASE("DeleteWithSidecar removes file and sidecar") {
    auto p = writeTempFile("data", "del_test.txt");
    FinderInfo custom{FourCC("APPL"), FourCC("myap"), 0};
    SetFinderInfo(p, custom);
    REQUIRE(std::filesystem::exists(p));
    REQUIRE(std::filesystem::exists(SidecarPathFor(p)));
    CHECK(DeleteWithSidecar(p));
    CHECK_FALSE(std::filesystem::exists(p));
    CHECK_FALSE(std::filesystem::exists(SidecarPathFor(p)));
}

TEST_CASE("DeleteWithSidecar file with no sidecar") {
    auto p = writeTempFile("data", "del_nsc.txt");
    CHECK(DeleteWithSidecar(p));
    CHECK_FALSE(std::filesystem::exists(p));
}

TEST_CASE("DeleteWithSidecar empty directory") {
    auto d = std::filesystem::temp_directory_path() / "del_emptydir_test";
    std::filesystem::create_directory(d);
    CHECK(DeleteWithSidecar(d));
    CHECK_FALSE(std::filesystem::exists(d));
}

TEST_CASE("DeleteWithSidecar non-empty directory fails") {
    auto d = std::filesystem::temp_directory_path() / "del_fulldir_test";
    std::filesystem::create_directories(d);
    std::ofstream(d / "child.txt") << "x";
    CHECK_FALSE(DeleteWithSidecar(d));
    // cleanup
    std::filesystem::remove_all(d);
}

TEST_CASE("RenameWithSidecar moves file and sidecar") {
    auto old_p = writeTempFile("data", "ren_old.txt");
    FinderInfo custom{FourCC("APPL"), FourCC("myap"), 0};
    SetFinderInfo(old_p, custom);

    auto new_p = std::filesystem::temp_directory_path() / "ren_new.txt";
    CHECK(RenameWithSidecar(old_p, new_p));
    CHECK_FALSE(std::filesystem::exists(old_p));
    CHECK_FALSE(std::filesystem::exists(SidecarPathFor(old_p)));
    CHECK(std::filesystem::exists(new_p));
    CHECK(std::filesystem::exists(SidecarPathFor(new_p)));
    CHECK(GetFinderInfo(new_p) == custom);
    // cleanup
    DeleteWithSidecar(new_p);
}

TEST_CASE("RenameWithSidecar file with no sidecar") {
    auto old_p = writeTempFile("data", "ren_noscar.txt");
    auto new_p = std::filesystem::temp_directory_path() / "ren_noscar2.txt";
    CHECK(RenameWithSidecar(old_p, new_p));
    CHECK(std::filesystem::exists(new_p));
    CHECK_FALSE(std::filesystem::exists(SidecarPathFor(new_p)));
    std::filesystem::remove(new_p);
}

TEST_CASE("GetFileInfo non-TEXT file") {
    auto p = writeTempFile("hello", "info_bin.jpg");
    auto info = GetFileInfo(p);
    CHECK(info.finder.type == FourCC("JPEG"));
    CHECK(info.dataForkSize == 5);
    CHECK(info.rsrcForkSize == 0);
    CHECK(info.isText == false);
    CHECK(info.modDate > 0);
    std::filesystem::remove(p);
}

TEST_CASE("GetFileInfo TEXT file") {
    // "café" = 5 UTF-8 bytes but 4 Mac Roman
    auto p = writeTempFile("caf\xC3\xA9", "info_text.txt");
    auto info = GetFileInfo(p);
    CHECK(info.finder.type == FourCC("TEXT"));
    CHECK(info.dataForkSize == 4);
    CHECK(info.isText == true);
    std::filesystem::remove(p);
}

TEST_CASE("GetFileInfo with sidecar") {
    auto p = writeTempFile("hello", "info_scar.txt");
    FinderInfo custom{FourCC("APPL"), FourCC("myap"), 0};
    SetFinderInfo(p, custom);
    std::vector<uint8_t> rsrc = {1, 2, 3};
    WriteResourceFork(p, 0, rsrc);

    auto info = GetFileInfo(p);
    CHECK(info.finder == custom);
    CHECK(info.rsrcForkSize == 3);
    CHECK(info.dataForkSize == 5); // "hello" is not TEXT (APPL)
    CHECK(info.isText == false);
    DeleteWithSidecar(p);
}
```

### Fence

- [ ] `MacDateFromFileTime` and `SetModDate` implemented and tested
- [ ] `DeleteWithSidecar` handles files, files with sidecars, empty
      dirs, non-empty dirs
- [ ] `RenameWithSidecar` moves both file and sidecar
- [ ] `GetFileInfo` correctly composes all sub-queries
- [ ] All Phase 7 tests pass
- [ ] Commit: `"appledouble: phase 7 — dates, lifecycle, GetFileInfo"`

---

## Phase 8 — Build integration

Wire the new files into CMakeLists.txt so everything compiles and
tests run.

**Important:** This phase should be cherry-picked and combined with
Phase 1's commit if you prefer to build/test as you go.  Alternatively,
do Phase 8 first (add the files to CMakeLists.txt with stubs) and
then fill in implementations through phases 2–7.  Either way, the
build must be green at every commit.

### 8.1 — Add storage sources to `MINIVMAC_SOURCES`

In `CMakeLists.txt`, after the existing platform sections, add:

```cmake
    # Storage (AppleDouble library)
    src/storage/appledouble.cpp
    src/storage/filename_encoding.cpp
    src/storage/text_convert.cpp
```

### 8.2 — Add test sources to `tests` executable

```cmake
add_executable(tests
    ...existing sources...
    test/test_appledouble.cpp
    src/storage/appledouble.cpp
    src/storage/filename_encoding.cpp
    src/storage/text_convert.cpp
    src/platform/common/mac_roman.cpp
)
```

Note: `mac_roman.cpp` may already be pulled in by another test source.
If there are duplicate symbol errors, remove the redundant entry.

### 8.3 — Verify

```bash
cmake --preset macos
cmake --build bld/macos
./bld/macos/tests --test-case="*appledouble*,*FourCC*,*FinderInfo*,*HostName*,\
*MacName*,*IsSidecar*,*MacRoman*,*UTF8*,*Resource*,*Sidecar*,*Delete*,*Rename*,\
*GetFileInfo*,*MacDate*,*SetModDate*"
```

All tests pass, no warnings with `-Wall -Wextra -Werror`.

### Fence

- [ ] `CMakeLists.txt` includes all `src/storage/*.cpp` in both targets
- [ ] `cmake --build bld/macos` succeeds clean
- [ ] `./bld/macos/tests` passes all tests
- [ ] Commit: `"appledouble: phase 8 — build integration"`

---

## Recommended Execution Order

The phases above are written in dependency order, but the build can't
compile until Phase 8 adds the CMakeLists.txt entries.  In practice,
do **Phase 8 first** (add all source files as stubs to CMakeLists.txt),
then fill in Phase 1 through 7.  This way you can run the build and
tests after every phase.

Concrete order: **8 → 1 → 2 → 3 → 4 → 5 → 6 → 7**.

Each phase gets its own commit.  Phase 8 and Phase 1 can be combined
into a single commit if preferred.
