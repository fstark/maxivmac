# Trap Dictionary Unification — Implementation Plan

Spec: this document (no separate spec/design)
Related: [traps.def](traps.def), [trap_counter.cpp](../src/cpu/trap_counter.cpp), [trap_defs.h](../src/cpu/trap_defs.h)

## Goal

Eliminate the hard-coded `s_dict` array of ~681 {trapWord, name} entries
in `trap_counter.cpp`.  Instead, have a single trap dictionary loaded
from `assets/traps.def` at startup (`TrapDefs`), and make
`trap_counter.cpp` delegate all name lookups to `TrapDefs`.

This requires:
1. Expanding `traps.def` with all ~681 trap names (most as header-only
   entries without `in`/`out` parameter lines).
2. Adding name-lookup helpers to the `TrapDefs` class so it can serve
   the role `s_dict` currently fills.
3. Rewiring `trap_counter.cpp` to use `TrapDefs` instead of `s_dict`.
4. Removing the dead `s_dict` code.

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Expand `traps.def` with all 681 trap names | |
| 2 | Add name/search API to `TrapDefs` | |
| 3 | Rewire `trap_counter.cpp` to use `TrapDefs` | |
| 4 | Remove dead `s_dict` code and `TrapInfo` struct | |

Build gate: `cmake --preset macos && cmake --build --preset macos`
Test gate:  `./bld/macos/tests`

---

## Phase 1 — Expand `traps.def` with all 681 trap names

The current `assets/traps.def` has 58 entries with full parameter
definitions.  The hard-coded `s_dict` in `trap_counter.cpp` has ~681
entries.  In this phase we add every trap from `s_dict` that is not
already in `traps.def`, as header-only lines (trap word, name,
convention — no `in`/`out` lines).

### 1.1 — Generate the missing entries

For each `{0xAXXX, "Name"}` in `s_dict`:
- If `traps.def` already has a line starting with the same hex word
  (without `0x` prefix, e.g. `A122`), skip it.
- Otherwise, emit a new header line: `AXXX Name os|toolbox`
  - Convention: bit 11 set (trap word & 0x0800) → `toolbox`, else → `os`.

Group the new entries into labelled sections matching the existing
`traps.def` style (`# ── Section Name ──`).  Append after existing
sections.  Suggested groupings:

- `# ── Memory Manager (additional)` — OS traps not already present
- `# ── Toolbox (A–Z)` — remaining Toolbox traps, alphabetically

Keep existing entries with their `in`/`out`/`show-in`/`show-out` lines
**exactly as they are** — do not touch them.

### 1.2 — Verify

Confirm the file parses correctly by running the emulator and checking
the `trap_defs: loaded N traps` diagnostic is ≥ 681.

### Fence

- [ ] `assets/traps.def` contains ≥ 681 header lines (one per trap)
- [ ] All 58 existing entries retain their parameter definitions
- [ ] No duplicate trap words
- [ ] Build clean
- [ ] Commit: `"traps: expand traps.def with all 681 trap names"`

---

## Phase 2 — Add name/search API to `TrapDefs`

The `trap_counter.cpp` code currently exposes four dict functions that
callers depend on: `trap_dict_size()`, `trap_dict_entry(int)`,
`trap_dict_name(uint16_t)`, and `trap_dict_search(prefix, ...)`.

In this phase we add equivalent methods to `TrapDefs` so it can fully
replace `s_dict`.

### 2.1 — Add a sorted name vector to `TrapDefs`

At the end of `TrapDefs::load()`, build a sorted `std::vector<TrapInfo>`
(reusing the existing `TrapInfo` struct from `trap_counter.h` for now)
from the loaded `defs_` map.  Sort alphabetically by name.  This mirrors
the sorted order of the old `s_dict`.

Add a private member:

```cpp
// trap_defs.h, inside class TrapDefs:
std::vector<std::pair<uint16_t, std::string>> sortedNames_;
```

Populate in `load()` after all entries are parsed:

```cpp
sortedNames_.clear();
sortedNames_.reserve(defs_.size());
for (auto &[tw, def] : defs_)
    sortedNames_.push_back({def.trapWord, def.name});
std::sort(sortedNames_.begin(), sortedNames_.end(),
    [](auto &a, auto &b) { return a.second < b.second; });
```

### 2.2 — Add public query methods to `TrapDefs`

In `trap_defs.h`:

```cpp
/* Number of loaded trap definitions. */
int size() const;

/* Access by index (0 .. size()-1), sorted by name. */
std::pair<uint16_t, std::string_view> entry(int index) const;

/* Look up trap word → name.  Returns empty string_view if unknown. */
std::string_view nameOf(uint16_t trapWord) const;

/* Search for entries whose name starts with prefix (case-insensitive).
   Appends {trapWord, name} pairs to results.  Stops after maxResults. */
void search(std::string_view prefix,
            std::vector<std::pair<uint16_t, std::string_view>> &results,
            int maxResults = 20) const;
```

Implement in `trap_defs.cpp`:

- `size()` — return `sortedNames_.size()`.
- `entry(int)` — return `{sortedNames_[i].first, sortedNames_[i].second}`.
- `nameOf(uint16_t)` — call `find(trapWord)` and return `def->name` or `{}`.
- `search(prefix, ...)` — linear scan `sortedNames_`, case-insensitive
  prefix match on `.second`.

### 2.3 — Tests

Add test cases in `test/test_tracing.cpp` (already includes `trap_defs.h`):

