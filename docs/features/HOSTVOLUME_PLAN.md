# HostVolume — Implementation Plan

Design: [HOSTVOLUME_DESIGN.md](HOSTVOLUME_DESIGN.md)
Spec: [HOSTVOLUME.md](HOSTVOLUME.md)

This plan covers: building the `HostVolume` class with full unit tests,
then rewiring `extn_extfs.cpp` to delegate to it.  Each phase compiles
and passes all existing tests.

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Header, types, empty class skeleton | ✅ done |
| 2 | Catalog scan + query methods + tests | ✅ done |
| 3 | File/directory creation + deletion + tests | ✅ done |
| 4 | Move/rename + metadata (SetFileInfo) + tests | ✅ done |
| 5 | Data fork I/O + tests | ✅ done |
| 6 | Resource fork I/O + tests | ✅ done |
| 7 | TEXT fork I/O + conversion stats + tests | ✅ done |
| 8 | Working directories + volumeStats + tests | ✅ done |
| 9 | Build integration (CMakeLists.txt, main exe) | ✅ done |
| 10 | Rewire extn_extfs.cpp read-only commands | ✅ done |
| 11 | Rewire extn_extfs.cpp write commands | ✅ done |
| 12 | Remove dead code from extn_extfs.cpp | ✅ done |

Build gate: `cmake --build --preset macos`
Test gate:  `./bld/macos/tests`

---

## Phase 1 — Header, types, empty class skeleton

Create the public header and a .cpp with stub methods so everything
compiles.  No logic yet — just the type definitions and signatures
from Design §2.

### 1.1 — Create `src/storage/host_volume.h`

The full header as specified in Design §2, containing:

- `namespace storage`
- `enum class FMErr` with all error codes
- `struct CatalogEntry` with all fields (cnid, parentDirID, isDirectory,
  hostPath, macName, type, creator, finderFlags, dataForkSize,
  rsrcForkSize, crDate, modDate, isText)
- `enum class ForkType { Data, Resource }`
- `class HostVolume` with all public method declarations:
  - `mount()`, `isMounted()`
  - `kRootParentID = 1`, `kRootDirID = 2`
  - `findByCNID()`, `findByName()`, `nthChild()`, `childCount()`
  - `volumeStats()`
  - `createFile()`, `createDir()`
  - `remove()`
  - `move()`
  - `setFileInfo()`
  - `openFork()`, `readFork()`, `writeFork()`, `closeFork()`
  - `openWD()`, `wdToDirID()`, `closeWD()`
  - `struct TextStats`, `textConversionStats()`, `resetTextConversionStats()`
- Private members:
  - `rootPath_`, `mounted_`, `catalog_`, `nextCNID_`
  - `struct OpenFork { cnid, fork, fp }`, `openForks_`, `nextHandle_`
  - `wdTable_`, `nextWD_`
  - `textStats_`
  - Private helpers: `scanDirectory()`, `mutableFindByCNID()`,
    `resolveParentPath()`, `invalidateTextSize()`

Include guards: `#pragma once`.  Includes: `storage/appledouble.h`,
`<cstdint>`, `<filesystem>`, `<span>`, `<string>`, `<string_view>`,
`<vector>`, `<unordered_map>`.

### 1.2 — Create `src/storage/host_volume.cpp`

Include `storage/host_volume.h`.  Implement every method as a stub:

- `mount()` → `return false;`
- `isMounted()` → `return mounted_;`
- `findByCNID()` → `return nullptr;`
- `findByName()` → `return nullptr;`
- `nthChild()` → `return nullptr;`
- `childCount()` → `return 0;`
- `volumeStats()` → set both out params to 0
- `createFile()` → `errOut = FMErr::kFnfErr; return 0;`
- `createDir()` → `errOut = FMErr::kFnfErr; return 0;`
- `remove()` → `return FMErr::kFnfErr;`
- `move()` → `return FMErr::kFnfErr;`
- `setFileInfo()` → `return FMErr::kFnfErr;`
- `openFork()` → `errOut = FMErr::kFnfErr; return 0;`
- `readFork()` → `outRead = 0; return FMErr::kRfNumErr;`
- `writeFork()` → `outWritten = 0; return FMErr::kRfNumErr;`
- `closeFork()` → empty
- `openWD()` → `return 0;`
- `wdToDirID()` → `return 0;`
- `closeWD()` → empty
- `textConversionStats()` → `return textStats_;`
- `resetTextConversionStats()` → `textStats_ = {};`
- `scanDirectory()` → empty
- `mutableFindByCNID()` → `return nullptr;`
- `resolveParentPath()` → `return {};`
- `invalidateTextSize()` → empty

### 1.3 — Create `test/test_host_volume.cpp`

Include `<doctest/doctest.h>` and `storage/host_volume.h`.  Add a
single smoke test:

```cpp
TEST_CASE("HostVolume: default state") {
    storage::HostVolume vol;
    CHECK_FALSE(vol.isMounted());
    CHECK(vol.findByCNID(2) == nullptr);
    CHECK(vol.childCount(2) == 0);
}
```

