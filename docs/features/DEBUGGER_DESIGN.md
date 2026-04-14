# Debugger — Detailed Design

Implements the specification in [DEBUGGER.md](DEBUGGER.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

All debugger code lives in `src/debugger/`.  The rest of the codebase
sees only a single header with a narrow interface.

```
src/debugger/
    debugger.h          # Public interface — the only header included outside
    debugger.cpp        # Debugger object, CPU/trap hooks, command dispatch
    dbg_io.h            # I/O abstraction: DbgIO base, StdioIO, SocketIO
    dbg_io.cpp          # StdioIO and SocketIO implementations
    dbg_client.cpp      # Client-mode entry point (maxivmac debug)
    cmd_parser.cpp      # Line tokenizer and argument parsing
    cmd_parser.h
    cmd_exec.cpp        # Execution commands (run/step/next/finish/until)
    cmd_break.cpp       # Breakpoints and watchpoints
    cmd_memory.cpp      # x, print, set, info reg
    cmd_trace.cpp       # trace traps/insn/io
    cmd_info.cpp        # info break/traps/globals/symbol/insn, backtrace
    cmd_help.cpp        # help text table
    symbols.cpp         # Symbol tables (traps + low-memory globals)
    symbols.h
    expr.cpp            # Condition/expression evaluator
    expr.h
```

The split is by command group so each file stays small (~200–400 lines).
Every `cmd_*.cpp` file includes `debugger.h` and accesses the Debugger
object through a module-internal pointer — no new globals leak out.

---

## 2. The Debugger Object

```cpp
// src/debugger/debugger.h
#pragma once
#include <cstdint>
#include <string_view>

class Debugger {
public:
    // --- Lifecycle (called from main.cpp) ---
    static Debugger *instance();          // singleton, nullptr when --debugger not passed
    static void create();                 // allocate + init (called once)

    // --- CPU-loop hooks (called from m68k.cpp) ---

    // Called at the top of every instruction, before execution.
    // Returns true if the CPU should stop (breakpoint hit, step done).
    // When it returns true, enters the command loop; returns when the
    // user issues run/step/continue.
    //
    // This is the ONLY function called per instruction.
    bool instructionHook(uint32_t pc);

    // Called from DoCodeA() when an A-line trap is about to dispatch.
    // Returns true if a trap breakpoint matched (caller must then
    // call instructionHook to enter the command loop).
    bool trapHook(uint16_t trapWord);

    // --- Memory hooks (called from get/put_byte_ext, get/put_word_ext) ---

    // Check whether addr is being watched.  Called only on the slow
    // path (_ext functions), so zero cost on the fast path.
    // direction: 'R' for read, 'W' for write.
    // Returns true if a watchpoint fired.
    bool memoryHook(uint32_t addr, uint32_t size, char direction,
                    uint32_t oldVal, uint32_t newVal);

    // --- State query (used by cmd_*.cpp, not by external code) ---
    // (listed here for clarity; actual interface is richer)
    bool isRunning() const;
    bool isStopped() const;
    void stop(std::string_view reason);    // force stop + print reason

private:
    Debugger();
    void commandLoop();                   // read-eval-print loop
    // ... internal state, see §4
};

// --- Fast-path gate (one global boolean) ---

// True when the debugger is active (--debugger was passed).
// When false, every hook compiles to a single branch-not-taken.
extern bool g_debuggerActive;
```

### Why a singleton?

There is exactly one CPU.  A singleton avoids passing a `Debugger*`
through every call chain.  The `g_debuggerActive` flag makes the
singleton check free — the flag is tested first, and only when it's
true does `Debugger::instance()` get called.

---

## 3. Integration Points

The entire debugger touches the rest of the codebase in exactly **five
places**.  Each is a 2–4 line insertion guarded by the `g_debuggerActive`
flag.

### 3.1  CPU instruction loop — `m68k.cpp` `m68k_go_MaxCycles()`

Insert at the top of the `do { … } while` loop, right after the
existing logging block and before the `d()` call:

```cpp
// --- existing code (instruction logging, StateRecorder, g_instructionCount++) ---

// NEW: debugger hook
if (g_debuggerActive) {
    if (Debugger::instance()->instructionHook(pc)) {
        // instructionHook ran the command loop and returned.
        // The user issued run/step/continue.
        // We fall through to execute the current instruction.
    }
}

// --- existing code: d(); ---
```

**Cost when debugger is off**: one branch-not-taken on `g_debuggerActive`
(a global bool, likely in the same cache line as `g_instructionCount`).

**Cost when debugger is on and running**: `instructionHook` checks the
breakpoint hash set.  If no breakpoints exist, it checks only the
step-counter (one integer decrement + compare).

### 3.2  Trap dispatch — `m68k.cpp` `DoCodeA()`

Insert after `trap_counter_record(tw)`:

```cpp
static void DoCodeA()
{
    BackupPC();
    uint16_t tw = do_get_mem_word(V_pc_p);
    trap_counter_record(tw);

    // NEW: debugger trap hook
    if (g_debuggerActive && Debugger::instance()->trapHook(tw)) {
        // trapHook returned true — a trap breakpoint matched.
        // Force the instruction-level hook to fire on this instruction
        // so the command loop runs.
    }

    g_tracer.enter(tw);
    Exception(0xA);
}
```

`trapHook` is a hash lookup on the trap word — O(1), small table.

### 3.3  Memory slow path — `m68k.cpp` `get_byte_ext()` / `put_byte_ext()` (and `_word_ext` variants)

Insert at the point where the data value is known, just before
returning.  Example for `get_byte_ext`:

```cpp
static uint32_t get_byte_ext(uint32_t addr)
{
    // ... existing ATT lookup, data read ...

    // NEW: watchpoint check
    if (g_debuggerActive) {
        Debugger::instance()->memoryHook(addr, 1, 'R', Data, Data);
    }

    return ...;
}
```

For write paths, `oldVal` is read before the write and `newVal` after.
The watchpoint list is tiny (typically <10 entries), so a linear scan
is fine here — this code only runs on MATC cache misses.

### 3.4  Startup — `main.cpp` `ProgramEarlyInit()`

```cpp
if (s_launchConfig.debugger) {
    Debugger::create();
    g_debuggerActive = true;
}
```

### 3.5  CLI parsing — `config_loader.cpp`

Add `--debugger` flag to `LaunchConfig` and the argument parser.

```cpp
// In LaunchConfig:
bool debugger = false;

// In ParseCommandLine:
else if (arg == "--debugger") { config.debugger = true; }
```

**That's it.**  Five surgical insertions.  No other file needs to
include `debugger.h`.

---

## 4. Internal State

```cpp
// Inside Debugger (private)

enum class State { Stopped, Running, Stepping };

State          state_ = State::Stopped;
uint32_t       stepsRemaining_ = 0;       // for step/next

// Breakpoints
struct Breakpoint {
    uint32_t    id;
    bool        enabled;
    uint32_t    address;                   // PC address (0 if trap-only)
    uint16_t    trapWord;                  // non-zero for trap breakpoints
    std::string condition;                 // parsed on hit
    std::vector<std::string> commands;     // auto-execute on hit
};

uint32_t nextBpId_ = 1;
std::unordered_map<uint32_t, Breakpoint>  bpByAddr_;   // PC → bp
std::unordered_map<uint16_t, Breakpoint>  bpByTrap_;   // trap word → bp
std::unordered_map<uint32_t, Breakpoint*> bpById_;     // id → bp (for delete/enable/disable)

// Watchpoints
struct Watchpoint {
    uint32_t    id;
    bool        enabled;
    uint32_t    address;
    uint32_t    length;
    char        mode;                      // 'W' write, 'R' read, 'A' access
    bool        hasValCond;                // true if "if val <op> <v>" was specified
    uint8_t     valCondOp;                 // comparison operator enum
    uint32_t    valCondValue;              // right-hand side of the condition
    uint8_t     shadow[];                  // copy of watched bytes for old-value display
};

std::vector<Watchpoint>  watchpoints_;     // small, linear scan ok

// Finish / Next tracking
uint32_t       savedSP_ = 0;              // for finish
bool           finishing_ = false;
bool           nexting_ = false;

// Tracing flags
bool           traceTraps_ = false;
bool           traceInsn_  = false;
bool           traceIO_    = false;
std::unordered_set<uint16_t> trapFilter_; // empty = trace all
```

---

## 5. Key Algorithms

### 5.1  `instructionHook(pc)` — the hot path

```
if state == Stopped:
    commandLoop()        # blocks until user types run/step/continue
    return true          # first instruction after resume

if state == Stepping:
    if --stepsRemaining_ == 0:
        stop("step completed")
        return true

if finishing_:
    if currentSP >= savedSP_ and instrIsReturn(pc):
        finishing_ = false
        stop("finish completed")
        return true

if nexting_:
    if currentSP >= savedSP_:
        nexting_ = false
        stop("next completed")
        return true

if bpByAddr_ contains pc:
    bp = bpByAddr_[pc]
    if bp.enabled and evalCondition(bp.condition):
        stop("Breakpoint N hit at $XXXX")
        executeCommands(bp.commands)
        if state == Stopped:       # commands didn't resume
            return true

if traceInsn_:
    printInstructionLog(pc)

return false                       # keep running
```

### 5.2  `trapHook(trapWord)`

```
if bpByTrap_ contains trapWord:
    bp = bpByTrap_[trapWord]
    if bp.enabled:
        stop("Breakpoint N hit: TrapName ($XXXX)")
        # The command loop runs inside instructionHook on the next call
        return true

if traceTraps_ and (trapFilter_ empty or contains trapWord):
    print "[TRAP] $XXXX TrapName  pc=$XXXX sp=$XXXX"

return false
```

### 5.3  `memoryHook(addr, size, direction, oldVal, newVal)`

```
for each wp in watchpoints_:
    if not wp.enabled: continue
    if ranges overlap (addr..addr+size) and (wp.address..wp.address+wp.length):
        if direction matches wp.mode:
            if wp.hasValCond:
                # compare newVal (for writes) or oldVal (for reads)
                # against wp.valCondValue using wp.valCondOp
                if not evalOp(wp.valCondOp, newVal, wp.valCondValue):
                    continue   # value doesn't match, skip
            stop("Watchpoint N hit")
            print old/new values
            return true
return false
```

### 5.4  `finish` implementation

When the user types `finish`:
1. Record current SP = `m68k_areg(7)`
2. Set `finishing_ = true`
3. Set state = Running

`instructionHook` checks: if `finishing_` and current SP ≥ savedSP_,
peek at the opcode at PC.  If it's RTS (`$4E75`), RTD (`$4E74`),
or RTE (`$4E73`), stop.

For trap calls, `finish` is equivalent: the ROM dispatcher will
eventually RTS back to the caller, at which point SP ≥ savedSP_.

### 5.5  `next [N]` implementation

When the user types `next [N]` (default N=1):
1. Set `nextsRemaining_ = N`
2. Read the opcode at PC
3. If it's BSR, JSR, or an A-line trap:
   - Record saved SP
   - Set `nexting_ = true`, state = Running
4. Otherwise:
   - Execute a single `step` (decrement `nextsRemaining_`)
   - If `nextsRemaining_ > 0`, repeat from step 2

In `instructionHook`, when `nexting_` and SP ≥ savedSP_:
- Decrement `nextsRemaining_`
- If `nextsRemaining_ > 0`, start the next "next" iteration (step 2)
- If `nextsRemaining_ == 0`, stop

### 5.6  Breakpoint commands

When a breakpoint with commands fires:
1. Stop and print the breakpoint hit message
2. Feed each command string into the command dispatcher, in order
3. Commands like `trace insn on`, `print`, `info reg` execute immediately
4. Commands like `finish`, `continue` change the state to Running and
   return — the remaining commands are deferred until the next stop
5. `end` is not a real command; it terminates the `commands` input
   during interactive entry

For the `break GetResource` + `trace insn on` + `finish` +
`trace insn off` + `continue` pattern:
- Breakpoint fires → print message
- `trace insn on` → set flag, continue to next command
- `finish` → set finishing_ + Running, save remaining commands
- CPU runs with instruction logging until finish completes
- Finish stop → execute remaining: `trace insn off`, `continue`
- `trace insn off` → clear flag
- `continue` → set Running, no more commands

---

## 6. Command Parser

`cmd_parser.cpp` implements a simple tokenizer:

```cpp
struct Token {
    enum class Kind { Word, Number, Operator, End };
    Kind kind;
    std::string text;
    uint32_t numValue;     // valid when kind == Number
};

std::vector<Token> Tokenize(std::string_view line);
```

Tokens are split on whitespace.  Numbers accept `$hex`, `0xHex`, and
plain decimal.  The parser recognizes the shortest unambiguous prefix
for each command.

Command dispatch is a table:

```cpp
struct CmdEntry {
    std::string_view name;      // full name
    std::string_view shortcut;  // abbreviation (empty if none)
    void (*handler)(Debugger &dbg, const std::vector<Token> &args);
    std::string_view helpBrief;
    std::string_view helpFull;
};

static const CmdEntry s_commands[] = {
    {"run",      "r",   CmdRun,      "Start/resume execution", ...},
    {"continue", "c",   CmdContinue, "Resume after stop", ...},
    {"step",     "s",   CmdStep,     "Execute N instructions", ...},
    {"next",     "n",   CmdNext,     "Step over calls", ...},
    {"finish",   "fin", CmdFinish,   "Run until subroutine returns", ...},
    {"break",    "b",   CmdBreak,    "Set breakpoint", ...},
    {"delete",   "d",   CmdDelete,   "Delete breakpoint", ...},
    ...
};
```

Dispatch: find the unique entry whose name or shortcut matches the
first token as a prefix.  If ambiguous, print candidates.

---

## 7. Symbol Tables

`symbols.cpp` provides two sorted arrays built at startup:

### 7.1  Trap symbols

Loaded from the existing `trap_dict_*()` API in `trap_counter.h`.
681 entries.  Each has `trapWord` and `name`.

Lookup: by name (binary search on sorted name array) or by trap word
(hash map from `trap_counter.cpp`'s reverse index).

### 7.2  Low-memory globals

Reuses the existing `kLowMemGlobals[]` / `kLowMemCount` table from
`src/platform/lomem_globals.h`.  This is the same ~160-entry table
used by the ImGui `LowMemTool` panel — each entry has name, address,
size, type, and category.

`symbols.cpp` builds two indexes at startup from the existing table:
- **By name** — sorted array of pointers into `kLowMemGlobals[]` for
  binary search and prefix matching (`info globals`)
- **By address** — sorted for reverse lookup (`info symbol`)

No data is duplicated.  If `lomem_globals.cpp` gains new entries, the
debugger picks them up automatically.

Note: `lomem_globals.h` currently lives under `src/platform/`.  Since
it has no platform dependencies (pure data), it can be included from
`src/debugger/` as-is.  If this cross-directory include bothers us
later, the table can be moved to `src/core/` — but that's cosmetic.

Lookup: by name (binary search) or by address (second index, also
sorted, for `info symbol`).

### 7.3  Unified resolution

When the user types a location (in `break`, `watch`, `x`, `print`),
the resolver tries in order:
1. Parse as number (`$hex`, `0xhex`, decimal)
2. Look up as trap name → return trap word (for `break`) or error
3. Look up as low-memory global name → return address
4. Look up as register name → return register value

---

## 8. Expression Evaluator

`expr.cpp` handles `print <expr>` and `break ... if <cond>`.

### Value expressions (for `print`)

```
expr     → unary (('+' | '-') unary)*
unary    → '(' expr ')'        # dereference: read long at address
         | atom
atom     → register             # d0-d7, a0-a7, pc, sr, sp
         | '$' hexdigits        # hex literal
         | '0x' hexdigits
         | digits               # decimal
         | globalName           # resolved to address, then read value
```

Parentheses dereference: `(a0)` = read long at the value of A0.
`(a0 + 4)` = read long at A0+4.

### Condition expressions (for breakpoint conditions)

```
condition → comparison ('&&' comparison)*
comparison → value op value
op        → '==' | '!=' | '<' | '>' | '<=' | '>=' | '&'
```

`&` is bitwise AND, true if nonzero:  `sr & $2000` checks supervisor
bit.

---

## 9. Watchpoint MATC Invalidation

When a watchpoint is set on a RAM address:

1. Walk the ATT list to find the entry covering that address
2. Remember its original `Access` flags
3. Clear `kATTA_readreadybit` and/or `kATTA_writereadybit` from
   `Access`, set `kATTA_ntfybit` instead
4. Invalidate all MATC cache entries (set cmpmask/cmpvalu to impossible
   values so every access goes through `_ext`)

This forces all accesses to the watched address range through the slow
path where `memoryHook` runs.

When the watchpoint is deleted, restore the original `Access` flags
and let the MATC repopulate naturally.

**Scoping**: Only the ATT entry covering the watched range is modified.
All other RAM regions keep their fast-path flags.  However, the MATC
is global (one entry per access type), so a cache invalidation affects
all addresses temporarily — the first access to any address repopulates
the cache.  This is the same mechanism `LocalMemAccessNtfy` already
uses.

---

## 10.  Stopping the CPU Loop

The CPU loop in `m68k_go_MaxCycles` runs until `V_MaxCyclesToGo <= 0`.
When the debugger needs to stop (breakpoint hit, step done), it calls
`commandLoop()` directly from inside `instructionHook()` — which is
inside the `do { } while` loop.

This means `commandLoop()` **blocks** the CPU loop.  This is correct:

- The emulator is single-threaded
- All debugger I/O is synchronous (stdin/stdout)
- No emulated time passes while the command loop is active
- When the user types `continue`, `commandLoop` returns, and the
  CPU loop resumes exactly where it left off

For Ctrl-C handling: install a SIGINT handler that sets a flag.
`instructionHook` checks this flag and calls `stop("interrupted")`
when set.

---

## 10b.  Memory Access Paths — CPU Independence

The MATC fast path → `_ext` slow path dispatch (`get_byte` →
`get_byte_ext`, etc.) is **not** conditional on `use68020`.  All CPU
variants (68000, 68020) use the identical memory access code.  The
`use68020` flag only affects exception frame format, MOVEM pre-
decrement order, privileged instructions, and VBR — never the memory
path.  Watchpoints based on `_ext` interception will work on every
emulated CPU model.

---

## 11.  Integration with Existing Logging

The debugger reuses the existing trap tracing subsystem, not a
parallel implementation.

**Output stream:** All trace and log output goes to **stdout** —
always, not just in debugger mode.  maxivmac is a GUI app; stdout is
otherwise unused.  This applies to `trap_trace_log`, `TrapTracer`,
instruction logging (`--log-start`), and I/O logging.  The existing
`fprintf(stderr, ...)` calls in `trap_counter.cpp`, `trap_tracer.cpp`,
and `m68k.cpp` should be changed to `fprintf(stdout, ...)`.  stderr
remains for actual errors (ROM load failure, assertion messages, etc.).

**`trace traps`** calls `BeginTraceTraps()` / `EndTraceTraps()` to
enable the existing reference-counted trace system.  The debugger's
trap filter set (`trapFilter_`) is checked by a thin wrapper around
`trap_trace_log` — when the filter is non-empty, only matching traps
are printed.

**`trace insn`** reuses the same `fprintf` format string as the
existing `--log-start/--log-count` mechanism, but controlled by a
boolean flag (`traceInsn_`) instead of a range.  No duplication of
the format string.

**`trace io`** hooks into the same `IOR`/`IOW` logging path that
the instruction logger uses, controlled by `traceIO_`.

---

## 12.  Build Integration

### CMakeLists.txt

```cmake
# src/debugger/
file(GLOB DEBUGGER_SOURCES src/debugger/*.cpp)
target_sources(maxivmac PRIVATE ${DEBUGGER_SOURCES})
target_include_directories(maxivmac PRIVATE src)
```

The debugger files only depend on:
- `src/debugger/debugger.h` (their own public header)
- `src/cpu/m68k.h` (register accessors, memory access)
- `src/cpu/trap_counter.h` (trap dictionary)
- `src/cpu/disasm.h` (disassembly for `x/i`)
- `<cstdio>`, `<cstdint>`, `<string>`, `<vector>`, `<unordered_map>`

No dependency on platform code, device code, or ImGui.

---

## 13.  Dependency Diagram

```
                ┌──────────────────────────────┐
                │     m68k.cpp  (CPU loop)     │
                │                              │
                │  if (g_debuggerActive)        │
                │    dbg->instructionHook(pc)   │
                │    dbg->trapHook(tw)          │
                │    dbg->memoryHook(...)       │
                └──────────┬───────────────────┘
                           │ calls (via debugger.h)
                           ▼
                ┌──────────────────────────────┐
                │     src/debugger/            │
                │                              │
                │  debugger.cpp ◄── cmd_*.cpp  │
                │        │                     │
                │        ├─► symbols.cpp       │
                │        ├─► expr.cpp          │
                │        └─► cmd_parser.cpp    │
                └──────────┬───────────────────┘
                           │ reads (via existing headers)
                           ▼
                ┌──────────────────────────────┐
                │  m68k.h    (regs, memory)    │
                │  trap_counter.h  (trap dict) │
                │  disasm.h  (disassembly)     │
                └──────────────────────────────┘
```

Arrows point in one direction.  The CPU loop calls into the debugger.
The debugger reads CPU state through the existing public API.
No circular dependencies.

---

## 14.  File Summary

| File | Lines (est.) | Responsibility |
|------|-------------|----------------|
| `debugger.h` | 40 | Public interface: hooks + singleton |
| `debugger.cpp` | 250 | Object, command loop, hook dispatch |
| `cmd_parser.h/cpp` | 100 | Tokenizer, dispatch table |
| `cmd_exec.cpp` | 150 | run, continue, step, next, finish, until |
| `cmd_break.cpp` | 200 | break, delete, disable, enable, commands |
| `cmd_memory.cpp` | 250 | x, set *, find, print, set, info reg |
| `cmd_trace.cpp` | 80 | trace traps/insn/io |
| `cmd_info.cpp` | 150 | info break/traps/globals/symbol, backtrace |
| `cmd_help.cpp` | 100 | help text |
| `symbols.h/cpp` | 250 | Trap + global tables, unified resolver |
| `expr.h/cpp` | 150 | Expression + condition evaluator |
| **Total** | **~1700** | |

---

## 15.  Testing

The debugger should have unit tests under `test/debugger/`.  Key areas:

- **Command parser** — tokenization, prefix matching, ambiguity
  detection.  Pure string-in / tokens-out, no emulator state needed.
- **Expression evaluator** — arithmetic, dereference, register names,
  global names, condition operators.  Mock register/memory accessors.
- **Symbol resolver** — trap name lookup, global name lookup, address
  reverse lookup, prefix search.
- **Breakpoint/watchpoint management** — add, delete, enable, disable,
  lookup by address/id/trap, condition attachment.

These are all self-contained data structures that can be tested without
booting the emulator.  Use the existing doctest framework.

---

## 16.  I/O Abstraction (DbgIO)

All debugger I/O (`std::printf` for output, `std::fgets` for input)
must be routed through a polymorphic interface so the same command
handlers work with both the interactive stdin/stdout mode and the
Unix-socket server mode.

### 16.1  `DbgIO` base class

```cpp
// src/debugger/dbg_io.h
#pragma once
#include <cstdarg>
#include <string_view>

class DbgIO {
public:
    virtual ~DbgIO() = default;

    // Read one line of input (blocking).  Returns false on EOF/error.
    virtual bool readLine(char *buf, size_t len) = 0;

    // Formatted write — printf semantics.
    virtual void write(const char *fmt, ...) = 0;

    // Mark end of one command response (no-op for stdio; sends EOT
    // byte for socket mode).
    virtual void endResponse() = 0;

    // Flush the output stream.
    virtual void flush() = 0;
};
```

### 16.2  `StdioIO` — stdin/stdout transport

Used by `--debugger`.  Wraps `std::fgets(stdin)` and `std::printf`.
`endResponse()` is a no-op.  `flush()` calls `std::fflush(stdout)`.

```cpp
class StdioIO final : public DbgIO {
public:
    bool readLine(char *buf, size_t len) override;   // fgets(buf, len, stdin)
    void write(const char *fmt, ...) override;       // vprintf(fmt, ap)
    void endResponse() override {}                   // no-op
    void flush() override;                           // fflush(stdout)
};
```

### 16.3  `SocketIO` — Unix domain socket transport

Used by `--debugserver`.  Wraps `recv`/`send` on an accepted client
file descriptor.

```cpp
class SocketIO final : public DbgIO {
public:
    explicit SocketIO(int listenFd);
    ~SocketIO() override;

    bool readLine(char *buf, size_t len) override;   // recv until '\n'
    void write(const char *fmt, ...) override;       // send formatted
    void endResponse() override;                     // send '\x04\n'
    void flush() override {}                         // send is unbuffered

    // Accept a client connection (blocks).  Must be called before
    // readLine/write.  Returns false if the listen socket was closed.
    bool acceptClient();

    // Close the current client connection.
    void closeClient();

private:
    int listenFd_ = -1;
    int clientFd_ = -1;
    // Internal read buffer for line assembly from recv() chunks.
    char recvBuf_[4096] {};
    size_t recvPos_ = 0;
    size_t recvLen_ = 0;
};
```

**Socket setup** (done in `Debugger::create()` when `--debugserver`):

1. `socket(AF_UNIX, SOCK_STREAM, 0)` → `listenFd`.
2. Construct path: `/tmp/maxivmac-dbg-<PID>.sock` or the user-supplied
   path.
3. `unlink(path)` (remove any stale socket), `bind`, `listen(1)`.
4. Print `"debugserver: listening on <path>\n"` to stderr.
5. Register an `atexit()` handler to `unlink(path)` on exit.

**Per-command flow:**

1. `acceptClient()` blocks until a client connects.
2. Command loop: `readLine()` → dispatch → `write(...)` →
   `endResponse()` → loop.
3. On client EOF (`readLine` returns false): `closeClient()`,
   call `acceptClient()` to wait for the next client.

The command loop in `debugger.cpp` does not change structurally — it
calls `io_->readLine()` instead of `std::fgets(stdin)` and commands
call `io_->write()` instead of `std::printf()`.

### 16.4  Command handler migration

All `std::printf(...)` calls in `debugger.cpp` and `cmd_*.cpp` are
replaced with `io.write(...)`, where `io` is obtained via
`dbg.io()` (a new accessor).  This is a mechanical find-and-replace:

```cpp
// Before:
std::printf("Breakpoint %u at $%08X\n", id, addr);

// After:
dbg.io().write("Breakpoint %u at $%08X\n", id, addr);
```

`commandLoop()` changes:

```cpp
// Before:
std::printf("(dbg) ");
std::fflush(stdout);
if (!std::fgets(buf, sizeof(buf), stdin)) { ... }

// After:
io_->write("(dbg) ");
io_->flush();
if (!io_->readLine(buf, sizeof(buf))) { ... }
```

The `io()` accessor returns a `DbgIO&` reference stored in `Impl`:

```cpp
// In debugger.h:
DbgIO &io();

// In Debugger::Impl:
std::unique_ptr<DbgIO> io_;
```

---

## 17.  Debug Server Startup

### 17.1  `--debugserver` flag

Parsed in `config_loader.cpp`.  Added to `LaunchConfig`:

```cpp
std::string debugServerPath;  // empty = not enabled, "auto" = default path
```

`--debugserver` without `=PATH` sets `debugServerPath = "auto"`.
`--debugserver=/path/to/sock` sets the explicit path.

`--debugger` and `--debugserver` are mutually exclusive — if both are
passed, print an error and exit.

### 17.2  Initialization sequence

In `ProgramEarlyInit()`:

```cpp
if (s_launchConfig.debugger) {
    Debugger::create(std::make_unique<StdioIO>());
    g_debuggerActive = true;
} else if (!s_launchConfig.debugServerPath.empty()) {
    auto path = s_launchConfig.debugServerPath;
    if (path == "auto")
        path = "/tmp/maxivmac-dbg-" + std::to_string(getpid()) + ".sock";
    auto socketIO = std::make_unique<SocketIO>(createListenSocket(path));
    std::fprintf(stderr, "debugserver: listening on %s\n", path.c_str());
    Debugger::create(std::move(socketIO));
    g_debuggerActive = true;
}
```

`Debugger::create()` gains a `std::unique_ptr<DbgIO>` parameter.

---

## 18.  Client Mode (`maxivmac debug`)

### 18.1  Subcommand interception

In `ProgramEarlyInit()`, before `ParseCommandLine()`:

```cpp
if (argc >= 2 && std::strcmp(argv[1], "debug") == 0) {
    std::exit(DebugClientMain(argc - 1, argv + 1));
}
```

This early exit means zero emulator initialization occurs.  The
function `DebugClientMain` lives in `src/debugger/dbg_client.cpp`.

### 18.2  `DebugClientMain` implementation

```cpp
// src/debugger/dbg_client.cpp
int DebugClientMain(int argc, char *argv[]);
```

~80 lines.  Algorithm:

1. Parse `--socket=PATH` if present (shift argv).
2. If no `--socket`: scan `/tmp/maxivmac-dbg-*.sock` using `glob()`.
   - 0 matches → print error, exit 1.
   - 1 match → use it.
   - N matches → print list, exit 1.
3. `socket(AF_UNIX, SOCK_STREAM, 0)` → `connect()`.
4. If remaining args contain a command string:
   - **One-shot mode:** `send(cmd + "\n")`, `recv` until EOT,
     write to stdout, close, exit.
5. If no command arg:
   - **Interactive mode:** loop on `fgets(stdin)` (or readline if
     available), send each line, recv until EOT, print.  Exit on
     EOF or `quit`.

### 18.3  Socket auto-discovery

```cpp
static std::string FindSocket()
{
    glob_t g;
    if (glob("/tmp/maxivmac-dbg-*.sock", 0, nullptr, &g) != 0)
        return {};
    if (g.gl_pathc == 1) {
        std::string result = g.gl_pathv[0];
        globfree(&g);
        return result;
    }
    // Multiple or zero — print diagnostics
    if (g.gl_pathc > 1) {
        std::fprintf(stderr, "Multiple debug servers found:\n");
        for (size_t i = 0; i < g.gl_pathc; ++i)
            std::fprintf(stderr, "  %s\n", g.gl_pathv[i]);
        std::fprintf(stderr, "Use --socket=PATH to select one.\n");
    }
    globfree(&g);
    return {};
}
```

### 18.4  EOT framing

The client reads from the socket into a buffer and scans for the EOT
sentinel (`'\x04'`).  Everything before the EOT is the command
response — written to stdout verbatim.  The EOT itself is not printed.

```cpp
static bool RecvResponse(int fd)
{
    char buf[4096];
    for (;;) {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) return false;
        for (ssize_t i = 0; i < n; ++i) {
            if (buf[i] == '\x04') return true;
            putchar(buf[i]);
        }
    }
}
```

---

## 19.  Build Integration Update

### CMakeLists.txt additions

```cmake
    # Debugger
    src/debugger/dbg_io.cpp
    src/debugger/dbg_client.cpp
```

Added alongside the existing debugger sources.  `dbg_client.cpp` is
compiled into the main binary — it's only reached via the `argv[1]`
early exit so has zero cost in normal emulator mode.

No new external dependencies.  Unix domain sockets are part of POSIX
(`<sys/socket.h>`, `<sys/un.h>`).

---

## 20.  Testing (Debug Server)

### Unit tests

In `test/test_debugger.cpp`:

- **StdioIO** — cannot easily unit-test (requires stdin/stdout pipes);
  covered by the existing smoke test.
- **SocketIO** — create a `socketpair(AF_UNIX, ...)`, wrap the server
  end in a `SocketIO`, write a command from the client end, verify
  the response and EOT framing.
- **EOT framing** — send a multi-line response, verify the client
  receives everything before EOT and nothing after.
- **Socket auto-discovery** — create a temp socket file, verify
  `FindSocket()` returns it; create two, verify it returns empty.

### Integration test

Extend `test/debugger_smoke.sh` with server-mode tests:

```bash
# Start emulator in server mode
$MAXIVMAC --debugserver --model MacPlus --headless 608.hfs &
SERVER_PID=$!
sleep 1

# Discover socket
SOCK="/tmp/maxivmac-dbg-${SERVER_PID}.sock"

# One-shot commands
$MAXIVMAC debug --socket="$SOCK" "info reg" | grep -q "D0="
$MAXIVMAC debug --socket="$SOCK" "step"
$MAXIVMAC debug --socket="$SOCK" "info reg" | grep -q "PC="
$MAXIVMAC debug --socket="$SOCK" "quit"

wait $SERVER_PID
```

Integration-level tests (command sequences against a running CPU) are
harder and can come later — the unit tests above cover the riskiest
code paths.
