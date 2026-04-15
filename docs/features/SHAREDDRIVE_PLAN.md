# Shared Drive — Implementation Plan

Each phase ends with a concrete verification step.  Do not advance to
the next phase until the current one passes.  Commit after each phase.

## Implementation Status

| Phase | Status | Notes |
|-------|--------|-------|
| 1 | **DONE** | Extension registered, round-trip works (commit 55b0a15) |
| 2 | **DONE** | VCB, DQE, trap patches, disk-insert event all in init.c |
| 3 | **DONE** | Handlers for Open/Read/Close/GetEOF/SetFPos/GetFPos in init.c |
| 4 | **DONE** | Host dir scanner, type/creator mapping, chunked reads in extn_extfs.cpp |
| 5 | **DONE** | Recursive scan, WD stubs return paramErr (minimal WD support) |
| 6 | **DONE** | Write/Create/Delete/Rename/SetFileInfo patches return vLckdErr |
| 7 | Not started | Eject/re-mount support |
| 8 | Not started | Edge cases, polish |

### Files created/modified

- `src/core/extn_extfs.h` — declares ExtnExtFSDispatch()
- `src/core/extn_extfs.cpp` — host-side: all commands $0200-$020D
- `src/core/machine.h` — added kExtnExtFS enum
- `src/core/machine.cpp` — FindExtn registration, $02xx routing
- `CMakeLists.txt` — added extn_extfs.cpp
- `macsrc/shareddrive/init.c` — guest INIT (THINK C code resource)
- `shared/` — test directory with sample files

### Known limitations (v1)

- File open is root-only (no ioDirID resolution in DoOpen)
- WD open/close/getinfo return paramErr (not fully implemented)
- No catalog refresh while mounted
- No eject/re-mount
- Text encoding: raw bytes only (no conversion)
- Name disambiguation (collisions from truncation) not implemented

---

## Phase 1 — Extension registration and round-trip

**Goal:** prove that 68k guest code can call a new ExtFS extension on
the host and get a response back.

### Host side

1. **Add extension enum.**  In `machine.h`, add `kExtnExtFS` to the
   extension ID enum (next available slot, after
   `kExtnHostTextClipExchange`).  Add a 4-byte signature constant
   `kExtFSExtension`.

2. **Create `extn_extfs.cpp` / `extn_extfs.h`.**  Implement
   `ExtnExtFS_Access()` following the pattern of `extn_clip.cpp`.
   Handle one command: `$0200` (ExtFSVersion) — return version 1 in
   p0.  Log `"ExtFS: version query"` to `dbglog`.

3. **Register in machine.cpp.**  In `Extn_Access()`, add a case for
   `kExtnExtFS` that calls `ExtnExtFS_Access()`.  Register it in the
   `FindExtn` signature table.

4. **Register in the register-block dispatcher.**  The clipboard
   extension handles commands `$01xx` via the register block at
   `extnBlockBase + $20`.  Route commands `$02xx` to
   `ExtnExtFS_Access()` using the same mechanism.

5. **Build and verify the host compiles.**

### Guest side (THINK C INIT)

6. **Create `macsrc/shareddrive/init.c`.**  Minimal INIT: discover
   the register base (reuse `find_reg_base()` from ClipSync), issue
   command `$0200`, read p0.  Log the version via `$020D` (DbgLog).
   If version < 1, bail out.

7. **Build the INIT** with THINK C on the emulated Mac (or
   cross-compile if tooling exists).

### Verify

- Boot the emulated Mac with the INIT installed.
- `dbglog.txt` shows `"ExtFS: version query"` and the INIT's
  `"SharedDrive INIT: version=1"` log line.
- No crash.

---

## Phase 2 — Empty volume on the desktop

**Goal:** a volume icon labelled "Shared" appears on the Finder
desktop.  Double-clicking opens an empty window.

### Guest INIT (`init.c`)

1. **Patch `_HFSDispatch` ($A260).**  After the version check:
   - `GetTrapAddress(_HFSDispatch)` → save as `oldHFSDispatch`.
   - Install our patch via `SetTrapAddress`.
   - The patch extracts `ioVRefNum` from the parameter block (A0).
     If it matches our volume, dispatch to the extension.  Otherwise
     call through to `oldHFSDispatch`.

