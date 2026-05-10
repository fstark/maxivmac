# Catalog Cache — Implementation Plan

Design: [CACHE_DESIGN.md](CACHE_DESIGN.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 0 | CACHE diagnostic channel | |
| 1 | CnidTable data structure + unit tests | |
| 2 | OpenFork extension + fork I/O resilience | |
| 3 | Lazy scanning (ensureScanned + mount refactor) | |
| 4 | Query methods + findByCNID resurrection | |
| 5 | Invalidation (invalidateDir, invalidateAll) | |
| 6 | rename() / move() — CnidTable consistency | |
| 7 | Periodic refresh hook + volumeStats update | |
| 8 | End-to-end smoke test + manual verification | |

Build gate: `cmake --preset macos && cmake --build --preset macos`
Test gate:  `ctest --preset macos`

---

## General Guidance — Comments

Every non-trivial block of new code must carry a brief comment
explaining *why*, not just *what*.  Use the knowledge and rationale
from the design document and this plan to write the comments, but
**never cite the design or plan docs from code comments** — no
`// See CACHE_DESIGN.md` or `// per §4.2`.  Comments must be
self-contained: a reader who has never seen the design doc should
understand the *why* from the comment alone.

Match the tone and density of existing comments in
[host_volume.h](../src/storage/host_volume.h) and
[host_volume.cpp](../src/storage/host_volume.cpp).

Specifically:

- Each new struct and class gets a block comment describing its
  purpose and key properties (e.g. "bidirectional map, only grows,
  cleared on mount").
- Each new public method gets a one-line `//` comment above it
  summarizing what it does and any non-obvious preconditions.
- Invariants (e.g. "CNIDs are never reused," "scanned_ prevents
  hot-loop rescans") should appear as inline comments at the code
  sites that enforce them.
- The `ensureScanned()` algorithm is the heart of the cache; its
  implementation deserves a multi-line block comment explaining the
  flow: short-circuit → resolve hostPath → iterate → update/add →
  prune deleted → mark scanned.
- The periodic invalidation hook (Phase 7) should explain the
  tick-gating rationale (guest calls ~60 Hz, we fire every ~1.5 s).

Follow [STYLE.md](STYLE.md) and [NAMING.md](NAMING.md) throughout.

---

## Phase 0 — CACHE Diagnostic Channel

Register a dedicated `CACHE` diagnostic subsystem so that all
cache-related tracing can be toggled independently from the
general `ExtFS` channel.  This keeps the output manageable:
`--diag=CACHE` shows only cache lifecycle, while `--diag=ExtFS`
continues to show trap-level I/O as before.

### 0.1 — Add DiagSubsystem::CACHE

**File:** `src/core/diag.h`

Add a new entry before `kCount`:

```cpp
CACHE, /* [CACHE] catalog cache: scan, evict, CNID resolve  */
```

### 0.2 — Register the tag string

**File:** `src/core/diag.cpp`

Add `"CACHE"` to the `tag()` switch and `fromName()` lookup,
following the existing pattern for other channels.

### 0.3 — Add convenience macro

**File:** `src/storage/host_volume.cpp` (file-scope, near includes)

Define a local shorthand to keep call sites compact:

```cpp
#define CACHE_LOG(fmt, ...) DIAG(CACHE, fmt "\n", ##__VA_ARGS__)
```

Guidelines for `CACHE_LOG` messages across all phases:

- **One line per event**.  Format: `verb: key=value key=value …`.
  Examples: `resolve: new cnid=17 parent=2 name="README"`.
- **Include the CNID and macName** in every message that touches
  an identity.  These are the primary correlation keys when
  reading log output.
- **Avoid logging inside tight loops** that iterate every catalog
  entry (e.g. `findByCNID` linear scan).  Log the *outcome*, not
  each iteration step.
- **Log at decision boundaries**: cache hit vs. miss, scan
  triggered vs. short-circuited, entry evicted vs. retained.

### Fence

- [ ] `DiagSubsystem::CACHE` compiles and toggles independently
- [ ] `--diag=CACHE` enables the channel (manual verification)
- [ ] `CACHE_LOG` macro defined in `host_volume.cpp`
- [ ] Full build clean
- [ ] Commit: `"cache: phase 0 — CACHE diagnostic channel"`

---

## Phase 1 — CnidTable Data Structure + Unit Tests

Introduce the `CnidTable` class as a standalone, testable component
inside `HostVolume`.  No behavioral changes to the emulator yet — the
table is created but not wired into any query paths.

### 1.1 — Define CnidKey, CnidKeyHash, CnidValue, CnidTable

**File:** `src/storage/host_volume.h`

Add the following types *inside* the `private:` section of
`HostVolume`, above the existing `OpenFork` struct.  They are
implementation details of `HostVolume` and should not be visible
outside the class.

```cpp
// Identity key for the CNID table.  Matches Mac filesystem
// semantics: (parentDirID, macName) identifies a catalog node.
// Comparison and hashing are case-insensitive (MacRoman folding)
// because Mac filenames are case-insensitive.
struct CnidKey {
    uint32_t    parentDirID = 0;
    std::string macName;           // case-preserved Mac name

    bool operator==(const CnidKey &o) const;
};

// Case-insensitive hash for CnidKey.
// Combines parentDirID with a case-folded FNV-1a of macName.
struct CnidKeyHash {
    std::size_t operator()(const CnidKey &k) const;
};

// Reverse-lookup value: stores identity and hostPath for a CNID.
struct CnidValue {
    CnidKey     key;        // (parentDirID, macName) that owns this CNID
    std::string hostPath;   // absolute host path at creation time
};

// Bidirectional identity map: (parentDirID, macName) ↔ CNID.
// Only grows during a mount session — CNIDs are never freed or
// reused.  Cleared on mount().
class CnidTable {
public:
    // Look up an existing CNID for (parentDirID, macName), or
    // allocate a new one.  hostPath is stored for reverse lookup;
    // ignored if the CNID already exists.
    uint32_t resolve(uint32_t parentDirID, std::string_view macName,
                     std::string_view hostPath);

    // Reverse lookup: CNID → (parentDirID, macName, hostPath).
    // Returns nullptr if cnid was never assigned.
    const CnidValue *reverse(uint32_t cnid) const;

    // Update identity after a guest rename or move.  Changes the
    // forward key and stored hostPath for an existing CNID.
    void updateKey(uint32_t cnid, uint32_t newParentDirID,
                   std::string_view newMacName,
                   std::string_view newHostPath);

    // Update hostPath only (used after parent dir move/rename to
    // fix descendant paths).
    void updateHostPath(uint32_t cnid, std::string_view newHostPath);

    void clear();                    // mount-time reset
    uint32_t nextCnid() const;      // next CNID that will be assigned

    // Directory scan tracking: prevents re-running directory_iterator
    // for the same parent within one invalidation cycle.
    bool isScanned(uint32_t dirID) const;
    void markScanned(uint32_t dirID);
    void clearScannedFor(uint32_t dirID);  // single-directory invalidation
    void clearScanned();                   // called by invalidateAll()

    // Size of internal maps (for testing / diagnostics).
    std::size_t size() const;

private:
    std::unordered_map<CnidKey, uint32_t, CnidKeyHash> forward_;
    std::unordered_map<uint32_t, CnidValue>             reverse_;
    std::unordered_set<uint32_t> scanned_;

    // HFS reserves CNIDs 1–15 (root parent, root dir, extents
    // overflow, catalog file, etc.).
    uint32_t nextCnid_ = 16;
};
```

### 1.2 — Implement CnidTable

**File:** `src/storage/host_volume.cpp` (or a new section at the top,
before `mount()`).

Implement the methods defined above:

**`CnidKey::operator==`** — compare `parentDirID` by value, then
compare `macName` character-by-character using MacRoman case
folding (use the same `tolower(static_cast<unsigned char>(c))`
idiom already used in `findByName()`).

**`CnidKeyHash::operator()`** — hash `parentDirID` XOR'd with an
FNV-1a hash of the lowercased `macName` bytes.

**`resolve()`** — look up `(parentDirID, macName)` in `forward_`.
If found, return the existing CNID (ignore hostPath).  Otherwise
allocate `nextCnid_++`, insert into both `forward_` and
`reverse_`, return the new CNID.
Log on new allocation only (not on cache hits — those are too
frequent):

```cpp
CACHE_LOG("resolve: new cnid=%u parent=%u name=\"%s\"",
          cnid, parentDirID, std::string(macName).c_str());
```

**`reverse()`** — look up `cnid` in `reverse_`.  Return pointer to
`CnidValue` or `nullptr`.  No logging (called in tight loops).

**`updateKey()`** — erase the old key from `forward_`, build a new
`CnidKey{newParentDirID, newMacName}`, insert into `forward_`
with the same CNID, update `reverse_[cnid]` with new key and
hostPath.  Log the identity change:

```cpp
CACHE_LOG("updateKey: cnid=%u -> parent=%u name=\"%s\"",
          cnid, newParentDirID, std::string(newMacName).c_str());
```

**`updateHostPath()`** — update only `reverse_[cnid].hostPath`.
Log the path change:

```cpp
CACHE_LOG("updateHostPath: cnid=%u -> \"%s\"",
          cnid, std::string(newHostPath).c_str());
```

**`clear()`** — clear all three maps, reset `nextCnid_ = 16`.
Log the size before clearing:

```cpp
CACHE_LOG("clear: dropping %zu entries", forward_.size());
```

**`nextCnid()`** — return `nextCnid_`.

**`isScanned()` / `markScanned()` / `clearScannedFor()` /
`clearScanned()`** — thin wrappers around `scanned_.contains()`,
`scanned_.insert()`, `scanned_.erase()`, `scanned_.clear()`.

**`size()`** — return `forward_.size()`.

### 1.3 — Add CnidTable member to HostVolume

**File:** `src/storage/host_volume.h`

Add to `private:` members of `HostVolume`:

```cpp
CnidTable cnidTable_;   // CNID identity table — maps (parentDirID, macName) ↔ CNID
```

At this point the member exists but is unused.  All existing
behavior is unchanged.

### 1.4 — Unit Tests for CnidTable

**File:** `test/test_host_volume.cpp` (append to existing file)

Add a section with a comment header:

```cpp
/* ── CnidTable unit tests ─────────────────────────── */
```

Test cases:

- **`CnidTable: resolve allocates sequential CNIDs`** — call
  `resolve()` three times with different names, verify CNIDs are
  16, 17, 18.

- **`CnidTable: resolve is stable`** — call `resolve()` twice with
  the same `(parentDirID, macName)`, verify same CNID returned.

- **`CnidTable: resolve is case-insensitive`** — resolve "README"
  then "readme" with the same parentDirID, verify same CNID.

- **`CnidTable: reverse lookup`** — resolve a name, then call
  `reverse(cnid)` and verify `key.parentDirID`, `key.macName`,
  and `hostPath` match.

- **`CnidTable: reverse unknown CNID`** — `reverse(9999)` returns
  `nullptr`.

- **`CnidTable: updateKey`** — resolve a name, call `updateKey()`
  with a new parent/name/path, verify `reverse()` returns updated
  values and the old forward key no longer resolves to this CNID.

- **`CnidTable: updateHostPath`** — resolve a name, call
  `updateHostPath()` with a new path, verify `reverse()` returns
  the new path while key is unchanged.

- **`CnidTable: clear resets state`** — resolve some names, call
  `clear()`, verify `size() == 0` and `nextCnid() == 16`.

- **`CnidTable: scanned tracking`** — verify `isScanned(42)` is
  false, call `markScanned(42)`, verify `isScanned(42)` is true,
  call `clearScanned()`, verify `isScanned(42)` is false again.

Because `CnidTable` is a private nested class of `HostVolume`, tests
cannot access it directly.  **Solution:** make `CnidTable` a
file-scope class in `host_volume.cpp` (inside an anonymous namespace
or the `storage` namespace) rather than a nested class.  It is still
an implementation detail — it has no header declaration — but test
code in `test_host_volume.cpp` can forward-declare it or include a
small internal header (`host_volume_internal.h`) that exposes just
the `CnidTable` type.  This avoids `friend` hacks and keeps the
public `HostVolume` API unchanged.

Alternatively, if the above feels like too much plumbing for Phase 1,
keep `CnidTable` private and test it indirectly through `HostVolume`
public methods in Phase 4.  In that case, move the CnidTable-specific
test cases listed above into Phase 4 and add a note.  **Pick one
approach and stick with it; do not leave a choice for the executor.**

### Fence

- [ ] `CnidKey`, `CnidKeyHash`, `CnidValue`, `CnidTable` defined in `host_volume.h`
- [ ] All `CnidTable` methods implemented in `host_volume.cpp`
- [ ] `cnidTable_` member exists in `HostVolume`
- [ ] `CACHE_LOG` calls in `resolve()`, `updateKey()`, `updateHostPath()`, `clear()`
- [ ] Unit tests pass: `ctest --preset macos`
- [ ] Full build clean
- [ ] Commit: `"cache: phase 1 — CnidTable data structure + unit tests"`

---

## Phase 2 — OpenFork Extension + Fork I/O Resilience

Extend `OpenFork` so that fork I/O does not require a live
`CatalogEntry`.  This is a prerequisite for eviction (Phase 5) —
without it, invalidating a `CatalogEntry` while a fork is open
would cause I/O failures.

### 2.1 — Extend OpenFork struct

**File:** `src/storage/host_volume.h`

Add two fields to the private `OpenFork` struct:

```cpp
struct OpenFork {
    uint32_t    cnid = 0;
    ForkType    fork = ForkType::Data;
    FILE       *fp = nullptr;
    bool        hasWrite = false;
    std::string hostPath;      // NEW — absolute path, copied at open time
    bool        isText = false; // NEW — copied at open time
};
```

Comment: "hostPath and isText are copied from CatalogEntry at open
time so that fork I/O remains functional even if the catalog entry
is evicted by invalidation."

### 2.2 — Populate new fields in openFork()

**File:** `src/storage/host_volume.cpp`, `openFork()` method
(currently at ~line 507)

After the `findByCNID(cnid)` lookup succeeds, when constructing the
`OpenFork` value to insert into `openForks_`, set:

```cpp
openForks_[handle] = {cnid, ForkType::Data, fp, wantWrite,
                      e->hostPath, e->isText};
```

Do this for both the data-fork and resource-fork branches.

### 2.3 — Update readFork() to tolerate evicted entries

**File:** `src/storage/host_volume.cpp`, `readFork()` method
(currently at ~line 593)

Current code calls `mutableFindByCNID(of.cnid)` and returns
`kFnfErr` if null.  Change to:

1. Call `mutableFindByCNID(of.cnid)`.
2. If null (entry evicted), proceed using `of.hostPath` and
   `of.isText` instead of `e->hostPath` and `e->isText`.
   Log the evicted-entry fallback:

   ```cpp
   CACHE_LOG("readFork: cnid=%u evicted, using of.hostPath", of.cnid);
   ```
3. For resource-fork reads: use `of.hostPath` instead of
   `e->hostPath` in calls to `appledouble::ReadResourceFork()`.
4. For TEXT reads: use `of.hostPath` instead of `e->hostPath` in
   `appledouble::MacRomanFromUTF8File()`.
5. For data-fork reads: continue using `of.fp` (unchanged).

The only thing that requires `e` is updating text stats; skip that
when `e` is null.

### 2.4 — Update writeFork() to tolerate evicted entries

**File:** `src/storage/host_volume.cpp`, `writeFork()` method
(currently at ~line 658)

Same pattern as readFork():

1. Call `mutableFindByCNID(of.cnid)`.
2. If null, use `of.hostPath` / `of.isText` for I/O.
   Log the evicted-entry fallback:

   ```cpp
   CACHE_LOG("writeFork: cnid=%u evicted, using of.hostPath", of.cnid);
   ```
3. After writes: if `e != nullptr`, update `e->dataForkSize` and
   `e->modDate` as before.  If `e` is null, skip the metadata
   update — values will be refreshed from disk when the entry is
   rebuilt.

Comment: "Entry may be null if evicted between open and I/O.
Fork I/O uses of.hostPath/of.isText copied at open time."

### 2.5 — Update setEOF() to tolerate evicted entries

**File:** `src/storage/host_volume.cpp`, `setEOF()` method
(currently at ~line 716)

`setEOF()` calls `mutableFindByCNID(of.cnid)` and returns `kFnfErr`
if null.  Same treatment as readFork/writeFork:

1. If `e` is null, use `of.hostPath` for resource-fork operations
   (`appledouble::SetResourceForkSize()`).
   Log the evicted-entry fallback:

   ```cpp
   CACHE_LOG("setEOF: cnid=%u evicted, using of.hostPath", of.cnid);
   ```
2. For data forks, `of.fp` is already available — `ftruncate()` works
   without a `CatalogEntry`.
3. Skip `e->dataForkSize` / `e->rsrcForkSize` / `e->modDate` updates
   when `e` is null.

### 2.6 — Virtual fork edge case

When populating `OpenFork` for a virtual entry (e.g. `Icon\r`),
`hostPath` should be left empty and `isText` false.  Virtual fork
I/O already has its own code path (`virtualIconFork_`).  Verify
that `readFork()` / `writeFork()` check `e->isVirtual` (or the
virtual icon path) *before* falling through to the `of.hostPath`
path — otherwise an empty `hostPath` will cause I/O failures.

### 2.7 — Tests

**File:** `test/test_host_volume.cpp`

Add test cases:

- **`HostVolume: openFork populates hostPath and isText`** — mount a
  volume with a TEXT file, open its data fork, verify the returned
  handle works.  (This is really testing existing behavior isn't
  broken; the new fields are internal.)

- **`HostVolume: fork read/write still work after open`** — mount,
  create a file, open its data fork, write some bytes, read them
  back, verify correctness.  This is a sanity check that the
  OpenFork extension didn't break anything.

### Fence

- [ ] `OpenFork` has `hostPath` and `isText` fields
- [ ] `openFork()` copies both fields from `CatalogEntry`
- [ ] `readFork()`, `writeFork()`, and `setEOF()` tolerate null `CatalogEntry`
- [ ] Evicted-entry fallback logged in `readFork`, `writeFork`, `setEOF`
- [ ] Virtual forks work correctly (empty `hostPath` handled)
- [ ] Existing fork tests still pass
- [ ] New tests pass
- [ ] Full build clean
- [ ] Commit: `"cache: phase 2 — OpenFork extension + fork I/O resilience"`

---

## Phase 3 — Lazy Scanning (ensureScanned + mount refactor)

Replace the recursive `scanDirectory()` call in `mount()` with a
root-only scan via `ensureScanned()`.  After this phase, subdirectory
contents are populated on demand.

### 3.1 — Implement ensureScanned()

**File:** `src/storage/host_volume.cpp`

Add a new private method:

```cpp
// Lazily populate catalog entries for children of dirID.
// Scans one directory level from the host filesystem.  Skips hidden
// files and AppleDouble sidecars.  New entries get CNIDs from
// cnidTable_; existing entries are updated in place.
// Entries whose host file no longer exists are pruned.
// Short-circuits if this directory was already scanned this
// invalidation cycle (tracked by cnidTable_.scanned_).
void HostVolume::ensureScanned(uint32_t dirID);
```

**File:** `src/storage/host_volume.h`

Add the declaration in the `private:` section, near `scanDirectory()`.

Implementation follows the algorithm in CACHE_DESIGN.md §4.2 closely:

1. **Short-circuit:** if `cnidTable_.isScanned(dirID)`, return.
   No logging on short-circuit (too noisy — this fires on every
   `GetCatInfo` during Finder enumeration).
2. **Resolve hostPath:**
   - If `dirID == kRootDirID`, use `rootPath_`.
   - Otherwise, `auto *val = cnidTable_.reverse(dirID)`.  If null,
     return (unknown directory).  Use `val->hostPath`.
3. **Validate:** if hostPath is empty or not a directory on disk,
   return.
4. **Walk directory** with `std::filesystem::directory_iterator`:
   - Skip hidden files (name starts with `.`).
   - Skip AppleDouble sidecars (`._` prefix).
   - Compute `macName` via existing `MacNameFromHost()` helper.
   - `cnid = cnidTable_.resolve(dirID, macName, entry.path())`.
   - If a `CatalogEntry` with this `cnid` already exists in
     `catalog_`, update its sizes/dates from disk (call the same
     metadata-reading logic `scanDirectory()` uses today).  Continue.
   - Otherwise build a new `CatalogEntry` using
     `buildCatalogEntry()` (see 3.1a below) and `push_back` into
     `catalog_`.
5. **Prune deleted:** iterate `catalog_`, remove entries where
   `parentDirID == dirID` and `!isVirtual` and the host file no
   longer exists on disk.
6. **Mark scanned:** `cnidTable_.markScanned(dirID)`.

After step 6, log a single summary line with the scan result.
Track `added`, `updated`, and `pruned` counters through steps 4–5:

```cpp
CACHE_LOG("ensureScanned: dir=%u added=%u updated=%u pruned=%u total=%zu",
          dirID, added, updated, pruned, catalog_.size());
```

If step 5 prunes any entries, log each pruned entry individually
(these are unexpected host-side deletions and worth tracking):

```cpp
CACHE_LOG("ensureScanned: pruned cnid=%u name=\"%s\"",
          e.cnid, e.macName.c_str());
```

This method replaces the per-directory work that `scanDirectory()`
did recursively.  `scanDirectory()` itself is retained temporarily
but will be unused after the mount refactor (3.2).

#### 3.1a — Extract buildCatalogEntry() helper

**File:** `src/storage/host_volume.cpp`

`scanDirectory()` currently has two inline branches: one for
directories (reads mtime, loads DirFinderInfo) and one for regular
files (calls `appledouble::GetFileInfo()`, populates type/creator/
flags/sizes/dates/isText).  Extract this into a private helper:

```cpp
// Build a CatalogEntry from a host filesystem entry.
// Populates metadata (type, creator, sizes, dates) from the
// AppleDouble sidecar and file stats.
CatalogEntry buildCatalogEntry(
    const std::filesystem::directory_entry &entry,
    uint32_t parentDirID, uint32_t cnid,
    std::string_view macName);
```

Both `ensureScanned()` and `scanDirectory()` (while it exists)
should call this helper.  This avoids duplicating ~30 lines of
metadata-filling logic.

### 3.2 — Refactor mount() to use ensureScanned()

**File:** `src/storage/host_volume.cpp`, `mount()` method

Replace:

```cpp
scanDirectory(hostDir, kRootDirID);
```

With:

```cpp
// Initialize CNID table.  Register root directory (CNID 2).
cnidTable_.clear();
cnidTable_.resolve(kRootParentID, rootPath_.filename().string(),
                   rootPath_.string());

// Scan root directory only — subdirectories are scanned lazily
// when the guest accesses them.
ensureScanned(kRootDirID);
```

Log the mount event after `ensureScanned` returns:

```cpp
CACHE_LOG("mount: root=\"%s\" rootChildren=%d cnids=%zu",
          rootPath_.string().c_str(), childCount(kRootDirID),
          cnidTable_.size());
```
```

Where `kRootParentID` is 1 (the HFS parent of root).  The Mac-visible
volume name is set by `DriveManager` *after* `mount()` returns, so it
is not available here.  Use the directory basename instead — the CNID
identity for root is looked up by dirID (2), not by name, so the
exact string does not matter for correctness.

After this change, `mount()` only scans one directory level.
Subdirectories appear in the root catalog (with CNIDs assigned) but
their children are not populated until accessed.

### 3.3 — Remove or deprecate scanDirectory()

**File:** `src/storage/host_volume.cpp` / `host_volume.h`

If `scanDirectory()` is no longer called, mark it with a `// TODO:
remove — replaced by ensureScanned()` comment.  Do not delete it in
this phase to keep the diff small and reversible.  Actual removal
is Phase 8.

### 3.4 — Tests

**File:** `test/test_host_volume.cpp`

All existing tests must continue to pass with the lazy-scan
behavior.  The key difference: subdirectory contents that were
previously populated at mount are now populated on first access.
Since existing tests call `findByName()` / `nthChild()` /
`childCount()` which will trigger `ensureScanned()` (wired in
Phase 4), the tests should pass transparently.

**However**, Phase 3 cannot be landed independently — sub-directory
tests will fail because the query methods don't call
`ensureScanned()` until Phase 4.  **Phases 3 and 4 must be committed
together as a single unit.**

Add one new test:

- **`HostVolume: lazy scan — subdirectory not populated until
  accessed`** — mount a directory with a subdirectory containing a
  file.  Immediately after mount, verify `childCount(rootDirID)` sees
  the subdirectory.  Then verify `childCount(subDirCnid)` returns the
  correct child count (proving `ensureScanned()` ran for the subdir).

### Fence (Phase 3 — verified jointly with Phase 4)

- [ ] `ensureScanned()` declared and implemented
- [ ] `ensureScanned()` logs summary line with added/updated/pruned counts
- [ ] `ensureScanned()` logs individual pruned entries
- [ ] `buildCatalogEntry()` helper extracted from `scanDirectory()`
- [ ] `mount()` calls `ensureScanned(kRootDirID)` instead of `scanDirectory()`
- [ ] `mount()` logs root mount summary
- [ ] `cnidTable_` is cleared and root is registered in `mount()`
- [ ] New lazy-scan test passes
- [ ] (Full build + existing tests verified in Phase 4 fence)

---

## Phase 4 — Query Methods + findByCNID Resurrection

Wire `ensureScanned()` into every query method so that directory
contents are populated on demand.  Add CNID resurrection to
`findByCNID()`.

### 4.1 — Gate nthChild(), childCount(), findByName() behind ensureScanned()

**File:** `src/storage/host_volume.cpp`

At the top of each method, add:

```cpp
ensureScanned(parentDirID);
```

This ensures the directory's children exist in `catalog_` before the
linear search runs.

For `nthChild()` and `childCount()`, the parameter is `parentDirID`.
For `findByName()`, the parameter is also `parentDirID`.

Since `ensureScanned()` short-circuits when already scanned, this adds
negligible cost to the hot path (a single `unordered_set::contains()`).

These methods must become non-const (they modify `catalog_` and
`cnidTable_`).  Update the declarations in `host_volume.h`:

```cpp
const CatalogEntry *nthChild(uint32_t parentDirID, int index);       // was const
int                 childCount(uint32_t parentDirID);                 // was const
const CatalogEntry *findByName(uint32_t parentDirID, std::string_view macName); // was const
const CatalogEntry *findByPath(uint32_t startDirID, std::string_view hfsPath);  // was const
```

`findByPath()` transitively calls `findByName()`, which is now
non-const, so `findByPath()` must also drop `const`.  No other
changes to `findByPath()` are needed — lazy scanning happens
through `findByName()`.

Also update `volumeStats()` to drop `const`:

```cpp
void volumeStats(uint32_t &outFiles, uint32_t &outDirs, uint32_t &outBytes); // was const
```

`volumeStats()` does not call `ensureScanned()` (it reports only
cached entries; see design §5.8), but it will later use
`cnidTable_` in Phase 7.  Dropping const now avoids a second
signature change.

### 4.1a — Switch resolveParentPath() to use CnidTable

**File:** `src/storage/host_volume.cpp`, `resolveParentPath()` method

`resolveParentPath()` currently walks `catalog_` linearly to find a
directory entry by CNID.  After eviction, the target directory may
not be in `catalog_`, causing `createFile()`, `createDir()`,
`rename()`, and `move()` to fail silently (returning empty path →
`kDirNFErr` / `kFnfErr`).

Replace the implementation with a CnidTable lookup:

```cpp
std::string HostVolume::resolveParentPath(uint32_t parentDirID) const
{
    if (parentDirID == kRootDirID) return rootPath_.string();
    auto *val = cnidTable_.reverse(parentDirID);
    return val ? val->hostPath : std::string{};
}
```

This is a direct O(1) lookup instead of an O(n) catalog scan, and
works even when the directory's `CatalogEntry` has been evicted.

### 4.2 — Implement findByCNID resurrection

**File:** `src/storage/host_volume.cpp`, `findByCNID()` method

Extend the current implementation with the resurrection path:

```cpp
const CatalogEntry *HostVolume::findByCNID(uint32_t cnid)
{
    // Fast path: entry is in catalog
    for (const auto &e : catalog_)
        if (e.cnid == cnid) return &e;

    // Resurrection: entry was evicted or never scanned.
    // Look up identity from cnidTable to find its parent, then
    // scan that parent directory to re-populate.
    auto *val = cnidTable_.reverse(cnid);
    if (!val) return nullptr;

    CACHE_LOG("findByCNID: resurrecting cnid=%u parent=%u name=\"%s\"",
              cnid, val->key.parentDirID, val->key.macName.c_str());

    ensureScanned(val->key.parentDirID);

    // Retry after scanning
    for (const auto &e : catalog_)
        if (e.cnid == cnid) return &e;

    return nullptr;  // file was deleted from host disk
}
```

Log the outcome when resurrection fails (file deleted from disk):

```cpp
CACHE_LOG("findByCNID: resurrection failed cnid=%u (deleted from disk)", cnid);
```

Also update `mutableFindByCNID()` with the same resurrection logic
and the same logging.

Both become non-const.  Update declarations accordingly.

### 4.3 — CNID allocation switch

**File:** `src/storage/host_volume.cpp`

Currently, CNID allocation is done via `nextCNID_++` inside
`scanDirectory()` and `createFile()` / `createDir()`.  Switch all
CNID allocation to go through `cnidTable_.resolve()`:

- In `ensureScanned()`: already uses `cnidTable_.resolve()` (Phase 3).
- In `createFile()`: replace `uint32_t cnid = nextCNID_++` with
  `uint32_t cnid = cnidTable_.resolve(parentDirID, macName, hostPath)`.
- In `createDir()`: same replacement.
- Retain `nextCNID_` temporarily for backward compatibility but stop
  incrementing it.  It can be removed after all callers are converted.

### 4.4 — Tests

**File:** `test/test_host_volume.cpp`

All existing tests should pass unchanged (the new `ensureScanned()`
calls are transparent to callers).

Add new tests:

- **`HostVolume: findByCNID resurrection after eviction`** — This
  test is a placeholder; actual eviction is Phase 5.  For now, test
  that `findByCNID()` works for entries in subdirectories that
  haven't been visited yet:
  - Mount a directory structure: root → sub → file.
  - After mount, the root's children include `sub` (with a CNID).
  - `findByCNID(sub_cnid)` should return the subdir entry.
  - Then `findByCNID(file_cnid)` should fail (file's CNID wasn't
    assigned yet) — this verifies that resurrection doesn't fabricate
    entries.

- **`HostVolume: createFile uses cnidTable`** — create a file, verify
  `cnidTable_.reverse(cnid)` returns the correct identity (if CnidTable
  is accessible in tests; otherwise verify via `findByCNID` stability).

- **`HostVolume: CNID stability across ensureScanned`** — mount,
  access a file (get its CNID), then trigger a re-scan of its parent
  (by calling `ensureScanned()` directly or via a query).  Verify the
  CNID is unchanged.

### Fence

- [ ] `nthChild()`, `childCount()`, `findByName()`, `findByPath()` call `ensureScanned()` or are transitively covered
- [ ] All five query methods are non-const
- [ ] `resolveParentPath()` uses `cnidTable_.reverse()` instead of catalog walk
- [ ] `findByCNID()` has resurrection path via `cnidTable_.reverse()`
- [ ] `findByCNID()` logs resurrection attempts and failures
- [ ] `mutableFindByCNID()` also has resurrection path
- [ ] `createFile()` and `createDir()` use `cnidTable_.resolve()` for CNIDs
- [ ] All existing tests pass
- [ ] New tests pass
- [ ] Full build clean
- [ ] Commit: `"cache: phase 3+4 — lazy scanning + query method integration"`
      (combined with Phase 3 if landed together)

---

## Phase 5 — Invalidation (invalidateDir, invalidateAll)

Add the cache eviction methods.  After this phase, the catalog is
a true cache that can be cleared and rebuilt from disk.

### 5.1 — Implement invalidateDir()

**File:** `src/storage/host_volume.h` — add declaration:

```cpp
// Evict cached children of dirID from catalog_.  Entries with
// open forks or isVirtual are retained.  The next query for this
// directory triggers re-scan from disk.
void invalidateDir(uint32_t dirID);
```

**File:** `src/storage/host_volume.cpp`

Implementation:

1. Collect the set of CNIDs with open forks:
   `openCnids = { of.cnid for each of in openForks_ }`.
2. Use `std::erase_if(catalog_, [&](const CatalogEntry &e) { ... })`
   to remove entries where:
   - `e.parentDirID == dirID`
   - AND `!e.isVirtual`
   - AND `e.cnid` not in `openCnids`
3. Clear the scanned flag for this dirID via
   `cnidTable_.clearScannedFor(dirID)`.  **This method must be
   added to `CnidTable` in Phase 1** (add it alongside
   `isScanned()` / `markScanned()` / `clearScanned()`):

Log the eviction with before/after catalog size and how many entries
were retained due to open forks:

```cpp
CACHE_LOG("invalidateDir: dir=%u evicted=%zu retained=%zu catalogSize=%zu",
          dirID, evicted, retained, catalog_.size());
```

Where `evicted` = catalog size before − after, `retained` = count of
entries matching `parentDirID == dirID` that survived (virtual or
open-fork).

### 5.2 — Implement invalidateAll()

**File:** `src/storage/host_volume.h` — add declaration:

```cpp
// Evict all non-virtual, non-open-fork entries from catalog_.
// Clears the scanned set, forcing re-scan on next access.
// Called periodically to sync with host filesystem changes.
void invalidateAll();
```

**File:** `src/storage/host_volume.cpp`

Implementation:

1. Collect open-fork CNIDs.
2. `std::erase_if(catalog_, ...)` — remove where `!isVirtual` and
   CNID not in open set.
3. `cnidTable_.clearScanned()`.

Log the eviction summary.  This fires every ~1.5 seconds once
periodic refresh is wired (Phase 7), so keep it to a single line:

```cpp
CACHE_LOG("invalidateAll: evicted=%zu retained=%zu openForks=%zu cnids=%zu",
          evicted, retained, openForks_.size(), cnidTable_.size());
```

Where `evicted` = catalog size before − after, `retained` = entries
that survived (virtual + open-fork protected).

### 5.3 — Tests

**File:** `test/test_host_volume.cpp`

- **`HostVolume: invalidateAll clears catalog`** — mount with files,
  call `invalidateAll()`, verify `childCount(rootDirID)` still
  returns the correct count (because `childCount` calls
  `ensureScanned()`, which rebuilds from disk).

- **`HostVolume: invalidateAll preserves virtual entries`** —
  mount, inject a virtual icon, call `invalidateAll()`, verify the
  virtual entry survives.

- **`HostVolume: invalidateAll preserves open fork entries`** —
  mount, create a file, open its fork, call `invalidateAll()`,
  verify `readFork()` / `writeFork()` still work on the handle.

- **`HostVolume: invalidateAll picks up new host files`** — mount,
  call `invalidateAll()`, add a file to the host directory from
  outside, then query `childCount()` — verify the new file appears.

- **`HostVolume: invalidateAll picks up deleted host files`** —
  mount, verify a file exists, call `invalidateAll()`, delete the
  file from host filesystem, then query `findByName()` — verify it
  returns null.

- **`HostVolume: invalidateDir only affects target directory`** —
  mount with root file + subdir/file, call `invalidateDir(rootDirID)`,
  verify root children are rebuilt but subdir contents are not
  evicted (subdir has a different parentDirID).

- **`HostVolume: findByCNID resurrection after invalidateAll`** —
  mount, note a file's CNID, call `invalidateAll()`,
  `findByCNID(cnid)` resurrects the entry with the same CNID.

### Fence

- [ ] `invalidateDir()` and `invalidateAll()` declared and implemented
- [ ] `invalidateDir()` logs eviction/retention counts
- [ ] `invalidateAll()` logs eviction/retention/openForks/cnids counts
- [ ] Virtual entries survive invalidation
- [ ] Open-fork entries survive invalidation
- [ ] Host filesystem additions/deletions detected after invalidation
- [ ] All existing + new tests pass
- [ ] Full build clean
- [ ] Commit: `"cache: phase 5 — invalidation (invalidateDir, invalidateAll)"`

---

## Phase 6 — rename() / move() — CnidTable Consistency

Update `rename()` and `move()` to keep the CnidTable in sync, so
that re-scans after invalidation match the new name/location to the
existing CNID.

### 6.1 — Update rename()

**File:** `src/storage/host_volume.cpp`, `rename()` method

After the existing catalog-entry update loop, add:

```cpp
// Keep CnidTable in sync so that re-scans after invalidation
// match the new name to the existing CNID.
cnidTable_.updateKey(cnid, dirID, newMacName, newHostPath);
```

Log the rename with old and new names:

```cpp
CACHE_LOG("rename: cnid=%u \"%s\" -> \"%s\" in dir=%u",
          cnid, std::string(oldMacName).c_str(),
          std::string(newMacName).c_str(), dirID);
```

For directory renames, also update descendant hostPaths in the
CnidTable.  Iterate `cnidTable_` reverse map (or iterate `catalog_`
to find descendant CNIDs, then call `cnidTable_.updateHostPath()`
for each).  Log the descendant count:

```cpp
CACHE_LOG("rename: updated %u descendant hostPaths", descendantCount);
```

### 6.2 — Update move()

**File:** `src/storage/host_volume.cpp`, `move()` method

After the existing catalog-entry update loop, add:

```cpp
// Update CnidTable: new parent + new hostPath.
cnidTable_.updateKey(cnid, dstDirID, macName, newHostPath);
```

Log the move:

```cpp
CACHE_LOG("move: cnid=%u \"%s\" dir=%u -> dir=%u",
          cnid, std::string(macName).c_str(), srcDirID, dstDirID);
```

For directory moves, update descendant hostPaths:

```cpp
if (isDir) {
    // Update all descendants' host paths in cnidTable.
    // Iterate catalog entries with matching path prefix and
    // update their cnidTable hostPath.
    uint32_t descendantCount = 0;
    for (const auto &entry : catalog_) {
        if (entry.hostPath.size() > oldHostPath.size() &&
            entry.hostPath.compare(0, oldHostPath.size(), oldHostPath) == 0 &&
            entry.hostPath[oldHostPath.size()] == '/') {
            cnidTable_.updateHostPath(
                entry.cnid,
                newHostPath + entry.hostPath.substr(oldHostPath.size()));
            ++descendantCount;
        }
    }
    CACHE_LOG("move: updated %u descendant hostPaths", descendantCount);
}
```

### 6.3 — Tests

**File:** `test/test_host_volume.cpp`

- **`HostVolume: rename preserves CNID after invalidation`** — mount,
  create a file, note its CNID, rename it, call `invalidateAll()`,
  verify `findByCNID(cnid)` returns the entry with the new name.

- **`HostVolume: move preserves CNID after invalidation`** — mount,
  create dirs A and B, create file in A, note its CNID, move to B,
  call `invalidateAll()`, verify `findByCNID(cnid)` returns the
  entry in B.

- **`HostVolume: directory rename updates descendant paths after
  invalidation`** — mount, create dir with child, rename the dir,
  call `invalidateAll()`, verify the child is found with correct
  hostPath prefix.

### Fence

- [ ] `rename()` calls `cnidTable_.updateKey()` and updates descendant paths
- [ ] `rename()` logs old/new name and descendant count
- [ ] `move()` calls `cnidTable_.updateKey()` and updates descendant paths
- [ ] `move()` logs src/dst dirs and descendant count
- [ ] CNID stability verified after rename + invalidation
- [ ] CNID stability verified after move + invalidation
- [ ] All existing + new tests pass
- [ ] Full build clean
- [ ] Commit: `"cache: phase 6 — rename/move CnidTable consistency"`

---

## Phase 7 — Periodic Refresh Hook + volumeStats Update

Wire the periodic `invalidateAll()` call into the emulator's
ExtFS path, and fix `volumeStats()` to use `cnidTable_.nextCnid()`
for `ioVNxtCNID`.

### 7.1 — Add DriveManager::invalidateAll()

**File:** `src/storage/drive_manager.h`

Add public method:

```cpp
// Invalidate all catalog caches.  Called periodically from the
// ExtFS poll path to sync with host filesystem changes.
void invalidateAll();
```

**File:** `src/storage/drive_manager.cpp`

Implementation uses the existing `forEach()` pattern:

```cpp
void DriveManager::invalidateAll()
{
    forEach([](int /*slot*/, HostVolume &vol) {
        vol.invalidateAll();
    });
}
```

### 7.2 — Add periodic invalidation in ExtFS dispatch

**File:** `src/core/extn_extfs.cpp`

The guest calls `kExtFSVersion` (0x200) periodically to check if the
ExtFS is alive.  This is the natural place to gate periodic
invalidation.

Add a file-scope counter and call `invalidateAll()` every N calls:

```cpp
// Periodic catalog invalidation.  The guest INIT polls
// kExtFSVersion roughly once per VBL (~60 Hz).  We gate
// invalidation behind a counter so it fires every ~1.5
// seconds, which is frequent enough for the Finder's own
// 2–3 second window refresh cycle.
static uint32_t s_invalidateCounter = 0;
static constexpr uint32_t kInvalidateInterval = 90;  // ~1.5 seconds

static void RegVersion(uint32_t regParam[], uint16_t &regResult)
{
    regParam[0] = (s_drives.mountedCount() > 0) ? 2u : 0u;
    regResult = 0;

    if (s_drives.mountedCount() > 0 &&
        ++s_invalidateCounter >= kInvalidateInterval) {
        s_invalidateCounter = 0;
        s_drives.invalidateAll();
    }
}
```

If testing reveals `kExtFSVersion` is not called frequently enough
(verify with a `DIAG(CACHE, ...)` trace), fall back to gating on
*any* ExtFS command in `ExtnExtFSDispatch()` — bump the counter at
the top of the dispatch function before the switch.

Add a `DIAG(CACHE, ...)` trace at the invalidation trigger site to
verify timing during manual testing:

```cpp
DIAG(CACHE, "periodic invalidate (counter=%u)\n", s_invalidateCounter);
```

This is the only `CACHE` log in `extn_extfs.cpp` — all other cache
logging lives in `host_volume.cpp` via `CACHE_LOG`.

### 7.3 — Update volumeStats()

**File:** `src/storage/host_volume.cpp`, `volumeStats()` method

Change: `ioVNxtCNID` should be `cnidTable_.nextCnid()`.

Currently the method only computes `outFiles`, `outDirs`, `outBytes`.
If `ioVNxtCNID` is set by the caller after `volumeStats()`, update
the caller (in `extn_extfs.cpp` — the `RegGetVol` function or the
HFS trap handler that reads volume info) to call a new accessor:

```cpp
uint32_t HostVolume::nextCnid() const { return cnidTable_.nextCnid(); }
```

Add this accessor to `host_volume.h`.  Update the call site in
`extn_extfs.cpp` where `ioVNxtCNID` is populated (search for
`ioVNxtCNID` or the computation `files + 16` that produces it).

### 7.4 — Tests

**File:** `test/test_host_volume.cpp`

- **`HostVolume: nextCnid reflects allocated CNIDs`** — mount with
  3 files, verify `nextCnid()` is at least 19 (16 base + 3 files).

Periodic invalidation is tested manually (Phase 8).  A unit test
could be added for `DriveManager::invalidateAll()` if the
`DriveManager` test fixture (`test_drive_manager.cpp`) supports it.

### Fence

- [ ] `DriveManager::invalidateAll()` declared and implemented
- [ ] Periodic invalidation call added to ExtFS dispatch
- [ ] Periodic invalidation site has `DIAG(CACHE, ...)` trace
- [ ] `HostVolume::nextCnid()` accessor exists
- [ ] `ioVNxtCNID` uses `nextCnid()` instead of old computation
- [ ] All existing + new tests pass
- [ ] Full build clean
- [ ] Commit: `"cache: phase 7 — periodic refresh hook + volumeStats update"`

---

## Phase 8 — End-to-End Smoke Test + Cleanup

Verify the full feature works in the emulator, clean up dead code,
and verify golden tests pass.

### 8.1 — Remove scanDirectory()

**File:** `src/storage/host_volume.h` — remove declaration of
`scanDirectory()`.

**File:** `src/storage/host_volume.cpp` — remove implementation of
`scanDirectory()`.

### 8.2 — Remove nextCNID_ member

**File:** `src/storage/host_volume.h` — remove `uint32_t nextCNID_`.
All CNID allocation now goes through `cnidTable_.resolve()`.

Verify no references remain (grep for `nextCNID_`).

### 8.2a — Update validateCatalog()

**File:** `src/storage/host_volume.cpp`

`validateCatalog()` currently calls `resolveParentPath()` for every
entry (which now uses CnidTable, so already works).  Review the
assertions — some may need relaxing because the catalog is now
partial (evicted entries are absent).  If `validateCatalog()` is
only called for debugging, add a note that it validates the
currently-cached subset, not the full volume.

### 8.3 — Golden tests

Run the full golden test suite:

```
cd test && ./verify.sh
```

All golden tests must pass.  The catalog cache should be transparent
to guest behavior — the same trap sequences should produce the same
screen hashes.

### 8.4 — Manual smoke test

Run all scenarios below with `--diag=CACHE` enabled to verify
logging output.  The log should tell a coherent story for each
scenario — if a step is missing or confusing, add or adjust the
log message before signing off.

Verify the following scenarios manually in the emulator:

1. **Mount a shared folder.**  Open it in the Finder.  Verify files
   and directories appear correctly.
   *Expected log:* `mount:` line, `ensureScanned:` for root dir,
   `resolve: new` for each child.

2. **Add a file from the host** while the Finder window is open.
   Wait 2–3 seconds.  Verify the file appears in the guest.
   *Expected log:* `periodic invalidate`, `invalidateAll:`,
   then `ensureScanned:` with `added=1`.

3. **Delete a file from the host.**  Wait 2–3 seconds.  Verify it
   disappears from the guest.
   *Expected log:* `ensureScanned: pruned cnid=...`.

4. **Open a file in a guest app** (e.g. TeachText).  While it's
   open, trigger invalidation (wait a few seconds).  Verify the
   file can still be read and saved.
   *Expected log:* `invalidateAll: ... retained=1` (the open fork
   entry survives), possibly `readFork: cnid=... evicted` or
   `writeFork: cnid=... evicted` if the entry was evicted.

5. **Rename a file in the guest.**  Verify it shows the new name.
   Trigger invalidation.  Verify the new name persists.
   *Expected log:* `rename: cnid=... "old" -> "new"`,
   `updateKey: cnid=...`.

6. **Deep directory access.**  Navigate into a nested subdirectory.
   Verify files appear (proving lazy scanning works for subdirs).
   *Expected log:* `ensureScanned:` lines for each level as the
   Finder opens them, with `added=` counts.

7. **Large directory.**  Mount a shared folder with 100+ files.
   Verify the Finder enumerates correctly (proving the scanned_ flag
   prevents hot-loop rescans).
   *Expected log:* one `ensureScanned:` line (not 100+), because
   the scanned flag short-circuits subsequent calls.

8. **CNID resurrection.**  Open a file, note its CNID in the log,
   wait for invalidation, then re-open it.  Verify the same CNID
   appears in the log.
   *Expected log:* `findByCNID: resurrecting cnid=...`.

Document results in the commit message.

### 8.5 — Comment and logging audit

Review all new code for comment completeness:

- Every new struct/class has a block comment describing its purpose
  and key properties.
- `ensureScanned()` has its multi-line algorithm comment.
- Invariants (CNID stability, scanned-flag hot-loop prevention,
  virtual entries survive eviction, open forks survive eviction)
  appear as inline comments at the enforcement points.
- The invalidation tick-gate has its rationale comment.
- **No comment references the design doc or plan doc** — all
  comments are self-contained.

Review all `CACHE_LOG` / `DIAG(CACHE, ...)` calls:

- Every log line follows the `verb: key=value` format.
- No logging inside tight iteration loops (linear scan of
  `catalog_`).  Log the *outcome* only.
- `ensureScanned` logs a summary with `added`/`updated`/`pruned`
  counts, plus individual lines for pruned entries.
- `invalidateAll` / `invalidateDir` log eviction counts.
- `findByCNID` resurrection logs the CNID and parent.
- Fork I/O evicted-entry fallback is logged.
- `rename()` / `move()` log identity changes and descendant
  path updates.
- The periodic invalidation trigger in `extn_extfs.cpp` logs
  once per fire.

### Fence

- [ ] `scanDirectory()` removed from header and implementation
- [ ] `nextCNID_` member removed
- [ ] Golden tests pass
- [ ] Manual smoke tests pass (all 8 scenarios with `--diag=CACHE`)
- [ ] Comment audit complete
- [ ] Logging audit complete — all cache events traceable
- [ ] Full build clean
- [ ] Commit: `"cache: phase 8 — cleanup + end-to-end verification"`
