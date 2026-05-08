# INIT Versioning — Implementation Plan

Spec: [INIT_VERSION.md](INIT_VERSION.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Host: `extn_system.cpp` with `InitInfo` class and `$0xx` dispatch route | |
| 2 | Host: overlay display of INIT status | |
| 3 | Host: session-reset on `InitIdent` (clear KV, clip state) | |
| 4 | **MANUAL CHECK** — host builds, overlay shows "INIT: not loaded" | |
| 5 | Guest: `version.h.default`, `kCmdInitIdent` constant, Globals changes | |
| 6 | Guest: new boot sequence with Shift-skip, `HomeResFile`, `SysEnvirons`, `kCmdInitIdent` | |
| 7 | Guest: unconditional trap install, remove version checks, reduce poll interval | |
| 8 | Guest: add `dbg_log` to clipboard error paths | |
| 9 | **MANUAL CHECK** — compile INIT in THINK C, boot, verify overlay shows version | |
| 10 | Build pipeline: `version.h.default`, `.gitignore`, `build.sh` | |
| 11 | **MANUAL CHECK** — full end-to-end: build.sh, boot, overlay, stale detection | |

Build gate: `cmake --preset macos && cmake --build --preset macos`
Test gate:  `./bld/macos/tests`

---

## Phase 1 — Host: `extn_system.cpp` with `InitInfo` and `$0xx` dispatch

Create the host-side handler for the new `$0xx` system command range.
This phase adds the dispatch route and the `InitInfo` class but
nothing calls it yet — the overlay and guest changes come later.

### 1.1 — Create `src/core/extn_system.h`

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

/*
    System extension handler for the register-block interface.
    Called from regDispatch() in machine.cpp when command codes
    $000–$0FF are written to the register block.
*/

class InitInfo {
public:
    bool        loaded() const        { return loaded_; }
    std::string_view version() const  { return version_; }
    int         apiVersion() const    { return apiVersion_; }
    bool        isStale() const;

    // Guest environment
    int         machineType() const   { return machineType_; }
    int         systemVersion() const { return systemVersion_; }

    // File spec (for future auto-update)
    int16_t     vRefNum() const       { return vRefNum_; }
    int32_t     dirID() const         { return dirID_; }
    std::string_view fileName() const { return fileName_; }

    // Called by ExtnSystemDispatch — not for external use.
    void populate(int apiVer, std::string_view version,
                  int16_t vRefNum, int32_t dirID,
                  std::string_view fileName,
                  int machineType, int systemVersion);
    void reset();

private:
    bool        loaded_ = false;
    std::string version_;
    int         apiVersion_ = 0;
    int16_t     vRefNum_ = 0;
    int32_t     dirID_ = 0;
    std::string fileName_;
    int         machineType_ = 0;
    int         systemVersion_ = 0;
};

void ExtnSystemDispatch(uint16_t cmd, uint32_t regParam[],
                        uint16_t &regResult);
const InitInfo& ExtnSystemInitInfo();
```

### 1.2 — Create `src/core/extn_system.cpp`

```cpp
#include "core/extn_system.h"
#include "core/diag.h"
#include "platform/platform_config.h"  // MAXIVMAC_VERSION

extern uint8_t get_vm_byte(uint32_t addr);

static constexpr uint16_t kCmdInitIdent = 0x0001;

// API version range the host accepts.
static constexpr int kMinApiVersion = 1;
static constexpr int kMaxApiVersion = 1;

static InitInfo s_initInfo;

// --- InitInfo implementation ---

bool InitInfo::isStale() const
{
    return loaded_ && version_ != MAXIVMAC_VERSION;
}

void InitInfo::populate(int apiVer, std::string_view version,
                        int16_t vRefNum, int32_t dirID,
                        std::string_view fileName,
                        int machineType, int systemVersion)
{
    loaded_ = true;
    apiVersion_ = apiVer;
    version_ = version;
    vRefNum_ = vRefNum;
    dirID_ = dirID;
    fileName_ = fileName;
    machineType_ = machineType;
    systemVersion_ = systemVersion;
}

void InitInfo::reset()
{
    loaded_ = false;
    version_.clear();
    apiVersion_ = 0;
    vRefNum_ = 0;
    dirID_ = 0;
    fileName_.clear();
    machineType_ = 0;
    systemVersion_ = 0;
}

// --- Guest Pascal string reader ---

static std::string readGuestPascalStr(uint32_t addr)
{
    if (addr == 0) return {};
    uint8_t len = get_vm_byte(addr);
    std::string s;
    s.reserve(len);
    for (uint8_t i = 0; i < len; i++)
        s.push_back(static_cast<char>(get_vm_byte(addr + 1 + i)));
    return s;
}

// --- Dispatch ---

void ExtnSystemDispatch(uint16_t cmd, uint32_t regParam[],
                        uint16_t &regResult)
{
    switch (cmd)
    {
        case kCmdInitIdent:
        {
            int apiVer = static_cast<int>(regParam[0]);
            std::string version = readGuestPascalStr(regParam[1]);
            auto vRefNum = static_cast<int16_t>(regParam[2]);
            auto dirID = static_cast<int32_t>(regParam[3]);
            std::string fileName = readGuestPascalStr(regParam[4]);
            int machType = static_cast<int>(regParam[5]);
            int sysVer = static_cast<int>(regParam[6]);

            s_initInfo.populate(apiVer, version, vRefNum, dirID,
                                fileName, machType, sysVer);

            bool compatible = (apiVer >= kMinApiVersion &&
                               apiVer <= kMaxApiVersion);
            regResult = compatible ? 0 : 1;

            DIAG(INIT, "InitIdent: api=%d ver=\"%s\" file=\"%s\" "
                 "machine=%d sysVer=$%04X → %s\n",
                 apiVer, version.c_str(), fileName.c_str(),
                 machType, sysVer,
                 compatible ? "OK" : "REJECTED");
            break;
        }
        default:
            regResult = 0xFFFF;
            break;
    }
}

const InitInfo& ExtnSystemInitInfo()
{
    return s_initInfo;
}
```

### 1.3 — Add `$0xx` dispatch route in `machine.cpp`

In `regDispatch()` in `src/core/machine.cpp`, add the `$0xx` route
**before** the existing `$1xx` block.

Add include at top (near the existing extn includes):
```cpp
#include "core/extn_system.h"
```

In `regDispatch()`, change:
```cpp
static void regDispatch(uint16_t cmd)
{
    if (cmd >= 0x100 && cmd <= 0x1FF)
```
to:
```cpp
static void regDispatch(uint16_t cmd)
{
    if (cmd <= 0x0FF)
    {
        ExtnSystemDispatch(cmd, s_regParam, s_regResult);
    }
    else if (cmd >= 0x100 && cmd <= 0x1FF)
```

### 1.4 — Add to CMakeLists.txt

Add `src/core/extn_system.cpp` to the source list, next to the other
`extn_*.cpp` files (after `src/core/extn_extfs.cpp`).

### 1.5 — Register DIAG channel

If the project uses registered diagnostic channels, add `INIT` to
the diagnostic channel list (same file/mechanism as `ExtFS` and
`Clip`).  If DIAG uses string-based ad-hoc channels, no registration
is needed.

### Fence

- [ ] `src/core/extn_system.h` exists with `InitInfo` class
- [ ] `src/core/extn_system.cpp` exists with `kCmdInitIdent` handler
- [ ] `regDispatch()` routes `$0xx` to `ExtnSystemDispatch`
- [ ] `CMakeLists.txt` includes `extn_system.cpp`
- [ ] Full build clean: `cmake --preset macos && cmake --build --preset macos`
- [ ] Tests pass: `./bld/macos/tests`
- [ ] Commit: `"init-version: phase 1 — extn_system with InitInfo class"`

---

## Phase 2 — Host: overlay display of INIT status

Add the INIT status line to the control overlay.  Since no INIT has
been updated yet, this will always show "INIT: not loaded".

### 2.1 — Add INIT status to `imgui_overlay.cpp`

In `src/platform/imgui_overlay.cpp`, in `drawAbout()` (after the
existing `maxivmac VERSION` line), add the INIT status display.

Add include:
```cpp
#include "core/extn_system.h"
```

After the `ImGui::TextDisabled("maxivmac %s", MAXIVMAC_VERSION);`
line, add:

```cpp
{
    const auto& info = ExtnSystemInitInfo();
    if (!info.loaded())
    {
        ImGui::TextDisabled("INIT: not loaded");
    }
    else if (info.isStale())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f),
                           "INIT: %s", std::string(info.version()).c_str());
    }
    else
    {
        ImGui::Text("INIT: %s", std::string(info.version()).c_str());
    }
}
```

### 2.2 — Add guest environment display

In the same block, when the INIT is loaded, also display the machine
type and system version.  Use a helper to format the BCD system
version (e.g., `0x0701` → `"7.0.1"`):

```cpp
// In extn_system.h or as a local helper:
static std::string formatSystemVersion(int bcd)
{
    int major = (bcd >> 8) & 0xFF;
    int minor = (bcd >> 4) & 0x0F;
    int patch = bcd & 0x0F;
    char buf[16];
    if (patch)
        snprintf(buf, sizeof(buf), "%d.%d.%d", major, minor, patch);
    else
        snprintf(buf, sizeof(buf), "%d.%d", major, minor);
    return buf;
}
```

Machine type names — use a static lookup based on the SysEnvirons
`machineType` constants (from Inside Macintosh IV / `SysEnvirons.h`).
These values come from the ROM, so this displays what the guest
actually reports about itself — not what the host thinks it launched:
```cpp
static std::string machineTypeName(int type)
{
    switch (type)
    {
        case 1:  return "Mac 512Ke";
        case 2:  return "Mac Plus";
        case 3:  return "Mac SE";
        case 5:  return "Mac II";
        case 6:  return "Mac IIx";
        case 7:  return "Mac IIcx";
        case 9:  return "Mac SE/30";
        case 11: return "Mac IIci";
        default: return std::format("Mac (type {})", type);
    }
}
```

Display as a single line:
```
Mac II · System 7.0.1 · INIT v1.0
```

### Fence

- [ ] Overlay shows "INIT: not loaded" (gray) when no InitIdent
      received
- [ ] Code compiles clean
- [ ] Commit: `"init-version: phase 2 — overlay INIT status display"`

---

## Phase 3 — Host: session-reset on InitIdent

When the host receives `kCmdInitIdent`, it means the guest has
(re)booted.  Clear stale state in all subsystems.

### 3.1 — Add reset functions to `extn_clip.h` / `extn_clip.cpp`

In `extn_clip.h`, add:
```cpp
void ExtnClipReset();
```

In `extn_clip.cpp`, implement:
```cpp
void ExtnClipReset()
{
    s_kvStore.clear();
    s_clipSeqNo = 0;
    s_clipCache.clear();
    s_lastClipText.clear();
}
```

### 3.2 — Call reset from `ExtnSystemDispatch`

In `extn_system.cpp`, in the `kCmdInitIdent` handler, **before**
`s_initInfo.populate(...)`:

```cpp
#include "core/extn_clip.h"

