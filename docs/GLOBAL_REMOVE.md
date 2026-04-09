# GLOBAL_REMOVE Plan

Remove the ~10 globals tagged **REMOVE** in `docs/GLOBALS.md`.
Each phase is independent and produces one commit.

**Build:** `cmake --build bld/macos-headless`
**Test:** `cd test && ./verify.sh`

---

## Phase 1 — Add `static` to `IWM` struct

**Variable:** `IWM` (`IWM_Ty`) in `src/devices/iwm.cpp`
**Problem:** File-scope struct missing `static` keyword. Never referenced externally (23 uses, all in iwm.cpp). No extern declaration exists.
**Fix:** Add `static` to the declaration at line ~68.

### Steps
1. In `src/devices/iwm.cpp`, change `IWM_Ty IWM;` → `static IWM_Ty IWM;`
2. Grep to confirm no other file references `IWM` as a variable name.
3. Build and test.

---

## Phase 2 — Remove redundant `g_mousePosCurH` / `g_mousePosCurV`

**Variables:** `g_mousePosCurH`, `g_mousePosCurV` (both `uint16_t`) in `osglu_common.cpp`
**Problem:** Cached copies of `g_curMouseH` / `g_curMouseV`, used only in `MyMousePositionSet()` in osglu_common.cpp.
**Fix:** Replace reads with `g_curMouseH` / `g_curMouseV`; delete declarations and definitions.

### Steps
1. In `src/platform/common/osglu_common.cpp` `MyMousePositionSet()` (~L626):
   - Replace `g_mousePosCurH` → `g_curMouseH` and `g_mousePosCurV` → `g_curMouseV` in the comparison.
   - Remove the two assignment lines (`g_mousePosCurH = h;` / `g_mousePosCurV = v;`).
2. Remove the definition lines (~L621-622): `uint16_t g_mousePosCurV = 0;` and `uint16_t g_mousePosCurH = 0;`.
3. Remove the extern declarations from `src/platform/common/osglu_common.h` (~L52-53).
4. Grep for `g_mousePosCur` across entire codebase — expect zero hits.
5. Build and test.

---

## Phase 3 — Remove `g_romLoaded` global

**Variable:** `g_romLoaded` (`bool`) in `osglu_common.cpp`
**Problem:** Init flag set in `keyboard_map.cpp`, read in `emulator_shell.cpp`. Should be a Shell member.
**Fix:** Add `bool romLoaded_` member to `EmulatorShell`, route through that.

### Steps
1. Add `bool romLoaded_ = false;` to `EmulatorShell` class (private member) in `src/platform/emulator_shell.h`.
2. Add a public setter: `void setRomLoaded(bool v) { romLoaded_ = v; }` and getter `bool isRomLoaded() const { return romLoaded_; }`.
3. In `src/platform/common/keyboard_map.cpp` (~L114): replace `g_romLoaded = true;` → `g_shell->setRomLoaded(true);`.
4. In `src/platform/emulator_shell.cpp` (~L70): replace `if (! g_romLoaded)` → `if (! romLoaded_)` (or `isRomLoaded()` if outside class).
5. Remove definition from `src/platform/common/osglu_common.cpp` (~L20).
6. Remove extern declaration from `src/platform/common/osglu_common.h` (~L29).
7. Grep for `g_romLoaded` — expect zero hits.
8. Build and test.

---

## Phase 4 — Remove `rom_path` global

**Variable:** `rom_path` (`char*`) in `rom_loader.cpp`
**Problem:** Set in Shell (CLI parsing + config), only read inside `LoadMacRom()` which already receives other params. Should be a parameter.
**Fix:** Add `rom_path` as parameter to `LoadMacRom()`.

### Steps
1. In `src/platform/common/rom_loader.h`:
   - Change signature: `bool LoadMacRom(char *d_arg, char *app_parent, char *pref_dir);`
     → `bool LoadMacRom(char *rom_path, char *d_arg, char *app_parent, char *pref_dir);`
   - Remove `extern char *rom_path;`
2. In `src/platform/common/rom_loader.cpp`:
   - Remove `char *rom_path = nullptr;` definition (~L6).
   - Update `LoadMacRom` implementation to use the new parameter instead of the global.
3. In `src/platform/emulator_shell.cpp`:
   - At the call site to `LoadMacRom()` (~L174), pass the rom path as first argument.
   - Remove the lines that assign to `rom_path` (~L170, L782, L785). Store the value in a local or Shell member instead, and pass it to `LoadMacRom()`.
4. Grep for `rom_path` as a global — should only appear as the function parameter name.
5. Build and test.

---

## Phase 5 — Remove `app_parent` global

