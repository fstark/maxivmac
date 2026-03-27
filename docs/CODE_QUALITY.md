# Code Quality Assessment: maxivmac

**Date:** 2026-03-27
**Scope:** src/
**Files:** 38 source files, 70 header files
**Lines of Code:** 47,659 (non-blank, non-comment)
**Exclusions:** None (src/macsrc/ included — 4 small C files for classic Mac utilities)

## Scorecard

| # | Metric | Score | Delta | Key Evidence |
|---|--------|-------|-------|-------------|
| M1 | File Organization | 3/5 | +1 | 12/108 files (11%) exceed 1000 lines; 70/70 headers have `#pragma once` guards |
| M2 | Function Complexity | 2/5 | = | 7,518 deeply-nested lines (4+ levels); m68k_tables.cpp alone has 1,739 |
| M3 | Naming Consistency | 3/5 | = | Dominant PascalCase convention (ParseModelName, BuildMachineConfig) with C-legacy exceptions (t_act, f_act, dbglog_writeReturn) |
| M4 | Memory & Resource Safety | 4/5 | +1 | Only 2 malloc calls (both checked), 26 malloc/free total; 0 unsafe string funcs; 5 snprintf uses |
| M5 | Error Handling | 3/5 | = | 302 checks vs 44 failable calls; fopen returns checked in most places |
| M6 | Preprocessor & Dead Code | 2/5 | = | 3,290 directives (69/KLOC); 211 `#if 0` blocks; 0 TODO/FIXME |
| M7 | Documentation | 3/5 | = | 7.2% comment ratio (3,944/54,997 lines); 0 files with zero comments |
| M8 | Build System | 4/5 | +1 | CMake with presets; -Wall -Wextra -Werror -Wundef; no CI integration |
| M9 | Portability | 4/5 | = | Platform code fully isolated in src/platform/; only 1 `#ifdef _WIN32` in sdl.cpp; 0 platform ifdefs in core/cpu/devices |
| M10 | Duplication | 3/5 | = | Significant line-level duplication in CPU emulation (dbglog_writeReturn ×106, t_act/f_act ×76); break ×451; mostly structural |

**Overall: 31/50** (Delta: +3)

## Detailed Findings

### M1: File Organization (3/5)
**Data:** 12/108 files exceed 1000 lines (11.1%). Largest: m68k.cpp (8,950), fpu_math.h (6,264), sdl.cpp (5,128), m68k_tables.cpp (3,179), scc.cpp (2,969), disasm.cpp (2,939). 70/70 headers have `#pragma once` guards (placed after license comment blocks).
**Assessment:** Over 10% of files exceed 1000 lines. However, all headers now have `#pragma once` guards — the previous assessment incorrectly reported them as missing because the detection script only checked the first 5 lines, while the guards appear after the license block (lines 7–26). With <10% threshold nearly met and all headers guarded, this scores 3: some organization issues remain (large files) but header discipline is complete.

### M2: Function Complexity (2/5)
**Data:** 7,518 lines at nesting depth ≥4 (essentially unchanged from 7,509). Worst offenders: m68k_tables.cpp (1,739), sdl.cpp (960), disasm.cpp (671), asc.cpp (423), m68k.cpp (420), sony.cpp (420), scc.cpp (380), video.cpp (357), machine.cpp (356).
**Assessment:** Deep nesting remains pervasive across multiple subsystems. The volume (7,518 lines, ~25% of source) is essentially unchanged. Switch/case blocks in CPU decode and device handling continue to drive the count.

### M3: Naming Consistency (3/5)
**Data:** Sampled 50 function definitions. Dominant convention is PascalCase for public functions (ParseModelName, BuildMachineConfig, MachineConfigForModel) and camelCase for methods (WireBus::set, ICTScheduler::add). Legacy C code uses mixed patterns: single-letter vars common in CPU emulation (t_act, f_act, p), macro-style names from original minivmac (dbglog_writeReturn, HaveSetUpFlags).
**Assessment:** Clear dominant convention in modernized C++ code, but the inherited C codebase introduces frequent exceptions. Score 3: dominant convention with frequent exceptions.

### M4: Memory & Resource Safety (4/5)
**Data:** 26 malloc/calloc/realloc/free references total. Only 2 actual malloc calls (both in sdl.cpp, both checked for NULL). Zero unsafe string functions (strcpy/strcat/sprintf/gets) — all 3 previous sprintf calls replaced with snprintf. 5 uses of safe alternatives (snprintf). Project is primarily C++ with stack/RAII allocation.
**Assessment:** All allocations are checked. All unsafe string operations have been eliminated — the 3 sprintf calls from the previous assessment are now snprintf. Very low manual memory management footprint with C++ RAII idioms (std::unique_ptr). Score 4: all allocations checked; safe alternatives used; clear ownership model.

### M5: Error Handling (3/5)
**Data:** 302 error-checking patterns (NULL checks, <0, error/fail tests). 44 failable external calls (fopen, fread, fwrite). Sample inspection: config_loader.cpp checks fopen; state_recorder.cpp checks fread return values; sdl.cpp has mixed checking — some fopen calls checked, some fwrite calls cast to void.
**Assessment:** Most external calls are checked, but the explicit `(void) fwrite(...)` casts in sdl.cpp indicate intentional suppression of error checking in logging paths. Error propagation is inconsistent — some functions return bool, others use fprintf+continue. Score 3.