// At the top of the kCmdInitIdent case:
s_initInfo.reset();
ExtnClipReset();
```

No drive reset is needed — the drive manager's state (mounted volumes)
is host-authoritative and should persist across guest reboots.

### Fence

- [ ] `ExtnClipReset()` exists and clears KV store + clip state
- [ ] `kCmdInitIdent` handler calls reset before populating
- [ ] Build clean, tests pass
- [ ] Commit: `"init-version: phase 3 — session reset on InitIdent"`

---

## Phase 4 — MANUAL CHECK: host-only verification

> **STOP.  This phase requires manual verification by the developer.**

1. Build the host: `cmake --preset macos && cmake --build --preset macos`
2. Boot a Mac with an existing (old) INIT on the disk image.
3. Verify the overlay shows **"INIT: not loaded"** (gray text) — the
   old INIT does not send `kCmdInitIdent`, so this is expected.
4. Verify shared drives and clipboard still work normally — the old
   `$0200` / `$0100` commands are still handled.
5. Check the debug console for any errors.

**Pass criteria:** Old INIT still works.  Overlay shows "not loaded".
No regressions.

---

## Phase 5 — Guest: version header, new constants, Globals changes

Update the INIT source files to prepare for the new boot sequence.
These are data-only changes — no logic changes yet, so the INIT
still compiles and works as before.

### 5.1 — Create `macsrc/init/version.h.default`

```c
/* Default INIT version — overwritten by build.sh for release builds.
   Copy this file to version.h for manual builds. */
