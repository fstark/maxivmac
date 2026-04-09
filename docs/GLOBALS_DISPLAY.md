# GLOBALS_DISPLAY Plan

Consolidate 14 display-related globals into a **`DisplayState`** struct owned by
`EmulatorShell`. The existing `vMacScreen*` macros shield 200+ consumer sites,
so most code won't change.

**Build:** `cmake --build bld/macos-headless`
**Test:** `cd test && ./verify.sh`

---

## Boundary Analysis

4 of the 14 globals are **written by core** (`machine_obj.cpp` at `Machine::init()`):
`g_screenWidth`, `g_screenHeight`, `g_screenDepth`, `g_colorModeWorks`.
The remaining 10 live entirely within the platform/device layer.

The core writes can be eliminated: `Machine::init()` copies from `MachineConfig`,
which the Shell already has access to. The Shell can initialize the struct fields
from config directly, and `Machine::init()` no longer needs to touch globals.

---

## Phase 1 — Create `DisplayState` struct

Define the struct in a new header. All 14 fields, zero-initialized defaults.

### Steps

1. Create `src/platform/display_state.h`:
   ```cpp
   #pragma once
   #include <cstdint>

   #define CLUT_SIZE 256

   struct DisplayState {
       /* Dimensions (set once from MachineConfig at init) */
       uint16_t screenWidth  = 640;
       uint16_t screenHeight = 480;
       uint8_t  screenDepth  = 3;

       /* Color mode */
       bool useColorMode       = false;
       bool colorModeWorks     = false;
       bool colorMappingChanged = false;
       bool colorTransValid    = false;

       /* CLUT (Color Lookup Table) */
       uint16_t clutReds[CLUT_SIZE]   = {};
       uint16_t clutGreens[CLUT_SIZE] = {};
       uint16_t clutBlues[CLUT_SIZE]  = {};

       /* Screen buffers & dirty tracking */
       uint8_t* screenCompareBuff = nullptr;
       bool     screenChanged     = false;

       /* Screen conversion output */
       uint8_t* scalingBuff = nullptr;
       uint8_t* clutFinal   = nullptr;
   };
   ```
2. Build and test (header-only, no consumers yet — just validates syntax).

---

## Phase 2 — Add `DisplayState` to EmulatorShell

### Steps

1. Add `#include "platform/display_state.h"` to `emulator_shell.h`.
2. Add `DisplayState display_;` as a private member of `EmulatorShell`.
3. Add a public accessor: `DisplayState& display() { return display_; }`.
4. Build and test (no behavioral change yet).

---

## Phase 3 — Initialize `DisplayState` from config

Move the 4 config-sourced assignments out of `Machine::init()` and into
`EmulatorShell::initMachine()`, targeting the new struct fields.

### Steps

1. In `src/platform/emulator_shell.cpp` `initMachine()`, **before** `Machine::init()` or
   immediately after, add:
   ```cpp
   const auto& mc = GetMachineConfig();
   display_.screenWidth  = mc.screenWidth;
   display_.screenHeight = mc.screenHeight;
   display_.screenDepth  = mc.screenDepth;
   display_.colorModeWorks = (mc.screenDepth > 0);
   ```
2. Keep the old globals alive — they still exist and get written by `Machine::init()`.
   This ensures zero behavioral change. The struct simply shadows them for now.
3. Build and test.

---

## Phase 4 — Redirect globals to struct fields

The old globals (`g_screenWidth` etc.) still exist in `osglu_common.cpp` and are
declared `extern` in `platform.h`. Replace their definitions with references to
the Shell's `DisplayState`.

### Steps

1. In `src/platform/common/osglu_common.cpp`, remove the 12 definitions:
   - `g_screenWidth`, `g_screenHeight`, `g_screenDepth`
   - `g_useColorMode`, `g_colorModeWorks`, `g_colorMappingChanged`
   - `g_colorTransValid`
   - `CLUT_reds[]`, `CLUT_greens[]`, `CLUT_blues[]`
   - `g_screenCompareBuff`, `g_screenChanged`

2. In `src/platform/screen_convert.cpp`, remove the 2 definitions:
   - `ScalingBuff`, `CLUT_final`