- `TEST_CASE("TrapDefs nameOf returns name for known trap")`
- `TEST_CASE("TrapDefs nameOf returns empty for unknown trap")`
- `TEST_CASE("TrapDefs search prefix match")`
- `TEST_CASE("TrapDefs size matches loaded count")`

### Fence

- [ ] `TrapDefs` has `size()`, `entry()`, `nameOf()`, `search()` methods
- [ ] New tests pass
- [ ] Build clean
- [ ] Commit: `"traps: add name/search API to TrapDefs"`

---

## Phase 3 — Rewire `trap_counter.cpp` to use `TrapDefs`

Replace all uses of `s_dict`, `s_revIndex`, `kDictSize`, and the four
`trap_dict_*()` free functions to delegate to `g_trapDefs`.

### 3.1 — Make `trap_counter.cpp` include and use `TrapDefs`

Add `#include "cpu/trap_tracer.h"` (which declares `extern TrapDefs
g_trapDefs`).  This is already included for `g_tracer`.

Rewrite the four functions:

```cpp
int trap_dict_size()
{
    return g_trapDefs.size();
}

const TrapInfo &trap_dict_entry(int index)
{
    // Provide a thin adapter — see 3.2 below
}

const char *trap_dict_name(uint16_t trapWord)
{
    auto sv = g_trapDefs.nameOf(trapWord);
    return sv.empty() ? nullptr : sv.data();
}

void trap_dict_search(const char *prefix,
                      std::vector<TrapInfo> &results, int maxResults)
{
    results.clear();
    if (!prefix || !prefix[0]) return;
    std::vector<std::pair<uint16_t, std::string_view>> raw;
    g_trapDefs.search(prefix, raw, maxResults);
    for (auto &[tw, name] : raw)
        results.push_back({tw, name.data()});
}
```

### 3.2 — Handle `trap_dict_entry()` return type

`trap_dict_entry()` returns `const TrapInfo &` — a struct with a
`const char *name`.  Since `TrapDefs` owns `std::string` names, we
can't safely return a reference to a stack temporary.

**Option A (preferred):** Keep a `std::vector<TrapInfo>` cache in
`trap_counter.cpp`, populated once on first call from
`g_trapDefs.sortedNames_`.  Or better: build it in a local static:

```cpp
static const std::vector<TrapInfo> &CachedDict()
{
    static std::vector<TrapInfo> cache;
    if (cache.empty()) {
        int n = g_trapDefs.size();
        cache.reserve(n);
        for (int i = 0; i < n; ++i) {
            auto [tw, sv] = g_trapDefs.entry(i);
            cache.push_back({tw, sv.data()});
            // sv.data() points into g_trapDefs' stable storage
        }
    }
    return cache;
}

const TrapInfo &trap_dict_entry(int index)
{
    return CachedDict()[index];
}
```

The `const char *` pointers are safe because `g_trapDefs.sortedNames_`
(and the underlying `defs_` map) are stable after load.

### 3.3 — Update `trap_trace_log`

`trap_trace_log()` already calls `trap_dict_name()`, which will now
delegate to `g_trapDefs`.  No change needed.

### 3.4 — Verify callers

Confirm all callers still compile and behave correctly:
- `src/debugger/debugger.cpp` — uses `trap_dict_name()`
- `src/debugger/cmd_break.cpp` — uses `trap_dict_name()`
- `src/debugger/cmd_info.cpp` — uses `trap_dict_name()`
- `src/debugger/symbols.cpp` — uses `trap_dict_size()`, `trap_dict_search()`
- `src/cpu/trap_counter.cpp` — uses `trap_dict_name()` internally

No header changes needed — the `trap_counter.h` API stays the same.

### Fence

- [ ] `trap_dict_name()`, `trap_dict_size()`, `trap_dict_entry()`,
      `trap_dict_search()` all delegate to `g_trapDefs`
- [ ] No direct use of `s_dict` remains (except the array itself,
      removed in Phase 4)
- [ ] All existing tests pass
- [ ] Build clean
- [ ] Commit: `"traps: rewire trap_counter dict functions to TrapDefs"`

---

## Phase 4 — Remove dead `s_dict` code and `TrapInfo` struct

### 4.1 — Delete from `trap_counter.cpp`

Remove:
- The entire `static const TrapInfo s_dict[]` array (~690 lines)
- `kDictSize`
- `s_revIndex[]`, `s_revReady`, `BuildRevIndex()`
- The `ciStartsWith()` helper (no longer used)

### 4.2 — Evaluate `TrapInfo` struct

`TrapInfo` (in `trap_counter.h`) is still used by:
- `trap_dict_entry()` return type
- `trap_dict_search()` output vector type
- `WatchEntry` — but that uses its own struct, not `TrapInfo`

If no external callers need `TrapInfo` (check `symbols.cpp` — it uses
`trap_dict_search` which fills a `vector<TrapInfo>`), either:
- Keep `TrapInfo` in the header as a lightweight adapter type, or
- Migrate callers to use `std::pair<uint16_t, std::string_view>` and
  remove `TrapInfo` entirely.

Preferred: keep `TrapInfo` for now as it provides a clean named struct
for the public API.  It can be modernised later.

### 4.3 — Verify

Run full test suite and manual smoke test.

### Fence

- [ ] `s_dict` array is gone from `trap_counter.cpp`
- [ ] `BuildRevIndex`, `s_revIndex`, `kDictSize`, `ciStartsWith` removed
- [ ] File is ~100 lines shorter
- [ ] All tests pass
- [ ] Build clean
- [ ] Commit: `"traps: remove hard-coded s_dict — single source of truth is traps.def"`
