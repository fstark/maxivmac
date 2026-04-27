# WD — Implementation Plan

Design: [WD_DESIGN.md](WD_DESIGN.md)

Phases 1–6 completed 27 Apr 2026.  Commits 21d06d5..e140511.
Phases 7–8 completed 28 Apr 2026.  Commit 0a63cbc.

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Move WD table from HostVolume to DriveManager | ✅ Done |
| 2 | Add `isDefaultOurs()` and new `resolveDir()` on DriveManager | ✅ Done |
| 3 | Add `kNotOurs` return path to existing PB handlers | ✅ Done |
| 4 | Add `kPB_SetVol` host handler | ✅ Done |
| 5 | Add `kPB_GetVol` host handler | ✅ Done |
| 6 | Remove old WD state from HostVolume | ✅ Done |
| 7 | Guest INIT: strip WD state and switch to host-first dispatch | ✅ Done |
| 8 | Remove dead host code | ✅ Done |
| 9 | End-to-end validation | Pending: Fred tests in emulator |

Build gate: `cmake --preset macos && cmake --build --preset macos`
Test gate:  `cd bld/macos && ctest --output-on-failure`

INIT build: You (Fred) build in THINK C inside the emulator.

---

## Phase 1 — Move WD table from HostVolume to DriveManager

The WD table moves from per-volume (`HostVolume::wdTable_`) to
global (`DriveManager`).  This fixes multi-volume WD scoping and
prepares the ground for all later phases.

Both old and new paths coexist — nothing is removed yet.

### 1.1 — Add WDEntry and WD API to DriveManager

**File:** `src/storage/drive_manager.h`

Add after existing members:

```cpp
struct WDEntry {
    int      slot    = -1;
    uint32_t dirID   = 0;
    uint32_t procID  = 0;
};

uint32_t openWD(int slot, uint32_t dirID, uint32_t procID = 0);
uint32_t wdToDirID(uint32_t wdRef) const;
uint32_t wdToProcID(uint32_t wdRef) const;
int      wdToSlot(uint32_t wdRef) const;
void     closeWD(uint32_t wdRef);
bool     isOurWD(uint32_t wdRef) const;

void     setDefaultWD(uint32_t wdRef);
uint32_t defaultWD() const;
```

Add private members:

```cpp
std::unordered_map<uint32_t, WDEntry> wdTable_;
uint32_t nextWD_    = 1;
uint32_t defaultWD_ = 0;
```

### 1.2 — Implement WD API in DriveManager

**File:** `src/storage/drive_manager.cpp`

```cpp
uint32_t DriveManager::openWD(int slot, uint32_t dirID, uint32_t procID)
{
    uint32_t wdRef = nextWD_++;
    wdTable_[wdRef] = {slot, dirID, procID};
    return wdRef;
}

uint32_t DriveManager::wdToDirID(uint32_t wdRef) const
{
    auto it = wdTable_.find(wdRef);
    return (it != wdTable_.end()) ? it->second.dirID : 0;
}

uint32_t DriveManager::wdToProcID(uint32_t wdRef) const
{
    auto it = wdTable_.find(wdRef);
    return (it != wdTable_.end()) ? it->second.procID : 0;
}

int DriveManager::wdToSlot(uint32_t wdRef) const
{
    auto it = wdTable_.find(wdRef);
    return (it != wdTable_.end()) ? it->second.slot : -1;
}

void DriveManager::closeWD(uint32_t wdRef)
{
    wdTable_.erase(wdRef);
}

bool DriveManager::isOurWD(uint32_t wdRef) const
{
    return wdTable_.contains(wdRef);
}

void DriveManager::setDefaultWD(uint32_t wdRef)
{
    defaultWD_ = wdRef;
}

uint32_t DriveManager::defaultWD() const
{
    return defaultWD_;
}
```

### 1.3 — Auto-create root WD on mount

**File:** `src/storage/drive_manager.cpp`, in `mount()`.

After the volume is created and slot assigned, add:

```cpp
// Create a permanent root WD for this slot.
openWD(slot, HostVolume::kRootDirID, 0);
```

