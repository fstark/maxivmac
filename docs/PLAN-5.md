# Phase 5 — Multi-Model Support & Runtime Configuration (Detailed Plan)

Complete the remaining compile-time → runtime conversions and make the emulator a true multi-model binary. A single `minivmac` binary will boot any supported Mac model from the same executable, configured via command-line flags or a config file.

---

## Context: What We're Starting With

After Phase 4, the emulator has a `Machine` object that owns all devices, a `WireBus` for inter-device signal routing, an `ICTScheduler` for cycle-based task dispatch, and a `MachineConfig` struct with per-model feature flags. Mac II boots System 7 identically to pre-Phase-4.

### What remains compile-time (deferred from Phase 4)

| Item | Where | Count | Why it stayed |
|------|-------|-------|---------------|
| `#define Use68020 1` | `CNFUDPIC.h` | 106 refs in `m68k.cpp`, 17 in `m68k_tables.cpp`, 33 in `disasm.cpp`, 1 in `m68k_tables.h` | Guards instruction decode hot paths |
| `#define EmFPU 1` | `CNFUDPIC.h` | 2 refs (`m68k.cpp`, `m68k_tables.cpp`) | FPU opcode handler inclusion |
| `#define EmMMU 0` | `CNFUDPIC.h` | 10 refs (`m68k.cpp`, `m68k_tables.cpp`) | MMU opcode handler inclusion |
| `#define EmClassicKbrd 0` | `CNFUDPIC.h` | Guards entire body of `keyboard.cpp`; 3 refs in `machine_obj.cpp`; 1 in `mouse.cpp` | `keyboard.cpp` uses `KYBD_ShiftInData`/`KYBD_ShiftOutData` externs + Wire macros that resolve differently per model |
| `#define EmClassicSnd 0` | `CNFUDPIC.h` | Guards entire body of `sound.cpp`; 3 refs in `machine_obj.cpp` | `sound.cpp` reads `SoundDisable`/`SoundVolb*` wire macros, uses `GetSoundInvertTime` → `VIA1_GetT1InvertTime`, and computes buffer addresses from `kRAM_Size` |
| `#define EmPMU 0` | `CNFUDPIC.h` | Guards entire body of `pmu.cpp`; 3 refs in `machine_obj.cpp` | `pmu.cpp` directly reads/writes `VIA1_iA0`–`VIA1_iA7` macros, which resolve to different Wire indices per model |
| `kRAMa_Size`, `kRAMb_Size` | `CNFUDPIC.h` | ~30 refs in `machine.cpp`, 2 in `main.cpp`, derived `kRAM_Size` in `machine.h` | Memory map address calculations use these as compile-time constants, some in `#if` preprocessor guards |
| `kVidMemRAM_Size`, `kVidROM_Size` | `CNFUDPIC.h` | ~8 refs in `video.cpp`, `main.cpp`, `machine.cpp` | VidROM init, VRAM slot addressing |
| `vMacScreenWidth/Height/Depth` | `CNFUDALL.h` | ~500 refs total across all platforms + devices | Used in `#if` preprocessor guards (CLUT sizing, screen map selection), platform window/texture sizing, video card ROM tables, screen buffer allocation |
| Wire `enum` + `#define` aliases | `CNFUDPIC.h` | ~30 wire IDs, ~40 `#define` macros | Model-specific: Mac II maps `VIA1_iB3` → ADB_Int, Plus maps it differently. VIA config `#define`s (float vals, port masks) are model-specific. |
| Global device pointers | 17 `g_xxx` pointers | Used in backward-compatible free-function stubs, some device cross-refs | Need full migration to `Machine::findDevice<>()` before removal |

### Current source files

```
src/config/CNFUDPIC.h     — 236 lines: compile-time model config, Wire enum, VIA config
src/config/CNFUDALL.h      — screen size, ROM size, sound constants
src/cpu/m68k.cpp           — ~9000 lines, 106× #if Use68020, 2× #if EmFPU, 8× #if EmMMU|EmFPU
src/cpu/m68k_tables.cpp    — ~3000 lines, 17× #if Use68020, 1× #if EmFPU, 2× #if EmMMU
src/cpu/disasm.cpp          — ~4000 lines, 33× #if Use68020
src/devices/keyboard.cpp   — 231 lines, entire body guarded by #if EmClassicKbrd
src/devices/sound.cpp      — 228 lines, entire body guarded by #if EmClassicSnd
src/devices/pmu.cpp         — 452 lines, entire body guarded by #if EmPMU
src/core/machine.cpp       — uses kRAMa_Size etc. in address map setup (~30 refs)
src/core/machine_obj.cpp   — 9× #if EmClassicKbrd/EmClassicSnd/EmPMU guards
```

---

## Architecture Overview

### Target state

```
CNFUDPIC.h   → deleted (or empty stub). All model/device config is in MachineConfig.
CNFUDALL.h   → screen size/ROM size become MachineConfig fields; only platform-independent
               constants remain.
Wire enum    → superset WireID enum in src/core/wire_ids.h. Per-model wiring registered
               at Machine::init() time. No #define aliases.
VIA config   → per-model VIA port masks stored in MachineConfig or VIA1Device/VIA2Device
               init params.
CPU flags    → Use68020/EmFPU/EmMMU are member bools on CPU class. Instruction decode
               tables built at init time with runtime checks. Hot-path handlers use
               if (cpu.use68020) with branch prediction.
Memory sizes → MachineConfig fields, dynamically allocated. Address map calculations
               use config values, not #define constants.
Screen sizes → MachineConfig fields. Platform layer reads config at init time.
               Platform #if guards converted to runtime dispatch.
keyboard/sound/pmu → Always compile. VIA cross-deps abstracted through WireBus
               callbacks and Machine::findDevice<>(). File-level #if guards removed.
Global g_xxx → Eliminated. All device access through Machine.
```

### Key design decisions

| Decision | Rationale |
|----------|-----------|
| **Superset Wire enum** | All ~50 unique wire IDs (union of all models) in one enum. Per-model `Machine::init()` only registers callbacks for wires that exist on that model. Unused wires have no callbacks and no cost. Simpler than runtime-allocated integer IDs. |
| **CPU tables built once at init** | The `#if Use68020` guards in `m68k_tables.cpp` become `if (cpu->use68020)` in the table-building functions. These run once. The resulting dispatch table contains only the correct handler pointers — the hot path (table lookup + indirect call) is unchanged. |
| **VIA config struct instead of #defines** | Per-model `VIA1Config { uint8_t oraFloatVal, orbFloatVal, oraCanIn, ... }` passed at VIA creation. Replaces 20 `#define` macros. |
| **Screen size as runtime values** | Platform layer reads `config.screenWidth/Height/Depth` at initialization. The `screen_map.h` / `screen_translate.h` C-template headers are refactored to take depth as a parameter (function pointer table indexed by depth) instead of being multi-included with different `#define` params. |
| **TOML config file** | `toml++` (header-only, C++17, MIT license) parses a config file. Command-line flags override file values. Simple key=value for model, RAM, screen, ROM path, disk images. |

---

## Steps

### Step 5.1 — Create superset WireID enum and per-model wiring factory

Replace the model-specific Wire `enum` in `CNFUDPIC.h` with a superset enum covering all models, and a factory function that registers the correct wire callbacks per model.

