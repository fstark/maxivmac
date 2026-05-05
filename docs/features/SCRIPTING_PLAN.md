# Scripting — Implementation Plan

Design: [SCRIPTING_DESIGN.md](SCRIPTING_DESIGN.md)
Spec: [SCRIPTING.md](SCRIPTING.md)

## Prerequisites

- **Event queue refactor** ([EVENTS.md](EVENTS.md)) — must be
  completed before this plan begins.  Key injection (Phase 7)
  depends on the new `EventQ_Push()` API with `fireCycle` timestamps.

## Phase Summary

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Breakpoint Kind enum + temporary/scriptOwned fields | |
| 2 | Script suspension and resumption (PendingScript) | |
| 3 | Text type in traps.def + TypeRegistry isText flag | |
| 4 | Text breakpoint matching (bp_text) | |
| 5 | Screen breakpoint matching (bp_screen) | |
| 6 | Script commands: wait, timeout, screenshot, fail, showtext | |
| 7 | Key injection: type, key, clear keys, info keys (requires EVENTS) | |
| 8 | Power-off breakpoint (break off / wait off) | |
| 9 | Guest commands: launch, exittoshell, shutdown | |
| 10 | End-to-end integration tests | |

Build gate: `cmake --preset macos && cmake --build --preset macos`
Test gate:  `./bld/macos/tests`

---

## Phase 1 — Breakpoint Kind Enum + Temporary/ScriptOwned Fields

Refactor the existing `Breakpoint` struct to support multiple
condition types via a discriminated enum.  This is the foundation
for all new breakpoint kinds (text, screen, power-off) without
changing any existing behavior.

### 1.1 — Extend `struct Breakpoint` in `src/debugger/debugger.h`

Add the `Kind` enum and new fields.  Existing breakpoints become
`Kind::Address` or `Kind::Trap`.

```cpp
struct Breakpoint
{
    uint32_t id;
    bool enabled = true;
    bool temporary = false;      // deleted after first hit (tbreak, wait)
    bool scriptOwned = false;    // created by `wait` — auto-resumes script

    // Condition type — exactly one is active:
    enum class Kind { Address, Trap, Text, Screen, PowerOff };
    Kind kind = Kind::Address;

    uint32_t address = 0;           // PC address (Kind::Address)
    uint16_t trapWord = 0;          // trap word (Kind::Trap)
    uint16_t subtrapSelector = 0;   // subtrap selector (Kind::Trap)

    std::string textPattern;        // UTF-8 substring (Kind::Text)
    // ScreenMatcher added in Phase 5

    std::string condition;          // expression (all kinds)
    std::vector<std::string> commands;
    uint32_t ignoreCount = 0;

    // Timeout: instruction-count deadline (0 = no timeout)
    uint64_t timeoutAt = 0;
};
```

### 1.2 — Update `addBreakpoint()` in `src/debugger/debugger.cpp`

Existing `addBreakpoint(addr, trapWord, condition)` sets
`kind = addr ? Kind::Address : Kind::Trap`.  No behavior change for
callers.

### 1.3 — Implement `temporary` semantics

In the breakpoint-fire path (inside `instructionHook()` and
`trapHook()`), after executing commands and evaluating conditions:

```cpp
if (bp.temporary)
    deleteById(bp.id);
```

### 1.4 — Implement `tbreak` command

Add a `tbreak` alias in `cmd_break.cpp` that creates a breakpoint
with `temporary = true`.  This is a one-line extension of `CmdBreak`.

### 1.5 — Tests

Create `test/debugger/test_breakpoint_kinds.cpp`:
- Verify `Kind::Address` breakback fire + deletion (temporary)
- Verify `Kind::Trap` breakpoint with temporary flag
- Verify `scriptOwned` flag is preserved through add/delete cycle
- Verify non-temporary breakpoints are NOT deleted on fire

### Fence

- [ ] `Breakpoint` has `Kind` enum, `temporary`, `scriptOwned`, `timeoutAt` fields
- [ ] `tbreak` command works (fires once, then auto-deleted)
- [ ] Existing breakpoints still work — no behavioral regression
- [ ] Unit tests pass: `./bld/macos/tests --test-case="*breakpoint*kind*"`
- [ ] Full build clean
- [ ] Commit: `"scripting: phase 1 — breakpoint Kind enum + temporary field"`

