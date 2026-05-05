# Scripting — Detailed Design

Implements the specification in [SCRIPTING.md](SCRIPTING.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Architecture Overview

Scripting adds three things to the existing debugger:

1. **New breakpoint types** — text match, screen match, power-off.
   These integrate into the existing breakpoint table and check
   infrastructure; they aren't a separate system.
2. **Script suspension/resumption** — a pending-script queue in the
   debugger that saves remaining lines when execution resumes and
   replays them when the debugger next stops.
3. **Utility commands** — key injection, screenshots, guest commands,
   showtext.

There is no new execution model.  No recursion.  No synchronous CPU
loops.  The debugger's existing flat state machine (`Running` /
`Stopped` / `Stepping`) is unchanged — `wait` simply creates a temp
breakpoint and sets state to `Running`, exactly like `continue`.

---

## 2. Directory Layout

```
src/debugger/
  cmd_script.cpp          Command handlers: wait, timeout, type, key,
                          screenshot, launch, exittoshell, shutdown,
                          showtext, fail, clear keys
  script_suspend.h        PendingScript struct + resume logic
  script_suspend.cpp      Script suspension/resumption implementation
  bp_text.h              ScriptCaptureText() + text BP matching
  bp_text.cpp            Text capture + match logic (no ring)
  bp_screen.h            Screen breakpoint condition
  bp_screen.cpp          PNG load, pixel comparison
  script_keymap.h        CharToMacKey table + modifier parsing
  script_keymap.cpp      Char→keycode mapping, UTF-8→MacRoman
```

Everything lives in `src/debugger/` — scripting is an extension of
the debugger, not a separate module.  The breakpoint types are peers
of the existing address/trap/watchpoint logic.

---

## 3. Public Interface

No new global engine class.  The breakpoint types and script suspend
logic integrate directly into `Debugger::Impl`:

```cpp
// src/debugger/script_suspend.h
#pragma once
#include <string>
#include <vector>

struct PendingScript
{
    std::vector<std::string> lines;
    int nextLine = 0;
    std::string sourceFile;     // for error messages

    bool exhausted() const { return nextLine >= (int)lines.size(); }
    std::string_view currentLine() const { return lines[nextLine]; }
    void advance() { ++nextLine; }
};
```

```cpp
// src/debugger/bp_text.h
#pragma once
#include <string_view>

/* Called from TrapTracer when a Text-typed param is formatted.
   Checks the incoming text against all active text breakpoints
   and fires any that match. */
void ScriptCaptureText(std::string_view utf8Text, std::string_view trapName);
```

```cpp
// src/debugger/bp_screen.h
#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

struct ScreenMatcher
{
    std::vector<uint32_t> refPixels;    // ARGB8888 from reference PNG
    int refWidth = 0;
    int refHeight = 0;
    float threshold = 99.85f;

    bool loadReference(const std::filesystem::path &png);
    bool matches(const uint8_t *framebuffer, int width, int height) const;
};
```

---

## 4. Integration Points

### 4.1 Breakpoint table — new condition types

**File:** [src/debugger/debugger.cpp](src/debugger/debugger.cpp) —
`struct Breakpoint`.

The existing `Breakpoint` struct gains new fields:

```cpp
struct Breakpoint
{
    uint32_t id;
    bool enabled = true;
    bool temporary = false;         // deleted on first hit (tbreak, wait)
    bool scriptOwned = false;       // created by `wait` — auto-resumes script on fire
    uint32_t ignoreCount = 0;
    std::string condition;          // expression (existing)
    std::vector<std::string> commands;

    // Condition type — exactly one is active:
    enum class Kind { Address, Trap, Text, Screen, PowerOff };
    Kind kind;

    // Address breakpoints (existing):
    uint32_t addr = 0;

    // Trap breakpoints (existing):
    uint16_t trapWord = 0;
    uint16_t subtrapSelector = 0;

    // Text breakpoints (new):
    std::string textPattern;        // UTF-8 substring to match

    // Screen breakpoints (new):
    ScreenMatcher screenMatcher;    // loaded reference + threshold

    // Timeout (new): instruction-count deadline for wait commands.
    // When g_instructionCount >= timeoutAt, the wait fails.
    // 0 = no timeout.
    uint64_t timeoutAt = 0;
};
```

### 4.2 Text breakpoint evaluation

**Where:** Inside `TrapTracer::formatParam()` — after reading a
`Str255` value from guest memory for a param typed `Text`:

```cpp
if (def.isText && g_debuggerActive) {
    std::string utf8 = UTF8FromMacRoman(pstr);
    ScriptCaptureText(utf8, trapName);
}
```

