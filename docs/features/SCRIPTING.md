# Scripting

New debugger commands that let you drive the guest Mac — type text,
press keys, wait for things to appear on screen, take screenshots,
launch apps, and shut down.  These compose freely with existing
debugger commands (breakpoints, tracing, memory inspection), so you
can instrument a scripted run.

Commands that inject guest-side actions (`launch`, `exittoshell`,
`shutdown`) require the SharedDrive INIT to be loaded in the guest.

## Core model

Scripting is built on two existing debugger concepts:

1. **Breakpoints** — extended with new condition types (text match,
   screen match, power-off).
2. **Script suspension** — when a script hits `wait` or `continue`,
   execution suspends; the remaining lines resume automatically when
   the debugger next stops.

`wait` is sugar for: create a temporary breakpoint + continue.  When
the breakpoint fires, the temp BP is deleted and the script resumes
at the next line.  This means `wait` composes with all existing
breakpoint features (conditions, ignore counts, attached commands).

## Invocation

Scripts are regular `.dbg` files, run with existing flags:

```sh
maxivmac -disk boot.hfs --headless --dbg-script=build.dbg          # headless, max speed
maxivmac -disk boot.hfs --debugger --dbg-script=build.dbg          # visible window, drops to prompt on finish
```

`--headless` implies max speed and no display window.  If any command
fails (timeout, explicit `fail`), the emulator exits non-zero, prints
the error to stderr, and saves a screenshot.

Since it's just a `.dbg` file, you can also `source build.dbg` from
the interactive debugger prompt.

## New breakpoint types

The existing `break` command gains new condition types.  All
breakpoint features work with these: `commands`, `ignore`, `disable`,
`enable`, `delete`, conditional expressions.

### break text

```
break text "pattern"
```

Fire when a trap passes text matching the substring.  Text is
captured from traps whose `traps.def` parameter type is `Text`:

```
A884 DrawString toolbox
  in  s:Text

A98B ParamText toolbox
  in  param0:Text  param1:Text  param2:Text  param3:Text
```

`Text` is a semantic annotation on `Str255` — a pointer to a Pascal
string in guest memory.  The trap tracer captures `Text` values and
checks them against active text breakpoints.

This piggybacks on the existing `traps.def` → `TypeRegistry` →
`TrapTracer` pipeline.  Adding text capture to a new trap is a
one-line edit in `traps.def` — change the param type to `Text`.

For traps that pass text as `Ptr` + length (e.g. `TextBox`), a
future `TextBuf` type could handle the pair, but `Text` (Pascal
string pointer) covers the common cases.

Script strings are UTF-8, converted to MacRoman for matching.

### break screen

```
break screen "reference.png"
break screen "reference.png" <percent>
```

Fire when the framebuffer matches a reference PNG.  Checked once per
tick (60 Hz).

Match is by percentage of identical pixels.  Default threshold is
99.85% — just enough to ignore a 16×16 cursor anywhere on a 512×342
screen.  Override with a second argument: `break screen "desktop.png"
95` requires 95% of pixels to match.

The reference PNG must be at the same bit depth as the emulated
screen — comparison is raw pixel values, not color-matched.

Future: use the PNG alpha channel as a mask — only non-transparent
pixels are compared.  This lets you match a region of the screen
(e.g. a dialog box) regardless of what's behind it.

### break off

```
break off
```

Fire when the machine powers off (Mac II and later).  On machines
without power-off hardware, this breakpoint will never fire.

## The `wait` command

```
wait <condition> [timeout]
```

Create a temporary breakpoint with the given condition, then continue
execution.  When the breakpoint fires, it's deleted and the script
resumes at the next line.  If the timeout expires first, the command
fails.

`wait` accepts the same conditions as `break`, plus any existing
breakpoint target:

| Form | What it waits for |
|---|---|
| `wait text "pattern"` | Text match |
| `wait text "pattern" <cycles>` | Text match with explicit timeout |
| `wait screen "ref.png"` | Screen match |
| `wait screen "ref.png" <cycles> <pct>` | Screen match with budget + threshold |
| `wait off` | Power-off |
| `wait $40122E` | PC hits address |
| `wait main` | PC hits symbol |
| `wait trap GetResource` | Trap fires |

**Timeout:** Each `wait` creates a paired instruction-count breakpoint
as a deadline.  Whichever fires first (condition or timeout) wins and
deletes the other.  The timeout defaults to the value set by the
`timeout` command.

**On timeout failure:**
- Save screenshot to `<script-dir>/fail-<line>.png`
- Print error to stderr
- In headless mode: exit non-zero
- In interactive mode: drop to debugger prompt

**Interactive use:** If you type `wait` at the prompt with no pending
script, it behaves like `tbreak <condition>` + `continue` — stops
when the condition fires and shows the prompt.

## Other new commands

### timeout

```
timeout <cycles>
```

Set the default cycle budget for `wait` commands that don't specify
their own limit.

Default: 40 000 000 cycles (~5s at 8 MHz).

On an 8 MHz 68000, 8 000 000 cycles ≈ 1 second.

Later: allow a suffix — `timeout 5s`, `timeout 500ms` — as syntactic
sugar for cycle counts.

### type

```
type "hello world"
```

