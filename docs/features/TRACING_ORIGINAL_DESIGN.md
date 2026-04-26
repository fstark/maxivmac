# Trap Tracing — Design Document

## Goal

Replace the current flat `[TRAP] $A9A0 GetResource` one-liner with a rich,
hierarchical trace that shows:

- Cycle number at call time
- Caller address (return PC)
- Entry and exit lines for every trap
- Indentation reflecting nesting depth
- Decoded input parameters on the entry line
- Decoded output parameters (+ result code) on the exit line

The parameter definitions live in an **external text file** so they can be
extended without recompiling.

---

## 1. Current State

| Component | File | Notes |
|-----------|------|-------|
| Trap dispatch | `src/cpu/m68k.cpp` `DoCodeA()` L4517 | Reads trap word, calls `trap_counter_record` + `trap_trace_log`, then `Exception(0xA)` |
| Counter + dict | `src/cpu/trap_counter.cpp` | 681-entry `s_dict[]`, flat counter array (1280 slots), watchlist |
| Console trace | `trap_trace_log()` | Prints `[TRAP] $XXXX Name` to stderr; depth-counted on/off via `BeginTraceTraps/EndTraceTraps` |
| Cycle counter | `V_MaxCyclesToGo` (m68k.cpp) | Cycles *remaining* in current timeslice (counts down) |
| Instruction counter | `g_instructionCount` (m68k.cpp) | Monotonically increasing |
| Register access | `m68k_dreg(n)`, `m68k_areg(n)` | D0–D7, A0–A7 |
| Guest memory | `get_vm_byte/word/long(addr)` | Read guest address space |

### Mac Trap Calling Conventions (recap)

| Category | Selection | Param passing | Return |
|----------|-----------|---------------|--------|
| **OS traps** (bit 11 = 0, `$A0xx`) | Trap word bits 0–7 | Registers (A0, D0, …) | D0 = OSErr, A0 = result |
| **Toolbox traps** (bit 11 = 1, `$A8xx`/`$A9xx`/`$AAxx`) | Trap word bits 0–9 | Stack (Pascal convention) | Stack (top = result) |

---

## 2. Architecture Overview

```
  ┌──────────────────┐      load at startup
  │  traps.def       │─────────────────────────┐
  │  (text file)     │                          ▼
  └──────────────────┘                ┌──────────────────┐
                                      │  TrapDefs        │
                                      │  .load() .find() │
                                      └────────┬─────────┘
                                               │ lookup
        DoCodeA()                              ▼
  ┌───────────────┐    ┌──────────────────────────────────┐
  │  m68k.cpp     │───▶│  TrapTracer                      │
  │  (A-line)     │    │    .enter(trapWord)             │
  └───────────────┘    │    .checkReturn(pc)            │
                       └──────────────────────────────────┘
        ▲ return                       │
        │  (RTS/RTE)                   │ emit
        │                              ▼
  ┌───────────────┐          ┌──────────────────┐
  │  Exception()  │          │  stderr / file   │
  │  dispatch     │          │  (trace output)  │
  └───────────────┘          └──────────────────┘
```

Three new components:

| Component | File(s) | Role |
|-----------|---------|------|
| **Trap definition file** | `assets/traps.def` | External text file: trap word, name, calling convention, parameters |
| **TrapDefs** | `src/cpu/trap_defs.h/.cpp` | Parse `traps.def` at startup into a lookup table |
| **TrapTracer** | `src/cpu/trap_tracer.h/.cpp` | Stateful tracer: manages nesting, formats entry/exit lines, reads regs/stack |

---

## 3. Trap Definition File — `assets/traps.def`

### 3.1 Format

Plain-text, one trap per line group, blank lines separate entries.
Comments start with `#`. Fields are whitespace-separated.

