# SharedDrive "Needs Minor Repairs" Debug Log

## The Problem

When the SharedDrive virtual volume mounts, the System 6 Finder shows a
dialog: **"The disk 'Shared' needs minor repairs. Do you want to repair it?"**

This happens every boot. Clicking OK or Cancel dismisses it, and the volume
works fine afterwards. But the dialog is unacceptable for a clean user experience.
=> FALSE. Ok or dismiss just re-present the dialog

## Architecture Context

SharedDrive is a guest-side INIT (`macsrc/shareddrive/init.c`) that:
- Patches File Manager flat traps (_Open, _Close, _Read, _OpenRF, _GetFileInfo, etc.)
  and _HFSDispatch (GetCatInfo, OpenWD, GetVolParms, etc.)
- Mounts a virtual read-only HFS volume (vRefNum=-32000, driveNum=8)
- Dispatches to the host emulator via a register-block interface at `extnBlockBase + $20`
- The host serves files from the `shared/` directory on the real filesystem

Key constants: kOurVRefNum=-32000, kOurDriveNum=8, kOurDrvrRefNum=-64, kRootDirID=2.

## Root Cause (Confirmed by Logs)

The System 6 Finder requires a **Desktop resource file** on every mounted HFS
volume. When it enumerates volumes, for each one it:

1. Calls `_GetVolInfo` to discover the volume
2. Calls `_SetVol` to make it the default volume
3. Calls `_OpenRFPerm` / `_OpenRF` to open the `Desktop` resource fork
4. Reads APPL resources to build the desktop database
=> IS THIS CONFIRMED ANYWHERE?

