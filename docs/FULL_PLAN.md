# Plan: Modernize minivmac into a Hackable Emulator Platform

**TL;DR:** Transform minivmac from a preprocessor-configured, compile-time-variant C89 emulator into a runtime-configurable, C++ emulator with a clean device model, modern build system, and an external control API. The work proceeds in 9 phases, each leaving the emulator fully buildable and runnable. The most impactful early wins are: replacing the 3-stage build pipeline with CMake, eliminating custom types, and introducing a `Machine` object that owns all state — which unblocks everything else (runtime config, testing, debugger, MCP server).

---

## Phase 1 — CMake Build System

Replace the 3-stage pipeline (compile `setup_t` → generate `setup.sh` → generate config headers + Xcode project) with a single CMakeLists.txt.

1. Create a top-level `CMakeLists.txt` that compiles all sources in `src/` directly, with `cfg/` and `src/` as include paths — mirroring what the Xcode project already does at `minivmac.xcodeproj/project.pbxproj`.
2. Hand-write the 6 config headers currently in `cfg/` as static files (they're already checked in and only ~240 lines total for `CNFUDPIC.h`, the largest). Use CMake `option()` / `set()` for the key knobs (`CurEmMd`, `Use68020`, `EmFPU`, screen size, etc.) and `configure_file()` to template them.
3. Add presets for the 3 primary platforms (macOS/Cocoa, Linux/SDL, Windows/SDL) — replacing `build_macos.sh`, `build_linux.sh`, `build_windows.sh`.
4. Keep the old build scripts and `setup/` intact for now (no deletion). Verify the CMake build produces a working binary by booting a System 7 disk image.

**Result:** `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build` produces a working emulator. The `setup/` directory becomes optional legacy.

---

## Phase 2 — Type System & Macro Cleanup

Replace the custom type system and visibility macros with standard C++.

1. Replace all custom types in `src/OSGLUAAA.h` and throughout `src/`: `ui3b` → `uint8_t`, `ui4b` → `uint16_t`, `ui5b` → `uint32_t`, `si3b` → `int8_t`, etc. Replace `blnr`/`trueblnr`/`falseblnr` → `bool`/`true`/`false`. Replace `nullpr` → `nullptr`. Replace `CPTR` → `uint32_t`. Replace `anyp` → `void*`. Use `<cstdint>` and `<cstdbool>`.
2. Replace the 15+ visibility macros: `LOCALVAR` → `static`, `GLOBALVAR` → (nothing, or `extern` in headers), `EXPORTVAR` → `extern`, `LOCALFUNC`/`LOCALPROC` → `static`, `GLOBALFUNC`/`GLOBALPROC` → (nothing), `EXPORTFUNC`/`EXPORTPROC` → (declared in headers), `IMPORTFUNC`/`IMPORTPROC` → `extern`.
3. These are mechanical find-and-replace operations. Do them file-by-file, compiling after each file to stay green.

**Result:** Code reads like standard C/C++. New contributors don't need a glossary.

---

## Phase 3 — File Rename & Directory Structure

Make the source tree navigable.

1. Rename source files to human-readable names and organize into subdirectories:
   - `src/cpu/` — `m68k.cpp` (from `MINEM68K.c`), `m68k_tables.cpp` (from `M68KITAB.c`), `m68k.h`, `disasm.cpp`
   - `src/devices/` — `via.cpp`, `via2.cpp`, `iwm.cpp`, `scc.cpp`, `scsi.cpp`, `rtc.cpp`, `rom.cpp`, `video.cpp`, `screen.cpp`, `sound.cpp`, `asc.cpp`, `sony.cpp`, `keyboard.cpp`, `adb.cpp`, `pmu.cpp`, `mouse.cpp`
   - `src/core/` — `machine.cpp` (from `GLOBGLUE.c`), `main.cpp` (from `PROGMAIN.c`), `machine.h`, `types.h`
   - `src/platform/` — `cocoa.mm`, `sdl.cpp`, `platform.h` (from `OSGLUAAA.h`)
   - `src/platform/common/` — shared platform code (from `COMOSGLU.h`, `CONTROLM.h`, `OSGCOMUI.h`, `OSGCOMUD.h`)
   - `cfg/` → `src/config/`
2. Update `CMakeLists.txt` to reflect new paths. Keep `.c` → `.cpp` renames as part of this phase (files already compile as C++, since the codebase avoids C++ keywords).
3. Move the code-as-headers pattern (`COMOSGLU.h`, `CONTROLM.h`, `SCRNMAPR.h`, `SCRNTRNS.h`, `FPCPEMDV.h`, `ADBSHARE.h`) into proper `.cpp` files that are linked, not `#include`'d.

**Result:** A navigable, standard-looking source tree.

---

## Phase 4 — Device Interface & Machine Object ✅

The architectural pivot that enables everything downstream. **Completed.**

1. Defined a `Device` abstract interface with `access()`, `reset()`, `zap()`, and `name()` methods.
2. Wrapped all 16 emulated devices (VIA1, VIA2, SCC, SCSI, IWM, ASC, ADB, Video, Screen, Sony, RTC, Keyboard, Mouse, PMU, Sound, ROM) in Device subclasses.
3. Created a `Machine` class owning:
   - `MachineConfig` struct (model, CPU type, RAM sizes, screen dimensions, device enables)
   - All device instances (via `std::unique_ptr<Device>`, created in `Machine::init()`)
   - `WireBus` for runtime inter-device signal routing
   - `ICTScheduler` for cycle-based task scheduling
4. Reworked ATT to store `Device*` pointers — MMDV enum switch eliminated.
5. Extracted CPU into a `CPU` class (from `regstruct` global).
6. Converted all `CurEmMd` compile-time checks to runtime `MachineConfig::model` checks.
7. Converted most `EmXxx` device-enable guards to runtime config checks.

**What remains compile-time** (deferred to Phase 5): CPU variant flags (`Use68020`, `EmFPU`, `EmMMU`), device VIA cross-dependencies (`EmClassicKbrd`, `EmPMU`, `EmClassicSnd`), Wire topology enum, memory size constants.

**Result:** A `Machine` object encapsulates all emulator state. Mac II boots System 7 identically to pre-Phase-4. See `docs/PLAN-4.md` for full details.

---

## Phase 5 — Multi-Model Support & Runtime Configuration

With the `Machine`/`Device`/`MachineConfig` infrastructure from Phase 4, complete the remaining compile-time → runtime conversions and make the emulator a true multi-model binary.

### 5a — Resolve remaining compile-time dependencies
1. **Decouple VIA cross-dependencies**: `keyboard.cpp`, `sound.cpp`, `pmu.cpp` directly reference VIA symbols (`VIA1_ShiftInData`, etc.) which are only available when `EmClassicKbrd`/`EmClassicSnd`/`EmPMU` are 1. Abstract these through WireBus or device method calls so all three files always compile.
2. **Per-model Wire topology**: Replace the single Wire enum in `CNFUDPIC.h` with a model-specific wire set created by `MachineConfigForModel()`. Mac Plus needs different wiring (no VIA2, no ADB, different VIA1 port assignments).
3. **CPU instruction set selection**: Make `Use68020`/`EmFPU`/`EmMMU` runtime booleans. The 55 `#if Use68020` blocks in m68k.cpp become `if (config.use68020)` — branch predictor handles this at zero cost.
4. **Dynamic memory sizes**: `kRAMa_Size`, `kRAMb_Size`, `kVidMemRAM_Size` → `MachineConfig` fields, allocated dynamically.
5. **Remove remaining `#define` guards** from `CNFUDPIC.h`. The file should become nearly empty or be folded into `machine_config.h`.

### 5b — Multi-model validation
6. Validate Mac Plus (68000, classic keyboard, no VIA2, classic sound, 24-bit, 512×342 1-bit screen) alongside Mac II.
7. Remove global device pointers (`g_via1`, `g_iwm`, etc.) and backward-compatible free-function API — all access goes through `Machine`.

### 5c — Runtime configuration interface
8. Load `MachineConfig` from TOML/JSON file or command-line args (model, CPU, RAM, screen, ROM path, disk images, speed, sound).
9. Replace compile-time screen constants (`vMacScreenWidth`, `vMacScreenHeight`, `vMacScreenDepth`) with `MachineConfig` fields. Allocate screen buffer dynamically.
10. Remove the need for multiple binaries: a single binary can emulate any supported Mac model.

**Result:** `./minivmac --model=MacII --ram=8M --screen=800x600x8 --rom=MacII.ROM disk1.img`

---

## Phase 6 — Platform Consolidation

Reduce 9 backends to 2.

1. Keep `src/OSGLUCCO.m` (Cocoa, macOS native) and `src/OSGLUSDL.c` (SDL, cross-platform). Drop Carbon (`OSGLUOSX.c`), X11 (`OSGLUXWN.c`), GTK (`OSGLUGTK.c`), Win32 (`OSGLUWIN.c`), DOS (`OSGLUDOS.c`), NDS (`OSGLUNDS.c`), Classic Mac (`OSGLUMAC.c`).
2. Split each remaining backend into subsystems implementing focused interfaces:
   - `platform::Window` — windowing, resize, fullscreen
   - `platform::Audio` — sound output
   - `platform::Input` — keyboard/mouse events
   - `platform::FileIO` — disk image access, ROM loading
   - `platform::Clipboard` — host clipboard exchange
3. Extract the shared code currently in `COMOSGLU.h` and `CONTROLM.h` (the in-emulator control mode UI) into a platform-independent module.

**Result:** Clean separation of concerns. Adding a new backend (e.g., Wayland-native) means implementing 5 small interfaces, not one 5,000-line file.

---

## Phase 7 — Testing Infrastructure

1. Add Google Test or Catch2 via CMake `FetchContent`.
2. **CPU tests:** Instantiate a `Machine` with minimal config, load test programs into RAM, run N cycles, assert register/memory state. Start with a known-good test suite like [m68k-test](https://github.com/TomHarte/ProcessorTests) JSON vectors for 68000.
3. **Device tests:** Instantiate individual device objects, call `write()`/`read()` sequences, verify state transitions. VIA timer tests, RTC PRAM read/write, SCSI bus arbitration.
4. **Integration tests:** Boot a minimal System disk image headlessly (no video output), verify it reaches a known state (e.g., the ROM checksum is accepted, system heap is initialized).
5. Add CI (GitHub Actions) running the test suite on macOS, Linux, Windows.

**Result:** Regressions are caught automatically. Contributors can verify changes without manual boot-testing.

---

## Phase 8 — Debug & Introspection Layer

1. Add a `Debugger` class that attaches to `Machine`:
   - Breakpoints (address, memory watchpoints, device access)
   - Single-step, run-to-next-tick
   - Register dump, memory dump, disassembly (the existing `DISAM68K.c` disassembler)
   - Device state inspection (VIA registers, RTC PRAM, interrupt state, Wires array)
2. Expose the debugger via a simple command interface (stdin/stdout or socket) so external tools can connect.
3. Add optional structured logging (replacing the ad-hoc `dbglog_*` system) with categories per device.

**Result:** `./minivmac --debug` drops into an interactive debugger. External tools can attach.

---

## Phase 9 — External Control API (MCP Server Foundation)

1. Add an IPC layer (Unix domain socket / TCP / named pipe) exposing the emulator's state and controls as a structured API (JSON-RPC or similar):
   - Query/set CPU registers
   - Read/write emulated memory
   - Insert/eject disk images
   - Send keyboard/mouse events
   - Query screen framebuffer
   - Pause/resume/step/reset
   - Query device state
2. Build this on top of the `Machine` + `Debugger` interfaces from Phases 4 and 8.
3. This becomes the foundation for an MCP server — the MCP tool definitions map 1:1 to the IPC API methods.

**Result:** An LLM (or any external program) can control the emulated Mac programmatically. Screenshot → OCR → keyboard input loops become possible.

---

## Verification

- After each phase: `cmake --build build && ./minivmac MacII.ROM` boots System 7 successfully
- Phase 2: Compile with `-Wall -Wextra -Wpedantic` under C++17 — zero warnings
- Phase 4: ✅ `Machine` object owns all state; Mac II boots System 7
- Phase 5: Boot the same ROM+disk with `--model=Plus` and `--model=II` from one binary
- Phase 7: `ctest` passes all CPU instruction tests and device unit tests
- Phase 8: `./minivmac --debug` can set a breakpoint, hit it, and inspect registers
- Phase 9: A Python script can connect to the socket, send a keystroke, and read back the screen buffer

---

## Key Decisions

- **CMake over Meson**: wider IDE support (CLion, VS Code, Xcode generator) and easier FetchContent for test frameworks
- **C++ over modern C**: classes for device encapsulation, `std::unique_ptr` for ownership, namespaces for organization, `constexpr` for compile-time constants — these are the features that matter, not heavy template metaprogramming
- **Keep Cocoa backend alongside SDL**: native macOS integration (menu bar, drag-and-drop, Retina, system audio) is important for the primary development platform and for "better integration with the mac desktop"
- **Runtime `if` over compile-time `#if` for CPU/model**: modern branch predictors make this zero-cost in practice. The simplicity win of one binary is enormous
- **TOML for config files**: human-editable, well-supported in C++ (toml++ is header-only)