2. **Allocate a VCB.**  `NewPtrSysClear(sizeof(VCB))`.  Fill in:
   - `vcbVN` = `"\pShared"` (Pascal string)
   - `vcbVRefNum` = our chosen vRefNum (ask host, or hardcode e.g. -32000)
   - `vcbDrvNum` = our drive number (e.g. 8)
   - `vcbAtrb` = bit 15 set (locked)
   - `vcbNmFls` = 0
   - `vcbCrDate`, `vcbLsMod` = current time
   - `vcbNmAlBlks` = some plausible value (e.g. 1024)
   - `vcbAlBlkSiz` = 512
   - `vcbFreeBks` = 0
   Link into VCB queue: `Enqueue((QElemPtr)vcb, (QHdrPtr)0x0356)`.

3. **Add drive queue entry.**  Build a `DrvQEl`, call
   `AddDrive(ourDrvrRefNum, ourDriveNum, &dqe)`.

4. **Post disk-inserted event.**
   ```
   PostEvent(diskEvt, ourDriveNum);
   ```

### Host side (`extn_extfs.cpp`)

5. **Handle `$0202` (ExtFSGetCatInfo).**  For now: when dirID = 2
   (root) and index = 0, return the root directory entry (flags =
   directory bit set, `ioDrNmFls` = 0).  For any index > 0, return
   `fnfErr`.

6. **Handle `$0201` (ExtFSGetVol).**  Return file count = 0, total
   bytes = 0.

### Guest INIT — trap patch bodies

7. **`_HFSDispatch` patch: `GetCatInfo` (selector $0009).**  Extract
   `ioVRefNum`, `ioDirID`, `ioFDirIndex`, `ioNamePtr` from the
   `CInfoPBRec` at A0.  Pack into registers, issue `$0202`.  Unpack
   the host response into the param block fields.

8. **`GetVolInfo` flat trap patch.**  Intercept `_GetVolInfo` ($A007)
   for our vRefNum.  Return volume name, dates, free space = 0.

### Verify

- Boot with the INIT.  "Shared" icon appears on the desktop.
- Double-click opens an empty Finder window titled "Shared".
- No crash.  Other volumes still work.

---

## Phase 3 — One hardcoded file

**Goal:** a file named "README" appears in the volume.  Opening it in
TeachText displays "Hello, World".

### Host side

1. **Hardcode one catalog entry.**  In `extn_extfs.cpp`, create a
   static `CatalogEntry`: CNID = 16, parentDirID = 2, name =
   "README", type = `TEXT`, creator = `ttxt`, size = 13.

2. **`$0202` GetCatInfo:** index 1 in root → return the README entry.
   Index 2 → `fnfErr`.

3. **`$0204` ExtFSOpen:** if CNID = 16, return a handle (e.g. 1).

4. **`$0205` ExtFSRead:** serve the bytes `"Hello, World\r"` (Mac
   line ending).  Write them into guest RAM at the buffer address.
   Return actual count in p0.

5. **`$0207` ExtFSGetFileInfo:** return type/creator/dates for
   CNID 16.

6. **`$0206` ExtFSClose:** release the handle.

### Guest INIT — additional trap patches

7. **Patch `_Open` ($A000).**  For our vRefNum: extract `ioNamePtr`,
   send `$0204`.  Allocate an FCB slot, populate it, return
   `ioRefNum`.

8. **Patch `_Read` ($A002).**  Check `fcbVPtr` on the FCB for this
   refNum.  If ours: send `$0205` with handle, current position,
   requested count, buffer address.  Update `fcbCrPs`.

9. **Patch `_GetFPos` ($A018) / `_SetFPos` ($A044) / `_GetEOF`
   ($A011).**  Trivial — read/write the FCB mark and return the
   known length.

10. **Patch `_Close` ($A001).**  Send `$0206`, release the FCB slot.

11. **Patch `_GetFileInfo` ($A00C).**  Send `$0207`, unpack into
    the param block.

### Verify

- Open "Shared" volume → "README" appears with TeachText icon.
- Double-click → TeachText opens, shows "Hello, World".
- Close the file.  No crash.  Other apps unaffected.

---

## Phase 4 — Real host directory, flat, read-only

**Goal:** the volume shows the actual contents of the host `shared/`
folder.  Files open correctly.

### Host side

