# Machine → Rig Rename Plan

Implements **ROADMAP_PLAN.md Phase A1**: rename the `Machine` class to
`Rig`, `g_machine` to `g_rig`, update all call sites.

**Gate:** builds, 260 unit tests pass, 4 headless golden tests pass.

See [GLOSSARY.md](GLOSSARY.md) for the Rig / Model / Macintosh distinction.

---

## What Changes

| Symbol / file | From | To |
|---------------|------|----|
| Class name | `Machine` | `Rig` |
| Global pointer | `g_machine` | `g_rig` |
| Header file | `machine_obj.h` | `rig.h` |
| Impl file | `machine_obj.cpp` | `rig.cpp` |
| Legacy header | `machine.h` | *(keep — hardware defs, not the class)* |
| Legacy impl | `machine.cpp` | *(keep — lifecycle / ATT code)* |
| Device member | `Device::machine_` | `Device::rig_` |
| Local in main | `s_machine` | `s_rig` |
| Forward decl | `class Machine` in device.h | `class Rig` |

## What Does NOT Change

| Symbol | Reason |
|--------|--------|
| `MachineConfig` | Represents the **Model** concept, not the runtime Rig |
| `MacModel` enum | Hardware platform type — separate concept |
| `machine_config.h / .cpp` | Model configuration files |
| `BuildMachineConfig()` | Builds Model configuration |
| `MachineConfigForModel()` | Factory for per-model config |
| `s_machineConfig` in main.cpp | Named configuration, distinct from Rig |
| `machine.h` / `machine.cpp` | Legacy hardware defs & ATT — not the class |
| "Integrated Woz Machine" in comments | Apple hardware name, not our class |

---

## Phases

Each phase ends with a compile + test gate. Commit after each phase.

### Phase 1 — Rename files (git mv)

```
git mv src/core/machine_obj.h   src/core/rig.h
git mv src/core/machine_obj.cpp src/core/rig.cpp
```

Update **CMakeLists.txt** (line 47):
```
-    src/core/machine_obj.cpp
+    src/core/rig.cpp
```

Update all 22 `#include "core/machine_obj.h"` → `#include "core/rig.h"`:

| File |
|------|
| src/core/rig.cpp |
| src/core/machine.cpp |
| src/core/main.cpp |
| src/debugger/cmd_info.cpp |
| src/devices/adb.cpp |
| src/devices/asc.cpp |
| src/devices/iwm.cpp |
| src/devices/keyboard.cpp |
| src/devices/mouse.cpp |
| src/devices/pmu.cpp |
| src/devices/rom.cpp |
| src/devices/rtc.cpp |
| src/devices/scc.cpp |
| src/devices/screen.cpp |
| src/devices/sony.cpp |
| src/devices/sound.cpp |
| src/devices/via_base.cpp |
| src/devices/via.cpp |
| src/devices/via2.cpp |
| src/devices/video.cpp |
| src/platform/common/rom_loader.h |
| src/platform/emulator_shell.cpp |

**Gate:** `cmake --build --preset macos` succeeds.

**Commit:** `refactor: rename machine_obj.{h,cpp} → rig.{h,cpp}`

---

### Phase 2 — Rename class Machine → Rig

In `src/core/rig.h`:
- `class Machine` → `class Rig`

In `src/core/rig.cpp`:
- Constructor `Machine::Machine` → `Rig::Rig`
- Destructor `Machine::~Machine` → `Rig::~Rig`
- All `Machine::method` → `Rig::method`

In `src/devices/device.h`:
- `friend class Machine` → `friend class Rig`
- `Machine *machine_` → `Rig *rig_`
- Forward declaration `class Machine` → `class Rig`

In `src/devices/device.cpp` (if any member references):
- `machine_` → `rig_`

All device `.cpp` files using `machine_->`:
- `machine_->` → `rig_->` (grep for `machine_->` in src/devices/)

