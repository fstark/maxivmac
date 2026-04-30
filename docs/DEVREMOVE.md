# Plan: Remove Developer Mode

## Motivation

Developer mode was built as a DockSpace-based UI with floating debug tool
panels (Registers, Disassembly, Memory, VIA, Traps, Scrap, Guest Console,
Low Memory Globals).  In practice the command-line debugger (`--debugger`) is
far more powerful — it supports breakpoints, watchpoints, conditional breaks,
stepping, backtrace, memory search, trap tracing, scripting, and a debug
server — and is what gets used day to day.  Developer mode adds significant
code weight for little return.  Removing it simplifies the UI state machine,
shrinks the build, and removes a maintenance burden.

We can always bring back a visual tool UI later if the debugger hits its
limits.

---

## What Goes Away

### 1. UIState enum value: `Developer`

**File:** `src/platform/imgui_backend.h`

The `UIState` enum drops from 4 values to 3:

```
ModelSelector, Windowed, Fullscreen     (was: + Developer)
```

Every `switch` on `UIState` and every `== UIState::Developer` test must be
updated or removed.

### 2. State transition: `enterDeveloper()`

**File:** `src/platform/imgui_backend.h`, `src/platform/imgui_backend.cpp`

Remove the method declaration and its implementation (saves/restores window
geometry, resizes to 80% of display).

### 3. Developer draw paths

**File:** `src/platform/imgui_backend.cpp`

| Code                            | What it does                                  |
|---------------------------------|-----------------------------------------------|
| `drawDeveloperState()`          | Stub that just calls `drawWindowedState()`    |
| `drawViewportDeveloper()`       | Renders emu viewport as a resizable ImGui win |
| Menu bar draw (`drawMenuBar()`) | Only shown in Developer state                 |
| `toolRegistry_.drawAllVisible()`| Only called in Developer state                |

After removal, `drawWindowedState()` no longer needs Developer branches.
`drawMenuBar()` itself becomes dead code — delete it entirely (there is no
menu bar in Windowed/Fullscreen).

### 4. Tool framework (entire subsystem)

| File                                  | Contents                                |
|---------------------------------------|-----------------------------------------|
| `src/platform/imgui_tool.h`          | `ToolPanel` base class                   |
| `src/platform/imgui_tool_registry.h` | `ToolRegistry` class declaration         |
| `src/platform/imgui_tool_registry.cpp`| `registerTool`, `drawAllVisible`, menu   |
| `src/platform/imgui_debug_windows.h` | 7 tool classes + `RegisterDebugTools()`  |
| `src/platform/imgui_debug_windows.cpp`| ~650 lines: Registers, Disassembly, Memory, VIA, Traps, Scrap, Console |
| `src/platform/imgui_lomem_tool.h`    | `LowMemTool` class                      |
| `src/platform/imgui_lomem_tool.cpp`  | Low Memory Globals viewer                |

All 7 files are deleted.

### 5. `ToolRegistry` member in `ImGuiBackend`

**File:** `src/platform/imgui_backend.h`

Remove:
- `#include "platform/imgui_tool_registry.h"`
- `ToolRegistry toolRegistry_;`
- `ToolRegistry &getToolRegistry()`
- `void drawDeveloperState();`
- `void drawViewportDeveloper();`

### 6. `RegisterDebugTools()` call sites

| File                           | Line                                      |
|--------------------------------|-------------------------------------------|
| `src/platform/app_main.cpp`   | `RegisterDebugTools(imguiBackend.getToolRegistry());` |
| `src/platform/imgui_main.cpp` | `RegisterDebugTools(backend.getToolRegistry());`      |
| `src/platform/imgui_backend.cpp` | in `bootFromSelector()`                |

Remove the calls and the `#include "platform/imgui_debug_windows.h"`.

### 7. Overlay "Developer Mode" button

**File:** `src/platform/imgui_overlay.cpp` (`drawAdvancedTab`)

Remove the entire Developer Mode toggle button and its surrounding logic
(the `isDeveloper` check, the button, the state request).  The Advanced tab
stays — it still has the About text and other items.

Also remove the `UIState::Developer` case from the state-change switch in
`drawWindowedState()`.

### 8. Overlay header

**File:** `src/platform/imgui_overlay.h`

`drawAdvancedTab` signature currently takes `UIState currentState` and
`requestedState` — after removing the developer button, it may no longer
need these parameters.  Simplify.

### 9. `createWindow()` developer sizing

**File:** `src/platform/imgui_backend.cpp`

In `createWindow()`, the `if (uiState_ == UIState::Developer)` branch adds
200px to width and height.  Remove this branch — the window is always
exactly the emulator size.

### 10. `onResolutionChanged()` developer branch

