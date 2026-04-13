# Trap Tracing — Implementation Plan

Design: `docs/features/TRACING_DESIGN.md`

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Infrastructure: trap definition file and parser (TrapDefs) | Not started |
| 2 | Basic entry tracing with parameter decode | Not started |
| 3 | Exit detection, output params, noreturn, context switches | Not started |

Build gate: `cmake --preset macos && cmake --build --preset macos`
Test gate:  `./bld/macos/tests && cd test && ./verify.sh`

---

## Phase 1 — Infrastructure (TrapDefs)

Create the external trap definition file, the parser, and the lookup class.
No changes to the CPU loop yet — this phase is purely additive.

### 1.1 — Create `assets/traps.def`

New directory `assets/` at the project root. New file `assets/traps.def`
with the initial set of ~25 common traps. Format: one trap per block,
blank-line separated, comments with `#`.

```
# Trap definitions for maxivmac hierarchical tracer.
# Format:
#   <trap_word> <name> <convention> [noreturn]
#   in  <name>:<type>[.reg] ...
#   out <name>:<type>[.reg] ...
#
# Convention: toolbox | os
# Types: byte word long Ptr Handle OSType Str255 OSErr Boolean Rect Point
# Register suffix (.D0-.D7, .A0-.A7) for OS traps; omit for toolbox (stack).

# ── Memory Manager ────────────────────────────────────

A122 NewHandle os
  in  size:long.D0
  out h:Handle.A0  err:OSErr.D0

A022 NewPtr os
  in  size:long.D0
  out p:Ptr.A0  err:OSErr.D0

A023 DisposHandle os
  in  h:Handle.A0
  out err:OSErr.D0

A01F DisposPtr os
  in  p:Ptr.A0
  out err:OSErr.D0

A024 GetHandleSize os
  in  h:Handle.A0
  out size:long.D0

A025 SetHandleSize os
  in  h:Handle.A0  size:long.D0
  out err:OSErr.D0

A029 HLock os
  in  h:Handle.A0
  out err:OSErr.D0

A02A HUnlock os
  in  h:Handle.A0
  out err:OSErr.D0

A02E BlockMove os
  in  src:Ptr.A0  dst:Ptr.A1  count:long.D0
  out err:OSErr.D0

A11E NewPtrClear os
  in  size:long.D0
  out p:Ptr.A0  err:OSErr.D0

# ── Resource Manager ─────────────────────────────────

A9A0 GetResource toolbox
  in  resType:OSType  resID:word
  out rsrc:Handle

A9A1 Get1Resource toolbox
  in  resType:OSType  resID:word
  out rsrc:Handle

A9AB AddResource toolbox
  in  rsrc:Handle  resType:OSType  resID:word  name:Str255

A9A2 LoadResource toolbox
  in  rsrc:Handle

A9A3 ReleaseResource toolbox
  in  rsrc:Handle

A9A4 HomeResFile toolbox
  in  rsrc:Handle
  out refNum:word

A9A5 SizeRsrc toolbox
  in  rsrc:Handle
  out size:long

# ── File Manager ──────────────────────────────────────

A000 Open os
  in  pb:Ptr.A0
  out err:OSErr.D0

A001 Close os
  in  pb:Ptr.A0
  out err:OSErr.D0

A002 Read os
  in  pb:Ptr.A0
  out err:OSErr.D0

A003 Write os
  in  pb:Ptr.A0
  out err:OSErr.D0

# ── Event Manager ─────────────────────────────────────

A970 GetNextEvent toolbox
  in  eventMask:word
  out gotEvent:Boolean

A971 EventAvail toolbox
  in  eventMask:word
  out gotEvent:Boolean

# ── Window Manager ────────────────────────────────────

A913 NewWindow toolbox
  in  wStorage:Ptr  boundsRect:Rect  title:Str255  visible:Boolean  procID:word  behind:Ptr  goAwayFlag:Boolean  refCon:long
  out theWindow:Ptr

A92C FindWindow toolbox
  in  thePoint:Point
  out partCode:word

# ── Misc ──────────────────────────────────────────────

A884 DrawString toolbox
  in  s:Str255

A9F4 ExitToShell toolbox noreturn

A9F2 Launch os noreturn
  in  launchPtr:Ptr.A0

A9C9 SysError toolbox noreturn
  in  errorCode:word
```

