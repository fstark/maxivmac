# Debugger — High-Level Specification

A command-line debugger for the emulated 68K CPU, inspired by GDB.
Runs inside the emulator process, reading commands from stdin and
printing output to stdout.  Designed for debugging classic Mac programs
and ROM code running inside maxivmac.

## Personas

**Beatrix** — vintage Mac developer who wants to step through her code,
set breakpoints on Toolbox traps, and inspect memory in the emulated
environment.

**Dorothee** — emulator developer who needs to trace I/O, catch
unexpected memory accesses, and compare behavior against the reference
build.

---

## Activation

```
maxivmac --debugger [other options…]
```

When `--debugger` is passed, the emulator starts paused at the first
instruction (reset vector target).  A `(dbg)` prompt appears on stdout.
The emulator does not advance until the user issues `run` or `step`.

Without `--debugger`, the emulator runs normally — zero overhead.

---

## Execution Model

The debugger hooks into the CPU loop at the top of each instruction
dispatch.  When the emulator is **stopped** (breakpoint hit, step
completed, user interrupt), it enters the command loop.  When it is
**running**, the hook checks breakpoints and watchpoints with minimal
cost (hash lookups, not linear scans).

Ctrl-C (SIGINT) pauses a running emulator and drops to the prompt.

---

## Commands

All commands use the shortest unambiguous prefix (e.g. `b` for `break`,
`c` for `continue`).  Arguments in `<>` are required; `[]` optional.

### Execution

| Command | Short | Description |
|---------|-------|-------------|
| `run` | `r` | Start / resume execution |
| `continue` | `c` | Resume after a stop |
| `step [N]` | `s` | Execute N instructions (default 1) |
| `next [N]` | `n` | Step N instructions, stepping *over* BSR/JSR/trap calls |
| `finish` | `fin` | Run until current function or trap handler returns |
| `until <addr>` | `u` | Run until PC reaches addr |
| `stepi [N]` | `si` | Alias for `step` (GDB compat) |

`next` works by recording the current stack depth and running until
the stack returns to that depth at the instruction *after* the call.

`finish` works the same way: run until SP ≥ saved SP and the
instruction is RTS, RTD, or RTE.  This works seamlessly with A-line
trap calls: when stopped at a trap breakpoint, `finish` will run
the entire trap handler and stop when it returns to the caller.

### Breakpoints

| Command | Description |
|---------|-------------|
| `break <location>` | Set breakpoint |
| `break <location> if <cond>` | Conditional breakpoint |
| `break #<N>` | Break at instruction number N |
| `break #-<N>` | Break at current insn count minus N |
| `ignore <id> <count>` | Skip next N hits of breakpoint |
| `delete <id>` | Delete breakpoint/watchpoint by ID |
| `delete` | Delete all |
| `disable <id>` | Disable without deleting |
| `enable <id>` | Re-enable |
| `info break` | List all breakpoints and watchpoints |

A `<location>` is one of:

- **Hex address**: `$4000`, `0x4000`, `4000` — break when PC == addr
- **Trap name**: `GetResource`, `_GetResource`, `$A9A0` — break when
  the trap is dispatched (A-line handler, before the ROM code runs)
- **Low-memory global**: `CurApName`, `ApplZone` — shorthand for the
  global's address (useful with watchpoints)

- **Instruction number**: `#12345` — break when the global instruction
  counter reaches that value (one-shot). Use `info insn` to see the
  current count.
- **Relative instruction number**: `#-50000` — set breakpoint at
  `current_insn_count - 50000`.  Useful for re-running deterministic
  boot sequences to stop just before a known crash point.

Examples:
```
(dbg) break $00401A
Breakpoint 1 at $00401A
(dbg) break GetResource
Breakpoint 2 on trap GetResource ($A9A0)
(dbg) break $4000 if d0 == 0
Breakpoint 3 at $4000 (conditional: d0 == 0)
(dbg) break #50000
Breakpoint 4 at instruction #50000
(dbg) break #-50000
(resolved to instruction #31401989)
Breakpoint 5 at instruction #31401989
(dbg) ignore 2 15
Will ignore next 15 crossings of breakpoint 2.
```

