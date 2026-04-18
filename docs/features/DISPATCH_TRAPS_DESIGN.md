# First-Class Dispatch Trap Subtraps — Detailed Design

Implements the specification in [DISPATCH_TRAPS.md](DISPATCH_TRAPS.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Concept

A dispatch trap is a parent A-line trap that multiplexes sub-functions
through a selector value in a register or on the stack.  Today the
emulator treats these as single traps.  This design makes every
subtrap a first-class entry in `TrapDefs` — with its own name, typed
parameters, and identity — so that the tracer, debugger, and counter
can operate on subtraps exactly as they do on regular traps.

The dispatch mechanism is transparent: the user sees `PBGetCatInfo`,
not `HFSDispatch(selector:9)`.

---

## 2. traps.def Syntax Extension

### 2.1 Dispatch header

The parent trap header gains an optional `dispatch=<type>.<loc>`
suffix that declares the selector location:

```
A260 HFSDispatch os dispatch=word.D0
```

This means: "when this trap fires, read a `word` from `D0` to get
the subtrap selector."

Dispatch location grammar reuses the existing `type.register`
notation from param definitions: `word.D0`, `sword`, `long`, etc.
For stack-based selectors (Toolbox convention), the type alone
suffices — the selector is the topmost argument.

### 2.2 Subtrap blocks

Indented `subtrap` lines follow the parent header:

```
A260 HFSDispatch os dispatch=word.D0
  subtrap 0x01 PBOpenWD
    in  pb:^WDParam.A0
    out err:OSErr.D0
    show-out pb ioResult
  subtrap 0x09 PBGetCatInfo
    in  pb:^CInfoPBRec.A0
    out err:OSErr.D0
    show-in  pb ioNamePtr ioDirID
    show-out pb ioResult ioFlAttrib ioFlFndrInfo
```

Each `subtrap <selector> <Name>` starts a nested block that accepts
the same `in`, `out`, `show-in`, `show-out` directives as a top-level
trap.  The block ends at the next `subtrap`, the next top-level trap
header, or end-of-file.

### 2.3 Backwards compatibility

Existing traps.def entries without `dispatch=` are unchanged.  The
twelve dispatch parents that currently have `in`/`out` lines
(e.g. `in selector:word.D0 pb:Ptr.A0` on HFSDispatch) become the
fallback parameter set: used when a selector doesn't match any
defined subtrap.

---

## 3. Data Model Changes

### 3.1 TrapDef additions

```cpp
// In trap_defs.h — added to struct TrapDef:

struct DispatchInfo
{
    ParamDef selectorParam;     // type + location of the selector
    // (no subtrap storage here — subtraps live in TrapDefs)
};
std::optional<DispatchInfo> dispatch;  // nullopt for non-dispatch traps
```

### 3.2 SubtrapDef

A subtrap is stored as a full `TrapDef` with an additional selector
field.  Its `trapWord` is the parent's trap word (for masking
consistency):

```cpp
struct SubtrapDef
{
    uint16_t selector;          // selector value (e.g. 0x09 for PBGetCatInfo)
    TrapDef def;                // full definition (name, params, show-in/out)
};
```

### 3.3 TrapDefs storage

```cpp
// In class TrapDefs (private):

// Existing:
std::unordered_map<uint16_t, TrapDef> defs_;

// New:
// Key: parent trapWord (masked).  Value: selector → SubtrapDef.
std::unordered_map<uint16_t, std::unordered_map<uint16_t, SubtrapDef>> subtraps_;

// Extended name index (sortedNames_) includes subtrap names.
// Each subtrap entry uses a synthetic 32-bit key for identification:
//   (parentTrapWord << 16) | selector
```

### 3.4 Synthetic keys

Subtraps need unique identifiers for breakpoints, counters, and name
resolution.  A 32-bit composite key avoids collisions with 16-bit
trap words:

```
syntheticKey = (maskedParentTrapWord << 16) | selector
```

Example: `PBGetCatInfo` = `(0xA260 << 16) | 0x0009` = `0xA260'0009`.

Regular traps keep 16-bit keys (zero-extended to 32 bits where
needed).  APIs that currently take `uint16_t trapWord` gain parallel
overloads or are widened to `uint32_t` internally.

---

## 4. Public Interface Changes

### 4.1 TrapDefs

```cpp
// New lookup:
const SubtrapDef *findSubtrap(uint16_t parentTrapWord, uint16_t selector) const;

// Returns the parent if it's a dispatch trap:
bool isDispatch(uint16_t trapWord) const;
const DispatchInfo *dispatchInfo(uint16_t trapWord) const;

// Extended search includes subtrap names:
// (existing search() already iterates sortedNames_ — just ensure
//  subtrap entries are inserted there during load())

// Name lookup by synthetic key:
std::string_view nameOfSubtrap(uint32_t syntheticKey) const;
```