### Fence

- [ ] `src/storage/host_volume.h` exists with full class declaration
- [ ] `src/storage/host_volume.cpp` exists with all stubs
- [ ] `test/test_host_volume.cpp` exists with smoke test
- [ ] Build clean (manually add files to CMake test target temporarily,
      or verify compilation with a direct clang++ invocation)
- [ ] Commit: `"hostvolume: phase 1 — header, types, skeleton"`

---

## Phase 2 — Catalog scan + query methods + tests

Implement `mount()`, `scanDirectory()`, and all four catalog query
methods.  This is the core of the class — everything else builds on
the catalog.

### 2.1 — Implement `mount()`

```
function mount(hostDir):
    if not fs::is_directory(hostDir): return false
    rootPath_ = hostDir
    catalog_.clear()
    nextCNID_ = 16      // kFirstCNID
    openForks_.clear()
    nextHandle_ = 1
    wdTable_.clear()
    nextWD_ = 0x8000
    textStats_ = {}

    // Load type mappings if first mount
    static bool s_typesLoaded = false
    if not s_typesLoaded:
        appledouble::LoadTypeMappings("assets/typemap.def")
        s_typesLoaded = true

    scanDirectory(hostDir, kRootDirID)
    mounted_ = true
    return true
```

### 2.2 — Implement `scanDirectory()`

As specified in Design §5.1:

```
for each entry in fs::directory_iterator(hostDir):
    name = entry.path().filename().string()
    if name is empty or starts with '.': skip
    if appledouble::IsSidecar(name): skip

    macName = appledouble::MacNameFromHost(name)
    if macName.size() > 31: macName = macName.substr(0, 31)
    if macName is empty: skip

    CatalogEntry ce{}
    ce.cnid = nextCNID_++
    ce.parentDirID = parentDirID
    ce.hostPath = entry.path().string()
    ce.macName = macName

    if entry.is_directory():
        ce.isDirectory = true
        auto ftime = fs::last_write_time(entry.path())
        ce.crDate = appledouble::MacDateFromFileTime(ftime)
        ce.modDate = ce.crDate
        uint32_t thisDirID = ce.cnid
        catalog_.push_back(std::move(ce))
        scanDirectory(entry.path(), thisDirID)

    else if entry.is_regular_file():
        auto info = appledouble::GetFileInfo(entry.path())
        ce.isDirectory = false
        ce.type = info.finder.type
        ce.creator = info.finder.creator
        ce.finderFlags = info.finder.flags
        ce.dataForkSize = info.dataForkSize
        ce.rsrcForkSize = info.rsrcForkSize
        ce.crDate = info.crDate
        ce.modDate = info.modDate
        ce.isText = info.isText
        catalog_.push_back(std::move(ce))
```

### 2.3 — Implement query methods

**`findByCNID(cnid)`** — linear scan of `catalog_`, return pointer to
matching entry or nullptr.

**`findByName(parentDirID, macName)`** — linear scan, compare
`parentDirID` and case-insensitive compare of `macName` (using
`tolower()` on each byte).

**`nthChild(parentDirID, index)`** — linear scan, count children of
`parentDirID`, return the `index`-th one (1-based).  Return nullptr if
`index` exceeds child count.

**`childCount(parentDirID)`** — linear scan, count entries with
matching `parentDirID`.

### 2.4 — Implement helpers

**`mutableFindByCNID(cnid)`** — same as `findByCNID` but returns
non-const `CatalogEntry*`.

**`resolveParentPath(parentDirID)`** — if `parentDirID == kRootDirID`,
return `rootPath_.string()`.  Otherwise, find the catalog entry with
that cnid and return its `hostPath`.  Return empty string if not found.

### 2.5 — Tests

Add to `test/test_host_volume.cpp`.  Each test creates a temporary
directory tree under `/tmp/test_hv_XXXX/` with test files, mounts a
`HostVolume`, and tears down afterward.

Helper function at the top of the test file:

```cpp
#include <filesystem>
namespace fs = std::filesystem;

static fs::path makeTempDir() {
    auto p = fs::temp_directory_path() / "test_hv";
    fs::create_directories(p);
    return p;
}

static void writeFile(const fs::path &p, std::string_view content) {
    std::ofstream f(p, std::ios::binary);
    f.write(content.data(), content.size());
}
```

**Test cases:**

1. **"mount empty directory"** — create empty dir, mount → `isMounted()`,
   `childCount(kRootDirID) == 0`.

2. **"mount with files"** — create dir with `hello.txt`, `image.png`,
   mount → `childCount(kRootDirID) == 2`, `findByName` finds both,
   both have correct `macName`.

3. **"mount with subdirectory"** — create dir with a subdirectory
   containing one file → verify the subdirectory has
   `isDirectory == true`, its child count is 1, the file inside is
   found by `nthChild`.

4. **"sidecar files hidden"** — create `test.txt` and `._test.txt` →
   only `test.txt` appears in catalog.