Store the root WD ref for later use — add `rootWD_[kMaxDrives]`
array to DriveManager, set `rootWD_[slot] = wdRef` here.

Also add:
```cpp
uint32_t DriveManager::rootWD(int slot) const;
```

On first mount (slot 0), also call
`setDefaultWD(rootWD_[slot])`.

### 1.4 — Tests

**File:** `test/test_drive_manager.cpp`

Add test cases:

- `"DriveManager: WD open/close"` — open a WD, verify
  wdToDirID/wdToProcID/wdToSlot, close it, verify wdToDirID
  returns 0.
- `"DriveManager: WD monotonic"` — open three WDs, verify refs
  are consecutive.
- `"DriveManager: WD default"` — setDefaultWD, verify
  defaultWD() returns it.
- `"DriveManager: WD root auto-created"` — mount a volume, verify
  rootWD(slot) returns a valid ref whose dirID is kRootDirID.
- `"DriveManager: WD isOurWD"` — open a WD, isOurWD returns true.
  Close it, isOurWD returns false.

### Fence

- [ ] DriveManager has WD API, all new unit tests pass
- [ ] Old HostVolume WD code still exists (unused by new code)
- [ ] Full build clean
- [ ] Commit: `"wd: phase 1 — WD table in DriveManager"`

---

## Phase 2 — Add `isDefaultOurs()` and new `resolveDir()` on DriveManager

### 2.1 — Add `isDefaultOurs()`

**File:** `src/storage/drive_manager.h`

```cpp
// Read DefVCBPtr from guest RAM, check if it points to one of
// our VCBs.  Returns true and sets outSlot if ours.
bool isDefaultOurs(int &outSlot) const;
```

**File:** `src/storage/drive_manager.cpp`

```cpp
bool DriveManager::isDefaultOurs(int &outSlot) const
{
    uint32_t vcbPtr = get_vm_long(0x0352);  // DefVCBPtr
    if (vcbPtr == 0) { outSlot = -1; return false; }
    auto vRef = static_cast<int16_t>(get_vm_word(vcbPtr + 78));
    outSlot = slotFromVRefNum(vRef);
    return outSlot >= 0;
}
```

This uses `get_vm_long`/`get_vm_word` from the emulator's memory
access API (same functions used by PBRef and elsewhere in
extn_extfs.cpp).  Add the necessary include if not already present.

### 2.2 — Add `resolveDir()` on DriveManager

**File:** `src/storage/drive_manager.h`

```cpp
// Resolve (vRefNum, rawDirID) → actual dirID + slot.
// Returns 0 if the vRefNum doesn't match any of our volumes
// (caller should return kNotOurs).
uint32_t resolveDir(int16_t vRefNum, uint32_t rawDirID,
                    int &outSlot) const;
```

**File:** `src/storage/drive_manager.cpp`

Implement exactly as shown in WD_DESIGN.md § "Host-Side
resolveDir() Redesign".  Key behaviours:

- `rawDirID != 0` → return rawDirID, determine slot from vRefNum
  (or DefVCBPtr if vRefNum=0).
- `vRefNum == 0` → `isDefaultOurs()`, then `wdToDirID(defaultWD_)`.
- Direct volume ref or drive number → kRootDirID.
- WD refnum → decode and look up in wdTable_.
- Unknown → return 0.

### 2.3 — Add helper: `decodeGuestWDRef()`

**File:** `src/storage/drive_manager.h` (or `drive_constants.h`)

```cpp
inline uint32_t DecodeGuestWDRef(int16_t guestVRefNum)
{
    return static_cast<uint32_t>(
        -(static_cast<int32_t>(guestVRefNum)) - kBaseVRefNum);
}

inline int16_t EncodeGuestWDRef(uint32_t wdRef)
{
    return static_cast<int16_t>(
        -(static_cast<int32_t>(wdRef) + kBaseVRefNum));
}
```

### 2.4 — Tests

**File:** `test/test_drive_manager.cpp`

Note: `isDefaultOurs()` reads guest RAM, which doesn't exist in
unit tests.  Two options:
- Mock it (add a virtual or function pointer for the RAM read).
- Test it in integration only.

