# Shared Drive

Mount a host directory as a read-only volume on the emulated Mac's
desktop.  Files in the host's `shared/` folder (relative to the
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
- **Copy:** drag files from the Shared volume to a real disk image.
- **Read-only:** you cannot save, delete, or create files on the
  Shared volume.  Finder shows it as a locked volume.

## Supported systems

- **System 6** (primary target).  No File System Manager required.
- **System 7.0–7.1** should also work (the trap-patching approach is
  compatible), but is not the primary test target.
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
│  ╔═══════════════════════════════════╗      │
│  ║  Shared                    3 items║      │
│  ╠═══════════════════════════════════╣      │
│  ║  README.txt        1K  TEXT      ║      │
│  ║  picture.jpg      42K  JPEG      ║      │
│  ║  Projects/                        ║      │
│  ╚═══════════════════════════════════╝      │
└─────────────────────────────────────────────┘
```

## File metadata

- **Type/creator codes** are derived from the file extension
  (`.txt` → `TEXT/ttxt`, `.c` → `TEXT/KAHL`, `.jpg` → `JPEG/ogle`,
  etc.) via a built-in mapping table.
- **Dates** are mapped from the host file's modification time to the
  Mac epoch (seconds since 1904-01-01).
- **Resource forks** are not supported in v1.  Only data forks are
  served.

## Limitations

- **Read-only.**  Write support is a future milestone (the
  architecture is designed for it — additive commands only).
- **Flat namespace limit:** filenames longer than 31 characters are
  truncated.
- **No live refresh.**  The catalog is built at mount time.  Files
  added to the host folder after boot are not visible until the
  volume is ejected and re-mounted.  (This is a performance cache,
  not an architectural constraint — can be lifted host-side later.)
- **No encoding conversion.**  File contents are served as raw bytes.
  ASCII text works perfectly; UTF-8 files with accented characters
  will display garbled on the Mac.  (Future: convert at open time.)
- **No resource forks** in v1.  Classic Mac apps that require resource
  forks cannot run directly from the Shared volume.  (Future: serve
  from macOS native forks or AppleDouble sidecars.)
- **No persistent file IDs.**  CNIDs are ephemeral — re-assigned on
  each mount.  Aliases pointing into the volume resolve by name.
- **System 6 only** (tested).  System 7.5+ with FSM is not targeted.

## Architecture (overview)

```
┌──────────────────────────────────────────────────┐
│  Guest Mac                                       │
│                                                  │
│  SharedDrive INIT                                │
│    patches _HFSDispatch ($A260)                  │
│    adds drive queue entry via _AddDrive           │
│    posts diskInsertEvent                         │
│    intercepts File Manager calls for our vRefNum │
│    dispatches to host via register block I/O     │
└──────────────┬───────────────────────────────────┘
               │  memory-mapped registers
               │  at extnBlockBase + $20
               ▼
┌──────────────────────────────────────────────────┐
│  Emulator (C++)                                  │
│                                                  │
│  extn_extfs.cpp                                  │
│    handles ExtFS commands ($200–$2xx)            │
│    reads host directory, builds catalog          │
│    serves file data from host filesystem         │
│    maps metadata (dates, type/creator)           │
└──────────────────────────────────────────────────┘
```

The 68k INIT is small — it patches one trap and dispatches every
matching File Manager call to the emulator.  All heavy work (directory
scanning, file I/O, metadata mapping) happens on the host side in C++.
