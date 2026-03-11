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

## Phase 4 — Device Interface & Machine Object

This is the architectural pivot that enables everything downstream.

1. Define a C++ `Device` interface:
   ```cpp
   class Device {
       virtual void reset() = 0;
       virtual uint8_t read(uint32_t addr) = 0;
       virtual void write(uint32_t addr, uint8_t data) = 0;
       virtual void tick() = 0;  // called per emulated tick if needed
   };
   ```
2. Wrap each emulated device (VIA, SCC, SCSI, IWM, RTC, ASC, ADB, Video, etc.) in a class implementing `Device`. Move their static/global state into member variables. The current per-device pattern (`XXX_Access(Data, WriteMem, addr)`) maps cleanly to `read()`/`write()`.
3. Create a `Machine` class that owns:
   - A `MachineConfig` struct (model, CPU type, RAM sizes, screen dimensions, device enables, wire topology) — replacing the compile-time `#define`s in `cfg/CNFUDPIC.h`
   - All device instances (via `std::unique_ptr<Device>`)
   - RAM/ROM/VRAM buffers
   - The Address Translation Table (currently the `ATTer` linked list in `src/GLOBGLUE.c`)
   - The Wires array (currently `ui3b Wires[kNumWires]` — global in GLOBGLUE.c)
   - The ICT task scheduler
   - The CPU instance
4. Rework the ATT and `MMDV_Access()` dispatch (`GLOBGLUE.c` lines 1304+) to use the device registry on `Machine` instead of a `switch` statement. Each ATT entry stores a `Device*` pointer instead of a `kMMDV_xxx` enum index.
5. Convert the CPU from static globals (`regstruct` at `MINEM68K.c` line 199) to a `CPU` class. The `Use68020`/`EmFPU`/`EmMMU` flags become runtime booleans on the CPU, gating instruction handlers via `if` rather than `#if`. The ~50 `#if Use68020` blocks in MINEM68K.c become `if (config.use68020)` — the compiler will optimize these since `config` is const for the lifetime of a session.
6. Convert `CurEmMd` comparisons (currently `#if CurEmMd <= kEmMd_Plus` etc., used in ~30 places across GLOBGLUE.c, IWMEMDEV.c, ROMEMDEV.c, SCRNHACK.h) to runtime checks on `MachineConfig::model`.

**Result:** A `Machine` object encapsulates all emulator state. You can instantiate multiple machines, configure them differently, and pass them to tests.

---

## Phase 5 — Runtime Configuration

With `Machine` owning config, make key parameters runtime-settable.

1. Create a `MachineConfig` that can be loaded from a TOML/JSON file or command-line args:
   - Mac model (Plus, SE, II, IIx, etc.)
   - CPU type (68000 / 68020+FPU)
   - RAM size
   - Screen resolution and color depth
   - ROM file path
   - Disk image paths
   - Speed multiplier
   - Sound enable, magnification, full-screen
2. Replace the compile-time screen constants — `vMacScreenWidth` (640), `vMacScreenHeight` (480), `vMacScreenDepth` (3) from `cfg/CNFUDALL.h` — with `MachineConfig` fields. Allocate the screen buffer dynamically based on config. Update the platform layer to query `Machine` for screen dimensions.
3. Wire topology (the signal routing in `cfg/CNFUDPIC.h`) becomes a function of the model: `MachineConfig::model` → a factory that creates the correct device set and wiring. This replaces the ~30 `kWire_xxx` `#define`s with a model-specific initialization function.
4. Remove the need for multiple binaries: a single binary can emulate any supported Mac model.

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