---

## Phase 2 — Script Suspension and Resumption

Implement the `PendingScript` struct and modify `executeCommands()`
and `instructionHook()` to support suspend/resume.  After this phase,
any command that sets state != Stopped will save remaining script
lines and replay them when the debugger next stops.

### 2.1 — Create `src/debugger/script_suspend.h`

See Design §3.

```cpp
// src/debugger/script_suspend.h
#pragma once
#include <string>
#include <string_view>
#include <vector>

// Holds the remaining lines of a suspended script.
// When a script command resumes the CPU (e.g. wait, continue),
// execution suspends here and resumes when the debugger next stops.
struct PendingScript
{
    std::vector<std::string> lines;
    int nextLine = 0;
    std::string sourceFile;  // for error messages

    bool exhausted() const { return nextLine >= static_cast<int>(lines.size()); }
    std::string_view currentLine() const { return lines[nextLine]; }
    void advance() { ++nextLine; }
};
```

### 2.2 — Create `src/debugger/script_suspend.cpp`

Minimal — just the TU anchor.  The logic lives in `debugger.cpp` but
this file allows future helpers (e.g. error formatting).

```cpp
// src/debugger/script_suspend.cpp
#include "script_suspend.h"
```

### 2.3 — Add `PendingScript` member to `Debugger::Impl`

In `src/debugger/debugger.cpp`, add:

```cpp
PendingScript pendingScript_;
```

### 2.4 — Modify `executeCommands()` to suspend

After each command dispatch, if `impl_->state != DbgState::Stopped`:

```cpp
// Script suspended — save remaining lines
impl_->pendingScript_ = PendingScript{
    std::move(cmds), i + 1, sourceFile_
};
return;
```

At the end (all lines executed): `impl_->pendingScript_ = {};`

### 2.5 — Modify stop-path to resume pending scripts

In the debugger's stop logic (where `commandLoop()` is entered),
before the prompt:

```cpp
if (!impl_->pendingScript_.exhausted()) {
    if (firedBp && firedBp->scriptOwned) {
        // Condition met — resume script
        executeCommands(impl_->pendingScript_.lines,
                        impl_->pendingScript_.nextLine);
        if (impl_->state != DbgState::Stopped)
            return;  // script resumed CPU again
    }
    // User breakpoint or script finished — fall to prompt
}
commandLoop();
```

See Design §5.2 for the full algorithm.

### 2.6 — Tests

Create `test/debugger/test_script_suspend.cpp`:
- `PendingScript::exhausted()` returns true for empty/complete
- `PendingScript::advance()` increments nextLine
- `PendingScript::currentLine()` returns correct line
- Integration: load a 3-line script, verify suspension on `continue`
  and resumption on next stop

### Fence

- [ ] `script_suspend.h` and `.cpp` exist
- [ ] `executeCommands()` suspends on state change
- [ ] Pending script resumes on debugger stop
- [ ] `scriptOwned` BPs auto-resume; non-scriptOwned BPs give prompt
- [ ] Unit tests pass: `./bld/macos/tests --test-case="*script*suspend*"`
- [ ] Existing `.dbg` scripts (`continue`, `run`) still work
- [ ] Full build clean
- [ ] Commit: `"scripting: phase 2 — script suspension and resumption"`

---

## Phase 3 — Text Type in traps.def + TypeRegistry isText Flag

Add the semantic `Text` type annotation to the type system.  This
enables the trap tracer to distinguish text-bearing parameters from
regular `Str255` values without changing display behavior.

### 3.1 — Add `isText` field to `ParamDef`

In `src/cpu/trap_defs.h`, add to `struct ParamDef`:

```cpp
bool isText = false;  // true if this param carries displayable text
```

### 3.2 — Recognize `Text` in TypeRegistry parser

In `src/cpu/type_registry.cpp` (or wherever `traps.def` parsing
resolves type names), when `typeName == "Text"`:

```cpp
field.baseType = BaseType::Str255;
field.isText = true;
```

Display format is identical to `Str255` — the flag is purely
semantic.

### 3.3 — Annotate traps in `data/debug/traps.def`

Change these entries:

```
A884 DrawString toolbox
  in  s:Text

A98B ParamText toolbox
  in  param0:Text  param1:Text  param2:Text  param3:Text
```

### 3.4 — Tests

- Verify `Text` parses without error: load `traps.def`, confirm
  DrawString's param has `isText == true` and `baseType == Str255`
- Verify `Str255` params do NOT have `isText`
- Trace output unchanged (Text displays the same as Str255)

### Fence

- [ ] `ParamDef` has `isText` field
- [ ] TypeRegistry recognizes `Text` → `Str255` + `isText = true`
- [ ] `traps.def` has `Text` annotation on DrawString and ParamText
- [ ] Trace output unchanged (no visible difference)
- [ ] Unit tests pass
- [ ] Full build clean
- [ ] Commit: `"scripting: phase 3 — Text type annotation in traps.def"`

---

## Phase 4 — Text Breakpoint Matching (bp_text)

Implement `ScriptCaptureText()` and the text breakpoint evaluation
logic.  After this phase, `break text "pattern"` can be set and will
fire when matching text flows through the trap tracer.

### 4.1 — Create `src/debugger/bp_text.h`

```cpp
// src/debugger/bp_text.h
#pragma once
#include <string_view>

// Called from TrapTracer::formatParam() when a Text-typed param
// is formatted.  Checks incoming text against all active text
// breakpoints and fires any that match.
void ScriptCaptureText(std::string_view utf8Text,
                       std::string_view trapName);

// Toggle live text display to console.
void ScriptShowTextSet(bool on);
bool ScriptShowTextGet();
```

### 4.2 — Create `src/debugger/bp_text.cpp`

Implement `ScriptCaptureText()` — see Design §5.3:

1. If `showtext` is on, print `[text] trapName: "utf8Text"`
2. Iterate all enabled `Kind::Text` breakpoints
3. Check timeout (`g_instructionCount >= bp.timeoutAt`)
4. Check substring match (`utf8Text.find(bp.textPattern) != npos`)
5. Honor `ignoreCount`, evaluate `condition`, fire if matched

Use `dbg.breakpoints()` to access the breakpoint vector.  Filter
to `Kind::Text` only.

### 4.3 — Hook into `TrapTracer::formatParam()`

In `src/cpu/trap_tracer.cpp`, after formatting a `Str255` value
when `def.isText` is true:

```cpp
if (def.isText) {
    ScriptCaptureText(utf8, trapDef.name);
}
```

Guard: only call when the debugger is active
(`extern bool g_debuggerActive;`).

### 4.4 — Extend `CmdBreak` for `break text "pattern"`

In `src/debugger/cmd_break.cpp`, parse:

```
break text "pattern"
```

Create a breakpoint with `kind = Kind::Text` and
`textPattern = pattern`.

### 4.5 — Add `showtext` command

In `src/debugger/cmd_break.cpp` (or a new `cmd_script.cpp` — but
defer creating that file until Phase 6 to avoid empty stubs):

```
showtext [on|off]
```

Calls `ScriptShowTextSet()` / toggles.  Add to `s_commands[]`.

### 4.6 — Tests

Create `test/debugger/test_bp_text.cpp`:
- Exact match fires breakpoint
- Substring match fires breakpoint
- Non-matching text does NOT fire
- `ignoreCount` honored (first N skipped)
- Disabled breakpoint does not match
- `showtext` output contains the text

### Fence

- [ ] `bp_text.h` / `.cpp` exist with `ScriptCaptureText()`
- [ ] `break text "pattern"` creates a Kind::Text breakpoint
- [ ] Text BPs fire when matching text appears in trap params
- [ ] `showtext` prints captured text to console
- [ ] No performance impact when no text BPs exist
- [ ] Unit tests pass: `./bld/macos/tests --test-case="*bp*text*"`
- [ ] Full build clean
- [ ] Commit: `"scripting: phase 4 — text breakpoint matching"`

---

## Phase 5 — Screen Breakpoint Matching (bp_screen)

Implement `ScreenMatcher` and per-tick evaluation.  After this phase,
`break screen "ref.png"` can be set and fires when the framebuffer
matches the reference.

### 5.1 — Create `src/debugger/bp_screen.h`

