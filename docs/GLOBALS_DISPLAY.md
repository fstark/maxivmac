# GLOBALS_DISPLAY Plan

Consolidate 14 display-related globals into a **`DisplayState`** struct owned by
`EmulatorShell`. The existing `vMacScreen*` macros shield 200+ consumer sites,
so most code won't change.

**Build:** `cmake --build bld/macos-headless`
**Test:** `cd test && ./verify.sh`

---

## Phases 1-4 — DONE (commit 55014c2)

Created `DisplayState` struct in `display_state.h`, added as
`EmulatorShell::display_` member. Replaced 14 global definitions with
`#define` macro shims that call `GetDisplayState()`. An `s_earlyDisplay`
static handles the case where `Machine::init()` runs before `g_shell` is
set (via `ProgramEarlyInit`), with the early state copied into `display_`
in `initPlatform()`.

Removed globals: `g_screenWidth`, `g_screenHeight`, `g_screenDepth`,
`g_useColorMode`, `g_colorModeWorks`, `g_colorMappingChanged`,
`g_colorTransValid`, `CLUT_reds[]`, `CLUT_greens[]`, `CLUT_blues[]`,
`g_screenCompareBuff`, `g_screenChanged`, `ScalingBuff`, `CLUT_final`.

All 6 models pass non-regression tests.

---

## Phase 5 — DONE (commit 9aa44ec)

Added `allocBuffers()`/`freeBuffers()` methods to `DisplayState`.
`EmulatorShell::allocMyMemory()` and `unallocMyMemory()` now call these
instead of raw `AllocBlock`/`free` on macro-redirected globals.

---

## Phase 6 — DONE (commit 9aa44ec)

Converted `emulator_shell.cpp` from macro-mediated access (`g_screenChanged`,
`g_useColorMode`, `ScalingBuff`) to direct `display_` member access. The
`#define` shims in `platform.h` are retained for device code (`video.cpp`)
and other cross-layer consumers that include `platform.h` transitively.

---

## Summary

| Phase | Description | Status |
|-------|-------------|--------|
| 1-4 | Create struct, add to Shell, redirect globals via macros | DONE (55014c2) |
| 5 | Migrate buffer lifecycle to `DisplayState` methods | DONE (9aa44ec) |
| 6 | Direct `display_` access in `emulator_shell.cpp` | DONE (9aa44ec) |

All phases complete. The `#define` shims in `platform.h` remain as the
stable API for device code and cross-layer consumers.
