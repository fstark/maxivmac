# Host File Sharing (virtual volume on the desktop)

Mount a host directory as a volume visible to the Finder, without
requiring a real disk image.  Targeting **System 6** as the primary
platform.

## The problem on System 6

System 7.5+ has the **File System Manager (FSM)** — a clean API for
installing external file systems (`InstallFS` / `_FSMDispatch` /
`$A0AC`).  BasiliskII's `extfs.cpp` (~3,000 lines, GPLv2) is a
complete working implementation of this approach.  It registers an
`FSDRec`, installs an HFS component, calls `PBVolumeMount`, and
translates every File Manager call to POSIX operations on a host
directory.

**FSM does not exist on System 6.**  There is no supported API for
registering a foreign filesystem.

## Approach: Patch the File Manager dispatch table

On System 6, every File Manager call goes through the `_HFSDispatch`
A-trap (`$A260`) or the older flat-file traps (`_Open` / `_Read` /
`_GetFileInfo` / etc.).  The classic technique for an INIT-based
virtual filesystem is:

1. Patch `_HFSDispatch` (and/or the individual flat-file traps).
2. Inspect each incoming `ParamBlockRec`.  If `ioVRefNum` matches your
   fake drive number, handle it; otherwise call through to the
   original trap.
3. Fake a drive in the drive queue (`AddDrive`) and post a
   `diskInsertEvent` so Finder mounts it.
4. Maintain your own VCB (Volume Control Block) and FCB (File Control
   Block) entries in the system's linked lists.

This is what every pre-FSM virtual filesystem did — including A/UX's
UFS bridge, various RAM-disk INITs, and network file systems like
TOPS.

The 68k INIT patches the traps and dispatches to the emulator via
maxivmac's existing extension RPC mechanism (see `machine.h`
`ExtnDat_*`, `machine.cpp` `Extn_Access()`).  A new extension
(`kExtnExtFS`) serves as the communication channel; the 68k INIT is
small (trap patch + dispatch to extension), and the heavy lifting
happens on the host side in C++ doing POSIX I/O.

**Difficulty:** you must handle ~30 File Manager selectors (`GetCatInfo`,
`Open`, `Read`, `Write`, `Close`, `Create`, `Delete`, `Rename`,
`CatMove`, `GetFileInfo`, `SetFileInfo`, `GetVol`, `SetVol`,
`OpenWD`, `CloseWD`, `GetWDInfo`, `FlushVol`, `GetVolInfo`,
`SetVolInfo`, `GetFPos`, `SetFPos`, `GetEOF`, `SetEOF`, `Allocate`,
`OpenRF`, etc.), plus the Finder calls `GetCatInfo` with indexed
directory enumeration which requires careful bookkeeping.

BasiliskII's `extfs.cpp` implements all of these (for the FSM code
path), so the *translation logic* is directly reusable — only the
*registration mechanism* is System 6-specific.

## Incremental plan

### Milestone 1 — Extension round-trip

Prove the 68k ↔ host communication path works.

- Register `kExtnExtFS` in the extension enum (`machine.h`).
- Add `ExtnExtFS_Access()` handler on the host side — for now it just
  logs calls to the debug log.
- Add a 68k ROM stub (following the `sony_driver` pattern in
  `rom.cpp`) that triggers the extension during boot.  The stub
  writes the parameter block to the extension block base address with
  command `kCmndExtFSInit`.
- **Verify:** debug log shows "ExtFS: init" at startup.

This milestone touches no File Manager state.  It only validates that
68k code can call host C++ code and get a result back.

Note: all existing extensions (Sony, clipboard, video) are ROM-patched.
There is no INIT-loading mechanism.  The ExtFS stub follows the same
ROM-patch approach.

### Milestone 2 — Empty volume on the desktop

A volume icon labelled "Host" appears on the Finder desktop.
Double-clicking it opens an empty window.

Steps:

1. **Patch `_HFSDispatch`.**  The 68k stub, after calling
   `kCmndExtFSInit`, patches the `_HFSDispatch` A-trap (`$A260`) via
   `GetTrapAddress` / `SetTrapAddress`.  The patch inspects each
   incoming `ParamBlockRec`; if `ioVRefNum` matches our volume, it
   dispatches to the extension.  Otherwise it calls through to the
   original handler.

2. **Populate a VCB.**  Allocate a block in system heap
   (`_NewPtr ,SYS`), fill in the volume name (`vcbVN` = "Host"),
   `vcbVRefNum`, `vcbDrvNum`, `vcbFsBkUp`, `vcbCrDate`, etc.  Link
   it into the system VCB queue (QHdr at low-memory global `$356`).