### 1.2 — Create `assets/errors.def`

New file `assets/errors.def` with OSErr code → symbolic name mappings.
Same directory as `traps.def`. Plain-text, one entry per line,
`#` comments, blank lines ignored.

```
# OSErr definitions for maxivmac trap tracer.
# Format: <decimal_code> <symbolic_name>

0 noErr
-33 dirFulErr
-34 dskFulErr
-35 nsvErr
-36 ioErr
-37 bdNamErr
-39 eofErr
-43 fnfErr
-48 dupFNErr
-54 permErr
-108 memFullErr
-109 nilHandleErr
-120 dirNFErr
-192 resNotFound
-193 resFNotFound
-194 addResFailed
-196 rmvResFailed
```

Loaded by `TrapDefs::loadErrors()` at startup alongside `traps.def`.
Stored in a `std::unordered_map<int16_t, std::string>` inside `TrapDefs`.
Looked up via `TrapDefs::errorName(int16_t code)` — returns `nullptr` if
unknown (caller just prints the raw number).

### 1.3 — Create `src/cpu/trap_defs.h`

New header with the data structures and class declaration.

Contents:

```cpp
/*
    trap_defs.h — External trap definition loader

    Parses assets/traps.def at startup into a lookup table
    mapping trap words to names, conventions, and typed parameters.
*/
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>

enum class ParamLoc {
    Stack,
    D0, D1, D2, D3, D4, D5, D6, D7,
    A0, A1, A2, A3, A4, A5, A6, A7
};

enum class ParamType {
    Byte, Word, Long, Ptr, Handle, OSType,
    Str255, OSErr, Boolean, Rect, Point
};

struct ParamDef {
    std::string name;
    ParamType   type;
    ParamLoc    loc;
};

enum class TrapConvention { OS, Toolbox };

struct TrapDef {
    uint16_t               trapWord;
    std::string            name;
    TrapConvention         convention;
    bool                   noreturn = false;
    std::vector<ParamDef>  paramsIn;
    std::vector<ParamDef>  paramsOut;
};

class TrapDefs {
public:
    int load(const std::filesystem::path &path);
    int loadErrors(const std::filesystem::path &path);
    const TrapDef *find(uint16_t trapWord) const;
    const char *errorName(int16_t code) const;

private:
    std::unordered_map<uint16_t, TrapDef> defs_;
    std::unordered_map<int16_t, std::string> errors_;
};
```

### 1.4 — Create `src/cpu/trap_defs.cpp`

Parser implementation. Key behaviour:

- Read the file line by line.
- Skip blank lines (they delimit entries) and `#` comment lines.
- First non-blank, non-comment line of a group is the header:
  split on whitespace → `trapWord` (hex, no `$` or `0x` prefix in file,
  parse with `strtoul(s, nullptr, 16)`), `name`, `convention`
  (`"os"` / `"toolbox"`, case-insensitive), optional `"noreturn"` flag.
- Subsequent lines starting with `in` or `out` (after leading whitespace)
  define parameters. Each token after `in`/`out` is `name:type[.reg]`.
  Parse the type string and optional register suffix.
- On malformed lines, log a warning to stderr and skip.
- `find()` returns `nullptr` for unknown traps (caller falls back to
  `trap_dict_name()` from trap_counter).

Type string → `ParamType` mapping:

| String | Enum |
|--------|------|
| `byte` | Byte |
| `word` | Word |
| `long` | Long |
| `Ptr` | Ptr |
| `Handle` | Handle |
| `OSType` | OSType |
| `Str255` | Str255 |
| `OSErr` | OSErr |
| `Boolean` | Boolean |
| `Rect` | Rect |
| `Point` | Point |

Register suffix → `ParamLoc` mapping: `.D0`→`D0`, `.A0`→`A0`, etc.
If no suffix, `ParamLoc::Stack` (default for Toolbox traps).

### 1.5 — Add to CMakeLists.txt

Add `src/cpu/trap_defs.cpp` to `MINIVMAC_SOURCES` after `src/cpu/trap_counter.cpp`.

### 1.6 — Copy `assets/traps.def` and `assets/errors.def` to build output

Add a `configure_file()` or `file(COPY ...)` rule in CMakeLists.txt so that
`assets/traps.def` and `assets/errors.def` are copied next to the built binary
(or into the app bundle on macOS). The tracer will look for them relative to
the executable path.

### 1.7 — Load at startup

In the emulator startup path (wherever `MINEM68K_Init()` or the machine
initialisation runs), add:

```cpp
g_trapDefs.load(exeDir / "assets" / "traps.def");
g_trapDefs.loadErrors(exeDir / "assets" / "errors.def");
```

Declare the global in `trap_defs.cpp`:
```cpp
TrapDefs g_trapDefs;
```

If the file is missing, `load()` returns 0 and tracing works without
parameter decode (same behaviour as today).

### 1.8 — Unit tests: `test/test_tracing.cpp`

Create `test/test_tracing.cpp` and add it to the `tests` binary in
CMakeLists.txt (alongside `test_main.cpp` and `test_slip.cpp`). Link
`src/cpu/trap_defs.cpp` into the `tests` target.

Test cases for `TrapDefs::load()`:

- **"TrapDefs load basic"** — Write a minimal `traps.def` to a temp file
  containing 2-3 entries (one OS, one Toolbox, one noreturn). Load it.
  Verify `load()` returns the correct count.
- **"TrapDefs find known trap"** — After loading, `find(0xA122)` returns
  non-null with `name=="NewHandle"`, `convention==OS`, `noreturn==false`,
  one `paramsIn` with `name=="size"`, `type==Long`, `loc==D0`.
- **"TrapDefs find toolbox trap"** — `find(0xA9A0)` returns Toolbox
  convention with two `paramsIn` (resType:OSType, resID:word) and one
  `paramsOut` (rsrc:Handle, loc==Stack).
- **"TrapDefs find noreturn"** — `find(0xA9F4)` has `noreturn==true`.
- **"TrapDefs find unknown returns null"** — `find(0xA999)` returns
  `nullptr`.
- **"TrapDefs trap word masking"** — `find(0xA222)` (NewHandle with SYS
  bit set) returns the same entry as `find(0xA122)`.
- **"TrapDefs skip malformed lines"** — Load a file with a valid entry
  plus a garbled line. Verify the valid entry loads and the bad one is
  skipped (count == 1).
- **"TrapDefs empty file"** — Load an empty file. `load()` returns 0,
  `find()` returns null.
- **"TrapDefs comments and blanks"** — File with only comments and blank
  lines. `load()` returns 0.

Test cases for `TrapDefs::loadErrors()` / `errorName()`:

- **"errors load and lookup"** — Write a temp `errors.def` with a few
  entries. `errorName(0)` returns `"noErr"`, `errorName(-43)` returns
  `"fnfErr"`.
- **"errors unknown code"** — `errorName(-9999)` returns `nullptr`.

### 1.9 — Build & verify

- Build: confirm clean compilation with no warnings.
- Test: `./bld/macos/tests` — all unit tests pass.
- Test: `cd test && ./verify.sh` — all golden tests pass (no runtime change).
- Spot-check: add a temporary `fprintf` in `load()` to print the count
  of loaded definitions, run the emulator, confirm it prints e.g.
  `"trap_defs: loaded 25 entries"`.

