# Phase 2 — Type System & Macro Cleanup (Detailed Plan)

Replace the custom type system and visibility macros with standard C/C++ equivalents. Every file in `src/` currently uses minivmac's custom `ui3b`/`blnr`/`LOCALPROC`/etc. vocabulary. After this phase, the code reads like standard C++ — new contributors don't need a glossary.

---

## Context: What Needs to Change

### Type definitions (in `cfg/CNFUIALL.h` and `src/DFCNFCMP.h`)

The codebase defines a layered type system:

| Layer | File | Examples |
|-------|------|----------|
| **Base types** | `cfg/CNFUIALL.h` | `typedef unsigned char ui3b;` `typedef short si4b;` etc. |
| **Representation types** | `cfg/CNFUIALL.h` | `typedef ui3b ui3r;` (register-width aliases, identical on modern platforms) |
| **Pointer types** | `src/DFCNFCMP.h` | `typedef ui3b *ui3p;` etc. |
| **Meta types** | `src/DFCNFCMP.h` | `typedef ui5r uimr;` `typedef si5r simr;` |
| **Boolean** | `src/DFCNFCMP.h` | `#define blnr ui3r` / `trueblnr 1` / `falseblnr 0` |
| **Null/void** | `src/DFCNFCMP.h` | `#define nullpr ((void *) 0)` / `#define anyp ui3p` |

### Visibility macros (in `src/DFCNFCMP.h`)

~3,600 occurrences across 45+ files:

| Macro | Expansion | Occurrences |
|-------|-----------|-------------|
| `LOCALVAR` | `static` | 763 |
| `LOCALPROC` | `static __attribute__((noinline)) void` | 1,120 |
| `LOCALFUNC` | `static __attribute__((noinline))` | 755 |
| `LOCALINLINEFUNC` | `static inline __attribute__((always_inline))` | 47 |
| `LOCALINLINEPROC` | `static inline __attribute__((always_inline)) void` | 28 |
| `LOCALPROCUSEDONCE` | same as `LOCALINLINEPROC` | 151 |
| `LOCALFUNCUSEDONCE` | same as `LOCALINLINEFUNC` | 1 |
| `FORWARDFUNC` / `FORWARDPROC` | `static __attribute__((noinline))` [void] | 10 / 173 |
| `GLOBALVAR` | *(empty)* | 47 |
| `GLOBALFUNC` / `GLOBALPROC` | `__attribute__((noinline))` [void] | 29 / 89 |
| `GLOBALOSGLUFUNC` / `GLOBALOSGLUPROC` | `__attribute__((noinline))` [void] | 76 / 60 |
| `EXPORTVAR(t, v)` | `extern t v;` | 46 |
| `EXPORTFUNC` / `EXPORTPROC` | `extern` [void] | 32 / 89 |
| `EXPORTOSGLUFUNC` / `EXPORTOSGLUPROC` | `extern` [void] | 15 / 24 |
| `IMPORTFUNC` / `IMPORTPROC` | `extern` [void] | 17 / 62 |
| `TYPEDEFFUNC` / `TYPEDEFPROC` | `typedef` [void] | 2 / 1 |

### Calling-convention macros

| Macro | Expansion | Occurrences |
|-------|-----------|-------------|
| `my_reg_call` | *(empty)* — was `__fastcall` on x86-32 | 251 |
| `my_osglu_call` | *(empty)* | 6 |

### Other compiler macros

| Macro | Expansion | Occurrences |
|-------|-----------|-------------|
| `MayInline` | `inline __attribute__((always_inline))` | 3 (in macro defs) |
| `MayNotInline` | `__attribute__((noinline))` | 4 (in macro defs) |
| `my_align_8` | *(empty)* | 3 |
| `my_cond_rare(x)` | `(x)` — was `__builtin_expect((x),0)` | 20 |
| `Have_ASR` | `0` | 3 |
| `HaveMySwapUi5r` | `0` | 4 |

### Scale

- **~3,345** custom type occurrences
- **~2,741** boolean type occurrences
- **~386** pointer/null type occurrences
- **~3,636** visibility macro occurrences
- **~251** calling-convention macro occurrences
- **Total: ~10,359** replacements across **86 files** (~88,651 lines)

---