### Watchpoints (memory breakpoints)

| Command | Description |
|---------|-------------|
| `watch <addr> [len]` | Break on write to address (default len=1) |
| `watch <addr> [len] if val == <v>` | Break only when written value matches |
| `rwatch <addr> [len]` | Break on read |
| `awatch <addr> [len]` | Break on read or write |

`<addr>` accepts hex addresses or low-memory global names.

The optional `if val == <v>` condition filters on the written value
so the watchpoint only fires when a specific value is stored.  The
`val` keyword refers to the value being written; standard comparison
operators (`==`, `!=`, `<`, `>`, `<=`, `>=`, `&`) are supported.

```
(dbg) watch CurApRefNum
Watchpoint 4 on write to CurApRefNum ($0900, 2 bytes)
(dbg) awatch $1000 4
Watchpoint 5 on access to $1000 (4 bytes)
(dbg) watch $DEAD if val == $42
Watchpoint 6 on write to $DEAD (1 byte, if val == $42)
(dbg) watch CurApRefNum if val != 0
Watchpoint 7 on write to CurApRefNum ($0900, 2 bytes, if val != $0000)
```

When a watchpoint fires, the debugger prints the old and new values:
```
Watchpoint 4 hit: CurApRefNum ($0900)
  Old value: $0002
  New value: $0003
```

### Registers

| Command | Description |
|---------|-------------|
| `info reg` | Print all registers (D0–D7, A0–A7, PC, SR, USP, ISP) |
| `print <expr>` | Print expression value |
| `set <reg> = <val>` | Modify a register |

`<expr>` can reference registers by name (`d0`–`d7`, `a0`–`a7`, `pc`,
`sr`, `sp`) and low-memory globals by name.  Basic arithmetic is
supported: `a0 + 4`, `(a7)` to dereference, `pc - $10`.

```
(dbg) info reg
D0=00000000  D1=00FF4002  D2=00000000  D3=00000000
D4=00000000  D5=00000000  D6=00000000  D7=00000000
A0=00400000  A1=00000000  A2=00000000  A3=00000000
A4=00000000  A5=00000000  A6=00FFFE00  A7=00FFFE00
PC=00400200  SR=2700 [S--Z---]  USP=00000000  ISP=00FFFE00
(dbg) print d0
$00000000
(dbg) print (a0)
$4E750000
```

### Memory Examination

| Command | Description |
|---------|-------------|
| `x[/FMT] <addr>` | Examine memory at address |

Format: `x/[count][size][format] <addr>`

- **count**: number of units (default 1)
- **size**: `b` byte, `w` word (2), `l` long (4) — default `w`
- **format**: `x` hex (default), `d` decimal, `s` string (Mac Roman),
  `i` disassemble, `t` type-aware struct dump (see below)

```
(dbg) x/8w $400
$000400: 0001 0002 0003 0004 0005 0006 0007 0008
(dbg) x/4i $401A
$00401A: 4E56 0000    LINK    A6,#0
$00401E: 2F00         MOVE.L  D0,-(A7)
$004020: 4EB9 0040102A JSR     $0040102A
$004026: 4E5E         UNLK    A6
(dbg) x/1s CurApName
$000910: "Finder"
(dbg) x/16b $0
$000000: 00 04 00 00 00 00 04 00 00 00 28 00 00 00 04 1A
```

#### Structured memory display (`x/t`)

`x/t <addr> <TypeName> [variant]` formats guest memory as a named
Mac OS structure, using definitions from `assets/types.def`.  The
count and size fields are ignored for `t` format.  For unions, pass
the variant tag to select the arm.