5. **"hidden files skipped"** — create `.hidden` → not in catalog.

6. **"filename decoding"** — create file `^3Acolon` on host → macName
   should be `:colon`.

7. **"findByName case-insensitive"** — create `Hello.txt`, find by
   `"hello.txt"` → found.

8. **"findByCNID not found"** — returns nullptr for non-existent CNID.

9. **"nthChild out of range"** — returns nullptr for index beyond
   child count.

10. **"metadata from extension"** — create `readme.txt`, mount →
    type == `TEXT`, creator == `ttxt`.

11. **"metadata from sidecar"** — create `app` + `._app` sidecar with
    custom type/creator → catalog entry reflects sidecar metadata, not
    extension default.

12. **"mount nonexistent directory"** — returns false, `isMounted()`
    is false.

Cleanup: each test calls `fs::remove_all(tempDir)` at the end.

### Fence

- [ ] `mount()` scans host directory tree via AppleDouble
- [ ] All four query methods work (findByCNID, findByName, nthChild,
      childCount)
- [ ] `resolveParentPath()` works for root and subdirectory
- [ ] 12 test cases pass
- [ ] Full build clean, all existing tests still pass
- [ ] Commit: `"hostvolume: phase 2 — catalog scan and queries"`

---

## Phase 3 — File/directory creation + deletion + tests

Implement `createFile()`, `createDir()`, and `remove()`.

### 3.1 — Implement `createFile()`

As specified in Design §5.6:

1. Check for duplicate via `findByName()` → `kDupFNErr`.
2. `resolveParentPath()` → `kDirNFErr` if empty.
3. `appledouble::HostNameFromMac(macName)` → host filename.
4. `fopen(hostPath, "wb")` → create empty file, `fclose()` immediately.
5. Build `CatalogEntry`, push to `catalog_`.
6. Return new CNID.

Use a `currentMacDate()` private helper:

```cpp
static uint32_t currentMacDate() {
    auto now = std::chrono::system_clock::now();
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(
                    now.time_since_epoch()).count();
    return static_cast<uint32_t>(secs + appledouble::kMacEpochOffset);
}
```

### 3.2 — Implement `createDir()`

Same pattern as `createFile()` but uses `fs::create_directory()` and
sets `isDirectory = true`.

### 3.3 — Implement `remove()`

As specified in Design §5.7:

1. `findByName(parentDirID, macName)` → `kFnfErr` if not found.
2. If directory: check `childCount() > 0` → `kFBsyErr`.  Then
   `fs::remove()`.
3. If file: `appledouble::DeleteWithSidecar()`.
4. Erase from `catalog_` by CNID.

Implement `eraseCatalogEntry(cnid)` as a private helper — simple erase
from vector.

### 3.4 — Tests

Add to `test/test_host_volume.cpp`:

1. **"createFile basic"** — mount, createFile("newfile.txt") → CNID > 0,
   file exists on host, `findByName` finds it.

2. **"createFile duplicate"** — create file, create again with same
   name → `kDupFNErr`, CNID == 0.

3. **"createFile escaped name"** — create file with Mac name
   `:special` → host file named `^3Aspecial` exists.

4. **"createFile bad parent"** — create in non-existent directory →
   `kDirNFErr`.

5. **"createDir basic"** — mount, createDir → dir exists on host,
   `isDirectory == true` in catalog.

6. **"remove file"** — create file, remove → host file gone, not in
   catalog.

7. **"remove file with sidecar"** — create file, set type/creator
   (via `appledouble::SetFinderInfo` to create a sidecar), remove →
   both file and sidecar gone.

8. **"remove non-empty directory"** — create dir, create file inside →
   remove dir → `kFBsyErr`.

9. **"remove empty directory"** — create dir, remove → gone.

10. **"remove non-existent"** — remove unknown name → `kFnfErr`.

### Fence

- [ ] `createFile()` creates host file + catalog entry
- [ ] `createDir()` creates host directory + catalog entry
- [ ] `remove()` deletes file+sidecar or empty directory, updates catalog
- [ ] 10 test cases pass
- [ ] Full build clean, all existing tests still pass
- [ ] Commit: `"hostvolume: phase 3 — create and delete"`

---

## Phase 4 — Move/rename + metadata (SetFileInfo) + tests

### 4.1 — Implement `move()`

As specified in Design §5.8:

1. `findByName(srcDirID, macName)` → `kFnfErr` if not found.
2. `resolveParentPath(dstDirID)` → `kFnfErr` if empty.
3. Compute `newHostPath`.
4. If directory: `fs::rename()`.
   If file: `appledouble::RenameWithSidecar()`.
5. Update the moved entry's `parentDirID` and `hostPath`.
6. If directory: update all descendant `hostPath` values by rewriting
   the prefix.

Implement `updateDescendantPaths(cnid, oldPrefix, newPrefix)` as a
private helper.  Linear scan of `catalog_`, for each entry whose
`hostPath` starts with `oldPrefix + "/"`, replace the prefix with
`newPrefix`.

