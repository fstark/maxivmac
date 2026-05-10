# Catalog Cache — Detailed Design

The SharedDrive catalog is currently a static snapshot built by a
recursive scan at mount time.  Files added, removed, or modified on
the host after mount are invisible to the guest until a full
remount.  This design replaces the snapshot with a **lazy,
demand-populated catalog cache** backed by a persistent CNID
identity table.

All code must follow [STYLE.md](STYLE.md) and [NAMING.md](NAMING.md).

---

## 1. Problem Statement

The current `HostVolume::mount()` recursively walks the entire host
directory tree and builds a flat `std::vector<CatalogEntry>` in
memory.  This has three problems:

1. **No host sync.**  Files added or removed by the user after mount
   are invisible to the guest.  Modified file sizes are stale in
   `GetFileInfo`/`GetCatInfo` results.

2. **Full tree scan at mount.**  Large archive volumes with thousands
   of directories pay the full scan cost up front, even though the
   guest may only open a few folders.

3. **Unbounded memory.**  Every file and directory in the tree gets a
   `CatalogEntry` allocation, regardless of whether the guest ever
   looks at it.

---

## 2. Design Principle

**The catalog is an ephemeral cache; the CNID table is the source of
truth for identity.**

- `CatalogEntry` objects are rebuilt from disk on demand and can be
  evicted at any time.
- A lightweight `CnidTable` maps `(parentDirID, macName) ↔ cnid`
  persistently within a mount session.  It is the only structure
  that guarantees CNID stability.
- Directory contents are scanned lazily — only when the guest
  enumerates or looks up a child.  Unvisited subtrees cost nothing.
- Periodic invalidation (clearing cached entries) causes the next
  guest access to re-scan from the host filesystem, picking up any
  external changes.

---

## 3. Data Structures

### 3.1 CnidTable

A bidirectional identity map that only grows during a mount session.
Never evicted; cleared only on `mount()`.

```cpp
struct CnidKey {
    uint32_t    parentDirID;
    std::string macName;       // case-preserved Mac name

    // Case-insensitive comparison (Mac filenames are
    // case-insensitive).  Uses MacRoman tolower().
    bool operator==(const CnidKey &o) const;
};

// Case-insensitive hash matching CnidKey::operator==.
struct CnidKeyHash { /* hash parentDirID ^ case-folded hash macName */ };

struct CnidValue {
    CnidKey     key;           // identity: (parentDirID, macName)
    std::string hostPath;      // absolute host path at creation time
};

class CnidTable {
public:
    // Look up an existing CNID, or allocate a new one.
    // hostPath is stored for resolveHostPath(); ignored if
    // the CNID already exists.
    uint32_t resolve(uint32_t parentDirID, std::string_view macName,
                     std::string_view hostPath);

    // Reverse lookup: CNID → (parentDirID, macName, hostPath).
    // Returns nullptr if cnid was never assigned.
    const CnidValue *reverse(uint32_t cnid) const;

    // Update identity after a guest rename or move.  Changes the
    // forward key and the stored hostPath for an existing CNID.
    void updateKey(uint32_t cnid, uint32_t newParentDirID,
                   std::string_view newMacName,
                   std::string_view newHostPath);

    // Update hostPath for a CNID (used after move/rename of a
    // parent directory, to fix child paths).
    void updateHostPath(uint32_t cnid, std::string_view newHostPath);

    void clear();                    // mount-time reset
    uint32_t nextCnid() const;      // next value that will be assigned

private:
    std::unordered_map<CnidKey, uint32_t, CnidKeyHash> forward_;
    std::unordered_map<uint32_t, CnidValue>             reverse_;
    std::unordered_set<uint32_t> scanned_;   // directory CNIDs whose
                                             // children are in catalog_
    uint32_t nextCnid_ = 16;     // HFS reserves CNIDs 1–15
                                 // (root parent, root dir, extents
                                 // overflow, catalog file, etc.)
};
```

**Key properties:**

