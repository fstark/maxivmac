# Code Quality Assessment: maxivmac

**Date:** 2026-03-27
**Scope:** src/
**Files:** 38 source files, 69 header files
**Lines of Code:** 48,088 (non-blank, non-comment)
**Exclusions:** None (src/macsrc/ included — 4 small C files for classic Mac utilities)

## Scorecard

| # | Metric | Score | Delta | Key Evidence |
|---|--------|-------|-------|-------------|
| M1 | File Organization | 2/5 | — | 12/107 files (11%) exceed 1000 lines; 69/69 headers lack guards |
| M2 | Function Complexity | 2/5 | — | 7,509 deeply-nested lines (4+ levels); m68k_tables.cpp alone has 1,739 |
| M3 | Naming Consistency | 3/5 | — | Dominant PascalCase convention (ParseModelName, BuildMachineConfig) with C-legacy exceptions (t_act, f_act, dbglog_writeReturn) |
| M4 | Memory & Resource Safety | 3/5 | — | Only 2 malloc calls, 26 malloc/free total; 3 sprintf uses; 2 snprintf |
| M5 | Error Handling | 3/5 | — | 302 checks vs 44 failable calls; fopen returns checked in most places |
| M6 | Preprocessor & Dead Code | 2/5 | — | 3,751 directives (78/KLOC); 210 `#if 0` blocks; 0 TODO/FIXME |
| M7 | Documentation | 3/5 | — | 7.1% comment ratio (3,938/55,398 lines); 0 files with zero comments |
| M8 | Build System | 3/5 | — | CMake with presets; -Wall only; no -Wextra or -Werror |
| M9 | Portability | 4/5 | — | Platform code fully isolated in src/platform/; only 1 `#ifdef _WIN32` in sdl.cpp; 0 platform ifdefs in core/cpu/devices |
| M10 | Duplication | 3/5 | — | Significant line-level duplication in CPU emulation (dbglog_writeReturn ×106, t_act/f_act ×76); license headers ×29; mostly structural |

**Overall: 28/50** (Delta: —)

## Detailed Findings

### M1: File Organization (2/5)
**Data:** 12/107 files exceed 1000 lines (11.2%). Largest: m68k.cpp (8,950), fpu_math.h (6,262), sdl.cpp (5,352), m68k_tables.cpp (3,179), scc.cpp (2,969). 69/69 headers have no `#pragma once` or `#ifndef` guards.
**Assessment:** Over 10% of files exceed 1000 lines, putting this firmly in the "2" band. The complete absence of include guards across all 69 headers is a significant structural issue — these rely entirely on include ordering rather than guards to prevent double-inclusion. The CPU and platform files concentrate most of the size.

### M2: Function Complexity (2/5)
**Data:** 7,509 lines at nesting depth ≥4. Worst offenders: m68k_tables.cpp (1,739 deep lines), sdl.cpp (960), disasm.cpp (671), asc.cpp (423), sony.cpp (421), m68k.cpp (420). Large switch/case blocks in CPU decode and device handling drive the count.
**Assessment:** Deep nesting is pervasive across multiple subsystems, not just the CPU. While some nesting is inherent to switch-based dispatch, the volume (7,509 lines, ~25% of source lines) exceeds the "adequate" threshold. Functions in sdl.cpp and machine.cpp are also very long.

### M3: Naming Consistency (3/5)
**Data:** Sampled 50 function definitions. Dominant convention is PascalCase for public functions (ParseModelName, BuildMachineConfig, MachineConfigForModel) and camelCase for methods (WireBus::set, ICTScheduler::add). Legacy C code uses mixed patterns: single-letter vars common in CPU emulation (t_act, f_act, p), macro-style names from original minivmac (dbglog_writeReturn, HaveSetUpFlags).
**Assessment:** There is a clear dominant convention in the modernized C++ code, but the inherited C codebase introduces frequent exceptions. Score 3: dominant convention with frequent exceptions.

### M4: Memory & Resource Safety (3/5)
**Data:** 26 malloc/calloc/realloc/free references total. Only 2 actual malloc calls (both in sdl.cpp). 3 uses of unsafe `sprintf` (state_recorder.cpp lines 103, 368, 373). 2 uses of safe alternatives (snprintf/strncpy). The project is primarily C++ with stack/RAII allocation.
**Assessment:** Very low manual memory management footprint — most code uses C++ idioms (std::unique_ptr visible in machine_obj.cpp). The 3 sprintf calls are the main safety concern, writing to fixed-size char buffers. malloc calls in sdl.cpp do check for NULL. Score 3: mostly safe, some unsafe string usage remains.