#ifndef VERSION_H
#define VERSION_H
#define kInitVersion "\pdev"
#endif
```

### 5.2 — Create `macsrc/init/version.h` (initial copy)

Copy `version.h.default` to `version.h` so THINK C can find it.
This file is gitignored (see Phase 10).

### 5.3 — Add new constants to `defs.h`

Add near the top of the command codes section (before the `$1xx`
clipboard commands):

```c
/* ---- Host command codes — system range ($0xx) ---- */

#define kCmdInitIdent  0x0001
#define kApiVersion    1
```

### 5.4 — Add file spec fields to Globals in `defs.h`

Add three fields to the `Globals` struct, after the `lastPollTick`
field:

```c
    /* INIT file location (for future auto-update) */
    short       initVRefNum;
    long        initDirID;
    Str63       initFileName;
```

### 5.5 — Change resource ID constant

In `defs.h`, add:
```c
#define kInitResID  128
```

**Do not change `init.c` yet** — that happens in Phase 6.

### Fence

- [ ] `macsrc/init/version.h.default` exists
- [ ] `macsrc/init/version.h` exists (copy of default)
- [ ] `defs.h` has `kCmdInitIdent`, `kApiVersion`, `kInitResID`
- [ ] `Globals` has `initVRefNum`, `initDirID`, `initFileName`
- [ ] INIT still compiles in THINK C (no logic changes)
- [ ] Commit: `"init-version: phase 5 — version header and new constants"`

---

## Phase 6 — Guest: new boot sequence

This is the critical phase.  Rewrite `main()` in `init.c` to follow
the new boot sequence from the spec.  The old `ExtFSVersion` /
`ClipVersion` checks are removed.  The INIT now unconditionally
installs everything if the host accepts the API version.

### 6.1 — Add `#include "version.h"` to `init.c`