**File:** `src/platform/imgui_backend.cpp`

The comment says "Developer modes handle the new resolution automatically" —
update comment and remove the Developer consideration.

### 11. `imGuiConsumedEvent()` developer comment

**File:** `src/platform/imgui_backend.cpp`

The comment mentions "in Developer mode, debug tool windows on top correctly
block the hover."  After removal the viewport always fills the window, so
`emuViewportHovered_` is always true.  Simplify the logic and update the
comment.

### 12. Saved window geometry for developer

**File:** `src/platform/imgui_backend.h`

The comment on `savedWinX_/Y_/W_/H_` says "for returning from
fullscreen/developer".  Update comment to "for returning from fullscreen".
The members stay — they're used by fullscreen transitions.

### 13. CMakeLists.txt

**File:** `CMakeLists.txt`

Remove from `IMGUI_SOURCES`:
```
src/platform/imgui_debug_windows.cpp
src/platform/imgui_tool_registry.cpp
src/platform/imgui_lomem_tool.cpp
```

### 14. Command-line args

No developer-mode-specific CLI args exist (no `--developer` flag).  Nothing
to change in `ParseCommandLine()` or `PrintUsage()`.

### 15. Documentation updates

| File                          | Action                                    |
|-------------------------------|-------------------------------------------|
| `docs/TODO.md`                | Remove "Remove Developer Mode UI" line    |
| `docs/BUGS.md`               | Remove two `[DONE] in developer mode:` entries |
| `docs/done/UI.md`            | Add note that Developer mode was removed; leave as historical reference |
| `docs/done/UI_PLAN.md`       | Same — mark Phase 5 (Developer Mode + DockSpace) as removed |
| `docs/BUILDING.md`           | Update "debug tools" mention              |
| `docs/PLATFORM_ARCH.md`      | Remove debug-windows references from the frame loop diagram |
| `docs/specs/MOUSE.md`        | Remove the "Developer" display mode section entirely |
| `docs/UI_OVERLAY_PLAN.md`    | Remove "developer-only" references        |
| `docs/CODE_QUALITY.md`       | References to `imgui_debug_windows.cpp` nesting will become stale — OK, leave as historical |
| `docs/MACROMAN_UNIFY_DESIGN.md` | References to `imgui_debug_windows.cpp` and `imgui_lomem_tool.cpp` become stale — add "[removed]" note |
| `docs/features/LOW_MEMORY_DESIGN.md` | Mark as historical (implementation removed) |
| `docs/features/LOW_MEMORY_PLAN.md`  | Mark as historical                   |

---

## What Stays

- **ImGui library** — still used for the control overlay and model selector.
- **Control overlay** (`imgui_overlay.h/.cpp`) — still works in
  Windowed/Fullscreen; just loses the Developer Mode button.
- **Model selector** (`imgui_model_selector.h/.cpp`) — unchanged.
- **`UIState::Windowed` and `UIState::Fullscreen`** — remain as-is.
- **`UIState::ModelSelector`** — remains.
- **SDL window** — sized to emulator output in Windowed; fullscreen in
  Fullscreen.  No extra padding for tool panels.
- **The debugger** (`src/debugger/`) — untouched, this is the primary debug
  interface going forward.
- **Trap counter** (`src/cpu/trap_counter.h/.cpp`) — used by the debugger's
  `trace traps` command, not only by the Traps tool panel.

---

## Debugger Gap Analysis