### 4.2 TrapTracer

No public API changes. The behavior change is internal:
`enter(uint16_t trapWord)` detects dispatch traps and resolves
subtraps automatically.

### 4.3 Debugger / Symbols

```cpp
// SymbolsResolve gains subtrap awareness:
// When "PBGetCatInfo" is looked up, it finds the subtrap entry
// in sortedNames_ and returns:
//   outAddr = 0
//   outTrapWord = maskedParentTrapWord (0xA260)
//   outSubtrapSelector = 0x0009  (new output parameter)

bool SymbolsResolve(std::string_view name, uint32_t &outAddr,
                    uint16_t &outTrapWord, uint16_t &outSubtrapSelector);

// Breakpoint struct gains:
uint16_t subtrapSelector = 0;  // non-zero for subtrap breakpoints
```

### 4.4 Trap counter

```cpp
// New per-subtrap counting:
void trap_counter_record_subtrap(uint16_t parentTrapWord, uint16_t selector);
uint32_t trap_counter_get_subtrap(uint16_t parentTrapWord, uint16_t selector);
```

---

## 5. Integration Points

### 5.1 Parser — `TrapDefs::load()` in `src/cpu/trap_defs.cpp` (line 281)

The parsing loop currently recognises two line types: header (no
leading whitespace) and param (leading whitespace).  Extend with:

1. After `parseHeaderLine()` succeeds, check for `dispatch=` in the
   header (already captured by `parseHeaderLine` after the convention
   token).
2. Recognise `subtrap` as a new indented directive in `parseParamLine`.
3. When `subtrap <sel> <name>` is seen, start accumulating params
   into a new `SubtrapDef` instead of the parent.
4. On the next `subtrap`, header, or end: flush the current subtrap
   into `subtraps_[maskedParent][selector]`.
5. Insert subtrap names into `sortedNames_` during the post-load
   index build.

**Cost:** parsing runs once at startup.  No runtime overhead.

### 5.2 Tracer — `TrapTracer::enter()` in `src/cpu/trap_tracer.cpp` (line 87)

Current flow:
```
enter(tw) → find(tw) → emitEntry(frame, def) → push frame
```

New flow for dispatch traps:
```
enter(tw) → find(tw) → if def.dispatch:
    read selector from def.dispatch.selectorParam (via readParamRaw)
    findSubtrap(tw, selector)
    if subtrap found:
        emitEntry(frame, subtrap.def)    // uses subtrap name + params
        frame.trapWord stays as parent   // return detection still works
        frame stores subtrap selector    // for exit-time formatting
    else:
        emitEntry(frame, def)            // fallback to parent
→ push frame
```

The selector read uses the **same** `readParamRaw()` path that
already handles register and stack params.  One `m68k_getRegs()` call
is already made at line 115 to read SP; the register values are
available at that point.

**TrapFrame addition:**
```cpp
uint16_t subtrapSelector = 0;  // for dispatch traps
```

On `checkReturn()`, the exit formatter uses the stored selector to
look up the subtrap def and format return params.

**Cost:** one extra `find` + one register/stack read per dispatch
trap.  Negligible compared to the existing `m68k_getRegs()` +
`emitStr()` + `formatStructDump()` work.

### 5.3 Debugger trap hook — `Debugger::trapHook()` in `src/debugger/debugger.cpp` (line 788)

Current flow:
```
trapHook(tw) → lookupByTrap(tw) → check condition → stop
```

New flow:
```
trapHook(tw) → lookupByTrap(tw)           // parent breakpoint?
    if hit: stop (as before)
    if miss and isDispatch(tw):
        read selector (same as tracer)
        lookupBySubtrap(tw, selector)     // subtrap breakpoint?
        if hit: stop
```

`lookupBySubtrap` scans breakpoints where `trapWord == tw` and
`subtrapSelector == selector`.

**Cost:** for non-dispatch traps, one extra `isDispatch()` check
(hash lookup, returns null — fast).  For dispatch traps without
subtrap breakpoints, one selector read + one scan of breakpoints.

### 5.4 Breakpoint creation — `CmdBreak()` in `src/debugger/cmd_break.cpp` (line 70)

`SymbolsResolve("PBGetCatInfo")` now returns `trapWord=0xA260,
subtrapSelector=0x0009`.  `CmdBreak` passes both to
`addBreakpoint()`.

### 5.5 Symbol resolution — `SymbolsResolve()` in `src/debugger/symbols.cpp` (line 60)