`ScriptCaptureText()`:
1. If `showtext` is on, prints `[text] TrapName: "text"` to console.
2. Iterates all enabled `Kind::Text` breakpoints.  For each, checks
   if `utf8.find(bp.textPattern) != npos`.  If matched:
   - Honors `ignoreCount` (decrement, skip if positive)
   - Evaluates `condition` expression if present
   - Fires: `stop()`, executes `commands`, marks `temporary` BPs
     for deletion.

**Cost when no text BPs exist:** One null check + empty-vector scan.

### 4.3 Screen breakpoint evaluation

**Where:** End of each emulation tick — in `SixtiethEndNotify()` or
an equivalent per-tick hook.

```cpp
// Called once per tick (60 Hz), after the framebuffer is updated:
void CheckScreenBreakpoints()
{
    if (!g_debuggerActive) return;
    auto *dbg = Debugger::instance();
    for (auto &bp : dbg->screenBreakpoints()) {
        if (!bp.enabled) continue;
        if (bp.screenMatcher.matches(getFramebuffer(), g_screenWidth, g_screenHeight)) {
            // same fire logic as text: ignoreCount, condition, commands, temp
            dbg->fireBreakpoint(bp);
            return;  // stop CPU after this tick
        }
    }
}
```

We poll once per tick for efficiency.  The framebuffer is valid guest
VRAM at all times (written by CPU instructions), but checking every
instruction would be wasteful and would catch mid-draw states.  Once
per tick gives the guest a full frame to finish drawing.

**Cost when no screen BPs exist:** One pointer check + empty-vector
iteration.

### 4.3a Power-off breakpoint (`break off`)

**Where:** VIA2 register write path — when the soft-power bit in
`vBufB` transitions to the off state.

```cpp
// In VIA2 vBufB write handler:
if (powerBitTransitioned) {
    if (g_debuggerActive)
        CheckPowerOffBreakpoints();
}
```

No per-tick polling — fires exactly when the guest initiates
shutdown via the power manager.  Only relevant on Mac II and later
(machines without soft-power hardware will never trigger this).

### 4.4 Script suspension and resumption

**File:** [src/debugger/debugger.cpp](src/debugger/debugger.cpp) —
`executeCommands()` and `commandLoop()`.

**Suspension:** When `executeCommands()` encounters a command that
sets state != `Stopped` (e.g. `wait` → `setRunning()`), it saves
the remaining lines:

```cpp
void Debugger::executeCommands(std::vector<std::string> cmds, int startLine)
{
    for (int i = startLine; i < (int)cmds.size(); ++i)
    {
        // ... parse and dispatch line ...
        entry->handler(*this, args);

        if (impl_->state != DbgState::Stopped) {
            // Script suspended — move remaining lines (no copy)
            impl_->pendingScript = PendingScript{std::move(cmds), i + 1, std::move(scriptFile_)};
            return;
        }
    }
    // Script completed — clear pending
    impl_->pendingScript = {};
}
```

**Resumption:** When the debugger stops (any reason — breakpoint,
trap break, timeout, Ctrl-C), before entering `commandLoop()`:

```cpp
bool Debugger::instructionHook(uint32_t pc)
{
    // ... existing checks (stepping, until, finish, next, BPs) ...

    // If stopped and a script is pending, resume it instead of
    // entering the interactive command loop:
    if (impl_->state == DbgState::Stopped && !impl_->pendingScript.exhausted()) {
        executeCommands(impl_->pendingScript.lines, impl_->pendingScript.nextLine);
        if (impl_->state != DbgState::Stopped)
            return true;  // script resumed execution
        // Script finished — fall through to commandLoop
    }

    commandLoop();
    return true;
}
```

**Key behavior:**
- A user breakpoint fires during a `wait` → debugger stops → but
  there IS a pending script.  However, user BPs should give the
  prompt.
- Solution: `wait` sets `bp.scriptOwned = true`.  When a BP fires:
  - `scriptOwned` → auto-resume the pending script.
  - Not `scriptOwned` → enter interactive prompt (script stays
    suspended until the user types `continue`).
- This avoids ambiguity with user-created `tbreak` (which is
  temporary but not script-owned).

### 4.5 The `wait` command implementation

`CmdWait` in `cmd_script.cpp`:

```cpp
void CmdWait(Debugger &dbg, const std::vector<Token> &args)
{
    // Parse: wait text "pattern" [cycles]
    //        wait screen "ref.png" [cycles] [pct]
    //        wait off [cycles]
    //        wait <addr/symbol>
    //        wait trap <name>

    // 1. Create a temporary breakpoint with the parsed condition
    Breakpoint bp;
    bp.temporary = true;
    bp.scriptOwned = true;
    bp.kind = ... // from args
    bp.textPattern = ... // or screenMatcher.loadReference(...)

    // 2. Set timeout deadline on the BP itself
    uint64_t budget = parseTimeout(args, dbg.defaultTimeout());
    bp.timeoutAt = g_instructionCount + budget;

    dbg.addBreakpoint(bp);

    // 3. Resume execution (script will suspend)
    dbg.setRunning();
}
```

When the condition matches → temp BP deleted, script resumes at
next line.

When `g_instructionCount >= bp.timeoutAt` → BP deleted, screenshot
saved, error reported, script aborts (headless → exit; interactive
→ prompt).  Timeout is checked in the same per-tick/per-trap
evaluation path as the condition itself — no separate breakpoint
needed.

### 4.6 Debugger command table

**File:** [src/debugger/debugger.cpp](src/debugger/debugger.cpp) — `s_commands[]`.

```cpp
{"timeout",     "",  CmdTimeout,     "Set default wait budget", ...},
{"wait",        "",  CmdWait,        "Wait for condition", ...},
{"type",        "",  CmdType,        "Inject text as keystrokes", ...},
{"key",         "",  CmdKey,         "Inject a key press", ...},
{"screenshot",  "",  CmdScreenshot,  "Save framebuffer to PNG", ...},
{"showtext",    "",  CmdShowText,    "Toggle live text display", ...},
{"launch",      "",  CmdLaunch,      "Launch app via INIT", ...},
{"exittoshell", "",  CmdExitToShell, "Quit current app via INIT", ...},
{"shutdown",    "",  CmdShutdown,    "Shut down guest via INIT", ...},
{"fail",        "",  CmdFail,        "Abort with error", ...},
```

Plus: `break` extended to accept `break text "pat"`, `break screen
"ref.png" [pct]`, `break off` as new condition types in the existing
`CmdBreak` handler.

`info keys` added as a subcommand of the existing `CmdInfo`.
`clear keys` added to the main table.

### 4.7 Trap tracer — Text type hook

**File:** [src/cpu/trap_tracer.cpp](src/cpu/trap_tracer.cpp)

In `formatParam()`, when processing a param with `isText` flag:

```cpp
if (def.isText) {
    std::string utf8 = UTF8FromMacRoman(pstr);
    ScriptCaptureText(utf8, trapDef.name);
}
```

**Cost when inactive:** One boolean check per Text-typed param.

### 4.8 traps.def — Text annotation

**File:** [data/debug/traps.def](data/debug/traps.def)

`Text` is a recognized type — behaves as `Str255` for display, but
sets the `isText` semantic flag.  Changed traps:

```
A884 DrawString toolbox
  in  s:Text

A98B ParamText toolbox
  in  param0:Text  param1:Text  param2:Text  param3:Text
```

More added incrementally: `SetIText` (A98F), `TESetText` (A9CF).

### 4.9 Key injection

**Prerequisite:** [EVENTS.md](EVENTS.md) — the event queue refactor.

After the refactor, the event queue is time-sorted: each event
carries a `fireCycle` timestamp and is delivered when
`g_instructionCount >= fireCycle`.

Key injection is trivial: `type` and `key` compute timestamps and
push events directly into the unified queue.  No scheduler, no ICT
task, no separate pending list.

```cpp
void CmdType(Debugger &dbg, const std::vector<Token> &args)
{
    std::string text = args[0].str;  // UTF-8
    uint64_t t = g_instructionCount;

    for (uint8_t ch : MacRomanFromUTF8(text)) {
        auto [keycode, needShift] = CharToMacKey(ch);
        if (needShift)
            EventQ_Push({t, EvtQElKind::Key, {MKC_Shift, true}});
        EventQ_Push({t,            EvtQElKind::Key, {keycode, true}});
        EventQ_Push({t + 80000,    EvtQElKind::Key, {keycode, false}});
        if (needShift)
            EventQ_Push({t + 80000, EvtQElKind::Key, {MKC_Shift, false}});
        t += 160000;  // inter-key gap
    }
}
```

`key cmd-K` works the same way — push modifier down, key down, key
up, modifier up with appropriate timestamps.

`clear keys` → `EventQ_ClearFutureKeys()`.
`info keys` → `EventQ_PendingKeys()`.

**Character-to-keycode mapping:** `CharToMacKey()` in
`script_keymap.cpp` — a `constexpr` array mapping MacRoman byte →
(Mac virtual keycode, needShift).  Covers ASCII printable range.
UTF-8 input is converted to MacRoman first via `MacRomanFromUTF8()`.

