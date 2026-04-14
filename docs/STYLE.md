# Code Style Guide

This document defines structural and stylistic recommendations for the
maxivmac codebase.  It complements [NAMING.md](NAMING.md), which covers
identifier naming.  The goal is readable, maintainable code — not
mechanical uniformity.

---

## Language Standard

**maxivmac is a C++23 project** (CMake: `CMAKE_CXX_STANDARD 23`).
All new code must use modern C++ idioms.  Do not write C-with-classes.

Key expectations for new code:

- **Strings**: use `std::string_view` for non-owning read-only string
  parameters and return values.  Use `std::string` when ownership is
  needed.  Do not use `const char *` in new interfaces except where
  required by C APIs or null termination is essential.
- **Ownership**: prefer value types, `std::unique_ptr`, or references
  over raw `new`/`delete`.  Raw pointers are fine for non-owning
  observation of objects whose lifetime is managed elsewhere.
- **Containers**: prefer `std::vector`, `std::array`,
  `std::unordered_map` over hand-rolled linked lists or arrays.
- **Enumerations**: use `enum class` with named values, not bare
  integer constants.
- **Constants**: use `constexpr` or `const` instead of `#define` for
  values.
- **Casts**: use `static_cast<>`, not C-style casts.
- **Output parameters**: prefer return values (including `std::optional`
  or structured bindings) over output-pointer parameters when feasible.
- **Range-for / algorithms**: prefer range-based for loops and
  `<algorithm>` over manual index loops when the intent is clearer.

Legacy code under `src/cpu/` is exempt (see Scope below).  When
*interfacing* with legacy APIs that use `const char *` or raw pointers,
convert at the boundary — keep the modern API clean (unless absolutely needed for performance reasons).

---

## Scope

These guidelines apply to all code under `src/` **except**:

| Directory | Reason |
|-----------|--------|
| `src/cpu/` | 68K interpreter inherited from minivmac.  Instruction handlers are necessarily repetitive and table-driven; restructuring them would risk correctness for no practical gain. |
| `src/macsrc/` | Classic Macintosh source built with vintage toolchains. |
| Third-party code | SoftFloat, Bochs FPU fragments, ImGui — keep upstream style. |

When editing files in excepted directories, match the surrounding style.

---

## Function Length

| Guideline | Target |
|-----------|--------|
| New functions | **≤ 30 lines** |
| Existing functions being modified | Extract helpers when touching a section of a long function |
| Hard upper limit for new code | **60 lines** — beyond this, split or justify in a comment |

Thirty lines is enough for a function that does one thing well:
validate inputs, apply state, and return a result.  If a function
needs more, that is usually a sign it mixes concerns and should be
split.  The well-structured parts of the codebase (WireBus,
EmulatorShell, serial backends) already hit this target naturally.

Long functions are the single biggest readability problem in the
legacy code.  `extnVideoAccess()` in video.cpp runs to ~640 lines;
several device `access()` methods are similarly large.  These exist
because minivmac packed entire trap dispatchers into one function.
New code must not repeat this pattern.

### How to break up a large switch-dispatch function

The typical problem is a method that switches on a command code, then
switches again on a sub-code, with validation, guest-memory access, and
state updates interleaved at every level.

Recommended approach:

```cpp
// Instead of one 600-line function:
void VideoDevice::extnVideoAccess(uint32_t p)
{
    switch (get_vm_word(p + ExtnDat_commnd)) {
        case kCmndVideoControl: handleControl(p); break;
        case kCmndVideoStatus:  handleStatus(p);  break;
        ...
    }
}

// Each handler is a focused function:
void VideoDevice::handleControl(uint32_t p)
{
    uint32_t csParam = ...;
    switch (csCode) {
        case 2: result = doSetVidMode(csParam);   break;
        case 3: result = doSetEntries(csParam);    break;
        case 10: result = doSwitchMode(csParam);   break;
        ...
    }
}
```

Each leaf handler (`doSetVidMode`, `doSwitchMode`, …) should be a
self-contained function that validates input, applies state, and
returns an error code — typically 15–30 lines.

---

## File Length and Organisation

| Guideline | Target |
|-----------|--------|
| Source file length | **≤ 800 lines** preferred; ≤ 1200 acceptable |
| Header file length | **≤ 200 lines** preferred |

### File structure

Organise a `.cpp` file in this order:

1. File header comment (brief — what the module emulates / does)
2. `#include` directives (own header first, then project, then system)
3. Local constants and types (`static`, anonymous namespace)
4. Local helper functions (`static`)
5. Class method implementations (grouped logically, not alphabetically)
6. Module-level free functions (public API of the translation unit)

Avoid scattering unrelated functionality into a file just because it
was convenient.  If a device grows a second major responsibility, give
it a second file (e.g. `video_slot_rom.cpp` for ROM-building helpers).

### Include guards

Use `#pragma once` in all headers.  Do not use `#ifndef`/`#define`
include guards in new code.

---

## Nesting Depth

Aim for **≤ 4 levels** of indentation inside a function body.  Deep
nesting is a signal that a block should be extracted into a helper or
that early-return / guard clauses should be used.

```cpp
// Prefer early return:
if (!res) {
    result = tMacErr::paramErr;
    break;
}
// ... main logic at lower nesting depth

// Instead of:
if (res) {
    if (depth >= 0) {
        if (depth <= resMaxDepth) {
            // ... everything indented 4+ levels
        }
    }
}
```

---

## Conditions

Write conditions in natural reading order: variable on the left,
constant on the right.  The legacy codebase is full of Yoda
conditions (`0x85 == type`, `0 != chan`, `nullptr == ptr`) inherited
from minivmac.  Do not introduce new ones.  When modifying a line
that has a Yoda test, flip it.

```cpp
// Good
if (type == 0x85) ...
if (chan != 0) ...
if (ptr == nullptr) ...
if (!ptr) ...

// Bad (Yoda) — do not write new code this way
if (0x85 == type) ...
if (0 != chan) ...
if (nullptr == ptr) ...
```

Modern compilers warn on accidental `=` in conditions (`-Wparentheses`),
so the original safety justification for Yoda style no longer applies.

---

## Comments

Brief comments explaining *why*, not *what*.  The code should be clear
enough that line-by-line narration is unnecessary.

- Use a comment block at the top of a non-trivial static function to
  explain its purpose (one or two sentences).
- Inside long switch cases, a short `/* SetEntries */` tag on the case
  label is helpful (the existing code already does this well).
- Do not add comments to code you are not otherwise modifying.

---

## Constants

Prefer named constants over magic numbers.  Existing `#define` macros
for struct offsets (e.g. `VDSwitchInfo_csMode`) are acceptable; new
constants should be `static constexpr` or `enum` where possible.

```cpp
// Good
static constexpr uint32_t kVidBaseAddr = 0xF9900000;

// Avoid
put_vm_long(csParam + 8, 0xF9900000);
```

---

## Error Handling

- Device trap handlers return `tMacErr` — validate inputs early and
  return the appropriate error rather than nesting success paths.
- Use `ReportAbnormalID()` for conditions that indicate emulator bugs
  or unimplemented features, not for guest-caused errors.

---

## Globals

See [NAMING.md](NAMING.md) for the `g_` prefix rule.  Beyond naming:

- Prefer passing values through function parameters or device state
  over adding new `static` file-scope variables.
- When file-scope state is necessary, group related variables together
  with a brief comment block explaining the group.
- Module-specific state should live in the device class where possible
  rather than in file-scope statics.

---

## Formatting

A `.clang-format` file in the project root encodes the mechanical
rules (indentation, braces, line length).  Run `clang-format` on
all new and modified files.

Legacy files should be reformatted too — the original minivmac
source is preserved in `reference/src/` for behavioural comparison,
so there is no need to keep legacy formatting in `src/`.  When
reformatting a file, make it a **separate commit** from logic changes
so that the two are easy to distinguish in the history.

Key choices captured in `.clang-format`:

- **Indentation**: tabs, width 4.
- **Braces**: Allman (opening brace on its own line for functions
  and control flow).
- **Line length**: 100 columns.
- **Pointer alignment**: `Type *name` (right-aligned).

---

## Pragmatism

These are guidelines, not laws.  Some situations where deviation is
expected:

- **CPU emulation** (`src/cpu/`): instruction handlers are short but
  numerous and repetitive by nature.  Table-driven dispatch, terse
  variable names, and macro-heavy code are fine here.
- **Slot ROM building** (`VideoDevice::init`): the ROM is constructed
  sequentially; one longer function that reads top-to-bottom can be
  clearer than a dozen tiny helpers.
- **Legacy device code**: when fixing a bug in a 400-line function, you
  are not obligated to refactor the whole function — but extracting the
  block you touched into a helper is encouraged.