- **Case-insensitive keys.**  `CnidKey` equality and hash fold
  MacRoman characters to lowercase.  A host file renamed from
  "README" to "readme" matches the same CNID.

- **hostPath stored per CNID.**  `resolveHostPath(cnid)` is a
  direct reverse lookup — no parent-chain walking required.
  `rename()` and `move()` call `updateKey()` / `updateHostPath()`
  to keep the stored path current.

- **Monotonic, never freed.**  `resolve()` is the only allocation
  point.  CNIDs are never removed or reused.  A CNID is stable
  for the lifetime of the mount session.

- **Scanned set.**  `scanned_` tracks which directory CNIDs have
  had their children populated into `catalog_`.  This prevents
  `ensureScanned()` from re-running `directory_iterator` on every
  call within the same invalidation cycle.  `invalidateAll()`
  clears the set; `ensureScanned()` inserts into it.

### 3.2 CatalogEntry (no struct changes)

The existing `CatalogEntry` struct is not modified.  It carries
all per-file and per-directory metadata (CNID, parentDirID,
hostPath, macName, type/creator, Finder flags, fork sizes, dates,
`isText`, `dirFinderInfo`, etc.).  See `host_volume.h` for the
full definition.  Entries are populated from disk by
`ensureScanned()` and can be removed from the catalog vector at
any time.  The `isVirtual` flag continues to protect synthesized
entries (e.g. `Icon\r`) from eviction.

### 3.3 OpenFork (extended)

The `OpenFork` struct gains a `hostPath` field so that fork I/O is
self-contained and does not require the `CatalogEntry` to exist in
the catalog at the time of a read/write:

```cpp
struct OpenFork {
    uint32_t    cnid = 0;
    ForkType    fork = ForkType::Data;
    FILE       *fp = nullptr;
    bool        hasWrite = false;
    std::string hostPath;          // NEW — copied at open time
    bool        isText = false;    // NEW — copied at open time
};
```

When `readFork()` or `writeFork()` is called:
1. Look up the handle in `openForks_`.
2. If the `CatalogEntry` is in the catalog, use it (for size updates).
3. If evicted, use `OpenFork::hostPath` directly for I/O.  The entry
   can be resurrected via `cnidTable_.reverse(cnid)` if metadata
   updates are needed.

### 3.4 Catalog vector

```cpp
std::vector<CatalogEntry> catalog_;   // unchanged type
```

Remains a flat vector searched linearly.  Entries are added by
`ensureScanned()` and removed by `invalidateAll()`.

The root directory (CNID 2) has no `CatalogEntry` — it is handled
specially via `rootPath_` and `rootDirFinderInfo_`, as today.
`ensureScanned(kRootDirID)` uses `rootPath_` directly.

---

## 4. Key Algorithms

### 4.1 Mount (root-only scan)

```
mount(hostDir):
    cnidTable_.clear()
    catalog_.clear()
    openForks_.clear()

    // Existing mount steps (unchanged):
    //   - load global type mappings (once per process)
    //   - load per-volume typemap from .maxivmac/typemap.def
    //   - load root directory Finder info from sidecar

    // Register root directory in CNID table
    cnidTable_.resolve(kRootParentID, volumeName, rootPath_)  → CNID 2

    // Scan root directory only (one level)
    ensureScanned(kRootDirID)

    // Existing post-scan steps (unchanged):
    //   - inject virtual Icon\r if no real one exists
    //   - set kHasCustomIcon on root frFlags
```

No recursive scan.  Subdirectories get a `CatalogEntry` with their
CNID assigned, but their own children are not scanned until the
guest accesses them.

### 4.2 ensureScanned(dirID) — Lazy Directory Population

Called before any operation that needs a directory's children:
`nthChild()`, `childCount()`, `findByName()`.

