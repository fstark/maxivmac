# WD_DESIGN — Working Directory Management

## Principle

The host owns all working-directory state.  The guest INIT is a thin
dispatcher: it forwards every File Manager trap to the host, which
decides ownership, resolves WDs, and either handles the call or
returns `kNotOurs` so the guest passes through to the ROM.

## WD Refnum Encoding

```
guestVRefNum = -(wdRef + kBaseVRefNum)   // kBaseVRefNum = 32000
wdRef        = -(guestVRefNum) - kBaseVRefNum
```

`wdRef` is a monotonically increasing `uint32_t` allocated by
`DriveManager`.  `DecodeGuestWDRef` / `EncodeGuestWDRef` in
`drive_constants.h` are the only places this arithmetic is done.

Direct volume vRefNums use the same base: slot 0 → -32000, slot 1 →
-32001, etc.  WD refs start at 1, so their encoded values begin at
-32001 and go more negative; they cannot collide with direct refs.

## Host Data Model

`DriveManager` owns all WD state:

```
DriveManager
├── wdTable_: unordered_map<uint32_t, WDEntry>  // all WDs, all volumes
├── nextWD_: uint32_t                            // monotonic allocator
├── defaultWD_: uint32_t                         // current default WD
├── rootWD_[kMaxDrives]: uint32_t               // root WD per slot
└── slots_[kMaxDrives]: Slot
    └── HostVolume                               // catalog + open forks
```

`WDEntry` holds `{ slot, dirID, procID }`.

`HostVolume` holds no WD state.

A root WD for each slot is created automatically in `DriveManager::mount()`.

## Default Volume Resolution

`vRefNum == 0` means "default volume".  The host reads `DefVCBPtr`
(guest low-memory global at 0x0352) to find the current default VCB,
then checks whether that VCB belongs to one of our slots.
`DriveManager::isDefaultOurs(outSlot)` encapsulates this.  There is
no cached default slot — reading DefVCBPtr directly avoids stale-cache
bugs when another filesystem calls `_SetVol`.

## Directory Resolution — `DriveManager::resolveDir()`

Resolves `(vRefNum, rawDirID)` → `(dirID, slot)`:

1. If `rawDirID != 0` — use it directly; determine slot from vRefNum.
2. If `vRefNum == 0` — `isDefaultOurs()` → `wdToDirID(defaultWD_)`.
3. If vRefNum is a direct slot ref or drive number → `kRootDirID`.
4. If vRefNum is a WD refnum → decode and look up in `wdTable_`.
5. Otherwise → return 0 (error).

**Unknown WD refnum returns 0, never `kRootDirID`.**

## Guest Dispatch Protocol

Every File Manager trap follows this pattern:

```c
reg_set(regBase, 0, (unsigned long)pb);
reg_set(regBase, 1, (unsigned long)isHFS);
reg_command(regBase, kCmd);
if ((unsigned short)reg_result(regBase) == kNotOurs)
    return kPassThrough;  // let ROM handle it
return host_err(regBase);
```

The host returns `kNotOurs` (0xFFFE) when the vRefNum/name doesn't
match any of our volumes.  The guest never compares vRefNums or
VCB pointers; all ownership decisions are made by the host.

File-handle-based traps (Read, Write, GetEOF, etc.) are the exception:
the guest checks its own FCB table to decide ownership, since the host
never sees those refnums.

## Host Commands

| Command         | Code   | Description                              |
|-----------------|--------|------------------------------------------|
| `kPB_SetVol`    | 0x0246 | Set default volume/directory.  Host resolves WD, updates `defaultWD_`, returns slot index so guest can write `DefVCBPtr`. |
| `kPB_GetVol`    | 0x0247 | Fill PB with current default WD info (name, vRefNum, ioWDDirID, etc.). |
| `kPB_OpenWD`    | 0x0242 | Open a WD; returns encoded guest vRefNum. |
| `kPB_CloseWD`   | 0x0243 | Close a WD.                              |
| `kPB_GetWDInfo` | 0x0244 | Query WD dirID / procID / vRefNum.        |
| All other PBs   | —      | Host handles or returns `kNotOurs`.      |

## Fail-Fast Rules

- Unknown WD refnum → 0 from `resolveDir()` → error to caller, never root.
- `kPB_SetVol` with invalid dirID → `kDirNFErr`, no fallback.
- Volume not ours → `kNotOurs` (0xFFFE), guest passes through explicitly.
- No `else return kRootDirID` fallbacks anywhere in resolution paths.

## Key Files

| File | Role |
|------|------|
| `src/storage/drive_constants.h` | `kBaseVRefNum`, `DecodeGuestWDRef`, `EncodeGuestWDRef` |
| `src/storage/drive_manager.h/cpp` | WD table, `openWD`, `closeWD`, `resolveDir`, `isDefaultOurs` |
| `src/core/extn_extfs.cpp` | Host PB handlers, `kNotOurs`/`kNotOursErr`, `volumeFromPB`, `pbResolveDir` |
| `macsrc/init/drive.c` | Guest trap handlers (thin dispatchers) |
| `macsrc/init/defs.h` | `kPB_SetVol`, `kPB_GetVol`, `kNotOurs` constants; `Globals` struct |