1. **Directory scanner.**  On extension init (`$0200`), scan
   `shared/` relative to the working directory.  Build the catalog
   vector.  Assign CNIDs starting at 16 (ephemeral — not persisted
   between boots; see SHAREDRIVE_DESIGN.md §CNID lifetime).  Flatten
   (no subdirectories yet — skip them or list them as empty entries).

2. **Filename conversion.**  Convert UTF-8 filenames to Mac OS Roman.
   Truncate to 31 characters.  Disambiguate collisions with `~N`
   suffix.  Note: only *filenames* are converted — file *contents*
   are served as raw bytes (no encoding conversion in v1).

3. **Type/creator mapping.**  Implement the extension → type/creator
   table from the design doc.  Apply it to each entry.

4. **Date mapping.**  Convert POSIX `st_mtime` to seconds since
   1904-01-01 (add `2082844800`).

5. **File serving.**  On `$0204` (Open), `open()` the host file.
   On `$0205` (Read), `pread()` at the requested offset, write into
   guest RAM.  On `$0206` (Close), `close()` the host fd.

6. **`$0201` GetVol update:** return real file count and total size.

7. **`$0202` GetCatInfo update:** enumerate from the catalog.

### Guest INIT

8. No INIT changes needed — the trap patches from Phase 3 are generic
   enough.  Verify that indexed enumeration works for arbitrary counts
   (Finder calls `GetCatInfo` in a loop until `fnfErr`).

### Verify

- Place 5–10 files of various types in `shared/`.
- Boot → "Shared" volume shows all files with correct names, sizes,
  icons.
- Open a .txt file in TeachText → content displays correctly.
- Open a .jpg in an image viewer (if available) → displays correctly.

---

## Phase 5 — Subdirectory support

**Goal:** the volume mirrors the full directory tree from the host.

### Host side

1. **Recursive scan.**  The directory scanner now recurses into
   subdirectories.  Each directory gets its own CNID and
   `parentDirID`.  Root = CNID 2.

2. **`$0202` GetCatInfo update.**  When `ioDirID` ≠ 2 and index > 0,
   enumerate that directory's children.  When index = 0, return the
   directory's own info (parent, child count, dates).

3. **`$0208` ExtFSReadDir.**  Return child count for a given dirID.

### Guest INIT

4. **Working directory support.**  Implement patches for:
   - `_HFSDispatch` selector $0001: `PBOpenWD` → send `$020B`, host
     allocates a WD mapping, return `wdRefNum`.
   - `_HFSDispatch` selector $0002: `PBCloseWD` → send `$020C`.
   - `_HFSDispatch` selector $0007: `PBGetWDInfo` → send `$020A`.
   Standard File dialogs need these to navigate into subfolders.

5. **vRefNum vs. wdRefNum resolution.**  In every trap patch, if
   `ioVRefNum` is a wdRefNum (negative, matching one of our WDs),
   resolve it to `(vRefNum, dirID)` before checking if it's our
   volume.

### Verify

- Create `shared/Projects/hello.c` and `shared/Docs/notes.txt`.
- Boot → "Shared" shows "Projects" and "Docs" folders.
- Double-click "Projects" → window shows "hello.c".
- Use Standard File (Open dialog in an app) to navigate into
  subdirectories.

---

## Phase 6 — Write protection feedback

**Goal:** graceful error messages when the user tries to write.

### Guest INIT

1. **Patch `_Write` ($A003).**  Return `vLckdErr` (-46).
2. **Patch `_Create` ($A008).**  Return `vLckdErr`.
3. **Patch `_Delete` ($A009).**  Return `vLckdErr`.
4. **Patch `_Rename` ($A00B).**  Return `vLckdErr`.
5. **Patch `_SetFileInfo` ($A00D).**  Return `vLckdErr`.
6. **Patch `_HFSDispatch` selectors for `SetCatInfo`, `CatMove`,
   `DirCreate`.**  Return `vLckdErr`.

### Verify

- Try to drag a file onto the Shared volume → Finder shows "The disk
  is locked" (or similar).
- Try to rename a file → error.
- Try Save As into the volume → application shows a write error.

---

## Phase 7 — Eject and re-mount

**Goal:** the user can eject the Shared volume and re-mount it.

### Guest INIT

