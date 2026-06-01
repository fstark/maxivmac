---
name: code-modernizer
description: "Audit and modernize C++ code to match project conventions. Use when: upgrading legacy code to C++23 idioms, replacing const char* with string_view, converting bare enums to enum class, removing C-style casts, fixing naming violations, splitting oversized functions. Accepts a file or directory path."
argument-hint: "Path to file or directory to modernize, e.g. src/devices/via.cpp or src/devices/"
---

# Code Modernizer

Audit C++ source files for legacy patterns and either fix them directly
(small scope) or produce a phased plan (large scope).  All changes must
follow [STYLE.md](docs/STYLE.md) and [NAMING.md](docs/NAMING.md).

**This is a C++23 project.**

## When to Use

- Upgrading a file or directory from C-style code to modern C++
- Cleaning up naming violations outside `src/cpu/`
- Splitting oversized functions or files
- The user says "modernize", "clean up", or "bring up to style"

## Scope Exclusions

Do **not** modify:

| Directory | Reason |
|-----------|--------|
| `src/cpu/` | Frozen legacy 68K interpreter.  Only *public API* symbols that leak into other directories may be modernized. |
| `src/macsrc/` | Classic Macintosh source — not part of the emulator. |
| Third-party code | SoftFloat, Bochs FPU, ImGui — keep upstream style. |

When a target path falls inside an excluded directory, say so and stop.

## What to Look For

Audit the target for these patterns, grouped by risk.  Lower-risk
categories are safe mechanical transforms; higher-risk categories
require understanding call sites.

### Category A — Safe Mechanical (low risk)

| Pattern | Replacement | How to find |
|---------|-------------|-------------|
| `NULL`, `0` as null pointer | `nullptr` | `grep -n '\bNULL\b\|([^0-9])0)' *.cpp` |
| C-style cast `(type)expr` | `static_cast<type>(expr)` | Visual scan of target |
| `typedef old new;` | `using new = old;` | `grep -n '\btypedef\b'` |
| `#define` numeric constant | `constexpr` / `enum class` | `grep -n '^#define.*[0-9]'` |
| Yoda conditions `0 == x` | `x == 0` | `grep -n '0 ==\|nullptr ==\|NULL =='` |
| C header includes `<string.h>` | `<cstring>` (or remove) | `grep -n '#include <[a-z]*\.h>'` |
| `#ifndef`/`#define` include guard | `#pragma once` | `grep -rn '#ifndef.*_H'` |

### Category B — Interface Modernization (medium risk)

| Pattern | Replacement | Notes |
|---------|-------------|-------|
| `const char *` parameter | `std::string_view` | Must update all callers.  Keep `const char *` where null termination is required (e.g. `fopen`). |
| `const char *` return | `std::string_view` | Only if returned string has stable lifetime. |
| Bare `enum { ... }` | `enum class` | Must update all references to use qualified names. |
| Output pointer param | Return value / `std::optional` | Only when there's a single output and no performance concern. |
| Raw `new` / `delete` | `std::unique_ptr` or value type | Only when ownership is clear and local. |

### Category C — Naming (medium risk)

| Pattern | Rule (NAMING.md) | Notes |
|---------|-------------------|-------|
| `camelCase` free function | Should be `PascalCase` | Grep for function definitions outside classes. |
| `snake_case` free function | Should be `PascalCase` | Same. |
| Bare global `FooBar` | Should be `g_fooBar` | `grep -n '^extern'` in headers. |
| File-scope static `FooBar` | Should be `s_fooBar` | `grep -n '^static '` in .cpp files. |
| Legacy `My` prefix | Drop prefix | Only when refactoring that subsystem. |
| `Module_PascalCase` free function (`Sound_Start`) | Drop underscore → `SoundStart` | Only when modernizing that subsystem. |

### Category D — Structural (higher risk)

