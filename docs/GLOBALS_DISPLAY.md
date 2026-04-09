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
