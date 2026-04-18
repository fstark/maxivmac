# First-Class Dispatch Trap Subtraps — Implementation Plan

Design: [DISPATCH_TRAPS_DESIGN.md](DISPATCH_TRAPS_DESIGN.md)
Spec: [DISPATCH_TRAPS.md](DISPATCH_TRAPS.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Data types and traps.def syntax extension | |
| 2 | Parser extension with HFSDispatch entries and unit tests | |
| 3 | Lookup API and name index with unit tests | |
| 4 | TrapTracer dispatch resolution | |
| 5 | Symbol resolution and debugger breakpoints | |
| 6 | Command wiring (break, trace) | |
| 7 | Trap counter, extfs_log retirement, remaining traps.def entries | |
| 8 | Integration tests and selftest | |

Build gate: `cmake --preset macos && cmake --build --preset macos`
Test gate:  `cd bld/macos && ctest --output-on-failure`

---

## Phase 1 — Data Types and traps.def Syntax Extension

Add the new structs (`DispatchInfo`, `SubtrapDef`), new storage
members on `TrapDefs`, and the `dispatch=` / `subtrap` syntax to
`assets/traps.def` for HFSDispatch only.  No behavior changes yet —
the parser ignores the new syntax until Phase 2.

### 1.1 — Add `DispatchInfo` and `SubtrapDef` to `trap_defs.h`

File: `src/cpu/trap_defs.h`

After the `TrapDef` struct (line 59), add:

```cpp
struct DispatchInfo
{
    ParamDef selectorParam;  // type + location of the selector
};

struct SubtrapDef
{
    uint16_t selector;  // selector value (e.g. 0x09 for PBGetCatInfo)
    TrapDef def;        // full definition (name, params, show-in/out)
};
```

Add an `std::optional<DispatchInfo>` member to `TrapDef`:

```cpp
struct TrapDef
{
    // ... existing members ...
    std::optional<DispatchInfo> dispatch;  // nullopt for non-dispatch traps
};
```

This requires `#include <optional>` at the top.

### 1.2 — Add subtrap storage to `TrapDefs` class

File: `src/cpu/trap_defs.h`

Add to the private section of `class TrapDefs`:

```cpp
// Key: parent trapWord (masked). Value: selector → SubtrapDef.
std::unordered_map<uint16_t, std::unordered_map<uint16_t, SubtrapDef>> subtraps_;
```

### 1.3 — Extend `assets/traps.def` with HFSDispatch subtraps

File: `assets/traps.def`

Replace the existing HFSDispatch block (lines 228–230):

```
A260 HFSDispatch os dispatch=word.D0
  in  selector:word.D0 pb:Ptr.A0
  out err:OSErr.D0
  subtrap 0x01 PBOpenWD
    in  pb:^WDParam.A0
    out err:OSErr.D0
    show-out pb ioResult
  subtrap 0x02 PBCloseWD
    in  pb:^WDParam.A0
    out err:OSErr.D0
    show-out pb ioResult
  subtrap 0x05 PBCatMove
    in  pb:^CInfoPBRec.A0
    out err:OSErr.D0
    show-in  pb ioNamePtr ioDirID
    show-out pb ioResult
  subtrap 0x06 PBDirCreate
    in  pb:^HFileInfo.A0
    out err:OSErr.D0
    show-in  pb ioNamePtr ioDirID
    show-out pb ioResult ioDirID
  subtrap 0x07 PBGetWDInfo
    in  pb:^WDParam.A0
    out err:OSErr.D0
    show-out pb ioResult ioWDProcID ioWDVRefNum
  subtrap 0x08 PBGetFCBInfo
    in  pb:^FCBPBRec.A0
    out err:OSErr.D0
    show-out pb ioResult
  subtrap 0x09 PBGetCatInfo
    in  pb:^CInfoPBRec.A0
    out err:OSErr.D0
    show-in  pb ioNamePtr ioDirID
    show-out pb ioResult ioFlAttrib ioFlFndrInfo
  subtrap 0x0A PBSetCatInfo
    in  pb:^CInfoPBRec.A0
    out err:OSErr.D0
    show-in  pb ioNamePtr ioDirID
    show-out pb ioResult
  subtrap 0x0B PBSetVInfo
    in  pb:^VolumeParam.A0
    out err:OSErr.D0
    show-out pb ioResult
  subtrap 0x10 PBCreateFileIDRef
    in  pb:^HFileInfo.A0
    out err:OSErr.D0
    show-out pb ioResult
  subtrap 0x11 PBDeleteFileIDRef
    in  pb:^HFileInfo.A0
    out err:OSErr.D0
    show-out pb ioResult
  subtrap 0x30 PBGetVolParms
    in  pb:^HFileInfo.A0
    out err:OSErr.D0
    show-out pb ioResult
```

### Fence

- [ ] `DispatchInfo` and `SubtrapDef` structs exist in `trap_defs.h`
- [ ] `TrapDef::dispatch` optional member exists
- [ ] `TrapDefs::subtraps_` private map exists
- [ ] `assets/traps.def` has `dispatch=word.D0` on HFSDispatch header
- [ ] `assets/traps.def` has 12 `subtrap` blocks under HFSDispatch
- [ ] Full build clean (parser ignores new syntax gracefully — unknown
      header suffix and unknown indented directive are silently skipped
      or produce non-fatal warnings)
- [ ] Commit: `"dispatch-traps: phase 1 — data types and traps.def syntax"`

---

## Phase 2 — Parser Extension with Unit Tests

Extend `TrapDefs::load()` and the parsing helpers to recognise
`dispatch=` on header lines and `subtrap` blocks in indented lines.
Store parsed subtraps in `subtraps_`.  Add unit tests.

### 2.1 — Extend `parseHeaderLine()` to capture `dispatch=`

File: `src/cpu/trap_defs.cpp`

In `parseHeaderLine()` (line 187), after parsing the `conv` token and
checking for `noreturn` in the `extra` token, also check for a
`dispatch=<type>.<loc>` prefix:

```cpp
// After reading `extra` from iss:
if (extra.starts_with("dispatch="))
{
    std::string dispatchSpec = extra.substr(9); // e.g. "word.D0"
    ParamDef selectorParam;
    selectorParam.name = "selector";
    if (ParseParam("selector:" + dispatchSpec, selectorParam))
    {
        out.dispatch = DispatchInfo{std::move(selectorParam)};
    }
    else
    {
        fprintf(stderr, "trap_defs: bad dispatch spec '%s'\n", dispatchSpec.c_str());
    }
}
else if (StrToLower(extra) == "noreturn")
{
    out.noreturn = true;
}
```

This reuses the existing `ParseParam()` helper to parse `type.register`
notation.  The synthetic token `"selector:" + dispatchSpec` produces a
`ParamDef` with the correct type and location.

### 2.2 — Add `subtrap` handling to the `load()` parsing loop

File: `src/cpu/trap_defs.cpp`

The `load()` function (line 273) currently has two branches:

1. Non-indented line → `parseHeaderLine()` → new trap
2. Indented line → `parseParamLine()` → params on current trap

Extend with subtrap state tracking:

```cpp
// New state variables alongside the existing `current` TrapDef:
bool inSubtrap = false;
uint16_t currentSubSelector = 0;
SubtrapDef currentSub;
uint16_t currentParentMasked = 0;
```

In the indented-line branch, before calling `parseParamLine()`, check
if the trimmed line starts with `subtrap`:

```cpp
auto trimmed = TrimLeadingWhitespace(line);
if (trimmed.starts_with("subtrap "))
{
    // Flush previous subtrap if any
    if (inSubtrap)
    {
        subtraps_[currentParentMasked][currentSubSelector] = std::move(currentSub);
    }

    // Parse: "subtrap <hex_selector> <name>"
    std::istringstream iss(std::string(trimmed));
    std::string directive, hexSel, name;
    if (!(iss >> directive >> hexSel >> name))
    {
        fprintf(stderr, "trap_defs: bad subtrap line '%s'\n", line.c_str());
        continue;
    }
    char *end = nullptr;
    unsigned long sel = std::strtoul(hexSel.c_str(), &end, 16);

    inSubtrap = true;
    currentSubSelector = static_cast<uint16_t>(sel);
    currentParentMasked = maskTrapWord(current.trapWord);
    currentSub = SubtrapDef{};
    currentSub.selector = currentSubSelector;
    currentSub.def.trapWord = current.trapWord;
    currentSub.def.name = name;
    currentSub.def.convention = current.convention;
    continue;
}
```

When `inSubtrap` is true, direct `parseParamLine()` to
`currentSub.def` instead of `current`:

```cpp
if (inSubtrap)
    parseParamLine(line, currentSub.def);
else
    parseParamLine(line, current);
```

When a new header line arrives (non-indented), flush the last subtrap:

```cpp
if (inSubtrap)
{
    subtraps_[currentParentMasked][currentSubSelector] = std::move(currentSub);
    inSubtrap = false;
}
```

Also flush at end-of-file (after the loop).

Add a `TrimLeadingWhitespace()` helper (static, file-scope) if one
doesn't exist.  It returns a `std::string_view` of the line with
leading spaces/tabs stripped.

### 2.3 — Unit tests for parser

File: `test/test_tracing.cpp`

Add these test cases:

```cpp
TEST_CASE("TrapDefs parse dispatch= header")
{
    auto path = writeTempFile("test_dispatch.def",
        "A260 HFSDispatch os dispatch=word.D0\n"
        "  in  selector:word.D0 pb:Ptr.A0\n"
        "  out err:OSErr.D0\n");
    TrapDefs defs;
    REQUIRE(defs.load(path) > 0);
    auto *def = defs.find(0xA260);
    REQUIRE(def != nullptr);
    CHECK(def->name == "HFSDispatch");
    REQUIRE(def->dispatch.has_value());
    CHECK(def->dispatch->selectorParam.typeName == "word");
    CHECK(def->dispatch->selectorParam.loc == ParamLoc::D0);
}

TEST_CASE("TrapDefs parse subtrap blocks")
{
    auto path = writeTempFile("test_subtraps.def",
        "A260 HFSDispatch os dispatch=word.D0\n"
        "  in  selector:word.D0 pb:Ptr.A0\n"
        "  out err:OSErr.D0\n"
        "  subtrap 0x09 PBGetCatInfo\n"
        "    in  pb:^CInfoPBRec.A0\n"
        "    out err:OSErr.D0\n"
        "    show-in  pb ioNamePtr ioDirID\n"
        "    show-out pb ioResult ioFlAttrib ioFlFndrInfo\n"
        "  subtrap 0x01 PBOpenWD\n"
        "    in  pb:^WDParam.A0\n"
        "    out err:OSErr.D0\n");
    TrapDefs defs;
    REQUIRE(defs.load(path) > 0);

    auto *sub9 = defs.findSubtrap(0xA260, 0x09);
    REQUIRE(sub9 != nullptr);
    CHECK(sub9->def.name == "PBGetCatInfo");
    CHECK(sub9->selector == 0x09);
    CHECK(sub9->def.paramsIn.size() == 1);
    CHECK(sub9->def.paramsIn[0].isStructPtr == true);
    CHECK(sub9->def.showIn.size() == 1);
    CHECK(sub9->def.showOut.size() == 1);

    auto *sub1 = defs.findSubtrap(0xA260, 0x01);
    REQUIRE(sub1 != nullptr);
    CHECK(sub1->def.name == "PBOpenWD");
}

TEST_CASE("TrapDefs subtrap inherits parent convention")
{
    auto path = writeTempFile("test_sub_conv.def",
        "A260 HFSDispatch os dispatch=word.D0\n"
        "  subtrap 0x09 PBGetCatInfo\n"
        "    in  pb:^CInfoPBRec.A0\n");
    TrapDefs defs;
    REQUIRE(defs.load(path) > 0);
    auto *sub = defs.findSubtrap(0xA260, 0x09);
    REQUIRE(sub != nullptr);
    CHECK(sub->def.convention == TrapConvention::OS);
    CHECK(sub->def.trapWord == 0xA260);
}

TEST_CASE("TrapDefs no dispatch= means no subtraps")
{
    auto path = writeTempFile("test_no_dispatch.def",
        "A000 Open os\n"
        "  in  pb:^IOParam.A0\n");
    TrapDefs defs;
    REQUIRE(defs.load(path) > 0);
    auto *def = defs.find(0xA000);
    REQUIRE(def != nullptr);
    CHECK_FALSE(def->dispatch.has_value());
}
```

### Fence

- [ ] `parseHeaderLine()` populates `TrapDef::dispatch` for `dispatch=` headers
- [ ] `load()` parses `subtrap` blocks and stores in `subtraps_`
- [ ] Existing `in`/`out`/`show-in`/`show-out` directives work inside subtrap blocks
- [ ] All four new unit tests pass
- [ ] All pre-existing tests pass (no regressions)
- [ ] `TrapDefs::load()` on the real `assets/traps.def` succeeds (no errors)
- [ ] Full build clean
- [ ] Commit: `"dispatch-traps: phase 2 — parser extension"`

---

## Phase 3 — Lookup API and Name Index

Expose `findSubtrap()`, `isDispatch()`, `dispatchInfo()`, and
`nameOfSubtrap()` on `TrapDefs`.  Insert subtrap names into
`sortedNames_` so that `search()` and `nameOf()` find them.

### 3.1 — Public lookup methods on `TrapDefs`

File: `src/cpu/trap_defs.h`

Add to the public section of `class TrapDefs`:

```cpp
const SubtrapDef *findSubtrap(uint16_t parentTrapWord, uint16_t selector) const;
bool isDispatch(uint16_t trapWord) const;
const DispatchInfo *dispatchInfo(uint16_t trapWord) const;
std::string_view nameOfSubtrap(uint32_t syntheticKey) const;
```

### 3.2 — Implement lookup methods

File: `src/cpu/trap_defs.cpp`

```cpp
const SubtrapDef *TrapDefs::findSubtrap(uint16_t parentTrapWord, uint16_t selector) const
{
    uint16_t key = maskTrapWord(parentTrapWord);
    auto it = subtraps_.find(key);
    if (it == subtraps_.end()) return nullptr;
    auto sit = it->second.find(selector);
    if (sit == it->second.end()) return nullptr;
    return &sit->second;
}

bool TrapDefs::isDispatch(uint16_t trapWord) const
{
    auto *def = find(trapWord);
    return def && def->dispatch.has_value();
}

const DispatchInfo *TrapDefs::dispatchInfo(uint16_t trapWord) const
{
    auto *def = find(trapWord);
    if (!def || !def->dispatch) return nullptr;
    return &*def->dispatch;
}

std::string_view TrapDefs::nameOfSubtrap(uint32_t syntheticKey) const
{
    uint16_t parent = static_cast<uint16_t>(syntheticKey >> 16);
    uint16_t sel = static_cast<uint16_t>(syntheticKey & 0xFFFF);
    auto *sub = findSubtrap(parent, sel);
    if (sub) return sub->def.name;
    return {};
}
```

### 3.3 — Insert subtrap names into `sortedNames_`

File: `src/cpu/trap_defs.cpp`

In the `load()` function, after the existing name-index build
(lines 313–318), add subtrap entries using synthetic 32-bit keys.

Since `sortedNames_` currently stores `std::pair<uint16_t, std::string>`,
it cannot hold 32-bit synthetic keys directly.  Two approaches:

**Option A:** Widen `sortedNames_` to `uint32_t`.  Regular traps use
`(uint32_t)trapWord`, subtraps use `(parent << 16) | selector`.

**Option B:** Keep a separate `subtrapSortedNames_` vector.  The
`search()` method merges both.

**Chosen:** Option A — cleaner, single lookup path.

Change `sortedNames_` type declaration in `trap_defs.h`:

```cpp
std::vector<std::pair<uint32_t, std::string>> sortedNames_;
```

Update existing code that iterates `sortedNames_`:
- `entry()` — cast key to `uint16_t` for backwards compat (or widen return)
- `nameOf()` — iterate with `uint32_t` comparison
- `search()` — no changes needed (searches by name, returns key)

In `load()`, extend the name-index build:

```cpp
sortedNames_.clear();
sortedNames_.reserve(defs_.size() + /* estimate subtrap count */ 64);
for (auto &[tw, def] : defs_)
    sortedNames_.push_back({static_cast<uint32_t>(def.trapWord), def.name});
for (auto &[parentTw, selMap] : subtraps_)
{
    for (auto &[sel, sub] : selMap)
    {
        uint32_t synKey = (static_cast<uint32_t>(parentTw) << 16) | sel;
        sortedNames_.push_back({synKey, sub.def.name});
    }
}
std::sort(sortedNames_.begin(), sortedNames_.end(),
          [](auto &a, auto &b) { return a.second < b.second; });
```

### 3.4 — Update `entry()` and `search()` return types

File: `src/cpu/trap_defs.h`

Change `entry()` to return `uint32_t`:

```cpp
std::pair<uint32_t, std::string_view> entry(int index) const;
```

Update `search()` result type:

```cpp
void search(std::string_view prefix,
            std::vector<std::pair<uint32_t, std::string_view>> &results,
            int maxResults = 20) const;
```

File: `src/cpu/trap_defs.cpp` — update the implementations accordingly.

Check all callers of `entry()` and `search()` — update them to accept
`uint32_t` keys.  Callers that need a `uint16_t` trapWord can mask:
if `key <= 0xFFFF` it's a regular trap; otherwise it's a subtrap.

### 3.5 — Update callers of widened API

Files to check and update:
- `src/cpu/trap_counter.cpp` — `trap_dict_entry()`, `trap_dict_search()`
  return `TrapInfo` which has `uint16_t trapWord`.  Widen
  `TrapInfo::trapWord` to `uint32_t`, or add a `subtrapSelector`
  field.  **Chosen:** add `uint16_t subtrapSelector = 0` to `TrapInfo`.
- `src/cpu/trap_counter.h` — update `TrapInfo` struct.
- `src/debugger/symbols.cpp` — `SymbolsSearch()` and
  `SymbolsResolve()` use `TrapInfo`.  Follow updated struct.

### 3.6 — Unit tests for lookup API

File: `test/test_tracing.cpp`

```cpp
TEST_CASE("TrapDefs findSubtrap lookup")
{
    auto path = writeTempFile("test_find_sub.def",
        "A260 HFSDispatch os dispatch=word.D0\n"
        "  subtrap 0x09 PBGetCatInfo\n"
        "    in  pb:^CInfoPBRec.A0\n"
        "  subtrap 0x01 PBOpenWD\n"
        "    in  pb:^WDParam.A0\n");
    TrapDefs defs;
    defs.load(path);

    CHECK(defs.isDispatch(0xA260));
    CHECK_FALSE(defs.isDispatch(0xA000));

    auto *info = defs.dispatchInfo(0xA260);
    REQUIRE(info != nullptr);
    CHECK(info->selectorParam.loc == ParamLoc::D0);

    auto *sub = defs.findSubtrap(0xA260, 0x09);
    REQUIRE(sub != nullptr);
    CHECK(sub->def.name == "PBGetCatInfo");

    CHECK(defs.findSubtrap(0xA260, 0xFF) == nullptr);
    CHECK(defs.findSubtrap(0xA000, 0x01) == nullptr);
}

TEST_CASE("TrapDefs synthetic key name lookup")
{
    auto path = writeTempFile("test_synkey.def",
        "A260 HFSDispatch os dispatch=word.D0\n"
        "  subtrap 0x09 PBGetCatInfo\n"
        "    in  pb:^CInfoPBRec.A0\n");
    TrapDefs defs;
    defs.load(path);

    uint32_t key = (0xA060u << 16) | 0x0009u;  // masked A260 = A060
    CHECK(defs.nameOfSubtrap(key) == "PBGetCatInfo");
    CHECK(defs.nameOfSubtrap(0x00000000).empty());
}

TEST_CASE("TrapDefs search finds subtrap names")
{
    auto path = writeTempFile("test_search_sub.def",
        "A260 HFSDispatch os dispatch=word.D0\n"
        "  subtrap 0x09 PBGetCatInfo\n"
        "    in  pb:^CInfoPBRec.A0\n"
        "  subtrap 0x07 PBGetWDInfo\n"
        "    in  pb:^WDParam.A0\n"
        "A000 Open os\n"
        "  in  pb:^IOParam.A0\n");
    TrapDefs defs;
    defs.load(path);

    std::vector<std::pair<uint32_t, std::string_view>> results;
    defs.search("PBGet", results);
    CHECK(results.size() == 2);
    // Both PBGetCatInfo and PBGetWDInfo should appear
}
```

### Fence

- [ ] `findSubtrap()`, `isDispatch()`, `dispatchInfo()`, `nameOfSubtrap()` work
- [ ] `sortedNames_` includes subtrap entries with synthetic 32-bit keys
- [ ] `search("PBGet")` finds subtrap names
- [ ] `TrapInfo` struct has `subtrapSelector` field
- [ ] All callers of `entry()`/`search()` compile with widened types
- [ ] All new and pre-existing unit tests pass
- [ ] `TrapDefs::load()` on real `assets/traps.def` succeeds
- [ ] Full build clean
- [ ] Commit: `"dispatch-traps: phase 3 — lookup API and name index"`

---

## Phase 4 — TrapTracer Dispatch Resolution

Extend `TrapTracer::enter()` to detect dispatch traps, read the
selector, resolve subtraps, and emit subtrap-aware trace output.
Extend `checkReturn()` for subtrap exit formatting.  Add the subtrap
filter map.

### 4.1 — Add `subtrapSelector` to `TrapFrame`

File: `src/cpu/trap_tracer.h`

Add to `struct TrapFrame` (after the `depth` field):

```cpp
uint16_t subtrapSelector = 0;  // non-zero for dispatch traps
```

### 4.2 — Add subtrap filter map to `TrapTracer`

File: `src/cpu/trap_tracer.h`

Add to the private section of `class TrapTracer`:

```cpp
// Subtrap-level filter: syntheticKey → allowed.
// Empty = all subtraps of an allowed parent pass.
std::unordered_map<uint32_t, bool> subtrapAllowed_;
```

Add public methods:

```cpp
void addSubtrap(uint16_t parentTrapWord, uint16_t selector);
void removeSubtrap(uint16_t parentTrapWord, uint16_t selector);
bool hasSubtrapFilter() const;
```

### 4.3 — Implement `readSelector()` private method

File: `src/cpu/trap_tracer.h` — add to the private section of
`TrapTracer`:

```cpp
uint16_t readSelector(const DispatchInfo &info, uint32_t sp);
```

File: `src/cpu/trap_tracer.cpp`

```cpp
uint16_t TrapTracer::readSelector(const DispatchInfo &info, uint32_t sp)
{
    int stackOff = 0;
    uint32_t raw = readParamRaw(info.selectorParam, sp, stackOff);
    if (info.selectorParam.isStructPtr)
        return static_cast<uint16_t>(get_vm_word(raw)); // deref pointer
    return static_cast<uint16_t>(raw);
}
```

This reuses the existing `readParamRaw()` (already accessible as a
private member) to extract the selector from the register or stack
location specified by `DispatchInfo`.

### 4.4 — Extend `enter()` for dispatch resolution

File: `src/cpu/trap_tracer.cpp`

In `enter()` (line 87), after finding the parent `TrapDef` via
`defs_.find(trapWord)`, add dispatch detection:

```cpp
const TrapDef *effectiveDef = def;
uint16_t subtrapSel = 0;
if (def && def->dispatch)
{
    subtrapSel = readSelector(*def->dispatch, frame.sp);
    auto *sub = defs_.findSubtrap(trapWord, subtrapSel);
    if (sub)
    {
        effectiveDef = &sub->def;
    }
    // Check subtrap filter
    if (!subtrapAllowed_.empty())
    {
        uint32_t synKey = (static_cast<uint32_t>(
            TrapDefs::maskTrapWord(trapWord)) << 16) | subtrapSel;
        if (subtrapAllowed_.find(synKey) == subtrapAllowed_.end())
            return;  // filtered out
    }
}
frame.subtrapSelector = subtrapSel;
```

Then pass `effectiveDef` to `emitEntry()` instead of `def`.  The
frame's `trapWord` stays as the parent (return detection still works).

Note: `maskTrapWord` is currently a private static method.  Either
make it public or add a public static wrapper.  **Chosen:** make
`maskTrapWord` public static (it's a pure function, safe to expose).

### 4.5 — Extend `checkReturn()` for subtrap exit

File: `src/cpu/trap_tracer.cpp`

In `checkReturn()` (line 174), when emitting the exit, look up the
subtrap if `frame.subtrapSelector != 0`:

```cpp
if (frame.subtrapSelector != 0)
{
    auto *sub = defs_.findSubtrap(frame.trapWord, frame.subtrapSelector);
    if (sub)
    {
        emitExit(frame, sub->def);
        // skip the normal emitExit
    }
}
```

### 4.6 — Implement filter methods

File: `src/cpu/trap_tracer.cpp`

```cpp
void TrapTracer::addSubtrap(uint16_t parentTrapWord, uint16_t selector)
{
    uint32_t synKey = (static_cast<uint32_t>(
        TrapDefs::maskTrapWord(parentTrapWord)) << 16) | selector;
    subtrapAllowed_[synKey] = true;
    // Also ensure parent is allowed (so enter() fires)
    allowed_.set(parentTrapWord);
}

void TrapTracer::removeSubtrap(uint16_t parentTrapWord, uint16_t selector)
{
    uint32_t synKey = (static_cast<uint32_t>(
        TrapDefs::maskTrapWord(parentTrapWord)) << 16) | selector;
    subtrapAllowed_.erase(synKey);
}

bool TrapTracer::hasSubtrapFilter() const
{
    return !subtrapAllowed_.empty();
}
```

### Fence

- [ ] `TrapFrame::subtrapSelector` field exists
- [ ] `enter()` detects dispatch traps via `TrapDef::dispatch`
- [ ] `enter()` reads selector and resolves subtrap name/params
- [ ] `checkReturn()` uses subtrap def for exit formatting
- [ ] Subtrap filter map controls per-subtrap trace output
- [ ] `maskTrapWord()` is public static on `TrapDefs`
- [ ] `readSelector()` private method exists on `TrapTracer`
- [ ] `ReadTrapSelector()` free function exists for shared use by debugger
- [ ] Tracing `HFSDispatch` shows `PBGetCatInfo(...)` not `HFSDispatch(selector:9)`
- [ ] Full build clean, all tests pass
- [ ] Commit: `"dispatch-traps: phase 4 — tracer dispatch resolution"`

---

## Phase 5 — Symbol Resolution and Debugger Breakpoints

Extend `SymbolsResolve()` to return subtrap selectors.  Add
`subtrapSelector` to `Breakpoint`.  Extend `trapHook()` for subtrap
breakpoint matching.

### 5.1 — Extend `SymbolsResolve()` signature

File: `src/debugger/symbols.h`

Change the signature to include a subtrap selector output:

```cpp
bool SymbolsResolve(std::string_view name, uint32_t &outAddr,
                    uint16_t &outTrapWord, uint16_t &outSubtrapSelector);
```

Add a backward-compatible overload that ignores the selector:

```cpp
bool SymbolsResolve(std::string_view name, uint32_t &outAddr,
                    uint16_t &outTrapWord);
```

### 5.2 — Implement subtrap-aware `SymbolsResolve()`

File: `src/debugger/symbols.cpp`

The existing implementation calls `trap_dict_search()` which iterates
`sortedNames_`.  Now that `TrapInfo` has `subtrapSelector`, the search
results carry the selector.

In `SymbolsResolve()` (line 48), when a match is found and
`ti.subtrapSelector != 0`:

```cpp
outSubtrapSelector = ti.subtrapSelector;
outTrapWord = ti.trapWord;  // this is already the parent trap word
outAddr = 0;
return true;
```

The backward-compatible overload simply discards the selector output.

### 5.3 — Populate `TrapInfo::subtrapSelector` in `trap_dict_search()`

File: `src/cpu/trap_counter.cpp`

In the function that builds `TrapInfo` entries from `TrapDefs::search()`,
decode the synthetic key:

```cpp
if (key > 0xFFFF)
{
    info.trapWord = static_cast<uint16_t>(key >> 16);
    info.subtrapSelector = static_cast<uint16_t>(key & 0xFFFF);
}
else
{
    info.trapWord = static_cast<uint16_t>(key);
    info.subtrapSelector = 0;
}
```

### 5.4 — Add `subtrapSelector` to `Breakpoint`

File: `src/debugger/debugger.h`

Add to `struct Breakpoint`:

```cpp
uint16_t subtrapSelector = 0;  // non-zero for subtrap breakpoints
```

### 5.5 — Extend `addBreakpoint()` signature

File: `src/debugger/debugger.h`

Add a subtrap-aware overload or extend the existing signature:

```cpp
uint32_t addBreakpoint(uint32_t addr, uint16_t trapWord,
                       uint16_t subtrapSelector,
                       const std::string &condition);
```

Keep the old two-arg version as an inline overload for compatibility:

```cpp
uint32_t addBreakpoint(uint32_t addr, uint16_t trapWord,
                       const std::string &condition)
{
    return addBreakpoint(addr, trapWord, 0, condition);
}
```

File: `src/debugger/debugger.cpp` — update `addBreakpoint()`
implementation to store `subtrapSelector`.

### 5.6 — Add `lookupBySubtrap()`

File: `src/debugger/debugger.h`

```cpp
const Breakpoint *lookupBySubtrap(uint16_t trapWord, uint16_t selector) const;
```

File: `src/debugger/debugger.cpp`

```cpp
const Debugger::Breakpoint *Debugger::lookupBySubtrap(
    uint16_t trapWord, uint16_t selector) const
{
    for (auto &bp : impl_->breakpoints)
    {
        if (bp.enabled && bp.trapWord == trapWord &&
            bp.subtrapSelector == selector && bp.subtrapSelector != 0)
            return &bp;
    }
    return nullptr;
}
```

### 5.7 — Extend `trapHook()` for subtrap matching

File: `src/debugger/debugger.cpp`

In `trapHook()` (line 788), after the existing `lookupByTrap()` check,
add subtrap resolution:

```cpp
// After existing parent breakpoint check:
if (!bp && g_trapDefs.isDispatch(trapWord))
{
    auto *info = g_trapDefs.dispatchInfo(trapWord);
    if (info)
    {
        // Read selector (same as tracer)
        uint32_t dregs[8], aregs[8];
        m68k_getRegs(dregs, aregs);
        uint16_t selector = /* read from info->selectorParam */;
        bp = lookupBySubtrap(trapWord, selector);
    }
}
```

The selector read logic should use a shared free function callable
from both the tracer and the debugger.  Extract the core of
`TrapTracer::readSelector()` into a free function in
`src/cpu/trap_defs.cpp` (or a small new header):

```cpp
// In trap_defs.h — free function (PascalCase per NAMING.md):
uint16_t ReadTrapSelector(const DispatchInfo &info);
```

Implementation reads the selector from the current CPU register
state via `m68k_getRegs()` and, for struct-pointer selectors,
dereferences guest memory.  `TrapTracer::readSelector()` becomes a
thin wrapper around this free function (passing the frame's SP for
stack-based selectors).

### 5.8 — Update `SymbolsTrapName()` for subtraps

File: `src/debugger/symbols.cpp`

The `SymbolsTrapName(uint16_t tw)` function only takes a trap word.
Add a subtrap-aware overload:

```cpp
const char *SymbolsSubtrapName(uint16_t trapWord, uint16_t selector);
```

Implementation: call `g_trapDefs.findSubtrap()`, return
`sub->def.name.c_str()` or format as `"ParentName$XX"`.

### Fence

- [ ] `SymbolsResolve("PBGetCatInfo")` returns `trapWord=0xA260, subtrapSelector=0x0009`
- [ ] `Breakpoint::subtrapSelector` field exists
- [ ] `lookupBySubtrap()` finds subtrap breakpoints
- [ ] `trapHook()` reads selector and matches subtrap breakpoints
- [ ] Backward-compatible overloads compile for all existing callers
- [ ] Full build clean, all tests pass
- [ ] Commit: `"dispatch-traps: phase 5 — symbols and debugger breakpoints"`

---

## Phase 6 — Command Wiring (break, trace)

Update `CmdBreak` and `CmdTrace` to pass subtrap selectors through
to the breakpoint and filter systems.

### 6.1 — Update `CmdBreak` for subtrap names

File: `src/debugger/cmd_break.cpp`

In `CmdBreak()` (line 70), `SymbolsResolve()` already returns the
subtrap selector (Phase 5).  Pass it to `addBreakpoint()`:

```cpp
uint16_t subtrapSelector = 0;
if (SymbolsResolve(name, addr, trapWord, subtrapSelector))
{
    auto id = dbg.addBreakpoint(addr, trapWord, subtrapSelector, condition);
    // ...
}
```

Update the confirmation message to show the subtrap name:

```cpp
if (subtrapSelector != 0)
{
    auto *sub = g_trapDefs.findSubtrap(trapWord, subtrapSelector);
    io.write("Breakpoint %u on subtrap %s ($%04X sel $%02X)\n",
             id, sub ? sub->def.name.c_str() : "?",
             trapWord, subtrapSelector);
}
else
{
    io.write("Breakpoint %u on trap %s ($%04X)\n",
             id, SymbolsTrapName(trapWord), trapWord);
}
```

### 6.2 — Update `CmdTrace` for subtrap filter

File: `src/debugger/cmd_trace.cpp`

In the `trace traps <names>` path (lines 47–60), when
`SymbolsResolve()` returns a subtrap, call `addSubtrap()` on the
tracer instead of `addTrap()`:

```cpp
uint16_t subtrapSelector = 0;
if (SymbolsResolve(name, addr, trapWord, subtrapSelector))
{
    if (subtrapSelector != 0)
    {
        dbg.addSubtrap(trapWord, subtrapSelector);
        // addSubtrap ensures parent is also in allowed_
    }
    else
    {
        dbg.addTrap(trapWord);
    }
}
```

This requires forwarding `addSubtrap()` / `removeSubtrap()` through
the `Debugger` class to the `TrapTracer`:

File: `src/debugger/debugger.h` — add:

```cpp
void addSubtrap(uint16_t trapWord, uint16_t selector);
void removeSubtrap(uint16_t trapWord, uint16_t selector);
```

File: `src/debugger/debugger.cpp` — implement as delegation to
`impl_->tracer->addSubtrap(...)`.

### 6.3 — Update `info traps` for subtrap display (if applicable)

If there is a `cmd_info` command that lists traps, ensure subtrap
names appear in its output.  Since subtraps are already in
`sortedNames_` (Phase 3), the existing `search()` API should surface
them.  Verify and adjust formatting if needed.

### Fence

- [ ] `break PBGetCatInfo` creates a breakpoint with `subtrapSelector=0x09`
- [ ] `trace traps PBGetCatInfo PBOpenWD` traces only those two HFS calls
- [ ] `trace traps HFSDispatch` traces all HFS subtraps (no subtrap filter)
- [ ] `info traps PBGet*` shows subtrap matches
- [ ] All existing break/trace commands still work unchanged
- [ ] Full build clean, all tests pass
- [ ] Commit: `"dispatch-traps: phase 6 — command wiring"`

---

## Phase 7 — Trap Counter, extfs_log Retirement, Remaining traps.def Entries

Add per-subtrap counting (Option A from design: only when
tracer/debugger is active).  Replace hardcoded subtrap names in
`extfs_log.cpp`.  Add all remaining dispatch trap definitions to
`traps.def`.

### 7.1 — Add subtrap counter API

File: `src/cpu/trap_counter.h`

```cpp
void trap_counter_record_subtrap(uint16_t parentTrapWord, uint16_t selector);
uint32_t trap_counter_get_subtrap(uint16_t parentTrapWord, uint16_t selector);
```

File: `src/cpu/trap_counter.cpp`

Storage: a simple map (subtraps are diagnostic, not hot-path):

```cpp
static std::unordered_map<uint32_t, std::atomic<uint32_t>> s_subtrapCounters;

void trap_counter_record_subtrap(uint16_t parentTrapWord, uint16_t selector)
{
    uint32_t key = (static_cast<uint32_t>(parentTrapWord) << 16) | selector;
    s_subtrapCounters[key].fetch_add(1, std::memory_order_relaxed);
}

uint32_t trap_counter_get_subtrap(uint16_t parentTrapWord, uint16_t selector)
{
    uint32_t key = (static_cast<uint32_t>(parentTrapWord) << 16) | selector;
    auto it = s_subtrapCounters.find(key);
    if (it != s_subtrapCounters.end())
        return it->second.load(std::memory_order_relaxed);
    return 0;
}
```

Call `trap_counter_record_subtrap()` from `TrapTracer::enter()` when
a subtrap is resolved, and/or from `Debugger::trapHook()`.

### 7.2 — Retire `extfs_log.cpp` hardcoded names

File: `src/core/extfs_log.cpp`

Replace `hfsTrapName()` (line 84):

```cpp
static const char *hfsTrapName(uint16_t selector)
{
    auto *sub = g_trapDefs.findSubtrap(0xA260, selector);
    if (sub) return sub->def.name.c_str();
    static thread_local char buf[16];
    std::snprintf(buf, sizeof(buf), "HFS$%02X", selector);
    return buf;
}
```

This requires `#include "cpu/trap_defs.h"` and access to `g_trapDefs`.
Check if `g_trapDefs` is already accessible from this translation
unit; if not, add `extern TrapDefs g_trapDefs;` or use the existing
global accessor.

Keep `flatTrapName()` as-is for now — flat File Manager traps are not
dispatch traps (they're separate A-line traps with similar names).

### 7.3 — Add remaining dispatch traps to `assets/traps.def`

File: `assets/traps.def`

Add `dispatch=` and `subtrap` blocks for:

**SCSIDispatch (A815):**
```
A815 SCSIDispatch toolbox dispatch=sword
  in  selector:sword
  subtrap 0x00 SCSIReset
    void
  subtrap 0x01 SCSIGet
    void
  subtrap 0x02 SCSISelect
    in  targetID:sword
    out err:OSErr
  subtrap 0x03 SCSICmd
    in  buffer:Ptr  count:word
    out err:OSErr
  subtrap 0x04 SCSIComplete
    in  stat:Ptr  message:Ptr  wait:long
    out err:OSErr
  subtrap 0x05 SCSIRead
    in  tibPtr:Ptr
    out err:OSErr
  subtrap 0x06 SCSIWrite
    in  tibPtr:Ptr
    out err:OSErr
  subtrap 0x07 SCSIInstall
    void
  subtrap 0x08 SCSIRBlind
    in  tibPtr:Ptr
    out err:OSErr
  subtrap 0x09 SCSIWBlind
    in  tibPtr:Ptr
    out err:OSErr
  subtrap 0x0A SCSIStat
    out stat:word
  subtrap 0x0B SCSISelAtn
    in  targetID:sword
    out err:OSErr
  subtrap 0x0C SCSIMsgIn
    out message:word
  subtrap 0x0D SCSIMsgOut
    in  message:sword
    out err:OSErr
```

**TEDispatch (A83D):**
```
A83D TEDispatch toolbox dispatch=sword
  in  selector:sword
  subtrap 0x00 TEStylePaste
    in  te:Handle
  subtrap 0x01 TESetStyle
    in  mode:sword  newStyle:Ptr  redraw:Boolean  te:Handle
  subtrap 0x02 TEReplaceStyle
    in  mode:sword  oldStyle:Ptr  newStyle:Ptr  redraw:Boolean  te:Handle
  subtrap 0x03 TEGetStyle
    in  offset:sword  theStyle:Ptr  lineHeight:Ptr  fontAscent:Ptr  te:Handle
  subtrap 0x04 GetStyleHandle
    in  te:Handle
    out style:Handle
  subtrap 0x05 SetStyleHandle
    in  style:Handle  te:Handle
  subtrap 0x06 GetStyleScrap
    in  te:Handle
    out scrap:Handle
  subtrap 0x07 TEStyleInsert
    in  text:Ptr  length:long  style:Handle  te:Handle
  subtrap 0x08 TEGetPoint
    in  offset:sword  te:Handle
    out pt:long
  subtrap 0x09 TEGetHeight
    in  endLine:long  startLine:long  te:Handle
    out height:long
```

**Shutdown (A895):**
```
A895 Shutdown toolbox dispatch=sword
  in  selector:sword
  subtrap 0x01 ShutDwnPower
    void
  subtrap 0x02 ShutDwnStart
    void
  subtrap 0x03 ShutDwnInstall
    in  proc:Ptr  flags:sword
  subtrap 0x04 ShutDwnRemove
    in  proc:Ptr
```

**SlotManager (A06E):**
```
A06E SlotManager os dispatch=byte.A0
  in  pb:Ptr.A0
  out err:OSErr.D0
  subtrap 0x00 sReadByte
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x01 sReadWord
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x02 sReadLong
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x03 sGetcString
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x05 sGetBlock
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x06 sFindStruct
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x07 sReadStruct
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x10 sReadInfo
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x11 sReadPRAMRec
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x12 sPutPRAMRec
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x13 sReadFHeader
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x14 sNextRsrc
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x15 sNextTypesRsrc
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x16 sRsrcInfo
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x17 sDisposePtr
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x18 sCkCardStatus
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x19 sReadDrvrName
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x1B sFindDevBase
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x20 InitSDeclMgr
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x21 sPrimaryInit
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x22 sCardChanged
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x23 sExec
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x24 sOffsetData
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x25 InitPRAMRecs
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x26 sReadPBSize
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x28 sCalcStep
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x29 InitsRsrcTable
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x2A sSearchSRT
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x2B sUpdateSRT
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x2C sCalcsPointer
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x2D sGetDriver
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x2E sPtrToSlot
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x2F sFindsInfoRecPtr
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x30 sFindsRsrcPtr
    in  pb:Ptr.A0
    out err:OSErr.D0
  subtrap 0x31 sdeleteSRTRec
    in  pb:Ptr.A0
    out err:OSErr.D0
```

**ScriptUtil (A8B5):**
```
A8B5 ScriptUtil toolbox dispatch=long
  in  selector:long
  subtrap 0x00 smFontScript
    out result:sword
  subtrap 0x02 smIntlScript
    out result:sword
  subtrap 0x04 smKybdScript
    out result:sword
  subtrap 0x06 smFont2Script
    in  fontNum:sword
    out result:sword
  subtrap 0x08 smGetEnvirons
    in  verb:sword
    out result:long
  subtrap 0x0A smSetEnvirons
    in  verb:sword  value:long
    out result:OSErr
  subtrap 0x0C smGetScript
    in  script:sword  verb:sword
    out result:long
  subtrap 0x0E smSetScript
    in  script:sword  verb:sword  value:long
    out result:OSErr
  subtrap 0x10 smCharByte
    in  text:Ptr  offset:sword  script:sword
    out result:sword
  subtrap 0x12 smCharType
    in  text:Ptr  offset:sword  script:sword
    out result:sword
  subtrap 0x14 smPixel2Char
    in  text:Ptr  length:sword  slop:sword  pixelWidth:Ptr  leadingEdge:Ptr  widthRemaining:Ptr  styleRun:sword  numer:long  denom:long
    out result:sword
  subtrap 0x16 smChar2Pixel
    in  text:Ptr  length:sword  slop:sword  offset:sword  direction:sword  styleRun:sword  numer:long  denom:long
    out result:sword
  subtrap 0x18 smTranslit
    in  srcHandle:Handle  dstHandle:Handle  target:sword  srcMask:long
    out result:OSErr
  subtrap 0x1A smFindWord
    in  text:Ptr  length:sword  offset:sword  leadingEdge:Boolean  breaks:Ptr  offsets:Ptr
  subtrap 0x1C smHiliteText
    in  text:Ptr  length:sword  range1:Ptr  range2:Ptr  offsets:Ptr
  subtrap 0x1E smDrawJust
    in  text:Ptr  length:sword  slop:sword  styleRun:sword  numer:long  denom:long
  subtrap 0x20 smMeasureJust
    in  text:Ptr  length:sword  slop:sword  charLocs:Ptr  styleRun:sword  numer:long  denom:long
```

### Fence

- [ ] `trap_counter_record_subtrap()` and `trap_counter_get_subtrap()` work
- [ ] `hfsTrapName()` in `extfs_log.cpp` delegates to `TrapDefs`
- [ ] All six dispatch traps (HFS, SCSI, TE, Shutdown, SlotManager, ScriptUtil)
      have subtrap entries in `assets/traps.def`
- [ ] `TrapDefs::load()` on updated `traps.def` succeeds with no errors
- [ ] Full build clean, all tests pass
- [ ] Commit: `"dispatch-traps: phase 7 — counter, extfs_log, remaining traps.def"`

---

## Phase 8 — Integration Tests and Selftest

Verify end-to-end behaviour with debugger scripts and extend the
selftest to cover dispatch trap tracing.

### 8.1 — Debugger test script: subtrap breakpoint

File: `test/test_subtrap_break.dbg`

```
break PBGetCatInfo
continue
# Should stop on HFSDispatch when D0=0x09
```

### 8.2 — Debugger test script: subtrap trace filter

File: `test/test_subtrap_trace.dbg`

```
trace traps PBGetCatInfo PBOpenWD
continue
# Output should show PBGetCatInfo(...) / PBOpenWD(...)
# not HFSDispatch(selector:...)
```

### 8.3 — Unit test: full traps.def subtrap count

File: `test/test_tracing.cpp`

```cpp
TEST_CASE("TrapDefs load actual traps.def has dispatch subtraps")
{
    TrapDefs defs;
    int n = defs.load("assets/traps.def");
    REQUIRE(n > 0);

    // HFSDispatch should have subtraps
    CHECK(defs.isDispatch(0xA260));
    auto *sub = defs.findSubtrap(0xA260, 0x09);
    REQUIRE(sub != nullptr);
    CHECK(sub->def.name == "PBGetCatInfo");

    // SCSIDispatch should have subtraps
    CHECK(defs.isDispatch(0xA815));
    auto *scsi = defs.findSubtrap(0xA815, 0x02);
    REQUIRE(scsi != nullptr);
    CHECK(scsi->def.name == "SCSISelect");

    // Search finds subtraps alongside regular traps
    std::vector<std::pair<uint32_t, std::string_view>> results;
    defs.search("SCSI", results, 50);
    bool foundSCSISelect = false;
    for (auto &[key, name] : results)
        if (name == "SCSISelect") foundSCSISelect = true;
    CHECK(foundSCSISelect);
}
```

### 8.4 — Unit test: tracer emits subtrap name

File: `test/test_tracing.cpp`

```cpp
TEST_CASE("TrapTracer emits subtrap name for dispatch trap")
{
    ensureTypeRegistryInit();
    auto path = writeTempFile("test_trace_sub.def",
        "A260 HFSDispatch os dispatch=word.D0\n"
        "  in  selector:word.D0 pb:Ptr.A0\n"
        "  out err:OSErr.D0\n"
        "  subtrap 0x09 PBGetCatInfo\n"
        "    in  pb:^CInfoPBRec.A0\n"
        "    out err:OSErr.D0\n");
    TrapDefs defs;
    defs.load(path);

    TrapTracer tracer(defs);
    CaptureIO io;
    tracer.setIO(&io);
    tracer.enable(true);
    tracer.addAllTraps();

    // Set D0 = 0x09 (PBGetCatInfo selector), A0 = some PB address
    uint32_t dregs[8] = {}, aregs[8] = {};
    dregs[0] = 0x0009;  // D0 = selector
    aregs[0] = 0x1000;  // A0 = PB pointer
    test_set_regs(dregs, aregs);

    // Set SP and return address
    put_be32(0x2000, 0x00004000);  // return PC on stack
    test_set_pc(0x00003000);
    g_instructionCount = 100;

    tracer.enter(0xA260);

    CHECK(io.captured.find("PBGetCatInfo") != std::string::npos);
    CHECK(io.captured.find("HFSDispatch") == std::string::npos);
}
```

### 8.5 — Extend selftest (optional)

If `selftest.sh` supports debug script injection, add a mode that
boots with `test_subtrap_trace.dbg` and verifies subtrap names
appear in the trace log.  Otherwise, manual verification during
development suffices — the unit tests provide the primary coverage.

### Fence

- [ ] `test_subtrap_break.dbg` and `test_subtrap_trace.dbg` exist
- [ ] "load actual traps.def has dispatch subtraps" test passes
- [ ] "TrapTracer emits subtrap name" test passes
- [ ] All pre-existing tests pass (no regressions)
- [ ] Full build clean
- [ ] Commit: `"dispatch-traps: phase 8 — integration tests"`