```cpp
// src/debugger/bp_screen.h
#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

// Loads a reference PNG and compares against the live framebuffer.
// Pixel comparison: count identical ARGB pixels, fire if
// matchCount / totalPixels * 100 >= threshold.
struct ScreenMatcher
{
    std::vector<uint32_t> refPixels;  // ARGB8888 from reference PNG
    int refWidth = 0;
    int refHeight = 0;
    float threshold = 99.85f;         // percent match required

    bool loadReference(const std::filesystem::path &png);
    bool matches(const uint8_t *framebuffer, int width, int height) const;
};

// Called once per tick (60 Hz).  Checks all active screen breakpoints.
void CheckScreenBreakpoints();
```

### 5.2 — Create `src/debugger/bp_screen.cpp`

Implement:

- `loadReference()` — use `stb_image.h` to load PNG as ARGB8888.
  Validate it loaded successfully.  Store width/height/pixels.
- `matches()` — iterate all pixels, count identical (ignoring alpha
  channel), return `count * 100.0f / total >= threshold`.
- `CheckScreenBreakpoints()` — see Design §5.4.  Early-return if
  no screen BPs exist.  Access framebuffer via
  `EmulatorShell::getFramebuffer()`.

### 5.3 — Add `ScreenMatcher` field to Breakpoint

In `src/debugger/debugger.h`, add to `struct Breakpoint`:

```cpp
ScreenMatcher screenMatcher;  // Kind::Screen only
```

Include `"bp_screen.h"` from `debugger.h`.

### 5.4 — Hook `CheckScreenBreakpoints()` into the tick path

In `src/core/main.cpp`, in `SixtiethEndNotify()`:

```cpp
if (g_debuggerActive)
    CheckScreenBreakpoints();
```

One function call per tick when the debugger is active.

### 5.5 — Extend `CmdBreak` for `break screen "ref.png" [pct]`

Parse:

```
break screen "reference.png"
break screen "reference.png" 95
```

Create breakpoint with `kind = Kind::Screen`, call
`bp.screenMatcher.loadReference(path)`.  Error if PNG fails to load.
Optional second argument overrides `threshold`.

### 5.6 — Implement `screenshot` command

Save the current framebuffer to a PNG file using `stb_image_write.h`:

```
screenshot "label"
```

Saves to `<script-dir>/label.png`.  Add `CmdScreenshot` to
`s_commands[]`.  This is also used by the timeout failure path.

### 5.7 — Tests

Create `test/debugger/test_screen_match.cpp`:
- Load a known PNG, compare against identical buffer → match
- Compare against different buffer → no match
- Threshold 50% with half-different → match
- Threshold 100% with one pixel off → no match
- Load nonexistent PNG → returns false
- PNG size mismatch → returns false

Create a small test PNG fixture in `test/fixtures/`:
- 8×8 solid white PNG for deterministic testing

### Fence

- [ ] `bp_screen.h` / `.cpp` exist with `ScreenMatcher`
- [ ] `break screen` creates Kind::Screen breakpoint
- [ ] Screen BPs check once per tick, fire on match
- [ ] `screenshot` command saves framebuffer to PNG
- [ ] No performance impact when no screen BPs exist
- [ ] Unit tests pass: `./bld/macos/tests --test-case="*screen*match*"`
- [ ] Full build clean
- [ ] Commit: `"scripting: phase 5 — screen breakpoint matching"`

---

## Phase 6 — Script Commands: wait, timeout, fail, showtext

Implement the core scripting command set.  These compose with the
suspension system (Phase 2) and breakpoint types (Phases 4–5).

### 6.1 — Create `src/debugger/cmd_script.cpp`

Command handlers for scripting.  All follow the standard
`void CmdXxx(Debugger&, const std::vector<Token>&)` signature.

### 6.2 — Implement `CmdTimeout`

```
timeout <cycles>
```

Sets `impl_->defaultTimeout_` (new field, default 40'000'000).
Used by `wait` when no explicit timeout is given.

### 6.3 — Implement `CmdWait`

See Design §4.5.  Parse the condition type from args:

| Form | Creates |
|---|---|
| `wait text "pat" [cycles]` | Kind::Text, temporary, scriptOwned |
| `wait screen "ref.png" [cycles] [pct]` | Kind::Screen, temporary, scriptOwned |
| `wait off [cycles]` | Kind::PowerOff, temporary, scriptOwned |
| `wait $addr` / `wait symbol` | Kind::Address, temporary, scriptOwned |
| `wait trap Name` | Kind::Trap, temporary, scriptOwned |

Set `bp.timeoutAt = g_instructionCount + budget`.
Call `dbg.addBreakpoint(bp)` then `dbg.setRunning()`.

The `setRunning()` call triggers script suspension (Phase 2 logic
in `executeCommands()`).

### 6.4 — Implement timeout checking

Add timeout evaluation to the breakpoint check paths:

- In `ScriptCaptureText()` (bp_text.cpp): before condition check,
  if `bp.timeoutAt != 0 && g_instructionCount >= bp.timeoutAt` →
  call `FireTimeout(bp)`.
- In `CheckScreenBreakpoints()` (bp_screen.cpp): same check.
- In `instructionHook()`: for Kind::Address with `timeoutAt`, check
  on each instruction.

`FireTimeout()` — see Design §5.7:
1. Save fail screenshot
2. Print error
3. Delete the breakpoint
4. Clear `pendingScript_`
5. If headless → `exit(1)`; else → `commandLoop()`

### 6.5 — Implement `CmdFail`

```
fail "message"
```

Print message to stderr, save screenshot, exit(1) if headless,
else drop to prompt.

### 6.6 — Register commands in `s_commands[]`

Add to the command table in `debugger.cpp`:

```cpp
{"timeout",    "",  CmdTimeout,    "Set default wait budget"},
{"wait",       "",  CmdWait,       "Wait for condition"},
{"fail",       "",  CmdFail,       "Abort with error"},
```

(`showtext` already added in Phase 4; `screenshot` in Phase 5.)

### 6.7 — Tests

Integration test script `test/scripts/wait_text.dbg`:

```
showtext on
timeout 10000000
wait text "Welcome"
screenshot "after_welcome"
```

Unit tests in `test/debugger/test_cmd_script.cpp`:
- `CmdTimeout` sets the default budget
- `CmdWait` with text creates a temporary, scriptOwned, Kind::Text BP
- `CmdFail` exits with code 1 in headless mode
- Timeout fires when `g_instructionCount` exceeds deadline

### Fence

- [ ] `cmd_script.cpp` exists with `CmdWait`, `CmdTimeout`, `CmdFail`
- [ ] `wait text` / `wait screen` / `wait <addr>` / `wait trap` all work
- [ ] Timeout fires and produces screenshot + error message
- [ ] `fail` exits non-zero
- [ ] Commands registered in `s_commands[]`
- [ ] Unit tests pass: `./bld/macos/tests --test-case="*cmd*script*"`
- [ ] Full build clean
- [ ] Commit: `"scripting: phase 6 — wait, timeout, fail commands"`

---

## Phase 7 — Key Injection: type, key, clear keys, info keys

### 7.1 — Create `src/debugger/script_keymap.h`

```cpp
// src/debugger/script_keymap.h
#pragma once
#include <cstdint>
#include <string_view>
#include <utility>

// Map a MacRoman byte to (Mac virtual keycode, needShift).
// Returns {0xFF, false} for unmappable characters.
std::pair<uint8_t, bool> CharToMacKey(uint8_t macRomanChar);

// Parse a modifier+key spec like "cmd-shift-S" or "return".
// Returns (keycode, modifierMask).  modifierMask bits:
//   bit 0 = cmd, bit 1 = shift, bit 2 = option, bit 3 = ctrl
struct KeySpec { uint8_t keycode; uint8_t modifiers; };
KeySpec ParseKeySpec(std::string_view spec);
```

### 7.2 — Create `src/debugger/script_keymap.cpp`

Implement:

- `CharToMacKey()` — `constexpr` lookup table indexed by MacRoman
  byte.  Covers ASCII 0x20–0x7E.  Returns keycode + shift flag.
  Reference: Inside Macintosh, Volume V — Mac virtual keycodes.

- `ParseKeySpec()` — split on `-`, recognise modifier names (`cmd`,
  `shift`, `opt`, `ctrl`), last token is the key name.  Special key
  names: `return`, `enter`, `tab`, `escape`, `delete`, `space`,
  `left`, `right`, `up`, `down`, `f1`–`f15`.