At the top of `init.c`, after `#include "defs.h"`:
```c
#include "version.h"
```

### 6.2 — Add Shift-key check helper

Add a static function above `main()`:

```c
static Boolean ShiftKeyDown(void)
{
    KeyMap keys;
    GetKeys(keys);
    /* Shift = bit 56 of the KeyMap (byte 7, bit 0) */
    return (((unsigned char *)keys)[7] & 0x01) != 0;
}
```

### 6.3 — Rewrite `main()`

Replace the entire body of `main()`.  The new flow is:

```c
void main(void)
{
    char *regBase;
    Globals *g;
    Handle self;
    Ptr myINITPtr;
    SysEnvRec env;
    FCBPBRec fcbPB;
    Str63 initName;
    short homeRef;
    unsigned long slot;

    asm { move.l a0, myINITPtr }
    RememberA0();
    SetUpA4();
    DriveRememberA4();

    regBase = find_reg_base();
    if (regBase == NULL) goto bail;

    /* Shift-skip: standard Mac INIT convention */
    if (ShiftKeyDown())
    {
        dbg_log(regBase, "maxivmac INIT: skipped by user (Shift held)");
        goto bail;
    }

    /* Locate our own resource and capture file info BEFORE detach */
    self = GetResource('INIT', kInitResID);
    if (self == NULL)
    {
        dbg_log(regBase, "maxivmac INIT: GetResource failed");
        goto bail;
    }
    homeRef = HomeResFile(self);

    /* Get file spec via PBGetFCBInfo */
    initName[0] = 0;
    mem_zero((char *)&fcbPB, sizeof(fcbPB));
    fcbPB.ioNamePtr = (StringPtr)initName;
    fcbPB.ioRefNum = homeRef;
    fcbPB.ioFCBIndx = 0;  /* use ioRefNum, not index */
    PBGetFCBInfoSync(&fcbPB);

    /* Detach and lock the code resource */
    DetachResource(self);
    HLock(self);
    HNoPurge(self);

    /* Get guest environment */
    SysEnvirons(1, &env);

    dbg_log1(regBase, "maxivmac INIT: regBase=%lx", (unsigned long)regBase);

    /* Allocate globals */
    g = (Globals *)NewPtrSysClear(sizeof(Globals));
    if (g == NULL)
    {
        dbg_log(regBase, "maxivmac INIT: NewPtrSys failed");
        goto bail;
    }
    g->regBase = regBase;
    g->driveCount = 0;
    g->initVRefNum = fcbPB.ioFCBVRefNum;
    g->initDirID = fcbPB.ioFCBParID;
    pstr_copy((char *)g->initFileName, (char *)initName);

    /* Save A4 */
    {
        long *a4dst = &g->savedA4;
        asm {
            move.l a4dst, a0
            move.l a4, (a0)
        }
    }

    set_globals(regBase, g);

    /* ---- Identify to host ---- */
    {
        static char versionStr[] = kInitVersion;

        reg_set(regBase, 0, (unsigned long)kApiVersion);
        reg_set(regBase, 1, (unsigned long)versionStr);
        reg_set(regBase, 2, (unsigned long)g->initVRefNum);
        reg_set(regBase, 3, (unsigned long)g->initDirID);
        reg_set(regBase, 4, (unsigned long)g->initFileName);
        reg_set(regBase, 5, (unsigned long)env.machineType);
        reg_set(regBase, 6, (unsigned long)env.systemVersion);
        reg_command(regBase, kCmdInitIdent);

        if (reg_result(regBase) != 0)
        {
            dbg_log(regBase, "maxivmac INIT: API rejected by host, bailing");
            goto bail;
        }
    }

    dbg_log1(regBase, "maxivmac INIT: version=%s",
             (unsigned long)kInitVersion + 1);  /* skip Pascal length byte */

    /* ---- Install traps (unconditional) ---- */
    InitTrapTables();
    InstallHFSPatch(regBase);
    InstallFlatPatch(0xA000, regBase); /* _Open */
    InstallFlatPatch(0xA001, regBase); /* _Close */
    InstallFlatPatch(0xA002, regBase); /* _Read */
    InstallFlatPatch(0xA003, regBase); /* _Write */
    InstallFlatPatch(0xA007, regBase); /* _GetVolInfo */
    InstallFlatPatch(0xA008, regBase); /* _Create */
    InstallFlatPatch(0xA009, regBase); /* _Delete */
    InstallFlatPatch(0xA00A, regBase); /* _OpenRF */
    InstallFlatPatch(0xA00B, regBase); /* _Rename */
    InstallFlatPatch(0xA00C, regBase); /* _GetFileInfo */
    InstallFlatPatch(0xA00D, regBase); /* _SetFileInfo */
    InstallFlatPatch(0xA00E, regBase); /* _UnmountVol */
    InstallFlatPatch(0xA010, regBase); /* _Allocate */
    InstallFlatPatch(0xA011, regBase); /* _GetEOF */
    InstallFlatPatch(0xA012, regBase); /* _SetEOF */
    InstallFlatPatch(0xA013, regBase); /* _FlushVol */
    InstallFlatPatch(0xA014, regBase); /* _GetVol */
    InstallFlatPatch(0xA015, regBase); /* _SetVol */
    InstallFlatPatch(0xA017, regBase); /* _Eject */
    InstallFlatPatch(0xA018, regBase); /* _GetFPos */
    InstallFlatPatch(0xA044, regBase); /* _SetFPos */
    InstallFlatPatch(0xA045, regBase); /* _FlushFile */
    dbg_log(regBase, "maxivmac INIT: traps patched");

    /* ---- Drain pending mount queue ---- */
    reg_command(regBase, kExtFSPollMount);
    slot = reg_get(regBase, 0);
    while (slot != 0xFFFFFFFFUL)
    {
        short s = (short)slot;
        short vRefNum = (short)reg_get(regBase, 1);
        short driveNum = (short)reg_get(regBase, 2);
        MountNewDrive(g, s, vRefNum, driveNum);
        reg_command(regBase, kExtFSPollMount);
        slot = reg_get(regBase, 0);
    }

    if (g->driveCount > 0)
        dbg_log1(regBase, "maxivmac INIT: %ld drives mounted",
                 (long)g->driveCount);

    /* ---- Install jGNEFilter ---- */
    g->oldFilter = *(long *)kJGNEFilter;
    g->lastPollTick = 0;
    g->lastClipTicks = 0;
    *(long *)kJGNEFilter = (long)FilterEntry;
    dbg_log(regBase, "maxivmac INIT: jGNEFilter installed");

    dbg_log(regBase, "maxivmac INIT: done!");

bail:
    RestoreA4();
}
```

