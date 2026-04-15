# HostVolume — Detailed Design

Implements the specification in [HOSTVOLUME.md](HOSTVOLUME.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

```
src/
  storage/
    appledouble.h              # (existing) AppleDouble library API
    appledouble.cpp            # (existing)
    host_volume.h              # Public interface — catalog + fork I/O
    host_volume.cpp            # Implementation
test/
    test_host_volume.cpp       # Unit tests
```

`HostVolume` lives in `src/storage/` alongside the AppleDouble library.
It depends on AppleDouble but has **no dependency on the emulator core**
— it knows nothing about guest RAM, registers, or the wire bus.  This
keeps it testable in isolation and lets `extn_extfs.cpp` remain the sole
boundary between guest and host.

---

## 2. Public Interface

### `host_volume.h`

```cpp
#pragma once

#include "storage/appledouble.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace storage {

/* ── Error codes (match Mac OS File Manager) ──────── */

enum class FMErr : int16_t {
    kNoErr      =   0,
    kFnfErr     = -43,  // file not found
    kDupFNErr   = -48,  // duplicate filename
    kParamErr   = -50,  // bad parameter
    kRfNumErr   = -51,  // bad file reference number
    kIoErr      = -36,  // I/O error
    kDirNFErr   = -120, // directory not found
    kFBsyErr    = -47,  // file busy (dir not empty)
    kWPrErr     = -44,  // volume locked (optional, future)
};

/* ── Catalog entry ────────────────────────────────── */

struct CatalogEntry {
    uint32_t    cnid        = 0;
    uint32_t    parentDirID = 0;
    bool        isDirectory = false;
    std::string hostPath;           // absolute host path
    std::string macName;            // Mac OS Roman, ≤31 bytes

    /* Cached metadata (populated from AppleDouble at scan time) */
    uint32_t    type        = 0;
    uint32_t    creator     = 0;
    uint16_t    finderFlags = 0;
    uint32_t    dataForkSize = 0;   // Mac-visible size (converted for TEXT)
    uint32_t    rsrcForkSize = 0;
    uint32_t    crDate      = 0;    // Mac epoch
    uint32_t    modDate     = 0;    // Mac epoch
    bool        isText      = false;
};

/* ── Open fork descriptor ─────────────────────────── */

enum class ForkType { Data, Resource };

/* ── HostVolume ───────────────────────────────────── */

class HostVolume {
public:
    /* Mount the volume.  Scans the directory tree and populates the
       catalog.  Calls appledouble::LoadTypeMappings() if not already
       loaded.  Returns false if the directory doesn't exist. */
    bool mount(const std::filesystem::path &hostDir);

    /* True after a successful mount(). */
    bool isMounted() const;

    /* The root directory's CNID (always kRootDirID). */
    static constexpr uint32_t kRootParentID = 1;
    static constexpr uint32_t kRootDirID    = 2;

    /* ── Catalog queries ──────────────────────────── */

    const CatalogEntry *findByCNID(uint32_t cnid) const;

    const CatalogEntry *findByName(uint32_t parentDirID,
                                   std::string_view macName) const;

    const CatalogEntry *nthChild(uint32_t parentDirID,
                                 int index) const;

    int childCount(uint32_t parentDirID) const;

    /* Total file count and byte count (for GetVol). */
    void volumeStats(uint32_t &outFiles, uint32_t &outBytes) const;

    /* ── File/directory creation ──────────────────── */

    /* Create an empty file.  Returns the new CNID, or 0 on error.
       `errOut` receives the Mac error code. */
    uint32_t createFile(uint32_t parentDirID,
                        std::string_view macName,
                        FMErr &errOut);

    uint32_t createDir(uint32_t parentDirID,
                       std::string_view macName,
                       FMErr &errOut);

    /* ── Deletion ─────────────────────────────────── */

    FMErr remove(uint32_t parentDirID, std::string_view macName);

    /* ── Move / rename ────────────────────────────── */

    FMErr move(uint32_t srcDirID,
               std::string_view macName,
               uint32_t dstDirID);

    /* ── Metadata ─────────────────────────────────── */

    FMErr setFileInfo(uint32_t cnid, uint32_t type, uint32_t creator);

    /* ── Fork I/O ─────────────────────────────────── */

    /* Open a fork.  Returns a handle (>0), or 0 on error.
       For resource forks: the handle is bookkeeping only — actual
       I/O goes through the AppleDouble library by path. */
    uint32_t openFork(uint32_t cnid, ForkType fork,
                      uint32_t &outSize, FMErr &errOut);

    FMErr readFork(uint32_t handle, uint32_t offset,
                   std::span<uint8_t> buf, uint32_t &outRead);

    FMErr writeFork(uint32_t handle, uint32_t offset,
                    std::span<const uint8_t> data,
                    uint32_t &outWritten);

    void closeFork(uint32_t handle);

    /* ── Working directories ──────────────────────── */

    uint32_t openWD(uint32_t dirID);
    uint32_t wdToDirID(uint32_t wdRef) const;
    void closeWD(uint32_t wdRef);

    /* ── TEXT conversion stats ────────────────────── */

    struct TextStats {
        uint64_t conversions = 0;   // number of MacRomanFromUTF8File calls
        uint64_t bytesIn     = 0;   // total UTF-8 bytes read
        uint64_t bytesOut    = 0;   // total Mac Roman bytes produced
    };

    TextStats textConversionStats() const;
    void resetTextConversionStats();

private:
    /* ── Internal state ───────────────────────────── */

    std::filesystem::path         rootPath_;
    bool                          mounted_ = false;
    std::vector<CatalogEntry>     catalog_;
    uint32_t                      nextCNID_ = 16;

    /* Open file handles */
    struct OpenFork {
        uint32_t cnid    = 0;
        ForkType fork    = ForkType::Data;
        FILE    *fp      = nullptr;  // non-null for data forks only
    };
    std::unordered_map<uint32_t, OpenFork> openForks_;
    uint32_t                      nextHandle_ = 1;

    /* Working directories */
    std::unordered_map<uint32_t, uint32_t> wdTable_; // wdRef → dirID
    uint32_t                      nextWD_ = 0x8000;

    /* TEXT conversion tracking */
    mutable TextStats             textStats_;

    /* ── Internal helpers ─────────────────────────── */

    void scanDirectory(const std::filesystem::path &hostDir,
                       uint32_t parentDirID);

    CatalogEntry *mutableFindByCNID(uint32_t cnid);
    std::string resolveParentPath(uint32_t parentDirID) const;
    void invalidateTextSize(CatalogEntry &entry);
};

} // namespace storage
```

### Design decisions

**Namespace.** The class lives in `namespace storage`, alongside the
existing `namespace appledouble`.  This avoids namespace collisions
and keeps the dependency direction clear: `storage::HostVolume` calls
`appledouble::*` functions.

**Error type.** `FMErr` wraps standard Mac OS File Manager error codes
as an `enum class`.  The dispatch layer in `extn_extfs.cpp` converts
these to the positive uint16_t values the register protocol expects
(e.g. `FMErr::kFnfErr` → `regResult = 43`).

**Fork handles.** Data forks keep a `FILE*` for seek/read/write.
Resource forks store only the CNID in the handle table — actual reads
and writes delegate to `appledouble::ReadResourceFork()` /
`WriteResourceFork()`, which are path+offset based.  This asymmetry is
intentional: the AppleDouble library already handles sub-file access
within the sidecar, so there is no need to duplicate that logic with
a `FILE*`.

**No guest RAM access.** `HostVolume` never calls `get_vm_byte()` or
`put_vm_byte()`.  All data passes through `std::span<uint8_t>` buffers.
Guest-memory marshalling stays in `extn_extfs.cpp`.

---

## 3. Integration Points

### 3.1 `extn_extfs.cpp` — becomes a thin dispatch shell

The entire body of `ExtnExtFSDispatch()` is rewritten.  Instead of
file-scope statics and inline filesystem calls, each `case` becomes
a ~5–15 line function that:

1. Reads register parameters.
2. Calls one or two `HostVolume` methods.
3. Writes results back to register parameters.

Example — `kExtFSGetCatInfo` (indexed enumeration):

```cpp
case kExtFSGetCatInfo: {
    uint32_t dirID = regParam[0];
    int32_t index  = static_cast<int32_t>(regParam[1]);
    uint32_t nameBuf = regParam[2];

    if (index > 0) {
        auto *e = s_volume.nthChild(dirID, index);
        if (!e) { regResult = 43; break; }
        regParam[0] = e->cnid;
        regParam[1] = e->isDirectory ? 0x10u : 0u;
        regParam[2] = e->isDirectory
            ? static_cast<uint32_t>(s_volume.childCount(e->cnid))
            : e->dataForkSize;
        regParam[3] = e->parentDirID;
        regParam[4] = e->rsrcForkSize;
        if (nameBuf) writePascalString(nameBuf, e->macName);
        regResult = 0;
    }
    // ... (index == 0 case for directory-self info)
    break;
}
```

The following file-scope statics are **removed** from `extn_extfs.cpp`:

| Removed | Replaced by |
|---------|-------------|
| `s_catalog` (vector) | `HostVolume::catalog_` |
| `s_nextCNID` | `HostVolume::nextCNID_` |
| `s_openFiles` (map) | `HostVolume::openForks_` |
| `s_nextHandle` | `HostVolume::nextHandle_` |
| `s_wdTable` (map) | `HostVolume::wdTable_` |
| `s_nextWD` | `HostVolume::nextWD_` |
| `s_sharedDir` (string) | `HostVolume::rootPath_` |
| `s_mounted` (bool) | `HostVolume::mounted_` |
| `s_typeMap[]` (array) | `assets/typemap.def` via AppleDouble |
| `mapTypeCreator()` | `appledouble::GetFinderInfo()` |
| `toMacRoman()` | `appledouble::MacNameFromHost()` |
| `toMacDate()` | `appledouble::MacDateFromFileTime()` |
| `getResourceForkSize()` | `appledouble::ResourceForkSize()` |
| `fourCC()` | `appledouble::FourCC()` |

What **stays** in `extn_extfs.cpp`:

- The switch/dispatch for each command code.
- Guest RAM helpers: `readPascalString()`, `writePascalString()`,
  `get_vm_byte()`, `put_vm_byte()`.
- Error code translation (`FMErr` → `uint16_t`).
- Debug logging (`fprintf(stderr, ...)`).
- Guest debug console integration (`kExtFSDbgLog`, `kExtFSGuestVars`,
  `kExtFSFatal`).

### 3.2 `machine.cpp` — no changes

The `regDispatch()` function at [machine.cpp](../../src/core/machine.cpp#L560)
continues to call `ExtnExtFSDispatch()` with the same signature.
HostVolume is an internal detail of the ExtFS implementation.

### 3.3 CMakeLists.txt — add new source files

Add to `MINIVMAC_SOURCES` (after the existing storage/ entries or
alongside `extn_extfs.cpp`):

```cmake
    src/storage/host_volume.cpp
```

Add to the test executable:

```cmake
    test/test_host_volume.cpp
    src/storage/host_volume.cpp
```

---

## 4. Internal State

### 4.1 CatalogEntry

```cpp
struct CatalogEntry {
    uint32_t    cnid;           // unique within this volume
    uint32_t    parentDirID;    // cnid of parent directory
    bool        isDirectory;
    std::string hostPath;       // e.g. "shared/Documents/^3Areadme.txt"
    std::string macName;        // e.g. ":readme.txt" (decoded from host)

    uint32_t    type;           // from sidecar or extension default
    uint32_t    creator;        // from sidecar or extension default
    uint16_t    finderFlags;    // from sidecar
    uint32_t    dataForkSize;   // host bytes, or Mac Roman count for TEXT
    uint32_t    rsrcForkSize;   // from sidecar
    uint32_t    crDate;         // Mac epoch (from POSIX mtime)
    uint32_t    modDate;        // Mac epoch (from POSIX mtime)
    bool        isText;         // true when type == FourCC("TEXT")
};
```

All metadata fields are populated from `appledouble::GetFileInfo()` at
scan time.  This includes the TEXT-converted data fork size, which
avoids re-reading and converting the file on every `GetCatInfo` call.

The `isText` flag is cached so that `readFork()` knows to convert
UTF-8 → Mac Roman on the fly, and `writeFork()` knows to convert
Mac Roman → UTF-8 before writing to disk.

### 4.2 OpenFork

```cpp
struct OpenFork {
    uint32_t cnid;
    ForkType fork;        // Data or Resource
    FILE    *fp;          // non-null for data forks only
};
```

Resource fork handles exist only for bookkeeping (tracking that a
fork is open, returning its size at open time).  Actual I/O goes
through the AppleDouble path-based API.

### 4.3 CNID space

| Range | Purpose |
|-------|---------|
| 1 | Virtual root parent |
| 2 | Root directory |
| 3–15 | Reserved |
| 16+ | User files and directories, sequentially allocated |

CNIDs are not persisted across sessions.  This matches current behavior.
Future work could persist a CNID ↔ path mapping file, but that is out
of scope.

---

## 5. Key Algorithms

### 5.1 Directory scan

```
function scanDirectory(hostDir, parentDirID):
    for each entry in directory_iterator(hostDir):
        name = entry.filename()
        if name starts with '.': skip         // hidden files
        if IsSidecar(name): skip              // AppleDouble sidecars

        macName = truncate(MacNameFromHost(name), 31)
        if macName is empty: skip

        ce.cnid = nextCNID_++
        ce.parentDirID = parentDirID
        ce.hostPath = entry.path()
        ce.macName = macName

        if entry is directory:
            ce.isDirectory = true
            ftime = last_write_time(entry)
            ce.crDate = MacDateFromFileTime(ftime)
            ce.modDate = ce.crDate
            catalog_.push_back(ce)
            scanDirectory(entry.path(), ce.cnid)      // recurse

        else if entry is regular file:
            info = appledouble::GetFileInfo(entry.path())
            ce.isDirectory = false
            ce.type = info.finder.type
            ce.creator = info.finder.creator
            ce.finderFlags = info.finder.flags
            ce.dataForkSize = info.dataForkSize        // already TEXT-converted
            ce.rsrcForkSize = info.rsrcForkSize
            ce.crDate = info.crDate
            ce.modDate = info.modDate
            ce.isText = info.isText
            catalog_.push_back(ce)
```

The key differences from the current `scanDirectory()`:

1. **Sidecar filtering:** `IsSidecar(name)` prevents `._*` files from
   appearing in the guest catalog.
2. **Lossless filenames:** `MacNameFromHost()` decodes `^XX` escapes
   instead of the lossy `toMacRoman()`.
3. **Persistent metadata:** `GetFileInfo()` reads type/creator from
   sidecars when they exist, falling back to extension defaults. The
   current code only uses extension defaults and never persists.
4. **Correct TEXT sizes:** `GetFileInfo()` returns the Mac Roman byte
   count for TEXT files.  The current code returns the UTF-8 byte count.

### 5.2 Fork open (resource fork)

```
function openFork(cnid, Resource, outSize, errOut):
    entry = findByCNID(cnid)
    if not entry or entry.isDirectory:
        errOut = kFnfErr; return 0

    handle = nextHandle_++
    openForks_[handle] = { cnid, Resource, nullptr }

    outSize = appledouble::ResourceForkSize(entry.hostPath)
    errOut = kNoErr
    return handle
```

No file pointer is opened for resource forks.  The AppleDouble
library opens/closes the sidecar internally on each read/write call.

### 5.3 Fork read (resource fork)

```
function readFork(handle, offset, buf, outRead):
    of = openForks_[handle]
    entry = findByCNID(of.cnid)

    data = appledouble::ReadResourceFork(entry.hostPath, offset, buf.size())
    copy data into buf
    outRead = data.size()
    return kNoErr
```

### 5.4 Fork read (data fork, TEXT file)

```
function readFork(handle, offset, buf, outRead):
    of = openForks_[handle]
    entry = findByCNID(of.cnid)

    if entry.isText:
        // Convert entire file UTF-8 → Mac Roman, then serve from offset
        converted = appledouble::MacRomanFromUTF8File(entry.hostPath)
        available = max(0, converted.size() - offset)
        toRead = min(buf.size(), available)
        copy converted[offset..offset+toRead] into buf
        outRead = toRead
    else:
        // Direct fread from the data fork FILE*
        fseek(of.fp, offset, SEEK_SET)
        outRead = fread(buf.data(), 1, buf.size(), of.fp)

    return kNoErr
```

For TEXT files, the full conversion is performed per read.  This is
consistent with the AppleDouble spec's statement that the library does
not cache.  If profiling shows this is hot, a future optimization can
cache the converted bytes in the `OpenFork` struct for the duration
the file is open.

To decide whether caching is needed, `HostVolume` maintains internal
TEXT conversion stats — a counter of how many text conversions have
been performed and how many total bytes were converted.  These are
exposed via `textConversionStats()` so that debug logging or the
debugger console can report them.  A typical concern: a guest program
reading a 32 KB TEXT file in 1 KB blocks would trigger 32 full-file
conversions.  The stats will quantify this and inform whether an
open-file-level cache is warranted.

### 5.5 Fork write (data fork, TEXT file)

```
function writeFork(handle, offset, data, outWritten):
    of = openForks_[handle]
    entry = mutableFindByCNID(of.cnid)

    if entry.isText:
        // Guest writes Mac Roman bytes → convert to UTF-8 and rewrite
        utf8 = appledouble::UTF8FromMacRoman(data)
        // Full file rewrite (TEXT writes are infrequent and typically
        // small; partial-file UTF-8 patching is not feasible because
        // character widths differ)
        writeEntireFile(entry.hostPath, utf8)
        outWritten = data.size()
        // Recache the Mac-visible size
        entry.dataForkSize = appledouble::MacRomanSizeFromUTF8File(entry.hostPath)
    else:
        fseek(of.fp, offset, SEEK_SET)
        outWritten = fwrite(data.data(), 1, data.size(), of.fp)
        fflush(of.fp)
        fseek(of.fp, 0, SEEK_END)
        entry.dataForkSize = ftell(of.fp)

    entry.modDate = currentMacDate()
    return kNoErr
```

### 5.6 Create file

```
function createFile(parentDirID, macName, errOut):
    if findByName(parentDirID, macName):
        errOut = kDupFNErr; return 0

    parentPath = resolveParentPath(parentDirID)
    if parentPath is empty:
        errOut = kDirNFErr; return 0

    hostName = appledouble::HostNameFromMac(macName)
    hostPath = parentPath / hostName

    fp = fopen(hostPath, "wb")
    if not fp:
        errOut = kIoErr; return 0
    fclose(fp)

    ce.cnid = nextCNID_++
    ce.parentDirID = parentDirID
    ce.hostPath = hostPath
    ce.macName = macName
    ce.isDirectory = false
    ce.crDate = ce.modDate = currentMacDate()
    catalog_.push_back(ce)

    errOut = kNoErr
    return ce.cnid
```

### 5.7 Delete

```
function remove(parentDirID, macName):
    entry = findByName(parentDirID, macName)
    if not entry: return kFnfErr

    if entry.isDirectory:
        if childCount(entry.cnid) > 0: return kFBsyErr
        fs::remove(entry.hostPath)
    else:
        appledouble::DeleteWithSidecar(entry.hostPath)

    eraseCatalogEntry(entry.cnid)
    return kNoErr
```

### 5.8 Move

```
function move(srcDirID, macName, dstDirID):
    entry = findByName(srcDirID, macName)
    if not entry: return kFnfErr

    dstPath = resolveParentPath(dstDirID)
    if dstPath is empty: return kFnfErr

    newHostPath = dstPath / entry.hostPath.filename()

    if entry.isDirectory:
        fs::rename(entry.hostPath, newHostPath)
    else:
        appledouble::RenameWithSidecar(entry.hostPath, newHostPath)

    // Update catalog entry and all descendant paths
    oldPrefix = entry.hostPath
    entry.parentDirID = dstDirID
    entry.hostPath = newHostPath
    updateDescendantPaths(entry.cnid, oldPrefix, newHostPath)

    return kNoErr
```

---

## 6. Reused Infrastructure

| Component | Location | Used for |
|-----------|----------|----------|
| `appledouble::GetFileInfo()` | `src/storage/appledouble.h` | Populate catalog entries at scan time |
| `appledouble::SetFinderInfo()` | `src/storage/appledouble.h` | Persist type/creator on SetFileInfo |
| `appledouble::ReadResourceFork()` | `src/storage/appledouble.h` | Resource fork reads |
| `appledouble::WriteResourceFork()` | `src/storage/appledouble.h` | Resource fork writes |
| `appledouble::ResourceForkSize()` | `src/storage/appledouble.h` | Fork size at open time |
| `appledouble::SetResourceForkSize()` | `src/storage/appledouble.h` | Truncate/extend resource fork |
| `appledouble::DeleteWithSidecar()` | `src/storage/appledouble.h` | Atomic file+sidecar deletion |
| `appledouble::RenameWithSidecar()` | `src/storage/appledouble.h` | Atomic file+sidecar rename |
| `appledouble::HostNameFromMac()` | `src/storage/appledouble.h` | Escape Mac filenames for host |
| `appledouble::MacNameFromHost()` | `src/storage/appledouble.h` | Decode host filenames to Mac |
| `appledouble::IsSidecar()` | `src/storage/appledouble.h` | Filter sidecars during scan |
| `appledouble::MacRomanFromUTF8File()` | `src/storage/appledouble.h` | TEXT file read conversion |
| `appledouble::MacRomanSizeFromUTF8File()` | `src/storage/appledouble.h` | TEXT file size for catalog |
| `appledouble::UTF8FromMacRoman()` | `src/storage/appledouble.h` | TEXT file write conversion |
| `appledouble::MacDateFromFileTime()` | `src/storage/appledouble.h` | POSIX → Mac epoch dates |
| `appledouble::LoadTypeMappings()` | `src/storage/appledouble.h` | Load typemap.def at mount |
| `std::filesystem` | Standard library | Directory iteration, path ops, stat |
| `doctest` | `libs/doctest/` | Unit test framework |

The following items in `extn_extfs.cpp` are **replaced and deleted**:

| Deleted | Replacement |
|---------|-------------|
| `s_typeMap[]` array + `mapTypeCreator()` | `appledouble::FinderInfoFromExtension()` via `GetFileInfo()` |
| `toMacRoman()` | `appledouble::MacNameFromHost()` |
| `toMacDate()` | `appledouble::MacDateFromFileTime()` |
| `fourCC()` | `appledouble::FourCC()` |
| `truncateMacName()` | inline truncation in `HostVolume::scanDirectory()` |
| `getResourceForkSize()` | `appledouble::ResourceForkSize()` |
| `kMacEpochOffset` (local copy) | `appledouble::kMacEpochOffset` |

---

## 7. Build Integration

### CMakeLists.txt changes

Add `host_volume.cpp` to `MINIVMAC_SOURCES` alongside the existing
storage sources.  Also add it (and the existing storage sources, if
not already present) to the main executable — the current build only
includes storage in the test target:

```cmake
# In MINIVMAC_SOURCES:
    src/storage/appledouble.cpp
    src/storage/filename_encoding.cpp
    src/storage/text_convert.cpp
    src/storage/host_volume.cpp
```

Add test file to the test executable:

```cmake
# In add_executable(tests ...):
    test/test_host_volume.cpp
    src/storage/host_volume.cpp
```

No new external dependencies.

---

## 8. Dependency Diagram

```
┌──────────────────────┐
│    extn_extfs.cpp    │  Register dispatch (guest ↔ host boundary)
│  (cmd switch, guest  │
│   RAM marshalling)   │
└──────────┬───────────┘
           │ owns / calls
           ▼
┌──────────────────────┐
│   HostVolume         │  Catalog, handles, WDs, CNID allocation
│   (host_volume.h)    │
└──────────┬───────────┘
           │ calls
           ▼
┌──────────────────────┐
│   appledouble.h      │  Sidecar format, metadata, filename escaping,
│                      │  text conversion, fork I/O
└──────────┬───────────┘
           │ uses
     ┌─────┴──────┐
     ▼            ▼
┌──────────┐  ┌────────────┐
│ filename │  │ text_      │
│ encoding │  │ convert    │
└──────────┘  └─────┬──────┘
                    │ uses
                    ▼
              ┌────────────┐
              │ mac_roman  │  (existing, in platform/common/)
              └────────────┘
```

Arrows point downward = dependency direction.  No cycles.
`HostVolume` depends on `appledouble` but not on `extn_extfs`.
`appledouble` depends on nothing in `src/core/`.

---

## 9. Multi-Volume Architecture

The current design naturally supports multiple volumes.  Each
`HostVolume` instance is fully self-contained: its own catalog, CNID
space, handle table, and WD table.

### Dispatch-layer changes for multi-volume

`extn_extfs.cpp` would hold a vector of `HostVolume` instances:

```cpp
static std::vector<storage::HostVolume> s_volumes;
```

Volume routing depends on how the guest identifies which volume a
request targets:

| Request type | Routing key | Lookup |
|-------------|-------------|--------|
| By CNID (GetCatInfo, GetFileInfo, Open) | CNID | Scan all volumes for matching CNID — or partition CNID space by volume |
| By WD ref (GetWDInfo) | WD ref | Each volume manages its own WD range |
| By name + parentDirID | parentDirID | Same as CNID — find which volume owns that directory |
| Mount-specific (GetVol) | vRefNum index | Direct table lookup |

**CNID partitioning** (preferred approach): Assign each volume a
non-overlapping CNID range.  Volume 0 starts at CNID 16, volume 1
at 0x01000000, volume 2 at 0x02000000, etc.  This makes volume
lookup from a CNID a single shift operation.

This is a future concern — the initial implementation can support a
single volume and refactor to multi-volume later.  The class design
does not prevent it.

---

## 10. Testing

### Test file: `test/test_host_volume.cpp`

Uses doctest.  Each test creates a temporary directory under `/tmp/`,
populates it with test files, mounts a `HostVolume`, and exercises the
API.

### Test cases

**Catalog scan:**
- Mount a directory with files and subdirectories → verify CNIDs,
  parent IDs, Mac names.
- Files with `^XX` escaped names → verify `MacNameFromHost` decoding.
- `._` sidecar files are hidden from the catalog.
- Files with AppleDouble sidecars → verify type/creator read from
  sidecar (not just extension default).
- TEXT files → verify `dataForkSize` is Mac Roman byte count.
- Empty directory → mounts successfully with zero entries.

**Catalog queries:**
- `findByCNID` — found and not-found cases.
- `findByName` — case-insensitive matching.
- `nthChild` — 1-based indexing, out-of-range returns nullptr.
- `childCount` — correct for root and subdirectories.

**File creation:**
- `createFile` in root → file exists on disk with escaped name.
- `createFile` with duplicate name → `kDupFNErr`.
- `createDir` → directory exists on host.

**Deletion:**
- `remove` file → host file and sidecar deleted.
- `remove` non-empty directory → `kFBsyErr`.
- `remove` empty directory → succeeds.

**Move:**
- `move` file between directories → host path updated, sidecar moved.
- `move` directory → all descendant paths updated.

**Metadata:**
- `setFileInfo` → sidecar created/updated via AppleDouble, catalog
  entry updated.
- `setFileInfo` back to extension default → sidecar removed.

**Fork I/O (data fork):**
- Open, write, read back, close → data matches.
- Read at offset → correct partial read.
- Write updates catalog size and modDate.

**Fork I/O (resource fork):**
- Open resource fork → size from AppleDouble.
- Write resource fork → data persisted in sidecar.
- Read resource fork at offset → correct bytes.

**Fork I/O (TEXT file):**
- Write Mac Roman bytes → host file contains UTF-8.
- Read → returns Mac Roman bytes converted from UTF-8.
- `dataForkSize` reflects Mac Roman count, not UTF-8 byte count.

**Working directories:**
- `openWD` / `wdToDirID` / `closeWD` round-trip.