### 7.3 — Implement `CmdType` in `cmd_script.cpp`

See Design §5.5:

```cpp
void CmdType(Debugger &dbg, const std::vector<Token> &args)
{
    // Convert UTF-8 argument to MacRoman
    std::string macRoman = MacRomanFromUTF8(args[0].str);
    uint64_t t = g_instructionCount;

    for (uint8_t ch : macRoman) {
        auto [keycode, needShift] = CharToMacKey(ch);
        if (keycode == 0xFF) continue;  // unmappable

        if (needShift)
            EventQ_Push({t, EvtQElKind::Key, {MKC_Shift, true}});
        EventQ_Push({t,         EvtQElKind::Key, {keycode, true}});
        EventQ_Push({t + 80000, EvtQElKind::Key, {keycode, false}});
        if (needShift)
            EventQ_Push({t + 80000, EvtQElKind::Key, {MKC_Shift, false}});
        t += 160000;  // ~20ms inter-key gap at 8 MHz
    }
}
```

### 7.4 — Implement `CmdKey` in `cmd_script.cpp`

```
key cmd-S
key return
```

Parse via `ParseKeySpec()`.  Push modifier-down, key-down, key-up,
modifier-up with staggered timestamps into the event queue.

### 7.5 — Implement `clear keys` and `info keys`

- `clear keys` → `EventQ_ClearFutureKeys()`
- `info keys` → iterate `EventQ_PendingKeys()`, print each pending
  event with its scheduled cycle

Add `clear keys` to `s_commands[]`.
Add `info keys` as a subcommand of the existing `CmdInfo`.

### 7.6 — Register commands

```cpp
{"type",  "",  CmdType,  "Inject text as keystrokes"},
{"key",   "",  CmdKey,   "Inject a key press"},
```

### 7.7 — Tests

Create `test/debugger/test_script_keymap.cpp`:
- `CharToMacKey('a')` → correct keycode, shift=false
- `CharToMacKey('A')` → same keycode, shift=true
- `CharToMacKey('1')` → correct keycode
- `CharToMacKey('!')` → correct keycode, shift=true
- `ParseKeySpec("return")` → MKC_Return, modifiers=0
- `ParseKeySpec("cmd-S")` → MKC_S, modifiers=cmd
- `ParseKeySpec("cmd-shift-N")` → MKC_N, modifiers=cmd|shift

Integration test `test/scripts/type_test.dbg`:
```
type "Hello"
info keys
```

### Fence

- [ ] `script_keymap.h` / `.cpp` exist with CharToMacKey + ParseKeySpec
- [ ] `type "text"` enqueues timed key events
- [ ] `key cmd-S` enqueues a single modified key press
- [ ] `clear keys` removes all pending key events
- [ ] `info keys` displays the queue
- [ ] Unit tests pass: `./bld/macos/tests --test-case="*keymap*"`
- [ ] Full build clean
- [ ] Commit: `"scripting: phase 7 — key injection (type, key)"`

---

## Phase 8 — Power-Off Breakpoint (break off / wait off)

Implement the `Kind::PowerOff` breakpoint that fires when the guest
initiates shutdown via the power manager (Mac II and later).

### 8.1 — Hook the existing power-off callback

The mechanism already exists: `Wire_VIA2_iB2_PowerOff` triggers
`PowerOff_ChangeNtfy()` in `src/core/machine.cpp` (which sets
`g_forceMacOff = true`).  Add the debugger check there:

```cpp
void PowerOff_ChangeNtfy()
{
    if (!VIA2_iB2) {
        // Scripting: check power-off breakpoints before quitting
        if (g_debuggerActive)
            CheckPowerOffBreakpoints();
        g_forceMacOff = true;
    }
}
```

No new wire registration needed — just insert the check before the
existing `g_forceMacOff = true` line.

### 8.2 — Implement `CheckPowerOffBreakpoints()`

In `src/debugger/bp_screen.cpp` (small, colocated with other BP
checks):

```cpp
void CheckPowerOffBreakpoints()
{
    auto &bps = Debugger::instance()->breakpoints();
    for (auto &bp : bps) {
        if (bp.kind != Breakpoint::Kind::PowerOff) continue;
        if (!bp.enabled) continue;
        Debugger::instance()->fireBreakpoint(bp);
        return;
    }
}
```