```
# <trap_word> <name> <convention>
# [in <name>:<type> ...]
# [out <name>:<type> ...]

A9A0 GetResource toolbox
  in  resType:OSType  resID:word
  out rsrc:Handle

A122 NewHandle os
  in  size:long.D0
  out h:Handle.A0  err:OSErr.D0

A9AB AddResource toolbox
  in  rsrc:Handle  resType:OSType  resID:word  name:Str255
  out err:OSErr.D0

A02E BlockMove os
  in  src:Ptr.A0  dst:Ptr.A1  count:long.D0
  out err:OSErr.D0
```

### 3.2 Convention field

- `toolbox` — parameters pushed on stack in Pascal order (left-to-right = deepest first)
- `os` — parameters in registers

### 3.3 Type system

Start minimal, extend later:

| Type | Size | Display | Notes |
|------|------|---------|-------|
| `byte` | 1 | `$XX` | |
| `word` | 2 | `$XXXX` | Includes trap numbers, selector IDs |
| `long` | 4 | `$XXXXXXXX` | |
| `Ptr` | 4 | `$XXXXXXXX` | Displayed as hex address |
| `Handle` | 4 | `$XXXXXXXX→$XXXXXXXX` | Dereferences one level to show master pointer |
| `OSType` | 4 | `'XXXX'` | Four ASCII chars (e.g. `'DRVR'`) |
| `Str255` | 1+n | `"text"` | Pascal string: first byte = length, then chars |
| `OSErr` | 2 | signed decimal + symbolic name | e.g. `-43 fnfErr` |
| `Boolean` | 1 | `true`/`false` | |
| `Rect` | 8 | `{t,l,b,r}` | Four 16-bit fields |
| `Point` | 4 | `{v,h}` | Two 16-bit fields |

### 3.4 Register suffix

For OS traps, a register suffix says where to find the parameter:

- `.D0` through `.D7` — data register
- `.A0` through `.A7` — address register

If no suffix, Toolbox convention: parameters are read sequentially from the
stack (SP at entry), and the result (if any) is written back to the stack on
exit.

### 3.5 Extensibility

Unknown traps (not in `traps.def`) still trace with the existing dictionary
name from `trap_counter.cpp`; they just show no parameter decode.  
Entries can be added incrementally — start with the 20-30 most common traps
and grow the file as debugging needs dictate.

### 3.6 Non-returning traps

Some traps never return to their caller (e.g. `ExitToShell`, `Launch`,
`SysError`). These are marked with a `noreturn` flag in the definition:

```
A9F4 ExitToShell toolbox noreturn

A9F2 Launch os noreturn
  in  launchPtr:Ptr.A0

A9C9 SysError toolbox noreturn
  in  errorCode:word
```

The tracer handles `noreturn` traps specially — see §6.1.

---

## 4. Trap Defs — `trap_defs.h/.cpp`

### 4.1 Data structures

```cpp
enum class ParamLoc { Stack, D0, D1, D2, D3, D4, D5, D6, D7,
                      A0, A1, A2, A3, A4, A5, A6, A7 };

enum class ParamType { Byte, Word, Long, Ptr, Handle, OSType,
                       Str255, OSErr, Boolean, Rect, Point };

struct ParamDef {
    std::string name;       // e.g. "resType"
    ParamType   type;
    ParamLoc    loc;        // Stack for toolbox, register for OS
};

enum class TrapConvention { OS, Toolbox };

struct TrapDef {
    uint16_t               trapWord;
    std::string            name;
    TrapConvention         convention;
    bool                   noreturn = false;
    std::vector<ParamDef>  paramsIn;    // input params
    std::vector<ParamDef>  paramsOut;   // output params
};
```

### 4.2 Class API

```cpp
class TrapDefs {
public:
    // Load definitions from file. Returns number of entries loaded.
    // Falls back gracefully if file is missing (0 entries, tracing still works).
    int load(const std::filesystem::path &path);

    // Lookup by trap word. Returns nullptr if no definition found.
    const TrapDef *find(uint16_t trapWord) const;

private:
    std::unordered_map<uint16_t, TrapDef> defs_;
};
```

### 4.3 Storage