**Recommendation:** Add a `resolveDir` test that exercises the
non-DefVCBPtr paths (direct vRefNum, WD refnum, unknown).  The
DefVCBPtr path is tested in integration (Phase 9).

Test cases:

- `"DriveManager: resolveDir explicit dirID"` — vRefNum=-32000,
  dirID=17 → returns 17, slot=0.
- `"DriveManager: resolveDir WD refnum"` — open a WD for dirID=17,
  encode as guest vRefNum, call resolveDir → returns 17.
- `"DriveManager: resolveDir unknown WD"` — pass a WD refnum that
  was never opened → returns 0.
- `"DriveManager: resolveDir drive number"` — vRefNum=8, dirID=0
  → returns kRootDirID, slot=0.
- `"DriveManager: EncodeDecodeGuestWDRef"` — round-trip test.

### Fence

- [ ] `resolveDir` and `isDefaultOurs` exist on DriveManager
- [ ] All new unit tests pass
- [ ] Nothing calls the new functions yet (no integration)
- [ ] Full build clean
- [ ] Commit: `"wd: phase 2 — resolveDir on DriveManager"`

---

## Phase 3 — Add `kNotOurs` return path to existing PB handlers

The host handlers (`PbOpen`, `PbGetCatInfo`, etc.) currently assume
the guest validated ownership.  They call `volumeFromPB()` which
returns `kNsvErr` if the volume doesn't match.  We change this so
that `kNsvErr` from `volumeFromPB()` on the "not our volume" path
surfaces as `kNotOurs` to the guest.

### 3.1 — Define `kNotOurs` sentinel

**File:** `src/core/extn_extfs.cpp`

```cpp
static constexpr uint16_t kNotOurs = 0xFFFE;
```

Using 0xFFFE (not 0xFFFF, which is already "unknown command").

### 3.2 — Update `volumeFromPB()` to distinguish "not ours" from real errors

Currently `volumeFromPB()` returns `kNsvErr` for both "not our
volume" and "our volume but something wrong."

Add a new thin wrapper or change the existing function so that
when the vRefNum is simply not any of our volumes or WDs, it returns
a distinct error.  The simplest approach:

```cpp
// New enum value in storage namespace:
static constexpr OSErr kNotOursErr = -9999;
```

When `volumeFromPB()` exhausts all checks and finds no matching
volume, return `kNotOursErr` instead of `kNsvErr`.

### 3.3 — Update PB handler dispatch

In `ExtnExtFSDispatch`, for every PB-based case that calls a
`PbXxx()` function, check: if the result is `kNotOursErr`, set
`regResult = kNotOurs` instead.

Simple approach — a helper:

```cpp
static uint16_t translateResult(OSErr err)
{
    if (err == kNotOursErr) return kNotOurs;
    return static_cast<uint16_t>(err);
}
```

Then the dispatch becomes:
```cpp
case kPB_Open:
    regResult = translateResult(
        PbOpen(PBRef{regParam[0]}, regParam, regParam[1] != 0));
    break;
```

Apply to all PB cases: `kPB_GetCatInfo`, `kPB_GetFileInfo`,
`kPB_Open`, `kPB_OpenRF`, `kPB_Create`, `kPB_Delete`,
`kPB_Rename`, `kPB_DirCreate`, `kPB_CatMove`, `kPB_SetFileInfo`,
`kPB_SetCatInfo`, `kPB_OpenWD`, `kPB_CloseWD`, `kPB_GetWDInfo`.

### 3.4 — Backward compatibility

The **old guest INIT** currently calls these handlers only after
its own `IsOurVolume()` check, so `kNotOursErr` should never be
reached from the old guest.  But if it is, the old guest will see
`regResult = 0xFFFE`, which is a large positive number cast to
`short` = -2 — a harmless "unknown error" that won't be confused
with `kNoErr`.  Safe.

### Fence

- [ ] `kNotOurs` defined, `volumeFromPB()` returns `kNotOursErr`
      for non-matching volumes
