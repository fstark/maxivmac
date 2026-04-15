# Debugger — Bugs & Friction (SharedDrive Session, April 2025)

Issues encountered by an AI agent during multi-day SharedDrive INIT
debugging (root-cause analysis of a Pack7/ROMMapHndl corruption bug).
Ranked by actual time wasted.

## Summary

| ID | Type | Title | Impact |
|----|------|-------|--------|
| BUG-1 | Bug | Client has no connect retry / server-ready signal | **Critical** — every invocation needed `sleep 2-3` |
| BUG-2 | Bug | Stale sockets from killed processes break FindSocket | **High** — `rm -f /tmp/maxivmac-dbg-*.sock` in every script |
| BUG-3 | Bug | Watchpoint MATC interaction unclear | **Medium** — worked at $0AD4, earlier report says broken at $7E9180 |
| FEAT-1 | Want | `break #-N` relative instruction breakpoints | High — manual arithmetic on 8-digit insn counts is error-prone |
| FEAT-2 | Want | Trace filtering / streaming to file | High — 1.8M-line traces overwhelm stdout |
| FEAT-3 | Want | `disas` command with address range | Medium — `x/Ni` requires guessing instruction count |
| FEAT-4 | Want | `info globals` / low-memory map awareness | Medium — constant `x/1l $0B06` to check globals |
| FEAT-5 | Want | Script-level timeout for `run`/`continue` | Medium — scripts hang forever if no breakpoint fires |

---

## Bugs

### BUG-1: No server-ready signaling; client has no connect retry

**Time wasted**: Hours total across the session (every single emulator
restart).

**Symptom**: The debug client fails immediately if the server socket
doesn't exist yet. Every scripted workflow required a blind `sleep 2`
(sometimes `sleep 3`) between starting the emulator and sending the
first debug command. On a slow start or under load, the sleep was too
short and the command failed. On a fast start, 2 seconds was wasted
for nothing.

**Root cause**: `ConnectToSocket()` (dbg_client.cpp L44–62) calls
`connect()` once with no retry. `FindSocket()` (L20–41) uses `glob()`
to discover the socket file; if the file doesn't exist yet, it prints
"No debug server found" and returns empty.

The server creates the listen socket in `CreateListenSocket()`
(dbg_io.cpp L168–210) but prints no machine-readable ready indicator.

**Suggested fix**: Add `--wait` (or make it the default) to the client:
poll for the socket file with exponential backoff (10ms, 20ms, …
up to 2s total), then connect with retry. Print a clear error if the
timeout expires. This eliminates every `sleep N` in the workflow.

Alternatively, the server could write a sentinel file or print a
known string that a wrapper script can `grep` for.

---

### BUG-2: Stale sockets from killed processes confuse FindSocket

**Time wasted**: Moderate — every script had `rm -f /tmp/maxivmac-dbg-*.sock`.

**Symptom**: After killing the emulator with SIGKILL (exit 137), the
socket file remains in `/tmp/`. On the next launch, if the PID changes,
a **new** socket file is created alongside the stale one. `FindSocket()`
finds 2 matches, prints "Multiple debug servers found", and bails.

**Root cause**: `atexit(CleanupSocket)` (dbg_io.cpp L206) doesn't run
on SIGKILL. `CreateListenSocket()` does `unlink(path)` before `bind()`,
so the **new** server's socket is fine — but the **old** stale socket
from the killed process remains. With PID-based socket names, each run
creates a different file.

**Suggested fix**: Either:
1. Register a `SIGTERM`/`SIGINT` handler that calls `unlink()`, or
2. In `FindSocket()`, when multiple sockets exist, probe each with
   `connect()` and discard stale ones that refuse connection, or
3. Use a fixed socket name (no PID) so `CreateListenSocket()`'s
   `unlink()` always cleans up the previous one.

Option 3 is simplest and fine for single-instance use.

---

### BUG-3: Watchpoint MATC interaction — uncertain reliability

**Severity**: Needs investigation.

**Symptom**: In an earlier part of the session, a watchpoint on address
$7E9180 reportedly never fired despite the watched address being
written to (confirmed by manual memory reads before/after). Later in
the session, a watchpoint on $0AD4 (AppPacks[7]) worked correctly —
it caught 5 writes with correct values and the final NULL write.

**Analysis**: `RecalcWatchpointFlag()` (debugger.cpp L307–314) sets
`g_watchpointActive` and calls `m68k_InvalidateMATC()`. The `_ext()`
slow-path functions (m68k.cpp L8324–8519) check `g_watchpointActive`
to prevent MATC re-population, and call `memoryHook()` for every access.

The design looks correct: after MATC invalidation, the fast path
(m68k.cpp L807–862) should always miss (`(addr & 0) == 0xFFFFFFFF`
is always false), forcing every access through `_ext()`.