3. **Add a drive queue entry.**  Call `AddDrive` with a matching drive
   number.

4. **Handle `GetVolInfo`** (selector `$0007` of `_HFSDispatch`).
   Return volume name, creation date, free space = 0, total size =
   some plausible value.

5. **Handle `GetCatInfo`** for the root directory *only*: return
   `ioFlAttrib` with the directory bit set, `ioDrNmFls` = 0 (empty),
   directory ID = 2 (the HFS root directory ID).

- **Verify:** Finder shows the "Host" icon.  Double-click opens an
  empty window.  No crashes.

### Milestone 3 — A single hardcoded file

"README" (type `TEXT`, creator `KAHL`) appears in the volume, and
opens in TeachText displaying "Hello, World".

This is the smallest increment that exercises the file-open-read-close
path end-to-end.

1. **`GetCatInfo` indexed enumeration.**  When Finder calls
   `GetCatInfo` with `ioFDirIndex` = 1 on the root directory, return
   one file entry: name "README", type `TEXT`, creator `KAHL`,
   data fork logical length = 13.  Index = 2 returns `fnfErr`.

2. **`GetFileInfo`.**  Return the same metadata when called by name.

3. **`Open` (data fork).**  Allocate an FCB entry in the system FCB
   buffer.  Set `fcbFlNum` (CNID), `fcbFlags` (read-only), return
   `ioRefNum`.

4. **`Read`.**  The host side serves `"Hello, World\r"` (Mac line
   ending) from a static buffer.  Copy bytes into the Mac memory
   pointed to by `ioBuffer`, update `ioActCount` and the FCB's
   `fcbCrPs` (mark/position).

5. **`GetFPos` / `SetFPos` / `GetEOF`.**  Trivial — read/write the
   FCB mark and return the known length.

6. **`Close`.**  Release the FCB.

- **Verify:** double-click "Host" volume → window shows "README" →
  double-click "README" → TeachText displays "Hello, World".

### Milestone 4 — Host directory, flat, data-fork only, read-only

Replace the hardcoded file with the real contents of a host directory.
No subdirectories yet — all files appear in the root.

1. **Command-line option.**  `--host-dir=/path/to/folder`.
   The host side scans the directory at startup and builds an
   in-memory catalog: filename, host path, size, modification date.
   Assign each file a unique CNID (starting at 16, as HFS reserves
   2–15).

2. **`GetCatInfo` indexed.**  Walk the catalog by index.  Return file
   count in `ioDrNmFls` when querying the root directory.

3. **`GetFileInfo` by name.**  Look up the catalog by filename.
   Filenames longer than 31 characters get truncated (with
   disambiguation if needed).

4. **`Open` / `Read`.**  On `Open`, the host opens the real file
   (POSIX `open()`).  On `Read`, the host reads from the file at the
   current FCB mark/position and copies data into Mac memory.
   Use pbufs for the transfer if the amount exceeds what fits in the
   extension parameter block, or do chunked reads.

5. **Dates.**  Map POSIX `st_mtime` to the Mac epoch (seconds since
   1904-01-01).  Use it for `ioFlMdDat` and `ioFlCrDat`.

6. **Volume size.**  `GetVolInfo` returns the sum of file sizes as the
   volume's total size, and free space = 0 (read-only).

- **Verify:** `--host-dir=~/some-folder` → volume mounts → Finder
  shows real filenames and sizes → files open in the appropriate
  application.

### Milestone 5 — Drag/drop and eject

1. **Drag/drop.**  Dropping a folder onto the emulator window mounts
   it as a new virtual volume.  Each dropped folder gets its own
   VCB, drive number, and volume name (derived from the folder name).
   Reuse the same extension; the host side identifies volumes by
   drive number.

2. **Multiple volumes.**  The catalog, FCB tracking, and trap-patch
   dispatch all need to handle multiple volumes.  The `ioVRefNum` or
   `vcbDrvNum` in the parameter block selects which one.

3. **Eject / unmount.**  When the user drags the volume to the Trash
   (or Cmd-E), the Finder calls `Eject` + `UnmountVol`.  The host
   side closes open host files, frees the catalog, removes the VCB
   and drive queue entry.

4. **Re-mount.**  Drag the same folder again — mount a fresh volume.

- **Verify:** drag three folders → three volumes on desktop → eject
  one → two remain → drag it again → it's back.

### Milestone 6 — Finder metadata, resource forks, text encoding

1. **Type / creator codes.**  Two sources, in priority order:
   - macOS extended attributes (`com.apple.FinderInfo`, 32 bytes) —
     if the host is macOS and the xattr exists, use it.
   - Extension mapping table (`.txt` → `TEXT/ttxt`,
     `.c` → `TEXT/KAHL`, `.jpg` → `JPEG/ogle`, etc.).
   Provide a built-in default table; allow user overrides later.