```
ensureScanned(dirID):
    // Short-circuit: already scanned this invalidation cycle
    if dirID in cnidTable_.scanned_:
        return

    if dirID == kRootDirID:
        hostPath = rootPath_              // root is always known
    else:
        val = cnidTable_.reverse(dirID)
        if val == nullptr: return         // unknown directory
        hostPath = val->hostPath          // direct lookup, no chain

    if hostPath is empty or not a directory on disk:
        return                            // dir was deleted externally

    // Walk the host directory (one level)
    for each entry in directory_iterator(hostPath):
        skip hidden files, AppleDouble sidecars
        macName = MacNameFromHost(entry.name)
        cnid = cnidTable_.resolve(dirID, macName, entry.path)

        // Already in catalog? Update metadata in place.
        if catalog entry with this cnid exists:
            update sizes, dates from disk
            continue

        // New entry: build CatalogEntry from disk
        ce = buildCatalogEntry(entry, dirID, cnid, macName)
        catalog_.push_back(ce)

    // Remove catalog entries whose parentDirID == dirID
    // but whose host file no longer exists on disk.
    for each entry in catalog_ where parentDirID == dirID:
        if not exists(entry.hostPath) and not isVirtual:
            remove from catalog_

    cnidTable_.scanned_.insert(dirID)
```

The `scanned_` check at the top prevents re-running the
`directory_iterator` when the Finder calls `GetCatInfo` in a
tight loop (index 1, 2, 3…) to enumerate a directory.  Without
it, each call would re-scan — N × `directory_iterator` for N
files.  The flag is cleared by `invalidateAll()`.

Cost per scan: one `directory_iterator` + one `stat()` per child.
Only the requested directory is scanned — not its subdirectories.

### 4.3 invalidateDir(dirID) — Evict Cached Children

Removes all non-virtual, non-open-fork file entries that are
children of `dirID` from `catalog_`.  Subdirectory entries within
this directory are also removed (their own identity persists in
`cnidTable_`).

```
invalidateDir(dirID):
    openCnids = { of.cnid for of in openForks_ }

    remove from catalog_ where:
        parentDirID == dirID
        AND NOT isVirtual
        AND cnid NOT IN openCnids
```

After invalidation, the next `nthChild()` or `childCount()` call
triggers `ensureScanned()`, which rebuilds from disk.

### 4.4 invalidateAll() — Periodic Full Eviction

Called periodically (e.g. every 1–2 seconds) to force the catalog
to re-sync with the host filesystem.

```
invalidateAll():
    openCnids = { of.cnid for of in openForks_ }

    remove from catalog_ where:
        NOT isVirtual
        AND cnid NOT IN openCnids

    cnidTable_.scanned_.clear()     // force re-scan on next access
```

All entries are rebuilt on next guest access.  The root directory
has no CatalogEntry (it is implicit), so no special-case is needed.

### 4.5 findByCNID(cnid) — Resurrection

When `findByCNID()` is called for a CNID not currently in the
catalog (evicted, or never scanned), it can be resurrected:

```
findByCNID(cnid):
    // Fast path: already in catalog
    for entry in catalog_:
        if entry.cnid == cnid: return &entry

    // Resurrection: look up identity + hostPath from cnidTable
    val = cnidTable_.reverse(cnid)
    if val == nullptr: return nullptr

    // Ensure the parent directory is scanned (will add our entry).
    // ensureScanned resolves the parent's hostPath via cnidTable_
    // — no chain walking needed.
    ensureScanned(val->key.parentDirID)

    // Try again
    for entry in catalog_:
        if entry.cnid == cnid: return &entry

    return nullptr    // file was deleted from disk
```

### 4.6 readFork / writeFork — Open Fork Resilience

Fork I/O uses `OpenFork::hostPath` directly when the `CatalogEntry`
has been evicted.  The only reason to look up the entry is to
update cached sizes/dates after a write:

```
readFork(handle, offset, buf):
    of = openForks_[handle]
    e = mutableFindByCNID(of.cnid)

    if of.fork == Resource:
        // Uses of.hostPath — does not need CatalogEntry
        data = ReadResourceFork(of.hostPath, offset, count)
        ...

    if of.isText:
        // Uses of.hostPath
        converted = MacRomanFromUTF8File(of.hostPath)
        ...

    // Non-TEXT data fork: uses of.fp (FILE*)
    fseek(of.fp, offset, SEEK_SET)
    fread(buf, ...)
```