A `std::unordered_map<uint16_t, TrapDef>` keyed by trap word, owned by
the `TrapDefs` instance. Loaded once at startup; read-only thereafter —
no locking needed.

---

## 5. Trap Tracer — `trap_tracer.h/.cpp`

### 5.1 State

```cpp
struct TrapFrame {
    uint16_t  trapWord;
    uint32_t  callerPC;       // return address (from stack at entry)
    uint32_t  entryCycle;     // g_instructionCount at entry
    uint32_t  sp;             // SP at entry (for toolbox param read)
    uint16_t  appId;          // CurApRefNum at entry time
};
```

### 5.2 Class API

```cpp
class TrapTracer {
public:
    explicit TrapTracer(TrapDefs &defs);

    // Called from DoCodeA(), before Exception(0xA).
    void enter(uint16_t trapWord);

    // Called from the instruction loop to detect returns (Strategy A).
    void checkReturn(uint32_t pc);

    // True when the nesting stack is non-empty (inline fast-path).
    bool active() const { return depth_ > 0; }

    // Master enable/disable + depth limit.
    void enable(bool on);
    void setMaxDepth(int depth);

    // Filter: if non-empty, only trace these traps.
    void addFilter(uint16_t trapWord);
    void clearFilter();

private:
    void flushStack(std::string_view reason);
    void emitEntry(const TrapFrame &frame, const TrapDef *def);
    void emitExit(const TrapFrame &frame, const TrapDef *def);
    std::string formatParam(const ParamDef &p, uint32_t rawValue);
    uint16_t readAppId() const;       // reads CurApRefNum ($0900)
    std::string readAppName() const;  // reads CurApName ($0910)

    TrapDefs                  &defs_;
    std::array<TrapFrame, 64>  stack_;
    int                        depth_ = 0;
    bool                       enabled_ = false;
    int                        maxDepth_ = 64;
    uint16_t                   lastAppId_ = 0;
    std::vector<uint16_t>      filter_;
};
```

### 5.3 Output format

Entry line:
```
→ 1234567 [3] GetResource(resType:'DRVR', resID:$0008) [caller:$00412E]
```

Exit line:
```
← 1234890 [3] GetResource → rsrc:$00F234→$0040A8  (+323 cycles)
```

The `[3]` is the current `CurApRefNum` — it identifies which application
is running. When tracing across MultiFinder context switches, this makes
it immediately clear which app each trap belongs to.

Nested example:
```
→ 1230000 [3] GetResource(resType:'DRVR', resID:$0008) [caller:$00412E]
  → 1230010 [3] NewHandle(size:$00000100) [caller:$013E02]
    → 1230020 [3] BlockMove(src:$040000, dst:$050000, count:$00000100) [caller:$013F44]
    ← 1230035 [3] BlockMove → err:0  (+15 cycles)
  ← 1230050 [3] NewHandle → h:$00F234→$050000 err:0  (+40 cycles)
← 1230100 [3] GetResource → rsrc:$00F234→$050000  (+100 cycles)
```

### 5.4 Parameter formatting

Reading parameters at entry time:

- **Toolbox (stack)**: Read sequentially from `m68k_areg(7) + 4` (skip return address). For each `in` param, read the appropriate number of bytes using `get_vm_byte/word/long`.
- **OS (register)**: Read from `m68k_dreg(n)` or `m68k_areg(n)` as specified by the `.Dn`/`.An` suffix.

Reading results at exit time follows the same logic for `out` parameters.

Type-specific formatting:

```cpp
std::string formatParam(const ParamDef &p, uint32_t rawValue);
```

| Type | Formatting |
|------|-----------|
| `byte` | `$XX` |
| `word` | `$XXXX` |
| `long` | `$XXXXXXXX` |
| `Ptr` | `$XXXXXXXX` |
| `Handle` | `$XXXXXXXX→$XXXXXXXX` (deref one level) |
| `OSType` | `'XXXX'` (4 ASCII chars, replace non-printable with `.`) |
| `Str255` | `"..."` (read length byte, then chars, cap at 63 for display) |
| `OSErr` | `0` or `-43 fnfErr` (small lookup table for common errors) |
| `Boolean` | `true`/`false` |
| `Rect` | `{top,left,bottom,right}` |
| `Point` | `{v,h}` |

