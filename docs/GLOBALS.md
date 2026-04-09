# Global Variables Audit

Full review of all mutable global state inherited from minivmac.
Excludes `static constexpr` / `inline constexpr` compile-time constants and `static const` lookup tables
(these are immutable data, not state).

## Disposition Legend

| Tag | Meaning |
|-----|---------|
| **KEEP** | Architecturally justified long-term, or already well-encapsulated (static file-scope) |
| **REMOVE** | Can be eliminated now with straightforward refactoring |
| **NEEDS WORK** | Should go eventually, but requires design or restructuring first |

---

## 1. Root Singleton Pointers

| Variable | Type | File | Tag | Notes |
|----------|------|------|-----|-------|
| `g_machine` | `Machine*` | core/machine_obj.h | **KEEP** | Central Machine singleton; accessor pattern already wraps it |
| `g_shell` | `EmulatorShell*` | platform/emulator_shell.h | **KEEP** | Platform shell singleton; needed for legacy free-function wrappers |
| `g_cpu` | `CPU` | cpu/cpu.cpp | **NEEDS WORK** | Should be owned by Machine. Currently standalone global instance |

## 2. Machine Subsystem Objects

| Variable | Type | File | Tag | Notes |
|----------|------|------|-----|-------|
| `g_ict` | `ICTScheduler` | core/main.cpp | **NEEDS WORK** | Should be owned by Machine |
| `g_wires` | `WireBus` | core/wire_bus.cpp | **NEEDS WORK** | Should be owned by Machine |
| `g_recorder` | `StateRecorder` | core/state_recorder.cpp | **KEEP** | Debug/test tool, orthogonal to emulation |
| `g_wiresData` | `uint8_t*` | core/machine.cpp | **REMOVE** | Compatibility shim into WireBus; replace with `g_wires.data()` |

## 3. Memory Buffers

| Variable | Type | File | Tag | Notes |
|----------|------|------|-----|-------|
| `g_ram` | `uint8_t*` | core/machine.cpp | **NEEDS WORK** | Should be owned by Machine, but deeply threaded through ATT and CPU |
| `g_rom` | `uint8_t*` | platform/common/osglu_common.cpp | **NEEDS WORK** | ROM buffer — same as g_ram |
| `g_vidMem` | `uint8_t*` | core/machine.cpp | **NEEDS WORK** | Video RAM — should be owned by Machine/Video device |
| `g_vidROM` | `uint8_t*` | core/machine.cpp | **NEEDS WORK** | Video ROM — same |

## 4. Display & Screen

All 14 of these should become a single **`DisplayState`** struct on Shell or Machine.

| Variable | Type | File | Tag | Notes |
|----------|------|------|-----|-------|
| `g_screenWidth` | `uint16_t` | osglu_common.cpp | **NEEDS WORK** | |
| `g_screenHeight` | `uint16_t` | osglu_common.cpp | **NEEDS WORK** | |
| `g_screenDepth` | `uint8_t` | osglu_common.cpp | **NEEDS WORK** | |
| `g_useColorMode` | `bool` | osglu_common.cpp | **NEEDS WORK** | |
| `g_colorModeWorks` | `bool` | osglu_common.cpp | **NEEDS WORK** | |
| `g_colorMappingChanged` | `bool` | osglu_common.cpp | **NEEDS WORK** | |
| `g_colorTransValid` | `bool` | osglu_common.cpp | **NEEDS WORK** | |
| `CLUT_reds[256]` | `uint16_t[]` | osglu_common.cpp | **NEEDS WORK** | |
| `CLUT_greens[256]` | `uint16_t[]` | osglu_common.cpp | **NEEDS WORK** | |
| `CLUT_blues[256]` | `uint16_t[]` | osglu_common.cpp | **NEEDS WORK** | |
| `g_screenCompareBuff` | `uint8_t*` | osglu_common.cpp | **NEEDS WORK** | Shadow framebuffer for dirty tracking |
| `g_screenChanged` | `bool` | osglu_common.cpp | **NEEDS WORK** | Dirty flag |
| `ScalingBuff` | `uint8_t*` | screen_convert.h | **NEEDS WORK** | Output buffer for screen conversion |
| `CLUT_final` | `uint8_t*` | screen_convert.h | **NEEDS WORK** | Pre-computed CLUT |