**New file: `src/core/wire_ids.h`** (replace current placeholder if it exists):
```cpp
#pragma once

// Superset of all wire IDs across all Mac models.
// Not all wires are used on every model — unused wires simply
// have no registered callbacks and cost nothing.
enum WireID {
    // Sound (compact Macs: directly driven by VIA1)
    Wire_SoundDisable,
    Wire_SoundVolb0,
    Wire_SoundVolb1,
    Wire_SoundVolb2,

    // VIA1 Port A
    Wire_VIA1_iA0,     // Plus: unused. II: overlay-related
    Wire_VIA1_iA1,
    Wire_VIA1_iA2,
    Wire_VIA1_iA3_SCCvSync,
    Wire_VIA1_iA4_MemOverlay,
    Wire_VIA1_iA5_IWMvSel,
    Wire_VIA1_iA6,     // Plus: unused. PB100: PMU
    Wire_VIA1_iA7_SCCwaitrq,

    // VIA1 Port B
    Wire_VIA1_iB0_RTCdataLine,
    Wire_VIA1_iB1_RTCclock,
    Wire_VIA1_iB2_RTCunEnabled,
    Wire_VIA1_iB3,     // II: ADB_Int. Plus: unused.
    Wire_VIA1_iB4,     // II: ADB_st0. Plus: unused.
    Wire_VIA1_iB5,     // II: ADB_st1. Plus: unused.
    Wire_VIA1_iB6,     // PB100: PMU
    Wire_VIA1_iB7,     // Sound compat

    // VIA1 CB2/CA2/CA1
    Wire_VIA1_iCB2,    // II: ADB_Data. Plus: Keyboard data.
    Wire_VIA1_CA1,      // Sixtieth pulse
    Wire_VIA1_CA2,      // RTC one-second pulse

    // VIA2 (Mac II family only)
    Wire_VIA2_InterruptRequest,
    Wire_VIA2_iA0,     // VBL interrupt slot
    Wire_VIA2_iA6,     // Addr32 related
    Wire_VIA2_iA7,     // Addr32 related
    Wire_VIA2_iB2_PowerOff,
    Wire_VIA2_iB3_Addr32,
    Wire_VIA2_iB7,
    Wire_VIA2_iCB2,
    Wire_VIA2_CA1,      // VBL interrupt pulse
    Wire_VIA2_CB1,      // ASC interrupt pulse

    // VIA1 interrupt
    Wire_VIA1_InterruptRequest,

    // SCC
    Wire_SCCInterruptRequest,

    // ADB (Mac II / SE family)
    Wire_ADBMouseDisabled,

    // Video (Mac II only)
    Wire_VBLinterrupt,
    Wire_VBLintunenbl,

    // PMU (PB100 only)
    Wire_PMU_FromReady,
    Wire_PMU_ToReady,

    kNumWires
};
```

**New file: `src/core/wire_topology.h` + `wire_topology.cpp`:**
```cpp
// Per-model wire callback registration.
void SetupWireTopology_MacPlus(Machine& m);
void SetupWireTopology_MacSE(Machine& m);
void SetupWireTopology_MacII(Machine& m);
void SetupWireTopology_PB100(Machine& m);
```

Each function registers callbacks on the WireBus for the wires used by that model. For example, `SetupWireTopology_MacII` registers:
- `Wire_VIA1_iB3` → ADB_Int → `ADBDevice::intChange()`
- `Wire_VIA1_iB4`/`iB5` → ADB state → `ADBDevice::stateChange()`
- `Wire_VIA1_iCB2` → ADB data → `ADBDevice::dataLineChngNtfy()`

While `SetupWireTopology_MacPlus` registers:
- `Wire_VIA1_iCB2` → Keyboard data → `KeyboardDevice::dataLineChngNtfy()`
- `Wire_SoundDisable`/`SoundVolb*` → `SoundDevice::volumeChange()`

**Migration of CNFUDPIC.h wire section:**
- Remove the Wire `enum` from `CNFUDPIC.h`.
- Remove all `#define VIA1_iB3 (Wires[Wire_VIA1_iB3_ADB_Int])` aliases from `CNFUDPIC.h`.
- All code that currently uses `VIA1_iB3` macro → directly uses `machine_->wireBus().get(Wire_VIA1_iB3)`.
- The `Wires` global pointer compatibility (`g_wires.data()`) is removed. All access through `wireBus()`.

**Also replace VIA config `#define`s:**
- Remove `VIA1_ORA_FloatVal`, `VIA1_ORB_FloatVal`, `VIA1_ORA_CanIn`, etc. from `CNFUDPIC.h`.
- Create a `VIAConfig` struct:
  ```cpp
  struct VIAConfig {
      uint8_t oraFloatVal, orbFloatVal;
      uint8_t oraCanIn, oraCanOut;
      uint8_t orbCanIn, orbCanOut;
      uint8_t ierNever0, ierNever1;
      uint8_t cb2ModesAllowed, ca2ModesAllowed;
  };
  ```
- `VIA1Device` and `VIA2Device` constructors accept `VIAConfig`. `MachineConfigForModel()` produces the correct config per model.

**Also move pulse notification aliases:**
- `#define VIA1_iCA1_PulseNtfy ...` → registered as `wireBus.onPulse(Wire_VIA1_CA1, ...)` per model.
- `#define ADB_ShiftInData VIA1_ShiftOutData` → the ADB device calls `machine_->findDevice<VIA1Device>()->shiftOutData()` directly.

**Validation:**
```bash
rm -rf bld/macos-cocoa && cmake --preset macos-cocoa && cmake --build --preset macos-cocoa
./bld/macos-cocoa/minivmac.app/Contents/MacOS/minivmac MacII.ROM extras/disks/608.hfs
```
Mac II boots System 7. All ADB keyboard/mouse input works. RTC keeps time. Sound plays. All wire-driven interactions behave identically.

**Commit:** `Replace model-specific Wire enum with superset WireID and per-model topology factory`

---

### Step 5.2 — Decouple keyboard.cpp from VIA cross-dependencies

Make `keyboard.cpp` always compile regardless of model — remove the `#if EmClassicKbrd` guard around the entire file body.

**Problem:** `keyboard.cpp` currently:
1. Declares `extern void KYBD_ShiftOutData(uint8_t v)` and `extern uint8_t KYBD_ShiftInData(void)` — these must be provided by VIA1 when classic keyboard mode is active.
2. Uses `VIA1_iCB2` macro — reads the wire value.
3. Uses `Wire_VIA1_iCB2_ADB_Data` — this wire name is ADB-specific in Mac II config.
4. Calls `ICT_add(kICT_Kybd_ReceiveCommand, ...)` with compile-time timing constants.
5. Calls `g_wires.set(Wire_VIA1_iCB2_ADB_Data, 1)`.

**Solution:**
1. Replace `KYBD_ShiftOutData`/`KYBD_ShiftInData` externs with `machine_->findDevice<VIA1Device>()` method calls:
   ```cpp
   void KeyboardDevice::shiftOut(uint8_t v) {
       machine_->findDevice<VIA1Device>()->shiftInData(v);
   }
   uint8_t KeyboardDevice::shiftIn() {
       return machine_->findDevice<VIA1Device>()->shiftOutData();
   }
   ```
2. Replace `VIA1_iCB2` macro → `machine_->wireBus().get(Wire_VIA1_iCB2)`.
3. Replace `Wire_VIA1_iCB2_ADB_Data` → `Wire_VIA1_iCB2` (the superset name from Step 5.1).
4. Replace `g_wires.set(...)` → `machine_->wireBus().set(Wire_VIA1_iCB2, 1)`.
5. Replace `kCycleScale * kMyClockMult` with `machine_->config().clockMult` and computed cycle scale.
6. Move all `static` state variables (`KybdState`, `HaveKeyBoardResult`, etc.) into `KeyboardDevice` class members.
7. Remove the `#if EmClassicKbrd` / `#else` / `#endif` file guard — the file always compiles. The `Machine` only creates a `KeyboardDevice` if `config.emClassicKbrd` is true.

**Remove backward-compat stubs:**
- `Kybd_DataLineChngNtfy()`, `DoKybd_ReceiveEndCommand()`, `DoKybd_ReceiveCommand()`, `KeyBoard_Update()` free functions are removed. The wire topology factory (Step 5.1) registers callbacks directly on the `KeyboardDevice` instance.