- [ ] All PB handler dispatch lines use `translateResult()`
- [ ] Old guest still works identically (it never triggers kNotOurs)
- [ ] Full build clean, existing tests pass
- [ ] Commit: `"wd: phase 3 — kNotOurs return path"`

---

## Phase 4 — Add `kPB_SetVol` host handler

### 4.1 — Define command constant

**File:** `src/core/extn_extfs.cpp`

```cpp
static constexpr uint16_t kPB_SetVol = 0x0246;
```

### 4.2 — Implement `PbSetVol()`

**File:** `src/core/extn_extfs.cpp`

```cpp
static void PbSetVol(uint32_t regParam[], uint16_t &regResult)
{
    PBRef pb{regParam[0]};
    bool isHFS = regParam[1] != 0;

    int16_t vRefNum = pb[ioVRefNum];
    uint32_t nameAddr = pb[ioNamePtr];

    // 1. Try to find the volume by vRefNum.
    int slot = -1;
    if (vRefNum == 0 && nameAddr != 0)
    {
        // By-name lookup.
        std::string name = readPascalString(nameAddr);
        slot = s_drives.slotFromName(name);
    }
    else
    {
        // By vRefNum (including WD refnums).
        slot = s_drives.slotFromVRefNum(vRefNum);
        if (slot < 0)
        {
            // Try WD refnum.
            auto wdRef = DecodeGuestWDRef(vRefNum);
            int wdSlot = s_drives.wdToSlot(wdRef);
            if (wdSlot >= 0) slot = wdSlot;
        }
    }

    if (slot < 0)
    {
        regResult = kNotOurs;
        return;
    }

    // 2. Determine which WD to set as default.
    uint32_t wdRef;
    if (isHFS)
    {
        uint32_t dirID = static_cast<uint32_t>(pb[ioDrDirID]);
        if (dirID != 0 && dirID != kRootDirID)
        {
            // Open a new WD for this dirID.
            wdRef = s_drives.openWD(slot, dirID, 0);
        }
        else if (vRefNum != 0)
        {
            // Caller passed a WD refnum — try to reuse it.
            auto decoded = DecodeGuestWDRef(vRefNum);
            if (s_drives.isOurWD(decoded))
                wdRef = decoded;
            else
                wdRef = s_drives.rootWD(slot);
        }
        else
        {
            wdRef = s_drives.rootWD(slot);
        }
    }
    else
    {
        wdRef = s_drives.rootWD(slot);
    }

    s_drives.setDefaultWD(wdRef);

    regParam[0] = static_cast<uint32_t>(slot);
    regResult = 0;
}
```

### 4.3 — Add `slotFromName()` to DriveManager

**File:** `src/storage/drive_manager.h`

```cpp
int slotFromName(std::string_view name) const;
```

**File:** `src/storage/drive_manager.cpp`

Iterate slots, case-insensitive compare against `volumeName(i)`.

### 4.4 — Wire into dispatch switch

**File:** `src/core/extn_extfs.cpp`, in `ExtnExtFSDispatch`:

```cpp
case kPB_SetVol:
    PbSetVol(regParam, regResult);
    break;
```

### Fence

- [ ] `kPB_SetVol` handler exists and compiles
- [ ] `slotFromName()` works
- [ ] Not yet called by any guest (old guest uses old path)
- [ ] Full build clean, existing tests pass
- [ ] Commit: `"wd: phase 4 — kPB_SetVol handler"`

---

## Phase 5 — Add `kPB_GetVol` host handler

### 5.1 — Define command constant and implement

**File:** `src/core/extn_extfs.cpp`

```cpp
static constexpr uint16_t kPB_GetVol = 0x0247;
```