### M5: Error Handling (3/5)
**Data:** 302 error-checking patterns (NULL checks, <0, error/fail tests). 44 failable external calls (fopen, fread, fwrite). Sample inspection: config_loader.cpp checks fopen; state_recorder.cpp checks fread return values; sdl.cpp has mixed checking — some fopen calls checked, some fwrite calls cast to void.
**Assessment:** Most external calls are checked, but the explicit `(void) fwrite(...)` casts in sdl.cpp (lines 135, 148) indicate intentional suppression of error checking in logging paths. Error propagation is inconsistent — some functions return bool, others use fprintf+continue. Score 3.

### M6: Preprocessor & Dead Code (2/5)
**Data:** 3,751 preprocessor directives across 48 KLOC = 78 per KLOC. 210 `#if 0` blocks, concentrated in machine.cpp (7), m68k.cpp (3), main.cpp (3), endian.h (2). Many conditional compilation flags (WantCycByPriOp ×113, SCC_dolog ×92, Use68020 ×83, SCC_TrackMore ×71, WantCloserCyc ×61, EmLocalTalk ×37). 0 TODO/FIXME markers.
**Assessment:** While 78 directives/KLOC is well under the 500/KLOC "critical" threshold, the 210 `#if 0` blocks represent substantial dead code. The config-driven preprocessor usage (model/feature flags) is functional but heavy. Score 2 due to the #if 0 volume.

### M7: Documentation (3/5)
**Data:** 3,938 comment lines out of 55,398 total = 7.1% comment ratio. Zero source files completely lack comments. Comment style is mostly inline `/* */` blocks explaining hardware behavior and protocol details (particularly in SCC, VIA, and CPU code).
**Assessment:** Comment ratio falls in the 5–15% band. Comments are present throughout and tend to explain hardware quirks and emulation logic, which is valuable. No formal API documentation or doxygen-style comments. Score 3.

### M8: Build System (3/5)
**Data:** CMakeLists.txt present with CMakePresets.json. cmake/ directory has model configs and template headers. Only `-Wall` found as warning flag — no `-Wextra`, `-Werror`, or `-pedantic`.
**Assessment:** Modern CMake with preset support is a good foundation. The cmake/models/ directory suggests well-organized multi-target builds. However, only `-Wall` is enabled — adding `-Wextra` would catch additional issues. Score 3: standard build system, -Wall enabled.

### M9: Portability (4/5)
**Data:** 1 platform `#ifdef` in non-platform code (src/platform/sdl.cpp:51 `#ifdef _WIN32`). 5 platform-specific includes (all in src/platform/). Zero platform ifdefs in src/core/, src/cpu/, or src/devices/. Platform code cleanly isolated in src/platform/ with src/platform/common/ for shared abstractions.
**Assessment:** Excellent platform separation. All platform-specific code lives in src/platform/. The core emulator, CPU, and device code contain zero platform dependencies. Clean abstraction layer via platform.h. Score 4.

### M10: Duplication (3/5)
**Data:** Top duplicate lines: `dbglog_writeReturn()` ×106, `break;` ×451 (structural), `t_act()`/`f_act()` ×76 each, `V_regs.LazyXFlagKind = kLazyFlagsDefault` ×32, `HaveSetUpFlags()` ×31, license headers ×29. Much duplication is in CPU emulation and disassembly dispatch code.
**Assessment:** The meaningful duplication is concentrated in the CPU emulation code (m68k.cpp, disasm.cpp) where instruction decode patterns repeat. This is common in interpreter-style emulators. The dbglog_writeReturn pattern (106 uses) suggests a logging idiom that could be macro-ized. Score 3: some repeated patterns exist.

## Top 5 Improvement Actions
1. **Add `#pragma once` to all 69 headers** — fixes M1 header discipline completely; mechanical change with high impact (+1 to M1 likely)
2. **Remove `#if 0` blocks** — 210 dead code blocks across the codebase; `git log` preserves history; direct impact on M6 (+1 to M6 likely)
3. **Replace 3 `sprintf` calls with `snprintf`** in state_recorder.cpp — small change, eliminates all unsafe string operations (+0.5 to M4)
4. **Add `-Wextra` to CMakeLists.txt** — one-line change, surfaces additional warnings; stepping stone to `-Werror` (+1 to M8 likely)
5. **Break up sdl.cpp (5,352 lines)** into logical modules (disk I/O, display, input, audio) — reduces largest platform file and nesting (+0.5 to M1, +0.5 to M2)