If `_OpenRF "Desktop"` returns `fnfErr` (-43), the Finder tries to **create** it:
- Calls `_Delete "Desktop"` (clear any old file)
- Calls `CreateResFile` (which internally calls `_OpenRF` then `_Create`)
- If creation fails (we return `wPrErr` because we're read-only), it shows the dialog

### Log Evidence (start.txt, commit f6367ab)

The critical sequence at lines 7776–7853:
```
SD _GetVolParms -> 0                         # Finder queries our capabilities
SD _GetVolInfo vr=-32000 vidx=0 -> 0         # Finder reads our volume info
SD _SetVol vr=-32000 -> setting DefVCBPtr    # Finder makes us the default volume ✓
  $A9C4 OpenRFPerm
  $A00A OpenRF
SD _OpenRF vr=0 nm=Desktop -> fnfErr         # Intercepted! No Desktop file → fnfErr
  $A9C4 OpenRFPerm
  $A00A OpenRF
SD _OpenRF vr=0 nm=Desktop -> fnfErr         # Tries with different permissions → fnfErr
  $A042 RstFilLock
  $A009 Delete
SD _Delete vr=-32000 nm=Desktop -> wPrErr    # Tries to delete old Desktop → write-protected
  $A9B1 CreateResFile
  $A00A OpenRF
SD _OpenRF vr=0 nm=Desktop -> fnfErr         # CreateResFile tries OpenRF first → fnfErr
  $A008 Create
SD _Create vr=0 nm=Desktop -> wPrErr         # CreateResFile tries Create → write-protected!
  $A9AF ResError                             # CreateResFile sets ResError
  ...
  $A98B ParamText                            # Sets up dialog text ("Shared", etc.)
  $A985 Alert                                # ← THE DIALOG
```

## What We Tried

### 1. bNoDeskItems in GetVolParms (ineffective)

Set bit 20 (`bNoDeskItems`) in the `vMAttrib` field returned by `_GetVolParms`.
The full vMAttrib is `0x181B0000` which includes:
- bit 28: bExtFSVol (external file system volume)
- bit 27: bNoSysDir
- bit 20: bNoDeskItems
- bit 19: bNoBootBlks
- bit 17: bNoLclSync
- bit 16: bNoVNEdit

**Result**: System 6 Finder ignores `bNoDeskItems`. It still tries to open/create
Desktop. This flag may only be respected by System 7+.
=> I DO NOT THINK THIS DOES WHAT YOU THINK IT DOES. IT IS ONLY ABOUT PUTTING ICONS ON THE DESKTOP.

### 2. DefVCBPtr check in IsOurVolume (commit a6ae3a7, necessary but insufficient)

Added check: when `vRefNum == 0` (default volume), read `DefVCBPtr` ($0352) and
compare with our VCB pointer.

**Result**: DefVCBPtr was never being set to our VCB in the first place, so this
check never matched. The real File Manager's `_SetVol` does NOT update DefVCBPtr
for external FS VCBs.

### 3. Patch _SetVol and _GetVol (commit f6367ab, partially successful)

Patched `_SetVol` ($A015) and `_GetVol` ($A014):
- When `_SetVol` is called with our vRefNum, set `DefVCBPtr = g->vcb`
- When `_SetVol` is called by name "Shared", also set DefVCBPtr
- When `_GetVol` is called and DefVCBPtr is ours, return our volume info

**Result**: The DefVCBPtr fix WORKS — after `_SetVol`, subsequent `_OpenRF "Desktop"`
calls with `vr=0` are correctly intercepted (log shows `SD _OpenRF vr=0 nm=Desktop -> fnfErr`).
However, this revealed the NEXT problem: Finder gets `fnfErr`, then tries to create
Desktop, gets `wPrErr`, and shows the dialog anyway. The interception isn't enough;
we need to actually serve a Desktop file.

### 4. Implement file creation, writing, and deletion (commit 38e3102)

Implemented the real fix: full file creation and write support so the Finder can
build the Desktop file itself.

**Host side** (extn_extfs.cpp) — new RPC commands:
- `$210 CreateFile` — creates file on disk, adds to catalog
- `$211 Write` — writes guest RAM to open file handle
- `$212 DeleteFile` — removes file and .rsrc from disk
- `$213 SetFileInfo` — updates type/creator in catalog
- Open with fork=1 creates `.rsrc` file if it doesn't exist
- Open uses `r+b` (read-write) first, falls back to `rb`

**Guest side** (init.c) — updated handlers:
- `_Create` → RPC to host CreateFile
- `_Delete` → RPC to host DeleteFile
- `_OpenRF` → opens resource fork (`.rsrc` file) via host Open
- `_Write` → RPC to host Write, updates FCB mark/EOF
- `_SetEOF` → updates FCB EOF (resource manager needs this)
- `_SetFileInfo` → RPC to host SetFileInfo
- `_Allocate` → returns noErr
- Volume no longer marked as software-locked (`vcbAtrb = 0`)

Resource forks are stored as `filename.rsrc` alongside the data fork on the host.

**Status**: Needs testing. Rebuild INIT in THINK C and boot.

## Diagnostic Logging

The INIT has these log messages (prefix `SD`):

- `SD _SetVol vr=X -> setting DefVCBPtr` — SetVol intercepted
- `SD _Create vr=X nm=Y -> Z` — file creation result
- `SD _Delete vr=X nm=Y -> Z` — file deletion result
- `SD _OpenRF vr=X nm=Y -> Z` — resource fork open result
- `SD _Write ref=X cnt=Y -> Z act=W` — write result
- `SD _SetEOF ref=X eof=Y` — EOF change
- `SD _SetFileInfo vr=X nm=Y -> Z` — file info update
- `SD pass-thru Xa vr=X` / `nm=X` — _Open/_OpenRF not on our volume
- All other handlers: `SD _Open`, `SD _Read`, `SD _GetVolInfo`, `SD _FlushVol`, etc.

Logging uses extension command $020D (ExtFSDbgLog). Format: `%d`, `%x`, `%u`,
`%s` (C string), `%S` (Pascal string).

## What The Fix Should Be

Implemented: full file creation and write support. The Finder can now:
1. `_Create "Desktop"` → creates empty file on host
2. `_OpenRF "Desktop"` → creates/opens `Desktop.rsrc` on host
3. Resource Manager `_Write` calls → writes resource data to `.rsrc` file
4. `_Close` → closes the file

The Desktop file's resource fork is stored as `shared/Desktop.rsrc` on the host.

## Current Git State

- Branch: `master`
- Last commit: `38e3102` — implement file creation, writing, and deletion
- Guest source: `macsrc/shareddrive/init.c` (~1600 lines)
- Guest compiler: THINK C 5, System 6.0.8
- The INIT must be rebuilt in THINK C and installed in the emulated System Folder

## File Map

- `macsrc/shareddrive/init.c` — Guest INIT source (THINK C)
- `src/core/extn_extfs.cpp` — Host ExtFS RPC handlers (C++)
- `src/core/extn_clip.cpp` — Has guestFormatLog / %S format support (C++)
- `start.txt` — Most recent boot log (73840 lines)