### 4.2 — Implement `setFileInfo()`

1. `mutableFindByCNID(cnid)` → `kFnfErr` if not found.
2. `appledouble::SetFinderInfo(entry.hostPath, {type, creator, entry.finderFlags})`.
3. Update `entry.type` and `entry.creator` in the catalog.
4. If the type changed to/from `TEXT`, update `entry.isText` and
   call `invalidateTextSize(entry)`.

### 4.3 — Implement `invalidateTextSize()`

If `entry.isText` is true, recompute `entry.dataForkSize` via
`appledouble::MacRomanSizeFromUTF8File(entry.hostPath)`.
If false, recompute from `fs::file_size(entry.hostPath)`.

### 4.4 — Tests

1. **"move file between dirs"** — create two dirs, create file in dir A,
   move to dir B → file's parentDirID == dir B, host file in new
   location, old location gone.

2. **"move file with sidecar"** — create file + sidecar, move →
   sidecar also moved.

3. **"move directory with children"** — create dir with file inside,
   move dir → child's hostPath prefix updated.

4. **"move non-existent"** — `kFnfErr`.

5. **"setFileInfo basic"** — create file, setFileInfo(type=APPL,
   creator=test) → catalog entry updated, sidecar exists on disk with
   correct metadata.

6. **"setFileInfo to default removes sidecar"** — create `.txt` file,
   set to custom type, set back to TEXT/ttxt → sidecar removed.

7. **"setFileInfo updates isText"** — create generic file, setFileInfo
   type=TEXT → `isText` becomes true.

### Fence

- [ ] `move()` relocates file/dir + sidecar, updates descendants
- [ ] `setFileInfo()` persists metadata via AppleDouble
- [ ] `invalidateTextSize()` recaches TEXT file size
- [ ] 7 test cases pass
- [ ] Full build clean, all existing tests still pass
- [ ] Commit: `"hostvolume: phase 4 — move and metadata"`

---

## Phase 5 — Data fork I/O + tests

Implement `openFork()`, `readFork()`, `writeFork()`, `closeFork()` for
data forks (non-TEXT files).  TEXT fork I/O is deferred to Phase 7.

### 5.1 — Implement `openFork()` (data fork path)

1. `findByCNID(cnid)` → `kFnfErr` if not found or is directory.
2. If `fork == ForkType::Data`:
   - `fopen(entry.hostPath, "r+b")`, fall back to `fopen("rb")`.
   - If `fp == nullptr` and file was just created (size 0), try
     `fopen("w+b")`.
   - Seek to end → get file size → seek to start.
   - Store `OpenFork{cnid, Data, fp}` in `openForks_`.
3. `outSize` = file size.
4. Return handle.

### 5.2 — Implement `readFork()` (data fork, non-TEXT path)

1. Look up handle in `openForks_` → `kRfNumErr` if not found.
2. If `fork == Data` and entry is not TEXT:
   - `fseek(fp, offset, SEEK_SET)`.
   - `fread` into `buf`.
   - `outRead` = bytes actually read.
3. Return `kNoErr`.

### 5.3 — Implement `writeFork()` (data fork, non-TEXT path)

1. Look up handle → `kRfNumErr`.
2. `fseek(fp, offset, SEEK_SET)`.
3. `fwrite` from `data`.
4. `fflush(fp)`.
5. Update `entry.dataForkSize` (seek to end, ftell).
6. Update `entry.modDate` to `currentMacDate()`.
7. `outWritten` = bytes written.

### 5.4 — Implement `closeFork()`

1. Look up handle.
2. If `fp` is non-null, `fclose(fp)`.
3. Erase from `openForks_`.

### 5.5 — Tests

1. **"open/close data fork"** — create file, open data fork → handle > 0,
   size == 0.  Close → no crash.

2. **"write then read data fork"** — open, write "hello" at offset 0,
   close, reopen, read 5 bytes at offset 0 → "hello".

3. **"read at offset"** — write "hello world", read 5 bytes at offset 6
   → "world".

4. **"write updates catalog size"** — write 100 bytes → entry.dataForkSize
   == 100.

5. **"open non-existent CNID"** — `kFnfErr`.

6. **"read with bad handle"** — `kRfNumErr`.

7. **"write updates modDate"** — write data, check entry.modDate is
   recent (within 5 seconds of current Mac date).

### Fence

- [ ] Data fork open/read/write/close works for non-TEXT files
- [ ] Catalog is updated after writes (size, modDate)
- [ ] 7 test cases pass
- [ ] Full build clean, all existing tests still pass
- [ ] Commit: `"hostvolume: phase 5 — data fork I/O"`

---

## Phase 6 — Resource fork I/O + tests

Implement the resource fork path in `openFork()`, `readFork()`,
`writeFork()`, and `closeFork()`.

### 6.1 — Implement `openFork()` (resource fork path)

As specified in Design §5.2:

1. `findByCNID(cnid)` → `kFnfErr`.
2. `handle = nextHandle_++`.
3. `openForks_[handle] = { cnid, Resource, nullptr }` — no FILE*.
4. `outSize = appledouble::ResourceForkSize(entry.hostPath)`.
5. Return handle.

### 6.2 — Implement `readFork()` (resource fork path)

As specified in Design §5.3:

1. `of = openForks_[handle]`.
2. Entry lookup via `findByCNID(of.cnid)`.
3. `auto data = appledouble::ReadResourceFork(entry.hostPath, offset, buf.size())`.
4. Copy to `buf`, set `outRead`.

### 6.3 — Implement `writeFork()` (resource fork path)

1. Entry lookup.
2. `appledouble::WriteResourceFork(entry.hostPath, offset, data)`.
3. Update `entry.rsrcForkSize = appledouble::ResourceForkSize(entry.hostPath)`.
4. Update `entry.modDate`.
5. `outWritten = data.size()`.

### 6.4 — Implement `closeFork()` (resource fork update)

Already handled in Phase 5: resource fork handles just get erased
from the map (no FILE* to close).

### 6.5 — Tests

1. **"open resource fork (no sidecar)"** — create file with no sidecar,
   open resource fork → size == 0.

2. **"write then read resource fork"** — open resource fork, write
   bytes, close, reopen, read back → matches.

3. **"resource fork write creates sidecar"** — create plain file, write
   resource fork → sidecar file appears on disk.

4. **"resource fork at offset"** — write 10 bytes at offset 0, write
   5 bytes at offset 5 → read 10 bytes from offset 0, verify overlap
   region has the second write's data.

5. **"resource fork updates rsrcForkSize"** — write → catalog entry's
   `rsrcForkSize` matches.

### Fence

- [ ] Resource fork open/read/write via AppleDouble library
- [ ] Sidecar creation on first resource fork write
- [ ] Catalog entry updated after resource fork writes
- [ ] 5 test cases pass
- [ ] Full build clean, all existing tests still pass
- [ ] Commit: `"hostvolume: phase 6 — resource fork I/O"`

---

## Phase 7 — TEXT fork I/O + conversion stats + tests

Implement the TEXT-specific paths in `readFork()` and `writeFork()`,
plus the `TextStats` tracking.

### 7.1 — TEXT read path

In `readFork()`, when `entry.isText` and `of.fork == Data`:

```cpp
auto converted = appledouble::MacRomanFromUTF8File(entry.hostPath);

// Update stats
textStats_.conversions++;
// bytesIn = file size on disk (UTF-8)
std::error_code ec;
textStats_.bytesIn += fs::file_size(entry.hostPath, ec);
textStats_.bytesOut += converted.size();

uint32_t available = (offset < converted.size())
    ? static_cast<uint32_t>(converted.size() - offset) : 0;
uint32_t toRead = std::min(static_cast<uint32_t>(buf.size()), available);
std::memcpy(buf.data(), converted.data() + offset, toRead);
outRead = toRead;
```

### 7.2 — TEXT write path

In `writeFork()`, when `entry.isText` and `of.fork == Data`:

```cpp
auto utf8 = appledouble::UTF8FromMacRoman(data);

// Full file rewrite
std::ofstream out(entry.hostPath, std::ios::binary | std::ios::trunc);
out.write(utf8.data(), utf8.size());
out.close();

outWritten = static_cast<uint32_t>(data.size());

// Recache the Mac-visible size
entry.dataForkSize = appledouble::MacRomanSizeFromUTF8File(entry.hostPath);
entry.modDate = currentMacDate();
```

### 7.3 — TEXT open fork size

In `openFork()` for data forks, when the entry is TEXT:

For TEXT files, `outSize` must be the Mac Roman byte count (already
cached in `entry.dataForkSize` from scan time), not the host file
size.  Use `entry.dataForkSize` directly.  Still open the `FILE*`
(it may be needed for fallback, and `closeFork` expects it).

### 7.4 — Stats accessors

Already stubbed.  Now make them real — they just return/reset
`textStats_`.

### 7.5 — Tests

1. **"TEXT file read converts UTF-8"** — create a file containing
   UTF-8 `"café"` (5 bytes: `63 61 66 C3 A9`), set type to TEXT
   (via sidecar), mount → `dataForkSize == 4` (Mac Roman).  Open,
   read 4 bytes → should contain Mac Roman bytes for "café".

2. **"TEXT file read at offset"** — same file, read from offset 2,
   count 2 → the "fé" portion in Mac Roman.

3. **"TEXT file write converts to UTF-8"** — open, write Mac Roman
   bytes for "résumé", close.  Read host file as UTF-8 → should be
   valid UTF-8.

4. **"TEXT conversion stats"** — mount with TEXT file, read 3 times →
   `textConversionStats().conversions == 3`.  `bytesOut` should be
   3 × Mac Roman size.

5. **"reset text stats"** — accumulate stats, reset → all zero.

6. **"TEXT file dataForkSize in catalog"** — create UTF-8 file with
   known Mac Roman size, mount → `dataForkSize` matches Mac Roman
   count (not UTF-8 byte count).

