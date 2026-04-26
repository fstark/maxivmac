# Shared Drive Multi-Volume — Implementation Plan

Design: [SHAREDRIVE_DESIGN.md](SHAREDRIVE_DESIGN.md)
Spec: [SHAREDRIVE.md](SHAREDRIVE.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | HostVolume identity: per-instance slot, vRefNum, driveNum | |
| 2 | DriveManager class with unit tests | |
| 3 | Handle encoding (slot in upper 4 bits) | |
| 4 | extn_extfs.cpp: replace s_volume with DriveManager dispatch | |
| 5 | New register commands: PollMount, GetVolName, RegVersion change | |
| 6 | Config loader `--drive` flag and boot-time mount | |
| 7 | Debugger `drive` command | |
| 8 | Drag-and-drop directory mount | |
| 9 | Guest INIT: multi-VCB bootstrap and jGNEFilter polling | |
| 10 | End-to-end smoke tests | |

Build gate: `cmake --preset macos && cmake --build --preset macos`
Test gate:  `./bld/macos/tests`

---

## Design Discrepancies Found

Before starting, these differences between the design and the current
codebase should be noted:

1. **`closeAllForks()` does not exist** on HostVolume.  Individual
   `closeFork(handle)` exists.  Phase 1 adds a `closeAllForks()`
   method.
2. **No `--drive` or `--disk` named flag** in config_loader.cpp.
   Disk images use positional arguments (`lc.diskPaths`).  Phase 6
   adds the `--drive` flag.
3. **No jGNEFilter** in `macsrc/shareddrive/init.c`.  The ClipSync
   INIT has one.  Phase 9 adds a jGNEFilter to the SharedDrive INIT.
4. **Handle encoding** — currently handles are plain monotonic
   integers (1, 2, 3…).  Phase 3 adds slot bits in the upper 4 bits.

---

## Phase 1 — HostVolume Identity: Per-Instance Slot

Add per-instance identity to HostVolume so DriveManager can assign
a slot index after construction.  Also add the missing
`closeAllForks()` bulk cleanup method.

### 1.1 — Move constants to drive_manager.h (stub file)

Create `src/storage/drive_manager.h` with only the constants for now
(the full DriveManager class comes in Phase 2):

```cpp
// src/storage/drive_manager.h
#pragma once
#include <cstdint>

namespace storage {

inline constexpr int kMaxDrives    = 8;
inline constexpr int16_t kBaseVRefNum  = 32000;   // vRefNum = -(kBaseVRefNum + slot)
inline constexpr int16_t kBaseDriveNum = 8;        // driveNum = kBaseDriveNum + slot

} // namespace storage
```

### 1.2 — Modify host_volume.h

**File:** `src/storage/host_volume.h`

- Remove `static constexpr int16_t kGuestVRefNum = -32000;` (line 136).
- Remove `static constexpr int16_t kGuestDriveNum = 8;` (line 137).
- Add `#include "storage/drive_manager.h"` for the new constants.
- Add the following public methods:

```cpp
void     setSlot(int slot);
int      slot() const;
int16_t  guestVRefNum() const;   // -(kBaseVRefNum + slot_)
int16_t  guestDriveNum() const;  // kBaseDriveNum + slot_
void     closeAllForks();
```

- Add private member: `int slot_ = 0;`
- Change default for `defaultVRefNum_` to use `guestVRefNum()` after
  slot assignment (set in `setSlot()`).

### 1.3 — Implement in host_volume.cpp

**File:** `src/storage/host_volume.cpp`

```cpp
void HostVolume::setSlot(int slot)
{
    slot_ = slot;
    defaultVRefNum_ = guestVRefNum();
}

int HostVolume::slot() const { return slot_; }

int16_t HostVolume::guestVRefNum() const
{
    return static_cast<int16_t>(-(kBaseVRefNum + slot_));
}

int16_t HostVolume::guestDriveNum() const
{
    return static_cast<int16_t>(kBaseDriveNum + slot_);
}

void HostVolume::closeAllForks()
{
    for (auto &[handle, of] : openForks_) {
        if (of.fp) {
            std::fclose(of.fp);
            of.fp = nullptr;
        }
    }
    openForks_.clear();
}
```

### 1.4 — Update extn_extfs.cpp aliases

**File:** `src/core/extn_extfs.cpp`

Change the local constant aliases (lines 53–54):

```cpp
// Before:
static constexpr int16_t kGuestVRefNum = storage::HostVolume::kGuestVRefNum;
static constexpr int16_t kGuestDriveNum = storage::HostVolume::kGuestDriveNum;

// After:
static constexpr int16_t kGuestVRefNum = -static_cast<int16_t>(storage::kBaseVRefNum);
static constexpr int16_t kGuestDriveNum = storage::kBaseDriveNum;
```

This preserves binary compatibility (slot 0 → same values as before).

### 1.5 — Tests

**File:** `test/test_host_volume.cpp` — add:

```cpp
TEST_CASE("HostVolume: slot identity")
{
    storage::HostVolume vol;
    vol.setSlot(3);
    CHECK(vol.slot() == 3);
    CHECK(vol.guestVRefNum() == -32003);
    CHECK(vol.guestDriveNum() == 11);
}

TEST_CASE("HostVolume: closeAllForks")
{
    TempDir td;
    writeFile(td.path / "test.txt", "hello");
    storage::HostVolume vol;
    vol.mount(td.path);
    auto *e = vol.findByName(storage::HostVolume::kRootDirID, "test.txt");
    REQUIRE(e != nullptr);
    uint32_t size = 0;
    storage::OSErr err;
    uint32_t h = vol.openFork(e->cnid, storage::ForkType::Data, size, err);
    CHECK(h != 0);
    vol.closeAllForks();
    // After closeAllForks, closing the same handle should be safe (no-op)
}
```

### Fence

- [ ] `kGuestVRefNum` / `kGuestDriveNum` removed from HostVolume
- [ ] `kBaseVRefNum`, `kBaseDriveNum`, `kMaxDrives` in `drive_manager.h`
- [ ] `setSlot()`, `slot()`, `guestVRefNum()`, `guestDriveNum()`, `closeAllForks()` implemented
- [ ] Existing tests still pass (slot 0 produces same values as old constants)
- [ ] New tests pass: `./bld/macos/tests --test-case="*slot*"`
- [ ] Full build clean
- [ ] Commit: `"shareddrive: phase 1 — per-instance slot identity on HostVolume"`

---

## Phase 2 — DriveManager Class with Unit Tests

The central slot manager.  At this point it is self-contained and
testable without touching any other code.

### 2.1 — Complete drive_manager.h

**File:** `src/storage/drive_manager.h` (extend the stub from Phase 1)

Add the full `DriveManager` class as specified in Design §2.1.
Key interface:

```cpp
class DriveManager {
public:
    static constexpr int kMaxDrives = storage::kMaxDrives;

    int  mount(const std::filesystem::path &hostDir);
    bool unmount(int slot);

    HostVolume       *volume(int slot);
    const HostVolume *volume(int slot) const;
    HostVolume       *volumeByVRefNum(int16_t vRefNum);
    const HostVolume *volumeByVRefNum(int16_t vRefNum) const;
    HostVolume       *volumeByDriveNum(int16_t driveNum);
    int               slotFromVRefNum(int16_t vRefNum) const;
    int               mountedCount() const;
    std::string_view  volumeName(int slot) const;
    const std::filesystem::path &hostPath(int slot) const;
    int               openForkCount(int slot) const;

    template <typename Fn>
    void forEach(Fn &&fn) const;

    int  popPendingMount();
    void queuePendingMount(int slot);

private:
    struct Slot {
        std::optional<HostVolume> vol;
        std::string volumeName;
    };
    std::array<Slot, kMaxDrives> slots_;
    std::vector<int> pendingMounts_;

    std::string deduplicateName(std::string_view baseName) const;
};
```

### 2.2 — Implement drive_manager.cpp

**New file:** `src/storage/drive_manager.cpp` (~250 lines)

Key algorithms:

- `mount()`: find first empty slot, construct HostVolume in-place via
  `slots_[i].vol.emplace()`, call `vol.mount(hostDir)`, call
  `vol.setSlot(i)`, compute volume name via `deduplicateName()`,
  call `queuePendingMount(i)`, return slot index.
- `unmount()`: call `vol.closeAllForks()`, reset the optional
  (`slots_[i].vol.reset()`), clear volume name.
- `slotFromVRefNum()`: `slot = -(vRefNum) - kBaseVRefNum`; bounds
  check `[0, kMaxDrives)`.
- `deduplicateName()`: truncate to 27 chars, scan existing slot names
  for collisions, append `-2`, `-3`, etc.
- `popPendingMount()` / `queuePendingMount()`: simple vector front/back.
- `forEach()`: iterate slots_, skip nullopt, call fn(i, *vol).

### 2.3 — CMakeLists.txt

**File:** `CMakeLists.txt`

Add `src/storage/drive_manager.cpp` to `MINIVMAC_SOURCES` after
`src/storage/host_volume.cpp` (around line 59).

Add `src/storage/drive_manager.cpp` to the `tests` target source list
(after `src/storage/host_volume.cpp`, around line 289).

### 2.4 — Tests

**New file:** `test/test_drive_manager.cpp`

Add to `tests` target in CMakeLists.txt.

```cpp
#include <doctest/doctest.h>
#include "storage/drive_manager.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace {
fs::path makeTempDir(const char *name) {
    auto p = fs::temp_directory_path() / name;
    fs::remove_all(p);
    fs::create_directories(p);
    return p;
}
struct TempDir {
    fs::path path;
    TempDir(const char *n) : path(makeTempDir(n)) {}
    ~TempDir() { fs::remove_all(path); }
};
} // namespace

TEST_CASE("DriveManager: MountOne") { /* mount returns 0; volume(0) non-null */ }
TEST_CASE("DriveManager: MountMax") { /* mount 8; 9th returns -1 */ }
TEST_CASE("DriveManager: Unmount") { /* unmount releases; volume returns null */ }
TEST_CASE("DriveManager: Remount") { /* post-unmount, mount reuses freed slot */ }
TEST_CASE("DriveManager: VRefNumLookup") { /* slotFromVRefNum(-32000..-32007) → 0..7 */ }
TEST_CASE("DriveManager: NameDedup") { /* two dirs "tmp" → "tmp", "tmp-2" */ }
TEST_CASE("DriveManager: PendingQueue") { /* queue/pop FIFO order */ }
```

Each test case creates real temp directories on disk and mounts them.

### Fence

- [ ] `drive_manager.h` and `drive_manager.cpp` exist and compile
- [ ] All 7 DriveManager test cases pass
- [ ] Existing tests unaffected
- [ ] Full build clean
- [ ] Commit: `"shareddrive: phase 2 — DriveManager class with unit tests"`

---

## Phase 3 — Handle Encoding (Slot in Upper 4 Bits)

Encode the slot index in the upper 4 bits of every handle so that
register-based I/O commands (Read, Write, Close, SetEOF) can route
to the correct volume in O(1).

### 3.1 — Add handle encoding helpers

**File:** `src/storage/drive_manager.h`

Add free helper functions:

```cpp
namespace storage {

inline constexpr uint32_t EncodeHandle(int slot, uint32_t localHandle)
{
    return (static_cast<uint32_t>(slot) << 28) | (localHandle & 0x0FFF'FFFF);
}

inline constexpr int SlotFromHandle(uint32_t handle)
{
    return static_cast<int>(handle >> 28);
}

inline constexpr uint32_t LocalHandle(uint32_t handle)
{
    return handle & 0x0FFF'FFFF;
}

} // namespace storage
```

### 3.2 — DriveManager: encode handles on open, decode on I/O

**File:** `src/storage/drive_manager.cpp`

Add a public method on DriveManager for opening a fork that wraps
HostVolume::openFork() and encodes the slot:

```cpp
uint32_t DriveManager::openFork(int slot, uint32_t cnid,
                                 ForkType fork, uint32_t &outSize,
                                 OSErr &errOut, uint8_t perm)
{
    auto *vol = volume(slot);
    if (!vol) { errOut = kNsvErr; return 0; }
    uint32_t local = vol->openFork(cnid, fork, outSize, errOut, perm);
    if (local == 0) return 0;
    return EncodeHandle(slot, local);
}
```

Add a `resolveHandle()` helper to decode:

```cpp
std::pair<HostVolume*, uint32_t> DriveManager::resolveHandle(uint32_t handle) const
{
    int slot = SlotFromHandle(handle);
    uint32_t local = LocalHandle(handle);
    auto *vol = const_cast<DriveManager*>(this)->volume(slot);
    return {vol, local};
}
```

### 3.3 — Tests

**File:** `test/test_drive_manager.cpp` — add:

```cpp
TEST_CASE("DriveManager: HandleEncoding")
{
    CHECK(storage::SlotFromHandle(storage::EncodeHandle(3, 42)) == 3);
    CHECK(storage::LocalHandle(storage::EncodeHandle(3, 42)) == 42);
    CHECK(storage::EncodeHandle(0, 1) == 1);  // slot 0 is pass-through
    CHECK(storage::SlotFromHandle(0x70000005) == 7);
    CHECK(storage::LocalHandle(0x70000005) == 5);
}
```

### Fence

- [ ] `EncodeHandle`, `SlotFromHandle`, `LocalHandle` in `drive_manager.h`
- [ ] `DriveManager::openFork()` and `resolveHandle()` implemented
- [ ] Handle encoding test passes
- [ ] Full build clean
- [ ] Commit: `"shareddrive: phase 3 — handle encoding with slot bits"`

---

## Phase 4 — extn_extfs.cpp: Replace s_volume with DriveManager

This is the largest and highest-risk phase.  Replace the singleton
`s_volume` with `s_drives` and route every handler through the
DriveManager.

### 4.1 — Replace the static instance

**File:** `src/core/extn_extfs.cpp`

```cpp
// Before:
static storage::HostVolume s_volume;

// After:
#include "storage/drive_manager.h"
static storage::DriveManager s_drives;
```

Remove the old constant aliases and replace with:

```cpp
static constexpr int16_t kGuestVRefNum  = -static_cast<int16_t>(storage::kBaseVRefNum);
static constexpr int16_t kGuestDriveNum = storage::kBaseDriveNum;
```

### 4.2 — Add volume resolution helpers

Add two static helpers near the top of the file:

```cpp
// Resolve a PB's ioVRefNum to a HostVolume*.  Returns nullptr + sets
// errOut = kNsvErr if the volume is not ours.
static storage::HostVolume *volumeFromPB(PBRef pb, bool isHFS,
                                          storage::OSErr &errOut)
{
    int16_t vRefNum = pb[ioVRefNum];
    int slot = s_drives.slotFromVRefNum(vRefNum);
    if (slot < 0) { errOut = storage::kNsvErr; return nullptr; }
    auto *vol = s_drives.volume(slot);
    if (!vol) { errOut = storage::kNsvErr; return nullptr; }
    errOut = storage::kNoErr;
    return vol;
}

// Resolve a handle to (HostVolume*, localHandle).
static std::pair<storage::HostVolume*, uint32_t>
volumeFromHandle(uint32_t handle)
{
    return s_drives.resolveHandle(handle);
}
```

### 4.3 — Update pbResolveDir

```cpp
// Before:
static uint32_t pbResolveDir(PBRef pb, bool isHFS)
{
    int16_t vRefNum = pb[ioVRefNum];
    uint32_t dirID = isHFS ? static_cast<uint32_t>(pb[ioDrDirID]) : 0;
    return s_volume.resolveDir(vRefNum, dirID);
}

// After — takes a HostVolume* parameter:
static uint32_t pbResolveDir(PBRef pb, bool isHFS, storage::HostVolume &vol)
{
    int16_t vRefNum = pb[ioVRefNum];
    uint32_t dirID = isHFS ? static_cast<uint32_t>(pb[ioDrDirID]) : 0;
    return vol.resolveDir(vRefNum, dirID);
}
```

### 4.4 — Update PB-based handlers

Every PB handler gains a volume resolution step at the top.  Example
for `PbGetCatInfo`:

```cpp
static OSErr PbGetCatInfo(PBRef pb, bool isHFS)
{
    OSErr err;
    auto *vol = volumeFromPB(pb, isHFS, err);
    if (!vol) return err;

    uint32_t dirID = pbResolveDir(pb, isHFS, *vol);
    // ... rest uses *vol instead of s_volume ...
}
```

Apply the same pattern to all PB handlers: `PbGetFileInfo`,
`PbOpen`, `PbOpenRF`, `PbCreate`, `PbDelete`, `PbRename`,
`PbSetFileInfo`, `PbSetCatInfo`, `PbDirCreate`, `PbCatMove`,
`PbOpenWD`, `PbCloseWD`, `PbGetWDInfo`, `PbSetDefaultVRefNum`.

For `PbOpenFork`: after calling `vol->openFork()`, encode the
handle using `EncodeHandle(vol->slot(), localHandle)`.

### 4.5 — Update register-based handlers

**RegRead, RegWrite, RegClose, RegSetEOF:** decode via
`volumeFromHandle(regParam[0])`:

```cpp
static void RegRead(uint32_t regParam[], uint16_t &regResult)
{
    auto [vol, local] = volumeFromHandle(regParam[0]);
    if (!vol) { regResult = storage::kRfNumErr; return; }
    // ... use vol->readFork(local, ...) instead of s_volume.readFork(handle, ...)
}
```

**RegGetVol, RegOpenWD, RegGetWDInfo, RegCloseWD:** these currently
operate on the single volume.  For multi-drive, regParam must carry
a vRefNum or slot identifier.  Check the guest INIT — the register
commands already pass vRefNum in a register field.  Route through
`s_drives.volumeByVRefNum()`.

**RegVersion:** rewrite per Design §3.3 — return `mountedCount()`,
no auto-mount.

### 4.6 — Update PbGetVolInfo

`PbGetVolInfo` (register-based, returns volume stats) must resolve
which volume is being queried.  Currently it hardcodes "Shared" as
the name.  Change to use `s_drives.volumeName(slot)`.

### 4.7 — Implement ExtFSMountDrive / ExtFSUnmountDrive

**File:** `src/core/extn_extfs.h` — add:

```cpp
#include <filesystem>

void ExtnExtFSDispatch(uint16_t cmd, uint32_t regParam[], uint16_t &regResult);
int  ExtFSMountDrive(const std::filesystem::path &hostDir);
bool ExtFSUnmountDrive(int slot);
```

**File:** `src/core/extn_extfs.cpp` — implement:

```cpp
int ExtFSMountDrive(const std::filesystem::path &hostDir)
{
    return s_drives.mount(hostDir);
}

bool ExtFSUnmountDrive(int slot)
{
    return s_drives.unmount(slot);
}
```

### Fence

- [ ] `s_volume` fully replaced by `s_drives`
- [ ] All PB and register handlers route through DriveManager
- [ ] `ExtFSMountDrive()` and `ExtFSUnmountDrive()` exported
- [ ] Handle encoding/decoding wired into open/read/write/close paths
- [ ] Full build clean (no references to `s_volume` remain)
- [ ] Existing unit tests still pass
- [ ] Commit: `"shareddrive: phase 4 — replace singleton with DriveManager dispatch"`

---

## Phase 5 — New Register Commands: PollMount, GetVolName

Add the two guest-facing register commands that let the INIT
discover drives mounted at runtime.

### 5.1 — Add command codes

**File:** `src/core/extn_extfs.cpp`

```cpp
static constexpr uint16_t kExtFSPollMount  = 0x219;
static constexpr uint16_t kExtFSGetVolName = 0x21A;
```

### 5.2 — Implement RegPollMount

```cpp
static void RegPollMount(uint32_t regParam[], uint16_t &regResult)
{
    int slot = s_drives.popPendingMount();
    if (slot < 0) {
        regParam[0] = 0xFFFFFFFF;
        regResult = 0;
        return;
    }
    auto *vol = s_drives.volume(slot);
    regParam[0] = static_cast<uint32_t>(slot);
    regParam[1] = static_cast<uint32_t>(vol->guestVRefNum());
    regParam[2] = static_cast<uint32_t>(vol->guestDriveNum());
    regResult = 0;
}
```

### 5.3 — Implement RegGetVolName

Pack the volume name (Pascal string) into regParam[0]..regParam[3]
big-endian (up to 27 chars = 28 bytes with length byte = 7 uint32s,
but only 4 available — limit to 15 chars in registers; alternatively,
write the name to a guest buffer address passed in regParam[1]).

Design §3.2 says "packed big-endian into register words (up to 27 chars)."
With 4 register words (16 bytes), the effective Pascal string capacity
is 15 characters.  Volume names up to 15 chars fit directly.  For
longer names, the guest must pass a buffer pointer.

Simpler approach: use regParam[1] as a guest buffer address if
non-zero, and write the Pascal string there via `writePascalString()`.

```cpp
static void RegGetVolName(uint32_t regParam[], uint16_t &regResult)
{
    int slot = static_cast<int>(regParam[0]);
    uint32_t guestBuf = regParam[1];
    auto name = s_drives.volumeName(slot);
    if (name.empty()) { regResult = storage::kNsvErr; return; }
    if (guestBuf != 0)
        writePascalString(guestBuf, std::string(name));
    regResult = 0;
}
```

### 5.4 — Update RegVersion

```cpp
static void RegVersion(uint32_t regParam[], uint16_t &regResult)
{
    regParam[0] = static_cast<uint32_t>(s_drives.mountedCount());
    regResult = 0;
    DIAG(ExtFS, "version query → %u drives\n", regParam[0]);
}
```

No more auto-mount of "shared".

### 5.5 — Wire into dispatch switch

Add cases to `ExtnExtFSDispatch()`:

```cpp
case kExtFSPollMount:
    RegPollMount(regParam, regResult);
    break;
case kExtFSGetVolName:
    RegGetVolName(regParam, regResult);
    break;
```

### Fence

- [ ] `kExtFSPollMount` (0x219) and `kExtFSGetVolName` (0x21A) implemented
- [ ] `RegVersion` returns mounted count (no auto-mount)
- [ ] Full build clean
- [ ] Commit: `"shareddrive: phase 5 — PollMount, GetVolName register commands"`

---

## Phase 6 — Config Loader `--drive` Flag

### 6.1 — Add drivePaths to LaunchConfig

**File:** `src/core/config_loader.h`

Add to `LaunchConfig` struct:

```cpp
std::vector<std::string> drivePaths;  // --drive <path> (repeatable)
```

### 6.2 — Parse --drive

**File:** `src/core/config_loader.cpp`

In the argument parsing loop, alongside existing flag handling:

```cpp
if (arg == "--drive" && i + 1 < argc) {
    lc.drivePaths.push_back(argv[++i]);
    continue;
}
```

Add `--drive <path>` to the help text.

### 6.3 — Mount at boot

**File:** `src/core/main.cpp` (or wherever `LaunchConfig` is consumed
to start emulation — verify the exact location).

After the config is loaded and before the emulation loop starts,
mount each drive path:

```cpp
for (auto &dp : lc.drivePaths) {
    int slot = ExtFSMountDrive(dp);
    if (slot < 0)
        DIAG(ExtFS, "failed to mount drive: %s\n", dp.c_str());
    else
        DIAG(ExtFS, "mounted drive slot %d: %s\n", slot, dp.c_str());
}
```

Verify the boot sequence: find where `LaunchConfig` fields like
`diskPaths` are consumed and mount drives at the same point.

### Fence

- [ ] `--drive` accepted on command line, can be repeated
- [ ] Each `--drive` path mounted via `ExtFSMountDrive()` at boot
- [ ] `--help` shows `--drive` option
- [ ] Full build clean
- [ ] Commit: `"shareddrive: phase 6 — --drive CLI flag and boot-time mount"`

---

## Phase 7 — Debugger `drive` Command

### 7.1 — Create cmd_drive.cpp

**New file:** `src/debugger/cmd_drive.cpp` (~120 lines)

```cpp
#include "debugger/debugger.h"
#include "core/extn_extfs.h"
#include "storage/drive_manager.h"
#include <filesystem>

void CmdDrive(Debugger &dbg, const std::vector<Token> &args)
{
    auto &io = dbg.io();
    if (args.size() < 2) { /* print usage */ return; }

    if (args[1].text == "mount" && args.size() >= 3) {
        int slot = ExtFSMountDrive(args[2].text);
        if (slot < 0)
            io.printf("mount failed: path invalid or no free slot\n");
        else
            io.printf("mounted slot %d\n", slot);
    }
    else if (args[1].text == "unmount" && args.size() >= 3) {
        int slot = std::atoi(std::string(args[2].text).c_str());
        if (!ExtFSUnmountDrive(slot))
            io.printf("slot %d is not mounted\n", slot);
        else
            io.printf("unmounted slot %d\n", slot);
    }
    else if (args[1].text == "list") {
        // Use a DriveManager accessor exposed via extn_extfs.h
        // or call a list helper.
        io.printf(" Slot  Volume      Host path            Forks\n");
        ExtFSDriveList(io);  // new helper
    }
    else {
        io.printf("usage: drive mount <path> | unmount <slot> | list\n");
    }
}
```

For the `list` subcommand, add a helper `ExtFSDriveList(DbgIO &io)` in
`extn_extfs.cpp` that calls `s_drives.forEach()` to print the table.
Declare it in `extn_extfs.h`.

### 7.2 — Register in debugger.cpp

**File:** `src/debugger/debugger.cpp`

Add forward declaration and entry to `s_commands[]`:

```cpp
extern void CmdDrive(Debugger &, const std::vector<Token> &);

// In s_commands[]:
{"drive", "", CmdDrive, "Mount/unmount shared drives",
 "drive mount <path>\n  Mount a host directory as a new shared drive.\n"
 "drive unmount <slot>\n  Unmount and release shared drive by slot number.\n"
 "drive list\n  Show all mounted shared drives.\n"},
```

### 7.3 — CMakeLists.txt

Add `src/debugger/cmd_drive.cpp` to `MINIVMAC_SOURCES` in the
Debugger section (after `src/debugger/cmd_help.cpp`, around line 87).

### Fence

- [ ] `drive mount`, `drive unmount`, `drive list` commands work
- [ ] Registered in `s_commands[]`
- [ ] `cmd_drive.cpp` compiles clean
- [ ] Full build clean
- [ ] Commit: `"shareddrive: phase 7 — debugger drive command"`

---

## Phase 8 — Drag-and-Drop Directory Mount

### 8.1 — Modify emulator_shell.cpp

**File:** `src/platform/emulator_shell.cpp`

Add `#include <filesystem>` and `#include "core/extn_extfs.h"`.

Change the FileDrop handler:

```cpp
case PlatformEvent::Type::FileDrop:
    if (evt.filePath)
    {
        if (std::filesystem::is_directory(evt.filePath))
            ExtFSMountDrive(evt.filePath);
        else
            (void)Sony_Insert1a_impl(const_cast<char *>(evt.filePath), false);
    }
    break;
```

### Fence

- [ ] Dropping a directory on the window calls `ExtFSMountDrive()`
- [ ] Dropping a file still inserts a Sony disk
- [ ] Full build clean
- [ ] Commit: `"shareddrive: phase 8 — drag-and-drop directory mount"`

---

## Phase 9 — Guest INIT: Multi-VCB Bootstrap and jGNEFilter

This phase modifies the guest-side 68k C code.  It is built with the
vintage THINK C toolchain (or whatever cross-compiler the project uses
for `macsrc/`).  Changes here are described conceptually; the exact
syntax depends on the toolchain.

### 9.1 — Expand Globals struct

**File:** `macsrc/shareddrive/init.c`

```c
// Before:
typedef struct {
    char    *regBase;
    Ptr      vcb;
    Ptr      dqe;
    long     volFileCount;
    long     volTotalBytes;
    long     savedA4;
    short    rootWDRefNum;
    short    defaultWDRefNum;
    short    ejected;
} Globals;

// After:
#define kMaxDrives  8

typedef struct {
    char    *regBase;
    Ptr      vcb[kMaxDrives];
    Ptr      dqe[kMaxDrives];
    short    driveCount;
    long     savedA4;
    short    defaultWDRefNum;
    short    ejected;
    long     oldFilter;       /* previous jGNEFilter */
    long     lastPollTick;    /* TickCount at last poll */
} Globals;
```

Remove `rootWDRefNum`, `volFileCount`, `volTotalBytes` if unused
elsewhere (verify before removal).

### 9.2 — Update constants

```c
#define kBaseVRefNum     32000
#define kBaseDriveNum    8
#define kMaxDrives       8

// Legacy single-drive aliases for backward compat:
#define kOurVRefNum      (-(kBaseVRefNum))
#define kOurDriveNum     (kBaseDriveNum)

// New command codes:
#define kExtFSPollMount  0x219
#define kExtFSGetVolName 0x21A
```

### 9.3 — Rewrite IsOurVolume

Per Design §5.4:

```c
static Boolean IsOurVolume(short vRefNum)
{
    if (vRefNum <= -kBaseVRefNum
        && vRefNum > -(kBaseVRefNum + kMaxDrives))
        return true;

    if (vRefNum >= kBaseDriveNum
        && vRefNum < kBaseDriveNum + kMaxDrives)
        return true;

    if (vRefNum < -kBaseVRefNum
        && vRefNum > -(kBaseVRefNum + kMaxDrives * 100))
        return true;  /* WD refnums */

    if (vRefNum == 0) {
        Globals *g = get_globals();
        return (g != NULL && IsOurVCB(*(Ptr *)kDefVCBPtr, g));
    }
    return false;
}
```

Add `IsOurVCB` helper:

```c
static Boolean IsOurVCB(Ptr vcb, Globals *g)
{
    short i;
    for (i = 0; i < g->driveCount; i++)
        if (g->vcb[i] == vcb) return true;
    return false;
}
```

### 9.4 — Multi-VCB allocation at INIT time

Replace the single VCB/DQE allocation with a loop over
`driveCount` returned by `kExtFSVersion`:

```c
/* Get number of mounted drives from host */
short driveCount = (short)regParam[0];  /* from kExtFSVersion reply */
g->driveCount = driveCount;

for (short i = 0; i < driveCount; i++) {
    /* Get volume name from host */
    regParam[0] = i;
    regParam[1] = (unsigned long)nameBuf;  /* guest buffer pointer */
    call_extfs(kExtFSGetVolName, regParam, &result);

    /* Allocate VCB */
    g->vcb[i] = NewPtrSysClear(178);
    if (g->vcb[i] == NULL) continue;

    Ptr v = g->vcb[i];
    short vRefNum = -(kBaseVRefNum + i);
    short driveNum = kBaseDriveNum + i;

    /* Fill VCB fields (same pattern as current single-VCB code) */
    *(short *)(v + 4)  = 1;               /* qType = fsQType */
    *(short *)(v + 8)  = 0x4244;          /* vcbSigWord = HFS */
    /* ... copy name, set vRefNum, driveNum ... */
    *(short *)(v + 72) = driveNum;
    *(short *)(v + 78) = vRefNum;

    Enqueue((QElemPtr)g->vcb[i], (QHdrPtr)kVCBQHdr);

    /* Allocate DQE and call AddDrive */
    g->dqe[i] = NewPtrSysClear(20);
    /* ... fill and AddDrive ... */
}
```

### 9.5 — Add jGNEFilter for runtime mount polling

Model after `macsrc/clipsync/init.c` FilterEntry:

```c
#define kJGNEFilter  0x029A

static pascal Boolean FilterEntry(EventRecord *evt, Boolean result)
{
    long oldA4;
    Globals *g;

    oldA4 = SetA4(/* saved A4 */);
    g = get_globals();

    /* Throttle: once per 60 ticks (~1 sec) */
    if (TickCount() - g->lastPollTick < 60) goto chain;
    g->lastPollTick = TickCount();

    /* Poll for new drives */
    {
        unsigned long regParam[4];
        unsigned short res;
        call_extfs(kExtFSPollMount, regParam, &res);
        if (regParam[0] != 0xFFFFFFFF) {
            short slot = (short)regParam[0];
            short vRefNum = (short)regParam[1];
            short driveNum = (short)regParam[2];
            allocateVCBAndDQE(g, slot, vRefNum, driveNum);
            PostEvent(diskEvt, driveNum);
        }
    }

chain:
    SetA4(oldA4);
    /* Chain to previous filter */
    if (g->oldFilter != 0)
        return CallGNEFilterProc(g->oldFilter, evt, result);
    return result;
}
```

Install the filter at INIT time:

```c
g->oldFilter = *(long *)kJGNEFilter;
*(long *)kJGNEFilter = (long)FilterEntry;
```

### 9.6 — Per-drive WD encoding

Update WD refnum allocation in the trap patches to encode the slot:

```c
// WD refnum = -(kBaseVRefNum + slot * 100 + localWDRef)
```

This range (-32000 to -32799) stays within what IsOurVolume accepts.

### Fence

- [ ] Globals struct expanded to arrays
- [ ] IsOurVolume handles multi-drive range
- [ ] INIT allocates N VCBs from kExtFSVersion count
- [ ] jGNEFilter installed, polls kExtFSPollMount
- [ ] Runtime mount creates VCB + DQE and posts diskEvt
- [ ] Guest INIT builds with vintage toolchain
- [ ] Commit: `"shareddrive: phase 9 — guest INIT multi-VCB and jGNEFilter"`

---

## Phase 10 — End-to-End Smoke Tests

### 10.1 — Boot with two shared drives

Create a debugger script `debug_multidrive.dbg`:

```
# Mount two directories before boot
drive mount ./test_dir_a
drive mount ./test_dir_b
run
# After boot, verify both appear
drive list
```

Create two test directories with known files:

```bash
mkdir -p test_dir_a test_dir_b
echo "hello" > test_dir_a/file_a.txt
echo "world" > test_dir_b/file_b.txt
```

Boot the emulation and confirm both volumes appear on the Finder
desktop.

### 10.2 — CLI --drive test

```bash
./bld/macos/maxivmac --drive test_dir_a --drive test_dir_b <disk.hfs>
```

Verify host logs show two mounts at slot 0 and slot 1.

### 10.3 — Runtime mount via debugger

With the emulator running:

```
drive mount /tmp
drive list
drive unmount 2
drive list
```

Verify the volume appears and disappears from `drive list`.

### 10.4 — Verify handle isolation

Open files from two different drives, read/write, close.  Verify
handles from drive 0 don't collide with handles from drive 1.

### 10.5 — Verify unmount cleanup

Mount a drive, open a file, unmount without closing.  Verify:
- `closeAllForks()` ran (no leaked FILE* handles)
- The slot is freed for reuse

### Fence

- [ ] Two-drive boot test passes
- [ ] CLI `--drive` works
- [ ] Debugger `drive mount / unmount / list` works at runtime
- [ ] Handle isolation verified
- [ ] Unmount cleanup verified
- [ ] All unit tests pass: `./bld/macos/tests`
- [ ] Full build clean
- [ ] Commit: `"shareddrive: phase 10 — end-to-end multi-drive smoke tests"`