---

## Phase 2 — Basic Entry Tracing

Wire `TrapTracer::enter()` into `DoCodeA()` and emit entry lines with
decoded parameters to stderr. No exit detection yet.

### 2.1 — Create `src/cpu/trap_tracer.h`

New header with the `TrapTracer` class declaration:

```cpp
/*
    trap_tracer.h — Hierarchical A-line trap tracer
*/
#pragma once

#include "cpu/trap_defs.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct TrapFrame {
    uint16_t  trapWord;
    uint32_t  callerPC;       // return address from stack
    uint32_t  entryCycle;     // g_instructionCount at entry
    uint32_t  sp;             // SP at entry (for toolbox param read)
    uint16_t  appId;          // CurApRefNum at entry time
};

class TrapTracer {
public:
    explicit TrapTracer(TrapDefs &defs);

    void enter(uint16_t trapWord);
    void checkReturn(uint32_t pc);           // stub in Phase 2
    bool active() const { return depth_ > 0; }

    void enable(bool on);
    void setMaxDepth(int depth);

    void addFilter(uint16_t trapWord);
    void clearFilter();

private:
    void flushStack(const char *reason);
    void emitEntry(const TrapFrame &frame, const TrapDef *def);
    std::string formatParam(const ParamDef &p, uint32_t rawValue);
    std::string formatOSType(uint32_t raw);
    std::string formatStr255(uint32_t addr);
    std::string formatOSErr(int16_t err);
    uint32_t readParamRaw(const ParamDef &p, uint32_t sp, int &stackOffset);
    uint16_t readAppId() const;

    TrapDefs                  &defs_;
    std::array<TrapFrame, 64>  stack_;
    int                        depth_ = 0;
    bool                       enabled_ = false;
    int                        maxDepth_ = 64;
    uint16_t                   lastAppId_ = 0;
    std::vector<uint16_t>      filter_;
};

extern TrapDefs   g_trapDefs;
extern TrapTracer g_tracer;
```

### 2.2 — Create `src/cpu/trap_tracer.cpp`

Implementation of `enter()` and the parameter formatting helpers.

**`enter(uint16_t trapWord)`:**
1. If `!enabled_`, return immediately.
2. If `filter_` is non-empty and `trapWord` is not in it, return.
3. If `depth_ >= maxDepth_`, return (overflow guard).
4. Read CurApRefNum from guest address `$0900` via `get_vm_word(0x0900)`.
5. Build a `TrapFrame`: trapWord, callerPC from stack
   (`get_vm_long(m68k_areg(7))`), g_instructionCount, SP (`m68k_areg(7)`),
   appId.
6. Look up `const TrapDef *def = defs_.find(trapWord)`.
7. Call `emitEntry(frame, def)`.
8. If def is non-null and `def->noreturn` is true:
   - Call `flushStack("noreturn")` to emit `⊘` lines for all pending frames.
   - Do NOT push frame.
9. Else: push frame onto `stack_[depth_++]`.

**`emitEntry()`:**
Format the entry line:
```
{indent}→ {cycle} [{appId}] {name}({params}) [caller:{callerPC}]
```
- Indent: `depth_ * 2` spaces.
- `name`: from `def->name` if available, else from `trap_dict_name(trapWord)`,
  else `$XXXX`.
- Params: if `def` is non-null, iterate `def->paramsIn`, read each raw value
  via `readParamRaw()`, format via `formatParam()`, join with `, `.
- Print to stderr with `fprintf`.

**`readParamRaw()`:**
- For `ParamLoc::Stack`: read from `sp + 4 + stackOffset` using
  `get_vm_word()` or `get_vm_long()` depending on type size. Advance
  `stackOffset` by the type's size (2 for word/OSErr/Boolean, 4 for
  long/Ptr/Handle/OSType, 8 for Rect, 4 for Point, variable for Str255
  — read as Ptr to string).