1. **Patch `_Eject` ($A017).**  For our drive: close all open files
   (send `$0206` for each), unlink VCB from the queue, remove drive
   queue entry.  Notify host via a new command `$020E` (ExtFSUnmount)
   so it releases resources.

2. **Patch `_UnmountVol` ($A00E).**  Same as Eject for our volume.

3. **Re-mount trigger.**  Define a host-side mechanism:
   - Option A: a keyboard shortcut (e.g., Cmd-Shift-S) triggers a
     `diskInsertEvent` for our drive.
   - Option B: the INIT periodically checks (via KV or a command)
     whether the host wants to mount.

### Host side

4. **`$020E` ExtFSUnmount.**  Close all open host file descriptors.
   Mark the catalog as invalid.

5. **Re-scan on re-mount.**  When re-mount is triggered, re-scan the
   `shared/` directory and rebuild the catalog (fresh CNIDs).  This
   picks up any new files added while the volume was ejected.  The
   catalog is a performance cache, not an architectural pillar — the
   command protocol is agnostic about whether answers come from a
   cached catalog or live `readdir()` calls.

### Verify

- Drag "Shared" to Trash → volume disappears.
- Trigger re-mount → "Shared" reappears with any new files.
- Files that were open during eject are properly closed on host.

---

## Phase 8 — Polish and edge cases

1. **Empty `shared/` directory.**  Must not crash — show an empty
   volume.
2. **Missing `shared/` directory.**  INIT queries host; host returns
   version 0 or error.  INIT does not mount.
3. **Filenames with special characters.**  Mac OS Roman conversion for
   filenames (accented characters without a mapping become `?`).
   File *contents* are raw bytes — no encoding conversion in v1.
4. **Large files.**  Files > 64KB: verify chunked reads work.
   Files > 1MB: verify no integer overflow in FCB/catalog sizes.
5. **Many files.**  Test with 100+ files to verify indexed enumeration
   handles large directories.
6. **Deeply nested paths.**  5+ levels deep.
7. **Long host filenames.**  Verify truncation + disambiguation works
   without collisions.
8. **Concurrent file access.**  Two Mac apps reading the same file
   simultaneously (two FCBs, two host fds).
9. **Standard File dialogs.**  `SFGetFile` and `SFPutFile` navigate
   the volume correctly.
10. **PBCreateFileIDRef / PBResolveFileIDRef.**  Confirm `paramErr`
    is returned (persistent file IDs not supported).
11. **Changed host directory between boots.**  Eject, modify files on
    host, re-mount — verify new catalog with fresh CNIDs, no stale
    references.

### Verify

- Run through each edge case.  No crashes, no hangs, correct errors.

---

## Phase summary

| Phase | What | Host changes | Guest changes |
|-------|------|-------------|---------------|
| 1 | Extension round-trip | `extn_extfs.cpp`, machine.h | `init.c` skeleton |
| 2 | Empty volume on desktop | GetCatInfo root, GetVol | trap patches, VCB, DQE |
| 3 | One hardcoded file | static catalog, Open/Read/Close | Open/Read/Close/GetEOF patches |
| 4 | Real host directory | dir scanner, file serving | (none) |
| 5 | Subdirectories | recursive scan, WD table | WD patches, vRefNum resolution |
| 6 | Write protection | (none) | write-trap patches return errors |
| 7 | Eject / re-mount | unmount handler, re-scan | Eject/UnmountVol patches |
| 8 | Polish | edge-case hardening | edge-case hardening |

---

## Future phases (not in v1)

These are explicitly designed-for but deferred:

- **Text encoding conversion.**  Convert TEXT file contents at `Open`
  time into a host-side Mac OS Roman buffer.  `GetEOF` returns the
  converted size.  Contained host-side change — INIT and protocol
  unchanged.
- **Write support.**  Additive: new commands (`$0210`–`$0216`), host
  handlers (`write`, `unlink`, `mkdir`), catalog mutation, clear VCB
  lock bit.  No protocol redesign.
- **Resource forks.**  Fork flag on `ExtFSOpen` (p1 = 1).  Host opens
  `path/..namedfork/rsrc` (macOS) or AppleDouble sidecar.  INIT code
  unchanged.
- **Live catalog refresh.**  Replace cached scan with live `readdir()`
  per GetCatInfo, or periodic re-scan.  Host-side only.