**Validation:** Build + boot Mac II (which does NOT use classic keyboard — verify no crash from always-compiled keyboard code). The Mac Plus test will validate actual keyboard function in Step 5.9.

**Commit:** `Decouple keyboard.cpp from VIA: always compiles, uses WireBus + findDevice`

---

### Step 5.3 — Decouple sound.cpp from VIA cross-dependencies

Make `sound.cpp` always compile — remove the `#if EmClassicSnd` guard.

**Problem:** `sound.cpp` currently:
1. Reads `SoundDisable`, `SoundVolb0`, `SoundVolb1`, `SoundVolb2` — Wire macros.
2. Uses `GetSoundInvertTime` → aliased to `VIA1_GetT1InvertTime`.
3. Computes buffer addresses from `kRAM_Size` (`kSnd_Main_Buffer = kRAM_Size - kSnd_Main_Offset`).
4. Calls `MySound_BeginWrite()` / `MySound_EndWrite()` — platform-provided sound output.
5. All static state is file-scoped.

**Solution:**
1. Replace `SoundDisable` → `machine_->wireBus().get(Wire_SoundDisable)`.
2. Replace `SoundVolb0/1/2` → `machine_->wireBus().get(Wire_SoundVolb0/1/2)`.
3. Replace `GetSoundInvertTime` → `machine_->findDevice<VIA1Device>()->getT1InvertTime()` (add method to VIA1Device).
4. Replace `kRAM_Size` → `machine_->config().ramSize()`. Compute buffer addresses at init time:
   ```cpp
   void SoundDevice::init() {
       sndMainBuffer_ = machine_->config().ramSize() - 0x0300;
       sndAltBuffer_  = machine_->config().ramSize() - 0x5F00;
   }
   ```
5. Move static state into `SoundDevice` class members.
6. Remove `#if EmClassicSnd` guard.
7. Remove backward-compat free-function stubs.

**Validation:** Build + boot Mac II (uses ASC, not classic sound — verify no crash). Classic sound tested with Mac Plus in Step 5.9.

**Commit:** `Decouple sound.cpp from VIA: always compiles, uses WireBus + findDevice`

---

### Step 5.4 — Decouple pmu.cpp from VIA cross-dependencies

Make `pmu.cpp` always compile — remove the `#if EmPMU` guard.

**Problem:** `pmu.cpp` currently:
1. `#include "devices/via.h"` and directly reads/writes `VIA1_iA0`–`VIA1_iA7` macros in `GetPMUbus()` and `SetPMUbus()`.
2. Uses `PmuToReady` / `PmuFromReady` — Wire-aliased macros for PMU handshake lines.
3. All static state is file-scoped.

**Solution:**
1. Replace `VIA1_iA0`–`VIA1_iA7` reads → `machine_->wireBus().get(Wire_VIA1_iA0)` etc.
2. Replace `VIA1_iA0`–`VIA1_iA7` writes → `machine_->wireBus().set(Wire_VIA1_iA0, val)` etc. This triggers VIA port callbacks correctly.
3. Replace `PmuToReady`/`PmuFromReady` → `machine_->wireBus().get(Wire_PMU_ToReady)` / `machine_->wireBus().set(Wire_PMU_FromReady, v)`.
4. Move all static state and `PARAMRAM[128]` into `PMUDevice` class members.
5. Remove `#if EmPMU` guard.
6. Remove backward-compat stubs.

**Update `machine_obj.cpp`:**
- Remove the `#if EmClassicKbrd`, `#if EmClassicSnd`, `#if EmPMU` guards around device creation. These now use runtime config checks:
  ```cpp
  if (config_.emClassicKbrd) addDevice(std::make_unique<KeyboardDevice>());
  if (config_.emClassicSnd)  addDevice(std::make_unique<SoundDevice>());
  if (config_.emPMU)         addDevice(std::make_unique<PMUDevice>());
  ```
- Remove corresponding `#include` guards.

**Remove `EmClassicKbrd`, `EmClassicSnd`, `EmPMU` from `CNFUDPIC.h`:**
These three `#define`s are no longer needed — all code paths are runtime.

**Validation:** Build + boot Mac II. Verify no regressions.

**Commit:** `Decouple pmu.cpp from VIA; remove EmClassicKbrd/EmClassicSnd/EmPMU compile-time guards`

---

### Step 5.5 — Convert CPU instruction set selection to runtime

Make `Use68020`, `EmFPU`, and `EmMMU` runtime booleans on the CPU class. This is the largest mechanical change in Phase 5.

**Strategy — three-tier conversion:**

**Tier 1: Table-building code (`m68k_tables.cpp`, 17 `Use68020` + 1 `EmFPU` + 2 `EmMMU` refs):**
These run once at startup. Convert `#if Use68020` → `if (cpu->use68020)`. Zero performance impact.

**Tier 2: Instruction handlers in `m68k.cpp` (55 `Use68020` + 1 `EmFPU` + 8 `EmMMU|EmFPU` refs):**
Within individual opcode handlers — e.g., extra addressing modes, bitfield instructions, extended shift counts. Convert to `if (cpu->use68020)`. Branch predictor handles this at ~100% accuracy since the value never changes during execution. The hot loop (opcode fetch → table dispatch → handler call) is unchanged.