## 5. View / Scroll

Bundle into **`ViewState`** on Shell.

| Variable | Type | File | Tag | Notes |
|----------|------|------|-----|-------|
| `g_viewHSize` | `uint16_t` | osglu_common.cpp | **NEEDS WORK** | |
| `g_viewVSize` | `uint16_t` | osglu_common.cpp | **NEEDS WORK** | |
| `g_viewHStart` | `uint16_t` | osglu_common.cpp | **NEEDS WORK** | |
| `g_viewVStart` | `uint16_t` | osglu_common.cpp | **NEEDS WORK** | |
| `g_savedMouseH` | `int16_t` | osglu_common.cpp | **NEEDS WORK** | |
| `g_savedMouseV` | `int16_t` | osglu_common.cpp | **NEEDS WORK** | |

## 6. Input (Mouse + Keyboard)

Bundle into **`InputState`** on Shell.

| Variable | Type | File | Tag | Notes |
|----------|------|------|-----|-------|
| `g_curMouseH` | `uint16_t` | osglu_common.cpp | **NEEDS WORK** | |
| `g_curMouseV` | `uint16_t` | osglu_common.cpp | **NEEDS WORK** | |
| `g_mousePosCurH` | `uint16_t` | osglu_common.cpp | **REMOVE** | Redundant cached copy of g_curMouseH |
| `g_mousePosCurV` | `uint16_t` | osglu_common.cpp | **REMOVE** | Redundant cached copy of g_curMouseV |
| `g_haveMouseMotion` | `bool` | osglu_common.cpp | **NEEDS WORK** | |
| `g_mouseButtonState` | `bool` | osglu_common.cpp | **NEEDS WORK** | |
| `theKeys[4]` | `uint32_t[]` | osglu_common.cpp | **NEEDS WORK** | Keyboard bitmap |
| `g_controlKeyPressed` | `bool` | keyboard_map.cpp | **NEEDS WORK** | |

## 7. Event Queue

Wrap into an **`EventQueue`** class (already a circular buffer pattern).

| Variable | Type | File | Tag | Notes |
|----------|------|------|-----|-------|
| `EvtQA[16]` | `EvtQEl[]` | osglu_common.h | **NEEDS WORK** | |
| `EvtQIn` | `uint16_t` | osglu_common.h | **NEEDS WORK** | |
| `EvtQOut` | `uint16_t` | osglu_common.h | **NEEDS WORK** | |
| `EvtQNeedRecover` | `bool` | osglu_common.h | **NEEDS WORK** | |
| `MasterEvtQLock` | `uint16_t` | machine.cpp | **NEEDS WORK** | Legacy lock counter; used by mouse device for sync |

## 8. Disk / Sony

Bundle into a **`DiskManager`** class.

| Variable | Type | File | Tag | Notes |
|----------|------|------|-----|-------|
| `g_sonyWritableMask` | `uint32_t` | osglu_common.cpp | **NEEDS WORK** | |
| `g_sonyInsertedMask` | `uint32_t` | osglu_common.cpp | **NEEDS WORK** | |
| `g_sonyRawMode` | `bool` | osglu_common.cpp | **NEEDS WORK** | Actively used by disk driver control calls |
| `g_sonyNewDiskWanted` | `bool` | osglu_common.cpp | **NEEDS WORK** | |
| `g_sonyNewDiskSize` | `uint32_t` | osglu_common.cpp | **NEEDS WORK** | |
| `g_sonyNewDiskName` | `PbufIndex` | osglu_common.cpp | **NEEDS WORK** | |
| `Drives[]` | `FILE*` | disk_io.h | **NEEDS WORK** | File handles |
| `DriveNames[]` | `char*` | disk_io.h | **NEEDS WORK** | Disk image paths |
| `g_diskIconAddr` | `uint32_t` | machine.cpp | **NEEDS WORK** | Set by ROM patching, read by Sony driver; narrow scope but cross-module |
| `g_pbufAllocatedMask` | `uint32_t` | osglu_common.cpp | **NEEDS WORK** | Parameter buffer pool — could be standalone or part of DiskManager |
| `PbufSize[]` | `uint32_t[]` | osglu_common.cpp | **NEEDS WORK** | Same |
| `PbufDat[]` | `void*[]` | param_buffers.h | **NEEDS WORK** | Same |