### 4.10 Framebuffer access

**File:** [src/platform/emulator_shell.h](src/platform/emulator_shell.h) —
`getFramebuffer()` returns `const uint8_t*` (ARGB8888).

Screen BPs and screenshots use this buffer.  PNG saved with
`stb_image_write.h` (already vendored).  Reference PNGs loaded with
`stb_image.h` (new, added to `libs/stb/`).

### 4.11 Extension interface — host→guest commands

**File:** [src/core/extn_extfs.cpp](src/core/extn_extfs.cpp)

New command code `kExtFSGuestCmd` (0x220).  Guest INIT polls it in
the jGNEFilter (alongside `kExtFSPollMount`).  Return values:
- 0 = nothing pending
- 1 = launch (path in registers)
- 2 = ExitToShell
- 3 = Shutdown

One additional MMIO read per poll cycle (every 60 ticks).

---

## 5. Key Algorithms

### 5.1 Script suspension

```
function executeCommands(lines, startAt):
    for i = startAt to lines.size():
        parse and dispatch lines[i]
        if state != Stopped:
            pendingScript = {lines, i+1}
            return          // suspended
    pendingScript = empty   // script completed
```

### 5.2 Script resumption

```
function onDebuggerStop(firedBP):
    if pendingScript is empty:
        commandLoop()       // interactive prompt
        return

    if firedBP.scriptOwned:
        // The wait's condition was met — resume script
        delete firedBP
        executeCommands(pendingScript.lines, pendingScript.nextLine)
    else:
        // User breakpoint — give prompt, keep script pending
        commandLoop()
        // After user types "continue", CPU resumes.
        // Script stays pending — next stop will re-enter this logic.
```

### 5.3 Text breakpoint matching

```
function ScriptCaptureText(utf8, trapName):
    if showtext: print "[text] trapName: \"utf8\""

    for each bp in textBreakpoints:
        if !bp.enabled: continue
        if bp.timeoutAt and g_instructionCount >= bp.timeoutAt:
            fireTimeout(bp)
            return
        if utf8.contains(bp.textPattern):
            if bp.ignoreCount > 0: bp.ignoreCount--; continue
            if bp.condition and !eval(bp.condition): continue
            fireBreakpoint(bp)
            return
```

### 5.4 Screen breakpoint matching

```
function CheckScreenBreakpoints():       // called once per tick
    for each bp in screenBreakpoints:
        if !bp.enabled: continue
        if bp.screenMatcher.matches(framebuffer):
            if bp.ignoreCount > 0: bp.ignoreCount--; continue
            fireBreakpoint(bp)
            return
```

Pixel comparison: count identical ARGB pixels (ignoring alpha),
fire if `count / total * 100 >= threshold`.

### 5.5 Key injection

No scheduler.  `type` and `key` push timed events directly into the
event queue (see [EVENTS.md](EVENTS.md)):

```
function CmdType(text):
    t = g_instructionCount
    for each char in MacRomanFromUTF8(text):
        (keycode, shift) = CharToMacKey(char)
        if shift: EventQ_Push(t, Key, Shift, down)
        EventQ_Push(t,         Key, keycode, down)
        EventQ_Push(t + 80000, Key, keycode, up)
        if shift: EventQ_Push(t + 80000, Key, Shift, up)
        t += 160000
```

### 5.6 Character-to-keycode mapping

`CharToMacKey()` — `constexpr` array mapping MacRoman byte → (Mac
virtual key code, needShift).  Covers ASCII printable range.
UTF-8 input is converted to MacRoman first via `MacRomanFromUTF8()`.

### 5.7 Breakpoint fire + timeout

```
function fireBreakpoint(bp):
    stop()
    execute bp.commands
    if bp.temporary: delete bp

function fireTimeout(bp):
    saveFailScreenshot(bp.lineNumber)
    reportError("timeout waiting for <condition>")
    delete bp
    pendingScript = empty
    if headless: exit(1)
    else: commandLoop()
```

Timeout is checked inline during condition evaluation — no separate
breakpoint, no linked-pair deletion.

---

## 6. Reused Infrastructure