2. **Finder flags.**  Default to 0.  If `com.apple.FinderInfo` xattr
   is present, extract the flags from bytes 8–9.

3. **Resource forks.**  Support macOS native resource forks
   (`file/..namedfork/rsrc`) where available; fall back to
   AppleDouble sidecar files (`._filename`).  Implement `OpenRF`,
   `Read`, `GetEOF`, `Close` for the resource fork — same path as
   data fork but reading from the alternate source.

4. **Text encoding transposition.**  For files with type `TEXT`, offer
   an option to convert between UTF-8 (host) and Mac OS Roman (Mac)
   on the fly during `Read`.  This should be opt-in (e.g., enabled by
   default for `TEXT` files, disableable via a flag) since binary
   files must never be converted.  Characters with no Mac OS Roman
   equivalent map to `?` (0x3F).

- **Verify:** files show correct icons in Finder.  ResEdit can open
  resource forks.  A UTF-8 text file with accented characters
  displays correctly in TeachText.

### Milestone 7 — Folders

Up to this point, all files are flattened into the root.  Now add
real subdirectory support.

1. **Directory entries in the catalog.**  Each host subdirectory
   gets its own CNID (directory ID) and parent directory ID.
   The root directory is always CNID 2.

2. **`GetCatInfo` with `ioDirID`.**  Enumerate any directory, not
   just the root.  Return `ioDrNmFls` (file + subfolder count) and
   `ioDrParID` (parent).

3. **Working directories.**  `OpenWD` creates a WD reference that
   maps `(vRefNum, dirID)` → `wdRefNum`.  `CloseWD` releases it.
   `GetWDInfo` returns the mapping.  Standard File (`SFGetFile`,
   `SFPutFile`) relies on these.

4. **`CatMove`.**  Move files between directories (read-only volumes
   can skip this; include it if write support is already landed).

- **Verify:** navigate into subfolders in Finder windows.  Standard
  File dialogs browse the volume.  Deeply nested paths work.

### Milestone 8 — Write support

The volume becomes read-write.

1. **`Create`.**  Create new files: host side does `open(O_CREAT)`.
   Assign a new CNID.  Handle `CreateResFile` for resource fork.

2. **`Write`.**  Write data fork bytes.  Host side writes to the real
   file.  Update `fcbCrPs` (mark).

3. **`SetEOF`.**  Truncate or extend.  Host side does `ftruncate()`.

4. **`Allocate`.**  Pre-allocate space — can be a no-op on the host
   (POSIX doesn't require pre-allocation for correctness).

5. **`Delete`.**  Remove file.  Host side does `unlink()` + remove
   AppleDouble sidecar if present.

6. **`DirCreate`.**  Create directory.  Host side does `mkdir()`.

7. **`Rename`.**  Rename file or directory.  Host side does
   `rename()`.

8. **`SetFileInfo` / `SetCatInfo`.**  Update type/creator, Finder
   flags, modification date.  Write back to xattrs or AppleDouble.

9. **`FlushVol`.**  Sync.  Host side does `fsync()` on open files.

10. **Volume free space.**  `GetVolInfo` returns actual free space on
    the host filesystem (via `statvfs()`).

- **Verify:** save a new document from a Mac application → file
  appears on the host.  Delete a file in Finder → file disappears
  from host.  Copy a file to the volume → readable on host.

## Reference material

- **BasiliskII `extfs.cpp`** — the definitive reference implementation
  (GPLv2, Christian Bauer).  Files:
  - `BasiliskII/src/extfs.cpp` — main dispatch + all FS calls
  - `BasiliskII/src/include/extfs_defs.h` — struct offsets, selector
    constants
  - `BasiliskII/src/Unix/extfs_platform_unix.cpp` — resource fork
    handling via AppleDouble
  - `BasiliskII/src/extfs_macosx.mm` — macOS-specific resource fork
    and Finder info via `getattrlist()`

- **Inside Macintosh: Files** — chapters on the File Manager, HFS, VCB,
  FCB, WDCBs, the drive queue, and `ParamBlockRec` layout.

- **Inside Macintosh: Devices** — the drive queue and `AddDrive` trap.

- **Macintosh Technical Note #102** — "HFS Elucidations" (drive queue,
  VCB internals).

- **Apple FSM 1.2 SDK** — "Guide to the File System Manager".  While
  FSM itself isn't usable on System 6, the document describes the
  HFS component interface, the utility routines (`UTAllocateVCB`,
  `UTDetermineVol`, etc.), and the full set of File Manager selectors
  that an external FS must handle.