The developer tools provided visual views that the text debugger doesn't
replicate 1:1.  Here's what each tool offered and how the debugger covers
(or doesn't cover) it, for future enhancement planning:

| Dev Tool             | Debugger equivalent         | Gap                                  |
|----------------------|-----------------------------|--------------------------------------|
| **Registers**        | `info reg`                  | Full coverage. No gap.               |
| **Disassembly**      | `x/i <addr>`, follow PC     | Full coverage (disassembles any range). No gap. |
| **Memory**           | `x/<fmt> <addr>`            | Full coverage (hex, ASCII, word sizes). No gap. |
| **VIA State**        | None                        | **Gap.** No `info via` command. Could add `info via` to print ORA/ORB/DDR/T1/T2/SR/ACR/PCR/IFR/IER. |
| **Traps**            | `info traps`, `trace traps` | Mostly covered. The visual tool had a live counter table with sorting; the debugger prints a flat list. Could add `info traps --top` for a sorted view. |
| **Scrap**            | None                        | **Gap.** No command to inspect the guest clipboard. Could add `info scrap`. |
| **Guest Console**    | `diag guest`                | Partial. The diag subsystem prints to stderr. The tool showed a scrollable log with clear button. A `info console` that shows buffered lines would be equivalent. |
| **Low Memory Globals** | `info globals`            | Mostly covered. The visual tool had category filtering and live snapshot diffing. Could add `info globals --section <name>` and `info globals --changed`. |

All four gaps are addressed in Phases 7–8 below so the removal is
feature-complete in a single sweep.

---

## Implementation Phases

### Phase 1: Delete tool framework files

1. Delete `src/platform/imgui_tool.h`
2. Delete `src/platform/imgui_tool_registry.h`
3. Delete `src/platform/imgui_tool_registry.cpp`
4. Delete `src/platform/imgui_debug_windows.h`
5. Delete `src/platform/imgui_debug_windows.cpp`
6. Delete `src/platform/imgui_lomem_tool.h`
7. Delete `src/platform/imgui_lomem_tool.cpp`
8. Remove the three entries from `CMakeLists.txt`
9. Build — expect compile errors from the includes and calls (fixed in Phase 2)

### Phase 2: Strip Developer from ImGuiBackend

1. Remove `Developer` from `UIState` enum
2. Remove `#include "imgui_tool_registry.h"` from `imgui_backend.h`
3. Remove `ToolRegistry toolRegistry_` member and `getToolRegistry()` accessor
4. Remove `enterDeveloper()` declaration and implementation
5. Remove `drawDeveloperState()` and `drawViewportDeveloper()`
6. Remove `drawMenuBar()` entirely (only used in Developer)
7. In `drawWindowedState()`: remove the Developer menu-bar guard, the
   Developer case in the state-change switch, and the `toolRegistry_` call
8. In `drawEmulatorViewport()`: remove the `Developer` switch case (default
   path handles it as Windowed)
9. In `createWindow()`: remove the Developer sizing branch
10. In `onResolutionChanged()`: update comment, remove Developer mention
11. In `imGuiConsumedEvent()`: simplify comment
12. Update `savedWin*` comment
13. Build — should compile cleanly

### Phase 3: Strip Developer from entry points

1. `app_main.cpp`: remove `#include "imgui_debug_windows.h"` and
   `RegisterDebugTools()` call
2. `imgui_main.cpp`: same
3. `imgui_backend.cpp` `bootFromSelector()`: remove `RegisterDebugTools()` call
4. Build and verify

### Phase 4: Clean overlay

1. `imgui_overlay.cpp` `drawAdvancedTab()`: remove Developer Mode button and
   `isDeveloper` logic
2. Simplify `drawAdvancedTab` signature if `UIState`/`requestedState` are no
   longer needed
3. Update `imgui_overlay.h` to match
4. Build and verify

### Phase 5: Documentation sweep

Update all docs listed in §15 above.

### Phase 6: Verify removal

1. `cmake --preset macos && cmake --build bld/macos` — clean build
2. Launch without `--model` → model selector works
3. Launch with `--model MacII disk.hfs` → boots into Windowed
4. Ctrl overlay: no "Developer Mode" button; all other buttons work
5. Fullscreen toggle works
6. `./selftest.sh` passes — no emulation regression
7. `--debugger` still works

### Phase 7: Debugger enhancements — fill the gaps

Add four new `info` sub-commands to `src/debugger/cmd_info.cpp` that
cover the capabilities lost with the developer tool panels.

#### 7.1 `info via`

Dump VIA1 (and VIA2 if present) register state.  Mirrors VIATool.

```
(dbg) info via
VIA1:
  ORA=00  ORB=00  DDRA=00  DDRB=00
  T1C=00000000  T1L=0000  T2C=00000000  T2L=00
  SR=00  ACR=00  PCR=00  IFR=00  IER=00
  T1Active=0  T2Active=0
VIA2:
  (same format)
```

Implementation:
1. `#include "devices/via.h"` and `"devices/via2.h"` (already available
   in debug builds)
2. Add `static void InfoVIA(Debugger &dbg)` that calls
   `g_machine->findDevice<VIA1Device>()` / `VIA2Device` and prints
   the register fields of `d_` (ORA, ORB, DDR_A, DDR_B, T1C_F,
   T1L_H, T1L_L, T2C_F, T2L_L, SR, ACR, PCR, IFR, IER, T1_Active,
   T2_Active).
3. Wire into `CmdInfo` dispatch: `else if (sub == "via") InfoVIA(dbg);`
4. Update the usage string and `cmd_help.cpp` help text.

#### 7.2 `info scrap`

Decode the guest clipboard from low-memory globals and display TEXT
entries.  Mirrors ScrapTool.

```
(dbg) info scrap
ScrapSize=42  ScrapHandle=$00012340  ScrapCount=3  ScrapState=1 (in memory)
Entry 0: 'TEXT' 42 bytes @$000124A0
  Hello, world.
```

Implementation:
1. Read low-memory globals at $0960–$096A (ScrapSize, ScrapHandle,
   ScrapCount, ScrapState).
2. If `ScrapState > 0`, dereference the handle and walk the scrap
   entries (4-byte type + 4-byte length + data, padded to even).
3. For `TEXT` entries: convert MacRoman → UTF-8 (using
   `util/macroman.h`) and print up to 4096 chars.
4. For non-TEXT entries: print a hex dump of the first 128 bytes.
5. Wire into `CmdInfo` dispatch.

#### 7.3 `info globals --section <name>`

Extend the existing `info globals` command with an optional
`--section` filter that restricts output to a named category from
the globals dictionary (e.g. `QuickDraw`, `MemoryMgr`, `FileMgr`).

```
(dbg) info globals --section FileMgr
Name                  Address   Size
FCBSPtr               $034E     4
DefVCBPtr             $0352     4
... (filtered to FileMgr section only)
```

Implementation:
1. The globals dictionary (`assets/globals.def`) already has section
   tags.  `SymbolsSearch` needs an optional section filter parameter.
2. Parse `--section <name>` from the args in `InfoGlobals()`.
3. Pass section to `SymbolsSearch` (or post-filter).

#### 7.4 `info console`

Show the buffered guest console output that was previously shown in
the ConsoleTool.  Optionally clear the buffer.

```
(dbg) info console
[guest console — 3 lines]
Welcome to Macintosh
Finder startup
Done.

(dbg) info console clear
Console cleared.
```

Implementation:
1. `extnDbgConsoleLines()` already returns the buffer
   (`core/extn_clip.h`).
2. Print all lines.  If `args[1] == "clear"`, call
   `ExtnDbgConsoleClear()`.
3. Wire into `CmdInfo` dispatch.

### Phase 8: Debugger tests

Add smoke tests to `test/debugger_smoke.sh` for the new commands,
following the existing `check` pattern.

```bash
# info via
check "info via" "info via\nquit" "VIA1:"

# info scrap (scrap may be uninitialized at boot — just verify
# the command runs without crash)
check "info scrap" "info scrap\nquit" "ScrapState"

# info globals --section (verify filtering works; CurApName is in
# the Process section, should not appear under FileMgr)
check "info globals --section" "info globals --section FileMgr\nquit" "Address"

# info console (buffer may be empty at boot)
check "info console" "info console\nquit" "console"
```

Also add a negative test confirming that `info via` / `info scrap`
appear in the `help` output and the `info` usage string.

### Phase 9: Final verification

1. Full clean build
2. `test/debugger_smoke.sh` — all existing + new tests pass
3. `./selftest.sh` — no emulation regression
4. Manual spot-check of each new `info` sub-command with a booted
   guest

---

## Files Modified (summary)

| Action | File |
|--------|------|
| DELETE | `src/platform/imgui_tool.h` |
| DELETE | `src/platform/imgui_tool_registry.h` |
| DELETE | `src/platform/imgui_tool_registry.cpp` |
| DELETE | `src/platform/imgui_debug_windows.h` |
| DELETE | `src/platform/imgui_debug_windows.cpp` |
| DELETE | `src/platform/imgui_lomem_tool.h` |
| DELETE | `src/platform/imgui_lomem_tool.cpp` |
| EDIT   | `CMakeLists.txt` |
| EDIT   | `src/platform/imgui_backend.h` |
| EDIT   | `src/platform/imgui_backend.cpp` |
| EDIT   | `src/platform/imgui_overlay.h` |
| EDIT   | `src/platform/imgui_overlay.cpp` |
| EDIT   | `src/platform/app_main.cpp` |
| EDIT   | `src/platform/imgui_main.cpp` |
| EDIT   | `src/debugger/cmd_info.cpp` |
| EDIT   | `src/debugger/cmd_help.cpp` |
| EDIT   | `src/debugger/debugger.cpp` |
| EDIT   | `test/debugger_smoke.sh` |
| EDIT   | `docs/TODO.md` |
| EDIT   | `docs/BUGS.md` |
| EDIT   | `docs/done/UI.md` |
| EDIT   | `docs/done/UI_PLAN.md` |
| EDIT   | `docs/BUILDING.md` |
| EDIT   | `docs/PLATFORM_ARCH.md` |
| EDIT   | `docs/specs/MOUSE.md` |
| EDIT   | `docs/UI_OVERLAY_PLAN.md` |
| EDIT   | `docs/MACROMAN_UNIFY_DESIGN.md` |
| EDIT   | `docs/features/LOW_MEMORY_DESIGN.md` |
| EDIT   | `docs/features/LOW_MEMORY_PLAN.md` |