## Strategy

### C → C++ transition

Switch the language standard from C to C++17 in CMake as the first step. The codebase has **zero C++ keyword conflicts** (verified: no identifiers named `class`, `new`, `delete`, `template`, `namespace`, `this`, `virtual`, `private`, `public`, `protected`). The Objective-C file `OSGLUCCO.m` stays as `.m` (compiled as ObjC, not ObjC++).

### Order of operations

Replace types and macros in a specific dependency order to stay compilable at every step:

1. **Switch to C++17** — the `.c` files compile as C++ without changes
2. **Replace base types** — `ui3b` → `uint8_t`, etc. (change definitions, not usages yet)
3. **Replace usages file-by-file** — mechanical find-and-replace, compile after each file
4. **Replace visibility macros** — `LOCALVAR` → `static`, etc.
5. **Replace booleans** — `blnr` → `bool`, `trueblnr` → `true`, `falseblnr` → `false`
6. **Replace pointer/null types** — `nullpr` → `nullptr`, `anyp` → `void*`
7. **Remove calling-convention macros** — `my_reg_call` → *(nothing)*
8. **Clean up leftover helper macros**
9. **Remove the definition files** — `DFCNFCMP.h` becomes minimal or empty, `CNFUIALL.h` becomes a `<cstdint>` include

### Mechanical replacement approach

Each type/macro replacement is a **global find-and-replace** using `sed` or a script. The replacements must be done as **whole-word** matches to avoid corrupting substrings (e.g., `ui3b` inside `ui3beqr`). For types used in casts like `(ui5b)`, the regex must handle the parenthesized form. After each replacement, compile and verify.

---

## Steps

### Step 2.1 — Switch to C++17