**Key differences from old code:**
- No `ExtFSVersion` / `ClipVersion` calls.
- No `driveAvail` / `clipAvail` conditionals.
- Shift-key check at top.
- `HomeResFile` + `PBGetFCBInfo` before `DetachResource`.
- `SysEnvirons` call.
- `kCmdInitIdent` with all 7 params.
- Bail on host rejection.
- Traps installed unconditionally (no `driveAvail` gate).
- Mount drain always runs (loop is a no-op if queue is empty).

### 6.4 — Remove `kCmdGetVol` call

The old code called `reg_command(regBase, kCmdGetVol)` to get
`volFileCount` / `volTotalBytes` before allocating globals.  This was
gated on `driveAvail` and used only for a log message.  Remove it.
The `Globals` fields `volFileCount` and `volTotalBytes` can be removed
in a later cleanup.

### Fence

- [ ] `init.c` includes `version.h`
- [ ] `main()` follows the new boot sequence exactly
- [ ] No references to `ExtFSVersion` or `ClipVersion` in boot path
- [ ] Traps installed unconditionally
- [ ] `kCmdInitIdent` sends all 7 params
- [ ] Shift-key skip works
- [ ] INIT compiles in THINK C
- [ ] Commit: `"init-version: phase 6 — new boot sequence with InitIdent"`

