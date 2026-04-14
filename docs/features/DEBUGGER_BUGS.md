# Debugger — Bugs & Feature Requests

Collected during the SharedDrive debugging session (April 2026).

## Summary

| ID | Title | Verdict | Effort |
|----|-------|---------|--------|
| BUG-1 | Watchpoints bypass MATC fast path | **IMPLEMENT** — invalidate MATC for watched ranges | Medium |
| BUG-2 | x/Ni garbled at trap PC | **INVALID** — correct behavior, cosmetic confusion | None |
| FEAT-1 | Multi-command one-shot mode | **IMPLEMENT** — `;`-split in client + `--script` | Low |
| FEAT-2 | `disas` command | **IMPLEMENT** — sugar over existing Disassemble() | Low |
| FEAT-3 | Guest log in debugger | **IMPLEMENT** — infra exists, just add command | Very low |
| FEAT-4 | `break #-N` relative insn break | **IMPLEMENT** — simple arithmetic on g_instructionCount | Low |
| FEAT-5 | Condition memory deref sizes | **IMPLEMENT** — add `.b`/`.w`/`.l` suffixes to expr parser | Moderate |
| FEAT-6 | `finish` for trap calls | **ALREADY WORKS** — just needs documentation | None |
| FEAT-7 | `ignore` / hit counts | **IMPLEMENT** — add hitCount/ignoreCount to Breakpoint | Low-medium |
| FEAT-8 | Print with size modifiers | **IMPLEMENT** — same expr parser change as FEAT-5 | (shared with FEAT-5) |

---

## Bugs

### BUG-1: Watchpoints never fire (MATC cache bypass)

**Verdict**: IMPLEMENT (Option 1) — Confirmed real bug. The fast path in
`get_byte()`/`put_byte()`/`get_word()`/`put_word()` (m68k.cpp L807–862)
returns directly from the MATC cache without calling `memoryHook()`.
Only the `_ext()` slow path checks watchpoints. Option 1 (invalidate MATC
lines for watched ranges, prevent re-population in `_ext()`) is cleanest.

**Severity**: High — watchpoints are effectively broken for normal RAM.

**Symptom**: `watch 0x7E9180 2` was set. The address was written to
(confirmed by subsequent reads showing changed values) but the
watchpoint never fired.

**Root Cause**: The MATC fast path in `get_byte()` / `put_byte()` /
`get_word()` / `put_word()` (m68k.cpp ~L807–861) returns directly
from the cache without calling the `_ext()` functions. Only the
`_ext()` slow path calls `Debugger::instance()->memoryHook()`.

Since stack and normal RAM almost always hit the MATC cache, watchpoints
on those addresses never trigger.

**Suggested Fix**: When any watchpoint is active (`g_debuggerActive &&
!watchpoints.empty()`), either:
1. Invalidate the MATC entries that cover watched address ranges, forcing
   those accesses through the `_ext()` path, or
2. Add watchpoint range checks to the fast path (more invasive, but
   avoids MATC thrashing when many watchpoints are set).

Option 1 is simpler: on watchpoint add/remove, call a function that
clears the relevant MATC line so the next access goes through `_ext()`.
The `_ext()` function should also avoid re-populating the MATC line
for watched ranges.

---

### BUG-2: `x/Ni` disassembly address calculation for multi-word instructions

**Verdict**: INVALID — The disassembler is working correctly. When stopped
at an A-line trap, the PC *is* at the trap word, and `DisasmALine()`
handles it. The "garbled" appearance is the trap word itself being
displayed, which is the correct behavior. This is purely cosmetic/UX
confusion, not a bug. No action needed.

**Severity**: Low.

---

## Feature Requests

### FEAT-1: Multi-command one-shot mode

**Verdict**: IMPLEMENT — Confirmed one-command-per-connection in
`DebugClientMain()` (dbg_client.cpp L85–127). Simplest approach: parse
`;`-separated commands in the client, loop send/recv for each. Also add
`--script=file.dbg` for reading commands from a file. Low complexity.

**Current**: Each `maxivmac debug "cmd"` invocation opens a new socket
connection, sends one command, reads one response, and disconnects.

**Wanted**: Support sending multiple commands separated by `;` or
newlines in a single invocation:
```
maxivmac debug "break GetResource; commands 1; print d0; continue; end; run"
```

This would dramatically speed up automated debugging from scripts and
AI agents, which currently need one round-trip per command.

**Alternative**: A `--script=file.dbg` flag that reads commands from a
file.

---

### FEAT-2: `disas` command (standalone disassembly)

**Verdict**: IMPLEMENT — Trivial sugar. Add a `CmdDisas()` handler that
parses range syntax (`disas $start $end` or `disas $start +len`),
computes the byte range, then calls the existing `Disassemble()` loop.
No new disassembly logic needed.

**Current**: Disassembly is only available via `x/Ni addr`.

**Wanted**: A `disas` / `disassemble` command with range or function
syntax:
```
(dbg) disas $7C3A60 $7C3AC0
(dbg) disas $7C3A60 +40
```

Mostly sugar over `x/Ni`, but auto-calculates count from address range
and avoids the cryptic format string.

---

### FEAT-3: Access guest console log from debugger

**Verdict**: IMPLEMENT — The infrastructure is fully in place.
`guestConsoleAppend()` (extn_clip.cpp L34–53) already stores up to 2048
lines in `s_consoleLines` deque, exposed via `extnDbgConsoleLines()`.
Just needs a `CmdLog()` handler registered in the command table. Very
low effort.

**Current**: `[GUEST]` log lines go to stderr only. When using
`--debugserver` with stderr redirected, the agent must `tail` the log
file in a separate terminal.