---

## 6. Detecting Trap Return

This is the central design challenge. Three strategies, from simplest to most
robust:

### Strategy A: Stack-frame matching (recommended first step)

On entry, save `returnPC` from the stack. On every instruction (or on the next
`RTS`/`RTE`), check whether `PC == returnPC` for the top-of-stack frame.

**Implementation**: Add a check inside the main instruction loop (next to the
existing `g_instructionCount++` block). If `m68k_getpc() == s_stack[s_depth-1].callerPC`,
pop the frame and emit the exit line.

**Cost**: One comparison per instruction while any trap frame is active.
Negligible vs. the instruction decode cost.

**Limitation**: Re-entrant code that jumps through the same address could
cause false matches. In practice, Mac OS traps are not re-entrant at the same
nesting level, so this works for the vast majority of cases.

### 6.1 Non-returning traps

When a `noreturn` trap (ExitToShell, Launch, SysError) is entered:

1. Emit the entry line as usual (with parameters).
2. **Do not push a frame** onto the nesting stack.
3. **Flush the entire nesting stack**: emit synthetic exit lines for all
   pending frames (innermost first), each marked `[abandoned]` instead of
   showing output params:
   ```
   → 5000000 [3] GetResource(resType:'DRVR', resID:$0008) [caller:$00412E]
     → 5000100 [3] ExitToShell() [caller:$004200]
     ⊘ GetResource [abandoned — ExitToShell]
   ```
4. Reset `s_depth` to 0.

This keeps the trace output coherent: every `→` has a matching `←` or `⊘`,
and subsequent traps are correctly un-indented.

### 6.2 MultiFinder context switches

Under MultiFinder, the system switches between applications. When this
happens, pending trap frames from the previous app become stale — their
return PCs will never be reached in the current app context.

The tracer detects context switches by monitoring the low-memory global
`CurApRefNum` ($0900). On each `trap_trace_enter()`, read this word; if it
has changed since the last call:

1. Flush the nesting stack with `[context switch]` markers (same mechanism
   as §6.1).
2. Emit a separator line showing the new app identity:
   ```
   ── context switch: appId=3 "MacWrite" ──
   ```
   (`CurApName` is the Pascal string at $0910, `CurApRefNum` at $0900.)
3. Reset depth to 0.

This is a lightweight heuristic — it doesn't try to maintain per-app stacks.
The goal is simply not to break the normal tracing flow when apps switch.

### Strategy B: Return-address patching

Replace the return address on the stack with a trampoline address in a
reserved page. The trampoline triggers a known exception, which the tracer
intercepts. After logging, restore the real return address and resume.

**Pro**: Zero per-instruction overhead when a trap is active.  
**Con**: Invasive; risks breaking code that reads its own return address.

### Strategy C: RTS/RTE interception

Hook the `DoCodeRts()` / `DoCodeRte()` functions. On each return instruction,
check if the destination matches a pending trap frame.

**Pro**: Only fires on actual returns.  
**Con**: Requires touching the instruction handlers; tail-call chains
(`JMP` instead of `RTS`) would be missed.

### Recommendation

**Start with Strategy A.** It is the simplest, non-invasive, and correct for
99%+ of real trap calls. If profiling shows the per-instruction check is
measurable, switch to Strategy C. Strategy B is an option for future
optimization but not needed initially.

---

## 7. Integration into `m68k.cpp`

### 7.1 Entry hook — `DoCodeA()`

```cpp
static void DoCodeA()
{
    BackupPC();
    uint16_t tw = do_get_mem_word(V_pc_p);
    trap_counter_record(tw);
    g_tracer.enter(tw);            // NEW — replaces trap_trace_log
    Exception(0xA);
}
```

