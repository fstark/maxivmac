# Shared Drive — Detailed Design

Implements the specification in [SHAREDRIVE.md](SHAREDRIVE.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

No new directories.  New files integrate into existing module boundaries.

```
src/
  storage/
    host_volume.h          (modified)   HostVolume — remove hardcoded VRefNum/DriveNum
    host_volume.cpp        (modified)   HostVolume — per-instance volume name
    drive_manager.h        (new ~120L)  DriveManager class
    drive_manager.cpp      (new ~250L)  slot lifecycle, lookup, mount/unmount
  core/
    extn_extfs.h           (modified)   add ExtFSMountDrive(), ExtFSUnmountDrive()
    extn_extfs.cpp         (modified)   replace s_volume with DriveManager dispatch
    config_loader.h        (modified)   add sharedDirs to LaunchConfig
    config_loader.cpp      (modified)   parse --shared
  platform/
    emulator_shell.cpp     (modified)   route FileDrop to mount
  debugger/
    cmd_drive.cpp          (new ~120L)  drive mount/unmount/list commands
    debugger.cpp           (modified)   add CmdDrive to s_commands[]
  macsrc/shareddrive/
    init.c                 (modified)   multi-VCB, polling for new drives
```

---

## 2. Public Interface

### 2.1  DriveManager

The single point of control for all shared drives.  Owns a fixed-size
array of slots; each slot holds an optional `HostVolume`.

```cpp
// src/storage/drive_manager.h
#pragma once

#include "storage/host_volume.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace storage {

class DriveManager {
public:
    static constexpr int kMaxDrives = 8;

    // Mount hostDir in the next free slot.  Returns the slot index,
    // or -1 if all slots are full or the path is invalid.
    int mount(const std::filesystem::path &hostDir);

    // Unmount slot: close all open forks, free catalog, release slot.
    // Returns false if the slot is empty or out of range.
    bool unmount(int slot);

    // Look up by slot index.  Returns nullptr if slot empty/invalid.
    HostVolume       *volume(int slot);
    const HostVolume *volume(int slot) const;

    // Look up by guest vRefNum.  Returns nullptr if no match.
    HostVolume       *volumeByVRefNum(int16_t vRefNum);
    const HostVolume *volumeByVRefNum(int16_t vRefNum) const;

    // Look up by guest drive number.  Returns nullptr if no match.
    HostVolume       *volumeByDriveNum(int16_t driveNum);

    // Slot index from vRefNum.  Returns -1 if not ours.
    int slotFromVRefNum(int16_t vRefNum) const;

    // Number of currently mounted drives.
    int mountedCount() const;

    // Volume name for a slot (derived from host dir basename, deduped).
    std::string_view volumeName(int slot) const;

    // Host path for a slot.
    const std::filesystem::path &hostPath(int slot) const;

    // Open-fork count across all forks in a slot.
    int openForkCount(int slot) const;

    // Iterate all slots.  Callback receives (slot, HostVolume&).
    template <typename Fn>
    void forEach(Fn &&fn) const;

    // Check whether a new drive has been requested from the host side
    // (debugger command, drag-drop, etc.) and is pending insertion.
    // Called by the guest INIT's polling loop via kExtFSPollMount.
    // Returns the slot index of the pending drive, or -1 if none.
    int popPendingMount();

    // Queue a drive for later guest discovery.
    void queuePendingMount(int slot);

private:
    struct Slot {
        std::optional<HostVolume> vol;
        std::string volumeName;     // deduped Mac-visible name
    };
    std::array<Slot, kMaxDrives> slots_;
    std::vector<int> pendingMounts_;  // slots awaiting guest pickup

    std::string deduplicateName(std::string_view baseName) const;
};

} // namespace storage
```

### 2.2  HostVolume changes

Remove the per-class constants `kGuestVRefNum` and `kGuestDriveNum`.
Add per-instance identity that DriveManager assigns after construction.

```cpp
// Added to HostVolume public interface:

// Assigned by DriveManager after mount().
void     setSlot(int slot);
int      slot() const;
int16_t  guestVRefNum() const;   // -(kBaseVRefNum + slot)
int16_t  guestDriveNum() const;  // kBaseDriveNum + slot
```

```cpp
// Removed from HostVolume:
//   static constexpr int16_t kGuestVRefNum  = -32000;  // gone
//   static constexpr int16_t kGuestDriveNum = 8;        // gone

// Moved to drive_manager.h as free constants:
inline constexpr int16_t kBaseVRefNum  = 32000;   // vRefNum = -(kBaseVRefNum + slot)
inline constexpr int16_t kBaseDriveNum = 8;        // driveNum = kBaseDriveNum + slot
```

VRefNum encoding:  slot 0 → -32000, slot 1 → -32001, … slot 7 → -32007.
Drive number:  slot 0 → 8, slot 1 → 9, … slot 7 → 15.

This is compatible with the existing guest INIT's `IsOurVolume()` guard,
which already accepts vRefNums in the range (-32000, -32100).

### 2.3  Host-side mount/unmount API

Callable from the debugger, CLI boot, and drag-drop handler:

```cpp
// src/core/extn_extfs.h  (added)

// Mount a host directory as a new shared drive.
// Returns the slot index, or -1 on failure.
// Thread-safe: can be called from the main/UI thread.
int  ExtFSMountDrive(const std::filesystem::path &hostDir);

// Unmount a shared drive by slot.  Closes all open forks.
// Returns false if the slot was already empty.
bool ExtFSUnmountDrive(int slot);
```

---

## 3. Integration Points

### 3.1  extn_extfs.cpp — replace singleton with DriveManager

**Current state:** file-static `static storage::HostVolume s_volume;`
at [extn_extfs.cpp L49](src/core/extn_extfs.cpp#L49).

**Change:** Replace with `static storage::DriveManager s_drives;`.

Every handler that currently calls `s_volume.method()` must first
resolve which volume is being addressed.  Two patterns:

1. **Register-based commands** (Read, Write, Close, SetEOF, OpenWD,
   etc.): the handle encodes the slot.  See §5.1.
2. **PB-based commands** (GetCatInfo, Open, Create, etc.): the PB's
   `ioVRefNum` field identifies the volume via `s_drives.slotFromVRefNum()`.

**Cost when multi-drive is active:** one `slotFromVRefNum()` lookup per
dispatch — a bounds-checked array index, effectively zero cost.

### 3.2  extn_extfs.cpp — new register commands

Two new command codes for mount polling:

```cpp
static constexpr uint16_t kExtFSPollMount  = 0x219;  // guest asks "any new drive?"
static constexpr uint16_t kExtFSGetVolName = 0x21A;  // guest asks for volume name
```

**`kExtFSPollMount`** (no parameters):

```
Returns: regParam[0] = slot index of newly mounted drive, or 0xFFFFFFFF if none.
         regParam[1] = vRefNum (negative)
         regParam[2] = driveNum
```

The guest INIT calls this periodically (e.g. on VBL task tick) to
discover drives mounted at runtime, then allocates a VCB + DQE for the
new volume just like it does today at INIT time.

**`kExtFSGetVolName`** (`regParam[0]` = slot index):

```
Returns: regParam[0]..regParam[3] = volume name as Pascal string,
         packed big-endian into register words (up to 27 chars).
```

### 3.3  extn_extfs.cpp — RegVersion (kExtFSVersion, 0x200)

Currently mounts `"shared"` if not already mounted.  **Change:**
remove the auto-mount.  RegVersion returns the mounted-drive count
(0 if nothing mounted).  The guest INIT uses this to decide whether
to begin polling.

```cpp
static void RegVersion(uint32_t regParam[], uint16_t &regResult)
{
    regParam[0] = static_cast<uint32_t>(s_drives.mountedCount());
    regResult = 0;
}
```

### 3.4  config_loader — add `--shared`

**File:** [config_loader.h](src/core/config_loader.h) and
[config_loader.cpp](src/core/config_loader.cpp).

Add to `LaunchConfig`:
```cpp
std::vector<std::string> sharedDirs;  // --shared <path> (repeatable)
```

In the parser (alongside the existing `--disk` handling):
```cpp
if (arg == "--shared" && i + 1 < argc) {
    lc.sharedDirs.push_back(argv[++i]);
}
```

At boot in `ProgramMain()` (or wherever LaunchConfig is consumed),
call `ExtFSMountDrive()` for each path before emulation starts.

### 3.5  Debugger — `drive` command

**New file:** `src/debugger/cmd_drive.cpp` (~120 lines).

**Command table entry** added to `s_commands[]` in
[debugger.cpp L60](src/debugger/debugger.cpp#L60):

```cpp
{"drive", "", CmdDrive, "Mount/unmount shared drives",
 "drive mount <path>\n  Mount a host directory as a new shared drive.\n"
 "drive unmount <slot>\n  Unmount and release shared drive by slot number.\n"
 "drive list\n  Show all mounted shared drives.\n"},
```

**Handler implementation sketch:**

```cpp
void CmdDrive(Debugger &dbg, const std::vector<Token> &args)
{
    auto &io = dbg.io();
    if (args.size() < 2) { /* show usage */ return; }

    if (args[1].text == "mount" && args.size() >= 3) {
        int slot = ExtFSMountDrive(args[2].text);
        if (slot < 0)
            io.printf("mount failed: path invalid or no free slot\n");
        else
            io.printf("mounted slot %d\n", slot);
    }
    else if (args[1].text == "unmount" && args.size() >= 3) {
        int slot = /* parse int from args[2] */;
        if (!ExtFSUnmountDrive(slot))
            io.printf("slot %d is not mounted\n", slot);
        else
            io.printf("unmounted slot %d\n", slot);
    }
    else if (args[1].text == "list") {
        io.printf(" Slot  Volume      Host path            Forks\n");
        s_drives.forEach([&](int slot, const auto &vol) {
            io.printf(" %-5d %-11s %-20s %d\n",
                       slot,
                       s_drives.volumeName(slot).data(),
                       s_drives.hostPath(slot).c_str(),
                       s_drives.openForkCount(slot));
        });
    }
}
```

### 3.6  Drag-and-drop

**File:** [emulator_shell.cpp L446](src/platform/emulator_shell.cpp#L446).

Currently `FileDrop` inserts a Sony floppy image.  **Change:** if the
dropped path is a directory, mount it as a shared drive; if it is a
file, use the existing Sony insert path.

```cpp
case PlatformEvent::Type::FileDrop:
    if (evt.filePath) {
        if (std::filesystem::is_directory(evt.filePath))
            ExtFSMountDrive(evt.filePath);
        else
            Sony_Insert1a_impl(const_cast<char *>(evt.filePath), false);
    }
    break;
```

### 3.7  CMakeLists.txt

Add `src/storage/drive_manager.cpp` to `MINIVMAC_SOURCES` (storage
section) and `src/debugger/cmd_drive.cpp` to the debugger section.

---

## 4. Internal State

### 4.1  Handle encoding

Handles are currently monotonic 32-bit integers in HostVolume.
To route handles back to the correct drive, encode the slot in the
upper 4 bits:

```
handle = (slot << 28) | per_volume_handle
```

Slot extraction is a single shift: `slot = handle >> 28`.
Per-volume handle: `localHandle = handle & 0x0FFFFFFF`.
With 28 bits for the local handle, the per-volume counter can run
to 268 million opens before wrapping — effectively infinite.

This keeps handle dispatch O(1) with no lookup table.

### 4.2  DriveManager::Slot

```cpp
struct Slot {
    std::optional<HostVolume> vol;   // nullopt → slot is free
    std::string volumeName;          // deduped Mac-visible name (≤27 chars)
};
```

The `std::optional` ensures that unmounting runs the HostVolume
destructor, which closes any open `FILE*` handles.  Re-mounting
into the same slot constructs a fresh HostVolume.

### 4.3  Volume name deduplication

`DriveManager::deduplicateName()` — when the host directory basename
collides with an existing mounted volume name, append a numeric suffix:

```
baseName      → "Pictures"
collision     → "Pictures-2"
next collision→ "Pictures-3"
```

The name is truncated to 27 characters (HFS volume name limit).

### 4.4  Pending-mount queue

`pendingMounts_` is a small vector of slot indices.  `queuePendingMount()`
appends; `popPendingMount()` removes and returns the front element.
This is only accessed from the emulation thread (register-block
dispatch runs synchronously inside the CPU loop) so no lock is needed
for the pop path.

For mounts triggered from the UI thread (drag-drop), the mount itself
creates the HostVolume and scans the directory on the UI thread (safe:
HostVolume construction doesn't touch guest RAM), then enqueues the
slot index.  The guest polls on the next jGNEFilter call (see §5.3)
and picks it up.

---

## 5. Key Algorithms

### 5.1  Dispatch routing

For every incoming command:

```
PB-based commands (0x230–0x245):
    vRefNum = read ioVRefNum from guest PB
    slot    = s_drives.slotFromVRefNum(vRefNum)
    if slot < 0 → return kNsvErr
    vol     = s_drives.volume(slot)

Register-based I/O commands (Read, Write, Close, SetEOF):
    handle  = regParam[0]
    slot    = handle >> 28
    local   = handle & 0x0FFFFFFF
    vol     = s_drives.volume(slot)
    if vol == nullptr → regResult = kRfNumErr
    forward call with localHandle

Register-based non-I/O (GetVol, OpenWD, etc.):
    These operate on a specific volume identified by regParam
    containing a vRefNum or WD ref — same lookup as PB path.
```

### 5.2  Unmount

```
ExtFSUnmountDrive(slot):
    vol = s_drives.volume(slot)
    if vol is null → return false
    // close all open forks held by this volume
    vol->closeAllForks()
    // destroy the volume (releases catalog + filesystem handles)
    s_drives.unmount(slot)
    return true
```

The guest side must separately eject the volume (VCB + DQE removal).
For debugger-initiated unmount, the host posts a synthetic
`diskEvt` with a special "eject slot N" payload so the guest INIT
does the cleanup.  Guest-initiated unmount (_Eject / _UnmountVol)
first runs through the normal trap path to close FCBs, then the
host-side handler for _Eject calls `ExtFSUnmountDrive()`.

### 5.3  Guest INIT multi-volume bootstrap

At INIT time, the guest calls `kExtFSVersion`.  The host returns the
count of already-mounted drives.  For each drive (0 to count−1), the
INIT calls `kExtFSGetVolName` to get the Mac-visible name, then
allocates a VCB and DQE with the corresponding vRefNum and driveNum.

#### Runtime mount discovery — jGNEFilter polling

After bootstrap, the INIT hooks `jGNEFilter` — the same mechanism
the ClipSync INIT already uses for clipboard polling
([macsrc/clipsync/init.c](../../macsrc/clipsync/init.c)).  On each
`GetNextEvent` call the filter calls `kExtFSPollMount`.  If a new
drive is pending, the filter allocates a VCB + DQE and posts
`diskInsertEvt` to make the Finder notice it.

This is safe because jGNEFilter runs at **task level** (inside
`GetNextEvent`), not at interrupt level.  `NewPtrSys`, `Enqueue`,
`AddDrive`, and `PostEvent` are all legal here.  A VBL task would
not be safe — VBL runs at interrupt level where Memory Manager
calls (`NewPtr`) and queue manipulation (`Enqueue` into VCBQHdr)
can corrupt the heap or the queue if they interrupt the same
operations in progress.

Polling is throttled to at most once per 60 ticks (~1 s) using the
same `TickCount()` comparison the clipboard filter uses.  The cost
when no drive is pending is a single register-block round-trip
(one word write + one word read) — negligible.

> **Future:** the SharedDrive and ClipSync jGNEFilters can be
> merged into a single shared filter that does both clipboard sync
> and drive polling on each `GetNextEvent` call.

#### Why not the Sony pseudo-exception mechanism?

The Sony driver uses a different approach for disk insertion:
`SonyDevice::update()` runs in the host-side emulation loop and
calls `g_cpu.diskInsertedPseudoException(callback, data)` to
inject an exception frame into the 68k CPU between instructions.
This forces the guest into a callback at a safe point.

We do not use this mechanism for shared drives because:

1. It requires a host-side device with an `update()` method called
   every timeslice.  SharedDrive is an extension, not a device.
2. The pseudo-exception delivers a single uint32 payload (drive
   index + lock flag).  Shared-drive mount needs the guest to
   perform several allocation steps (VCB, DQE, volume name copy)
   that don't fit a one-shot callback.
3. jGNEFilter polling is already proven in the codebase (clipboard)
   and naturally batches: if multiple drives are mounted while the
   guest is busy, the filter drains the pending queue one slot per
   event-loop iteration with no missed insertions.

### 5.4  IsOurVolume (guest side, multi-drive)

The current `IsOurVolume()` already accepts the range (-32000, -32100).
For the multi-drive case, change to explicit range check:

```c
static Boolean IsOurVolume(short vRefNum)
{
    if (vRefNum <= kBaseVRefNum
        && vRefNum > kBaseVRefNum - kMaxDrives)
        return true;   /* one of our vRefNums: -32000..-32007 */

    if (vRefNum >= kBaseDriveNum
        && vRefNum < kBaseDriveNum + kMaxDrives)
        return true;   /* one of our drive numbers: 8..15 */

    /* WD refnums: -(32000 + 100*slot + wdRef), where wdRef < 100 */
    if (vRefNum < -(kBaseVRefNum)
        && vRefNum > -(kBaseVRefNum + kMaxDrives * 100))
        return true;

    /* vRefNum 0 = default volume; check DefVCBPtr */
    if (vRefNum == 0) {
        Globals *g = get_globals();
        return (g != NULL && IsOurVCB(*(Ptr *)kDefVCBPtr, g));
    }
    return false;
}
```

Where `IsOurVCB` checks the Ptr against the array of per-slot VCB
pointers stored in the Globals struct.

---

## 6. Reused Infrastructure

| What | Where | How it's reused |
|------|-------|-----------------|
| `HostVolume` class | `storage/host_volume.h` | Multi-instantiated inside DriveManager slots; API unchanged |
| Register-block RPC | `machine.cpp` `regBlockAccess()` | Same dispatch; no changes to word layout or parameter count |
| PlatformEvent::FileDrop | `imgui_backend.cpp`, `emulator_shell.cpp` | Already captures SDL_EVENT_DROP_FILE; add directory check |
| CmdEntry table | `debugger.cpp` | Add one entry for `drive` |
| `appledouble.cpp` | `storage/appledouble.cpp` | Used identically by each HostVolume instance; no changes |
| `typemap.def` | `assets/typemap.def` | Loaded once, shared across all volumes |

Nothing is duplicated.  `HostVolume` is designed to be per-instance;
no static/global state exists inside it.

---

## 7. Build Integration

**CMakeLists.txt** — add two source files:

```cmake
# Storage
src/storage/drive_manager.cpp

# Debugger
src/debugger/cmd_drive.cpp
```

No new dependencies.  `drive_manager.cpp` includes `storage/host_volume.h`
(already part of the build).  `cmd_drive.cpp` includes
`core/extn_extfs.h` and `debugger/debugger.h` (both existing).

---

## 8. Dependency Diagram

```
                     ┌────────────────┐
                     │  LaunchConfig  │
                     │ (config_loader)│
                     └───────┬────────┘
                             │ sharedDirs
                             ▼
               ┌─────────────────────────┐
  CLI/debugger │   ExtFSMountDrive()     │◄── EmulatorShell (drag-drop)
  cmd_drive ──►│   ExtFSUnmountDrive()   │
               │   (extn_extfs.cpp)      │
               └─────────┬───────────────┘
                         │ owns
                         ▼
              ┌──────────────────────┐
              │    DriveManager      │
              │  (drive_manager.h)   │
              └────┬─────┬─────┬────┘
                   │     │     │   slots_[0..7]
                   ▼     ▼     ▼
              ┌─────┐ ┌─────┐ ┌─────┐
              │ HV0 │ │ HV1 │ │ HV2 │  ...  HostVolume instances
              └──┬──┘ └──┬──┘ └──┬──┘
                 │       │       │
                 ▼       ▼       ▼
              host FS  host FS  host FS
```

All arrows point downward.  No circular dependencies.

---

## 9. Guest-Side Changes

### 9.1  Globals struct expansion

```c
typedef struct {
    char    *regBase;
    Ptr      vcb[kMaxDrives];    // was: single Ptr vcb
    Ptr      dqe[kMaxDrives];    // was: single Ptr dqe
    short    driveCount;         // number of active drives
    long     savedA4;
    short    defaultWDRefNum;
    short    ejected;
} Globals;
```

### 9.2  Per-drive VCB allocation

At INIT time, for each drive reported by `kExtFSVersion`, and on each
`kExtFSPollMount` response in the jGNEFilter (§5.3), the INIT:

1. Calls `kExtFSGetVolName(slot)` → gets the name.
2. Allocates a new 178-byte VCB in the system heap (`NewPtrSysClear`).
3. Fills vRefNum = -(32000 + slot), driveNum = 8 + slot.
4. Writes the volume name.
5. Enqueues in VCBQHdr.
6. Allocates a 20-byte DQE, calls AddDrive.
7. Posts diskInsertEvt (for runtime mounts only; at boot, the
   Finder discovers volumes from the VCB queue).

All allocation happens at task level (INIT time or jGNEFilter
context), never at interrupt level.

### 9.3  Per-drive WD encoding

Working directory refnums must be unique across all drives.  Encode:

```
encoded WD vRefNum = -(kBaseVRefNum + slot * 100 + localWDRef)
```

With 100 WDs per drive and 8 drives, the range is -32000 to -32799 —
well within the (-32000, -32767) band that the existing `IsOurVolume()`
already accepts, and comfortably far from real File Manager WDCBs.

### 9.4  Trap routing

The trap patch stubs already extract `ioVRefNum` from the parameter
block and call `IsOurVolume()`.  For multi-drive, `IsOurVolume()` merely
needs the range check in §5.4.  Once the call is identified as "ours",
the dispatcher forwards the PB to the host, and the host resolves the
exact volume from the PB's `ioVRefNum` field.  No guest-side per-volume
dispatch table is needed.

---

## 10. Testing

### Unit tests

**File:** `test/test_drive_manager.cpp`

| Test | What it verifies |
|------|------------------|
| MountOne | mount() returns slot 0; volume() non-null |
| MountMax | mount 8 drives; 9th returns -1 |
| Unmount | unmount(slot) releases; volume(slot) returns null |
| Remount | after unmount, mount() reuses freed slot |
| VRefNumLookup | slotFromVRefNum(-32000..-32007) returns 0..7 |
| NameDedup | two dirs named "tmp" → "tmp", "tmp-2" |
| HandleEncoding | slot bits round-trip through encode/decode |
| PendingQueue | queuePendingMount / popPendingMount FIFO |

### Integration tests

- Boot with `--shared ./test_dir_a --shared ./test_dir_b`, verify both
  volumes appear via `kExtFSVersion` returning 2.
- Debugger script: `drive mount /tmp`, `drive list`, `drive unmount 0`,
  `drive list` — check output matches expectations.
- Drop a directory on the window, verify `kExtFSPollMount` returns
  the new slot on the next guest poll.