```
(dbg) x/t a0 HFileInfo
  header.qLink:          $00000000
  header.ioResult:       0 noErr
  header.ioNamePtr:      $00FC2000
  header.ioVRefNum:      -32000
  ioFlFndrInfo.fdType:   'TEXT'
  ioFlFndrInfo.fdCreator:'ttxt'
  ...
(dbg) x/t a0 CInfoPBRec file
  (displays HFileInfo arm of the union)
(dbg) x/t a0 Point
  v:  100
  h:  -50
```

Available types include `Point`, `Rect`, `FInfo`, `GrafPort`,
`WindowRecord`, `HFileInfo`, `DirInfo`, `CInfoPBRec` (union:
file/dir), and others — see `assets/types.def` for the full list.
New types can be added to the `.def` file without recompiling.

### Memory Modification

| Command | Description |
|---------|-------------|
| `set *<addr> = <val>` | Write byte to address |
| `set *<addr>.w = <val>` | Write word (2 bytes) |
| `set *<addr>.l = <val>` | Write long (4 bytes) |

`<addr>` accepts hex addresses, register expressions, or global names.

```
(dbg) set *$0900 = $03
(dbg) set *CurApRefNum.w = $0005
(dbg) set *(a0 + 4).l = $00410000
```

This extends the existing `set <reg> = <val>` syntax: a leading `*`
means "write to memory at this address" rather than to a register.

### Memory Search

| Command | Description |
|---------|-------------|
| `find <start> <end> <pattern>` | Search memory for a byte pattern |

`<pattern>` is a sequence of hex bytes, optionally interspersed with
`??` wildcards that match any byte.  A quoted string searches for the
ASCII (Mac Roman) byte sequence.

```
(dbg) find $0 $80000 4E56 0000
$004012: 4E 56 00 00
$00802A: 4E 56 00 00
2 matches
(dbg) find $0 $80000 A9A0
$00401C: A9 A0
1 match
(dbg) find $0 $1000 "Finder"
$000910: 46 69 6E 64 65 72
1 match
(dbg) find $400000 $410000 48E7 ?? ?? 2F00
$408C00: 48 E7 FF CE 2F 00
1 match
```

The search examines every byte offset in `[start, end)`.  It stops
after 64 matches by default; `find/N` limits to N matches.

### Disassembly

| Command | Description |
|---------|-------------|
| `disas <start> [<end>]` | Disassemble address range |
| `disas <start> +<len>` | Disassemble N bytes from start |

```
(dbg) disas $7C3A60 $7C3AC0
$007C3A60: MOVEA.L  (A7)+,A0
$007C3A64: MOVE.L   D0,-(A7)
…
(dbg) disas $7C3A60 +20
…
```

If no end address is given, disassembles 64 bytes from start.

### Trap Tracing

| Command | Description |
|---------|-------------|
| `trace traps on` | Enable trap call logging to stdout |
| `trace traps off` | Disable trap call logging |
| `trace traps [name…]` | Trace only listed traps (replaces filter) |
| `trace traps [+name… -name…]` | Add/remove individual traps from filter |
| `trace insn on` | Enable instruction logging to stdout |
| `trace insn off` | Disable instruction logging |
| `trace io on` | Enable I/O read/write logging |
| `trace io off` | Disable I/O logging |

Trap trace output uses the TrapTracer's rich hierarchical format with
entry/exit arrows, nesting, parameter decoding, and cycle counts:
```
(dbg) trace traps on
(dbg) c
→ 102844 [2] GetResource(resType:'TEXT', resID:128) [caller:$40802E]
  → 102860 [2] SetResLoad(load:true) [caller:$408C12]
  ← 102870 [2] SetResLoad  (+10 cycles)
← 102944 [2] GetResource → result:$00812400  (+100 cycles)
```

Instruction log output matches the `--log-start` format.

### Breakpoint Commands

