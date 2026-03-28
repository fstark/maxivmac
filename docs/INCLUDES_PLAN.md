# Config Header Refactoring Plan

Eliminate `src/config/` and `cfg/` by migrating content into properly named,
properly placed headers.  Each step is independently committable and testable
(build + golden-file self-tests).

Reference: [docs/INCLUDES.md](INCLUDES.md)

---

## Current include architecture

```
25 device/CPU .cpp files ──► src/core/common.h
                               ├── CNFUIALL.h   (types, macros)
                               ├── CNFUIPIC.h   (empty)
                               ├── core/defaults.h
                               ├── core/endian.h
                               ├── CNFUDALL.h   (NumDrives, NumPbufs, dbglog_HAVE)
                               ├── platform/platform.h
                               ├── CNFUDPIC.h   (CPU flags, wires, PRAM, Sony, …)
                               └── core/machine.h

5 platform .cpp files ──────► src/platform/common/osglu_ud.h
                               ├── CNFUDOSG.h   (key mappings, UI toggles)
                               ├── CNFUDALL.h
                               ├── platform/platform.h
                               └── STRCONST.h   (redirect → lang/)

7 platform .cpp/.h files ──► src/platform/common/osglu_ui.h
                               ├── CNFUIOSG.h   (SDL includes, app metadata)
                               ├── CNFUIALL.h
                               ├── core/defaults.h
                               └── core/endian.h

0 active files ─────────────► src/OSGCOMUI.h    (legacy duplicate of osglu_ui.h)
```

---

## Execution order

### Phase 0 — Dead-weight removal (no code changes)

| Step | Action | Files touched | Risk |
|------|--------|---------------|------|
| 0a | Delete `cfg/` directory (stale originals, not in build) | `cfg/*` | None |
| 0b | Delete `src/config/Info.plist` | 1 file | None |
| 0c | Delete `src/config/English.lproj/` | 1 dir | None |
| 0d | Delete `src/OSGCOMUI.h` (legacy duplicate, 0 active includers) | 1 file | None |
| 0e | Delete `src/config/CNFUIPIC.h`, remove its `#include` from `common.h` (line 16) | 2 files | Trivial — file is empty |
| 0f | Delete `src/config/STRCONST.h`, replace its `#include` in `osglu_ud.h` (line 22) with the two lang includes it forwarded | 2 files | Trivial |

**Verify:** Build + self-tests pass.

---

### Phase 1 — CNFUDPIC.h split

The largest and most coupled header.  Split into three pieces.

#### Step 1a — Extract PRAM defaults into `rtc.cpp`

Move `SpeakerVol`, `MenuBlink`, `AutoKeyThresh`, `AutoKeyRate`,
`pr_HilColRed/Green/Blue` out of CNFUDPIC.h and make them local constants
in `src/devices/rtc.cpp` (the only consumer).

Files touched: `CNFUDPIC.h`, `rtc.cpp`.

#### Step 1b — Remove duplicate `Mouse_Enabled()` declaration

Delete the `bool Mouse_Enabled();` line from CNFUDPIC.h.  It is already
declared in `mouse.h` and defined in `mouse.cpp`.

Files touched: `CNFUDPIC.h`.

#### Step 1c — Create `src/core/wire_macros.h`

Move the wire accessor macros (lines 47–113 of CNFUDPIC.h) into a new
`src/core/wire_macros.h`:

- `#include "core/wire_ids.h"` (already present)
- All `#define VIA1_iA0 …`, signal aliases (`MemOverlay`, `IWMvSel`, …),
  VIA2 aliases, interrupt-request aliases, `ChangeNtfy` callback aliases.

Then replace that block in CNFUDPIC.h with `#include "core/wire_macros.h"`.

Files touched: new `src/core/wire_macros.h`, `CNFUDPIC.h`.

#### Step 1d — Rename remainder to `src/core/emulation_config.h`

What remains in CNFUDPIC.h after 1a–1c:

- CPU feature flags (`Use68020`, `EmFPU`, `EmMMU`)
- Cycle-accuracy flags (`WantCycByPriOp`, `WantCloserCyc`)
- Extension flags (`IncludeExtnPbufs`, `IncludeExtnHostTextClipExchange`)
- Sony driver flags (`Sony_SupportDC42`, `Sony_SupportTags`, …)
- `WantDisasm`, `ExtraAbnormalReports`
- `#include "core/wire_macros.h"` (from step 1c)

Rename `src/config/CNFUDPIC.h` → `src/core/emulation_config.h`.
Update the single includer: `src/core/common.h` line 30.

Files touched: rename + `common.h`.

**Verify:** Build + self-tests pass.

---

### Phase 2 — CNFUDOSG.h → `src/platform/platform_config.h`

Rename `src/config/CNFUDOSG.h` → `src/platform/platform_config.h`.

Update the single includer: `src/platform/common/osglu_ud.h` line 10.

No content changes — the key mappings and UI toggles stay as-is.

Files touched: rename + `osglu_ud.h`.

**Verify:** Build + self-tests pass.

---

### Phase 3 — CNFUIOSG.h → `src/platform/sdl_config.h`

Rename `src/config/CNFUIOSG.h` → `src/platform/sdl_config.h`.

Update both includers: `src/platform/common/osglu_ui.h` line 16.

Files touched: rename + `osglu_ui.h`.

**Verify:** Build + self-tests pass.

---

### Phase 4 — CNFUIALL.h → `src/core/types.h`

Rename `src/config/CNFUIALL.h` → `src/core/types.h`.

Update includers:
- `src/core/common.h` (line 14)
- `src/platform/common/osglu_ui.h` (line 22)

Files touched: rename + 2 headers.

**Verify:** Build + self-tests pass.

---

### Phase 5 — CNFUDALL.h → inline into existing headers

Only three defines remain: `dbglog_HAVE`, `NumDrives`, `NumPbufs`.

| Define | Destination | Rationale |
|--------|-------------|-----------|
| `dbglog_HAVE` | CMake compile definition (`-Ddbglog_HAVE=1`) | Used in 100+ locations; compile-def avoids a header |
| `NumDrives` | `src/core/emulation_config.h` (from Phase 1d) | Emulation constant |
| `NumPbufs` | `src/core/emulation_config.h` | Emulation constant |

Delete `src/config/CNFUDALL.h`.  Remove includes from `common.h` (line 27)
and `osglu_ud.h` (line 15).

Files touched: `CNFUDALL.h` (delete), `CMakeLists.txt`,
`emulation_config.h`, `common.h`, `osglu_ud.h`.

**Verify:** Build + self-tests pass.

---

### Phase 6 — Remove `src/config/` from include path

After all phases, `src/config/` is empty.  Remove:

1. The directory itself.
2. The `"${CMAKE_SOURCE_DIR}/src/config"` line from CMakeLists.txt
   `target_include_directories`.

Files touched: `CMakeLists.txt`, delete empty dir.

**Verify:** Build + self-tests pass.

---

## Final state

```
src/core/
    types.h              ← was CNFUIALL.h
    emulation_config.h   ← was CNFUDPIC.h (feature flags, Sony, extensions)
    wire_macros.h        ← extracted from CNFUDPIC.h
    wire_ids.h           (unchanged)
    ...

src/platform/
    sdl_config.h         ← was CNFUIOSG.h
    platform_config.h    ← was CNFUDOSG.h
    ...
```

Deleted:
- `cfg/` (entire directory)
- `src/config/` (entire directory)
- `src/OSGCOMUI.h`

Inlined:
- PRAM defaults → `rtc.cpp`
- STRCONST.h → direct lang includes in `osglu_ud.h`
- `dbglog_HAVE` → CMake `-D`
- `NumDrives`, `NumPbufs` → `emulation_config.h`