- For register locations: read from `m68k_dreg(n)` or `m68k_areg(n)`.

**`formatParam()`:**
Dispatch on `ParamType`:
- `Byte`: `$XX`
- `Word`: `$XXXX`
- `Long`: `$XXXXXXXX`
- `Ptr`: `$XXXXXXXX`
- `Handle`: `$XXXXXXXX→$XXXXXXXX` (deref via `get_vm_long(raw)`)
- `OSType`: `'XXXX'` (4 ASCII chars, non-printable → `.`)
- `Str255`: `"..."` (read length byte at raw, then chars, cap 63)
- `OSErr`: signed decimal + symbolic name from small lookup table
- `Boolean`: `true`/`false`
- `Rect`: `{t,l,b,r}` (four signed 16-bit values)
- `Point`: `{v,h}` (two signed 16-bit values)

**`flushStack()`:**
For each frame from `depth_-1` down to 0, emit:
```
{indent}⊘ {name} [abandoned — {reason}]
```
Then set `depth_ = 0`.

**`readAppId()`:**
```cpp
return get_vm_word(0x0900);  // CurApRefNum
```

**OSErr lookup** — uses `g_trapDefs.errorName(code)`, which looks up the
external `assets/errors.def` loaded in Phase 1 (§1.2). If the code is not
found, just prints the raw decimal number.

### 2.3 — Add to CMakeLists.txt

Add `src/cpu/trap_tracer.cpp` to `MINIVMAC_SOURCES` after `src/cpu/trap_defs.cpp`.

### 2.4 — Wire `enter()` into `DoCodeA()`

In `src/cpu/m68k.cpp`, function `DoCodeA()` (~line 4516):

Replace:
```cpp
trap_counter_record(tw);
trap_trace_log(tw);
```

With:
```cpp
trap_counter_record(tw);
g_tracer.enter(tw);
```

Add `#include "cpu/trap_tracer.h"` to the includes in m68k.cpp.

The old `trap_trace_log()` call is replaced by the new tracer. The
`BeginTraceTraps/EndTraceTraps` API and `trap_trace_log()` remain in
`trap_counter.h/.cpp` for now (they are still potentially used elsewhere),
but `DoCodeA()` no longer calls them.

### 2.5 — Enable/disable integration

For Phase 2, keep it simple: the tracer is disabled by default. Add a way
to enable it:

- The existing `BeginTraceTraps()` call should also call `g_tracer.enable(true)`.
- The existing `EndTraceTraps()` call should call `g_tracer.enable(false)` when
  the depth reaches 0.
- Alternatively, add a command-line flag (`-trace-traps`) to enable at
  startup. Wire it through the config loader.

For now, use the `BeginTraceTraps()` bridge so that existing debug workflows
continue to work.

### 2.6 — Unit tests: formatter tests

Add test cases to `test/test_tracing.cpp` for the pure formatting
functions. These don't need guest memory — just pass raw values:

- **"formatOSType printable"** — `formatOSType(0x44525652)` → `"'DRVR'"`.
- **"formatOSType non-printable"** — `formatOSType(0x00015652)` → `"'..VR'"`.
- **"formatOSErr known"** — `formatOSErr(-43)` → `"-43 fnfErr"`.
- **"formatOSErr noErr"** — `formatOSErr(0)` → `"0 noErr"`.
- **"formatOSErr unknown"** — `formatOSErr(-9999)` → `"-9999"`.
- **"formatParam Byte"** — `formatParam(ByteDef, 0xFF)` → `"$FF"`.
- **"formatParam Word"** — `formatParam(WordDef, 0x1234)` → `"$1234"`.
- **"formatParam Long"** — `formatParam(LongDef, 0xDEADBEEF)` → `"$DEADBEEF"`.
- **"formatParam Boolean true"** — `formatParam(BoolDef, 1)` → `"true"`.
- **"formatParam Boolean false"** — `formatParam(BoolDef, 0)` → `"false"`.

