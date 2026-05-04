# Scripting

New debugger commands that let you drive the guest Mac — type text,
press keys, wait for things to appear on screen, take screenshots,
launch apps, and shut down.  These compose freely with existing
debugger commands (breakpoints, tracing, memory inspection), so you
can instrument a scripted run.

Commands that inject guest-side actions (`launch`, `exittoshell`,
`shutdown`) require the SharedDrive INIT to be loaded in the guest.

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

## New commands

All existing debugger commands (`break`, `trace`, `step`, `x`, etc.)
continue to work.  The following are new.

### timeout

```
timeout <cycles>
```

Set the default cycle budget for `gotext` and `goscreen`.  Applies to
all subsequent commands that don't specify their own limit.

Default: 40 000 000 cycles (~5s at 8 MHz).

On an 8 MHz 68000, 8 000 000 cycles ≈ 1 second.

Later: allow a suffix — `timeout 5s`, `timeout 500ms` — as syntactic
sugar for cycle counts.

### gotext

```
gotext "text"
gotext "text" <cycles>
```

Resume the guest and run until a trap fires with a text parameter
matching the given substring.  If the cycle budget is exhausted
first, the command fails (screenshot + error).

Script strings are UTF-8, converted to MacRoman for matching.

`gotext` is a `go` variant — it resumes the CPU, just like `continue`.
When it returns, the debugger is stopped and you can inspect state,
set breakpoints, enable tracing, etc. before the next command.

#### Which traps are watched?

No hardcoded list.  A trap parameter is captured for `gotext` when
its type in `traps.def` is marked `Text`:

```
A884 DrawString toolbox
  in  s:Text

A98B ParamText toolbox
  in  param0:Text  param1:Text  param2:Text  param3:Text
```

`Text` is a display type like `Str255` — a pointer to a Pascal string
in guest memory.  The difference is semantic: the trap tracer captures
`Text` values into the text-match buffer that `gotext` checks against.

This piggybacks on the existing `traps.def` → `TypeRegistry` →
`TrapTracer` pipeline.  Adding text capture to a new trap is a
one-line edit in `traps.def` — change the param type to `Text`.

For traps that pass text as `Ptr` + length (e.g. `TextBox`), a
future `TextBuf` type could handle the pair, but `Text` (Pascal
string pointer) covers the common cases.

### goscreen

```
goscreen "reference.png"
goscreen "reference.png" <cycles>
goscreen "reference.png" <cycles> <percent>
```

Resume the guest and run until the framebuffer matches a reference PNG.
Polled periodically — the screen doesn't have to match instantly, just
before the cycle budget runs out.

Match is by percentage of identical pixels.  Default threshold is
99.85% — just enough to ignore a 16×16 cursor anywhere on a 512×342
screen.  Override with a third argument: `goscreen "desktop.png"
40000000 95` requires 95% of pixels to match.

The reference PNG must be at the same bit depth as the emulated
screen — comparison is raw pixel values, not color-matched.

Future: use the PNG alpha channel as a mask — only non-transparent
pixels are compared.  This lets you match a region of the screen
(e.g. a dialog box) regardless of what's behind it.

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
  safely" — use `gotext` to detect it.
- Mac II: powers off — use `gooff` to wait for it.

### gooff

```
gooff
gooff <cycles>
```

Resume the guest and run until the machine powers off (Mac II and
later).  On machines without power-off hardware, this will time out.

### fail (future)

```
fail "something went wrong"
```

Terminate the emulator with exit code 1 and print the message to
stderr.  Useful in `commands` blocks on breakpoints, or in `ontext`
handlers to catch error conditions.  Not needed until we have
automated test scripts.

## Composability with existing commands

Because these are debugger commands, you can freely interleave them
with tracing, breakpoints, and memory inspection:

```
# Trace every instruction between two UI events
gotext "Open"
trace insns on
key return
gotext "MyFile.c"
trace insns off

# Break on a trap during a compile
gotext "SharedDrive.c"
break GetResource
key cmd-K
gotext "Build succeeded" 50000000
delete 1
```

## Reactive handlers (future)

```
ontext "Replace existing" { key return }
ontext "Error" { fail "compile error" }
onscreen "crash.png" { screenshot "crash"; fail "guest crashed" }
```

`ontext` fires whenever the guest displays text matching the pattern,
regardless of where the script is.  `onscreen` fires when the
framebuffer matches a reference PNG.  Useful for dismissing unexpected
dialogs or catching error conditions.

## Example: build an INIT with THINK C

```
# build.dbg — Build SharedDrive INIT with THINK C
# Boot disk: thinkc-build.hfs (THINK C as startup app)
# SharedDrive: shared/ (contains project + source)

timeout 40000000                   # ~5s at 8MHz

# THINK C launches, shows Open dialog
gotext "Open"
key tab                            # switch to SharedDrive volume
type "Shared Drive"
key return

# Project opens, source window visible
gotext "SharedDrive.c"

# Build
key cmd-K
gotext "Save code resource" 400000000   # long compile — ~50s budget
key return

# Back to editor = build complete
gotext "SharedDrive.c"
shutdown
```
