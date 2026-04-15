# AppleDouble Storage Library — Detailed Design

Implements the specification in [APPLEDOUBLE.md](APPLEDOUBLE.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

This document describes a **standalone, unit-tested library** with no
dependencies on the emulator core.  Integration with the Shared Drive
(`extn_extfs.cpp`) is a separate, later task — the API here is designed
so that integration *can* happen, but this document does not prescribe
how.

---

## 1. Directory Layout

```
assets/
    typemap.def            # Extension → type/creator mapping (runtime data)

src/
  storage/
    appledouble.h          # Public interface — the only header external code includes
    appledouble.cpp         # Binary format read/write, sidecar management
    filename_encoding.h    # ^XX filename escaping + Mac OS Roman ↔ UTF-8
    filename_encoding.cpp
    text_convert.h         # Whole-file UTF-8 ↔ Mac OS Roman conversion
    text_convert.cpp

test/
    test_appledouble.cpp   # Unit tests for the library
```

**Rationale:** A new `src/storage/` directory keeps the library
independent of both `src/core/` (emulator) and `src/platform/`.  The
library has no dependency on guest RAM, the wire bus, or SDL — it is
pure host-side C++23 operating on `std::filesystem::path` values and
byte buffers.

---

## 2. Public Interface

### `appledouble.h`

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

constexpr uint32_t kMacEpochOffset = 2082844800u; // 1904→1970

/* ── FourCC helper ────────────────────────────────── */

constexpr uint32_t FourCC(const char s[4]);

/* ── Finder info ──────────────────────────────────── */

struct FinderInfo {
    uint32_t type    = 0;   // e.g. FourCC("TEXT")
    uint32_t creator = 0;   // e.g. FourCC("ttxt")
    uint16_t flags   = 0;   // Finder flags
};

bool operator==(const FinderInfo &, const FinderInfo &) = default;

/* ── Per-file metadata snapshot ───────────────────── */

struct FileInfo {
    FinderInfo finder;
    uint32_t   dataForkSize = 0;     // host file size (or converted size for TEXT)
    uint32_t   rsrcForkSize = 0;     // from sidecar, 0 if none
    uint32_t   crDate       = 0;     // Mac epoch, from POSIX stat
    uint32_t   modDate      = 0;     // Mac epoch, from POSIX stat
    bool       isText       = false; // true when type == 'TEXT'
};

/* ── Type/creator mapping ─────────────────────────── */

/// Load extension → type/creator mappings from a .def file.
/// Must be called before FinderInfoFromExtension().  Returns the
/// number of mappings loaded, or -1 on error.
int LoadTypeMappings(const std::filesystem::path &defPath);

/// Return the default FinderInfo for a file based on its extension.
/// e.g. ".txt" → TEXT/ttxt, unknown → ????/????
/// Requires LoadTypeMappings() to have been called.
FinderInfo FinderInfoFromExtension(std::string_view extension);

/* ── Sidecar path ─────────────────────────────────── */

/// Return the sidecar path for a given host path.
/// e.g. "/dir/foo.txt" → "/dir/._foo.txt"
std::filesystem::path SidecarPathFor(const std::filesystem::path &hostPath);

/* ── Finder info access ───────────────────────────── */

/// Get Finder info for a file.  If a sidecar exists and contains a
/// Finder info entry, return that.  Otherwise derive type/creator
/// from the file extension.
FinderInfo GetFinderInfo(const std::filesystem::path &hostPath);

/// Set Finder info.  If the new info differs from the extension
/// default, write (or update) a sidecar.  If setting back to the
/// default and there's no resource fork, remove the sidecar.
void SetFinderInfo(const std::filesystem::path &hostPath,
                   const FinderInfo &info);

/* ── Resource fork access ─────────────────────────── */

/// Return the size of the resource fork (0 if none).
uint32_t ResourceForkSize(const std::filesystem::path &hostPath);

/// Read `count` bytes from the resource fork starting at `offset`.
/// Returns the bytes actually read (may be fewer if offset+count
/// exceeds the fork size, or empty if no resource fork exists).
std::vector<uint8_t> ReadResourceFork(
    const std::filesystem::path &hostPath,
    uint32_t offset, uint32_t count);

/// Write `data` into the resource fork at `offset`.
/// If the write extends past the current fork size, the fork grows.
/// Creates the sidecar if needed.
void WriteResourceFork(const std::filesystem::path &hostPath,
                       uint32_t offset,
                       std::span<const uint8_t> data);

/// Set the resource fork to exactly `newSize` bytes.
/// If shrinking, data beyond newSize is lost.
/// If growing, new bytes are zero-filled.
/// If newSize == 0 and no Finder info override exists, removes sidecar.
void SetResourceForkSize(const std::filesystem::path &hostPath,
                         uint32_t newSize);

/* ── Composite query ──────────────────────────────── */

/// Return a complete FileInfo for a host file.  Combines POSIX stat,
/// sidecar metadata, and text-conversion size in one call.
FileInfo GetFileInfo(const std::filesystem::path &hostPath);

/* ── Date handling ────────────────────────────────── */

/// Convert a POSIX file_time_type to Mac epoch seconds.
uint32_t MacDateFromFileTime(std::filesystem::file_time_type ft);

/// Set the modification date on the host file.
/// `macDate` is seconds since the Mac epoch (1904-01-01).
void SetModDate(const std::filesystem::path &hostPath, uint32_t macDate);

/* ── Text conversion (whole-file) ─────────────────── */

/// Convert a UTF-8 file to Mac OS Roman in memory.
/// Unmappable code points become '?'.
std::vector<uint8_t> MacRomanFromUTF8File(
    const std::filesystem::path &hostPath);

/// Compute the Mac OS Roman byte count of a UTF-8 file without
/// materialising the full conversion.  Still reads the entire file.
uint32_t MacRomanSizeFromUTF8File(
    const std::filesystem::path &hostPath);

/// Convert Mac OS Roman bytes to a UTF-8 string.
/// Used when the guest writes a TEXT file: the caller collects the
/// guest bytes then calls this to get UTF-8 for the host file.
std::string UTF8FromMacRoman(std::span<const uint8_t> macRoman);

/* ── Filename escaping ────────────────────────────── */

/// Escape a Mac filename for use on a POSIX filesystem.
/// Characters invalid on POSIX are encoded as ^XX.
std::string HostNameFromMac(std::string_view macName);

/// Decode a POSIX filename back to the original Mac name.
std::string MacNameFromHost(std::string_view hostName);

/* ── Directory enumeration ────────────────────────── */

/// Return true if `name` is a sidecar file (starts with "._").
bool IsSidecar(std::string_view name);

/* ── Sidecar lifecycle ────────────────────────────── */

/// Delete a file and its sidecar (if any).
/// For directories, deletes the directory only if empty (does NOT
/// recurse).  Returns false on error.
bool DeleteWithSidecar(const std::filesystem::path &hostPath);

/// Rename/move a file and its sidecar (if any).
bool RenameWithSidecar(const std::filesystem::path &oldPath,
                       const std::filesystem::path &newPath);

} // namespace appledouble
```

All functions are free functions in the `appledouble` namespace,
following the PascalCase convention for free functions.  No global
state — every call takes an explicit path.

### Design notes on the API

**Resource fork sub-file access.**  The Mac guest issues `_Read` and
`_Write` at arbitrary offsets and lengths within a resource fork.
A whole-file read/write API would force the caller to buffer the
entire fork for every small change.  Instead, the library provides
`ReadResourceFork(path, offset, count)`, `WriteResourceFork(path,
offset, data)` and `SetResourceForkSize(path, newSize)`.  When writing
within the current fork bounds, the library seeks directly into the
sidecar and overwrites just the affected bytes.  Only when the fork
must grow does the sidecar get fully rewritten (to update the entry
length in the header).

**Naming convention.**  Function names follow the NeXTstep `XFromY`
pattern where applicable: `MacRomanFromUTF8File`, `UTF8FromMacRoman`,
`HostNameFromMac`, `MacNameFromHost`, `FinderInfoFromExtension`,
`MacDateFromFileTime`.  This reads naturally at call sites:
`auto text = MacRomanFromUTF8File(path)`.

**Directory sidecars.**  Directories do not have resource forks or
data forks, but Finder stores DInfo/DXInfo for every directory:
icon position, view setting (list/icon), window rectangle, scroll
position.  Without persisting these, every folder's contents would
appear stacked at (0,0) — visually unusable.  The 32-byte DInfo/DXInfo
structure is identical in layout to FInfo/FXInfo, so the existing
`GetFinderInfo` / `SetFinderInfo` API handles directories with no
special-casing: the sidecar is named `._<dirname>` in the parent
directory.  There is no ambiguity with files named `dirname` because
the sidecar prefix makes the name `._dirname`, which is distinct.

**DeleteWithSidecar on directories.**  Follows POSIX `rmdir` semantics:
deletes the directory only if it is empty.  The caller (Shared Drive)
is responsible for emptying it first if recursive deletion is desired.
The HFS `_Delete` trap also requires the directory to be empty, so this
matches Mac semantics exactly.

**Type/creator mapping is data-driven.**  The extension → type/creator
table lives in `assets/typemap.def`, loaded once at startup via
`LoadTypeMappings()`.  This follows the same pattern as `traps.def`
and `errors.def` — runtime data that can be edited without
recompiling.  The Shared Drive does not need direct access to the
table — it calls `GetFinderInfo()` or `GetFileInfo()` which handle
the defaulting logic internally.  When the Shared Drive is eventually
integrated, its hardcoded `s_typeMap[]` can be removed.

---

## 3. Internal Data

The library has **minimal in-memory state**: a single module-level
vector holding the type/creator mappings loaded from
`assets/typemap.def`.  Everything else is stateless — every function
operates on paths passed as arguments and returns results by value.
All persistent state lives on the filesystem (the sidecar files).

### 3.1 AppleDouble sidecar binary layout

```
Offset  Size  Field
──────  ────  ─────
  0       4   Magic number     (0x00051607)
  4       4   Version          (0x00020000 = v2)
  8      16   Filler           (zeroed)
 24       2   Number of entries (1 or 2)
 26      12   Entry descriptor #1  (ID, offset, length)