**Possible explanations for the $7E9180 failure**:
- A write via `put_long()` that is composed of two `put_word()` calls,
  where the MATC invalidation timing left a window, or
- The watchpoint was on a byte/word boundary and the write crossed it
  in a way the range check in `memoryHook()` didn't catch, or
- User error (wrong address or mode).

**Recommendation**: Add a self-test: set a watchpoint, write to the
address via `put_vm_long()`, confirm the watchpoint fires. If it
doesn't, the MATC path has a real bug.

---

## Feature Requests

### FEAT-1: `break #-N` relative instruction breakpoints

**Time wasted**: High — every "go back and look earlier" iteration
required manually computing large instruction-count arithmetic.

**Context**: Runs are deterministic (identical disk image + shared/
contents = identical instruction sequence). We used `break #N`
extensively. The workflow was:
1. Run to crash at insn #28117145
2. Want to examine state 50K instructions earlier
3. Manually compute: 28117145 - 50000 = 28067145
4. `break #28067145` → `run` (restarts emulator)

With 8-digit numbers, arithmetic errors are likely. A `break #-50000`
syntax would compute the offset from the current (or last-stopped)
instruction count automatically.

**Implementation**: Trivial — in `CmdBreak()` (cmd_break.cpp L84–90),
if the token after `#` is negative, compute
`g_instructionCount + N` (where N is negative). Store the last stop
instruction count so it persists across `run` restarts.

---

### FEAT-2: Trace filtering / streaming to file

**Time wasted**: High — processing 1.8M-line traces was slow and
required external tools (`grep`, `head`, `tail`, `wc -l`).

**Current**: `trace traps on` dumps every trap call to the debug
client's stdout. For a full Finder boot, this produces ~1.8 million
lines. Even with `--script` redirecting to a file, the volume is
unwieldy.

**Wanted**:
```
(dbg) trace traps on filter GetResource,GetCatInfo
(dbg) trace traps on file /tmp/trace.txt
(dbg) trace traps on depth 1         # top-level only, skip nested
```

The `filter` option would restrict output to named traps. The `file`
option would write directly to a file from the server side (avoiding
socket throughput limits). The `depth` option would skip nested trap
calls.

During the session, the most useful traces were the filtered ones
(e.g., only GetResource calls during InitAllPacks), but filtering
had to be done post-hoc with grep.

---

### FEAT-3: `disas` command with address range

**Context**: Used `x/Ni addr` extensively to examine ROM trap dispatch
code. Determining the right instruction count `N` required guessing,
overshooting, and re-running.

**Wanted**:
```
(dbg) disas $40826456 $408264F2      # disassemble range
(dbg) disas $40826456 +40            # disassemble 40 bytes
```

Sugar over `x/Ni` but auto-calculates the count from the byte range.

---

### FEAT-4: `info globals` / low-memory map

**Context**: Spent significant time manually reading low-memory globals
by address:
```
(dbg) x/1l $0A50     # TopMapHndl
(dbg) x/1l $0A54     # SysMapHndl
(dbg) x/1l $0B06     # ROMMapHndl
(dbg) x/1l $0AD4     # AppPacks[7]
```

It would help to have a command that knows the Mac low-memory map:
```
(dbg) info globals TopMapHndl ROMMapHndl AppPacks
(dbg) info globals resource    # show all resource-related globals
```

This could be a simple table lookup — the classic Mac low-memory map
is well-documented and static. Even just accepting symbolic names in
`x/` and `print` would help: `print ROMMapHndl` instead of `x/1l $0B06`.

---

### FEAT-5: Script-level timeout for blocking commands

**Context**: When a `run` or `continue` command in a `--script` doesn't
hit a breakpoint, the debug client blocks forever. Had to kill the
emulator process externally.

**Wanted**: A per-command timeout in scripts:
```
# debug_finder.dbg
break #28000000
timeout 30
run
# if no breakpoint within 30 seconds, stop and report
trace traps on
step 200
```

Or a global `--timeout=N` flag for the client. When the timeout
fires, the client should send an interrupt/break signal to the server
(equivalent to Ctrl+C in interactive mode).

---

## Already Working (verified during session)

These features from the previous version of this document were confirmed
working and used successfully:

- **`;`-separated commands** in one-shot mode (dbg_client.cpp L185–191)
- **`--script=file.dbg`** reads commands from a file (L147–172)
- **`finish` for trap calls** — correctly waits for RTS at matching SP
- **`ignore <id> <count>`** — skip N breakpoint hits (cmd_break.cpp L339–363)
- **`.b`/`.w`/`.l` size modifiers** on memory dereferences in expressions
  (expr.cpp L127–145)
- **`break #N`** instruction-count breakpoints — fully reproducible with
  deterministic boot
- **`commands <id>`** auto-execute on breakpoint hit
- **Trap tracing** with nested call depth and return values
