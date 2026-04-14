# Debugger — Implementation Plan

Design: [DEBUGGER_DESIGN.md](DEBUGGER_DESIGN.md)
Spec: [DEBUGGER.md](DEBUGGER.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Data types, symbol tables, and expression evaluator | DONE |
| 2 | Command parser and dispatch table | DONE |
| 3 | Debugger object skeleton and command loop | DONE |
| 4 | Execution commands (run/step/continue) | DONE |
| 5 | Breakpoints and watchpoints | DONE |
| 6 | Memory examination and modification (x, set, find) | DONE |
| 7 | Trace commands and info commands | DONE |
| 8 | Help system and breakpoint commands | DONE |
| 9 | Redirect trace output from stderr to stdout | DONE |
| 10 | CPU loop integration hooks | DONE |
| 11 | Memory hook integration (watchpoints) | DONE |
| 12 | CLI wiring and startup integration | DONE |
| 13 | Next, finish, until, and SIGINT handling | DONE |
| 14 | End-to-end smoke test | DONE |
| 15 | I/O abstraction (DbgIO interface) | |
| 16 | SocketIO and debug server mode | |
| 17 | Client mode (maxivmac debug) | |
| 18 | Debug server integration test | |

Build gate: `cmake --preset macos && cmake --build --preset macos`
Test gate:  `./bld/macos/tests`

---

## Phase 1 — Data Types, Symbol Tables, and Expression Evaluator

Self-contained data types and lookup modules with no dependencies beyond
standard headers and the existing `trap_counter.h` / `lomem_globals.h`
tables.  These are leaf modules — everything else depends on them but
they depend on nothing in `src/debugger/`.

### 1.1 — Create `src/debugger/symbols.h`

Declare the symbol lookup interface.  See Design §7.

```cpp
// src/debugger/symbols.h
#pragma once
#include <cstdint>
#include <string_view>
#include <vector>

struct SymbolEntry {
    std::string_view name;
    uint32_t    address;
    uint16_t    size;       // 0 for traps
    bool        isTrap;     // true = trap, false = low-mem global
    uint16_t    trapWord;   // valid only when isTrap
};

// Must be called once at startup to build sorted indexes.
void SymbolsInit();

// Resolve a name to an address.  Returns false if not found.
// For traps, sets outAddr=0 and outTrapWord to the trap word.
// For globals, sets outAddr to the global's address and outTrapWord=0.
bool SymbolsResolve(std::string_view name, uint32_t &outAddr, uint16_t &outTrapWord);

// Reverse-lookup: given an address, find the symbol name.
// Returns empty string_view if no symbol at that address.
std::string_view SymbolsAtAddress(uint32_t addr);

// Prefix search.  Appends matching entries to results.
// kind: 't' for traps only, 'g' for globals only, '*' for both.
void SymbolsSearch(std::string_view prefix, char kind,
                   std::vector<SymbolEntry> &results, int maxResults = 20);

// Get the size of a low-memory global by address.  Returns 0 if unknown.
uint16_t SymbolsSizeAt(uint32_t addr);
```

### 1.2 — Create `src/debugger/symbols.cpp`

Implement the two sorted indexes over the existing tables.

- Include `cpu/trap_counter.h` and `platform/lomem_globals.h`.
- At `SymbolsInit()` time, iterate `kLowMemGlobals[0..kLowMemCount)` and
  build a `std::vector<const LMGlobal*>` sorted by name (case-insensitive).
- Build a second `std::vector<const LMGlobal*>` sorted by address for
  reverse lookup.
- For trap lookup, delegate to `trap_dict_name()` and `trap_dict_search()`.
- `SymbolsResolve()`: try `trap_dict_search()` with exact match first,
  then binary-search the globals-by-name vector.
- `SymbolsAtAddress()`: binary-search globals-by-address, then scan
  trap dictionary by address (traps don't have fixed addresses — only
  match against low-mem globals here).
- `SymbolsSearch()`: for traps, call `trap_dict_search()`; for globals,
  prefix scan the sorted name vector.

~120 lines.

### 1.3 — Create `src/debugger/expr.h`

Declare the expression evaluator.  See Design §8.

```cpp
// src/debugger/expr.h
#pragma once
#include <cstdint>
#include <string>
#include <string_view>

// Register read callback — caller supplies this to decouple from CPU.
struct ExprContext {
    uint32_t dregs[8];    // D0-D7
    uint32_t aregs[8];    // A0-A7
    uint32_t pc;
    uint16_t sr;
    // read memory:
    uint32_t (*readLong)(uint32_t addr);
    uint16_t (*readWord)(uint32_t addr);
    uint8_t  (*readByte)(uint32_t addr);
};

// Evaluate a value expression (for `print`).
// Returns true on success, sets outVal.  On error, sets outErr.
bool ExprEval(std::string_view text, const ExprContext &ctx,
              uint32_t &outVal, std::string &outErr);

// Evaluate a condition expression (for breakpoint conditions).
// Returns true if the condition is satisfied.
// On parse error, sets outErr and returns false.
bool ExprCheck(std::string_view text, const ExprContext &ctx,
               std::string &outErr);

// Parse a single value (register, hex, decimal, global name).
// Advances *pos past the consumed token.
// Returns true on success.
bool ExprParseValue(std::string_view text, int &pos,
                    const ExprContext &ctx,
                    uint32_t &outVal, std::string &outErr);
```

### 1.4 — Create `src/debugger/expr.cpp`

Implement the recursive-descent parser from Design §8.

- `ExprParseValue()`: recognize register names (`d0`-`d7`, `a0`-`a7`,
  `pc`, `sr`, `sp`), hex literals (`$xx`, `0xxx`), decimal literals,
  and global names (via `SymbolsResolve()`).
- Parenthesized expressions dereference: `(expr)` reads a long at the
  evaluated address using `ctx.readLong`.
- Arithmetic: `+` and `-` with left-to-right evaluation.
- `ExprEval()`: parse the full expression, return result.
- `ExprCheck()`: parse comparisons joined by `&&`.  Each comparison
  is `value op value` where op is `==`, `!=`, `<`, `>`, `<=`, `>=`,
  or `&` (bitwise AND, true if nonzero).

~150 lines.

### 1.5 — Tests for symbols and expression evaluator

Create `test/test_debugger.cpp` with doctest test cases:

- **Symbol init**: call `SymbolsInit()`, verify trap lookup by name
  (`"GetResource"` → `$A9A0`), global lookup by name
  (`"CurApRefNum"` → `$0900`), reverse lookup, prefix search.
- **Expression evaluator**: set up a mock `ExprContext` with known
  register values and a small memory buffer.  Test:
  - Register names: `"d0"` → D0 value
  - Hex literals: `"$1234"` → 0x1234, `"0xFF"` → 255
  - Decimal: `"42"` → 42
  - Arithmetic: `"a0 + 4"` → A0+4
  - Dereference: `"(a0)"` → readLong(A0 value)
  - Conditions: `"d0 == 0"` → true/false based on D0
  - Compound: `"d0 == 0 && a0 > $1000"` → correct evaluation
  - Error cases: unknown name, missing operator

Add to `CMakeLists.txt` test target:
```cmake
add_executable(tests
    test/test_main.cpp
    test/test_slip.cpp
    test/test_tracing.cpp
    test/test_debugger.cpp
    src/cpu/trap_defs.cpp
    src/cpu/trap_counter.cpp
    src/platform/lomem_globals.cpp
    src/debugger/symbols.cpp
    src/debugger/expr.cpp
)
```

### Fence

- [ ] `src/debugger/symbols.h` and `src/debugger/symbols.cpp` exist
- [ ] `src/debugger/expr.h` and `src/debugger/expr.cpp` exist
- [ ] `test/test_debugger.cpp` exists with symbol and expr tests
- [ ] Unit tests pass: `./bld/macos/tests --test-case="symbol*,expr*"`
- [ ] Full build clean
- [ ] Commit: `"debugger: phase 1 — symbol tables and expression evaluator"`

---

## Phase 2 — Command Parser and Dispatch Table

The tokenizer and command dispatch table used by all `cmd_*.cpp` files.
No debugger state yet — just string processing.  See Design §6.

### 2.1 — Create `src/debugger/cmd_parser.h`

```cpp
// src/debugger/cmd_parser.h
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct Token {
    enum class Kind { Word, Number, Operator, End };
    Kind        kind;
    std::string text;
    uint32_t    numValue;   // valid when kind == Number
};

// Tokenize a command line into tokens.
std::vector<Token> Tokenize(std::string_view line);

// Try to parse a token as a hex/decimal number.  Returns true on success.
bool ParseNumber(std::string_view text, uint32_t &outVal);
```

### 2.2 — Create `src/debugger/cmd_parser.cpp`

Implement the tokenizer:

- Split on whitespace.
- Recognize `$hex`, `0xhex` as Number tokens.
- Recognize plain decimal digits as Number tokens.
- Recognize `==`, `!=`, `<=`, `>=`, `<`, `>`, `&&`, `&`, `+`, `-`,
  `=`, `*`, `(`, `)` as Operator tokens.
- Everything else is a Word token.
- Append a sentinel `End` token.

Also define the command table structure (but not the handler
implementations — those come in later phases):

```cpp
class Debugger;  // forward

struct CmdEntry {
    std::string_view name;
    std::string_view shortcut;  // empty if none
    void (*handler)(Debugger &dbg, const std::vector<Token> &args);
    std::string_view helpBrief;
    std::string_view helpFull;
};

// Find the CmdEntry matching the first token (prefix match).
// Returns nullptr if no match or ambiguous (prints error in that case).
const CmdEntry *DispatchCommand(std::string_view input,
                                const CmdEntry *table, int tableSize);
```

`DispatchCommand()`: iterate the table, collect entries whose `name`
or `shortcut` starts with `input`.  If exactly one match, return it.
If zero, print "Unknown command".  If multiple, print "Ambiguous" with
candidates.

~100 lines.

### 2.3 — Tests for tokenizer and dispatch

Add test cases to `test/test_debugger.cpp`:

- **Tokenizer**: `"break $4000 if d0 == 0"` → Word("break"),
  Number("$4000", 0x4000), Word("if"), Word("d0"), Operator("=="),
  Number("0", 0), End.
- **Tokenizer edge cases**: empty string, double spaces, `0xFF`,
  `$$` (error), operators `&&`, `<=`, `>=`.
- **Dispatch**: mock table with 3 entries, test exact match, prefix
  match, ambiguous prefix, unknown command.

Add `src/debugger/cmd_parser.cpp` to the test target in `CMakeLists.txt`.

### Fence

- [ ] `src/debugger/cmd_parser.h` and `src/debugger/cmd_parser.cpp` exist
- [ ] Tokenizer and dispatch tests pass: `./tests --test-case="token*,dispatch*"`
- [ ] Full build clean
- [ ] Commit: `"debugger: phase 2 — command parser and dispatch table"`

---

## Phase 3 — Debugger Object Skeleton and Command Loop

Create the Debugger singleton with stub methods.  The command loop
reads from stdin and dispatches, but all command handlers are stubs
that print "not implemented".  See Design §2 and §4.

### 3.1 — Create `src/debugger/debugger.h`

Public header — the only file included outside `src/debugger/`.

```cpp
// src/debugger/debugger.h
#pragma once
#include <cstdint>
#include <string_view>

class Debugger {
public:
    static Debugger *instance();
    static void create();

    // CPU hooks
    bool instructionHook(uint32_t pc);
    bool trapHook(uint16_t trapWord);
    bool memoryHook(uint32_t addr, uint32_t size, char direction,
                    uint32_t oldVal, uint32_t newVal);

    bool isRunning() const;
    bool isStopped() const;
    void stop(std::string_view reason);

    // For cmd_*.cpp access (internal use)
    // ... exposed via friend or public accessors in implementation
private:
    Debugger();
    void commandLoop();
    struct Impl;
    Impl *impl_;
};

extern bool g_debuggerActive;
```

### 3.2 — Create `src/debugger/debugger.cpp`

Implement the singleton, state enum, and command loop skeleton.

Internal state (from Design §4):
```cpp
enum class State { Stopped, Running, Stepping };
```

- `create()`: allocate singleton, call `SymbolsInit()`.
- `instance()`: return static pointer.
- `commandLoop()`: print `(dbg) ` prompt, read line from stdin via
  `fgets()`, tokenize, dispatch via `DispatchCommand()`.  Loop until
  state transitions to Running/Stepping.
- All hooks return `false` (pass-through) for now.
- `stop()`: set state to Stopped, print reason.

Register the full command table with all entries pointing to stub
handlers that print `"(not implemented)"`.  The stubs live here
temporarily — later phases replace them with real implementations.

Print startup banner: `"maxivmac debugger — type 'help' for commands"`.
Print symbol counts: `"Loaded N trap symbols, M low-memory globals"`.

### 3.3 — Create stub `cmd_*.cpp` files

Create these files with empty handler function definitions that
`debugger.cpp` will reference.  Each file includes `debugger.h` and
`cmd_parser.h`:

- `src/debugger/cmd_exec.cpp` — stubs for `CmdRun`, `CmdContinue`,
  `CmdStep`, `CmdNext`, `CmdFinish`, `CmdUntil`, `CmdStepi`
- `src/debugger/cmd_break.cpp` — stubs for `CmdBreak`, `CmdDelete`,
  `CmdDisable`, `CmdEnable`, `CmdWatch`, `CmdRwatch`, `CmdAwatch`,
  `CmdCommands`
- `src/debugger/cmd_memory.cpp` — stubs for `CmdExamine`, `CmdPrint`,
  `CmdSet`, `CmdFind`
- `src/debugger/cmd_trace.cpp` — stubs for `CmdTrace`
- `src/debugger/cmd_info.cpp` — stubs for `CmdInfo`, `CmdBacktrace`
- `src/debugger/cmd_help.cpp` — stubs for `CmdHelp`, `CmdQuit`

Each stub: `void CmdXxx(Debugger &dbg, const std::vector<Token> &args)
{ std::printf("(not implemented)\n"); }`

### 3.4 — Update `CMakeLists.txt`

Add all `src/debugger/*.cpp` files to `MINIVMAC_SOURCES`:

```cmake
    # Debugger
    src/debugger/debugger.cpp
    src/debugger/cmd_parser.cpp
    src/debugger/cmd_exec.cpp
    src/debugger/cmd_break.cpp
    src/debugger/cmd_memory.cpp
    src/debugger/cmd_trace.cpp
    src/debugger/cmd_info.cpp
    src/debugger/cmd_help.cpp
    src/debugger/symbols.cpp
    src/debugger/expr.cpp
```

Insert after the `# CPU` section (after `src/cpu/disasm.cpp`).

### Fence

- [ ] `src/debugger/debugger.h` and `src/debugger/debugger.cpp` exist
- [ ] All `cmd_*.cpp` stub files exist
- [ ] `CMakeLists.txt` includes all debugger sources
- [ ] Full build clean (stubs compile, no linker errors)
- [ ] Commit: `"debugger: phase 3 — object skeleton and command loop"`

---

## Phase 4 — Execution Commands (run/step/continue)

Implement the core execution commands: `run`, `continue`, `step`,
`stepi`.  These change the debugger state and control how
`commandLoop()` exits.  `next`, `finish`, and `until` are deferred
to Phase 13 because they need the CPU hooks wired in.

See Design §5.1, Spec §Execution.

### 4.1 — Implement `CmdRun` and `CmdContinue` in `cmd_exec.cpp`

Both set `state_ = State::Running` and return from the command handler.
`commandLoop()` checks state each iteration — when no longer Stopped,
it returns.

`run` prints `"[running]"`.  `continue` prints `"[continuing]"`.

### 4.2 — Implement `CmdStep` and `CmdStepi` in `cmd_exec.cpp`

Parse optional `[N]` argument (default 1).  Set `stepsRemaining_ = N`,
`state_ = State::Stepping`.  `commandLoop()` exits.

`stepi` is an alias — same implementation.

### 4.3 — Expose internal state accessors

In `debugger.cpp`, add public/internal methods for cmd_exec to use:

```cpp
void setRunning();
void setStepping(uint32_t n);
State state() const;
```

These are called by cmd_exec.cpp.  Either make them public accessors
on `Debugger` or give `cmd_exec.cpp` access via a friend or internal
header.

### 4.4 — Update `instructionHook` for stepping

In `debugger.cpp`, implement the stepping logic in `instructionHook()`:

```
if state == Stopped:
    commandLoop()
    return true

if state == Stepping:
    if --stepsRemaining_ == 0:
        stop("step completed")
        return true

return false
```

This is enough to support `step N` even before breakpoints exist.

### 4.5 — Test execution commands

Add test cases to `test/test_debugger.cpp`:

- Cannot unit-test `commandLoop()` (needs stdin), but can test the
  state transitions directly:
  - Debugger starts in Stopped state
  - After `setRunning()`, `isRunning()` is true
  - After `setStepping(3)`, state is Stepping
  - `instructionHook()` decrements steps and stops at 0

### Fence

- [ ] `cmd_exec.cpp` implements run, continue, step, stepi
- [ ] `instructionHook()` handles Stopped and Stepping states
- [ ] Unit tests pass
- [ ] Full build clean
- [ ] Commit: `"debugger: phase 4 — execution commands (run/step/continue)"`

---

## Phase 5 — Breakpoints and Watchpoints

Implement breakpoint and watchpoint management: add, delete, enable,
disable, list.  The actual *hit detection* comes later when hooks are
wired into the CPU loop (Phase 10–11).  This phase focuses on the data
structures and commands.

See Design §4, §5.2, Spec §Breakpoints and §Watchpoints.

### 5.1 — Add breakpoint/watchpoint data structures to `debugger.cpp`

In the `Debugger::Impl` struct, add:

```cpp
struct Breakpoint {
    uint32_t    id;
    bool        enabled;
    uint32_t    address;       // PC address (0 for trap-only breakpoints)
    uint16_t    trapWord;      // non-zero for trap breakpoints
    std::string condition;     // raw condition text
    std::vector<std::string> commands;  // auto-execute on hit
};

uint32_t nextBpId_ = 1;
std::unordered_map<uint32_t, Breakpoint>  bpByAddr_;   // address → bp
std::unordered_map<uint16_t, Breakpoint>  bpByTrap_;   // trap word → bp
std::unordered_map<uint32_t, Breakpoint*> bpById_;     // id → bp ptr

struct Watchpoint {
    uint32_t    id;
    bool        enabled;
    uint32_t    address;
    uint32_t    length;
    char        mode;           // 'W', 'R', 'A'
    bool        hasValCond;
    uint8_t     valCondOp;      // 0=eq,1=ne,2=lt,3=gt,4=le,5=ge,6=and
    uint32_t    valCondValue;
};

std::vector<Watchpoint> watchpoints_;
```

Add internal methods:
```cpp
uint32_t addBreakpoint(uint32_t addr, uint16_t trapWord,
                       const std::string &condition);
uint32_t addWatchpoint(uint32_t addr, uint32_t len, char mode,
                       bool hasValCond, uint8_t valCondOp, uint32_t valCondValue);
bool deleteById(uint32_t id);
bool enableById(uint32_t id, bool enable);
const Breakpoint *lookupByAddr(uint32_t addr) const;
const Breakpoint *lookupByTrap(uint16_t trapWord) const;
```

### 5.2 — Implement `CmdBreak` in `cmd_break.cpp`

Parse `break <location> [if <cond>]`:

1. Tokenize the location argument.
2. Try `ParseNumber()` → address breakpoint.
3. If not a number, try `SymbolsResolve()`:
   - If resolved as trap → add trap breakpoint.
   - If resolved as global → add address breakpoint at global's addr.
4. If `if` keyword follows, capture the rest as condition text.
5. Call `addBreakpoint()`, print confirmation with ID.

### 5.3 — Implement `CmdWatch`, `CmdRwatch`, `CmdAwatch` in `cmd_break.cpp`

Parse `watch <addr> [len] [if val <op> <value>]`:

1. Parse address (number or global name via `SymbolsResolve()`).
   If a global name, auto-set `len` to the global's size.
2. Parse optional length (default 1 for raw address, global size for names).
3. Parse optional `if val <op> <value>` condition.
4. Call `addWatchpoint()` with mode `'W'`/`'R'`/`'A'`.

### 5.4 — Implement `CmdDelete`, `CmdDisable`, `CmdEnable`

- `delete <id>` → `deleteById(id)`.
- `delete` (no args) → delete all breakpoints and watchpoints.
- `disable <id>` → `enableById(id, false)`.
- `enable <id>` → `enableById(id, true)`.

### 5.5 — Implement `info break` in `cmd_info.cpp`

Print a numbered table of all breakpoints and watchpoints with their
status (enabled/disabled), type, address/trap, and condition.

### 5.6 — Tests

Add to `test/test_debugger.cpp`:

- Create Debugger, add breakpoint by address, verify lookup by addr.
- Add trap breakpoint, verify lookup by trap word.
- Add watchpoint, verify it's in the list.
- Delete by ID, verify removal.
- Disable/enable, verify flag.
- Conditional breakpoint — verify condition string stored.

### Fence

- [ ] `cmd_break.cpp` implements break, watch, rwatch, awatch, delete, disable, enable
- [ ] `cmd_info.cpp` implements `info break`
- [ ] Internal data structure methods work correctly
- [ ] Unit tests pass
- [ ] Full build clean
- [ ] Commit: `"debugger: phase 5 — breakpoints and watchpoints"`

---

## Phase 6 — Memory Examination and Modification

Implement `x` (examine), `print`, `set`, and `find` commands.
These read/write emulator memory via the `get_vm_*`/`put_vm_*` API
from `m68k.h`.

See Spec §Memory Examination, §Memory Modification, §Memory Search,
§Registers, Design §2.

### 6.1 — Implement `CmdExamine` (`x`) in `cmd_memory.cpp`

Parse `x[/FMT] <addr>`:

1. Parse the format spec after `/`: count (default 1), size (`b`/`w`/`l`,
   default `w`), format (`x`/`d`/`s`/`i`, default `x`).
2. Parse address (number, register expression, global name).
3. Loop `count` times:
   - `x` format: read memory via `get_vm_byte/word/long()`, print hex.
   - `d` format: same but decimal.
   - `s` format: read bytes until null, print as Mac Roman string.
   - `i` format: call `Disassemble(pc)` from `disasm.h`.
4. Print in rows of 8 (for byte) or 4 (for word/long).

### 6.2 — Implement `CmdPrint` in `cmd_memory.cpp`

Parse `print <expr>`:

1. Build an `ExprContext` from live CPU state (call `m68k_getRegs()`,
   `m68k_getPC_public()`, `m68k_getSR_public()`; memory callbacks
   use `get_vm_long()` etc.).
2. Call `ExprEval()`.
3. Print result as `$XXXXXXXX`.

### 6.3 — Implement `CmdSet` in `cmd_memory.cpp`

Parse `set <target> = <value>`:

- If target starts with `*`: memory write.
  - Parse address expression after `*`.
  - Check for `.w` or `.l` suffix for word/long size (default byte).
  - Evaluate value, write via `put_vm_byte/word/long()`.
- Otherwise: register write.
  - Recognize `d0`-`d7`, `a0`-`a7`, `pc`, `sr`.
  - Write via direct `m68k_dreg(n) = val` etc.
  - Requires write accessors (either macros already exist or add thin
    wrappers).

### 6.4 — Implement `CmdFind` in `cmd_memory.cpp`

Parse `find <start> <end> <pattern>`:

1. Parse start and end addresses.
2. Parse pattern: hex byte pairs, `??` wildcards, or quoted string.
3. Scan memory byte-by-byte from start to end, matching pattern.
4. Print each match address and the matching bytes (up to 64 matches
   by default; support `find/N` to limit to N matches).

### 6.5 — Implement `info reg` in `cmd_info.cpp`

Call `m68k_getRegs()`, `m68k_getPC_public()`, `m68k_getSR_public()`,
`m68k_getUSP()`, `m68k_getISP()`.  Print in the format from the spec:

```
D0=XXXXXXXX  D1=XXXXXXXX  ...
A0=XXXXXXXX  A1=XXXXXXXX  ...
PC=XXXXXXXX  SR=XXXX [flags]  USP=XXXXXXXX  ISP=XXXXXXXX
```

Format SR flags as `[S--Z---]` etc.

### 6.6 — Tests

Add to `test/test_debugger.cpp`:

- Test format spec parser: `"/8w"` → count=8, size=word, fmt=hex.
- Test `CmdFind` pattern parser: hex bytes, wildcards, strings.
- Expression evaluator already tested in Phase 1; just verify
  integration with `CmdPrint` if feasible.

### Fence

- [ ] `cmd_memory.cpp` implements x, print, set, find
- [ ] `cmd_info.cpp` implements info reg
- [ ] Unit tests pass
- [ ] Full build clean
- [ ] Commit: `"debugger: phase 6 — memory examination and modification"`

---

## Phase 7 — Trace Commands and Info Commands

Implement `trace traps/insn/io` and `info traps/globals/symbol/insn`.

See Spec §Trap Tracing, §Symbol Lookup, Design §7, §11.

### 7.1 — Implement `CmdTrace` in `cmd_trace.cpp`

Parse `trace <target> <on|off|names...>`:

- `trace traps on`: call `BeginTraceTraps()`, set `traceTraps_ = true`.
- `trace traps off`: call `EndTraceTraps()`, set `traceTraps_ = false`.
- `trace traps Name1 Name2`: resolve each name via `SymbolsResolve()`,
  add trap words to `trapFilter_` set, enable tracing.
- `trace insn on/off`: set `traceInsn_` flag.
- `trace io on/off`: set `traceIO_` flag.

Add internal flags to `Debugger::Impl`:
```cpp
bool traceTraps_ = false;
bool traceInsn_  = false;
bool traceIO_    = false;
std::unordered_set<uint16_t> trapFilter_;
```

### 7.2 — Implement info commands in `cmd_info.cpp`

- `info traps [prefix]`: call `SymbolsSearch(prefix, 't', ...)`,
  print name and trap word columns.
- `info globals [prefix]`: call `SymbolsSearch(prefix, 'g', ...)`,
  print name, address, and size columns.
- `info symbol <addr>`: call `SymbolsAtAddress(addr)`, print result.
- `info insn`: print `g_instructionCount`.

All dispatch through `CmdInfo()` which checks the second token
(`traps`/`globals`/`symbol`/`insn`/`reg`/`break`).

### 7.3 — Implement `CmdBacktrace` in `cmd_info.cpp`

Heuristic backtrace: read A7 (SP), scan the stack for values that
look like return addresses (within RAM/ROM range).  For each candidate,
disassemble the preceding bytes looking for JSR/BSR patterns.  Print
frame number, address, and any symbol match.

Limit to 20 frames.  This is best-effort — no debug info available.

### Fence

- [ ] `cmd_trace.cpp` implements trace traps/insn/io
- [ ] `cmd_info.cpp` implements info traps/globals/symbol/insn, backtrace
- [ ] Full build clean
- [ ] Commit: `"debugger: phase 7 — trace and info commands"`

---

## Phase 8 — Help System and Breakpoint Commands

Implement `help`, `commands`, and `quit`.

See Spec §Miscellaneous, §Breakpoint Commands, Design §5.6.

### 8.1 — Implement `CmdHelp` in `cmd_help.cpp`

- `help` (no args): print the grouped summary from the spec.
- `help <cmd>`: look up `CmdEntry` by name, print `helpFull` text.

The help text for each command is stored in the command table's
`helpFull` field, defined alongside the handler registrations.

### 8.2 — Implement `CmdQuit` in `cmd_help.cpp`

Call `std::exit(0)`.  This is the simple approach — the emulator is
a single-process app.

### 8.3 — Implement `CmdCommands` in `cmd_break.cpp`

Parse `commands <id>`:

1. Look up breakpoint/watchpoint by ID.
2. Enter a sub-loop reading lines from stdin, accumulating command
   strings until the user types `end`.
3. Store the command list in the breakpoint's `commands` vector.

### 8.4 — Implement breakpoint command execution in `debugger.cpp`

When a breakpoint fires (in `instructionHook` — full wiring in Phase 9,
but implement the method now):

```cpp
void Debugger::executeCommands(const std::vector<std::string> &cmds);
```

Feed each string through the tokenizer and dispatch.  If a command
changes state to Running (e.g., `continue`, `finish`), save the
remaining commands and return.  On next stop, resume executing the
saved commands before entering the interactive loop.

### Fence

- [ ] `cmd_help.cpp` implements help and quit
- [ ] `cmd_break.cpp` implements commands
- [ ] `debugger.cpp` has `executeCommands()` logic
- [ ] Full build clean
- [ ] Commit: `"debugger: phase 8 — help, quit, and breakpoint commands"`

---

## Phase 9 — Redirect Trace Output from stderr to stdout

Design §11 requires all trace and log output to go to stdout (the GUI
app doesn't use stdout otherwise).  stderr stays for real errors.  This
must happen before the CPU hooks wire up, so the debugger's trace
commands produce output on the correct stream.

### 9.1 — Change trap tracing output to stdout

In `src/cpu/trap_counter.cpp`, find every `fprintf(stderr, ...)` call
in `trap_trace_log()` and the watchlist output functions.  Change to
`fprintf(stdout, ...)`.

### 9.2 — Change trap tracer output to stdout

In `src/cpu/trap_tracer.cpp`, find every `fprintf(stderr, ...)` call
in the tracer output path.  Change to `fprintf(stdout, ...)`.

### 9.3 — Change instruction log output to stdout

In `src/cpu/m68k.cpp`, in `m68k_go_MaxCycles()`, the existing
instruction log block (`g_logEnd > 0`) uses `std::fprintf(stderr, ...)`.
Change to `std::fprintf(stdout, ...)` and the corresponding `fflush`.

Leave actual error messages (ROM load failures, assertions, etc.) on
stderr.

### Fence

- [ ] All trace/log `fprintf(stderr, ...)` calls changed to stdout
- [ ] Actual error messages remain on stderr
- [ ] Full build clean
- [ ] Non-regression: `--log-start` output now appears on stdout
- [ ] Commit: `"debugger: phase 9 — redirect trace output to stdout"`

---

## Phase 10 — CPU Loop Integration Hooks

Wire the debugger into the CPU instruction loop and trap dispatcher.
This is where the feature becomes *live*.  Each hook is a 3-5 line
insertion guarded by `g_debuggerActive`.

See Design §3.1, §3.2, §5.1, §5.2, §10.

### 10.1 — Declare `g_debuggerActive` global

In `debugger.cpp`:
```cpp
bool g_debuggerActive = false;
```

In `debugger.h`:
```cpp
extern bool g_debuggerActive;
```

### 10.2 — Insert instruction hook into `m68k_go_MaxCycles()`

In `src/cpu/m68k.cpp`, add `#include "debugger/debugger.h"` at the top.

Insert after the closing brace of the logging/recorder block (after
line 782 — after `DisasmOneOrSave(pc)` and the `#if WantBreakPoint`
block), before `d()`:

```cpp
        if (g_debuggerActive) {
            if (Debugger::instance()->instructionHook(pc)) {
                // Debugger command loop ran; user issued run/step.
            }
        }
```

### 10.3 — Insert trap hook into `DoCodeA()`

After `trap_counter_record(tw)`, before `g_tracer.enter(tw)`:

```cpp
    if (g_debuggerActive && Debugger::instance()->trapHook(tw)) {
        // Trap breakpoint matched — instructionHook will fire next.
    }
```

### 10.4 — Implement `instructionHook()` fully

Fill in the complete algorithm from Design §5.1:

1. Stopped state → `commandLoop()`, return true.
2. Stepping → decrement counter, stop when zero.
3. Check `bpByAddr_` for PC match → evaluate condition → stop.
4. `traceInsn_` → print instruction log line (reuse the format from
   the existing logging block: insn count, PC, opcode, regs).

### 10.5 — Implement `trapHook()` fully

From Design §5.2:

1. Check `bpByTrap_` for trap word match → stop.
2. `traceTraps_` with filter → print trap log line using
   `trap_dict_name()`.

### Fence

- [ ] `m68k.cpp` has debugger include and two hook insertions (~6 lines total)
- [ ] `instructionHook()` and `trapHook()` fully implemented
- [ ] `g_debuggerActive` declared in debugger.h, defined in debugger.cpp
- [ ] Full build clean
- [ ] Manual test: build, run `maxivmac --debugger`, observe `(dbg)` prompt,
      type `step`, `info reg`, `run`, Ctrl-C
- [ ] Commit: `"debugger: phase 10 — CPU loop integration hooks"`

---

## Phase 11 — Memory Hook Integration (Watchpoints)

Wire watchpoint checks into the memory slow path (`_ext` functions)
and implement MATC invalidation when watchpoints are added/removed.

See Design §3.3, §5.3, §9.

### 11.1 — Insert watchpoint checks into `_ext` functions

In `src/cpu/m68k.cpp`, in each of the four `_ext` functions, add a
watchpoint check where the data value is known.

For `get_byte_ext()`, after the data is loaded (before the return):
```cpp
    if (g_debuggerActive) {
        Debugger::instance()->memoryHook(addr, 1, 'R', Data, Data);
    }
```

For `put_byte_ext()`, after the write:
```cpp
    if (g_debuggerActive) {
        Debugger::instance()->memoryHook(addr, 1, 'W', /* oldVal */ 0, b);
    }
```

Similar for `get_word_ext()` and `put_word_ext()`, with size=2.

Note: for writes, getting the old value requires a read before the
write.  For simplicity in v1, pass `oldVal=0` for writes and document
this limitation.  The watchpoint still fires; old-value display will
show 0.  (A later enhancement can add a pre-read.)

### 11.2 — Implement `memoryHook()` fully

From Design §5.3:

1. Linear scan `watchpoints_` (typically <10 entries).
2. Check range overlap and direction match.
3. Check value condition if present.
4. Stop and print old/new values on match.

### 11.3 — Implement MATC invalidation on watchpoint add/remove

When `addWatchpoint()` is called:

1. Call `FindATTel(addr)` to find the ATT entry.
2. Save its original `Access` flags.
3. Clear `kATTA_readreadymask`/`kATTA_writereadymask` as needed,
   set `kATTA_ntfymask`.
4. Invalidate MATC entries (set `cmpmask`/`cmpvalu` to impossible
   values on all 6 MATC entries in `V_regs`).

When `deleteById()` removes a watchpoint:

1. Restore the ATT entry's original `Access` flags.
2. MATC repopulates naturally.

If `FindATTel` and MATC internals are not easily accessible from
`debugger.cpp`, add a thin helper in `m68k.cpp`:

```cpp
// In m68k.h:
void m68k_invalidateMATC();
ATTEntryPtr m68k_findATT(uint32_t addr);
```

### Fence

- [ ] Four `_ext` functions have watchpoint hooks (~4 lines each)
- [ ] `memoryHook()` fully implemented
- [ ] MATC invalidation on watchpoint add/remove implemented
- [ ] Full build clean
- [ ] Manual test: `watch $0900`, `run`, verify it fires on write
- [ ] Commit: `"debugger: phase 11 — memory hooks and watchpoints"`

---

## Phase 12 — CLI Wiring and Startup Integration

Connect the debugger to the command-line parser and startup sequence.

See Design §3.4, §3.5.

### 12.1 — Add `--debugger` flag to `LaunchConfig`

In `src/core/config_loader.h`, add to `LaunchConfig`:
```cpp
bool debugger = false;
```

### 12.2 — Parse `--debugger` in `ParseCommandLine()`

In `src/core/config_loader.cpp`, add to the argument parsing loop
(after the `--trace-traps` check):

```cpp
if (strcmp(arg, "--debugger") == 0)
{
    lc.debugger = true;
    continue;
}
```

### 12.3 — Initialize debugger in `ProgramEarlyInit()`

In `src/core/main.cpp`, add `#include "debugger/debugger.h"`.

After `s_launchConfig = ParseCommandLine(...)` and before the help
check:

```cpp
if (s_launchConfig.debugger) {
    Debugger::create();
    g_debuggerActive = true;
}
```

### 12.4 — Add `--debugger` to help text

In the help/usage output in `config_loader.cpp`, add a line:
```
  --debugger           Start with debugger prompt (paused at first instruction)
```

### Fence

- [ ] `--debugger` flag parsed and stored in LaunchConfig
- [ ] `Debugger::create()` called when flag is set
- [ ] `g_debuggerActive` set to true
- [ ] Full build clean
- [ ] Manual test: `./bld/macos/maxivmac --debugger` shows `(dbg)` prompt
- [ ] Commit: `"debugger: phase 12 — CLI flag and startup integration"`

---

## Phase 13 — Next, Finish, Until, and SIGINT Handling

Implement the remaining execution commands that depend on live CPU
hooks: `next`, `finish`, `until`.  Add Ctrl-C (SIGINT) handling.

See Design §5.4, §5.5, Spec §Execution.

### 13.1 — Implement `CmdFinish` in `cmd_exec.cpp`

1. Record current SP: `m68k_areg(7)`.
2. Set `finishing_ = true`, `savedSP_ = sp`.
3. Set state to Running.

In `instructionHook()`, add finish check (Design §5.4):
- If `finishing_` and current SP ≥ `savedSP_`:
  - Read opcode at PC.  If RTS (`$4E75`), RTD (`$4E74`), or
    RTE (`$4E73`), stop with `"finish completed"`.

### 13.2 — Implement `CmdNext` in `cmd_exec.cpp`

Parse optional `[N]` (default 1).

Algorithm from Design §5.5:
1. Read opcode at PC.
2. If BSR, JSR, or A-line trap: record SP, set `nexting_ = true`,
   state to Running.
3. Otherwise: execute as single `step`.

In `instructionHook()`, add next check:
- If `nexting_` and SP ≥ `savedSP_`: stop.

### 13.3 — Implement `CmdUntil` in `cmd_exec.cpp`

Parse `until <addr>`.  Set `untilAddr_` and state to Running.

In `instructionHook()`: if `untilAddr_ != 0` and `pc == untilAddr_`,
stop with `"until reached"`.

### 13.4 — SIGINT handler

In `debugger.cpp`, install a signal handler:

```cpp
#include <csignal>

static volatile sig_atomic_t s_interrupted = 0;

static void SignalHandler(int) {
    s_interrupted = 1;
}
```

In `Debugger::create()`:
```cpp
std::signal(SIGINT, SignalHandler);
```

In `instructionHook()`, check at the top (after Stopped check):
```cpp
if (s_interrupted) {
    s_interrupted = 0;
    stop("interrupted");
    return true;
}
```

### Fence

- [ ] `cmd_exec.cpp` implements finish, next, until
- [ ] `instructionHook()` handles finish, next, until, and SIGINT
- [ ] Full build clean
- [ ] Manual test: `next` steps over JSR, `finish` runs to RTS,
      Ctrl-C pauses running emulator
- [ ] Commit: `"debugger: phase 13 — next, finish, until, SIGINT"`

---

## Phase 14 — End-to-End Smoke Test

Verify the complete debugger works in a realistic session.

### 14.1 — Write a smoke test script

Create `test/debugger_smoke.sh`:

```bash
#!/bin/bash
# Feed debugger commands via stdin and verify output
set -e

MAXIVMAC=./bld/macos/maxivmac
ROM=roms/MacPlus.ROM

if [ ! -f "$ROM" ]; then
    echo "SKIP: $ROM not found"
    exit 0
fi

# Test 1: start and quit
echo "quit" | $MAXIVMAC --debugger --rom "$ROM" --headless 2>/dev/null | grep -q "(dbg)"

# Test 2: step and info reg
printf "step\ninfo reg\nquit\n" | $MAXIVMAC --debugger --rom "$ROM" --headless 2>/dev/null | grep -q "D0="

# Test 3: set breakpoint and run
printf "break \$400000\nrun\ninfo reg\nquit\n" | $MAXIVMAC --debugger --rom "$ROM" --headless 2>/dev/null | grep -q "Breakpoint"

echo "All debugger smoke tests passed."
```

### 14.2 — Run manual test session

Perform the example session from the spec manually:

1. `maxivmac --debugger --rom MacPlus.ROM --disk system6.hfs`
2. `break GetResource` → verify breakpoint created
3. `watch CurApRefNum` → verify watchpoint created
4. `run` → verify watchpoint fires first
5. `c` → verify trap breakpoint fires
6. `x/2w (a7)` → verify memory read
7. `info reg` → verify register display
8. `help` → verify help text
9. `quit`

### 14.3 — Document any found issues

If any command doesn't work as specified, file the issue in comments
at the top of the relevant `cmd_*.cpp` file for later fix.

### Fence

- [ ] `test/debugger_smoke.sh` exists and passes
- [ ] Manual session completes without crashes
- [ ] Full build clean
- [ ] Commit: `"debugger: phase 14 — smoke test"`

---

## Phase 15 — I/O Abstraction (DbgIO Interface)

Extract all debugger I/O behind a polymorphic interface so the command
loop and every command handler are transport-agnostic.  This phase
does not add socket support — it only introduces the abstraction and
migrates the existing stdin/stdout code to use it.

See Design §16.

### 15.1 — Create `src/debugger/dbg_io.h`

Define the `DbgIO` abstract base class:

```cpp
// src/debugger/dbg_io.h
#pragma once
#include <cstdarg>
#include <cstddef>

class DbgIO {
public:
    virtual ~DbgIO() = default;

    // Read one line of input (blocking).  Returns false on EOF/error.
    virtual bool readLine(char *buf, size_t len) = 0;

    // Formatted write — printf semantics.
    virtual void write(const char *fmt, ...) = 0;

    // Mark end of one command's response.
    // StdioIO: no-op.  SocketIO: sends EOT byte.
    virtual void endResponse() = 0;

    // Flush the output stream.
    virtual void flush() = 0;
};
```

### 15.2 — Create `src/debugger/dbg_io.cpp` with `StdioIO`

Implement the stdio transport that wraps the existing behaviour:

```cpp
// src/debugger/dbg_io.cpp
#include "debugger/dbg_io.h"
#include <cstdio>

class StdioIO final : public DbgIO {
public:
    bool readLine(char *buf, size_t len) override
    {
        return std::fgets(buf, static_cast<int>(len), stdin) != nullptr;
    }

    void write(const char *fmt, ...) override
    {
        std::va_list ap;
        va_start(ap, fmt);
        std::vprintf(fmt, ap);
        va_end(ap);
    }

    void endResponse() override {}

    void flush() override { std::fflush(stdout); }
};
```

Also provide a factory function declared in `dbg_io.h`:

```cpp
// In dbg_io.h:
#include <memory>
std::unique_ptr<DbgIO> CreateStdioIO();
```

### 15.3 — Add `DbgIO` to the Debugger object

Modify `Debugger::Impl` to hold a `std::unique_ptr<DbgIO> io_`.

Change `Debugger::create()` signature:

```cpp
// In debugger.h:
static void create(std::unique_ptr<DbgIO> io = nullptr);
```

When `io` is nullptr, `create()` defaults to `CreateStdioIO()`.
This preserves backward compatibility — the existing `--debugger`
startup code doesn't need to change yet.

Add a public accessor:

```cpp
// In debugger.h:
DbgIO &io();
```

### 15.4 — Migrate `commandLoop()` in `debugger.cpp`

Replace the direct `fgets`/`printf`/`fflush` calls:

```cpp
// Before:
std::printf("(dbg) ");
std::fflush(stdout);
if (!std::fgets(buf, sizeof(buf), stdin)) { ... }

// After:
impl_->io_->write("(dbg) ");
impl_->io_->flush();
if (!impl_->io_->readLine(buf, sizeof(buf))) { ... }
```

Also replace any `std::printf(...)` calls within `debugger.cpp`
(the ~17 calls for stop reasons, startup banner, instruction display,
etc.) with `impl_->io_->write(...)`.

### 15.5 — Migrate all `cmd_*.cpp` files

All command handlers receive a `Debugger &dbg` reference.  Replace
every `std::printf(...)` in each `cmd_*.cpp` with `dbg.io().write(...)`:

| File | Approx. replacements |
|------|---------------------|
| `cmd_exec.cpp` | 6 |
| `cmd_break.cpp` | 30 |
| `cmd_memory.cpp` | 34 |
| `cmd_trace.cpp` | 13 |
| `cmd_info.cpp` | 36 |
| `cmd_help.cpp` | 45 |

This is a mechanical search-and-replace.  No logic changes.

### 15.6 — Update CMakeLists.txt

Add `src/debugger/dbg_io.cpp` to the debugger sources list.

### 15.7 — Tests

Add to `test/test_debugger.cpp`:

- **StdioIO creation** — verify `CreateStdioIO()` returns non-null.
- **DbgIO interface through a mock** — create a `TestIO` subclass that
  captures all `write()` output into a `std::string` and feeds
  `readLine()` from a pre-loaded buffer.  Verify:
  - `write("hello %d", 42)` → captured output is `"hello 42"`.
  - `readLine()` returns the pre-loaded line.
  - `endResponse()` can be called without error.

This `TestIO` mock will also be useful for testing command handlers
in isolation in a future phase.

### Fence

- [ ] `src/debugger/dbg_io.h` exists with `DbgIO` abstract class
- [ ] `src/debugger/dbg_io.cpp` exists with `StdioIO` implementation
- [ ] `Debugger::create()` accepts optional `DbgIO` parameter
- [ ] `Debugger::io()` accessor exists
- [ ] All `std::printf` calls in debugger sources replaced with `io().write()`
- [ ] All `std::fgets` calls replaced with `io_->readLine()`
- [ ] No behaviour change — `--debugger` works identically to before
- [ ] Unit tests pass: `./bld/macos/tests --test-case="dbgio*"`
- [ ] Full build clean
- [ ] Commit: `"debugger: phase 15 — I/O abstraction (DbgIO interface)"`

---

## Phase 16 — SocketIO and Debug Server Mode

Implement the Unix domain socket transport (`SocketIO`) and the
`--debugserver` flag.  After this phase, the emulator can listen for
debugger connections.  The client comes in Phase 17.

See Design §16.3, §17.

### 16.1 — Implement `SocketIO` in `dbg_io.cpp`

Add the `SocketIO` class below `StdioIO` in `dbg_io.cpp`:

```cpp
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

class SocketIO final : public DbgIO {
public:
    explicit SocketIO(int listenFd);
    ~SocketIO() override;

    bool readLine(char *buf, size_t len) override;
    void write(const char *fmt, ...) override;
    void endResponse() override;
    void flush() override {}

    bool acceptClient();
    void closeClient();

private:
    int listenFd_ = -1;
    int clientFd_ = -1;
    char recvBuf_[4096] {};
    size_t recvPos_ = 0;
    size_t recvLen_ = 0;
};
```

Key implementation details:

- `readLine()`: recv bytes into `recvBuf_`, scan for `'\n'`, copy the
  line into `buf`.  Handles partial reads (TCP-style, though sockets
  are local).  Returns false on client disconnect.
- `write()`: `vsnprintf` into a stack buffer, then `send()` the
  formatted bytes.  For large outputs (>4096), use a heap buffer.
- `endResponse()`: `send` the 2-byte sequence `"\x04\n"`.
- `acceptClient()`: `accept()` on `listenFd_`, store in `clientFd_`.
  Blocks until a client connects.
- `closeClient()`: `close(clientFd_)`, reset `clientFd_ = -1` and
  clear `recvBuf_` state.
- Destructor: close both fds if open.

Add factory function in `dbg_io.h`:

```cpp
std::unique_ptr<DbgIO> CreateSocketIO(int listenFd);
```

### 16.2 — Add `CreateListenSocket()` helper

In `dbg_io.cpp`, add a free function:

```cpp
int CreateListenSocket(const std::string &path);
```

Declared in `dbg_io.h`.  Implementation:

1. `socket(AF_UNIX, SOCK_STREAM, 0)`.
2. `unlink(path.c_str())` — remove stale socket (ignore errors).
3. Fill `struct sockaddr_un`, `bind()`, `listen(1)`.
4. Return the fd.  On error, print to stderr and return -1.

At-exit cleanup: register `atexit()` callback that `unlink`s the
socket path.  Store the path in a file-scope `static std::string`.

### 16.3 — Add `--debugserver` flag to config_loader

In `config_loader.h`, add to `LaunchConfig`:

```cpp
std::string debugServerPath;  // empty = not enabled
```

In `config_loader.cpp`, parse:

```cpp
if (strcmp(arg, "--debugserver") == 0)
{
    lc.debugServerPath = "auto";
    continue;
}
if (strncmp(arg, "--debugserver=", 14) == 0)
{
    lc.debugServerPath = arg + 14;
    continue;
}
```

Add to help text:

```
  --debugserver[=PATH] Start debug server on Unix socket (default /tmp/maxivmac-dbg-<PID>.sock)
```

Add mutual exclusion check:

```cpp
if (lc.debugger && !lc.debugServerPath.empty()) {
    std::fprintf(stderr, "Error: --debugger and --debugserver are mutually exclusive\n");
    std::exit(1);
}
```

### 16.4 — Wire `--debugserver` into startup

In `main.cpp` `ProgramEarlyInit()`, extend the debugger initialization:

```cpp
if (s_launchConfig.debugger)
{
    Debugger::create();  // uses default StdioIO
    g_debuggerActive = true;
}
else if (!s_launchConfig.debugServerPath.empty())
{
    auto path = s_launchConfig.debugServerPath;
    if (path == "auto")
        path = "/tmp/maxivmac-dbg-" + std::to_string(getpid()) + ".sock";
    int listenFd = CreateListenSocket(path);
    if (listenFd < 0) std::exit(1);
    std::fprintf(stderr, "debugserver: listening on %s\n", path.c_str());
    Debugger::create(CreateSocketIO(listenFd));
    g_debuggerActive = true;
}
```

### 16.5 — Adapt command loop for socket mode

The `commandLoop()` already calls `io_->readLine()` after Phase 15.
Two additions:

1. **First entry**: when using `SocketIO`, call `acceptClient()`
   before the first `readLine()`.  Add a flag `clientConnected_` to
   `Impl` and check it at the top of `commandLoop()`.

2. **Client disconnect**: when `readLine()` returns false in socket
   mode, don't exit — call `closeClient()` and `acceptClient()` to
   wait for the next client.  In stdio mode, false still means exit.

Add a virtual method to `DbgIO` or a query method:

```cpp
// In DbgIO:
virtual bool isSocket() const { return false; }

// In SocketIO:
bool isSocket() const override { return true; }
```

### 16.6 — Tests

Add to `test/test_debugger.cpp`:

- **CreateListenSocket** — create a socket in `/tmp`, verify the file
  exists, close it, verify cleanup (manually call unlink).
- **SocketIO round-trip** — use `socketpair(AF_UNIX, SOCK_STREAM, 0)`
  to create a connected pair.  Wrap one end in `SocketIO` (pretending
  it's the accepted client fd — call an internal setter or use a test
  constructor).  From the other end:
  - Send `"step 5\n"` → verify `readLine()` returns `"step 5\n"`.
  - Call `write("hello %d\n", 42)` → recv from the other end, verify
    `"hello 42\n"`.
  - Call `endResponse()` → recv, verify `"\x04\n"`.
- **EOT framing** — verify multi-line response followed by EOT parses
  correctly on the recv side.

### Fence

- [ ] `SocketIO` implemented in `dbg_io.cpp`
- [ ] `CreateListenSocket()` implemented and tested
- [ ] `--debugserver` parsed in config_loader
- [ ] Mutual exclusion with `--debugger` enforced
- [ ] Server startup creates socket and prints path to stderr
- [ ] Command loop handles client connect/disconnect in socket mode
- [ ] Unit tests pass: `./bld/macos/tests --test-case="socket*"`
- [ ] Full build clean
- [ ] Commit: `"debugger: phase 16 — SocketIO and debug server mode"`

---

## Phase 17 — Client Mode (`maxivmac debug`)

Implement the `maxivmac debug` subcommand — the thin client that
connects to a running debug server and sends commands.

See Design §18.

### 17.1 — Create `src/debugger/dbg_client.cpp`

Declare the entry point in `dbg_io.h` (or a new `dbg_client.h` —
but since it's only called from `main.cpp`, a declaration in `dbg_io.h`
avoids a new header):

```cpp
// In dbg_io.h:
int DebugClientMain(int argc, char *argv[]);
```

Implement in `dbg_client.cpp` (~80 lines):

```cpp
#include "debugger/dbg_io.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <glob.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

// --- Socket auto-discovery ---

static std::string FindSocket()
{
    glob_t g;
    if (glob("/tmp/maxivmac-dbg-*.sock", 0, nullptr, &g) != 0)
    {
        std::fprintf(stderr, "No debug server found.\n");
        return {};
    }
    if (g.gl_pathc == 1)
    {
        std::string result = g.gl_pathv[0];
        globfree(&g);
        return result;
    }
    std::fprintf(stderr, "Multiple debug servers found:\n");
    for (size_t i = 0; i < g.gl_pathc; ++i)
        std::fprintf(stderr, "  %s\n", g.gl_pathv[i]);
    std::fprintf(stderr, "Use --socket=PATH to select one.\n");
    globfree(&g);
    return {};
}

// --- Connect to socket ---

static int ConnectToSocket(const std::string &path)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    struct sockaddr_un addr {};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(fd, reinterpret_cast<struct sockaddr *>(&addr),
                sizeof(addr)) < 0)
    {
        perror("connect");
        close(fd);
        return -1;
    }
    return fd;
}

// --- Receive until EOT ---

static bool RecvResponse(int fd)
{
    char buf[4096];
    for (;;)
    {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) return false;
        for (ssize_t i = 0; i < n; ++i)
        {
            if (buf[i] == '\x04') return true;
            std::putchar(buf[i]);
        }
    }
}

// --- Entry point ---

int DebugClientMain(int argc, char *argv[])
{
    std::string socketPath;
    const char *command = nullptr;

    // Parse args: [--socket=PATH] [command]
    for (int i = 1; i < argc; ++i)
    {
        if (std::strncmp(argv[i], "--socket=", 9) == 0)
            socketPath = argv[i] + 9;
        else
            command = argv[i];
    }

    if (socketPath.empty())
        socketPath = FindSocket();
    if (socketPath.empty())
        return 1;

    int fd = ConnectToSocket(socketPath);
    if (fd < 0) return 1;

    if (command)
    {
        // One-shot mode
        std::string msg = std::string(command) + "\n";
        send(fd, msg.data(), msg.size(), 0);
        RecvResponse(fd);
        close(fd);
        return 0;
    }

    // Interactive mode
    char line[1024];
    for (;;)
    {
        std::printf("(dbg) ");
        std::fflush(stdout);
        if (!std::fgets(line, sizeof(line), stdin))
            break;
        size_t len = std::strlen(line);
        if (len == 0) continue;
        send(fd, line, len, 0);
        if (!RecvResponse(fd)) break;
    }
    close(fd);
    return 0;
}
```

### 17.2 — Intercept `debug` subcommand in `main.cpp`

In `ProgramEarlyInit()`, add as the very first thing:

```cpp
if (argc >= 2 && std::strcmp(argv[1], "debug") == 0)
{
    std::exit(DebugClientMain(argc - 1, argv + 1));
}
```

This runs before `ParseCommandLine()` — zero emulator initialization.

Add `#include "debugger/dbg_io.h"` at the top of `main.cpp` (the
header already declares `DebugClientMain`).

### 17.3 — Update CMakeLists.txt

Add `src/debugger/dbg_client.cpp` to the debugger sources list
(if not already added in Phase 16).

### 17.4 — Tests

Add to `test/test_debugger.cpp`:

- **FindSocket** — create a temp file `/tmp/maxivmac-dbg-test.sock`,
  verify `FindSocket()` finds it.  Clean up with `unlink`.
- **ConnectToSocket** — use a `socketpair`, bind one end to a temp
  path, verify connect from the other end succeeds.

The full integration test is in Phase 18.

### Fence

- [ ] `src/debugger/dbg_client.cpp` exists with `DebugClientMain`
- [ ] `argv[1] == "debug"` intercepted in `ProgramEarlyInit()`
- [ ] `maxivmac debug --help` does not crash or start the emulator
- [ ] One-shot mode sends command and prints response
- [ ] Interactive mode loops on readline
- [ ] Unit tests pass
- [ ] Full build clean
- [ ] Commit: `"debugger: phase 17 — client mode (maxivmac debug)"`

---

## Phase 18 — Debug Server Integration Test

End-to-end test that starts the emulator in `--debugserver` mode,
sends commands via `maxivmac debug`, and verifies responses.

### 18.1 — Extend `test/debugger_smoke.sh`

Add a new section to the existing smoke test script:

```bash
# --- Debug Server Tests ---

echo "=== Debug server tests ==="

# Start emulator in server mode (headless, no disk needed for basic test)
$MAXIVMAC --debugserver --model MacPlus --headless 608.hfs &
SERVER_PID=$!

# Wait for socket to appear
SOCK="/tmp/maxivmac-dbg-${SERVER_PID}.sock"
for i in $(seq 1 30); do
    [ -S "$SOCK" ] && break
    sleep 0.1
done

if [ ! -S "$SOCK" ]; then
    echo "FAIL: debug server socket did not appear"
    kill $SERVER_PID 2>/dev/null
    exit 1
fi

# Test 1: one-shot info reg
$MAXIVMAC debug --socket="$SOCK" "info reg" | grep -q "D0="
echo "  [PASS] info reg"

# Test 2: step and verify PC changes
PC1=$($MAXIVMAC debug --socket="$SOCK" "print pc" | head -1)
$MAXIVMAC debug --socket="$SOCK" "step"
PC2=$($MAXIVMAC debug --socket="$SOCK" "print pc" | head -1)
if [ "$PC1" != "$PC2" ]; then
    echo "  [PASS] step advances PC"
else
    echo "  [FAIL] PC did not change after step"
    kill $SERVER_PID 2>/dev/null
    exit 1
fi

# Test 3: set and read back a breakpoint
$MAXIVMAC debug --socket="$SOCK" "break \$400000" | grep -q "Breakpoint"
echo "  [PASS] break command"

$MAXIVMAC debug --socket="$SOCK" "info break" | grep -q "400000"
echo "  [PASS] info break"

# Test 4: quit
$MAXIVMAC debug --socket="$SOCK" "quit"
wait $SERVER_PID 2>/dev/null

# Verify socket cleaned up
if [ -S "$SOCK" ]; then
    echo "  [WARN] socket not cleaned up"
    rm -f "$SOCK"
fi

echo "All debug server tests passed."
```

### 18.2 — Verify auto-discovery

Add a test that verifies `maxivmac debug` (without `--socket`) finds
the running server:

```bash
# Start server, then use auto-discovery
$MAXIVMAC --debugserver --model MacPlus --headless 608.hfs &
SERVER_PID=$!
SOCK="/tmp/maxivmac-dbg-${SERVER_PID}.sock"
for i in $(seq 1 30); do [ -S "$SOCK" ] && break; sleep 0.1; done

# Auto-discover (should find exactly one server)
$MAXIVMAC debug "info reg" | grep -q "D0="
echo "  [PASS] auto-discovery"

$MAXIVMAC debug "quit"
wait $SERVER_PID 2>/dev/null
```

### 18.3 — Clean up stale sockets

Add a cleanup trap at the top of the smoke test:

```bash
cleanup() {
    rm -f /tmp/maxivmac-dbg-*.sock
    kill $SERVER_PID 2>/dev/null || true
}
trap cleanup EXIT
```

### Fence

- [ ] `test/debugger_smoke.sh` has debug server tests
- [ ] Server starts, accepts connections, responds to commands
- [ ] One-shot mode works: send command, get response, exit
- [ ] Auto-discovery works when exactly one server is running
- [ ] Socket is cleaned up on quit
- [ ] All smoke tests pass
- [ ] Full build clean
- [ ] Commit: `"debugger: phase 18 — debug server integration test"`