### Fence

- [ ] TEXT reads convert UTF-8 → Mac Roman on the fly
- [ ] TEXT writes convert Mac Roman → UTF-8 and rewrite host file
- [ ] `TextStats` tracks conversion count, bytes in, bytes out
- [ ] 6 test cases pass
- [ ] Full build clean, all existing tests still pass
- [ ] Commit: `"hostvolume: phase 7 — TEXT conversion + stats"`

---

## Phase 8 — Working directories + volumeStats + tests

### 8.1 — Implement `openWD()`

```cpp
uint32_t HostVolume::openWD(uint32_t dirID) {
    uint32_t wdRef = nextWD_++;
    wdTable_[wdRef] = dirID;
    return wdRef;
}
```

### 8.2 — Implement `wdToDirID()`

```cpp
uint32_t HostVolume::wdToDirID(uint32_t wdRef) const {
    auto it = wdTable_.find(wdRef);
    return (it != wdTable_.end()) ? it->second : 0;
}
```

### 8.3 — Implement `closeWD()`

```cpp
void HostVolume::closeWD(uint32_t wdRef) {
    wdTable_.erase(wdRef);
}
```

### 8.4 — Implement `volumeStats()`

```cpp
void HostVolume::volumeStats(uint32_t &outFiles, uint32_t &outBytes) const {
    outFiles = 0;
    outBytes = 0;
    for (const auto &e : catalog_) {
        if (!e.isDirectory) {
            outFiles++;
            outBytes += e.dataForkSize;
        }
    }
}
```

### 8.5 — Tests

1. **"openWD / wdToDirID round-trip"** — openWD(42) → wdRef,
   wdToDirID(wdRef) == 42.

2. **"closeWD"** — open, close, wdToDirID → 0.

3. **"multiple WDs"** — open two → distinct refs, both resolve.

4. **"volumeStats"** — mount dir with 3 files totaling 100 bytes →
   `outFiles == 3`, `outBytes == 100`.

5. **"volumeStats empty"** — mount empty dir → both 0.

### Fence

- [ ] Working directory open/resolve/close functional
- [ ] `volumeStats()` sums file count and byte sizes
- [ ] 5 test cases pass
- [ ] Full build clean, all existing tests still pass
- [ ] Commit: `"hostvolume: phase 8 — working dirs and volume stats"`

---

## Phase 9 — Build integration (CMakeLists.txt, main exe)

Wire `host_volume.cpp` into both the main executable and the test
target.  Until now, tests may have been compiled with a manual command
or a temporary CMake addition.  This phase makes it official.

### 9.1 — Add to `MINIVMAC_SOURCES`

In `CMakeLists.txt`, add after the existing `src/storage/` entries:

```cmake
    src/storage/host_volume.cpp
```

Also add the three existing storage sources to `MINIVMAC_SOURCES` if
they are not already present (currently they are only in the test
target):

```cmake
    src/storage/appledouble.cpp
    src/storage/filename_encoding.cpp
    src/storage/text_convert.cpp
```

### 9.2 — Add to test target

In the `add_executable(tests ...)` block, add:

```cmake
    test/test_host_volume.cpp
    src/storage/host_volume.cpp
```

### 9.3 — Verify

- Build main executable: `cmake --build --preset macos`
- Run tests: `./bld/macos/tests`
- Both succeed with no warnings.

### Fence

- [ ] `host_volume.cpp` compiles as part of both main exe and tests
- [ ] Storage sources (`appledouble.cpp`, etc.) in main exe
- [ ] Full build clean, all tests pass
- [ ] Commit: `"hostvolume: phase 9 — build integration"`

---

## Phase 10 — Rewire extn_extfs.cpp read-only commands

Replace the read-only command handlers in `extn_extfs.cpp` with
`HostVolume` calls.  The volume is stored as a file-scope static:

```cpp
#include "storage/host_volume.h"
static storage::HostVolume s_volume;
```

This phase converts the read path only.  Write commands are Phase 11.
This split keeps each commit small and testable.

### 10.1 — Add `s_volume` and error translation helper

At file scope in `extn_extfs.cpp`:

```cpp
static storage::HostVolume s_volume;

static uint16_t fmErrToReg(storage::FMErr err) {
    // Register protocol uses positive error codes
    switch (err) {
        case storage::FMErr::kNoErr:    return 0;
        case storage::FMErr::kFnfErr:   return 43;
        case storage::FMErr::kDupFNErr: return 48;
        case storage::FMErr::kParamErr: return 50;
        case storage::FMErr::kRfNumErr: return 51;
        case storage::FMErr::kIoErr:    return 36;
        case storage::FMErr::kDirNFErr: return 120;
        case storage::FMErr::kFBsyErr:  return 47;
        case storage::FMErr::kWPrErr:   return 44;
    }
    return 43; // fallback
}
```

### 10.2 — Rewrite `kExtFSVersion`

Replace `buildCatalog()` call with `s_volume.mount("shared")`.
Remove `buildCatalog()` function entirely.