## 9. Machine Control & Power

Bundle into **`EmulatorState`** on Shell.

| Variable | Type | File | Tag | Notes |
|----------|------|------|-----|-------|
| `g_requestMacOff` | `bool` | osglu_common.cpp | **NEEDS WORK** | |
| `g_forceMacOff` | `bool` | osglu_common.cpp | **NEEDS WORK** | |
| `g_wantMacInterrupt` | `bool` | osglu_common.cpp | **NEEDS WORK** | |
| `g_wantMacReset` | `bool` | osglu_common.cpp | **NEEDS WORK** | |
| `g_interruptButton` | `bool` | machine.cpp | **NEEDS WORK** | |

## 10. Emulation Speed & Timing

Bundle into **`TimingState`** on Shell.

| Variable | Type | File | Tag | Notes |
|----------|------|------|-----|-------|
| `g_speedValue` | `uint8_t` | osglu_common.cpp | **NEEDS WORK** | |
| `g_SkipThrottle` | `bool` | osglu_common.cpp | **NEEDS WORK** | |
| `g_wantNotAutoSlow` | `bool` | osglu_common.cpp | **NEEDS WORK** | |
| `g_speedStopped` | `bool` | keyboard_map.cpp | **NEEDS WORK** | |
| `g_runInBackground` | `bool` | keyboard_map.cpp | **NEEDS WORK** | |
| `g_quietTime` | `uint32_t` | osglu_common.cpp | **NEEDS WORK** | |
| `g_quietSubTicks` | `uint32_t` | osglu_common.cpp | **NEEDS WORK** | |
| `g_trueEmulatedTime` | `uint32_t` | tick_timer.cpp | **NEEDS WORK** | |
| `g_newMacDateInSeconds` | `uint32_t` | tick_timer.cpp | **NEEDS WORK** | |
| `g_onTrueTime` | `uint32_t` | osglu_common.cpp | **NEEDS WORK** | |

## 11. Date & Location

| Variable | Type | File | Tag | Notes |
|----------|------|------|-----|-------|
| `g_curMacDateInSeconds` | `uint32_t` | osglu_common.cpp | **NEEDS WORK** | → TimingState |
| `g_curMacLatitude` | `uint32_t` | osglu_common.cpp | **NEEDS WORK** | Written into RTC PRAM at init (offset 0xE4). Feature from original minivmac — must be preserved, but should live in config/RTC, not as a bare global |
| `g_curMacLongitude` | `uint32_t` | osglu_common.cpp | **NEEDS WORK** | Same (PRAM offset 0xE8) |
| `g_curMacDelta` | `uint32_t` | osglu_common.cpp | **NEEDS WORK** | Timezone delta (PRAM offset 0xEC) |

## 12. UI Flags

| Variable | Type | File | Tag | Notes |
|----------|------|------|-----|-------|
| `g_wantFullScreen` | `bool` | keyboard_map.cpp | **NEEDS WORK** | → Shell config |
| `g_wantMagnify` | `bool` | keyboard_map.cpp | **NEEDS WORK** | → Shell config |
| `g_requestInsertDisk` | `bool` | keyboard_map.cpp | **NEEDS WORK** | → Shell command queue |
| `g_requestIthDisk` | `uint8_t` | keyboard_map.cpp | **NEEDS WORK** | Same |
| `SavedBriefMsg` | `const char*` | osglu_common.cpp | **REMOVE** | Active but legacy. Replace with std::string on Shell |
| `SavedLongMsg` | `const char*` | osglu_common.cpp | **REMOVE** | Same |
| `g_savedFatalMsg` | `bool` | osglu_common.cpp | **REMOVE** | Same |