[38      12   Entry descriptor #2  (ID, offset, length)]
 ..     var   Entry data (Finder info = 32 bytes, resource fork = variable)
```

Each entry descriptor:

```
Offset  Size  Field
──────  ────  ─────
  0       4   Entry ID    (2 = resource fork, 9 = Finder info)
  4       4   Data offset (from start of file)
  8       4   Data length
```

Finder info entry (32 bytes, matching the Mac `FInfo` + `FXInfo` structs):

```
Offset  Size  Field
──────  ────  ─────
  0       4   fdType
  4       4   fdCreator
  8       2   fdFlags
 10       4   fdLocation.v/h
 14       2   fdFldr
 16      16   FXInfo (extended Finder info, zeroed initially)
```

### 3.2 Sidecar naming

For a host file named `foo.txt`, the sidecar is `._foo.txt` in the
same directory.  The `._` prefix is the standard AppleDouble convention
used by macOS itself.

### 3.3 Type/creator mapping — `assets/typemap.def`

The mapping lives in an external `.def` file alongside `traps.def` and
`errors.def`, following the same convention: plain text, one entry per
line, `#` comments, blank lines ignored.

File format:

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

Loaded at startup by `LoadTypeMappings("assets/typemap.def")`.
Stored in a module-level `std::vector<TypeMapping>`:

```cpp
struct TypeMapping {
    std::string ext;      // ".txt", ".jpg", etc. (lowercased)
    uint32_t    type;     // FourCC("TEXT"), etc.
    uint32_t    creator;  // FourCC("ttxt"), etc.
};

static std::vector<TypeMapping> s_typeMappings;
```

Lookup (`FinderInfoFromExtension`) is case-insensitive: the extension
argument is lowercased before scanning the vector.  Unknown extensions
return `????/????`.  If `LoadTypeMappings` was never called, every
extension returns `????/????`.

---

## 4. Key Algorithms

### 4.1 Lazy sidecar creation (SetFinderInfo)

```
function SetFinderInfo(path, info):
    defaultInfo = FinderInfoFromExtension(path.extension())
    sidecar     = SidecarPathFor(path)

    if info == defaultInfo:
        if sidecar exists AND sidecar has no resource fork:
            delete sidecar
        elif sidecar exists AND sidecar has resource fork:
            rewrite sidecar with resource fork only
        return

    if sidecar exists:
        update Finder info entry in place (always 32 bytes)
    else:
        create sidecar with header + Finder info entry only
```

### 4.2 Resource fork sub-file write

```
function WriteResourceFork(path, offset, data):
    sidecar = SidecarPathFor(path)
    existing = parseSidecar(sidecar)       // may be empty

    if existing has resource fork entry:
        forkEntry = existing.resourceForkEntry
        endPos = offset + len(data)
        if endPos <= forkEntry.length:
            // In-place write: seek to data offset and overwrite
            seekAndWrite(sidecar, forkEntry.dataOffset + offset, data)
            return

    // Fork must grow (or sidecar doesn't exist yet): full rewrite
    fork = existing.resourceFork or []     // current fork bytes
    if offset + len(data) > len(fork):
        fork.resize(offset + len(data), 0)
    copy data into fork at offset

    writeSidecar(sidecar, existing.finderInfo, fork)
```

If the write stays within the existing fork size, the library can
seek directly to the resource fork data offset inside the sidecar and
overwrite just the affected bytes — no need to rewrite the whole file.
Only when the fork grows (changing the entry's length field in the
header) does the library need to rewrite the sidecar.  Shrinking via
`SetResourceForkSize` also requires a rewrite.

### 4.3 Resource fork sub-file read

```
function ReadResourceFork(path, offset, count):
    sidecar = SidecarPathFor(path)
    entry = findEntry(sidecar, entryID=2)  // resource fork
    if entry not found: return []

    // Clamp to actual fork bounds
    available = max(0, entry.length - offset)
    toRead = min(count, available)

    return readBytesFromSidecar(sidecar, entry.dataOffset + offset, toRead)
```

Like in-bounds writes, reads seek directly into the sidecar file
without loading the entire fork into memory.

### 4.4 GetFileInfo (composite)

```
function GetFileInfo(path):
    mtime     = last_write_time(path)
    finder    = GetFinderInfo(path)    // sidecar or extension default
    rsrcSize  = ResourceForkSize(path)

    result.finder       = finder
    result.crDate       = MacDateFromFileTime(mtime)
    result.modDate      = MacDateFromFileTime(mtime)
    result.rsrcForkSize = rsrcSize

    if finder.type == FourCC("TEXT"):
        result.dataForkSize = MacRomanSizeFromUTF8File(path)
        result.isText       = true
    else:
        result.dataForkSize = file_size(path)
        result.isText       = false

    return result
```

### 4.5 Filename escaping

```
function HostNameFromMac(macName):
    output = ""
    for each byte b in macName:
        if b in {0x22, 0x2a, 0x2f, 0x3a, 0x3c, 0x3e, 0x3f, 0x5c, 0x5e, 0x7c}:
            output += '^' + upperHex(b)   // e.g. '^3A'
        else:
            output += char(b)
    return output

function MacNameFromHost(hostName):
    output = ""
    i = 0
    while i < len(hostName):
        if hostName[i] == '^' AND i + 2 < len(hostName):
            output += char(parseHex(hostName[i+1..i+2]))
            i += 3
        else:
            output += hostName[i]
            i += 1
    return output
```

### 4.6 Text conversion

```
function MacRomanSizeFromUTF8File(path):
    count = 0
    for each Unicode code point in file(path) decoded as UTF-8:
        count += 1   // Mac OS Roman is always 1 byte per character
    return count

function MacRomanFromUTF8File(path):
    output = []
    for each Unicode code point cp in file(path) decoded as UTF-8:
        macByte = UniCodePoint2MacRoman(cp)   // existing function
        output.append(macByte != 0 ? macByte : '?')
    return output

function UTF8FromMacRoman(macBytes):
    output = ""
    for each byte b in macBytes:
        output += UTF8SequenceForMacRoman(b)  // uses existing tables
    return output
```

Both file-reading functions read the entire file.  The caller is
expected to cache the result (per the spec: "the Shared Drive catalog
is expected to cache converted sizes").

### 4.7 Sidecar parsing (internal)

```
function parseSidecar(sidecarPath):
    data = read entire file
    if len(data) < 26: return empty
    if bigEndian32(data[0..3]) != 0x00051607: return empty

    nEntries = bigEndian16(data[24..25])
    result = {}
    for i in 0..nEntries-1:
        off = 26 + i * 12
        id   = bigEndian32(data[off..off+3])
        dOff = bigEndian32(data[off+4..off+7])
        dLen = bigEndian32(data[off+8..off+11])
        result[id] = {offset: dOff, length: dLen, data: data[dOff..dOff+dLen]}

    return result
```

---

## 5. Reused Infrastructure

| Component | Location | Used for |
|-----------|----------|----------|
| `UniCodePoint2MacRoman()` | `src/platform/common/mac_roman.h` | `MacRomanFromUTF8File`: maps Unicode code point → Mac OS Roman byte |
| `MacRoman2UniCodeData()` | `src/platform/common/mac_roman.h` | `UTF8FromMacRoman`: maps Mac OS Roman bytes → UTF-8 |
| `.def` file convention | `assets/traps.def`, `assets/errors.def` | Pattern for `assets/typemap.def` — runtime data, one entry per line |
| `std::filesystem` | Standard library | Path manipulation, `stat`, `last_write_time`, `file_size` |
| `doctest` | `libs/doctest/` | Unit test framework |

The `kMacEpochOffset` constant and `FourCC()` helper are defined in
`appledouble.h` — they duplicate nothing, as the copies in
`extn_extfs.cpp` are file-local statics that will be removed at
integration time.

---

## 6. Build Integration

### CMakeLists.txt changes

Add the new source files to `MINIVMAC_SOURCES`:

```cmake
    # Storage (AppleDouble library)
    src/storage/appledouble.cpp
    src/storage/filename_encoding.cpp
    src/storage/text_convert.cpp
```

Add the test file to the `tests` executable:

```cmake
add_executable(tests
    ...
    test/test_appledouble.cpp
    src/storage/appledouble.cpp
    src/storage/filename_encoding.cpp
    src/storage/text_convert.cpp
    src/platform/common/mac_roman.cpp   # text_convert depends on this
)
```

No new external dependencies.  The library uses only:
- `<filesystem>`, `<span>`, `<string_view>` (C++23 standard library)
- `mac_roman.h` (project-internal, for text conversion)

---

## 7. Dependency Diagram

```
┌─────────────────────────┐
│    appledouble.h        │  Public API
└────────────┬────────────┘
             │ implemented by
             ▼
┌─────────────────────────┐
│    appledouble.cpp      │  Sidecar binary format, Finder info,
│                         │  resource fork, type/creator mapping,
│                         │  file lifecycle (delete/rename)
└────────────┬────────────┘
             │ uses
     ┌───────┴───────┐
     ▼               ▼
┌──────────┐  ┌──────────────┐
│ filename │  │ text_convert  │
│ encoding │  │   .h/cpp      │
│  .h/cpp  │  └──────┬───────┘
└──────────┘         │ uses
                     ▼
              ┌──────────────┐
              │ mac_roman.h  │  (existing, in platform/common/)
              └──────────────┘
```

Arrows point downward = dependency direction.  No cycles.  Nothing
in this diagram depends on the emulator core.

---

## 8. Testing

### Test file: `test/test_appledouble.cpp`

Uses doctest.  Tests operate on a temporary directory created in
`/tmp/` per test case (cleaned up in teardown).

### Test cases

**Sidecar binary format:**
- Write a Finder info entry, read it back, verify magic/version/offsets
- Write a resource fork entry, read it back, verify content
- Write both entries, read back both, verify no overlap
- Parse a sidecar with unknown entry IDs (skip gracefully)
- Reject truncated or corrupt sidecars (bad magic, short reads)

**Lazy sidecar creation:**
- `SetFinderInfo` with default type/creator → no sidecar created
- `SetFinderInfo` with custom type/creator → sidecar created
- `SetFinderInfo` back to default (no resource fork) → sidecar removed
- `SetFinderInfo` back to default (resource fork exists) → sidecar kept
  with resource fork only

**Resource fork sub-file access:**
- Write 10 bytes at offset 0 → sidecar created, `ResourceForkSize` = 10
- Read back same 10 bytes at offset 0 → data matches
- Write 5 bytes at offset 20 → fork grows to 25 (gap zero-filled)
- Read 5 bytes at offset 20 → returns the written data
- Read 100 bytes at offset 0 when fork is 25 → returns 25 bytes
- Read at offset beyond fork size → returns empty
- `SetResourceForkSize(path, 0)` with no Finder override → sidecar removed
- `SetResourceForkSize(path, 0)` with Finder override → sidecar kept
- Overwrite bytes in middle of existing fork → only changed bytes differ

**Finder info + resource fork interaction:**
- File has both → sidecar contains 2 entries
- Remove Finder info (set to default), resource fork stays → 1 entry
- Remove resource fork, Finder info stays → 1 entry
- Remove both → sidecar deleted

**Type/creator mapping:**
- `FinderInfoFromExtension(".txt")` → TEXT/ttxt
- `FinderInfoFromExtension(".jpg")` → JPEG/ogle
- `FinderInfoFromExtension(".xyz")` → ????/????
- Case insensitive: `FinderInfoFromExtension(".TXT")` → TEXT/ttxt

**Filename escaping:**
- Round-trip: `MacNameFromHost(HostNameFromMac(x)) == x` for all
  printable Mac characters
- Colon: `HostNameFromMac("My:File")` → `"My^3AFile"`
- Caret: `HostNameFromMac("A^B")` → `"A^5EB"`
- No-op: `HostNameFromMac("readme.txt")` → `"readme.txt"`

**Text conversion:**
- `MacRomanSizeFromUTF8File`: ASCII file → size == host size
- `MacRomanSizeFromUTF8File`: `"café"` (5 UTF-8 bytes) → 4
- `MacRomanSizeFromUTF8File`: emoji → replaced with `?`, size correct
- `MacRomanSizeFromUTF8File`: empty file → 0
- `MacRomanFromUTF8File` → verify byte-level content for known inputs
- `UTF8FromMacRoman`: ASCII bytes → identical string
- `UTF8FromMacRoman`: byte 0x8A (ä) → correct UTF-8 sequence
- `UTF8FromMacRoman({})` → empty string
- Round-trip: `MacRomanFromUTF8(UTF8FromMacRoman(x)) == x` for all
  256 Mac OS Roman byte values

**File lifecycle:**
- `DeleteWithSidecar` on file → removes both data file and sidecar
- `DeleteWithSidecar` on file with no sidecar → succeeds
- `DeleteWithSidecar` on empty directory → removes directory
- `DeleteWithSidecar` on non-empty directory → returns false
- `RenameWithSidecar` → moves both file and sidecar
- `RenameWithSidecar` on file with no sidecar → moves file only

**Date handling:**
- `SetModDate` with known Mac epoch value → `last_write_time` matches
- `MacDateFromFileTime` round-trips through `SetModDate`
- `GetFileInfo` returns `modDate` matching `last_write_time`

**Enumeration:**
- `IsSidecar("._foo.txt")` → true
- `IsSidecar("foo.txt")` → false
- `IsSidecar("._")` → true
- `IsSidecar(".hidden")` → false

**GetFileInfo composite:**
- Non-TEXT file: `dataForkSize` == host file size
- TEXT file: `dataForkSize` == Mac OS Roman converted size
- File with sidecar: `finder.type/creator` from sidecar
- File without sidecar: `finder.type/creator` from extension default