For the `EmFPU` guard: the FPU instruction handler (`DoCodeF`) checks `cpu->emFPU` and generates a line-F exception if disabled. The `fpu_emdev.h` included code is always compiled (it's dead code when FPU is off, but costs nothing — only the entry point check matters).

For the `EmMMU` guards: MMU instructions (`PMOVE`, `PFLUSH`, etc.) check `cpu->emMMU` and generate privilege violations if disabled.

**Tier 3: Disassembler (`disasm.cpp`, 33 `Use68020` refs):**
Not performance-critical at all. Convert directly.

**Tier 4: `m68k_tables.h` (1 ref):**
The `#if Use68020` guard in the header likely controls table sizing or opcode count. Convert to a runtime-initialized value.

**Implementation approach for `m68k.cpp`:**

The CPU class already has `use68020`, `emFPU`, `emMMU` as member bools (from Phase 4, Step 4.13). The current code uses `#define Use68020 1` from `CNFUDPIC.h`. Steps:

1. In `m68k.cpp`, define a file-local accessor:
   ```cpp
   // At top of file, after currentCPU is set:
   #define Use68020  (currentCPU->use68020)
   #define EmFPU     (currentCPU->emFPU)
   #define EmMMU     (currentCPU->emMMU)
   ```
   Wait — this won't work for `#if` preprocessor tests. We must convert `#if Use68020` to `if (Use68020)` C++ code.

2. Process file by file. For each `#if Use68020` block:
   - If it gates a code block (the common case), convert to `if (currentCPU->use68020) { ... }`.
   - If it gates a variable declaration, use the variable unconditionally (initialized to 0 when 68000 mode).
   - If it gates an `#else` branch with 68000-specific code, convert to `if/else`.
   - A few `#if Use68020` blocks wrap `#include "fpu_emdev.h"` — convert to always-include with runtime FPU check at the entry point.

3. In `m68k_tables.cpp`, same approach. The dispatch table will include 68020 entries that call handlers with runtime checks (or fewer entries that trap at decode time — but the former is simpler).

**Remove `Use68020`, `EmFPU`, `EmMMU` from `CNFUDPIC.h`.**

**Validation:**
```bash
rm -rf bld/macos-cocoa && cmake --preset macos-cocoa && cmake --build --preset macos-cocoa
```
Boot Mac II with System 7 (exercises 68020+FPU). Verify:
- [ ] System 7 boots (uses 68020 addressing modes)
- [ ] 32-bit addressing works
- [ ] FPU-using applications work (if test apps available)
- [ ] Performance: boot-to-desktop time within 5% of pre-change baseline

**Commit sequence (split for safety):**
1. `Convert Use68020/EmFPU/EmMMU to runtime in m68k_tables.cpp`
2. `Convert Use68020/EmFPU/EmMMU to runtime in m68k.cpp`
3. `Convert Use68020 to runtime in disasm.cpp`
4. `Remove Use68020/EmFPU/EmMMU from CNFUDPIC.h`

---

### Step 5.6 — Convert memory sizes to runtime

Replace `kRAMa_Size`, `kRAMb_Size`, `kVidMemRAM_Size`, `kVidROM_Size` compile-time constants with `MachineConfig` field reads.

**Problem areas in `machine.cpp`:**
Some memory size constants are used in `#if` preprocessor guards:
```cpp
#if kRAMa_Size == kRAMb_Size    // symmetric RAM banks
#if kVidMemRAM_Size >= 0x00200000   // large VRAM
```
These cannot be `if()` — they must be refactored to runtime checks.

**Changes to `src/core/machine.cpp` (~30 refs):**
1. Replace `kRAMa_Size` → `config.ramASize` (the Machine's config, passed to address setup functions).
2. Replace `kRAMb_Size` → `config.ramBSize`.
3. Replace `kRAM_Size` → `config.ramSize()`.
4. Replace `kVidMemRAM_Size` → `config.vidMemSize`.
5. Replace `kVidROM_Size` → `config.vidROMSize`.
6. Convert `#if kRAMa_Size == kRAMb_Size` → `if (config.ramASize == config.ramBSize)`.
7. Convert `#if kVidMemRAM_Size >= 0x00200000` → `if (config.vidMemSize >= 0x00200000)`.
8. Pass `MachineConfig` (by const ref) to `SetUp_address()` and related functions.

**Changes to `src/core/machine.h`:**
- Remove `#define kRAM_Size (kRAMa_Size + kRAMb_Size)`.

**Changes to `src/core/main.cpp` (2 refs):**
- Replace `kVidROM_Size` → `g_machine->config().vidROMSize`.
- Replace `kVidMemRAM_Size` → `g_machine->config().vidMemSize`.

**Changes to `src/devices/video.cpp` (~8 refs):**
- Replace `kVidROM_Size` → `machine_->config().vidROMSize`.
- Replace `kVidMemRAM_Size` → `machine_->config().vidMemSize`.

**Remove from `CNFUDPIC.h`:**
- `kRAMa_Size`, `kRAMb_Size`, `kVidMemRAM_Size`, `kVidROM_Size`
- Also `MaxATTListN` → `config.maxATTListN` (already in MachineConfig).

**Validation:** Build + boot Mac II.

**Commit:** `Convert memory size constants to runtime MachineConfig fields`

---

### Step 5.7 — Convert screen size constants to runtime

Replace `vMacScreenWidth`, `vMacScreenHeight`, `vMacScreenDepth` compile-time constants with `MachineConfig` fields. This is the most invasive change (~500 refs across many files), but most are mechanical.

**MachineConfig already has:**
```cpp
uint16_t screenWidth  = 640;
uint16_t screenHeight = 480;
uint8_t  screenDepth  = 3;
```

**Strategy — three tiers of conversion:**

**Tier 1: Device code (`video.cpp`, `screen.cpp`, `rtc.cpp`) — ~35 refs:**
Convert to `machine_->config().screenWidth` etc. Straightforward.

**Tier 2: Platform common code (`osglu_common.cpp`, `control_mode.cpp`, `platform.h`) — ~70 refs:**
These need access to `g_machine->config()`. Create helper accessors:
```cpp
// In platform.h:
inline uint16_t screenWidth()  { return g_machine->config().screenWidth; }
inline uint16_t screenHeight() { return g_machine->config().screenHeight; }
inline uint8_t  screenDepth()  { return g_machine->config().screenDepth; }
```

**Tier 3: Platform backends (`cocoa.mm`, `sdl.cpp`) — ~90 refs across kept backends:**
Replace each reference. Some are in `#if` guards:
```cpp
#if (0 != vMacScreenDepth) && (vMacScreenDepth < 4)
```
These are used for CLUT management (color lookup table only needed for 1–8 bpp, not 16/24 bpp). Convert to:
```cpp
if (screenDepth() != 0 && screenDepth() < 4) { ... }
```

**Tier 3b: `screen_map.h` and `screen_translate.h` (C-template headers):**
These are multi-included with different `#define` params to generate functions for each screen depth. This pattern is incompatible with runtime depth. Refactor to:
- Generate all depth variants at compile time (include with each depth value) into a function table.
- At runtime, index the table by `screenDepth()`:
  ```cpp
  typedef void (*ScreenMapFn)(uint8_t* src, uint8_t* dst, int top, int left, int bottom, int right);
  static ScreenMapFn screenMapTable[5] = {
      screenMap_1bpp, screenMap_2bpp, screenMap_4bpp, screenMap_8bpp, screenMap_16bpp
  };
  // Usage:
  screenMapTable[screenDepth()](src, dst, ...);
  ```
  The multi-include pattern stays for generating the variants — but the depth selection becomes runtime.

**Tier 4: Dropped backends (`x11.cpp`, `win32.cpp`, `carbon.cpp`, `gtk.cpp`, `nds.cpp`, `dos.cpp`, `classic_mac.cpp`):**
These will be removed in Phase 6. For now, keep them compiling with a backward-compat `#define`:
```cpp
// Temporary: will be removed when these backends are dropped in Phase 6
#define vMacScreenWidth  (g_machine->config().screenWidth)
#define vMacScreenHeight (g_machine->config().screenHeight)
#define vMacScreenDepth  (g_machine->config().screenDepth)
```
This lets them compile without touching hundreds of lines in files we'll delete. The `#if` guards in these files won't work with this approach — but we can ignore correctness in backends that are about to be removed. As long as the Mac II config (depth=3) builds, we're fine.

**Remove from `CNFUDALL.h`:**
- `vMacScreenWidth`, `vMacScreenHeight`, `vMacScreenDepth`.
- Also `kROM_Size` → derive from model config (Mac II = 256 KB, Plus = 64 KB).

**Video card ROM tables (`video.cpp`):**
The video device builds mode tables (resolution + depth combos) at init time from config values instead of compile-time constants. The `screen_hack.h` ROM patching code (112 refs to screen constants) needs the most careful conversion — but it already only applies to non-standard screen sizes, and the patches are written based on the desired screen geometry, so switching to runtime values is natural.

**Validation:**
```bash
rm -rf bld/macos-cocoa && cmake --preset macos-cocoa && cmake --build --preset macos-cocoa
```
Boot Mac II. Verify:
- [ ] Correct window size (640×480)
- [ ] Color display (8-bit depth)
- [ ] Mouse tracking respects screen bounds
- [ ] Control mode overlay renders correctly

**Commit sequence:**
1. `Convert screen size to runtime in device code (video, screen, rtc)`
2. `Convert screen size to runtime in platform common code`
3. `Convert screen size to runtime in cocoa.mm backend`
4. `Refactor screen_map/screen_translate to runtime depth dispatch`
5. `Add backward-compat screen macros for legacy backends; remove from CNFUDALL.h`

---

### Step 5.8 — Remove global device pointers

Replace all 17 `g_xxx` global device pointers with `Machine::findDevice<>()` lookups cached at init time.

**Current state:** Each device file defines a `g_xxx` pointer (e.g., `VIA1Device* g_via1`) and free-function stubs that forward to it (e.g., `void VIA1_Reset() { g_via1->reset(); }`). Other files use either the global pointer or the free-function stubs.

**Strategy:**
1. **Within device files**: Replace `g_xxx` self-references with `this` (already mostly done in Phase 4).
2. **Cross-device access** (e.g., PMU accessing VIA1): Use `machine_->findDevice<VIA1Device>()` — already the pattern since Steps 5.2–5.4.
3. **Platform layer** accessing devices (e.g., cocoa.mm reading screen buffer): Access through `g_machine->findDevice<ScreenDevice>()`. Cache the pointer at platform init time:
   ```cpp
   static ScreenDevice* cachedScreen = nullptr;
   // In platform init:
   cachedScreen = g_machine->findDevice<ScreenDevice>();
   ```
4. **`g_machine` stays** as the single global entry point. It's less invasive than threading `Machine*` through every platform callback. Can be removed in Phase 6 when platforms get a clean `Platform` interface.

**Per-device removal checklist:**
- For each `g_xxx`: grep all references, replace with `machine_->findDevice<T>()` or `g_machine->findDevice<T>()`.
- Remove the `extern XXXDevice* g_xxx;` declaration from the header.
- Remove the `XXXDevice* g_xxx = nullptr;` definition from the .cpp file.
- Remove the free-function stubs at the bottom of each device .cpp file.
- Update callers to use device methods directly.

**Order:** Start with devices least referenced by other code (SCSI, ASC, PMU) and progress to the most widely referenced (VIA1, Screen).

**Validation:** Build + boot Mac II after each device removal (or batched in groups of 3-4).

**Commit sequence:**
1. `Remove global pointers for SCSI, ASC, PMU, ROM devices`
2. `Remove global pointers for IWM, Sony, Video, Mouse devices`
3. `Remove global pointers for SCC, ADB, Keyboard, Sound devices`
4. `Remove global pointers for RTC, Screen, VIA1, VIA2 devices`

---

### Step 5.9 — Multi-model validation: Mac Plus

Boot a Mac Plus ROM alongside the existing Mac II config from the same binary. This is the acid test for multi-model support.

**What's different about Mac Plus vs. Mac II:**

| Feature | Mac Plus | Mac II |
|---------|----------|--------|
| CPU | 68000 (no 68020) | 68020 |
| FPU | No | Yes (68881) |
| RAM | 4 MB (single bank) | 8 MB (2×4 MB) |
| Screen | 512×342, 1-bit | 640×480, 8-bit |
| VIA2 | No | Yes |
| ADB | No | Yes |
| Classic Keyboard | Yes | No |
| Classic Sound | Yes (VIA1-driven) | No (uses ASC) |
| Video Card | No (built-in framebuffer) | Yes (NuBus video) |
| Address space | 24-bit | 32-bit capable |
| Wire topology | Different VIA1 port B mapping, no VIA2 wires, keyboard on CB2 | ADB on CB2, VIA2 wires, video VBL |

**Test procedure:**
1. Build with no model-specific `#define`s in `CNFUDPIC.h` (all model config from `MachineConfig`).
2. Run: `./minivmac --model=Plus --rom=extras/roms/MacPlusKanji.ROM extras/disks/608.hfs`
   - Alternatively, use `vMac.ROM` (Plus-compatible ROM) if available.
3. Expected: System boots to desktop. Mouse tracks. Keyboard types. Clock works. Floppy disk access works. Sound plays (beep on alert).

**Debug approach if it doesn't boot:**
- Use the existing disassembler (Phase 8 will add interactive debug, but for now, add a `--trace` flag that logs PC + opcode for the first N instructions).
- Compare the first 1000 instructions against a known-good 68000 emulation.
- Check address map: verify ROM is at 0x00400000 (Plus ROM base, not 0x00800000 like Mac II).
- Check VIA1 initialization: verify port B mask, timer values.

**Fixes expected:**
- Address map for Plus will need different `SetUp_address()` logic (24-bit, ROM at different base, no VRAM slot, no NuBus).
- The `MachineConfigForModel(MacModel::Plus)` already sets the correct feature flags — but the address map code must correctly branch on `config.isCompactMac()`.
- ROM size detection: Plus ROM is 64 KB (`0x00010000`) or 128 KB. Mac II ROM is 256 KB. ROM loading must use the correct size.

**Validation:**
- [ ] Mac Plus boots to desktop with System 6 (or compatible System 7)
- [ ] Mac Plus keyboard types correctly (classic keyboard protocol)
- [ ] Mac Plus startup beep plays (classic sound)
- [ ] Mac Plus floppy disk access works
- [ ] Mac II still boots correctly (regression test)
- [ ] Same binary, different `--model` flag → different Mac

**Commit:** `Validate Mac Plus boot alongside Mac II from single binary`

---

### Step 5.10 — Add command-line argument parsing

Add a proper argument parser so the model, ROM, RAM, screen, and disk images can be specified at runtime.

**Dependencies:** `toml++` (header-only, via CMake FetchContent) for config files. Standard `getopt_long` or a lightweight C++ arg parser for command line.

**New file: `src/core/config_loader.h` + `config_loader.cpp`:**
```cpp
struct LaunchConfig {
    MacModel model     = MacModel::II;
    std::string romPath;
    std::vector<std::string> diskPaths;
    uint32_t ramMB      = 0;  // 0 = use model default
    uint16_t screenW    = 0;  // 0 = use model default
    uint16_t screenH    = 0;
    uint8_t  screenDepth = 0;
    int      speed      = 0;  // 0 = model default, 1 = 1x, 8 = 8x, -1 = all-out
    bool     fullscreen = false;
    std::string configFile;
};

// Parse command-line arguments into LaunchConfig.
LaunchConfig ParseCommandLine(int argc, char* argv[]);

// Load a TOML config file, return a LaunchConfig.
// Command-line values override file values.
LaunchConfig LoadConfigFile(const std::string& path);

// Merge command-line config over file config.
LaunchConfig MergeConfigs(const LaunchConfig& file, const LaunchConfig& cmdLine);

// Convert LaunchConfig → MachineConfig.
MachineConfig BuildMachineConfig(const LaunchConfig& launch);
```

**Command-line interface:**
```
Usage: minivmac [options] [disk1.img] [disk2.img] ...

Options:
  --model=MODEL    Mac model: Plus, SE, II, IIx (default: II)
  --rom=PATH       Path to ROM file
  --ram=SIZE       RAM size: 1M, 2M, 4M, 8M (default: model-specific)
  --screen=WxHxD   Screen: 512x342x1, 640x480x8, etc.
  --speed=N        Emulation speed: 1 (1x), 2, 4, 8, 0 (all-out)
  --fullscreen     Start in fullscreen mode
  --config=FILE    Load config from TOML file
  -h, --help       Show this help
```

**Model name mapping:**
```
Plus     → MacModel::Plus
SE       → MacModel::SE
SEFDHD   → MacModel::SEFDHD
Classic  → MacModel::Classic
PB100    → MacModel::PB100
II       → MacModel::II
IIx      → MacModel::IIx
128K     → MacModel::Mac128K
512Ke    → MacModel::Mac512Ke
```

**TOML config file format:**
```toml
model = "II"
rom = "MacII.ROM"
ram = "8M"

[screen]
width = 640
height = 480
depth = 8

[disks]
paths = ["system7.img", "apps.img"]

[speed]
multiplier = 1  # 1x real speed
```

**Integration with platform backends:**
- Cocoa: `main()` in `cocoa.mm` calls `ParseCommandLine()` before creating the Machine.
- SDL: `main()` in `sdl.cpp` already has `ScanCommandLine()` — replace with `ParseCommandLine()`.
- The ROM path from `--rom` overrides the compiled-in `RomFileName`. ROM size is determined from the model config, not a compile-time constant.

**CMake changes:**
- Add `FetchContent` for `toml++` (or vendored copy in `extern/`).
- Add new source files to `CMakeLists.txt`.

**Validation:**
```bash
# Mac II (default)
./minivmac --rom=MacII.ROM extras/disks/608.hfs
# Mac Plus with explicit config
./minivmac --model=Plus --rom=extras/roms/vMac.ROM extras/disks/608.hfs
# From config file
./minivmac --config=my_macplus.toml
```

**Commit:** `Add command-line argument parser and TOML config file support`

---

### Step 5.11 — Dynamic ROM loading by model

Make ROM loading model-aware: the correct ROM size, base address, and validation are determined from `MachineConfig`, not compile-time constants.

**Current state:**
- `kROM_Size` is `0x00040000` (256 KB) in `CNFUDALL.h` — Mac II specific.
- `kROM_Base` is `0x00800000` in `CNFUDPIC.h` — Mac II specific.
- `RomFileName` is `"MacII.ROM"` in `CNFUDOSG.h`.
- ROM is loaded as a flat binary read into a fixed-size buffer.

**Changes:**
1. Add to `MachineConfig`:
   ```cpp
   uint32_t romSize     = 0x00040000;  // 256 KB for Mac II
   uint32_t romBase     = 0x00800000;  // Mac II ROM base address
   std::string romFileName = "MacII.ROM";
   ```
2. Update `MachineConfigForModel()`:
   - Plus: `romSize = 0x00010000` (64 KB) or `0x00020000` (128 KB), `romBase = 0x00400000`, `romFileName = "vMac.ROM"`.
   - SE: `romSize = 0x00040000`, `romBase = 0x00400000`, `romFileName = "MacSE.ROM"`.
   - II/IIx: `romSize = 0x00040000`, `romBase = 0x00800000`, `romFileName = "MacII.ROM"`.
3. ROM loading (`LoadMacRom()`) reads `config.romSize` bytes.
4. ROM base address used in `SetUp_address()` from `config.romBase`.
5. ROM validation accepts both 64 KB and 256 KB ROMs (detect by file size).
6. Remove `kROM_Size` from `CNFUDALL.h` and `kROM_Base` from `CNFUDPIC.h`.
7. The `--rom` CLI flag from Step 5.10 overrides `romFileName`.

**Validation:** Boot Mac II and Mac Plus with their respective ROMs.

**Commit:** `Dynamic ROM loading: size, base address, and filename from MachineConfig`

---

### Step 5.12 — Clean up CNFUDPIC.h and CNFUDALL.h

At this point, most config has moved to `MachineConfig`. Audit and remove the remaining compile-time defines.

**CNFUDPIC.h — items to remove or relocate:**

| Define | Disposition |
|--------|-------------|
| `EmClassicKbrd`, `EmClassicSnd`, `EmPMU` | Already removed (Step 5.4) |
| `Use68020`, `EmFPU`, `EmMMU` | Already removed (Step 5.5) |
| `kMyClockMult` | → `MachineConfig::clockMult` |
| `WantCycByPriOp`, `WantCloserCyc` | → `MachineConfig` or keep as build-time tuning |
| `kAutoSlowSubTicks`, `kAutoSlowTime` | → `MachineConfig` fields (already there) |
| `kRAMa_Size`, etc. | Already removed (Step 5.6) |
| `MaxATTListN` | Already removed (Step 5.6) |
| `IncludeExtnPbufs`, `IncludeExtnHostTextClipExchange` | Keep as build-time features (platform capability) |
| `Sony_*` preferences | → `MachineConfig` or keep as compile-time (they're build-independent) |
| `CaretBlinkTime`, `SpeakerVol`, etc. | PRAM defaults → `MachineConfig::pramDefaults` |
| Wire enum + all `#define` aliases | Already removed (Step 5.1) |
| VIA config `#define`s | Already removed (Step 5.1) |
| Pulse notification aliases | Already removed (Step 5.1) |
| `ADB_ShiftInData`/`ADB_ShiftOutData` aliases | Already removed (Step 5.1) |
| `kExtn_Block_Base`, `kExtn_ln2Spc` | → `MachineConfig` (model-specific) |
| `kROM_Base`, `kROM_ln2Spc` | Already removed (Step 5.11) |
| `WantDisasm`, `ExtraAbnormalReports` | Keep as build-time debug options or → `MachineConfig` |

**CNFUDALL.h — items to remove or relocate:**

| Define | Disposition |
|--------|-------------|
| `vMacScreenWidth/Height/Depth` | Already removed (Step 5.7) |
| `kROM_Size` | Already removed (Step 5.11) |
| Sound format constants (`kLn2SoundSampSz`, etc.) | Keep or → `MachineConfig` |
| `NumDrives` | → `MachineConfig::numDrives` |

**Goal:** `CNFUDPIC.h` should become nearly empty — ideally just build-time feature toggles like `IncludeExtnPbufs` and debug flags. Everything model-specific is gone.

**Also remove `cmake/models/MacII_CNFUDPIC.h`** — superseded by `MachineConfigForModel()`.

**Validation:** Full clean build. Boot Mac II + Mac Plus.

**Commit:** `Clean up CNFUDPIC.h and CNFUDALL.h: remove all model-specific defines`

---

### Step 5.13 — Multi-model CMake and build cleanup

Update the build system to reflect the new single-binary multi-model reality.

**CMakeLists.txt changes:**
1. Remove `MINIVMAC_MODEL` CMake option (no longer needed — model is runtime).
2. Remove `MINIVMAC_SCREEN_WIDTH/HEIGHT/DEPTH` CMake options (runtime config).
3. Remove `configure_file()` for `CNFUDPIC.h` — it's now a minimal static file.
4. Simplify `configure_file()` for `CNFUDALL.h` — fewer template variables.
5. Remove per-model CNFUDPIC template selection logic.
6. Add `toml++` as a dependency (if Step 5.10 hasn't already).
7. Add new source files: `wire_topology.cpp`, `config_loader.cpp`.
8. Remove deleted files from source lists.

**Documentation:**
- Update `docs/BUILDING.md` with new runtime flags.
- Remove references to `MINIVMAC_MODEL` build option.

**Validation:** Clean build from fresh checkout.

**Commit:** `Update CMake for single-binary multi-model build`

---

### Step 5.14 — Additional model validation (SE, Classic, PB100)

Validate models beyond Mac II and Mac Plus.

**Mac SE:**
```bash
./minivmac --model=SE --rom=extras/roms/MacSE.ROM extras/disks/608.hfs
```
SE is like Plus but with ADB (no classic keyboard) and SCSI. Should be straightforward if Plus and II both work.

**Mac Classic:**
```bash
./minivmac --model=Classic --rom=extras/roms/Classic.ROM extras/disks/608.hfs
```
Very similar to SE. Different ROM, same architecture.

**PB100 (PowerBook 100):**
```bash
./minivmac --model=PB100 --rom=extras/roms/PB100.ROM extras/disks/608.hfs
```
This is the only model that uses PMU. Tests the PMU decoupling (Step 5.4). Has ASC sound, 640×400 1-bit screen.

**For each model, validate:**
- [ ] Boots to desktop
- [ ] Keyboard input works (ADB or classic depending on model)
- [ ] Mouse tracks
- [ ] Disk access works
- [ ] Sound plays
- [ ] Clock keeps time

**Fix model-specific bugs as they arise.** Expect:
- Address map differences for compact Macs (24-bit, no NuBus slots)
- ROM checksum validation differences
- Interrupt priority scheme differences (compact vs. II)

**Commit:** `Validate SE, Classic, and PB100 models`

---

### Step 5.15 — Final cleanup and documentation

1. **Audit for remaining globals:** `grep -rn "^static " src/devices/ src/core/ src/cpu/` — any static mutable state that isn't inside a class must be migrated. This is critical for future multi-instance support (Phase 7 testing).
2. **Remove dead code:** Any free-function stubs, unused `#define` macros, orphaned declarations.
3. **Update `docs/INSIGHTS.md`:** Document the new architecture — single-binary, runtime config, wire topology factory, CPU feature selection.
4. **Update `docs/FULL_PLAN.md`:** Mark Phase 5 as complete, update "What remains compile-time" to reflect current state.
5. **Run compiler diagnostics:**
   ```bash
   cmake --build --preset macos-cocoa -- -Wall -Wextra -Wpedantic 2>&1 | head -100
   ```
   Address any new warnings introduced by the conversion.

**Validation:**
```bash
rm -rf bld/macos-cocoa && cmake --preset macos-cocoa && cmake --build --preset macos-cocoa
# Boot all validated models:
./minivmac --model=II    --rom=extras/roms/MacII.ROM  extras/disks/608.hfs
./minivmac --model=Plus  --rom=extras/roms/vMac.ROM   extras/disks/608.hfs
./minivmac --model=SE    --rom=extras/roms/MacSE.ROM  extras/disks/608.hfs
```

**Commit:** `Phase 5 complete: multi-model support and runtime configuration`

---

## File Changes Summary

### New Files

| File | Purpose |
|------|---------|
| `src/core/wire_ids.h` | Superset WireID enum for all models |
| `src/core/wire_topology.h` | Per-model wire callback registration declarations |
| `src/core/wire_topology.cpp` | Per-model wire callback registration implementations |
| `src/core/config_loader.h` | CLI argument parser + TOML config loader declarations |
| `src/core/config_loader.cpp` | CLI argument parser + TOML config loader implementations |
| `extern/tomlplusplus/` (or FetchContent) | TOML config file parsing library |

### Heavily Modified Files

| File | Nature of change |
|------|-----------------|
| `src/cpu/m68k.cpp` | 55× `#if Use68020` → `if (use68020)`, plus EmFPU/EmMMU |
| `src/cpu/m68k_tables.cpp` | 17× `#if Use68020` → `if (use68020)`, plus EmFPU/EmMMU |
| `src/cpu/disasm.cpp` | 33× `#if Use68020` → `if (use68020)` |
| `src/devices/keyboard.cpp` | Remove `#if EmClassicKbrd` guard, decouple from VIA macros |
| `src/devices/sound.cpp` | Remove `#if EmClassicSnd` guard, decouple from VIA macros |
| `src/devices/pmu.cpp` | Remove `#if EmPMU` guard, decouple from VIA macros |
| `src/core/machine.cpp` | ~30× `kRAMa_Size` etc. → `config.ramASize` etc. |
| `src/core/machine_obj.cpp` | Remove `#if Em*` guards, runtime device creation |
| `src/core/machine_config.h` | Add romSize, romBase, romFileName, numDrives, pramDefaults, VIAConfig |
| `src/core/machine_config.cpp` | Add ROM/VIA config per model |
| `src/config/CNFUDPIC.h` | Remove ~200 lines (Wire enum, VIA config, model defines). Nearly empty. |
| `src/config/CNFUDALL.h` | Remove screen size, ROM size defines |
| `src/platform/cocoa.mm` | Screen size → runtime, CLI args, ROM loading |
| `src/platform/sdl.cpp` | Screen size → runtime, CLI args, ROM loading |
| `src/platform/common/osglu_common.cpp` | Screen size → runtime |
| `src/platform/platform.h` | Screen size accessors |
| `src/devices/video.cpp` | Screen/VRAM sizes → runtime |
| `src/devices/via.h` / `via.cpp` | Accept VIAConfig, remove `g_via1` |
| `src/devices/via2.h` / `via2.cpp` | Accept VIAConfig, remove `g_via2` |
| All device `.h`/`.cpp` files | Remove `g_xxx` global pointers and free-function stubs |
| `CMakeLists.txt` | Remove model/screen CMake options, add new sources, add toml++ dep |

### Deleted Files

| File | Reason |
|------|--------|
| `cmake/models/MacII_CNFUDPIC.h` | Superseded by `MachineConfigForModel()` |

---

## Commit Sequence

| # | Commit Message | Steps | Risk |
|---|---------------|-------|------|
| 1 | `Replace model-specific Wire enum with superset WireID and per-model topology factory` | 5.1 | **High** — changes all wire interactions |
| 2 | `Decouple keyboard.cpp from VIA: always compiles, uses WireBus + findDevice` | 5.2 | Medium |
| 3 | `Decouple sound.cpp from VIA: always compiles, uses WireBus + findDevice` | 5.3 | Medium |
| 4 | `Decouple pmu.cpp from VIA; remove EmClassicKbrd/EmClassicSnd/EmPMU compile-time guards` | 5.4 | Medium |
| 5 | `Convert Use68020/EmFPU/EmMMU to runtime in m68k_tables.cpp` | 5.5a | Medium |
| 6 | `Convert Use68020/EmFPU/EmMMU to runtime in m68k.cpp` | 5.5b | **High** — 9000-line file, 55+ changes |
| 7 | `Convert Use68020 to runtime in disasm.cpp` | 5.5c | Low |
| 8 | `Remove Use68020/EmFPU/EmMMU from CNFUDPIC.h` | 5.5d | Low |
| 9 | `Convert memory size constants to runtime MachineConfig fields` | 5.6 | Medium |
| 10 | `Convert screen size to runtime in device code` | 5.7a | Medium |
| 11 | `Convert screen size to runtime in platform common code` | 5.7b | Medium |
| 12 | `Convert screen size to runtime in cocoa.mm backend` | 5.7c | Medium |
| 13 | `Refactor screen_map/screen_translate to runtime depth dispatch` | 5.7d | **High** — C-template pattern refactor |
| 14 | `Add backward-compat screen macros for legacy backends; remove from CNFUDALL.h` | 5.7e | Low |
| 15 | `Remove global pointers for SCSI, ASC, PMU, ROM devices` | 5.8a | Low |
| 16 | `Remove global pointers for IWM, Sony, Video, Mouse devices` | 5.8b | Low |
| 17 | `Remove global pointers for SCC, ADB, Keyboard, Sound devices` | 5.8c | Medium |
| 18 | `Remove global pointers for RTC, Screen, VIA1, VIA2 devices` | 5.8d | Medium |
| 19 | `Validate Mac Plus boot alongside Mac II from single binary` | 5.9 | **High** — first multi-model test |
| 20 | `Add command-line argument parser and TOML config file support` | 5.10 | Medium |
| 21 | `Dynamic ROM loading: size, base address, and filename from MachineConfig` | 5.11 | Medium |
| 22 | `Clean up CNFUDPIC.h and CNFUDALL.h: remove all model-specific defines` | 5.12 | Low |
| 23 | `Update CMake for single-binary multi-model build` | 5.13 | Low |
| 24 | `Validate SE, Classic, and PB100 models` | 5.14 | Medium |
| 25 | `Phase 5 complete: multi-model support and runtime configuration` | 5.15 | Low |

---

## Execution Dependencies

```
5.1 ── 5.2 ── 5.3 ── 5.4 ──┐
                             ├── 5.8 ── 5.9 ── 5.14
5.5a ── 5.5b ── 5.5c ── 5.5d ──┤
                                ├── 5.12 ── 5.13 ── 5.15
5.6 ────────────────────────────┤
                                │
5.7a ── 5.7b ── 5.7c ── 5.7d ── 5.7e ──┘
                                        │
5.10 ── 5.11 ──────────────────────────┘
```

- **5.1** (wire topology) must come first — it's the foundation for decoupling VIA cross-deps.
- **5.2–5.4** (device decoupling) depend on 5.1 and must precede 5.8 (global pointer removal) and 5.4 specifically removes the last `#if Em*` guards.
- **5.5** (CPU runtime) is independent of 5.1–5.4 — can run in parallel.
- **5.6** (memory sizes) is independent of 5.1–5.5 — can run in parallel.
- **5.7** (screen sizes) is independent of 5.1–5.6 — can run in parallel.
- **5.8** (remove globals) depends on 5.2–5.4 (VIA cross-deps resolved) and benefits from 5.7 (platform code cleaned up).
- **5.9** (Mac Plus validation) depends on 5.1–5.8 (all runtime conversions done).
- **5.10–5.11** (CLI + ROM loading) can start after 5.6 (memory sizes runtime) but are best done before 5.9 (to allow `--model` flag for testing).
- **5.12–5.13** (cleanup + CMake) depend on all conversion steps.
- **5.14** (more models) depends on 5.9 (Mac Plus working).
- **5.15** (final) depends on everything.

---

## Validation Checklist

After each commit, the following must pass:

### Build validation
```bash
rm -rf bld/macos-cocoa && cmake --preset macos-cocoa && cmake --build --preset macos-cocoa
```
Zero errors. Warning count should not increase.

### Boot validation (Mac II — every commit)
- [ ] System 7 boots to desktop (MacII.ROM + 608.hfs)
- [ ] Startup chime plays (ASC sound)
- [ ] Mouse tracks correctly
- [ ] Keyboard input works
- [ ] Clock shows correct time
- [ ] Floppy disk eject/insert works
- [ ] Control mode overlay works

### Performance validation (after Step 5.5)
- [ ] Boot-to-desktop time within 5% of pre-Phase-5 baseline
- [ ] No perceptible slowdown in emulated application execution
- [ ] Cursor blink rate unchanged

### Multi-model validation (after Step 5.9)
- [ ] Mac Plus boots with 68000 instruction set
- [ ] Mac Plus keyboard works (classic serial keyboard protocol)
- [ ] Mac Plus sound works (VIA1-driven classic sound)
- [ ] Mac Plus screen is 512×342 1-bit
- [ ] Mac II still boots (regression test after Plus changes)

### Full validation (after Step 5.14)
- [ ] Mac Plus boots (68000, classic kbd, no VIA2, classic sound, 512×342 1-bit)
- [ ] Mac SE boots (68000, ADB, no VIA2, classic sound, 512×342 1-bit)
- [ ] Mac Classic boots (68000, ADB, no VIA2, classic sound, 512×342 1-bit)
- [ ] Mac II boots (68020+FPU, ADB, VIA2, ASC, 640×480 8-bit)
- [ ] PB100 boots (68000, PMU, ASC, 640×400 1-bit) — if ROM available
- [ ] Single binary serves all models via `--model` flag
- [ ] TOML config file works

### CLI validation (after Step 5.10)
```bash
# Help
./minivmac --help
# Model selection
./minivmac --model=Plus --rom=vMac.ROM disk.img
# RAM override
./minivmac --model=II --ram=2M --rom=MacII.ROM disk.img
# Screen override
./minivmac --model=II --screen=800x600x8 --rom=MacII.ROM disk.img
# Config file
./minivmac --config=test.toml
```

---

## Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|-----------|
| **Wire topology mismatch** — wrong callbacks registered for a model → silent malfunction | Keyboard doesn't work, interrupts don't fire, devices hang | Comprehensive per-model validation. Add assertions that critical wires have exactly the expected number of callbacks. Log wire activity in debug builds. |
| **CPU `if (use68020)` performance** — 55 branches in instruction handlers slow down emulation | Measurable performance regression | Branch predictor handles constant-value branches perfectly (~100% prediction rate). Benchmark before/after: run a tight emulated loop, measure host cycles per emulated cycle. Expected impact: <1%. If measured >2%, convert hot-path checks to function pointer tables initialized at CPU::init(). |
| **Sound buffer address calculation wrong** — `kRAM_Size` replaced with dynamic value computed from wrong fields | Crackling audio or crash | Unit test: create a SoundDevice with known RAM size, verify buffer offsets match hand-calculated values. |
| **Screen depth dispatch table wrong** — runtime depth selection calls wrong screen map function | Corrupt display, visual artifacts | Test each supported depth (1, 2, 4, 8 bpp) independently. Compare framebuffer output pixel-by-pixel against known-good reference (screenshot at each depth). |
| **Mac Plus address map wrong** — ROM base, RAM mirror wrong, interrupt vectors misplaced | Crash at boot or ROM checksum error | Compare the generated address map (ATT entries) against the original minivmac setup tool's output for Mac Plus config. Add a `--dump-att` debug flag. |
| **PMU communication protocol broken** — VIA wire abstraction changes timing or bit ordering | PB100 won't boot, hangs at PMU init | PMU is timing-sensitive. Preserve exact cycle counts when translating wire reads/writes. Add debug logging for PMU bus transactions. |
| **CNFUDPIC.h cleanup removes something still referenced** — a define we missed is used somewhere | Build error | Run `grep -rn DEFINE_NAME src/` before removing each define. Build after each removal. |
| **Global pointer removal breaks platform code** — platform backends access `g_screen` or `g_via1` at shutdown/error paths we missed | Crash during cleanup or error handling | grep comprehensively. Test clean shutdown. Test error paths (missing ROM, bad disk image). |
| **toml++ compilation issues** — header-only library may conflict with project settings | Build errors | toml++ is well-tested with C++17. Pin a known-good version. Isolate in a wrapper header to limit template instantiation scope. |
| **Multiple models expose hidden assumptions** — code assumes Mac II layout in places we didn't audit | Subtle wrong behavior on Plus/SE | Run each model through a deterministic boot test. Compare checksum of VRAM after N million cycles against reference. |

---

## Estimated Effort

| Steps | Description | Effort |
|-------|-------------|--------|
| 5.1 | Superset WireID enum + topology factory | ~6–8 hours |
| 5.2–5.4 | Decouple keyboard, sound, PMU from VIA | ~4–6 hours |
| 5.5 | CPU instruction set → runtime (m68k.cpp, tables, disasm) | ~8–12 hours |
| 5.6 | Memory sizes → runtime | ~3–4 hours |
| 5.7 | Screen sizes → runtime (device + platform + depth dispatch) | ~10–14 hours |
| 5.8 | Remove global device pointers | ~4–6 hours |
| 5.9 | Mac Plus validation + bug fixes | ~6–10 hours |
| 5.10 | CLI argument parser + TOML config | ~4–6 hours |
| 5.11 | Dynamic ROM loading | ~2–3 hours |
| 5.12–5.13 | CNFUDPIC/CNFUDALL cleanup + CMake update | ~3–4 hours |
| 5.14 | Additional model validation (SE, Classic, PB100) | ~4–8 hours |
| 5.15 | Final cleanup + docs | ~2–3 hours |
| **Total** | | **~55–85 hours** |

---

## Post-Phase 5 State

After this phase:

- **A single binary emulates any supported Mac model** — Mac Plus, SE, Classic, PB100, Mac II, Mac IIx — selected via `--model` flag or TOML config file.
- **Zero compile-time model dependencies** — `CNFUDPIC.h` is nearly empty; all model config flows through `MachineConfig`.
- **CPU instruction set is runtime-selectable** — 68000/68020, FPU on/off, MMU on/off.
- **Memory sizes are dynamic** — RAM, VRAM, ROM allocated per model.
- **Screen dimensions are runtime** — platform code adapts to any resolution/depth.
- **No global device pointers** — all device access through `Machine` (except `g_machine` as the single entry point).
- **Wire topology is per-model** — `WireBus` callbacks registered by a model-specific factory.
- **Command-line interface** — `./minivmac --model=Plus --rom=vMac.ROM disk.img`.
- **TOML config files** for saved configurations.

**Result:** `./minivmac --model=MacII --ram=8M --screen=800x600x8 --rom=MacII.ROM disk1.img`

### What's next (Phase 6)

With the emulator fully runtime-configurable and multi-model, Phase 6 consolidates the 9 platform backends down to 2 (Cocoa + SDL), with clean interface separation.