**Wanted**: A debugger command to read recent guest log lines:
```
(dbg) log              # show last 20 lines
(dbg) log 50           # show last 50 lines
(dbg) log grep Desktop # show lines matching "Desktop"
```

The infrastructure already exists — `guestConsoleAppend()` stores lines
in a `std::deque`. Just expose it through a debugger command.

---

### FEAT-4: Deterministic instruction-number breakpoints for INIT debugging

**Verdict**: IMPLEMENT (simple arithmetic only) — `break #N` already
works (cmd_break.cpp L84–90, fires at debugger.cpp L659 when
`g_instructionCount >= N`). The `break #-N` syntax is just arithmetic:
compute `g_instructionCount - N` and set a forward breakpoint. No
instruction history or rewind needed — the user restarts manually with
`run`. Implementing the subtraction syntax is trivial. A `rewind`
command that auto-restarts the emulator is out of scope (would require
emulator reset support).

**Note from user**: The INIT loads at startup, so with identical disk
images and `shared/` contents, every run executes the exact same
instruction sequence. This makes instruction-number breakpoints
(`break #N`) extremely powerful — they are fully reproducible.

**Current**: `break #N` works, but the workflow to find the right N
requires either:
- Running to crash, noting the instruction count, then restarting
- Guessing a range for `--log-start` / `--log-count`

**Wanted**: A `break #-N` syntax meaning "break N instructions before
the last stop point":
```
(dbg) run
[GUEST FATAL at insn #31451989]
(dbg) break #-50000      # → break #31401989
(dbg) run                # restart from scratch, stop 50K insns before crash
```

Or simpler: `rewind N` that restarts the emulator and sets a breakpoint
at `current_insn - N`. Since runs are deterministic, this is safe.

---

### FEAT-5: Conditional trap breakpoints with param-block inspection

**Verdict**: PARTIALLY IMPLEMENTED — The expression evaluator (expr.cpp
L95–125) already supports `(a0+22)` dereference syntax, but it always
reads a 32-bit long. The condition `break HFSDispatch if d0 == 9 &&
(a0+22) == $FFFF8300` works today (using the sign-extended 32-bit value).
What's missing is `.w`/`.b` size modifiers. Add size suffix parsing
after the closing `)` — `.b` masks to 8 bits, `.w` masks to 16 bits,
default remains `.l`. Moderate effort.

**Current**: Trap breakpoints can condition on registers (`d0`, `a0`),
but not on memory pointed to by registers.

**Wanted**: Conditions that dereference pointers:
```
(dbg) break HFSDispatch if d0 == 9 && *(a0+22).w == -32000
```

This would let us break only when `_HFSDispatch(GetCatInfo)` is called
with `ioVRefNum == kOurVRefNum`, skipping thousands of irrelevant calls.

Without this, the agent had to manually `c` through dozens of
GetCatInfo calls checking vRefNum by hand.

---

### FEAT-6: `finish` for trap calls

**Verdict**: ALREADY WORKS — Verified in code. `CmdFinish()` (cmd_exec.cpp
L68–74) saves SP and `setFinishing()` sets the finish flag. The check
at debugger.cpp L601–616 waits for `SP >= savedSP` AND an RTS/RTD/RTE
opcode. This works correctly when stopped at an A-line trap — the trap
handler eventually executes RTS at the right stack depth. The `next`
command also explicitly handles A-line traps. No code change needed;
just document this in help text.

**Current**: `finish` watches for RTS/RTD/RTE at the same stack depth.
When stopped at a trap breakpoint (A-line instruction), `finish` should
run until the trap handler returns to the caller.

**Status**: ~~Untested — may already work.~~ Confirmed working.

---

### FEAT-7: Breakpoint hit counts and auto-skip

**Verdict**: IMPLEMENT — Not implemented; no `ignore` command or hit
counter in the breakpoint struct. Add a `hitCount` and `ignoreCount`
field to the `Breakpoint` struct, increment on every match, skip if
`hitCount <= ignoreCount`. Register an `ignore` command handler.
Low-moderate effort. Note: auto-execute `commands` already exists
(cmd_break.cpp L278–312), so a workaround is possible but clunky.

**Current**: Every breakpoint fires on every hit.

**Wanted**: `ignore <id> <count>` — skip the next N hits:
```
(dbg) break HFSDispatch if d0 == 9
Breakpoint 1 on trap HFSDispatch
(dbg) ignore 1 15        # skip first 15 GetCatInfo calls
(dbg) c                   # stops on the 16th
```

Useful when the interesting call is deep in a boot sequence with many
earlier benign calls.

---

### FEAT-8: Expression printing with memory dereference

**Verdict**: PARTIALLY IMPLEMENTED — `print (a0)`, `print (a0+18)`,
`print (a0-4)` all work today (expr.cpp L95–125), but always read
32-bit longs. Same gap as FEAT-5: no `.b`/`.w`/`.l` size modifiers.
Fix in the same place — after the closing `)` in the expression
parser, check for `.b`/`.w`/`.l` suffix and mask/truncate accordingly.
This is the same change needed for FEAT-5, so implement once.

**Current**: `print d0`, `print a0 + 4` work. `print (a0)` dereferences
a long at the address in A0.

**Wanted**: Extend to arbitrary sizes and offsets:
```
(dbg) print (a0 + 18).l    # read long at A0+18 (ioNamePtr)
(dbg) print (a0 + 22).w    # read word at A0+22 (ioVRefNum)
(dbg) print (a0).b         # read byte
```

This would eliminate many `x/` commands during param block inspection.
