# Debugger — Bugs & Friction (SharedDrive Session, April 2025)

Issues encountered by an AI agent during multi-day SharedDrive INIT
debugging (root-cause analysis of a Pack7/ROMMapHndl corruption bug).
Ranked by actual time wasted.

## Summary

| ID | Type | Status | Title | Impact |
|----|------|--------|-------|--------|
| BUG-1 | Bug | Valid | Client has no connect retry | **High** — every invocation needed `sleep 2-3` |
| BUG-2 | Bug | Valid | Stale sockets from killed processes break FindSocket | **High** — `rm -f /tmp/maxivmac-dbg-*.sock` in every script |
| BUG-3 | Bug | Invalid | Watchpoint MATC interaction unclear | Reporter likely had a wrong address/mode; code analysis and subsequent testing confirmed correctness |
| FEAT-1 | Want | Invalid | `break #-N` relative instruction breakpoints | Already implemented — `break #-N` resolves relative to `g_instructionCount` |
| FEAT-2 | Want | Mostly invalid | Trace filtering / streaming to file | Name filtering already works (`trace traps GetResource OpenRF`); only `file` and `depth` options missing |
| FEAT-3 | Want | Invalid | `disas` command with address range | Already implemented — `disas $start $end` and `disas $start +$len` both work |
| FEAT-4 | Want | Invalid | `info globals` / low-memory map awareness | Already implemented — `info globals` with prefix search |
| FEAT-5 | Want | Valid | Script-level timeout for `run`/`continue` | **Medium** — scripts hang forever if no breakpoint fires |

---

## Bugs

### BUG-1: Client has no connect retry

**Status**: Valid

**Symptom**: The debug client fails immediately if the server socket
doesn't exist yet. Every scripted workflow required a blind `sleep 2`
(sometimes `sleep 3`) between starting the emulator and sending the
first debug command.

**Root cause**: `ConnectToSocket()` (dbg_client.cpp) calls `connect()`
once with no retry. `FindSocket()` uses `glob()` to discover the socket
file; if the file doesn't exist yet, it returns empty.

Note: the original report claimed the server prints no ready indicator,
but it does (`"debugserver: listening on %s\n"` on stderr). The real
problem is solely on the client side.

**Planned fix**: Make the client retry by default — poll for the socket
file with exponential backoff (10 ms → 20 ms → … up to ~3 s total),
then `connect()` with retry on each found socket. Print a clear error
if the total timeout expires. A `--no-wait` flag disables the retry
for callers that want immediate failure. This eliminates every
`sleep N` in wrapper scripts.

---

### BUG-2: Stale sockets from killed processes confuse FindSocket

**Status**: Valid

**Symptom**: After killing the emulator with SIGKILL (exit 137), the
socket file remains in `/tmp/`. On the next launch, `FindSocket()`
finds 2 matches and bails.

**Root cause**: `atexit(CleanupSocket)` doesn't run on SIGKILL. With
PID-based socket names, each run creates a different file, leaving
stale sockets behind.

**Planned fix**: On startup, `FindSocket()` probes each discovered
socket with a non-blocking `connect()`. Sockets where `connect()`
returns `ECONNREFUSED` (no listener) are stale — unlink them
automatically and continue. This is simpler and more robust than
signal handlers (which cannot catch SIGKILL anyway) and eliminates the
need for manual `rm -f` cleanup.

---

### BUG-3: Watchpoint MATC interaction — uncertain reliability

**Status**: Invalid (likely user error)

The reporter observed a watchpoint on `$7E9180` not firing, but a later
watchpoint on `$0AD4` worked correctly.

Code analysis confirms the mechanism is sound:
`RecalcWatchpointFlag()` sets `g_watchpointActive` and calls
`m68k_InvalidateMATC()`. After invalidation, every memory access goes
through the `_ext()` slow path and `memoryHook()`. The reporter
themselves listed "user error (wrong address or mode)" as a possible
explanation, and subsequent successful use of watchpoints in the same
session confirms the mechanism works.

No action required.

---

## Feature Requests

### FEAT-1: `break #-N` relative instruction breakpoints

**Status**: Invalid — already implemented

`break #-N` is already supported. The parser in `CmdBreak()` detects
the `-` operator, validates that N ≤ `g_instructionCount`, resolves to
an absolute instruction number, and reports:
`(resolved to instruction #NNNNNN)`.

The reporter was apparently unaware of this feature and was doing
the arithmetic manually.

---

### FEAT-2: Trace filtering / streaming to file

**Status**: Mostly invalid — name filtering already works

`trace traps GetResource OpenRF` already filters output to named traps
via `SymbolsResolve()` and `addTrapFilter()`. This was the reporter's
primary pain point.

Two sub-features remain unimplemented:
- **`depth N`** — suppress nested trap calls beyond depth N
- **Server-side file output** — write trace directly to a file on the
  server, bypassing socket throughput limits

These are nice-to-haves, not currently planned.

---

### FEAT-3: `disas` command with address range

**Status**: Invalid — already implemented

Both `disas $start $end` and `disas $start +$len` are supported
(cmd_memory.cpp). The reporter was using `x/Ni` unnecessarily.

---

### FEAT-4: `info globals` / low-memory map

**Status**: Invalid — already implemented

`info globals` lists low-memory globals with symbolic names; an
optional prefix argument filters results (e.g. `info globals ROM`).
The `SymbolsSearch()` backend provides name, address, and size.

---

### FEAT-5: Script-level timeout for blocking commands

**Status**: Valid

**Symptom**: When a `run` or `continue` command in a `--script` doesn't
hit a breakpoint, the debug client blocks forever. Had to kill the
emulator process externally.

**Planned fix**: Instruction-count budget, not wall-clock timeout.
Add a `budget <N>` script command that sets a maximum number of
emulated instructions for the next blocking command (`run`, `continue`).
When the budget is exhausted, the emulator breaks automatically —
equivalent to an ephemeral `break #(current + N)`.

```
# debug_finder.dbg
break StopAlert
budget 5000000
run
# breaks at StopAlert, OR after 5M instructions with:
#   [budget exhausted after 5000000 instructions]
trace traps on
step 200
```

Why instruction count, not wall-clock seconds:
- **Deterministic**: same budget always covers the same guest work,
  regardless of host speed or load.
- **Composable**: `budget` + `break #-N` lets you bracket any region
  of execution precisely.
- **Simple to implement**: decrement a counter in the main emulation
  loop; when it hits zero, trigger a break. No threads, no timers,
  no signal races.

A global `--budget=N` client flag sets a default for all blocking
commands in the script.

---

## Already Working (verified during session)

These features were confirmed working and used successfully during
the SharedDrive debugging session. Several were re-reported as missing
(FEAT-1, FEAT-2 filtering, FEAT-3, FEAT-4) by a reporter who was
unaware they existed.

- **`;`-separated commands** in one-shot mode
- **`--script=file.dbg`** reads commands from a file
- **`finish` for trap calls** — correctly waits for RTS at matching SP
- **`ignore <id> <count>`** — skip N breakpoint hits
- **`.b`/`.w`/`.l` size modifiers** on memory dereferences in expressions
- **`break #N`** and **`break #-N`** instruction-count breakpoints
- **`commands <id>`** auto-execute on breakpoint hit
- **Trap tracing** with nested call depth and return values
- **`trace traps <name> [name...]`** — filter by trap name
- **`disas $start $end`** and **`disas $start +$len`** — disassembly
- **`info globals [prefix]`** — low-memory global lookup with search