Any breakpoint or watchpoint can have a list of commands that execute
automatically when it fires.  Commands are entered interactively after
`commands <id>`:

```
(dbg) break GetResource
Breakpoint 1 on trap GetResource ($A9A0)
(dbg) commands 1
> trace insn on
> finish
> trace insn off
> continue
> end
```

This is the core power feature: attach a script to a breakpoint.
When breakpoint 1 fires, the debugger:
1. Enables instruction logging
2. Runs `finish` (execute until GetResource returns)
3. Disables instruction logging
4. Resumes execution

This lets Beatrix capture exactly the instructions inside a trap call
without drowning in output.

Another example — log every call to SetTrapAddress:
```
(dbg) break SetTrapAddress
Breakpoint 2 on trap SetTrapAddress ($A047)
(dbg) commands 2
> print d0
> print a0
> continue
> end
```

### Symbol Lookup

| Command | Description |
|---------|-------------|
| `info traps [prefix]` | List trap names matching prefix |
| `info globals [prefix]` | List low-memory globals matching prefix |
| `info types [prefix]` | List type definitions matching prefix |
| `info symbol <addr>` | Reverse-lookup: what's at this address? |

Two built-in symbol tables are always available:

1. **Traps** — 681 entries from the trap dictionary (`trap_counter.cpp`).
   Both `GetResource` and `_GetResource` forms are accepted.
2. **Low-memory globals** — ~160 entries from Inside Macintosh
   (`GLOBAL_VARS.md` data compiled into the debugger).
   Each entry has name, address, and size.
3. **Types** — struct and union definitions loaded from
   `assets/types.def` (see `x/t` in Memory Examination).

```
(dbg) info traps Get
GetResource      $A9A0
Get1Resource     $A81F
GetNamedResource $A9A1
GetResInfo       $A9A8
GetResAttrs      $A9A6
…
(dbg) info globals Cur
CurApName    $0910  32 bytes
CurApRefNum  $0900  2 bytes
CurMap       $0A5A  2 bytes
CurStackBase $0908  4 bytes
CurrentA5    $0904  4 bytes
…
(dbg) info symbol $0900
CurApRefNum (2 bytes, low-memory global)
(dbg) info types Graf
GrafPort                  struct   108
(dbg) info types CInfo
CInfoPBRec                union    108
```

### Miscellaneous

| Command | Description |
|---------|-------------|
| `info insn` | Print current instruction count |
| `log [N]` | Show last N guest console log lines (default 20) |
| `log grep <pattern>` | Show guest log lines matching pattern |
| `backtrace` / `bt` | Show stack frames (heuristic: scan for LINK/RTS patterns) |
| `help [cmd]` | Show help (see below) |
| `quit` | Exit emulator |

#### help

`help` with no arguments prints a summary of all command groups:

```
(dbg) help
Execution:    run step next finish continue until
Breakpoints:  break delete disable enable
Watchpoints:  watch rwatch awatch
Registers:    info reg, print, set
Memory:       x, set *, find
Tracing:      trace traps, trace insn, trace io
Info:         info break, info traps, info globals, info types, info symbol, info insn
Other:        commands, backtrace, help, quit

Type 'help <command>' for details on a specific command.
```

`help <cmd>` prints usage and a short example for that command:

```
(dbg) help break
break <location>            -- set breakpoint at address or trap name
break <location> if <cond>  -- set conditional breakpoint

<location>: hex address ($4000), trap name (GetResource), or global name (CurApName)
<cond>:     <reg> <op> <value> [&& ...],  e.g. d0 == 0 && a0 > $1000

Example:
  break GetResource
  break $408000 if d0 != 0
```

---

## Conditions

Conditional breakpoints support a minimal expression language:

```
<reg> <op> <value>
```

- `<reg>` — `d0`–`d7`, `a0`–`a7`, `pc`, `sr`, `sp`, or a global name
- `<op>` — `==`, `!=`, `<`, `>`, `<=`, `>=`, `&` (bitwise AND nonzero)
- `<value>` — hex (`$FF`, `0xFF`) or decimal