Currently calls `trap_dict_search()` which iterates `sortedNames_`.
Since subtrap names are added to `sortedNames_` during load, the
existing prefix-search finds them.  The only change: when a subtrap
match is found, extract the parent trap word and selector from the
synthetic key and populate both output params.

### 5.6 Trap counter — `trap_counter_record()` in `src/cpu/trap_counter.cpp` (line 38)

Currently called from `DoCodeA()` in `m68k.cpp` (line 4533) with the
raw trap word.  Two options:

**Option A (simpler):** Keep `trap_counter_record(tw)` for the parent.
Add a second call `trap_counter_record_subtrap(tw, selector)` from
the tracer/debugger path only (when dispatch is detected).  This
means subtrap counts are only tracked when the debugger or tracer is
active.

**Option B (comprehensive):** Move the dispatch-detection + selector-
read into `DoCodeA()` itself for the twelve dispatch traps.  This
makes counting always-on but adds a register read to every dispatch
trap execution.

**Recommendation:** Option A.  The counter is a diagnostic tool.
Always-on parent counting is sufficient for the UI overlay; per-
subtrap detail is a debugging feature that only matters when
tracing/breaking.

### 5.7 Trace filtering — `allowed_` bitset in `TrapTracer`

The `allowed_` bitset is `std::bitset<65536>`, indexed by 16-bit trap
word.  It cannot index 32-bit synthetic keys.

**Solution:** Add a second filter map for subtraps:

```cpp
// In TrapTracer (private):
std::unordered_map<uint32_t, bool> subtrapAllowed_;
```

When `trace traps PBGetCatInfo` is issued:
1. `SymbolsResolve` returns `tw=0xA260, selector=0x0009`.
2. `addTrap()` sets `allowed_[0xA260]` = true (so the parent fires).
3. `addSubtrap(0xA260, 0x0009)` sets `subtrapAllowed_[key]` = true.
4. In `enter()`, after reading the selector, check `subtrapAllowed_`.
   If the map is non-empty and the selector is not in it, skip.

When `trace traps HFSDispatch` is issued (no subtrap qualifier),
all subtraps of that parent pass.

### 5.8 `extfs_log.cpp` retirement

`hfsTrapName()` (line 84) and `flatTrapName()` (line 28) in
`src/core/extfs_log.cpp` duplicate subtrap name knowledge.  After
subtraps are in `traps.def`, replace them:

```cpp
const char *hfsTrapName(uint16_t selector)
{
    auto *sub = g_trapDefs.findSubtrap(0xA260, selector);
    if (sub) return sub->def.name.c_str();
    static thread_local char buf[16];
    std::snprintf(buf, sizeof(buf), "HFS$%02X", selector);
    return buf;
}
```

---

## 6. Reused Infrastructure

| Component | How reused |
|-----------|------------|
| `TrapDefs` loader + `traps.def` | Extended with `dispatch=` + `subtrap` syntax.  All existing parsing logic (params, show-in/out) reused as-is for subtrap param blocks. |
| `ParamDef` / `ParamLoc` | Selector location described with the same type+register grammar. |
| `TrapTracer::readParamRaw()` | Reads selector from register or stack — same code path as existing param reads. |
| `TrapTracer::emitEntry()`/`emitExit()` | Called with the subtrap's `TrapDef` — no changes to formatting logic. |
| `SymbolsResolve()` / `trap_dict_search()` | Subtrap names join `sortedNames_` — existing prefix search finds them. |
| `Debugger::Breakpoint` / `lookupByTrap()` | Extended with `subtrapSelector` field; lookup gains one extra comparison. |
| `TypeRegistry` / struct definitions | Subtrap param blocks reference the same type names (`CInfoPBRec`, `WDParam`, etc.) already defined in `types.def`. |
| `m68k_getRegs()` | Already called in `TrapTracer::enter()` to read SP; selector read adds no extra call. |

Nothing is duplicated.  The subtrap mechanism is a natural extension
of the existing `TrapDef` → `TrapTracer` → `Debugger` pipeline.

---

## 7. Build Integration

No new source files.  All changes are in existing files:

| File | Change |
|------|--------|
| `src/cpu/trap_defs.h` | Add `DispatchInfo`, `SubtrapDef`, new lookup methods |
| `src/cpu/trap_defs.cpp` | Extend parser for `dispatch=` and `subtrap` blocks |
| `src/cpu/trap_tracer.h` | Add `subtrapSelector` to `TrapFrame`, subtrap filter map |
| `src/cpu/trap_tracer.cpp` | Dispatch detection + subtrap resolution in `enter()`/`checkReturn()` |
| `src/cpu/trap_counter.h` | Add `trap_counter_record_subtrap()` / `trap_counter_get_subtrap()` |
| `src/cpu/trap_counter.cpp` | Subtrap counter storage |
| `src/debugger/debugger.h` | Add `subtrapSelector` to `Breakpoint`, `lookupBySubtrap()` |
| `src/debugger/debugger.cpp` | Subtrap breakpoint matching in `trapHook()` |
| `src/debugger/symbols.h` | Extend `SymbolsResolve()` signature |
| `src/debugger/symbols.cpp` | Extract subtrap info from synthetic keys |
| `src/debugger/cmd_break.cpp` | Pass subtrap selector to `addBreakpoint()` |
| `src/debugger/cmd_trace.cpp` | Handle subtrap names in `trace traps` filter |
| `src/core/extfs_log.cpp` | Delegate to `TrapDefs` for subtrap names |
| `assets/traps.def` | Add `dispatch=` and `subtrap` blocks for HFS, SCSI, etc. |

---

## 8. Dependency Diagram

```
assets/traps.def
    │
    ▼
TrapDefs (parser + storage)
    │
    ├──────────────────┐
    ▼                  ▼
TrapTracer         trap_counter
    │                  │
    │    ┌─────────────┘
    ▼    ▼
  Debugger (trapHook, breakpoints)
    │
    ▼
  Symbols (name ↔ key resolution)
    │
    ▼
  cmd_break / cmd_trace / cmd_info
```

All arrows point downward.  No circular dependencies.
`TrapDefs` is the single source of truth for names and definitions.

---

## 9. Selector Reading Mechanics

Each dispatch convention needs specific selector extraction:

### 9.1 OS traps with register selector (HFSDispatch)

Selector in D0 as a word.  At `DoCodeA()` time, D0 is live with the
selector.  `readParamRaw()` with `ParamLoc::D0` extracts it.

### 9.2 Toolbox traps with stack selector (SCSIDispatch, TEDispatch, Shutdown, Packs)

Selector is the first (or last, depending on convention) word pushed
on the stack.  For Toolbox traps, parameters are pushed
right-to-left (Pascal convention), so the selector is at the
lowest stack offset among the arguments.

The `DispatchInfo::selectorParam` describes the location.  For stack-
based selectors, `readParamRaw()` computes the offset from SP
automatically using the existing stack-offset arithmetic in
`emitEntry()`.

### 9.3 SlotManager (A06E) — pointer-based selector

SlotManager passes an `SpBlock` pointer in A0.  The selector is at
a fixed offset within the struct.  This is modelled as a register
param (`Ptr.A0`) with a struct field dereference.  The parser treats
this as: selector location is `byte.A0` (the `spSlot` field at
offset 0 of SpBlock), or more practically, the `csCode` field read
from the parameter block.

For SlotManager specifically, the selector byte is at
`SpBlock.spSlot` = offset 0 from A0.  `readParamRaw()` with
`ParamLoc::A0` returns the pointer; the design reads the selector
byte from guest memory at that address.  This needs a special case
in the selector-read path:

```cpp
uint16_t readSelector(const DispatchInfo &info, uint32_t sp)
{
    int stackOff = 0;
    uint32_t raw = readParamRaw(info.selectorParam, sp, stackOff);
    if (info.selectorParam.isStructPtr)
        return static_cast<uint16_t>(get_vm_word(raw)); // deref
    return static_cast<uint16_t>(raw);
}
```

**Fallback:** if a dispatch trap's selector convention is too exotic
to model in the grammar, omit `dispatch=` — it behaves as today
(parent name only, no subtrap resolution).

---

## 10. Testing

### 10.1 Unit tests (doctest)

| Test | What |
|------|------|
| Parse `dispatch=` header | Verify `DispatchInfo` populated correctly |
| Parse `subtrap` blocks | Verify `SubtrapDef` names, selectors, params |
| `findSubtrap()` lookup | Correct subtrap for known selectors, null for unknown |
| Synthetic key round-trip | `(parent << 16 \| sel)` → `nameOfSubtrap()` → correct name |
| `search()` finds subtraps | Prefix "PBGet" returns `PBGetCatInfo`, `PBGetWDInfo`, etc. |
| Fallback to parent | Unknown selector → `find()` returns parent def |

### 10.2 Integration tests (debugger scripts)

```
# test_subtrap_break.dbg
break PBGetCatInfo
continue
# → should stop on HFSDispatch when D0=0x09

# test_subtrap_trace.dbg
trace traps PBGetCatInfo PBOpenWD
continue
# → output should show PBGetCatInfo(...) / PBOpenWD(...)
#   not HFSDispatch(selector:...)
```

### 10.3 Self-test script

Extend `selftest.sh` to boot with a debug script that traces
HFSDispatch subtraps and verifies subtrap names appear in the output.
