# Host File Sharing (virtual volume on the desktop)

Mount a host directory as a volume visible to the Finder, without
requiring a real disk image.  Targeting **System 6** as the primary
platform.

# Background research: look at old RAMdisk, either code, or trap trace to understand how they did it

# Bugs

Folders don'tt really work, files written in folders end up at root at some point

Icons positions are not kept (in root at least)

Folder display style not kept (in root at least)

Type/creator map doesn't seem to work for .tiff

Change file type/creator with ResEdit fails

Copied files seems to lose resources after reboot

# Works

Text file conversion

# New feature

Specify what gets text file conversion or not (ie: C source files)



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