```cpp
case kExtFSVersion: {
    if (!s_volume.isMounted()) s_volume.mount("shared");
    regParam[0] = s_volume.isMounted() ? 1 : 0;
    regResult = 0;
    break;
}
```

### 10.3 — Rewrite `kExtFSGetVol`

```cpp
case kExtFSGetVol: {
    uint32_t files, bytes;
    s_volume.volumeStats(files, bytes);
    regParam[0] = files;
    regParam[1] = bytes;
    regResult = 0;
    break;
}
```

### 10.4 — Rewrite `kExtFSGetCatInfo`

Replace the entire case body.  Delegate `index > 0` to
`s_volume.nthChild()` and `index == 0` to `s_volume.findByCNID()`.

Virtual root parent (dirID == 1) handling: return the root directory
(CNID 2, name "Shared", childCount of root).  This is special-cased
as before — HostVolume does not model the virtual parent.

### 10.5 — Rewrite `kExtFSGetCatInfoName`

Delegate to `s_volume.findByName()`.  Virtual root parent handling:
compare name to "Shared" and return root directory info.

### 10.6 — Rewrite `kExtFSGetFileInfo`

Delegate to `s_volume.findByCNID()`:

```cpp
case kExtFSGetFileInfo: {
    uint32_t cnid = regParam[0];
    auto *e = s_volume.findByCNID(cnid);
    if (!e) { regResult = 43; break; }
    regParam[0] = e->type;
    regParam[1] = e->creator;
    regParam[2] = e->crDate;
    regParam[3] = e->modDate;
    regResult = 0;
    break;
}
```

Keep the virtual root directory special case (cnid 1 or 2 → return
zeros and current time).

### 10.7 — Rewrite `kExtFSReadDir`

```cpp
case kExtFSReadDir: {
    regParam[0] = static_cast<uint32_t>(s_volume.childCount(regParam[0]));
    regResult = 0;
    break;
}
```

### 10.8 — Rewrite `kExtFSObjByName`

```cpp
case kExtFSObjByName: {
    uint32_t parentDir = regParam[0];
    std::string name = readPascalString(regParam[1]);
    auto *e = s_volume.findByName(parentDir, name);
    regParam[0] = e ? e->cnid : 0;
    regResult = 0;
    break;
}
```

### 10.9 — Rewrite WD commands

`kExtFSGetWDInfo` → `s_volume.wdToDirID()`.
`kExtFSOpenWD` → `s_volume.openWD()`.
`kExtFSCloseWD` → `s_volume.closeWD()`.

### 10.10 — Rewrite `kExtFSOpen`, `kExtFSRead`, `kExtFSClose`

`kExtFSOpen`:

```cpp
case kExtFSOpen: {
    uint32_t cnid = regParam[0];
    auto forkType = (regParam[1] == 1)
        ? storage::ForkType::Resource : storage::ForkType::Data;
    uint32_t size = 0;
    storage::FMErr err;
    uint32_t handle = s_volume.openFork(cnid, forkType, size, err);
    if (handle == 0) { regResult = fmErrToReg(err); break; }
    regParam[0] = handle;
    regParam[1] = size;
    regResult = 0;
    break;
}
```

`kExtFSRead`:

```cpp
case kExtFSRead: {
    uint32_t handle = regParam[0];
    uint32_t offset = regParam[1];
    uint32_t count  = regParam[2];
    uint32_t guestBuf = regParam[3];

    std::vector<uint8_t> buf(count);
    uint32_t got = 0;
    auto err = s_volume.readFork(handle, offset, buf, got);
    if (err != storage::FMErr::kNoErr) { regResult = fmErrToReg(err); break; }

    for (uint32_t i = 0; i < got; i++)
        put_vm_byte(guestBuf + i, buf[i]);
    regParam[0] = got;
    regResult = 0;
    break;
}
```

`kExtFSClose`:

```cpp
case kExtFSClose: {
    s_volume.closeFork(regParam[0]);
    regResult = 0;
    break;
}
```

### Fence

- [ ] All read-only commands delegate to `s_volume`
- [ ] Open/Read/Close delegate to `s_volume` fork I/O
- [ ] Debug logging preserved (fprintf statements kept)
- [ ] `buildCatalog()`, `scanDirectory()`, `findByCNID()`,
      `findByNameInDir()`, `getNthChild()`, `countChildren()` removed
- [ ] Manual test: emulator boots, Shared volume appears, files listed,
      files open successfully
- [ ] Full build clean, all automated tests still pass
- [ ] Commit: `"hostvolume: phase 10 — rewire read-only commands"`

---

## Phase 11 — Rewire extn_extfs.cpp write commands

Replace the write command handlers with `HostVolume` calls.

### 11.1 — Rewrite `kExtFSCreateFile`

```cpp
case kExtFSCreateFile: {
    uint32_t parentDir = regParam[0];
    std::string macName = readPascalString(regParam[1]);
    storage::FMErr err;
    uint32_t cnid = s_volume.createFile(parentDir, macName, err);
    if (cnid == 0) { regResult = fmErrToReg(err); break; }
    regParam[0] = cnid;
    regResult = 0;
    break;
}
```