### 8.3 — Extend `CmdBreak` for `break off`

Parse `break off` → create `Kind::PowerOff` breakpoint.

### 8.4 — Extend `CmdWait` for `wait off`

Already partially handled in Phase 6 — just ensure the `wait off`
path creates a `Kind::PowerOff` BP with temporary + scriptOwned.

### 8.5 — Tests

- Unit: create a Kind::PowerOff breakpoint, simulate calling
  `CheckPowerOffBreakpoints()`, verify it fires
- Note: full integration requires Mac II boot, which is a manual
  test.  Document in `docs/MANUAL_TEST.md`.

### Fence

- [ ] `break off` creates a Kind::PowerOff breakpoint
- [ ] `wait off` creates temporary + scriptOwned PowerOff BP
- [ ] VIA2 power-bit write triggers check
- [ ] Only fires on Mac II and later (no false triggers on Plus/SE)
- [ ] Unit test passes
- [ ] Full build clean
- [ ] Commit: `"scripting: phase 8 — power-off breakpoint"`

---

## Phase 9 — Guest Commands: launch, exittoshell, shutdown

Implement the host→guest command channel for controlling the guest
application lifecycle.  Requires the SharedDrive INIT to be loaded.

### 9.1 — Add command code to `src/core/extn_extfs.cpp`

Define `kExtFSGuestCmd = 0x220`.  Add a handler in the dispatch
switch:

```cpp
case kExtFSGuestCmd:
    // Return pending guest command (0=none, 1=launch, 2=exit, 3=shutdown)
    pbWrite<uint16_t>(kResult, s_pendingGuestCmd);
    s_pendingGuestCmd = 0;  // consume
    break;
```

Add a function to enqueue commands from the debugger:

```cpp
void ExtFS_QueueGuestCmd(uint16_t cmd, std::string_view path = {});
```

### 9.2 — Implement `CmdLaunch` in `cmd_script.cpp`

```
launch "Shared Drive:My App"
```

Calls `ExtFS_QueueGuestCmd(1, path)`.  The path is MacRoman,
converted from UTF-8 input.  Path is written to a transfer buffer
readable by the INIT.

### 9.3 — Implement `CmdExitToShell`

```
exittoshell
```

Calls `ExtFS_QueueGuestCmd(2)`.

### 9.4 — Implement `CmdShutdown`

```
shutdown
```

Calls `ExtFS_QueueGuestCmd(3)`.

### 9.5 — Register commands

```cpp
{"launch",      "",  CmdLaunch,      "Launch app via INIT"},
{"exittoshell", "",  CmdExitToShell, "Quit current app via INIT"},
{"shutdown",    "",  CmdShutdown,    "Shut down guest via INIT"},
```

### 9.6 — Tests

Integration test `test/scripts/launch_test.dbg`:
```
timeout 80000000
wait text "Welcome to Macintosh"
launch "Shared Drive:TeachText"
wait text "TeachText"
screenshot "teachtext_launched"
exittoshell
```

Note: requires a configured boot disk with SharedDrive INIT.
Mark as manual integration test in CI.

### Fence

- [ ] `kExtFSGuestCmd` (0x220) implemented in `extn_extfs.cpp`
- [ ] `ExtFS_QueueGuestCmd()` queues commands for INIT pickup
- [ ] `launch`, `exittoshell`, `shutdown` commands registered
- [ ] Path transfer works (MacRoman conversion)
- [ ] Full build clean
- [ ] Guest INIT modifications deferred (user builds INIT at end)
- [ ] Commit: `"scripting: phase 9 — guest commands (launch/exit/shutdown)"`

---

## Phase 10 — End-to-End Integration Tests

Verify the complete scripting flow works as a cohesive system.
Create test scripts that exercise multi-step scenarios.

### 10.1 — Boot-and-wait test

`test/scripts/boot_wait.dbg`:

```
# Boot Mac Plus to Finder desktop, verify it arrives.
timeout 80000000
showtext on
wait text "File"
screenshot "desktop"
```