**Changes:**
- In `CMakeLists.txt`: set `CMAKE_CXX_STANDARD 17`, add `CXX` to `project(LANGUAGES ...)`, change all `.c` source files to compile as C++ (either rename → `.cpp` or set `LANGUAGE CXX` property on them).
- Keep `OSGLUCCO.m` as Objective-C (it uses ObjC syntax throughout).
- Add `-std=c++17` to compile flags.
- Do NOT rename files yet (that's Phase 3) — use CMake's `set_source_files_properties(... PROPERTIES LANGUAGE CXX)` to compile `.c` files as C++.
- Add `-Wno-writable-strings` (or `-Wno-write-strings`) since the codebase has many `char *` literals that C++ treats as `const char *`.

**Verification:**
```bash
cmake --preset macos-cocoa && cmake --build --preset macos-cocoa
```
All files compile as C++ (except OSGLUCCO.m). Boot System 7 disk image — works identically.

**Commit:** `Switch build to C++17 (source files compiled as C++ via CMake property)`

---

### Step 2.2 — Add `<cstdint>` / `<cstddef>` and compatibility typedefs

**Changes:**
- In `cfg/CNFUIALL.h` (the CMake-copied version): add `#include <cstdint>` and `#include <cstddef>` at the top.
- Make the existing typedefs into aliases of the standard types:
  ```cpp
  #include <cstdint>
  #include <cstddef>
  #include <cstdbool>

  using ui3b = uint8_t;
  using si3b = int8_t;
  using ui4b = uint16_t;
  using si4b = int16_t;
  using ui5b = uint32_t;
  using si5b = int32_t;

  // Representation types — identical on modern platforms
  using ui3r = uint8_t;
  using si3r = int8_t;
  using ui4r = uint16_t;
  using si4r = int16_t;
  using ui5r = uint32_t;
  using si5r = int32_t;
  ```
- This is a **no-op change** — the old and new types are the same underlying types. It just makes the definitions explicit and adds the standard headers.
- The `HaveRealui3b` etc. guard macros become unnecessary but are kept for now (they're only tested in `CNFUIALL.h` itself).

**Verification:** Compiles and boots identically. No behavioral change.

**Commit:** `Add <cstdint> includes; rewrite custom type definitions as standard type aliases`

---

### Step 2.3 — Replace custom integer types in source files

Replace **all usages** of custom types with their standard equivalents. This is the largest single step (~3,345 replacements). Do it in sub-batches grouped by type width, compiling after each batch.

**Replacement table:**

| Old | New | Count |
|-----|-----|-------|
| `ui3b` | `uint8_t` | 346 |
| `ui3r` | `uint8_t` | 192 |
| `ui3rr` | `uint8_t` | 119 |
| `ui3p` | `uint8_t *` | 225 |
| `si3b` | `int8_t` | 16 |
| `si3r` | `int8_t` | 37 |
| `ui4b` | `uint16_t` | 295 |
| `ui4r` | `uint16_t` | 206 |
| `ui4rr` | `uint16_t` | 13 |
| `ui4p` | `uint16_t *` | 2 |
| `si4b` | `int16_t` | 104 |
| `si4r` | `int16_t` | 21 |
| `ui5b` | `uint32_t` | 491 |
| `ui5r` | `uint32_t` | 953 |
| `si5rr` | `int32_t` | 6 |
| `ui5p` | `uint32_t *` | 13 |
| `si5b` | `int32_t` | 103 |
| `si5r` | `int32_t` | 73 |
| `uimr` | `uint32_t` | 122 |
| `simr` | `int32_t` | 3 |

**Sub-batch order** (compile + verify after each):

1. **8-bit types:** `ui3b`, `ui3r`, `ui3rr` → `uint8_t`; `ui3p` → `uint8_t *`; `si3b`, `si3r` → `int8_t`
2. **16-bit types:** `ui4b`, `ui4r`, `ui4rr` → `uint16_t`; `ui4p` → `uint16_t *`; `si4b`, `si4r` → `int16_t`
3. **32-bit types:** `ui5b`, `ui5r`, `si5rr` → `uint32_t`/`int32_t`; `ui5p` → `uint32_t *`; `si5b`, `si5r` → `int32_t`; `uimr` → `uint32_t`; `simr` → `int32_t`

**Caution — tricky patterns:**

- **Pointer types in parameters:** `ui3p Buffer` → `uint8_t * Buffer` (space before `*` matters in some formatting).
- **Cast expressions:** `(ui5b)x` → `(uint32_t)x`. Must match the parenthesized form.
- **Typedef names in struct fields:** e.g., `ui3r MMDV;` in `struct ATTer`.
- **`#define` type aliases:** `#define tMacErr ui4r` → `#define tMacErr uint16_t`. These survive until a later cleanup turns them into proper `using` declarations.
- **Type names inside macro definitions:** e.g., `#define trSoundSamp ui3r` — these need updating too.
- **Guard against partial matches:** `ui3beqr` should NOT be matched when replacing `ui3b`. Use word-boundary regex: `\bui3b\b`.

**Method:** Use `sed -i '' 's/\bui3b\b/uint8_t/g'` (or equivalent) on each `.c`, `.h`, `.m` file in `src/`. Also update `cfg/CNFUIALL.h` template and `cmake/` templates.

**Verification after each sub-batch:**
```bash
cmake --build --preset macos-cocoa 2>&1 | head -20
```
Zero errors, zero new warnings. Boot test.

**Commit after all three sub-batches:** `Replace custom integer types with <cstdint> types (ui3b→uint8_t, ui5r→uint32_t, etc.)`

---

### Step 2.4 — Replace `CPTR` typedef

**Changes:**
- `CPTR` is defined in `src/GLOBGLUE.h` as `typedef ui5b CPTR;` — which is now `typedef uint32_t CPTR;`
- Replace all 161 occurrences of `CPTR` → `uint32_t` across source files.
- Remove the typedef from `GLOBGLUE.h`.

**Caution:** `CPTR` is a semantically meaningful name (emulated address pointer). Consider whether to keep it as a `using CPTR = uint32_t;` alias for documentation purposes. **Decision: remove it.** The context is always clear (parameter named `addr`), and a plain `uint32_t` is unambiguous.

**Verification:** Compile and boot.

**Commit:** `Replace CPTR typedef with uint32_t`

---

### Step 2.5 — Replace boolean types

**Changes:**

| Old | New | Count |
|-----|-----|-------|
| `blnr` | `bool` | 980 |
| `trueblnr` | `true` | 779 |
| `falseblnr` | `false` | 982 |

- Remove the `#define blnr ui3r` / `#define trueblnr 1` / `#define falseblnr 0` from `src/DFCNFCMP.h`.
- The old `blnr` was `uint8_t` (1 byte). C++ `bool` is also typically 1 byte. However, watch for code that does arithmetic on booleans or uses them in bitfields — scan for patterns like `blnr x = 2` or `blnr x = someInt & mask`.

**Caution — patterns to audit before replacement:**
- `blnr` used in struct fields (e.g., `MyEvtQEl.kind` adjacencies) — `bool` has the same alignment, safe.
- `blnr` return values compared to integers — `if (result == trueblnr)` becomes `if (result == true)` which is valid but odd; ideally just `if (result)`. Leave as-is for now; it's a style issue, not a correctness one.
- `blnr` in `EXPORTVAR(blnr, ...)` — becomes `extern bool ...;` after macro cleanup.

**Verification:** Compile and boot.

**Commit:** `Replace blnr/trueblnr/falseblnr with bool/true/false`

---

### Step 2.6 — Replace pointer/null/void types

**Changes:**

| Old | New | Count |
|-----|-----|-------|
| `nullpr` | `nullptr` | 189 |
| `anyp` | `void *` | 23 |
| `ps3p` | `uint8_t *` | 13 |

- `nullpr` → `nullptr` is the straightforward C++ modernization.
- `anyp` was `#define anyp ui3p` = `uint8_t *`. Its usage as "generic pointer" means it should become `void *` (the actual C/C++ idiom). Callers cast it.
- `ps3p` (pascal string pointer) was also `uint8_t *`. Replace with `uint8_t *`.

**Verification:** Compile and boot.

**Commit:** `Replace nullpr→nullptr, anyp→void*, ps3p→uint8_t*`

---

### Step 2.7 — Replace visibility macros (variables)

**Changes:**

| Old | New | Count |
|-----|-----|-------|
| `LOCALVAR` | `static` | 763 |
| `GLOBALVAR` | *(remove — empty anyway)* | 47 |
| `EXPORTVAR(t, v)` | `extern t v;` | 46 |

- `LOCALVAR x` → `static x` — simple prefix replacement.
- `GLOBALVAR x` → `x` — the macro expands to nothing, so just remove the word.
- `EXPORTVAR(t, v)` → `extern t v;` — this is a function-like macro that wraps two args. The replacement is `extern type name;`. This needs a regex or script that understands the `EXPORTVAR(type, name)` pattern.

**Special case for `EXPORTVAR`:** Some usages have array types:
```c
EXPORTVAR(ui3b, Wires[kNumWires])    →  extern uint8_t Wires[kNumWires];
EXPORTVAR(ui4r, CLUT_reds[CLUT_size]) →  extern uint16_t CLUT_reds[CLUT_size];
EXPORTVAR(iCountt, ICTwhen[kNumICTs]) →  extern iCountt ICTwhen[kNumICTs];
```
The regex must handle this: `EXPORTVAR\(([^,]+),\s*([^)]+)\)` → `extern \1 \2;`

**Verification:** Compile and boot.

**Commit:** `Replace LOCALVAR→static, GLOBALVAR→(remove), EXPORTVAR→extern`

---

### Step 2.8 — Replace visibility macros (functions)

This is the largest macro replacement group. Do it in sub-batches.

**Sub-batch A — Local functions (static):**

| Old | New | Count |
|-----|-----|-------|
| `LOCALPROC` | `static void` | 1,120 |
| `LOCALFUNC` | `static` | 755 |
| `FORWARDPROC` | `static void` | 173 |
| `FORWARDFUNC` | `static` | 10 |
| `LOCALPROCUSEDONCE` | `static inline void` | 151 |
| `LOCALFUNCUSEDONCE` | `static inline` | 1 |
| `LOCALINLINEPROC` | `static inline void` | 28 |
| `LOCALINLINEFUNC` | `static inline` | 47 |

Note: The `MayNotInline` / `MayInline` attributes are dropped. Modern compilers make better inlining decisions than forced attributes. If profiling later shows a regression, targeted `__attribute__((noinline))` can be re-added on hot paths.

**Sub-batch B — Global and export functions:**

| Old | New | Count |
|-----|-----|-------|
| `GLOBALPROC` | `void` | 89 |
| `GLOBALFUNC` | *(nothing — just the return type follows)* | 29 |
| `EXPORTPROC` | `extern void` | 89 |
| `EXPORTFUNC` | `extern` | 32 |
| `IMPORTPROC` | `extern void` | 62 |
| `IMPORTFUNC` | `extern` | 17 |
| `GLOBALOSGLUFUNC` | *(nothing)* | 76 |
| `GLOBALOSGLUPROC` | `void` | 60 |
| `EXPORTOSGLUFUNC` | `extern` | 15 |
| `EXPORTOSGLUPROC` | `extern void` | 24 |
| `TYPEDEFFUNC` | `typedef` | 2 |
| `TYPEDEFPROC` | `typedef void` | 1 |

**Note on `GLOBALFUNC`/`GLOBALOSGLUFUNC`:** These expand to `__attribute__((noinline))`, meaning the return type follows the macro. For example:
```c
GLOBALFUNC blnr SomeFunc(void)     // was: __attribute__((noinline)) bool SomeFunc(void)
```
The replacement removes the attribute: → `bool SomeFunc(void)`. Since `GLOBALFUNC` expands to just `MayNotInline` (no `static`, no `extern`), removing it leaves the bare return type, which is correct for a function defined in a `.c` file with external linkage.

**Note on `EXPORT*` in headers:** `EXPORTPROC`/`EXPORTFUNC` appear in **header files** (declarations). In `.c` files, the matching **definitions** use `GLOBALPROC`/`GLOBALFUNC`. After replacement:
- Header: `extern void SomeFunc(void);`
- Source: `void SomeFunc(void) { ... }`
This is correct C/C++ linkage.

**Verification after each sub-batch:** Compile and boot.

**Commit:** `Replace visibility macros with standard C++ linkage (static/extern/inline)`

---

### Step 2.9 — Remove calling-convention macros

**Changes:**

| Old | New | Count |
|-----|-----|-------|
| `my_reg_call` | *(remove)* | 251 |
| `my_osglu_call` | *(remove)* | 6 |

- `my_reg_call` appears in function signatures and typedef'd function pointer types in `MINEM68K.c`. It was `__fastcall` on 32-bit x86 (passed first arg in register). On all modern 64-bit ABIs, the default calling convention already uses registers. The macro expands to nothing.
- After removal, function signatures like `LOCALFUNC ui5r my_reg_call get_byte(CPTR addr)` (already partially replaced) become `static uint32_t get_byte(uint32_t addr)`.
- Also handle the typedef pattern: `typedef void (my_reg_call *ArgSetDstP)(uint32_t f);` → `typedef void (*ArgSetDstP)(uint32_t f);`

**Verification:** Compile and boot.

**Commit:** `Remove my_reg_call and my_osglu_call calling-convention macros`

---

### Step 2.10 — Clean up remaining helper macros

**Changes:**

Remove or simplify these from `DFCNFCMP.h` and `CNFUIALL.h`:

| Macro | Action |
|-------|--------|
| `MayInline` / `MayNotInline` | Remove definitions (no longer referenced after Step 2.8) |
| `my_align_8` | Remove (expands to nothing; 3 usages in MINEM68K.c — just delete the annotation) |
| `my_cond_rare(x)` | Replace with `__builtin_expect(!!(x), 0)` or just `(x)` — keep as `(x)` for now, modernize later |
| `Have_ASR` | Keep (used in `#if` guards for arithmetic-shift-right optimization) |
| `HaveMySwapUi5r` | Keep (used in `#if` guards for byte-swap optimization) |
| `UNUSED(exp)` | Replace with `[[maybe_unused]]` attribute or keep as-is — keep the macro for now, it's idiomatic C |
| `SmallGlobals` / `cIncludeUnused` | Keep (used in `#if` guards) |
| `UnusedParam(p)` | Keep (alias for `UNUSED`) |
| `HaveRealui3b` / `ui3beqr` / etc. | Remove all "type availability" guards — they're always 1 on modern platforms |

**Verification:** Compile and boot.

**Commit:** `Remove unused compiler-abstraction macros (MayInline, my_align_8, type guards)`

---

### Step 2.11 — Simplify `DFCNFCMP.h` and `CNFUIALL.h`

After all replacements, these files should be dramatically smaller.

**`CNFUIALL.h` becomes:**
```cpp
/* Compiler/platform configuration */
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdbool>

/* Optimization hints — compiler may override */
#define SmallGlobals 0
#define cIncludeUnused 0
#define UNUSED(p) (void)(p)

/* Endian/alignment support */
#ifndef BigEndianUnaligned
#define BigEndianUnaligned 0
#endif

#ifndef LittleEndianUnaligned
#define LittleEndianUnaligned 0
#endif

#ifndef Have_ASR
#define Have_ASR 0
#endif

#ifndef HaveMySwapUi5r
#define HaveMySwapUi5r 0
#endif
```

**`DFCNFCMP.h` becomes:**
```cpp
/* Defaults for configuration of compiler — mostly empty after type cleanup */
#pragma once

/* Branch prediction hint */
#ifndef my_cond_rare
#define my_cond_rare(x) (x)
#endif
```

**Verification:** Compile with `-Wall -Wextra -Wpedantic` — zero errors, zero new warnings (existing warnings from the original code are acceptable). Boot test.

**Commit:** `Simplify CNFUIALL.h and DFCNFCMP.h after type/macro cleanup`

---

### Step 2.12 — Update `#define` type aliases to `using` declarations

Several headers define shorthand types via `#define`. Convert them to proper C++ type aliases:

```cpp
// In OSGLUAAA.h:
#define tMacErr ui4r     →  using tMacErr = uint16_t;
#define tPbuf ui4r       →  using tPbuf = uint16_t;
#define tDrive ui4r      →  using tDrive = uint16_t;

// In MINEM68K.c:
#define iCountt ui5b     →  using iCountt = uint32_t;  // (defined in GLOBGLUE.h)

// Sound types in OSGLUAAA.h:
#define trSoundSamp ui3r →  using trSoundSamp = uint8_t;   // (when kLn2SoundSampSz==3)
#define tbSoundSamp ui3b →  using tbSoundSamp = uint8_t;
#define tpSoundSamp ui3p →  using tpSoundSamp = uint8_t *;
```

These are still inside `#if` guards, which is fine — `using` declarations work inside `#if` blocks.

**Verification:** Compile and boot.

**Commit:** `Convert #define type aliases to C++ using declarations`

---

### Step 2.13 — Replace `#error "header already included"` guards with `#pragma once`

Every header in `src/` uses this pattern:
```c
#ifdef SOMEFILE_H
#error "header already included"
#else
#define SOMEFILE_H
#endif
```

This is an overly strict include guard that **fatally errors** on double-inclusion instead of silently skipping. Replace with the standard `#pragma once` (supported by GCC, Clang, MSVC — all target compilers).

**Changes:** In every `.h` file in `src/` (56 files), replace the 4-line guard block with a single `#pragma once`.

**Verification:** Compile and boot.

**Commit:** `Replace #error include guards with #pragma once`

---

### Step 2.14 — Final validation

1. **Clean build from scratch:**
   ```bash
   rm -rf bld/macos-cocoa
   cmake --preset macos-cocoa && cmake --build --preset macos-cocoa
   ```

2. **Compile with strict warnings:**
   ```bash
   cmake --preset macos-cocoa -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic" \
     && cmake --build --preset macos-cocoa 2>&1 | grep -c "warning:"
   ```
   Target: zero new warnings from our changes (pre-existing warnings from original code are acceptable and will be addressed in later phases).

3. **Boot test:** Launch with Mac II ROM + System 7 disk image. Verify:
   - Boot to desktop
   - Mouse and keyboard work
   - Sound plays (startup chime)
   - Floppy disk operations work (eject/insert)

4. **Binary comparison:** The emulator behavior should be bit-identical to pre-Phase-2. The type replacements are all same-width, same-signedness substitutions.

**Commit:** `Phase 2 complete: type system and macro cleanup verified`

---

## File Changes Summary

### Modified Files

| File | Nature of change |
|------|-----------------|
| `CMakeLists.txt` | Add C++17, set `.c` files to compile as CXX |
| `cfg/CNFUIALL.h` | Rewritten: `<cstdint>` + minimal config macros |
| `src/DFCNFCMP.h` | Rewritten: minimal (just `my_cond_rare`) |
| All 29 `.c` files in `src/` | Type + macro replacements |
| All 56 `.h` files in `src/` | Type + macro replacements + `#pragma once` |
| `src/OSGLUCCO.m` | Type + macro replacements (stays as ObjC) |
| `cmake/CNFUIALL.h.in` | Updated template (if templated) |
| `cmake/CNFUDALL.h.in` | Type references updated |
| `cmake/CNFUDOSG.h.in` | Type references updated |
| `cmake/models/MacII_CNFUDPIC.h` | Type references updated |

### New Files

None.

### Deleted Files

None (files are simplified, not deleted).

---

## Commit Sequence

| # | Commit message | Steps |
|---|---------------|-------|
| 1 | `Switch build to C++17 (source files compiled as C++ via CMake property)` | 2.1 |
| 2 | `Add <cstdint> includes; rewrite custom type definitions as standard type aliases` | 2.2 |
| 3 | `Replace custom integer types with <cstdint> types (ui3b→uint8_t, ui5r→uint32_t, etc.)` | 2.3 |
| 4 | `Replace CPTR typedef with uint32_t` | 2.4 |
| 5 | `Replace blnr/trueblnr/falseblnr with bool/true/false` | 2.5 |
| 6 | `Replace nullpr→nullptr, anyp→void*, ps3p→uint8_t*` | 2.6 |
| 7 | `Replace LOCALVAR→static, GLOBALVAR→(remove), EXPORTVAR→extern` | 2.7 |
| 8 | `Replace visibility macros with standard C++ linkage (static/extern/inline)` | 2.8 |
| 9 | `Remove my_reg_call and my_osglu_call calling-convention macros` | 2.9 |
| 10 | `Remove unused compiler-abstraction macros (MayInline, my_align_8, type guards)` | 2.10 |
| 11 | `Simplify CNFUIALL.h and DFCNFCMP.h after type/macro cleanup` | 2.11 |
| 12 | `Convert #define type aliases to C++ using declarations` | 2.12 |
| 13 | `Replace #error include guards with #pragma once` | 2.13 |
| 14 | `Phase 2 complete: type system and macro cleanup verified` | 2.14 |

---

## Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|-----------|
| `bool` is 1 byte but old `blnr` was `uint8_t` — struct layout could change | Binary layout of `MyEvtQEl` and other structs may shift | Verify `sizeof(MyEvtQEl) == 8` after replacement. `bool` is 1 byte on all target platforms — should be identical. |
| `char *` string literals are `const char *` in C++ | Compiler warnings/errors on functions taking `char *` | Add `-Wno-write-strings` in Step 2.1. Address `const`-correctness in a future phase. |
| `sed` regex matches inside comments or string literals | Corrupted comments/strings | Use word-boundary regex `\b`. Post-replacement, `git diff` review for anomalies. Comments with type names (e.g., `/* ui3b */`) will be updated — this is acceptable and even desirable. |
| `OSGLUCCO.m` compiled as ObjC can't use `nullptr` | Compilation error in the ObjC file | `nullptr` is available in ObjC with `-std=gnu11` or later. If not, keep `NULL` in that one file. Or compile as ObjC++ (`.mm`). |
| Replacing `GLOBALFUNC`/`GLOBALPROC` (which include `noinline`) may change codegen | Possible performance regression in hot paths | Unlikely to matter. The CPU emulation loop in `MINEM68K.c` uses `LOCALFUNC`/`LOCALPROC` (→ `static`), which the compiler can inline at will. Monitor with a quick benchmark (boot-to-desktop time). |
| Some `#define` type aliases are inside `#if` blocks | Conditional `using` declarations | `using` works inside `#if` — no issue. |
| `my_reg_call` in function pointer typedefs | May need careful regex to handle `(my_reg_call *FnPtr)` pattern | Handle specifically: `(my_reg_call *` → `(*` |

---

## Execution Dependencies

- **Phase 1 must be complete** — CMake build working with all presets.
- **No changes to emulation logic** — this phase is purely mechanical type/macro substitution.
- **Each commit is independently buildable and bootable** — if any step breaks the build, bisect is straightforward.
- **Phase 3 (file rename) depends on Phase 2** — renaming `.c` → `.cpp` is simpler after we've already proven C++ compilation works.