### 11.2 — Rewrite `kExtFSWrite`

```cpp
case kExtFSWrite: {
    uint32_t handle   = regParam[0];
    uint32_t offset   = regParam[1];
    uint32_t count    = regParam[2];
    uint32_t guestBuf = regParam[3];

    std::vector<uint8_t> data(count);
    for (uint32_t i = 0; i < count; i++)
        data[i] = get_vm_byte(guestBuf + i);

    uint32_t written = 0;
    auto err = s_volume.writeFork(handle, offset, data, written);
    if (err != storage::FMErr::kNoErr) { regResult = fmErrToReg(err); break; }
    regParam[0] = written;
    regResult = 0;
    break;
}
```

### 11.3 — Rewrite `kExtFSDeleteFile`

```cpp
case kExtFSDeleteFile: {
    uint32_t parentDir = regParam[0];
    std::string macName = readPascalString(regParam[1]);
    auto err = s_volume.remove(parentDir, macName);
    regResult = fmErrToReg(err);
    break;
}
```

### 11.4 — Rewrite `kExtFSSetFileInfo`

```cpp
case kExtFSSetFileInfo: {
    uint32_t cnid   = regParam[0];
    uint32_t type   = regParam[1];
    uint32_t creator = regParam[2];
    auto err = s_volume.setFileInfo(cnid, type, creator);
    regResult = fmErrToReg(err);
    break;
}
```

### 11.5 — Rewrite `kExtFSCreateDir`

```cpp
case kExtFSCreateDir: {
    uint32_t parentDir = regParam[0];
    std::string macName = readPascalString(regParam[1]);
    storage::FMErr err;
    uint32_t cnid = s_volume.createDir(parentDir, macName, err);
    if (cnid == 0) { regResult = fmErrToReg(err); break; }
    regParam[0] = cnid;
    regResult = 0;
    break;
}
```

### 11.6 — Rewrite `kExtFSCatMove`

```cpp
case kExtFSCatMove: {
    uint32_t srcDir = regParam[0];
    std::string macName = readPascalString(regParam[1]);
    uint32_t dstDir = regParam[2];
    auto err = s_volume.move(srcDir, macName, dstDir);
    regResult = fmErrToReg(err);
    break;
}
```

### Fence

- [ ] All write commands delegate to `s_volume`
- [ ] Manual test: emulator boots, create file from guest, copy file,
      delete file — all work
- [ ] File metadata persists across emulator restart (type/creator
      stored in sidecar, not just in memory)
- [ ] Full build clean, all automated tests still pass
- [ ] Commit: `"hostvolume: phase 11 — rewire write commands"`

---

## Phase 12 — Remove dead code from extn_extfs.cpp

Clean up all the old code that is no longer needed.

### 12.1 — Remove old types and statics

Delete from `extn_extfs.cpp`:

- `struct CatalogEntry` (old version)
- `static std::vector<CatalogEntry> s_catalog`
- `static bool s_mounted`
- `static std::string s_sharedDir`
- `struct OpenFile`, `static std::unordered_map<uint32_t, OpenFile> s_openFiles`
- `static uint32_t s_nextHandle`
- `struct WDEntry`, `static std::unordered_map<uint32_t, WDEntry> s_wdTable`
- `static uint32_t s_nextWD`

### 12.2 — Remove old helper functions

Delete:

- `fourCC()`
- `s_typeMap[]` array and `struct TypeMap`
- `mapTypeCreator()`
- `toMacRoman()`
- `truncateMacName()`
- `toMacDate()`
- `getResourceForkSize()`
- `static constexpr uint32_t kMacEpochOffset` (use `appledouble::kMacEpochOffset` if still needed)
- `static uint32_t s_nextCNID`
- `scanDirectory()` (old version)
- `buildCatalog()`
- `findByCNID()` (old version)
- `findByNameInDir()`
- `getNthChild()`
- `countChildren()`

### 12.3 — Verify

What remains in `extn_extfs.cpp`:

- `#include "storage/host_volume.h"`
- `static storage::HostVolume s_volume`
- `fmErrToReg()` helper
- Guest RAM helpers: `readPascalString()`, `writePascalString()`
- `ExtnExtFSDispatch()` — the switch/case dispatch (now ~200 lines)
- `kExtFSDbgLog`, `kExtFSGuestVars`, `kExtFSFatal` handlers
  (unchanged — these don't touch the filesystem)
- Command code constants (`kExtFS*`)

The file should be approximately 250–300 lines, down from ~1,166.

### Fence

- [ ] No old catalog/file/helper code remains
- [ ] `extn_extfs.cpp` is ≤300 lines
- [ ] No compiler warnings
- [ ] Manual test: full emulator run, Shared Drive works end-to-end
- [ ] All automated tests pass
- [ ] Commit: `"hostvolume: phase 12 — remove dead code"`