Multiple conditions can be joined with `&&`:
```
(dbg) break $4000 if d0 == 0 && a0 > $1000
```

Parenthesized dereferences support size suffixes:
- `(a0 + 22).b` — read byte at A0+22
- `(a0 + 22).w` — read word at A0+22
- `(a0 + 22).l` — read long at A0+22 (default when no suffix)

```
(dbg) break HFSDispatch if d0 == 9 && (a0 + 22).w == $8300
(dbg) print (a0 + 22).w
$00008300 (33536)
```

---

## Implementation Hooks

The debugger leverages existing emulator infrastructure:

| Need | Existing facility |
|------|-------------------|
| Instruction interception | `m68k_go_MaxCycles()` loop — add check at top |
| Register read | `m68k_getRegs()`, `m68k_getPC_public()`, `m68k_getSR_public()` |
| Register write | Direct access to `V_regs` (new setter API) |
| Memory read/write | `get_vm_byte/word/long()`, `put_vm_byte/word/long()` |
| Watchpoints | `get_byte_ext()` / `put_byte_ext()` — add address check |
| Trap breakpoints | `DoCodeA()` — check trap word against breakpoint set |
| Trap tracing | `TrapTracer` with `DbgIO*` for unified output |
| Instruction logging | `g_logStart` / `g_logEnd` mechanism (repurposed as toggle) |
| I/O logging | Existing `IOR`/`IOW` log in CPU loop |
| Trap name lookup | `trap_dict_name()`, `trap_dict_search()` |
| Disassembly | `DisasmOneOrSave()` (existing in m68k.cpp) |
| Instruction count | `g_instructionCount` global |

### Data Structures

- **BreakpointTable** — hash map keyed by PC address, value is
  breakpoint record (id, enabled, condition, command list).
- **TrapBreakpointTable** — hash map keyed by trap word (uint16_t).
- **WatchpointList** — small vector (expect <10 active watchpoints);
  checked in `get_byte_ext()`/`put_byte_ext()` slow path.
- **Symbol tables** — sorted arrays for binary search + prefix matching.

### Performance

When no breakpoints/watchpoints are set and the debugger is in `run`
mode, the per-instruction cost is a single boolean check (`g_dbg_active`).
Breakpoints add a hash lookup per instruction.  Watchpoints only fire
on the memory slow path (cache miss → device or unmapped region), so
they have zero fast-path cost.

---

## Debug Server Mode

### Motivation

The interactive `--debugger` mode reads from stdin and writes to
stdout.  This is ideal for a human sitting at a terminal, but awkward
for automation agents (CI scripts, AI coding assistants, test harnesses)
that want to send a single command and get a single response.

Debug server mode exposes the same command set over a Unix domain
socket, separating the emulator process from the debugger client.

### Activation

```
maxivmac --debugserver[=PATH] [other options…]
```

When `--debugserver` is passed, the emulator starts paused (same as
`--debugger`) but instead of reading stdin, it listens on a Unix
domain socket at `PATH`.  If `PATH` is omitted, the default is
`/tmp/maxivmac-dbg-<PID>.sock`.  The socket path is printed to stderr
on startup:

```
debugserver: listening on /tmp/maxivmac-dbg-42.sock
```

The `--debugger` and `--debugserver` flags are mutually exclusive.

### Client Mode

When `argv[1]` is `debug`, the binary acts as a thin debugger client
instead of an emulator.  No emulator initialization occurs — the
`debug` subcommand is intercepted before any emulator setup.

```
# One-shot: send command, print response, exit
maxivmac debug "info reg"
maxivmac debug "break $00400000"
maxivmac debug "step 5"

# Multi-command: semicolons separate commands in one-shot mode
maxivmac debug "break $4000; run"
maxivmac debug "info reg; info insn"

# Script file: read commands from file
maxivmac debug --script=session.dbg

# Interactive: readline loop talking to the server
maxivmac debug

# Explicit socket path (when multiple servers are running)
maxivmac debug --socket=/tmp/maxivmac-dbg-42.sock "print $d0"
```