3. In `src/platform/platform.h`, replace the `extern` declarations and macros
   to access `g_shell->display()`:
   ```cpp
   /* Forward-declared display state accessors */
   #include "platform/display_state.h"
   class EmulatorShell;
   extern EmulatorShell* g_shell;
   DisplayState& GetDisplayState();  /* implemented in emulator_shell.cpp */

   #define g_screenWidth      (GetDisplayState().screenWidth)
   #define g_screenHeight     (GetDisplayState().screenHeight)
   #define g_screenDepth      (GetDisplayState().screenDepth)
   #define g_useColorMode     (GetDisplayState().useColorMode)
   #define g_colorModeWorks   (GetDisplayState().colorModeWorks)
   #define g_colorMappingChanged (GetDisplayState().colorMappingChanged)
   #define g_colorTransValid  (GetDisplayState().colorTransValid)
   #define CLUT_reds          (GetDisplayState().clutReds)
   #define CLUT_greens        (GetDisplayState().clutGreens)
   #define CLUT_blues         (GetDisplayState().clutBlues)
   #define g_screenCompareBuff (GetDisplayState().screenCompareBuff)
   #define g_screenChanged    (GetDisplayState().screenChanged)
   #define ScalingBuff        (GetDisplayState().scalingBuff)
   #define CLUT_final         (GetDisplayState().clutFinal)
   ```
   The `vMacScreenWidth` etc. macros keep working unchanged since they reference
   `g_screenWidth` which is now a macro itself.

4. Implement `GetDisplayState()` in `emulator_shell.cpp`:
   ```cpp
   DisplayState& GetDisplayState() { return g_shell->display(); }
   ```

5. Remove the 4 `extern` + assignment lines from `machine_obj.cpp`
   (`Machine::init()` no longer needs to set display globals).

6. Remove the `extern` from `osglu_common.h` for `g_screenCompareBuff`,
   `g_screenChanged`, `g_colorTransValid`.

7. Remove the `extern` declarations from `screen_convert.h` for `ScalingBuff`
   and `CLUT_final`.

8. Grep for each old variable name to confirm no duplicate definitions remain.
9. Build and test.

---

## Phase 5 — Migrate buffer lifecycle to DisplayState

Move `alloc`/`free` of display buffers into `DisplayState` methods.

### Steps

1. Add methods to `DisplayState`:
   ```cpp
   bool allocBuffers(long screenNumBytes, long clutFinalSize);
   void freeBuffers();
   ```

2. Implement them (simple `AllocBlock` / `free` wrappers).

3. In `EmulatorShell::allocMyMemory()`, replace:
   ```cpp
   AllocBlock(&g_screenCompareBuff, ...)
   AllocBlock(&CLUT_final, ...)
   ```
   with `display_.allocBuffers(...)`.

4. In `EmulatorShell::unallocMyMemory()`, replace:
   ```cpp
   free(g_screenCompareBuff); ...
   free(CLUT_final); ...
   ```
   with `display_.freeBuffers()`.

5. Build and test.

---

## Phase 6 — Remove legacy macro shims

Once all consumers go through `GetDisplayState()`, the macro shims in platform.h
can be cleaned up. Convert direct struct field access in platform code, keeping
only the `vMacScreen*` derived-quantity macros.

### Steps

1. In each platform .cpp file, replace macro-mediated access with direct
   `g_shell->display().fieldName` where readability improves.
2. Keep `vMacScreenWidth` / `vMacScreenHeight` / `vMacScreenDepth` macros
   (these are used 200+ times in device code and provide a clean read-only API).
3. Remove the `#define g_screenWidth ...` shims from platform.h once all direct
   `g_screenWidth` references in .cpp files are eliminated.
4. Grep for each old name to ensure zero residual references.
5. Build and test.

---

## Summary

| Phase | Description | Complexity | Files |
|-------|-------------|-----------|-------|
| 1 | Create `DisplayState` struct header | Trivial | 1 new |
| 2 | Add to EmulatorShell | Trivial | 1 |
| 3 | Shadow-init from config | Simple | 1 |
| 4 | Redirect globals → struct via macros | Moderate | ~6 |
| 5 | Migrate buffer lifecycle | Simple | 2 |
| 6 | Remove legacy macro shims | Moderate | ~8 |

Phases 1–4 are the core refactoring. Phases 5–6 are polish.
Each phase can be committed independently. Phase 4 is the big one
where the actual global elimination happens, but the macro-redirect
approach means zero changes to the 200+ consumer sites in device code.
