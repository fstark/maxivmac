# Shared Drive

Mount a host directory as a read-write HFS volume on the emulated
Mac's desktop.  Files in the host's `shared/` folder (relative to the
emulator's working directory) appear as a normal HFS volume — no disk
image required.

## Setup

1. Create a `shared/` folder next to the emulator binary (or wherever
   the emulator runs from).
2. Place files into it.
3. Boot the emulated Mac with the SharedDrive INIT installed in the
   System Folder.

A volume named **"Shared"** appears on the desktop.  Its contents
mirror the host `shared/` directory.

## What it does

- **Browse:** open the volume in Finder, navigate folders, view files
  with correct names, sizes, and dates.
- **Open:** double-click a file — it opens in the appropriate Mac
  application.  The data is served from the real host file.
- **Copy:** drag files between the Shared volume and disk images.
- **Edit:** save files, create new files and folders, rename, move,
  and delete — all changes are reflected on the host filesystem.
- **Resource forks:** stored in AppleDouble `._` sidecar files on
  the host, fully readable and writable from the guest.
- **TEXT encoding:** UTF-8 files on the host are transparently
  converted to/from MacRoman for the guest.

## Supported systems

- **System 6** (primary target).  No File System Manager required.
- **System 7.0–7.1** also works — the trap-patching approach is
  compatible; HFS-specific traps (`_HFSDispatch`) are supported.
- MultiFinder: supported.  The INIT patches OS traps globally.

## What it looks like

```
┌─────────────────────────────────────────────┐
│ Finder                                      │
│                                             │
│  ┌───────┐  ┌───────┐  ┌───────┐           │
│  │ HD 20 │  │Shared │  │ Trash │           │
│  └───────┘  └───────┘  └───────┘           │
│                                             │
│  ╔═══════════════════════════════════════╗  │
│  ║  Shared                    3 items    ║  │
│  ╠═══════════════════════════════════════╣  │
│  ║  README.txt        1K  TEXT           ║  │
│  ║  picture.jpg      42K  JPEG           ║  │
│  ║  Projects/                            ║  │
│  ╚═══════════════════════════════════════╝  │
└─────────────────────────────────────────────┘
```

## File metadata

- **Type/creator codes** are derived from the file extension via a
  configurable mapping table (`assets/typemap.def`).  Guest-side
  changes to Finder info (type, creator, flags) are persisted in
  AppleDouble sidecar files.  The default mappings include:

  | Extension | Type | Creator |
  |-----------|------|---------|
  | `.txt`, `.text`, `.md`, `.csv` | `TEXT` | `ttxt` |
  | `.c`, `.h`, `.p`, `.r`, `.cpp`, `.hpp`, `.s`, `.asm` | `TEXT` | `KAHL` |
  | `.htm`, `.html` | `TEXT` | `MOSS` |
  | `.jpg`, `.jpeg` | `JPEG` | `ogle` |
  | `.gif` | `GIFf` | `ogle` |
  | `.png` | `PNGf` | `ogle` |
  | `.bmp` | `BMPf` | `ogle` |
  | `.tiff` | `TIFF` | `8BIN` |
  | `.bin` | `BINA` | `hDmp` |

- **Dates** are mapped from the host file's modification time to the
  Mac epoch (seconds since 1904-01-01).
- **Resource forks** are stored in AppleDouble `._` sidecar files
  (see [AppleDouble format](#appledouble-format) below).
- **Directory Finder info** (DInfo + DXInfo, 32 bytes) is also
  persisted in sidecar files.

## Supported operations

### Read operations

| Operation | Flat trap | HFS selector |
|-----------|-----------|-------------|
| Open data fork | `_Open` | — |
| Open resource fork | `_OpenRF` | — |
| Read | `_Read` | — |
| GetFileInfo | `_GetFileInfo` | — |
| GetCatInfo | — | `PBGetCatInfo` (indexed + by-name) |
| GetVolInfo | `_GetVolInfo` | `PBHGetVInfo` |
| GetFPos / SetFPos | `_GetFPos` / `_SetFPos` | — |
| GetEOF | `_GetEOF` | — |
| GetFCBInfo | — | `PBGetFCBInfo` |
| GetVolParms | — | `PBGetVolParms` |
| GetVol | `_GetVol` | — |
| OpenWD / CloseWD / GetWDInfo | — | `PBOpenWD` / `PBCloseWD` / `PBGetWDInfo` |

### Write operations

| Operation | Flat trap | HFS selector |
|-----------|-----------|-------------|
| Write (data + resource) | `_Write` | — |
| Create file | `_Create` | — |
| Delete file / empty dir | `_Delete` | — |
| Rename | `_Rename` | — |
| SetFileInfo | `_SetFileInfo` | — |
| SetCatInfo | — | `PBSetCatInfo` |
| SetEOF | `_SetEOF` | — |
| SetVol | `_SetVol` | — |
| SetVInfo | — | `PBSetVInfo` |
| DirCreate | — | `PBDirCreate` |
| CatMove | — | `PBCatMove` |
| Allocate | `_Allocate` | — |
| FlushVol / FlushFile | `_FlushVol` / `_FlushFile` | — |
| Close | `_Close` | — |
| UnmountVol / Eject | `_UnmountVol` / `_Eject` | — |

## Limitations

- **Flat namespace limit:** filenames longer than 31 characters are
  truncated to fit HFS conventions.
- **No live refresh.**  The catalog is built at mount time.  Files
  added to the host folder after boot are not visible until the
  volume is ejected and re-mounted.
- **No persistent file IDs.**  CNIDs are ephemeral — assigned by a
  monotonic counter starting at 16, re-assigned on each mount.
  Aliases pointing into the volume resolve by name only.
- **Directory delete** requires the directory to be empty.
- **System 7.5+ with FSM** is not targeted (the trap-patching
  approach may conflict with the native File System Manager).

## AppleDouble format

Resource forks and non-default Finder info are persisted in
`._<filename>` sidecar files following the AppleDouble v2 format:

```
Header (26 bytes):
  magic:      0x00051607  (big-endian)
  version:    0x00020000  (AppleDouble v2)
  filler:     16 × 0x00
  numEntries: entry count (1 or 2)

Entry descriptor (12 bytes each):
  entryID:    2 = resource fork, 9 = Finder info
  offset:     byte offset in file
  length:     entry byte length

Data:
  [Finder info: 32 bytes if present]
  [Resource fork: variable length if present]
```

If a file's Finder info matches the extension-based default and has
no resource fork, the sidecar is omitted (or removed).

## Architecture

```
┌──────────────────────────────────────────────────┐
│  Guest Mac (68k)                                 │
│                                                  │
│  SharedDrive INIT  (init.c, THINK C)             │
│    patches 23 flat File Manager traps            │
│    patches 11 HFSDispatch selectors              │
│    adds VCB + drive queue entry via _AddDrive    │
│    posts diskInsertEvent                         │
│    intercepts File Manager calls for vRefNum     │
│    dispatches to host via register block I/O     │
└──────────────┬───────────────────────────────────┘
               │  memory-mapped register block
               │  at extnBlockBase + $20
               ▼
┌──────────────────────────────────────────────────┐
│  Emulator (C++)                                  │
│                                                  │
│  extn_extfs.cpp  — 31 command dispatcher         │
│    Phase 1 ($200–$21A): granular commands        │
│    Phase 2 ($220–$225): coarse resolve-and-act   │
│                                                  │
│  host_volume.cpp — in-memory catalog + fork I/O  │
│    recursive host directory scan                 │
│    CNID allocation, working directory table      │
│    data fork I/O via stdio                       │
│    TEXT: transparent UTF-8 ↔ MacRoman conversion │
│                                                  │
│  appledouble.cpp — sidecar file management       │
│    resource fork read/write                      │
│    Finder info (files + directories) persistence │
│    type/creator mapping from typemap.def         │
└──────────────────────────────────────────────────┘
```

### Guest-side design

The INIT patches **23 flat File Manager traps** (`_Open`, `_Close`,
`_Read`, `_Write`, `_Create`, `_Delete`, `_OpenRF`, `_Rename`,
`_GetFileInfo`, `_SetFileInfo`, `_GetVolInfo`, `_Allocate`,
`_GetEOF`, `_SetEOF`, `_FlushVol`, `_GetVol`, `_SetVol`,
`_UnmountVol`, `_Eject`, `_GetFPos`, `_SetFPos`, `_FlushFile`) and
**11 `_HFSDispatch` selectors** (`OpenWD`, `CloseWD`, `CatMove`,
`DirCreate`, `GetWDInfo`, `GetFCBInfo`, `GetCatInfo`, `SetCatInfo`,
`SetVInfo`, `GetVolParms`).

Each trap handler follows the pattern: **extract PB fields → one
register-block RPC → fill PB fields**.  The host resolves all
directory lookups (vRefNum, dirID, name) so the guest never walks the
catalog.

**Constants:**

| Name | Value | Purpose |
|------|-------|---------|
| `kOurVRefNum` | −32000 | Volume reference number |
| `kOurDriveNum` | 8 | Drive queue number |
| `kOurDrvrRefNum` | −64 | Driver reference number |
| `kRootDirID` | 2 | HFS root directory ID |
| `kAllocBlkSize` | 32 KB | Virtual allocation block size |
| `kTotalAllocBlks` | 32000 | ≈ 1 GB virtual capacity |

**Globals struct** (stored via `_NewPtr`, pointer at `$0B04`):

```c
typedef struct {
    char *regBase;           // register block base address
    Ptr   vcb;               // Volume Control Block
    Ptr   dqe;               // drive queue element
    long  volFileCount;      // cached file count
    long  volTotalBytes;     // cached total bytes
    long  savedA4;           // THINK C code-resource A4
    short rootWDRefNum;      // permanent WD for root
    short defaultWDRefNum;   // current WD (from SetVol)
    short ejected;           // nonzero after _Eject
} Globals;
```

**FCB usage:** the INIT repurposes the standard 94-byte FCB entry.
`fcbHostHandle` (offset 34, normally `fcbBTCBPtr`) stores the opaque
host file handle returned by the emulator.

### Host-side command table

31 commands, split into two phases:

| Code | Name | Purpose |
|------|------|---------|
| `$200` | Version | Check volume mounted (returns 1) |
| `$201` | GetVol | Volume file count + total bytes |
| `$202` | GetCatInfo | Catalog info by dirID + index |
| `$203` | GetCatInfoName | Catalog info by dirID + name |
| `$204` | Open | Open fork by CNID (data or resource) |
| `$205` | Read | Read from open fork |
| `$206` | Close | Close fork handle |
| `$207` | GetFileInfo | Get Finder info (type/creator/dates) |
| `$208` | ReadDir | File count in directory |
| `$209` | ObjByName | Look up child by name → CNID |
| `$20A` | GetWDInfo | Directory ID for a WD ref |
| `$20B` | OpenWD | Create WD ref for dirID |
| `$20C` | CloseWD | Close WD ref |
| `$20D` | DbgLog | Guest debug logging |
| `$20E` | GuestVars | Get/set guest globals pointer |
| `$20F` | LogTrap | Structured trap logging |
| `$210` | CreateFile | Create empty file → CNID |
| `$211` | Write | Write to open fork |
| `$212` | DeleteFile | Delete file or empty directory |
| `$213` | SetFileInfo | Update type/creator/flags |
| `$214` | Fatal | Guest fatal error |
| `$215` | CreateDir | Create subdirectory → CNID |
| `$216` | CatMove | Move file/dir to different parent |
| `$217` | Rename | Rename file or directory |
| `$218` | SetEOF | Truncate or extend fork |
| `$219` | GetDirInfo | Get directory Finder info (32 bytes) |
| `$21A` | SetDirInfo | Set directory Finder info |
| `$220` | OpenByName | Open file by name (coarse) |
| `$221` | GetCatInfoFull | Enhanced catalog info (coarse) |
| `$222` | GetFileInfoByName | File info by name + vRefNum resolution |
| `$223` | ResolveAndOpen | Resolve vRefNum/dirID, then open fork |
| `$224` | GetCatInfoResolved | Catalog info with vRefNum resolution |
| `$225` | FileOpByName | Multiplex: create/delete/rename/setFileInfo/setCatInfo |

### Host-side catalog

`HostVolume` maintains an in-memory catalog built by recursively
scanning the host `shared/` directory at mount time.

Each entry stores:

- **CNID** — monotonic counter starting at 16 (2 = root).
- **parentDirID** — parent's CNID.
- **hostPath** — absolute path on the host filesystem.
- **macName** — Mac OS Roman name, ≤ 31 bytes.
- **type / creator / finderFlags** — from extension mapping or sidecar.
- **dataForkSize / rsrcForkSize** — byte counts.
- **crDate / modDate** — Mac epoch timestamps.
- **isText** — true when type == `TEXT` (enables encoding conversion).
- **dirFinderInfo** — 32 bytes (DInfo + DXInfo) for directories.

Lookups are linear scans over a flat vector (adequate for typical
shared folder sizes).  After every mutating operation the catalog is
validated for consistency.

### Fork I/O

**Data forks** are backed by `FILE*` handles on the host.  Reads and
writes support absolute, relative, and from-EOF positioning modes.

**Resource forks** are stored in AppleDouble sidecar files.  Opening a
resource fork returns a handle with no backing `FILE*`; reads and
writes go through `appledouble::ReadResourceFork()` /
`appledouble::WriteResourceFork()`.

**TEXT files** (type == `TEXT`): data fork bytes are stored as UTF-8 on
the host but presented as MacRoman to the guest.  Encoding conversion
is transparent in both directions.

### Working directories

The host maintains a WD table (`wdRef → dirID`).  The guest encodes
WD refnums as `-(wdRef + 32000)` (e.g. wdRef 1 → −32001).  A
permanent root WD and a default WD (updated by `_SetVol`) are created
at init time.

### Error mapping

The host sets `regResult` to the positive magnitude of the Mac OS
error code.  The guest negates: `err = -(short)regResult`.  Standard
errors include fnfErr (−43), dupFNErr (−48), paramErr (−50),
fBsyErr (−47), dirNFErr (−120), ioErr (−36).