## 13. LocalTalk Networking

Bundle into **`NetworkState`** struct when networking matures.

| Variable | Type | File | Tag | Notes |
|----------|------|------|-----|-------|
| `g_ltNodeHint` | `uint8_t` | osglu_common.cpp | **NEEDS WORK** | |
| `g_certainlyNotMyPacket` | `bool` | osglu_common.cpp | **NEEDS WORK** | |
| `g_ltTxBuffer` | `uint8_t*` | osglu_common.cpp | **NEEDS WORK** | |
| `g_ltTxBuffSz` | `uint16_t` | osglu_common.cpp | **NEEDS WORK** | |
| `g_ltRxBuffer` | `uint8_t*` | osglu_common.cpp | **NEEDS WORK** | |
| `g_ltRxBuffSz` | `uint32_t` | osglu_common.cpp | **NEEDS WORK** | |
| `e_p[2]` | `uint32_t[]` | osglu_common.cpp | **NEEDS WORK** | Entropy pool for random generation |
| `g_ltMyStamp` | `uint32_t` | osglu_common.cpp | **NEEDS WORK** | |

## 14. Paths & Init

| Variable | Type | File | Tag | Notes |
|----------|------|------|-----|-------|
| `rom_path` | `char*` | rom_loader.h | **REMOVE** | Should be a parameter, not global |
| `app_parent` | `char*` | dbglog_platform.cpp | **REMOVE** | Same |
| `g_romLoaded` | `bool` | osglu_common.cpp | **REMOVE** | Init flag checked by shell; could be return value or Shell member |

## 15. CPU Internals

| Variable | Type | File | Tag | Notes |
|----------|------|------|-----|-------|
| `regs` | `struct regstruct` | m68k.cpp | **KEEP** | CPU register file; static file-scope, well-encapsulated |
| `g_InstructionCount` | `uint32_t` | m68k.cpp | **NEEDS WORK** | Debug counter — should be on CPU or Machine |
| `g_LogStart` | `uint32_t` | m68k.cpp | **NEEDS WORK** | Debug range — same |
| `g_LogEnd` | `uint32_t` | m68k.cpp | **NEEDS WORK** | Same |
| `g_regs` | `register regstruct*` | m68k.cpp | **KEEP** | Performance-critical register-pinned variable |
| `g_pc_p` | `register uint8_t*` | m68k.cpp | **KEEP** | Same |
| `g_MaxCyclesToGo` | `register int32_t` | m68k.cpp | **KEEP** | Same |
| `g_pc_pHi` | `register uint8_t*` | m68k.cpp | **KEEP** | Same |
| `fpu_dat` | `fpustruct` | fpu_emdev.h | **KEEP** | FPU state; static file-scope |
| `s_cpuConfig` | `const MachineConfig*` | m68k.cpp | **KEEP** | Cached config pointer; static file-scope |

## 16. Device File-Scope Statics

These are `static` (file-scope only) — not visible outside their translation unit.
They're inherited minivmac style but already encapsulated. Ideally they move to
class members on their respective Device subclasses as device refactoring continues.