Run: `maxivmac --headless --mac "Mac Plus" --disk test.hfs --dbg-script=test/scripts/boot_wait.dbg`

### 10.2 — Type-and-verify test

`test/scripts/type_verify.dbg`:

```
# Boot, open a DA, type text, verify it appears.
timeout 80000000
wait text "File"
type "Hello World"
wait text "Hello World"
screenshot "typed"
```

### 10.3 — Screen match test

`test/scripts/screen_match.dbg`:

```
# Boot to desktop, capture reference, verify screen match.
timeout 80000000
wait text "File"
break screen "test/fixtures/desktop.png" 90
continue
screenshot "matched"
```

### 10.4 — Failure test

`test/scripts/must_timeout.dbg`:

```
# Verify that timeout produces non-zero exit.
timeout 1000
wait text "This will never appear"
```

Expect: exit code 1, `fail-*.png` saved.

### 10.5 — CI integration

Add a test target or script that runs the headless tests:

```sh
#!/bin/sh
# test/run_script_tests.sh
set -e
./bld/macos/maxivmac --headless --mac "Mac Plus" \
    --disk test/fixtures/boot.hfs \
    --dbg-script=test/scripts/boot_wait.dbg

# Timeout test should fail — invert exit code
! ./bld/macos/maxivmac --headless --mac "Mac Plus" \
    --disk test/fixtures/boot.hfs \
    --dbg-script=test/scripts/must_timeout.dbg
```

### Fence

- [ ] `test/scripts/boot_wait.dbg` passes in headless mode
- [ ] `test/scripts/must_timeout.dbg` exits non-zero as expected
- [ ] Screenshot files are produced at expected paths
- [ ] No regressions in existing debugger functionality
- [ ] Test runner script exits 0
- [ ] Full build clean
- [ ] Commit: `"scripting: phase 10 — end-to-end integration tests"`

---

## Build Integration (applies across all phases)

### CMakeLists.txt additions (cumulative)

Add to the `MINIVMAC_SOURCES` list in the debugger section as each
file is created:

```cmake
# ── Scripting ─────────────────────────────────────────
src/debugger/script_suspend.cpp     # Phase 2
src/debugger/bp_text.cpp            # Phase 4
src/debugger/bp_screen.cpp          # Phase 5
src/debugger/cmd_script.cpp         # Phase 6
src/debugger/script_keymap.cpp      # Phase 7
```

Each phase that creates a new `.cpp` file must add it to CMakeLists
in that phase's commit.

---

## Comment Quality Guidelines

All new code must include:

- **File-level comments** — one-line purpose at the top of each new
  `.h` and `.cpp` file (after the `#pragma once` / includes):
  ```cpp
  // Text breakpoint matching — fires when trap params contain target text.
  ```

- **Public function doc comments** — brief description of what each
  public function does, its contract, and when it's called:
  ```cpp
  // Called from TrapTracer when a Text-typed param is formatted.
  // Thread safety: called from the CPU thread only.
  void ScriptCaptureText(std::string_view utf8Text, std::string_view trapName);
  ```

- **Non-obvious algorithm comments** — explain *why*, not *what*:
  ```cpp
  // Check timeout before condition — avoids matching stale text
  // that arrived after the deadline.
  if (bp.timeoutAt != 0 && g_instructionCount >= bp.timeoutAt) { ... }
  ```

- **Integration hook comments** — mark every insertion into existing
  files with context about why it's there:
  ```cpp
  // Scripting: capture text for text breakpoints (see bp_text.h)
  if (def.isText && g_debuggerActive)
      ScriptCaptureText(utf8, trapDef.name);
  ```

Do NOT add:
- Redundant comments restating the code
- `// TODO` without a tracking issue
- Comments on obvious operations (`i++; // increment i`)

---

## Design Concerns Noted

1. **No `temporary` field exists today** — The current `Breakpoint`
   struct has no `temporary` flag.  The `tbreak`/`until` mechanism
   uses a separate `insnBreakCount` path.  Phase 1 adds `temporary`
   as a first-class field and must carefully preserve the existing
   insn-break behavior.

2. **Guest INIT built separately** — Phase 9 only covers host-side
   code (`extn_extfs.cpp` + debugger commands).  The user will build
   the INIT (in `macsrc/shareddrive/`) at the end.
