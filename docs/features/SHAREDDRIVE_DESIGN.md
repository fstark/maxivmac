# Shared Drive — Design

## Overview

A System 6 INIT that mounts a host directory as a read-only HFS volume
by patching the File Manager trap dispatcher and communicating with the
emulator via the existing extension register-block mechanism.

---

## Why patch traps?

System 7.5+ provides the File System Manager (FSM, `$A0AC`) — a clean
API for registering external file systems.  BasiliskII uses this to
great effect (its `extfs.cpp` is ~3,000 lines).

**System 6 has no FSM.**  The only way to inject a virtual filesystem
is to intercept File Manager calls at the trap level, which is exactly
what pre-FSM virtual filesystems (A/UX UFS bridge, TOPS, RAM-disk
INITs) did.

---

## What we patch

### `_HFSDispatch` ($A260)

This is the single trap through which all HFS-aware File Manager calls
pass.  The trap word encodes a selector in the low byte that identifies
the operation:

| Selector | Call | Purpose |
|----------|------|---------|
| $0001 | PBOpenWD | Open working directory |
| $0002 | PBCloseWD | Close working directory |
| $0005 | PBCatMove | Move file between directories |
| $0006 | PBDirCreate | Create directory |
| $0007 | PBGetWDInfo | Get WD info |
| $0008 | PBGetFCBInfo | Get FCB info |
| $0009 | PBGetCatInfo | Get catalog info (Finder's workhorse) |
| $000A | PBSetCatInfo | Set catalog info |
| $000B | PBSetVInfo | Set volume info |
| $000C | PBLockRange | Lock byte range |
| $000D | PBUnlockRange | Unlock byte range |
| $0010 | PBCreateFileIDRef | Create file ID — return `paramErr` |
| $0011 | PBDeleteFileIDRef | Delete file ID — return `paramErr` |
| $0012 | PBResolveFileIDRef | Resolve file ID — return `paramErr` |
| $001A | PBOpenDF | Open data fork (HFS+) |
| $0020 | PBMakeFSSpec | Make FSSpec |
| $0030 | PBGetVolMountInfoSize | Volume mount info |
| $0031 | PBGetVolMountInfo | Volume mount info |
| $0060 | PBGetXCatInfo | Extended catalog info |

### Flat-file traps (pre-HFS compatibility)

Many applications (and the Finder itself on System 6) still call the
older flat-file traps.  We must patch these as well:

| Trap | Number | Purpose |
|------|--------|---------|
| `_Open` | $A000 | Open file (by name or WD) |
| `_Close` | $A001 | Close file |
| `_Read` | $A002 | Read data fork |
| `_Write` | $A003 | Write data fork |
| `_GetVolInfo` | $A007 | Get volume info |
| `_Create` | $A008 | Create file |
| `_Delete` | $A009 | Delete file |
| `_OpenRF` | $A00A | Open resource fork |
| `_Rename` | $A00B | Rename file |
| `_GetFileInfo` | $A00C | Get file info |
| `_SetFileInfo` | $A00D | Set file info |
| `_UnmountVol` | $A00E | Unmount volume |
| `_MountVol` | $A00F | Mount volume |
| `_Allocate` | $A010 | Allocate disk space |
| `_GetEOF` | $A011 | Get end-of-file |
| `_SetEOF` | $A012 | Set end-of-file |
| `_FlushVol` | $A013 | Flush volume |
| `_GetVol` | $A014 | Get default volume |
| `_SetVol` | $A015 | Set default volume |
| `_Eject` | $A017 | Eject volume |
| `_GetFPos` | $A018 | Get file position |
| `_SetFPos` | $A044 | Set file position |

### Dispatch logic

Each patch follows the same pattern:

```
MyTrapPatch:
    ; Extract ioVRefNum from the ParamBlockRec (A0)
    move.w  ioVRefNum(a0), d0
    cmp.w   #kOurVRefNum, d0
    bne.s   .passThrough        ; not ours — call original

    ; Dispatch to host via extension register block
    <pack ParamBlockRec fields into registers>
    <issue extension command>
    <unpack result into ParamBlockRec>
    move.w  d0, ioResult(a0)
    rts

.passThrough:
    move.l  oldTrapAddr, -(sp)
    rts                          ; jump to original handler
```

For `_HFSDispatch`, the selector is extracted from the trap word
and forwarded to the host alongside the parameter block fields.

---

## Drive and volume registration

### Drive queue entry

At INIT time, call `_AddDrive` to register a fake drive:

```
    ; AddDrive(drvrRefNum, driveNum, dqEl)
    move.w  #kOurDrvrRefNum, d0
    move.w  #kOurDriveNum, d1     ; pick an unused number (e.g., 8)
    lea     ourDQE, a0
    _AddDrive
```

The drive queue element (`DrvQEl`) must have:
- `dQDrive` = our drive number
- `dQRefNum` = our driver ref number
- `dQFSID` = 0 (native)
- `qType` = 1 (drive installed, no disk)
- Drive status fields indicating a non-ejectable, fixed-size drive

### Volume Control Block (VCB)

Allocate a VCB in the system heap and link it into the VCB queue
(`VCBQHdr` at low-memory global `$0356`):

| VCB Field | Value |
|-----------|-------|
| `vcbVN` | "Shared" (volume name, ≤27 chars) |
| `vcbVRefNum` | Our vRefNum (negative, e.g., -3) |
| `vcbDrvNum` | Matching our drive queue entry |
| `vcbDRefNum` | Our driver ref number |
| `vcbFsBkUp` | 0 |
| `vcbCrDate` | Current date |
| `vcbLsMod` | Current date |
| `vcbAtrb` | bit 15 set (volume locked/read-only) |
| `vcbNmFls` | File count from host |
| `vcbNmAlBlks` | Fake allocation block count |
| `vcbAlBlkSiz` | 512 (matches sector size) |
| `vcbClpSiz` | 512 |
| `vcbFreeBks` | 0 (read-only) |

### Triggering Finder discovery

After adding the drive and VCB, post a `diskInsertEvent`:

```
    move.w  #kOurDriveNum, d0
    ext.l   d0
    move.w  #7, a0               ; diskEvt
    _PostEvent
```

Finder receives the disk-inserted event, finds our VCB, and shows the
volume icon on the desktop.

---

## Host communication protocol

### Extension identity

Register a new extension `kExtnExtFS` in the extension enum
(`machine.h`).  Assign a unique 4-byte signature for `FindExtn`
discovery.

### Register block commands

The INIT communicates with the host via the register block at
`extnBlockBase + $20`, using the same mechanism as ClipSync.
Commands use the `$02xx` range to avoid collision with clipboard
commands (`$01xx`):

| Command | Name | Parameters | Returns |
|---------|------|------------|---------|
| $0200 | ExtFSVersion | — | p0 = version |
| $0201 | ExtFSGetVol | — | p0 = file count, p1 = total bytes |
| $0202 | ExtFSGetCatInfo | p0 = dirID, p1 = index | p0 = CNID, p1 = flags, p2 = size, p3 = name ptr |
| $0203 | ExtFSGetCatInfoByName | p0 = dirID, p1 = name ptr | (same as above) |
| $0204 | ExtFSOpen | p0 = CNID, p1 = fork (0=data) | p0 = host file handle |
| $0205 | ExtFSRead | p0 = handle, p1 = offset, p2 = count, p3 = buf addr | p0 = actual count |
| $0206 | ExtFSClose | p0 = handle | — |
| $0207 | ExtFSGetFileInfo | p0 = CNID | p0 = type, p1 = creator, p2 = crDate, p3 = modDate |
| $0208 | ExtFSReadDir | p0 = dirID | p0 = entry count |
| $0209 | ExtFSObjByName | p0 = parentDirID, p1 = name ptr (Mac RAM) | p0 = CNID or 0 |
| $020A | ExtFSGetWDInfo | p0 = wdRefNum | p0 = vRefNum, p1 = dirID |
| $020B | ExtFSOpenWD | p0 = vRefNum, p1 = dirID | p0 = wdRefNum |
| $020C | ExtFSCloseWD | p0 = wdRefNum | — |
| $020D | ExtFSDbgLog | (same as ClipSync $108) | — |

For file data larger than the register block can carry in one call,
the host reads/writes guest RAM directly (it has full access).  The
`ExtFSRead` command specifies a guest buffer address; the host copies
file data there.

### Name string transfer

Mac filenames are Pascal strings (length byte + up to 31 chars) in
guest RAM.  The host reads them directly from the guest address passed
in the parameter.  For returning names, the host writes a Pascal
string to a guest buffer whose address was provided by the INIT.

---

## Catalog model

### Host side (C++)

At mount time, the host scans the `shared/` directory recursively and
builds an in-memory catalog:

```cpp
struct CatalogEntry {
    uint32_t cnid;          // unique catalog node ID
    uint32_t parentDirID;   // parent directory CNID (2 = root)
    bool isDirectory;
    std::string hostPath;   // full POSIX path on host
    std::string macName;    // truncated to 31 chars, Mac OS Roman
    uint32_t dataForkSize;
    uint32_t type;          // from extension mapping
    uint32_t creator;       // from extension mapping
    uint32_t crDate;        // Mac epoch
    uint32_t modDate;       // Mac epoch
};
```

CNIDs are assigned sequentially starting at 16 (HFS reserves 1–15;
CNID 2 = root directory).

### CNID lifetime

CNIDs are **ephemeral** — assigned fresh on every mount from a
sequential counter.  They are not persisted between boots.  This is
safe because:

- All FCBs, WDs, and VCBs are wiped on reboot.  No stale reference
  can survive a power cycle.
- The Alias Manager (System 7+) resolves by path first, file ID only
  as fallback.  Even if an alias on another volume points into ours,
  it will find the file by name after a re-mount.
- We explicitly refuse `PBCreateFileIDRef` / `PBResolveFileIDRef`
  (`paramErr`), so no application can rely on persistent file IDs for
  our volume.

If the user changes the `shared/` contents between runs, new CNIDs are
assigned.  No wrong-file risk exists.

### Directory enumeration

`GetCatInfo` with `ioFDirIndex > 0` enumerates a directory by index.
The host keeps entries sorted per-directory and returns the Nth child.
Index 0 returns the directory itself.  When the index exceeds the
child count, the host returns `fnfErr` (-43).

### Filename truncation

Host filenames longer than 31 bytes (after Mac OS Roman conversion)
are truncated.  If truncation creates a duplicate within the same
directory, a numeric suffix is appended: `LongFileName~1`,
`LongFileName~2`.

---

## Working directories

Many System 6 applications use working directory reference numbers
(wdRefNums) rather than `(vRefNum, dirID)` pairs.  The INIT must
support `PBOpenWD`, `PBCloseWD`, and `PBGetWDInfo`.

The host maintains a WD table mapping `wdRefNum` → `(vRefNum, dirID)`.
WD reference numbers are negative and distinct from vRefNums.  They
are allocated on `OpenWD` and released on `CloseWD`.

Standard File dialogs (`SFGetFile`, `SFPutFile`) rely on working
directories to navigate into folders.

---

## File Control Blocks

Open files need FCB entries in the system FCB buffer (`FCBSPtr`).
The INIT allocates an FCB slot on `Open` / `OpenRF` and populates:

| FCB Field | Value |
|-----------|-------|
| `fcbFlNum` | CNID of the file |
| `fcbFlags` | read-only bit set |
| `fcbTypByt` | 0 (data fork) or $FF (resource fork) |
| `fcbEOF` | file size |
| `fcbCrPs` | 0 (current position / mark) |
| `fcbVPtr` | pointer to our VCB |

The INIT returns the FCB's `ioRefNum` (its index into the FCB buffer).
Subsequent `Read` / `GetFPos` / `SetFPos` / `GetEOF` / `Close` calls
use this refNum; the patch checks `fcbVPtr` to see if the file belongs
to our volume.

---

## Type/creator mapping

A built-in table maps file extensions to Mac type/creator codes:

| Extension | Type | Creator | Notes |
|-----------|------|---------|-------|
| `.txt` | `TEXT` | `ttxt` | TeachText |
| `.c`, `.h` | `TEXT` | `KAHL` | THINK C |
| `.p` | `TEXT` | `KAHL` | Pascal |
| `.r` | `TEXT` | `KAHL` | Rez |
| `.jpg`, `.jpeg` | `JPEG` | `ogle` | PictureViewer |
| `.gif` | `GIFf` | `ogle` | PictureViewer |
| `.bmp` | `BMPf` | `ogle` | PictureViewer |
| `.bin` | `BINA` | `hDmp` | hex dump |
| (none) | `????` | `????` | unknown |

The table is defined on the host side and is extensible.

---

## Text encoding

File *contents* are served as raw bytes — no encoding conversion in
v1.  ASCII-only text files display correctly in any Mac editor.
Files with accented characters encoded as UTF-8 will show garbled
bytes on the Mac side.

**Why not convert?**  UTF-8 → Mac OS Roman changes byte counts
(multi-byte sequences collapse to single bytes), which breaks
`SetFPos` / `GetEOF` consistency: the Mac app sees a file size that
doesn't match the host's `stat()` size, and seek offsets diverge.

**Future option:** convert the entire file at `Open` time into a
host-side Mac OS Roman buffer.  `GetEOF` returns the converted size.
All reads and seeks operate on the buffer.  This is a contained
host-side change — the INIT and the command protocol are unaffected.

Filename encoding (not content) *is* converted: the host maps UTF-8
filenames to Mac OS Roman for the catalog.  Characters without a
mapping become `?`.

---

## Catalog freshness

In v1, the catalog is built once at mount time and is immutable until
eject.  Files added to the host `shared/` directory while the volume
is mounted are invisible.

This is a **performance cache, not an architectural constraint**.  The
command protocol (`$0202` GetCatInfo, `$0209` ObjByName, etc.) is
agnostic about whether the host answers from a cached catalog or from
live `readdir()` + `stat()` calls.  Future upgrade paths:

| Approach | Tradeoff |
|----------|----------|
| Scan once at mount | Fast enumeration, stale if host changes files |
| Re-scan on eject + remount | Already in Phase 7 — user explicitly refreshes |
| Re-scan on every GetCatInfo | Always fresh, slower (real I/O per Finder window) |
| Fully live (no catalog) | Simplest host code, slowest; needs a path→CNID cache |

Switching strategies requires only host-side changes.

---

## Extensibility: write support

The architecture is ready for read-write.  The INIT already intercepts
every write trap (returning `vLckdErr`).  Upgrading to read-write
requires:

- New host commands (`$0210` Write, `$0211` Create, `$0212` Delete,
  `$0213` Rename, `$0214` DirCreate, `$0215` SetFileInfo, `$0216`
  CatMove).
- Host handlers: `open(O_CREAT)`, `write()`, `unlink()`, `mkdir()`,
  `rename()`, xattr writes.
- Catalog mutation (insert/remove entries, assign new CNIDs).
- Clear the VCB lock bit; report real free space via `statvfs()`.

No protocol redesign — additive commands only.

## Extensibility: resource forks

The FCB already distinguishes data vs. resource fork (`fcbTypByt`).
`_OpenRF` dispatches to the same `ExtFSOpen` command with a fork
flag (p1 = 1).  The host opens the alternate data source:

| Host platform | Resource fork source |
|---------------|---------------------|
| macOS | `path/..namedfork/rsrc` (native) |
| Cross-platform | AppleDouble sidecar (`._filename`) |
| Missing | Return empty fork (size 0) |

Read / Seek / Close are identical to data fork handling.  The INIT
code doesn't change — only the host's file-open logic gains a fork
switch.

---

## What we do NOT patch

- **Device Manager traps** (`_Control`, `_Status`, `_KillIO`).  Our
  fake drive number is in the drive queue but our driver ref number
  points to a minimal stub that returns `noErr` for control/status
  calls.  No actual block-level I/O goes through the Device Manager —
  all file operations are intercepted at the File Manager level.

- **Resource Manager traps** (`_OpenResFile`, `_GetResource`, etc.).
  In v1, resource forks are not served (empty fork, size 0).  When
  resource fork support is added, `_OpenRF` + `_Read` will work and
  `OpenResFile` should succeed for basic cases (the Resource Manager
  calls `_Read` on the fork, which we handle).  We never intercept
  the Resource Manager itself.

---

## Error handling

All File Manager patches return standard Mac OS error codes in
`ioResult` (D0):

| Error | Code | When |
|-------|------|------|
| `noErr` | 0 | Success |
| `fnfErr` | -43 | File not found / index past end |
| `vLckdErr` | -46 | Write to read-only volume |
| `wPrErr` | -44 | Write-protected disk |
| `paramErr` | -50 | Bad parameter |
| `nsvErr` | -35 | No such volume (if our VCB is gone) |
| `tmfoErr` | -42 | Too many files open |