### M6: Preprocessor & Dead Code (2/5)
**Data:** 3,290 preprocessor directives across 47.6 KLOC = 69 per KLOC (down from 3,751 / 78 per KLOC). 211 `#if 0` blocks, concentrated in scc.cpp (72), sdl.cpp (32), m68k_tables.cpp (20), fpu_math.h (17), video.cpp (9), asc.cpp (9). Many conditional compilation flags (WantCycByPriOp ×113, SCC_dolog ×92, Use68020 ×83, SCC_TrackMore ×71, WantCloserCyc ×61). 0 real TODO/FIXME markers.
**Assessment:** Preprocessor density improved from 78 to 69 per KLOC (-461 directives), but the 211 `#if 0` blocks are essentially unchanged from 210. scc.cpp alone contains 72 `#if 0` blocks. The dead code volume keeps this at score 2.

### M7: Documentation (3/5)
**Data:** 3,944 comment lines out of 54,997 total = 7.2% comment ratio. Zero source files completely lack comments. Comment style is mostly inline `/* */` blocks explaining hardware behavior and protocol details (particularly in SCC, VIA, and CPU code).
**Assessment:** Comment ratio falls in the 5–15% band (7.2%). Comments are present throughout and tend to explain hardware quirks and emulation logic. No formal API documentation or doxygen-style comments. Score 3.

### M8: Build System (4/5)
**Data:** CMakeLists.txt with CMakePresets.json (version 6, Ninja generator). Compiler flags: `-Wall -Wextra -Werror -Wundef -Wconditional-uninitialized -Wno-write-strings`. cmake/models/ directory for multi-target builds. No CI integration detected.
**Assessment:** Significant improvement from previous assessment. Now has `-Wall -Wextra -Werror` plus additional warnings (`-Wundef`, `-Wconditional-uninitialized`). Modern CMake with presets and well-organized targets. Lacks CI integration for score 5. Score 4: modern build system with comprehensive warnings.

### M9: Portability (4/5)
**Data:** 1 platform `#ifdef` in non-platform code (src/platform/sdl.cpp:46 `#ifdef _WIN32`). 5 platform-specific includes (all in src/platform/). Zero platform ifdefs in src/core/, src/cpu/, or src/devices/. Platform code cleanly isolated in src/platform/ with src/platform/common/ for shared abstractions.
**Assessment:** Excellent platform separation. All platform-specific code lives in src/platform/. The core emulator, CPU, and device code contain zero platform dependencies. Clean abstraction layer via platform.h. Score 4.

### M10: Duplication (3/5)
**Data:** Top duplicate lines: `dbglog_writeReturn()` ×106, `break;` ×451 (structural), `t_act()`/`f_act()` ×76 each, `#if WantCycByPriOp` ×113, `#if SCC_dolog` ×92, `#if 0` ×83. Much duplication is in CPU emulation and disassembly dispatch code.
**Assessment:** Meaningful duplication is concentrated in CPU emulation code (m68k.cpp, disasm.cpp) where instruction decode patterns repeat. This is common in interpreter-style emulators. The dbglog_writeReturn pattern (106 uses) is a logging idiom. Score 3: some repeated patterns exist.

## Progress Since Last Assessment

**Previous date:** 2026-03-27
**Score change:** 28/50 → 31/50 (+3)

| Metric | Previous | Current | Change | Notes |
|--------|----------|---------|--------|-------|
| M1 | 2 | 3 | +1 | Previous assessment wrong: all 70 headers have `#pragma once` (after license block); detection corrected |
| M2 | 2 | 2 | = | Deep nesting essentially unchanged (7,509→7,518) |
| M3 | 3 | 3 | = | Same naming patterns observed |
| M4 | 3 | 4 | +1 | All 3 sprintf calls replaced with snprintf; zero unsafe string functions remain |
| M5 | 3 | 3 | = | Same error checking patterns |
| M6 | 2 | 2 | = | Directive count dropped 3,751→3,290 (-461); #if 0 essentially unchanged (210→211) |
| M7 | 3 | 3 | = | Comment ratio stable (7.1%→7.2%) |
| M8 | 3 | 4 | +1 | Added -Wextra -Werror -Wundef -Wconditional-uninitialized to CMakeLists.txt |
| M9 | 4 | 4 | = | Platform isolation unchanged |
| M10 | 3 | 3 | = | Same duplication patterns |

**Key improvements:**
- M1: Previous assessment's `head -5` check missed `#pragma once` placed after license blocks (lines 7–26); all 70/70 headers are actually guarded
- M4: All 3 unsafe `sprintf` calls in state_recorder.cpp converted to `snprintf`; zero unsafe string operations now
- M8: Build system went from `-Wall` only to `-Wall -Wextra -Werror -Wundef -Wconditional-uninitialized`; compile-time safety net significantly strengthened
- M6 (partial): 461 fewer preprocessor directives (78→69 per KLOC) though `#if 0` blocks unchanged

**Regressions:**
- None

## Top 5 Improvement Actions
1. **Remove `#if 0` blocks** — 211 dead code blocks (72 in scc.cpp alone); `git log` preserves history; direct impact on M6 (+1 likely)
2. **Break up sdl.cpp (5,128 lines)** into logical modules (disk I/O, display, input, audio) — reduces largest platform file and nesting (+0.5 to M1, +0.5 to M2)
3. **Reduce files over 1000 lines** — 12 files (11%) exceed 1000 lines; getting below 10% (i.e. splitting 2-3 files) would strengthen M1 toward 4
4. **Add CI integration** (GitHub Actions or similar) — one-time setup; with -Werror already in place, CI would catch regressions automatically (+1 to M8)
5. **Extract common CPU dispatch patterns** — the 106× dbglog_writeReturn and 76× t_act/f_act patterns could be abstracted to reduce duplication in m68k.cpp/disasm.cpp (+0.5 to M10)