`g_tracer` is the global `TrapTracer` instance (see §8).

### 7.2 Exit check — main loop

Inside the instruction execution loop, after `g_instructionCount++`:

```cpp
if (g_tracer.active()) {
    g_tracer.checkReturn(m68k_getpc());
}
```

`active()` is an inline check of `depth_ > 0` — zero cost when
tracing is disabled or no frames are pending.

### 7.3 Cycle counting

Use `g_instructionCount` as the "cycle" number in trace output. It is
monotonic and stable across timeslice boundaries, unlike `V_MaxCyclesToGo`
which counts down within a slice. The delta (`exitCount - entryCount`) gives
a meaningful duration in instructions.

---

## 8. File Layout

```
assets/
  traps.def                    # trap definitions (external, editable)
  roms/                         # (future: ROM files, currently in roms/)
  disks/                        # (future: default boot disk images)

src/cpu/
  trap_defs.h                  # TrapDefs class, TrapDef/ParamDef structs
  trap_defs.cpp                # Parser for traps.def
  trap_tracer.h                # TrapTracer class
  trap_tracer.cpp              # Tracer implementation (nesting, formatting)
  trap_counter.h               # (existing — unchanged)
  trap_counter.cpp             # (existing — remove trap_trace_log, or keep as fallback)
```

The `assets/` directory is the top-level home for all runtime resources
shipped with the emulator: trap definitions, ROMs, boot disks, and any
future data files. The existing `roms/` folder will migrate into
`assets/roms/` in a separate step.

### Global instances

```cpp
// In trap_tracer.h / trap_tracer.cpp:
extern TrapDefs   g_trapDefs;
extern TrapTracer g_tracer;
```

`g_trapDefs` is loaded once at startup. `g_tracer` holds a reference
to it and is called from the CPU loop.

### CMakeLists.txt additions

```cmake
src/cpu/trap_defs.cpp
src/cpu/trap_tracer.cpp
```

Add `assets/traps.def` to install/resource copy rules.

---

## 9. Phased Implementation Plan

### Phase 1: Infrastructure (trap_defs)

- Define `traps.def` format and write 20-30 common traps.
- Implement `TrapDefs::load()` / `TrapDefs::find()`.
- Unit test: load file, look up known traps.

### Phase 2: Basic entry tracing

- Implement `TrapTracer::enter()` with cycle, appId, caller PC, indentation.
- Wire into `DoCodeA()` via `g_tracer.enter()`.
- OS trap register params + Toolbox stack params for the 20 defined traps.
- Output goes to stderr.

### Phase 3: Exit detection + output params

- Implement Strategy A (PC-match return detection) in `TrapTracer::checkReturn()`.
- Add return-line formatting with `out` parameters and cycle delta.
- Implement `noreturn` trap handling and MultiFinder context-switch detection.
- Wire `g_tracer.checkReturn()` into the instruction loop.

### Phase 4: Filtering and UI integration

- Add filter API (trace only selected traps).
- Wire enable/disable and filter to the ImGui debug UI.
- Add depth-limit control to avoid overwhelming output.

### Phase 5: Expand definitions

- Grow `traps.def` to 100+ traps based on debugging needs.
- Add more types as needed (StringPtr, RgnHandle, GrafPtr, etc.).
- Consider sub-dispatcher support (e.g. `HFSDispatch` selectors in D0).

---

## 10. Resolved Questions

1. **Output destination**: stderr for now.  ImGui trace viewer deferred to
   Phase 4 (ring buffer + lightweight lock for cross-thread snapshot).

2. **Sub-dispatchers**: Deferred to Phase 5.  For now, log the raw trap
   word; the `in` params will naturally show the selector value in the
   relevant register or stack slot.

3. **Thread safety**: Tracing state is CPU-thread-only.  When the ImGui
   viewer is added (Phase 4), a snapshot-under-lock API will be needed.

4. **Performance**: The per-instruction overhead of Strategy A is accepted.
   Will revisit only if profiling shows a measurable impact.