```cpp
static void PbGetVol(uint32_t regParam[], uint16_t &regResult)
{
    PBRef pb{regParam[0]};
    bool isHFS = regParam[1] != 0;

    int defSlot = -1;
    if (!s_drives.isDefaultOurs(defSlot))
    {
        regResult = kNotOurs;
        return;
    }

    auto *vol = s_drives.volume(defSlot);
    if (!vol)
    {
        regResult = kNotOurs;
        return;
    }

    // Fill ioVRefNum with current default WD (guest-encoded).
    uint32_t defWD = s_drives.defaultWD();
    pb[ioVRefNum] = EncodeGuestWDRef(defWD);

    // Fill volume name.
    uint32_t nameAddr = pb[ioNamePtr];
    if (nameAddr != 0)
    {
        auto name = s_drives.volumeName(defSlot);
        writePascalString(nameAddr,
            name.empty() ? std::string("Shared") : std::string(name));
    }

    if (isHFS)
    {
        pb[ioWDVRefNum] = EncodeGuestWDRef(s_drives.rootWD(defSlot));
        pb[ioWDProcID]  = 0u;
        uint32_t dirID  = s_drives.wdToDirID(defWD);
        pb[ioWDDirID]   = dirID != 0 ? dirID : kRootDirID;
    }

    regResult = 0;
}
```

### 5.2 — Wire into dispatch switch

```cpp
case kPB_GetVol:
    PbGetVol(regParam, regResult);
    break;
```

### Fence

- [ ] `kPB_GetVol` handler exists and compiles
- [ ] Not yet called by any guest
- [ ] Full build clean, existing tests pass
- [ ] Commit: `"wd: phase 5 — kPB_GetVol handler"`

---

## Phase 6 — Remove old WD state from HostVolume

Now that DriveManager has the WD table and the new handlers use it,
remove the old per-volume WD code.

### 6.1 — Migrate existing callers

**File:** `src/core/extn_extfs.cpp`

Update the **old** PB handlers that still use `vol->openWD()`,
`vol->closeWD()`, `vol->wdToDirID()`, etc. to call the
DriveManager equivalents:

- `PbOpenWD`: call `s_drives.openWD(slot, dirID, procID)` instead
  of `vol->openWD(dirID, procID)`.
- `PbCloseWD`: call `s_drives.closeWD(wdRef)`.
- `PbGetWDInfo`: call `s_drives.wdToDirID(wdRef)` and
  `s_drives.wdToProcID(wdRef)` and `s_drives.wdToSlot(wdRef)`.
- `RegOpenWD`, `RegCloseWD`, `RegGetWDInfo`: same migration.

Update `pbResolveDir()` to call `s_drives.resolveDir()` instead
of `vol.resolveDir()`.

Update `volumeFromPB()` — for the WD refnum path, use
`s_drives.wdToSlot()` instead of walking all volumes.

Remove `s_defaultSlot`; replace with `s_drives.isDefaultOurs()`.

### 6.2 — Remove from HostVolume

**File:** `src/storage/host_volume.h`

Remove:
- `WDEntry` struct
- `wdTable_`, `nextWD_`, `defaultVRefNum_` members
- `openWD()`, `closeWD()`, `wdToDirID()`, `wdToProcID()`
- `setDefaultVRefNum()`, `defaultVRefNum()`
- `resolveDir()`

**File:** `src/storage/host_volume.cpp`

Remove all corresponding implementations.

### 6.3 — Update tests

**File:** `test/test_host_volume.cpp`

Remove or update any tests that call the removed HostVolume WD
methods (if any exist).  The replacement tests are in
`test_drive_manager.cpp` from Phase 1.

### Fence

- [ ] No code references `HostVolume::openWD`, `resolveDir`, etc.
- [ ] `s_defaultSlot` gone from extn_extfs.cpp
- [ ] All existing PB handlers use DriveManager for WD resolution
- [ ] Full build clean, all tests pass
- [ ] Commit: `"wd: phase 6 — remove WD state from HostVolume"`

---

## Phase 7 — Guest INIT: strip WD state and switch to host-first dispatch

This is the guest-side rewrite.  **Fred builds the INIT in THINK C.**

This phase is large, so here's a suggested order with intermediate
test points.  Each step should leave the INIT compilable and
functional.

### 7.1 — Add new constants

```c
#define kPB_SetVol   0x0246
#define kPB_GetVol   0x0247
#define kNotOurs     0xFFFE
```

**Test:** Compiles.  No behaviour change.

### 7.2 — Rewrite TrapSetVol

Replace the entire function body with the host-first version:

```c
static OSErr TrapSetVol(char *pb, Globals *g, short isHFS)
{
    reg_set(g->regBase, 0, (unsigned long)pb);
    reg_set(g->regBase, 1, (unsigned long)isHFS);
    reg_command(g->regBase, kPB_SetVol);

    if ((unsigned short)reg_result(g->regBase) == kNotOurs) return 1;
    if (reg_result(g->regBase) != 0)
        return (short)reg_result(g->regBase);

    {
        short slot = (short)reg_get(g->regBase, 0);
        *(Ptr *)kDefVCBPtr = g->vcb[slot];
    }
    return kNoErr;
}
```

**Test:** Boot with shared drive.  `SetVol` to the shared volume
by name (e.g. from Finder or THINK C "Set Project" in a
subfolder).  The shared drive should become the default.  Then
switch to the boot disk — Finder should still work (SetVol for
the boot disk returns kNotOurs, falls through to ROM).

### 7.3 — Rewrite TrapGetVol

Replace with:

```c
static OSErr TrapGetVol(char *pb, Globals *g, short isHFS)
{
    reg_set(g->regBase, 0, (unsigned long)pb);
    reg_set(g->regBase, 1, (unsigned long)isHFS);
    reg_command(g->regBase, kPB_GetVol);

    if ((unsigned short)reg_result(g->regBase) == kNotOurs) return 1;
    return host_err(g->regBase);
}
```

**Test:** After `SetVol` to shared drive, call `GetVol` (Finder
does this constantly).  Should return the correct volume name and
vRefNum.  Switch to boot disk, GetVol should pass through to ROM.

### 7.4 — Rewrite TrapGetVolInfo

Replace with host-first dispatch.  The host fills all PB fields
including volume metadata (file count etc.).

Note: the host needs a new `kPB_GetVolInfo` handler (0x0249) for
this, OR TrapGetVolInfo can use `kPB_GetVol` for WD fields and
keep filling VCB metadata itself.

**Simpler approach for now:** Keep VCB metadata guest-side, only
replace the WD-related fields.  TrapGetVolInfo calls a new host
command `kPB_GetVolWDFields` (0x0249) that fills only ioVRefNum,
ioWDVRefNum, ioWDDirID.  Guest fills the rest from VCB as before.

Actually, simplest: just send the whole PB to the host via a
generic `kPB_GetVolInfo` command.  The host fills it all.

**Test:** Open "About This Macintosh" or select the shared volume
in Finder and "Get Info".  Free space and file counts should be
correct.  Repeat with boot disk selected — should also work (ROM
handles it).

### 7.5 — Remove IsOurVolume from dispatch path

In `DispatchFlat()` and `DispatchHFS()`, remove the
`IsOurVolume()` pre-check for non-refBased traps.  The host now
handles ownership detection.

For `!e->refBased` traps, just call the handler directly.  The
handler itself sends to host, which returns kNotOurs if not ours,
and the handler returns 1 (pass-through).