Files with `Machine` type references (not MachineConfig):
- `src/core/main.cpp`: `unique_ptr<Machine>` → `unique_ptr<Rig>`, `make_unique<Machine>` → `make_unique<Rig>`, `s_machine` → `s_rig`
- `src/core/machine.cpp`: function signatures taking `Machine&` or `Machine*`
- `src/core/ict_scheduler.h/cpp`: forward decl / parameter types
- `src/core/wire_bus.h/cpp`: forward decl / parameter types
- `src/cpu/cpu.h/cpp`: forward decl / parameter types
- `src/debugger/cmd_info.cpp`: any `Machine` type usage
- `src/platform/imgui_backend.cpp`: any `Machine` references
- `src/platform/imgui_model_selector.h/cpp`: any `Machine` references
- `src/platform/common/rom_loader.h`: type references

**Gate:** `cmake --build --preset macos` succeeds.

**Commit:** `refactor: rename class Machine → Rig`

---

### Phase 3 — Rename g_machine → g_rig

In `src/core/rig.h`:
- `extern Machine* g_machine` → `extern Rig* g_rig`

In `src/core/rig.cpp`:
- `Machine* g_machine = nullptr` → `Rig* g_rig = nullptr`

All 227 occurrences of `g_machine` across 18 files → `g_rig`:

| File | ~Count |
|------|--------|
| src/core/rig.cpp | ~5 |
| src/core/rig.h | ~2 |
| src/core/machine.cpp | ~40 |
| src/core/main.cpp | ~10 |
| src/debugger/cmd_info.cpp | ~5 |
| src/devices/hpmac_hack.h | ~3 |
| src/devices/iwm.cpp | ~10 |
| src/devices/mouse.cpp | ~5 |
| src/devices/rom.cpp | ~8 |
| src/devices/rtc.cpp | ~3 |
| src/devices/scc.cpp | ~5 |
| src/devices/screen_hack.h | ~3 |
| src/devices/screen.cpp | ~8 |
| src/devices/sony.cpp | ~15 |
| src/devices/sound.cpp | ~5 |
| src/devices/video.cpp | ~10 |
| src/platform/common/rom_loader.cpp | ~3 |
| src/platform/emulator_shell.cpp | ~15 |

**Gate:** `cmake --build --preset macos` succeeds.

**Commit:** `refactor: rename g_machine → g_rig`

---

### Phase 4 — Update comments and docs

Update header comments in `rig.h`:
- "Machine class — owns all emulator state" → "Rig class — owns all emulator state"

Update `device.h` comment:
- "set by Machine when device is registered" → "set by Rig when device is registered"

Update documentation files referencing `Machine` class or `g_machine`:
- docs/GLOSSARY.md — remove "(rename to `Rig` / `g_rig` pending)"
- docs/NAMING.md — update `class Machine` example to `class Rig`
- docs/GLOBALS.md — update `g_machine` entry to `g_rig`
- docs/roadmap/ROADMAP_PLAN.md — mark A1 as done
- Other docs (FULL_PLAN.md, INSIGHTS.md, etc.) — update stale references

**Gate:** `cmake --build --preset macos` succeeds. Documentation grep clean.

**Commit:** `docs: update references for Machine → Rig rename`

---

### Phase 5 — Full test validation

```sh
cmake --build --preset macos
ctest --preset macos          # 260 unit tests
./selftest.sh                 # 4 headless golden tests
```

**Gate:** all green.

**Final commit (if any fixups):** `fix: Machine → Rig rename fixups`

---

## Risk Notes

- **machine.h / machine.cpp stay** — these contain hardware definitions,
  ATT logic, and extension dispatch. They are *not* the Rig class. Renaming
  them is a separate task (if ever).
- **MachineConfig stays** — it is the Model concept per the glossary.
- **No runtime risk** — this is a pure mechanical rename. No logic changes.
  The golden tests catch any behavioral regression.
- **~270 total edits** across ~40 files. sed/IDE rename + compile loop is
  the safest approach.