| Area | Count | Tag | Notes |
|------|-------|-----|-------|
| ASC (sound chip) | ~15 | **KEEP** | Registers, sample buffer, FIFO pointers, playing flag |
| ADB | ~12 | **KEEP** | Data buffer, command state, mouse/keyboard addresses and deltas |
| IWM | 1 | **REMOVE** | `IWM` struct missing `static` keyword — only used internally, never externed |
| RTC | 2 | **KEEP** | `s_rtc` state, `s_lastRealDate` |
| SCSI | 1 | **KEEP** | Register array |
| SCC | ~9 | **KEEP** | Channel state, LocalTalk CTS/node vars |
| Sony | ~8 | **KEEP** | Mount mask, image metadata, delay counter, callbacks |
| Sound | ~4 | **KEEP** | Volume lookup tables, subtick tables |
| Video | 3 | **KEEP** | Driver binary, patch pointer, grayscale flag |
| ROM | 2 | **KEEP** | Embedded Sony driver and disk icon (const binary data) |
| Screen | 1 | **KEEP** | `kMain_Offset` constexpr |

## 17. Debug & Diagnostics (all static file-scope)

| Variable | File | Tag | Notes |
|----------|------|-----|-------|
| `s_counters` (atomic array) | trap_counter.cpp | **KEEP** | Trap profiling |
| `s_dict` (TrapInfo array) | trap_counter.cpp | **KEEP** | Trap name dictionary |
| `s_watchlist`, `s_revIndex` | trap_counter.cpp | **KEEP** | Trap UI state |
| `Disasm_*` (6 vars) | disasm.cpp | **KEEP** | Disassembler state |
| `SavedPCs` ring buffer | disasm.cpp | **KEEP** | PC history |
| `DumpTable` | m68k.cpp | **KEEP** | Instruction profiling (debug builds only) |
| `s_clipCache` etc. (5 vars) | extn_clip.cpp | **KEEP** | Clipboard bridge cache |
| `s_consoleLines` | extn_clip.cpp | **KEEP** | Debug console buffer |
| `dbglog_File` | dbglog_platform.cpp | **KEEP** | Log file handle |

## 18. Static File-Scope in core/main.cpp

Not globals — only visible within main.cpp. Listed for completeness.

| Variable | Type | Tag | Notes |
|----------|------|-----|-------|
| `s_machine` | `unique_ptr<Machine>` | **KEEP** | Actual Machine object; `g_machine` points here |
| `s_launchConfig` | `LaunchConfig` | **KEEP** | Startup config |
| `s_machineConfig` | `MachineConfig` | **KEEP** | Hardware config |
| `s_emulatorConfig` | `EmulatorConfig` | **KEEP** | Behavior config |
| `s_subTickCounter` | `uint16_t` | **KEEP** | Audio sub-tick position |
| `s_ticksSinceSecond` | `int` | **KEEP** | RTC update counter |
| `s_extraSubTicksToDo` | `uint32_t` | **KEEP** | Speed multiplier accumulator |

---

## Summary

| Disposition | Count | Description |
|-------------|-------|-------------|
| **KEEP** | ~30 | Architecturally sound, well-encapsulated, or performance-critical |
| **REMOVE** | ~10 | Straightforward elimination (shims, redundant copies, bare pointers) |
| **NEEDS WORK** | ~80 | Require struct consolidation or ownership changes |

The ~80 NEEDS WORK globals cluster into natural refactoring targets:

| Target Struct/Class | Globals Absorbed | Owner |
|---------------------|-----------------|-------|
| **`DisplayState`** | 14 display globals | Shell or Machine |
| **`InputState`** | 8 mouse/keyboard globals | Shell |
| **`EventQueue`** class | 5 event queue globals | Shell |
| **`DiskManager`** | 12 sony/disk/pbuf globals | Shell |
| **`TimingState`** | 10 speed/timing globals | Shell |
| **`NetworkState`** | 8 LocalTalk globals | Shell (when networking matures) |
| **`EmulatorState`** | 5 control/power globals | Shell |
| **Machine ownership** | `g_cpu`, `g_ict`, `g_wires`, `g_ram`, `g_rom`, `g_vidMem`, `g_vidROM` | Machine class |
| **`DateConfig`** | lat, long, delta, date | Config → RTC at init |

Each struct consolidation is independent and can be done incrementally without breaking
the non-regression tests.