| Pattern | Target | Notes |
|---------|--------|-------|
| Function > 60 lines | Split into helpers | Read STYLE.md Function Length section.  New helpers ≤ 30 lines. |
| File > 1200 lines | Split into separate files | Keep logical cohesion. |
| Nesting > 4 levels | Extract helper or use early return | Read STYLE.md Nesting Depth section. |

## Procedure

### Step 1 — Read

1. Read [STYLE.md](docs/STYLE.md) and [NAMING.md](docs/NAMING.md).
2. Read every file in the target path.  For directories, list contents
   first, then read each `.h` and `.cpp` file.
3. If the target includes a header, also read its implementation (and
   vice versa) to understand call sites.

### Step 2 — Audit

For each file, check every pattern from Categories A–D above.
Record findings as a list:

```
file.cpp:42  — const char * parameter in FooBar() → string_view  [B]
file.cpp:88  — bare enum ScreenMode → enum class  [B]
file.cpp:110 — (int)x cast → static_cast<int>(x)  [A]
file.h:15    — camelCase free function doThing() → DoThing()  [C]
```

Tag each with its category letter.

### Step 3 — Decide: Direct Fix or Plan

**Direct fix** (just do it) when ALL of these are true:
- Target is a single file (or 2-3 tightly coupled files)
- Total findings ≤ 15
- No Category D (structural) findings
- No cross-module interface changes (i.e. callers are all in the
  same file or trivially identifiable)

**Produce a plan** when ANY of these are true:
- Target is a directory with 4+ files
- Total findings > 15
- Category D findings exist
- Interface changes affect callers in multiple directories

### Step 4a — Direct Fix

1. Apply changes grouped by category (A first, then B, then C).
2. Build: `cmake --preset macos && cmake --build --preset macos`
3. Test: `./bld/macos/tests`
4. If build/test fails, diagnose and fix (up to 3 attempts).
5. Commit: `git add -A && git commit -m "modernize: <file> — <summary>"`

### Step 4b — Produce Plan

Create `docs/MODERNIZE_<SCOPE>_PLAN.md` (e.g.
`docs/MODERNIZE_DEVICES_PLAN.md`) following the execute-plan format:

```markdown
# Modernize <scope> — Implementation Plan

Reference: [STYLE.md](../STYLE.md), [NAMING.md](../NAMING.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Category A: mechanical fixes | |
| 2 | Category B: interface — enums | |
| 3 | Category B: interface — string_view | |
| ... | ... | |

Build gate: `cmake --preset macos && cmake --build --preset macos`
Test gate:  `./bld/macos/tests`

---

## Phase 1 — Mechanical fixes (Category A)

### 1.1 — nullptr, casts, typedefs in file1.cpp
...

### Fence
- [ ] Build clean
- [ ] Tests pass
- [ ] Commit: `"modernize: phase 1 — mechanical fixes in <scope>"`
```

**Phase ordering rules:**
1. Category A first (no behavioral change, lowest risk)
2. Category B enums before string changes (enums are self-contained)
3. Category B string_view changes (may touch callers)
4. Category C naming (may touch callers)
5. Category D structural last (highest risk, most review needed)

Within each category, group by file so each phase is a coherent commit.

After writing the plan, tell the user: "Plan written to
`docs/MODERNIZE_<SCOPE>_PLAN.md`.  Review it, then ask me to execute
it using the execute-plan skill."

## What NOT to Modernize

- **Working legacy code you don't understand.**  If a pattern looks
  wrong but you can't verify the fix is safe, skip it and note it
  in the plan as deferred.
- **Performance-critical hot paths** (e.g. the MATC/ATT fast path in
  m68k.cpp) — even though `src/cpu/` is excluded, adjacent code that
  feeds into it should not get gratuitous `string_view` conversions
  if it would add overhead.
- **Patterns with fewer than 3 occurrences in a file** — not worth a
  dedicated phase.  Bundle them with adjacent work or defer.