---

## Phase 7 — Guest: reduce poll interval, remove dead code

### 7.1 — Reduce poll interval in `FilterEntry`

In `init.c`, in `FilterEntry()`, change the throttle from 60 ticks
to 1 tick:

```c
/* Old: */
if (TickCount() - g->lastPollTick >= 60)

/* New: */
if (TickCount() != g->lastPollTick)
```

This polls every tick (~1/60th second).  The register read is a
single memory-mapped access — negligible cost.  This also sidesteps
the TickCount wraparound issue entirely (we only check inequality,
not a magnitude comparison).

### 7.2 — Remove dead variables from `main()`

Remove the now-unused local variables `driveAvail` and `clipAvail`
from `main()`.  (These were already removed in Phase 6's rewrite,
but verify they're gone.)

### Fence

- [ ] Poll interval is 1 tick (every jGNE call)
- [ ] No TickCount wraparound risk
- [ ] No dead `driveAvail` / `clipAvail` variables
- [ ] INIT compiles in THINK C
- [ ] Commit: `"init-version: phase 7 — reduce poll to 1 tick, remove dead code"`

---

## Phase 8 — Guest: log clipboard errors

### 8.1 — Add `dbg_log` calls in `clip.c`

In `ExportMacToHost()`, after each error return, add a log line.
The function currently returns `-1` silently on failure.

```c
/* After NewHandle(0) failure: */
dbg_log(g->regBase, "clip: export failed (NewHandle)");
return -1;

/* After reg_result check: */
dbg_log(g->regBase, "clip: export failed (host rejected)");
return -1;
```

Similarly in `ImportHostToMac()`:
```c
/* After size=0 or allocation failure: */
dbg_log(g->regBase, "clip: import failed (alloc)");
return -1;

/* After host transfer error: */
dbg_log(g->regBase, "clip: import failed (host error)");
return -1;
```

In `SyncClipboard()`, after calling export/import, log if the return
value is negative:
```c
if (ExportMacToHost(g) < 0)
    dbg_log(g->regBase, "clip: export error (ignored)");
```

### Fence

- [ ] All error paths in `clip.c` have `dbg_log` calls
- [ ] INIT compiles in THINK C
- [ ] Commit: `"init-version: phase 8 — log clipboard sync errors"`

---

## Phase 9 — MANUAL CHECK: full guest+host integration

> **STOP.  This phase requires manual verification by the developer.**

1. Build the host: `cmake --preset macos && cmake --build --preset macos`
2. Compile the INIT in THINK C inside the emulator (using the
   existing working INIT to bootstrap).
3. Copy the new INIT to the boot disk's System Folder / Extensions.
4. Reboot the guest.
5. **Verify:**
   - Overlay shows `INIT: dev` (since `version.h` has the default).
   - Overlay shows machine type and system version (e.g.,
     `Mac II · System 7.0.1`).
   - If the INIT version differs from the host version, the version
     text is yellow.
   - Shared drives mount normally (configured at launch or later).
   - Clipboard sync works.
   - Hold Shift during boot → overlay shows "INIT: not loaded".
   - Debug console shows the new log messages.
6. Test with the old INIT (pre-versioning) on a different disk:
   - Boot with old INIT → overlay shows "INIT: not loaded".
   - Shared drives and clipboard still work (old `$0200`/`$0100`
     commands are still handled by host).

**Pass criteria:** New INIT reports to overlay.  Old INIT still works.
Shift-skip works.  No regressions in drives or clipboard.

---

## Phase 10 — Build pipeline: version file and build script

### 10.1 — Add `version.h` to `.gitignore`

Append to `.gitignore`:
```
macsrc/init/version.h
```

### 10.2 — Create `build.sh`

Create `macsrc/init/build.sh`:

```bash
#!/bin/bash
# Build the maxivmac INIT with the current git version stamped in.
#
# Usage: ./build.sh [--disk DISK.hfs] [--mac macii-7.mac]
#
# Prerequisites:
#   - A working maxivmac build in bld/macos/
#   - A boot disk with THINK C 5.0 and an existing (old) INIT

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
INIT_DIR="$SCRIPT_DIR"

# Generate version.h from git
VERSION=$(git describe --tags --match "v*" --always 2>/dev/null || echo "dev-unknown")
if [[ ! "$VERSION" =~ ^v ]]; then
    VERSION="dev-$VERSION"
fi

cat > "$INIT_DIR/version.h" << EOF
/* Auto-generated by build.sh — do not edit */
#ifndef VERSION_H
#define VERSION_H
#define kInitVersion "\p${VERSION}"
#endif
EOF

echo "Stamped INIT version: $VERSION"
echo "Run the emulator with shared drive pointing at macsrc/init/"
echo "and use build.dbg to compile."
```

### 10.3 — Change INIT resource ID to 128

The INIT resource ID change (314 → 128) must be done inside the
THINK C project.  This is a manual step:

1. Open the THINK C project in the emulator.
2. In the project's resource settings, change the INIT resource ID
   from 314 to 128.
3. Rebuild.

The `init.c` code already uses `kInitResID` (128) from Phase 6.

### Fence

- [ ] `.gitignore` includes `macsrc/init/version.h`
- [ ] `build.sh` generates `version.h` from git
- [ ] `version.h.default` is committed
- [ ] Commit: `"init-version: phase 10 — INIT build pipeline"`

---

## Phase 11 — MANUAL CHECK: end-to-end with build script

> **STOP.  Final verification by the developer.**

1. Delete `macsrc/init/version.h`.
2. Run `macsrc/init/build.sh` → verify `version.h` is created with the
   correct git version.
3. Boot emulator with shared drive at `macsrc/init/`.
4. Compile INIT in THINK C via `build.dbg` or manually.
5. Install the new INIT, reboot.
6. **Verify overlay shows the git version** (not "dev").
7. If a tag exists (e.g., `v1.0`), verify the overlay shows `v1.0`.
8. Build host from a different commit → boot with previous INIT →
   verify overlay shows the old version in **yellow** (stale).
9. Test `git status` — `version.h` should not appear (gitignored).

**Pass criteria:** Version stamping works end-to-end.  Stale
detection works.  No git pollution.