**Keep `IsOurFCB()` for refBased traps** (Read, Write, Close,
GetEOF, SetEOF, GetFPos, SetFPos, FlushFile, GetFCBInfo).  These
are file-handle-based — the guest's FCB check is the correct
ownership test (if we allocated the FCB, it's our file).

**Test:** This is the critical step.  Boot the system.  The boot
disk's File Manager calls (hundreds of them before desktop appears)
must all pass through correctly.  Finder browsing, opening apps,
everything.  The shared drive should still mount and be browsable.

### 7.6 — Remove WD state from Globals

Remove from the Globals struct:
- `rootWDRefNum`
- `defaultWDRefNum`

Remove from file scope:
- `sOurWDs[]`, `sOurWDCount`
- `TrackWD()`, `UntrackWD()`

Remove from `main()`:
- The root WD creation block (lines ~2082-2093).

Remove from `TrapOpenWD`:
- The `TrackWD()` call.

Remove from `TrapCloseWD`:
- The `UntrackWD()` call.

Remove:
- `IsOurVolume()`, `IsOurVCB()`, `FindVCB()`.
- `kCmdOpenWD`, `kCmdGetWDInfo` constants (if no longer used).

**Test:** Full boot and exercise:
- Finder shows shared drive.
- Open a THINK C project in a subfolder → compile succeeds.
- Switch between projects in different subfolders.

### 7.7 — Simplify remaining trap handlers

Traps that currently have `IsOurVolume` checks embedded in their
body (if any remain beyond SetVol/GetVol) should be switched to
the host-first pattern.  Specifically verify:

- `TrapCreate`, `TrapDelete`, `TrapRename` — these are already
  PB pass-through but the dispatch path previously checked
  IsOurVolume.  After 7.5 that check is gone; confirm they work.
- `TrapGetFileInfo`, `TrapSetFileInfo`, `TrapGetCatInfo`,
  `TrapSetCatInfo` — same.

**Test:** Create a file on the shared drive.  Delete it.  Rename
a file.  Do the same on the boot disk.  Both should work.

### Fence

- [ ] Guest INIT has no WD state (no rootWDRefNum, defaultWDRefNum,
      sOurWDs)
- [ ] All trap handlers use host-first dispatch (non-refBased)
      or FCB check (refBased)
- [ ] No IsOurVolume/IsOurVCB/FindVCB in the codebase
- [ ] Full boot works with both shared and boot disk
- [ ] Commit: `"wd: phase 7 — guest INIT host-first dispatch"`

---

## Phase 8 — Remove dead host code

### 8.1 — Remove register-based WD handlers

**File:** `src/core/extn_extfs.cpp`

Remove:
- `RegOpenWD()` function
- `RegCloseWD()` function
- `RegGetWDInfo()` function
- Their dispatch cases (`kExtFSOpenWD`, `kExtFSCloseWD`,
  `kExtFSGetWDInfo`)
- `kPB_SetDefaultVRefNum` handler

Keep the command constants defined (they're just numbers) but the
handlers become `regResult = 0xFFFF` (unknown command).

### 8.2 — Remove stale register-based commands

`kExtFSGetVol` (0x201) — was used by old guest.  If the new guest
doesn't call it, remove `RegGetVol()`.

Check each `Reg*` function: is it still called by the new guest?
The new guest only uses: `kExtFSVersion`, `kExtFSRead`,
`kExtFSWrite`, `kExtFSClose`, `kExtFSSetEOF`, `kExtFSDbgLog`,
`kExtFSLogTrap`, `kExtFSGuestVars`, `kExtFSFatal`,
`kExtFSPollMount`, `kExtFSGetVolName`.

Remove any `Reg*` handlers that are only called by old guest code.

### Fence

- [ ] Dead register handlers removed
- [ ] Full build clean, all tests pass
- [ ] Boot still works
- [ ] Commit: `"wd: phase 8 — remove dead host code"`

---

## Phase 9 — End-to-end validation

No code changes.  This is the test phase.

### 9.1 — Boot test

Boot with one shared folder.  Finder shows it.  Browse directories.

### 9.2 — THINK C subfolder test

Open a THINK C project in `Shared/ProjectFolder/`.  Compile.
All source files found and compiled.

### 9.3 — SetVol + Open regression test

`SetVol("Shared:ProjectFolder:")` then `Open("SourceFile.c",
vRefNum=0)`.  Must find the file in ProjectFolder, not root.
(This is the original bug.)

### 9.4 — OpenRF regression test

Open a resource fork of a file in a subfolder.  Must work.
(This is the other original bug.)

### 9.5 — Two-volume test (if multi-volume supported)

Mount two shared folders.  SetVol to a subfolder on the second.
Open a file with vRefNum=0.  Must land on the second volume's
subfolder.

### 9.6 — Stale WD test

OpenWD for a subfolder.  CloseWD.  Try to use the returned
vRefNum.  Must get an error (not silently resolve to root).

### 9.7 — Non-regression

Run `selftest.sh` to verify determinism is preserved.

### 9.8 — Boot disk operations

While shared drive is mounted, verify: create/delete/rename files
on the boot HFS disk.  Must work (ROM handles, host returns
kNotOurs).

### Fence

- [ ] All tests above pass
- [ ] Commit: `"wd: phase 9 — validation complete"`