After writes, if `e != nullptr`, update `e->dataForkSize` and
`e->modDate`.  If `e` is null (evicted), skip the update — the
entry will get fresh values from disk when rebuilt.

---

### 4.7 rename() / move() — CnidTable Consistency

When the guest renames or moves a file, the cnidTable must be
updated so that a future re-scan matches the new name/location:

```
rename(dirID, oldMacName, newMacName):
    e = findByName(dirID, oldMacName)
    ... rename on host filesystem ...
    ... update CatalogEntry ...

    // Update cnidTable so re-scan finds the new name
    cnidTable_.updateKey(e->cnid, dirID, newMacName, newHostPath)

move(srcDirID, macName, dstDirID):
    e = findByName(srcDirID, macName)
    ... move on host filesystem ...
    ... update CatalogEntry ...

    // Update cnidTable with new parent + new hostPath
    cnidTable_.updateKey(e->cnid, dstDirID, macName, newHostPath)

    // For directory moves, update hostPath of all descendants
    if e->isDirectory:
        for each descendant cnid in cnidTable_ with matching
        hostPath prefix:
            cnidTable_.updateHostPath(descendant, newPrefix + suffix)
```

Without this, after eviction + re-scan, the scanner would see the
new name, fail to match it in the forward map, allocate a fresh
CNID, and orphan any open forks on the old CNID.

---

## 5. Integration Points

### 5.1 HostVolume::mount()

**File:** `src/storage/host_volume.cpp`

**Change:** replace the recursive `scanDirectory(hostDir, kRootDirID)`
call with `ensureScanned(kRootDirID)`.  Initialize `cnidTable_`
before scanning.  Register root in cnidTable:
`cnidTable_.resolve(kRootParentID, volumeName, rootPath_)`.

All other mount steps are unchanged: global/per-volume type-map
loading, root directory Finder info from sidecar, virtual Icon\r
injection, kHasCustomIcon flag.

### 5.2 HostVolume query methods

**File:** `src/storage/host_volume.cpp`

**Change:** `nthChild()`, `childCount()`, and `findByName()` call
`ensureScanned(parentDirID)` before iterating the catalog.
`findByCNID()` gains the resurrection path (§4.5).

### 5.3 OpenFork population

**File:** `src/storage/host_volume.cpp`, `openFork()` method

**Change:** copy `hostPath` and `isText` into the `OpenFork` struct
at open time.

### 5.4 Fork I/O methods

**File:** `src/storage/host_volume.cpp`, `readFork()` / `writeFork()`

**Change:** use `of.hostPath` and `of.isText` instead of requiring a
non-null `CatalogEntry`.  Tolerate `mutableFindByCNID()` returning
null (skip metadata updates).

### 5.5 rename() / move()

**File:** `src/storage/host_volume.cpp`, `rename()` and `move()`

**Change:** after updating the CatalogEntry and host filesystem,
call `cnidTable_.updateKey()` to keep the forward/reverse maps
consistent (§4.7).

### 5.6 Periodic invalidation hook

**File:** `src/core/extn_extfs.cpp`

**Change:** add a tick-gated call to `invalidateAll()` through
`DriveManager`, called from the existing ExtFS poll path.  The
poll already runs at regular intervals; no new timer infrastructure
is needed.

### 5.7 DriveManager

**File:** `src/storage/drive_manager.h` / `drive_manager.cpp`

**Change:** add `void invalidateAll()` that iterates all mounted
slots and calls `vol.invalidateAll()` on each.

### 5.8 volumeStats()

**File:** `src/storage/host_volume.cpp`

**Change:** `volumeStats()` continues to iterate only the in-memory
catalog.  After eviction it reports a partial count — this is
acceptable.  The Finder performs its own recursive enumeration when
it needs a true folder count (e.g. Get Info, copy pre-flight).
We serve each directory correctly via `ensureScanned()`; aggregate
accuracy is the Finder's responsibility, not ours.