**Variable:** `app_parent` (`char*`) defined in `emulator_shell.cpp`, externed in `dbglog_platform.cpp`
**Problem:** Global path string. Already passed as function parameter to `LoadMacRom()`; only remaining consumer is `dbglog_platform.cpp`.
**Fix:** Pass as parameter to debug log init, store locally in Shell.

### Steps
1. In `src/platform/emulator_shell.cpp`:
   - Change `char *app_parent = nullptr;` from file-scope global to a member of `EmulatorShell` (e.g. `char* appParent_ = nullptr;`) or a `static` local.
2. In `src/platform/common/dbglog_platform.cpp` (~L14):
   - Remove `extern char *app_parent;`.
   - Change `dbglog_open()` to accept a `const char* appParent` parameter.
   - Update body to use the parameter instead of the global.
3. In `src/platform/common/dbglog_platform.h` (or wherever `dbglog_open` is declared):
   - Update the declaration to match the new signature.
4. In `src/platform/emulator_shell.cpp`:
   - At the call site to `dbglog_open()`, pass the app parent path.
   - Update all other internal uses of `app_parent` to use the member/local.
5. Grep for `app_parent` as a global — should only appear as function parameters or local variables.
6. Build and test.

---

## Phase 6 — Move `SavedBriefMsg` / `SavedLongMsg` / `g_savedFatalMsg` to Shell

**Variables:** `SavedBriefMsg` (`const char*`), `SavedLongMsg` (`const char*`), `g_savedFatalMsg` (`bool`)
**Problem:** Act as a single-slot message queue between MacMsg() error handler and Shell's event loop. Works, but uses three bare globals with raw pointer strings.
**Fix:** Move to Shell members. MacMsg() routes through `g_shell->`.

### Steps
1. Add to `EmulatorShell` class in `src/platform/emulator_shell.h`:
   ```cpp
   private:
       const char* savedBriefMsg_ = nullptr;
       const char* savedLongMsg_ = nullptr;
       bool savedFatalMsg_ = false;
   public:
       void queueMessage(const char* brief, const char* longMsg, bool fatal);
       bool hasQueuedMessage() const { return savedBriefMsg_ != nullptr; }
       void consumeQueuedMessage();
   ```
2. Implement `queueMessage()` and `consumeQueuedMessage()` in `emulator_shell.cpp`.
3. In `src/platform/common/osglu_common.cpp`:
   - `MacMsg()` function (~L700): replace writes to globals → `g_shell->queueMessage(briefMsg, longMsg, fatal);`
   - Remove the definitions (~L692-694).
   - Remove the null-check early return that references `SavedBriefMsg` (~L698).
4. In `src/platform/emulator_shell.cpp` (~L210-212):
   - Replace direct reads of `SavedBriefMsg`/`SavedLongMsg` with member access through the new methods.
5. Remove extern declarations from `src/platform/common/osglu_common.h` (~L65-67).
6. Grep for `SavedBriefMsg`, `SavedLongMsg`, `g_savedFatalMsg` — expect zero hits.
7. Build and test.

---

## Phase 7 — Remove `g_wiresData` shim pointer

**Variable:** `g_wiresData` (`uint8_t*`) in `machine.cpp`
**Problem:** Raw pointer alias to `g_wires.data()`. 49 macros in `wire_macros.h` dereference it. One assignment at init.
**Fix:** Replace `g_wiresData` with `g_wires.data()` in the macro definitions and delete the pointer.

### Steps
1. In `src/core/wire_macros.h` (~L14-65):
   - In every macro that uses `g_wiresData[...]`, replace with `g_wires.data()[...]`.
   - Alternatively, add a `#define WIRE_DATA g_wires.data()` at the top and use that in all macros (less churn).
2. In `src/core/machine.cpp`:
   - Remove definition: `uint8_t* g_wiresData = nullptr;` (~L79).
   - Remove assignment: `g_wiresData = g_wires.data();` (~L1686).
3. Remove extern declaration from `src/core/machine.h` (~L153).
4. Grep for `g_wiresData` across entire codebase — expect zero hits.
5. Build and test.

---

## Summary

| Phase | Variable(s) | Complexity | Files touched |
|-------|-------------|-----------|---------------|
| 1 | `IWM` (add static) | Trivial | 1 |
| 2 | `g_mousePosCurH/V` | Simple | 2 |
| 3 | `g_romLoaded` | Simple | 4 |
| 4 | `rom_path` | Moderate | 3 |
| 5 | `app_parent` | Moderate | 3-4 |
| 6 | `SavedBriefMsg` etc. | Moderate | 3 |
| 7 | `g_wiresData` | Simple (many macros) | 3 |

Phases are ordered easiest-first. Each is independent — any can be skipped without affecting the others.