Note: `formatParam` for Handle, Str255, Rect, Point requires guest memory
access and cannot be unit tested here. Those are covered by manual boot
testing.

To make the formatters testable, expose them as free functions or static
methods (they are pure functions of their inputs). Alternatively, make
them `public` in `TrapTracer` for test access — acceptable since the class
is internal.

### 2.7 — Build & verify

- Build: clean compilation.
- Test: `./bld/macos/tests` — all unit tests pass (including new formatter tests).
- Test: `cd test && ./verify.sh` — all models pass (tracer disabled by default).
- Manual test: enable tracing and boot a disk image. Confirm entry lines
  appear on stderr with the expected format:
  ```
  → 123456 [0] GetResource(resType:'DRVR', resID:$0008) [caller:$00412E]
  → 123460 [0] NewHandle(size:$00000100) [caller:$013E02]
  ```
- Verify that traps not in `traps.def` still appear (with name from
  `trap_dict_name()` and no parameter decode).

---

## Phase 3 — Exit Detection + Output Parameters

Implement Strategy A (PC-match) to detect trap returns, emit exit lines
with decoded output parameters, and handle noreturn traps and MultiFinder
context switches.

### 3.1 — Implement `checkReturn()` in `trap_tracer.cpp`

```cpp
void TrapTracer::checkReturn(uint32_t pc)
{
    while (depth_ > 0 && pc == stack_[depth_ - 1].callerPC) {
        --depth_;
        const TrapFrame &frame = stack_[depth_];
        const TrapDef *def = defs_.find(frame.trapWord);
        emitExit(frame, def);
    }
}
```

Key points:
- Use a `while` loop, not `if`, because multiple traps may return at
  the same PC (e.g., a tail call chain where A calls B, B returns to A's
  caller, and A's frame also matches).
- `checkReturn()` is only called when `active()` is true, so the while-loop
  body runs only when there is actually a match.

### 3.2 — Implement `emitExit()` in `trap_tracer.cpp`

Format the exit line:
```
{indent}← {cycle} [{appId}] {name} → {outParams}  (+{delta} cycles)
```

- Indent: `frame`'s depth level × 2 spaces (i.e. `depth_` at the time the
  frame was pushed — store a `depth` field in `TrapFrame`, or compute from
  the current `depth_` value since we just decremented).
- `name`: same lookup as `emitEntry()`.
- Output params: if `def` is non-null and `def->paramsOut` is non-empty,
  read each `out` param from its register or stack location and format.
  For OS traps, registers are live (D0, A0, etc.). For Toolbox traps,
  the result is on the stack at `m68k_areg(7)`.
- Cycle delta: `g_instructionCount - frame.entryCycle`.
- Print to stderr with `fprintf`.

### 3.3 — Add `depth` field to `TrapFrame`

Add `int depth;` to `TrapFrame`. Set it to `depth_` (before increment) in
`enter()`:
```cpp
frame.depth = depth_;
stack_[depth_++] = frame;
```

Use `frame.depth` in `emitExit()` for indentation (instead of `depth_`
which has already been decremented).

### 3.4 — Wire `checkReturn()` into the instruction loop

In `src/cpu/m68k.cpp`, inside the main `do { ... } while` loop, immediately
after `g_instructionCount++` (~line 767):

```cpp
g_instructionCount++;

if (g_tracer.active()) {
    g_tracer.checkReturn(m68k_getpc());
}
```

`active()` is `depth_ > 0`, which is zero-cost when no trap frames are
pending. The `m68k_getpc()` call (resolving the cached PC pointer) is the
same call already used one line above for the instruction log.

### 3.5 — Implement context-switch detection

In `TrapTracer::enter()`, after reading `CurApRefNum`:

```cpp
uint16_t appId = readAppId();
if (depth_ > 0 && appId != lastAppId_) {
    flushStack("context switch");
    std::string appName = readAppName();
    fprintf(stderr, "── context switch: appId=%u \"%s\" ──\n",
            appId, appName.c_str());
}
lastAppId_ = appId;
```

**`readAppName()`:**
Read the Pascal string at guest address `$0910` (CurApName):
```cpp
std::string TrapTracer::readAppName() const
{
    uint8_t len = get_vm_byte(0x0910);
    if (len > 31) len = 31;
    std::string s(len, '\0');
    for (int i = 0; i < len; ++i)
        s[i] = static_cast<char>(get_vm_byte(0x0911 + i));
    return s;
}
```

Add `readAppName()` declaration to the private section of `TrapTracer`.

### 3.6 — Noreturn flush refinement

Phase 2 already has `flushStack()` in `enter()` for noreturn traps. Verify
the output format is correct:

```
→ 5000000 [3] GetResource(resType:'DRVR', resID:$0008) [caller:$00412E]
  → 5000100 [3] ExitToShell() [caller:$004200]
  ⊘ GetResource [abandoned — ExitToShell]
```

The `flushStack()` for noreturn should pass the trap name (not just
`"noreturn"`) to produce a clear message. Update `enter()` to pass
the trap name:

```cpp
if (def && def->noreturn) {
    flushStack(def->name.c_str());
    return;  // do not push frame
}
```

Update `flushStack(const char *reason)` to use the reason string:
```
{indent}⊘ {frameName} [abandoned — {reason}]
```

### 3.7 — Stack overflow guard

If nesting exceeds 64 frames, stop pushing and emit a warning:
```
[TRACE] nesting overflow at depth 64, suppressing further nesting
```

The guard is already in Phase 2 (`depth_ >= maxDepth_` check); confirm
it emits a one-time warning, not one per trap.

### 3.8 — Build & verify

- Build: clean compilation.
- Test: `./bld/macos/tests` — all unit tests pass.
- Test: `cd test && ./verify.sh` — all models pass.
- Manual test with tracing enabled:
  1. Boot a disk image, confirm entry/exit pairs appear:
     ```
     → 123456 [0] NewHandle(size:$00000100) [caller:$013E02]
     ← 123500 [0] NewHandle → h:$00F234→$050000 err:0  (+44 cycles)
     ```
  2. Verify nested traps indent correctly.
  3. Verify unknown traps (no def) show name from dict, no params, and
     still get entry/exit lines.
  4. Test noreturn: trace through a quit sequence, confirm `⊘` lines
     appear for pending frames.
  5. If MultiFinder is running, confirm context-switch separator lines.

### 3.9 — Edge case review

Before marking Phase 3 complete, verify these scenarios:

- **Trap that returns via JMP** (not RTS): the return PC will never match.
  The frame will be flushed on the next context switch or noreturn trap,
  or remain pending (bounded by the 64-frame stack). Acceptable for now.
- **Recursive traps**: e.g. `GetResource` calling `GetResource`. Each gets
  its own frame with its own return PC. The `while` loop in `checkReturn()`
  handles the case where both return to the same PC (unlikely but safe).
- **Re-entrant interrupt traps**: interrupts save/restore state, so the
  return PC check remains valid. The nesting stack naturally reflects the
  interrupt nesting.
- **Trap-word variants**: Some traps have flag bits set (e.g. `$A222` =
  NewHandle with bit 9 set for `SYS`). The def file uses the canonical
  word; `find()` should mask off the flag bits before lookup. Add masking
  in `TrapDefs::find()`:
  ```cpp
  uint16_t key = trapWord;
  if (key & 0x0800)
      key = 0x0800 | (key & 0x03FF);  // Toolbox: keep bits 0-9 + bit 11
  else
      key = key & 0x00FF;              // OS: keep bits 0-7
  ```
  Store defs keyed by this masked value in `load()` as well.