| Existing component | Used by | How |
|---|---|---|
| `Breakpoint` struct + dispatch | Text/screen/off BPs | Extended with new `Kind` variants + `timeoutAt` + `scriptOwned` |
| `instructionHook()` / `trapHook()` | Script resume | Stop point triggers resume logic |
| `executeCommands()` | Script suspend | Moves remaining lines on state change |
| Event queue ([EVENTS.md](EVENTS.md)) | Key injection | Push timed events directly |
| `MacRomanFromUTF8()` / `UTF8FromMacRoman()` | Text conversion | Script↔guest string encoding |
| `stb_image_write.h` | Screenshot | PNG encoding (already vendored) |
| `getFramebuffer()` | Screen BP / screenshot | ARGB8888 pixel access |
| `g_screenWidth`, `g_screenHeight` | Screen compare | Dimensions |
| `g_instructionCount` | Timeout | Instruction-count deadlines |
| `TrapTracer::formatParam()` | Text capture | Hook point for Text params |
| `TypeRegistry` + `traps.def` | Text type flag | Define which params trigger capture |
| Extension register block | Guest commands | MMIO RPC at extnBlockBase+$20 |
| VIA2 write path | Power-off BP | Hook on soft-power bit transition |

---

## 7. Build Integration

### CMakeLists.txt additions

```cmake
# ── Scripting (in debugger section) ───────────────────
src/debugger/cmd_script.cpp
src/debugger/script_suspend.cpp
src/debugger/bp_text.cpp
src/debugger/bp_screen.cpp
src/debugger/script_keymap.cpp
```

Added to the `MINIVMAC_SOURCES` list in the debugger section.

### New vendored library

```
libs/stb/stb_image.h
```

Single-header PNG loader from stb.  Added alongside `stb_image_write.h`.

---

## 8. Dependency Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                      src/debugger/                           │
│                                                             │
│  debugger.cpp ─── Breakpoint table (extended)               │
│       │           script_suspend.h (PendingScript)          │
│       │                                                     │
│  cmd_script.cpp ── wait, type, key, screenshot, etc.        │
│       │                                                     │
│  bp_text.cpp ──── ScriptCaptureText()                       │
│       │                                                     │
│  bp_screen.cpp ── ScreenMatcher, CheckScreenBreakpoints()   │
│       │                                                     │
│  script_keymap.cpp ─ CharToMacKey table                     │
└────────────┬────────────────┬────────────────┬──────────────┘
             │                │                │
             ▼                ▼                ▼
   ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
   │ TrapTracer   │  │ EmulatorShell│  │ EventQueue   │
   │ (text hook)  │  │ (framebuf)   │  │ (timed push) │
   └──────────────┘  └──────────────┘  └──────────────┘
```

No new module — scripting is part of the debugger.  Dependencies
flow downward into existing infrastructure.  No circular deps.

---

## 9. Testing

### Unit tests

| Test | What it verifies |
|---|---|
| `test_script_keymap.cpp` | CharToMacKey table, modifier parsing |
| `test_screen_match.cpp` | Pixel comparison at various thresholds |
| `test_script_suspend.cpp` | Suspend/resume line tracking |

Tests live in `test/debugger/` using doctest.

### Integration tests

`.dbg` scripts in `test/scripts/` exercised by the headless backend:

```sh
maxivmac --headless --mac "Mac Plus" --dbg-script=test/scripts/boot_wait.dbg
```

### Golden tests

`break screen` reference PNGs double as golden-test fixtures for CI.

---

## 10. Failure Handling

When a timeout fires (i.e. `g_instructionCount >= bp.timeoutAt`
during condition evaluation):

1. Save screenshot to `<script-dir>/fail-<lineNum>.png`
2. Print: `"wait: timeout after N cycles waiting for <condition>"`
3. Delete the breakpoint
4. Clear the pending script
5. If headless → `exit(1)`; if interactive → `commandLoop()`

---

## 11. traps.def Type System Extension

`Text` is a semantic annotation on `Str255`:

```cpp
// In TypeRegistry (type_registry.cpp), when parsing param types:
if (typeName == "Text") {
    field.baseType = BaseType::Str255;
    field.isText = true;
}
```

Displays identically to `Str255` in trace output.  The `isText` flag
triggers `ScriptCaptureText()` in the tracer.

---

## 12. Guest INIT Modifications

**File:** [macsrc/shareddrive/init.c](macsrc/shareddrive/init.c)

The jGNEFilter gains a second poll after `kExtFSPollMount`:

```c
reg_command(g->regBase, kExtFSGuestCmd);  // 0x220
{
    unsigned short cmd = reg_result(g->regBase);
    switch (cmd) {
        case 1: {  // Launch
            // Read path from transfer buffer
            // Call _Launch
            break;
        }
        case 2:  // ExitToShell
            ExitToShell();
            break;
        case 3:  // Shutdown
            ShutDown();
            break;
    }
}
```

Path transfer: host writes MacRoman bytes into a guest-memory buffer
(address communicated to host via `kExtFSGuestVars` at INIT install).

**Cost:** One MMIO read per poll cycle (every 60 ticks ≈ once/second).