The free-space direction is always safe: partial counts mean lower
`totalBytes`, which computes *more* free blocks, never "disk full."

`ioVNxtCNID` should use `cnidTable_.nextCnid()` instead of the
old `files + 16` computation.

---

## 6. Refresh Triggering

The ExtFS poll handler in `extn_extfs.cpp` runs once per emulated
vblank (~60 Hz).  For MVP, `invalidateAll()` is gated behind a
counter and called every 60–120 poll cycles (i.e. roughly every
1–2 seconds of wall-clock time).  This is sufficient because:

- The Finder polls open windows every 2–3 seconds
- Directory iteration always re-scans from disk after invalidation
- File content is never cached (reads go to disk every time)

The periodic invalidation has minimal cost: it is a single pass
over the catalog vector removing entries, plus clearing the
`scanned_` set.  The real disk I/O happens lazily when the guest
next accesses the directory.

### Future refinement (not for MVP)

The design supports adding smarter eviction without structural
changes:

- **mtime gating:** keep entries whose on-disk mtime matches the
  cached `modDate`, only evict stale ones.  Reduces disk I/O for
  unchanged directories.
- **Access-sequence LRU:** stamp entries with a monotonic request
  counter; evict entries older than N requests.  Caps memory for
  volumes with many directories.
- **Combined:** keep if mtime matches AND recently accessed.

These are pure policy changes to `invalidateAll()` — no other
method signatures or data structures change.

---

## 7. Invariants

1. **CNID stability:** a `(parentDirID, macName)` pair always
   resolves to the same CNID within a mount session.  CNIDs are
   never reused or reassigned.  Comparison is case-insensitive
   (MacRoman folding).

2. **Monotonic allocation:** `cnidTable_.nextCnid_` only increases.
   No CNID is ever freed.

3. **cnidTable tracks mutations:** `rename()` and `move()` update
   the cnidTable forward/reverse maps so that re-scans match the
   new name/location to the existing CNID.

4. **hostPath always available:** every CNID has a stored `hostPath`
   in the cnidTable reverse map.  `resolveHostPath()` is a direct
   lookup — no parent-chain walking.

5. **Virtual entries survive eviction:** entries with `isVirtual ==
   true` are never removed from the catalog by invalidation.

6. **Open forks survive eviction:** entries with an active handle in
   `openForks_` are never evicted.  Fork I/O uses `OpenFork::hostPath`
   directly, so it works even if the entry is evicted between open
   and the next I/O call.

7. **No recursive scan:** `ensureScanned()` scans one directory
   level.  Subdirectories are discovered (assigned CNIDs) but their
   children are not populated until accessed.

8. **Scanned flag prevents hot-loop rescans.**  The `scanned_` set
   in `CnidTable` ensures a `GetCatInfo` enumeration loop does not
   re-run `directory_iterator` on every call.  The set is cleared
   by `invalidateAll()`; `ensureScanned()` inserts into it.

9. **Deletion is eventual:** a host file deleted externally will
   disappear from the guest catalog at the next `ensureScanned()`
   call for its parent directory.  Open forks on deleted files will
   get I/O errors from the OS on the next read/write.

10. **Volume stats are approximate.** `volumeStats()` reports only
   currently-cached entries.  Free-space errs on the generous side
   (never "disk full").  The Finder handles true recursive counts
   itself.

---

## 8. Build Integration

No new dependencies.  The `CnidTable` class lives in
`host_volume.h`, private to `HostVolume`.

---

## 9. Testing

- **Existing golden tests** must continue to pass — the catalog
  cache is transparent to correct guest operation.
- **Unit tests** for `CnidTable`: verify `resolve()` stability,
  `reverse()` correctness, `clear()` reset.
- **Manual test:** mount a shared folder, open it in Finder, add a
  file from the host, verify it appears in the guest within 2–3
  seconds.  Delete from host, verify it disappears.
- **Open fork resilience:** open a file in a guest app, trigger
  invalidation, verify reads/writes still work.