Inject text as keystrokes.  UTF-8 in the script, converted to MacRoman
key events in the guest.  Each character produces a key-down/key-up
pair.  Does not resume the guest — just enqueues input.

### key

```
key return
key cmd-K
key cmd-shift-S
```

Inject a single key press, optionally with modifiers.  Does not resume
the guest — just enqueues a key-down/key-up pair into the input queue.

**Modifiers:** `cmd`, `shift`, `opt`, `ctrl` — combined with `-`.

**Special keys:** `return`, `enter`, `tab`, `escape`, `delete`, `space`,
`left`, `right`, `up`, `down`, `f1`–`f15`.

#### Key timing

Each key event has built-in timing: ~10ms press (key-down → key-up)
followed by ~20ms pause before the next event.  Times are converted
to cycles.  If the queue is empty, the first event is immediate;
subsequent events are scheduled relative to the previous one.

`type` and `key` both push to the same queue — you can interleave
them freely, and the timing is cumulative.

### info keys

```
info keys
```

Show the pending input queue — future key-down/key-up events and
their scheduled cycle counts.  Useful for debugging when keystrokes
aren't being consumed as expected.

### clear keys

```
clear keys
```

Discard all pending key events from the input queue.

### showtext

```
showtext on
showtext off
showtext
```

Toggle live display of captured text to the console.  When enabled,
every `Text`-typed trap parameter is printed as it fires:

```
[text] DrawString: "Open"
[text] ParamText: "Save changes to \"MyFile\"?"
```

Useful for discovering what strings to match with `wait text` or
`break text`.  With no argument, toggles.

### launch

```
launch "Shared Drive:My App"
```

Launch an application by its Mac path.  The INIT handles this via
the host→guest command channel — the emulator tells the INIT to
call `_Launch` with the given path.

The INIT's jGNE filter runs in the context of the current app, so
`_Launch` is valid — it terminates the current app and starts the
new one.  Under MultiFinder, `_Launch` should spawn a new process
instead of replacing the current one, but this needs testing.

### screenshot

```
screenshot "label"
```

Save the current framebuffer to `<script-dir>/label.png`.

Also saved automatically on timeout/failure (`<script-dir>/fail-<line>.png`).

### exittoshell

```
exittoshell
```

Queue an `_ExitToShell` command to the INIT, terminating the current
application.  Under non-MultiFinder this returns to the Finder (or
whatever launched the app).

### shutdown

```
shutdown
```

Queue a `_ShutDown` command to the INIT — the Mac goes through its
normal shutdown sequence.  This is guest-side shutdown, not emulator
termination.

The script must wait for shutdown to complete:
- Mac Plus/SE: displays "You may now switch off your Macintosh
  safely" — use `wait text` to detect it.
- Mac II: powers off — use `wait off`.

### fail

```
fail "something went wrong"
```

Terminate the emulator with exit code 1 and print the message to
stderr.  Useful in `commands` blocks on breakpoints to catch error
conditions early.

## Script suspension and resumption

When a script (`source` or `--dbg-script`) encounters a command that
resumes the CPU (`wait`, `continue`, `run`), execution **suspends**:
the remaining lines are saved.  When the debugger next stops (any
breakpoint, timeout, or manual interrupt), the script **resumes**
from the next line.

This means scripts are linear sequences of commands interspersed with
wait points.  No nesting, no recursion, no re-entrancy.

**What happens at a user breakpoint during a `wait`:**

If you have `break GetResource` active and a `wait text "Open"` is
running, the address breakpoint fires first.  The script is still
suspended.  You get the interactive prompt.  When you type `continue`,
the CPU resumes and the `wait text` timeout keeps ticking.  When the
text breakpoint eventually fires (or times out), the script resumes.

This means user breakpoints compose: they interrupt execution, you
inspect, you continue, and the script picks up where it left off.

## Composability with existing commands

Because `break text` and `break screen` are regular breakpoints, all
existing features work:

```
# Persistent text breakpoint with auto-commands (≈ "ontext")
break text "Replace existing"
  commands
    key return
  end

# Catch errors — persistent, never deleted
break text "Error"
  commands
    screenshot "error"
    fail "compile error"
  end

# Trace every instruction between two UI events
wait text "Open"
trace insns on
key return
wait text "MyFile.c"
trace insns off

# Break on a trap during a compile
wait text "SharedDrive.c"
break GetResource
key cmd-K
wait text "Build succeeded" 400000000
delete 1
```

Note: persistent breakpoints with `commands` that include `key` or
`continue` give you reactive handlers — the old `ontext` concept
falls out for free.

## Example: build an INIT with THINK C

```
# build.dbg — Build SharedDrive INIT with THINK C
# Boot disk: thinkc-build.hfs (THINK C as startup app)
# SharedDrive: shared/ (contains project + source)

timeout 40000000                   # ~5s at 8MHz

# Dismiss unexpected "Replace?" dialogs automatically
break text "Replace existing"
  commands
    key return
    continue
  end

# THINK C launches, shows Open dialog
wait text "Open"
key tab                            # switch to SharedDrive volume
type "Shared Drive"
key return

# Project opens, source window visible
wait text "SharedDrive.c"

# Build
key cmd-K
wait text "Save code resource" 400000000   # long compile — ~50s budget
key return

# Back to editor = build complete
wait text "SharedDrive.c"
shutdown
```