**Socket auto-discovery:** when `--socket` is not provided, the client
looks for a single `/tmp/maxivmac-dbg-*.sock` file.  If exactly one
exists, it connects.  If multiple exist, it lists them and exits with
an error asking the user to specify `--socket=`.

### Protocol

Text-based, one command per line.  Each response is terminated by an
ASCII EOT byte (`\x04`) on a line by itself:

```
Client → Server:  step 5\n
Server → Client:  $00400E2A: 4E75  RTS\n
                   $0040087C: 2F00  MOVE.L  D0,-(A7)\n
                   \x04\n
```

The server accepts one client connection at a time (serialized).
A second connection blocks until the first disconnects.  The emulator
is single-threaded, so concurrent commands are not meaningful.

**One-shot mode:** connect → send → read until EOT → print → close.

**Interactive client mode:** keep the connection open, loop on
readline, send each line, read until EOT, print.

### Personas

**Colin** — an AI coding assistant that needs to inspect emulated Mac
state.  Colin launches the emulator with `--debugserver`, then sends
individual commands and parses the responses programmatically.

---

## Non-Goals (for now)

- **Source-level debugging** — no C/Pascal source correlation.
- **GDB RSP / MI protocol** — the debug server uses a simpler
  text protocol suited to the single-threaded emulator.
- **GUI** — command-line only; a future ImGui panel may wrap this.
- **Multi-threading** — the 68K is single-core; no thread awareness.
- **Script files** — ~~no `source` command to load scripts from files~~
  Now supported via `maxivmac debug --script=FILE` and semicolons in
  one-shot mode.

---

## Example Session

```
$ maxivmac --debugger --rom MacPlus.ROM --disk system6.hfs

maxivmac debugger — type 'help' for commands
Loaded 681 trap symbols, 160 low-memory globals
Stopped at $00400000 (reset vector)

(dbg) break GetResource
Breakpoint 1 on trap GetResource ($A9A0)

(dbg) break $00408000
Breakpoint 2 at $00408000

(dbg) watch CurApRefNum
Watchpoint 3 on write to CurApRefNum ($0900, 2 bytes)

(dbg) run
[running]
Watchpoint 3 hit: CurApRefNum ($0900)
  Old value: $0000
  New value: $0002
Stopped at $00401C2E (insn #48201)

(dbg) info reg
D0=00000002  D1=00000000  …
PC=00401C2E  SR=0000 [--------]

(dbg) c
[running]
Breakpoint 1 hit: GetResource ($A9A0)
Stopped at $00408C00 (insn #102844)

(dbg) x/2w (a7)
$FFFE00: 5459 5854

(dbg) commands 1
> trace insn on
> finish
> trace insn off
> continue
> end

(dbg) c
[running]
Breakpoint 1 hit: GetResource ($A9A0)
102900 00408C00: 48E7 …
102901 00408C04: 2F00 …
… (instruction log of GetResource internals) …
102988 00408CFA: 4E75   RTS
[finished GetResource, 88 instructions]
[running]
Breakpoint 1 hit: GetResource ($A9A0)
103500 00408C00: 48E7 …
…

(dbg) delete 1
(dbg) trace traps GetResource Get1Resource
Tracing 2 traps

(dbg) trace traps +OpenResFile -GetResource
  + filter: OpenResFile ($A997)
  - filter: GetResource ($A9A0)
Trap tracing enabled (filtered)

(dbg) c
[running]
[TRAP] $A9A0 GetResource  pc=$40A010 sp=$FFFC00
[TRAP] $A81F Get1Resource  pc=$40A044 sp=$FFFC00
…

(dbg) quit
```
