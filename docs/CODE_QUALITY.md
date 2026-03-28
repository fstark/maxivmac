# Code Quality Assessment: maxivmac

**Date:** 2026-03-28
**Scope:** src/
**Files:** 51 source files, 65 header files
**Lines of Code:** 45,185 (non-blank, non-comment)
**Exclusions:** None (src/macsrc/ included — 4 small C files for classic Mac utilities)

## Scorecard

| # | Metric | Score | Delta | Key Evidence |
|---|--------|-------|-------|-------------|
| M1 | File Organization | 3/5 | = | 12/116 files (10.3%) exceed 1000 lines; 65/65 headers have `#pragma once` guards |
| M2 | Function Complexity | 2/5 | = | 7,077 deeply-nested lines (4+ levels); m68k_tables.cpp alone has 1,704 |
| M3 | Naming Consistency | 3/5 | = | Dominant PascalCase convention (ParseModelName, BuildMachineConfig) with C-legacy exceptions (t_act, f_act, dbglog_writeReturn) |
| M4 | Memory & Resource Safety | 4/5 | = | Only 2 malloc calls (both checked), 26 malloc/free total; 0 unsafe string funcs; 5 snprintf uses |
| M5 | Error Handling | 3/5 | = | 301 checks vs 44 failable calls; fopen returns checked in most places |
| M6 | Preprocessor & Dead Code | 3/5 | +1 | 2,723 directives (60/KLOC); 34 `#if 0` blocks (down from 211); 0 TODO/FIXME |
| M7 | Documentation | 3/5 | = | 7.1% comment ratio (3,655/51,727 lines); 2 files with zero comments |
| M8 | Build System | 4/5 | = | CMake with presets; -Wall -Wextra -Werror -Wundef; no CI integration |
| M9 | Portability | 4/5 | = | Platform code fully isolated in src/platform/; only 1 `#ifdef _WIN32` in sdl.cpp; 0 platform ifdefs in core/cpu/devices |
| M10 | Duplication | 3/5 | = | Significant line-level duplication in CPU emulation (dbglog_writeReturn ×103, t_act/f_act ×74); break ×432; mostly structural |

**Overall: 32/50** (Delta: +1)

## Detailed Findings

### M1: File Organization (3/5)
**Data:** 12/116 files exceed 1000 lines (10.3%). Largest: m68k.cpp (8,863), fpu_math.h (6,151), sdl.cpp (5,085), m68k_tables.cpp (3,056), disasm.cpp (2,891), scc.cpp (2,504), intl_chars.cpp (1,926), machine.cpp (1,780), sony.cpp (1,542), osglu_common.cpp (1,211), fpu_emdev.h (1,189), control_mode.cpp (1,129). 65/65 headers have `#pragma once` guards. Total file count increased from 108 to 116 (+8) due to refactoring (lang/ localization files, via split).
**Assessment:** 10.3% of files exceed 1000 lines — marginally above the 10% threshold. All headers have `#pragma once` guards. The large files are slightly smaller than the previous assessment (scc.cpp dropped 465 lines, m68k.cpp dropped 87). Score 3: some organization issues remain (large files) but header discipline is complete.

### M2: Function Complexity (2/5)
**Data:** 7,077 lines at nesting depth ≥4 (down from 7,518). Worst offenders: m68k_tables.cpp (1,704), sdl.cpp (962), disasm.cpp (659), m68k.cpp (420), asc.cpp (405), sony.cpp (387), machine.cpp (339), video.cpp (332), intl_chars.cpp (325), scc.cpp (322).
**Assessment:** Deep nesting improved slightly (7,518→7,077, -441 lines) but remains pervasive. scc.cpp deep nesting dropped from 380 to 322 (-58), consistent with the 465-line reduction in that file. Switch/case blocks in CPU decode and device handling continue to drive the count. Score 2.

### M3: Naming Consistency (3/5)
**Data:** Sampled 50 function definitions. Dominant convention is PascalCase for public functions (ParseModelName, BuildMachineConfig, MachineConfigForModel) and camelCase for methods (WireBus::set, ICTScheduler::add). Legacy C code uses mixed patterns: single-letter vars common in CPU emulation (t_act, f_act, p), macro-style names from original minivmac (dbglog_writeReturn, HaveSetUpFlags).
**Assessment:** Clear dominant convention in modernized C++ code, but the inherited C codebase introduces frequent exceptions. Score 3: dominant convention with frequent exceptions.

### M4: Memory & Resource Safety (4/5)
**Data:** 26 malloc/calloc/realloc/free references total (unchanged). Only 2 actual malloc calls (both in sdl.cpp, both checked for NULL). Zero unsafe string functions (strcpy/strcat/sprintf/gets). 5 uses of safe alternatives (snprintf). Project is primarily C++ with stack/RAII allocation.
**Assessment:** All allocations are checked. Zero unsafe string operations. Very low manual memory management footprint with C++ RAII idioms (std::unique_ptr). Score 4: all allocations checked; safe alternatives used; clear ownership model.

### M5: Error Handling (3/5)
**Data:** 301 error-checking patterns (NULL checks, <0, error/fail tests). 44 failable external calls (fopen, fread, fwrite). Sample inspection: config_loader.cpp checks fopen; state_recorder.cpp checks fread return values; sdl.cpp has mixed checking — some fopen calls checked, some fwrite calls cast to void.
**Assessment:** Most external calls are checked, but the explicit `(void) fwrite(...)` casts in sdl.cpp indicate intentional suppression of error checking in logging paths. Error propagation is inconsistent — some functions return bool, others use fprintf+continue. Score 3.

### M6: Preprocessor & Dead Code (3/5)
**Data:** 2,723 preprocessor directives across 45.2 KLOC = 60.2 per KLOC (down from 3,290 / 69 per KLOC). 34 `#if 0` blocks (down dramatically from 211), concentrated in sdl.cpp (27), rtc.cpp (6), screen_map.h (1). Conditional compilation flags still present: WantCycByPriOp ×113, SCC_dolog ×92, Use68020 ×83, SCC_TrackMore ×71, WantCloserCyc ×61. 0 TODO/FIXME markers.
**Assessment:** Major improvement: 177 `#if 0` blocks removed (211→34), with scc.cpp going from 72 to 0 blocks. Preprocessor density dropped from 69 to 60 per KLOC. The remaining 34 `#if 0` blocks are mostly in sdl.cpp (27). This now qualifies as "moderate preprocessor use; few #if 0 blocks" — Score 3.

### M7: Documentation (3/5)
**Data:** 3,655 comment lines out of 51,727 total = 7.1% comment ratio. 2 source files completely lack comments (via.cpp, via2.cpp — likely newly split from via_base.cpp). Comment style is mostly inline `/* */` blocks explaining hardware behavior and protocol details.
**Assessment:** Comment ratio falls in the 5–15% band (7.1%). Two newly-created files (via.cpp, via2.cpp) lack comments — a minor regression from the previous 0 files without comments. Overall comment coverage remains adequate. Score 3.

### M8: Build System (4/5)
**Data:** CMakeLists.txt with CMakePresets.json (version 6, Ninja generator). Compiler flags: `-Wall -Wextra -Werror -Wundef -Wconditional-uninitialized -Wno-write-strings`. cmake/models/ directory for multi-target builds. No CI integration detected.
**Assessment:** Modern CMake with presets and comprehensive warning flags. Lacks CI integration for score 5. Score 4.

### M9: Portability (4/5)
**Data:** 1 platform `#ifdef` in platform code (src/platform/sdl.cpp:46 `#ifdef _WIN32`). 5 platform-specific includes (2 in src/config/, 1 in src/platform/, 2 in src/macsrc/). Zero platform ifdefs in src/core/, src/cpu/, or src/devices/. Platform code cleanly isolated in src/platform/ with src/platform/common/ for shared abstractions.
**Assessment:** Excellent platform separation. All platform-specific code lives in src/platform/. The core emulator, CPU, and device code contain zero platform dependencies. Clean abstraction layer via platform.h. Score 4.

### M10: Duplication (3/5)
**Data:** Top duplicate lines: `dbglog_writeReturn()` ×103 (down from 106), `break;` ×432 (structural), `t_act()`/`f_act()` ×74 each (down from 76), `#if WantCycByPriOp` ×113, `#if SCC_dolog` ×92. Much duplication is in CPU emulation and disassembly dispatch code.
**Assessment:** Meaningful duplication is concentrated in CPU emulation code (m68k.cpp, disasm.cpp) where instruction decode patterns repeat. Slight reduction in logging/action duplicates. Score 3: some repeated patterns exist.

## Progress Since Last Assessment

**Previous date:** 2026-03-27
**Score change:** 31/50 → 32/50 (+1)

| Metric | Previous | Current | Change | Notes |
|--------|----------|---------|--------|-------|
| M1 | 3 | 3 | = | 12/116 (10.3%) vs 12/108 (11.1%); large files slightly smaller; all headers guarded |
| M2 | 2 | 2 | = | Deep nesting reduced 7,518→7,077 (-441) but still pervasive |
| M3 | 3 | 3 | = | Same naming patterns observed |
| M4 | 4 | 4 | = | Unchanged: 2 checked mallocs, 0 unsafe string funcs |
| M5 | 3 | 3 | = | 301 checks vs 44 failable calls (essentially unchanged) |
| M6 | 2 | 3 | +1 | `#if 0` blocks dropped 211→34 (-177); directives 3,290→2,723 (-567); scc.cpp cleaned |
| M7 | 3 | 3 | = | Comment ratio 7.2%→7.1%; 2 new zero-comment files (via.cpp, via2.cpp) |
| M8 | 4 | 4 | = | Same warning flags; still no CI |
| M9 | 4 | 4 | = | Platform isolation unchanged |
| M10 | 3 | 3 | = | Slight reduction in duplicates (dbglog_writeReturn 106→103, t_act/f_act 76→74) |

**Key improvements:**
- M6: 177 `#if 0` blocks removed (211→34) — the largest single cleanup. scc.cpp went from 72 `#if 0` blocks to 0. Preprocessor directive count dropped by 567 (3,290→2,723), density from 69→60 per KLOC
- M2 (partial): Deep nesting reduced by 441 lines (7,518→7,077), though not enough to change the score
- M1 (partial): scc.cpp dropped 465 lines (2,969→2,504); total LOC dropped from 47,659 to 45,185

**Regressions:**
- M7 (minor): 2 files now have zero comments (via.cpp, via2.cpp) vs previously 0 files

## Top 5 Improvement Actions
1. **Remove remaining `#if 0` blocks** — 34 blocks remain (27 in sdl.cpp, 6 in rtc.cpp); clearing these would push M6 toward 4
2. **Break up sdl.cpp (5,085 lines)** into logical modules (disk I/O, display, input, audio) — reduces largest platform file and its 27 `#if 0` blocks (+0.5 to M1, +0.5 to M2, helps M6)
3. **Add comments to via.cpp and via2.cpp** — newly split files lack any comments; quick fix to prevent M7 regression
4. **Add CI integration** (GitHub Actions or similar) — one-time setup; with -Werror already in place, CI would catch regressions automatically (+1 to M8)
5. **Reduce files over 1000 lines** — 12 files (10.3%) exceed 1000 lines; splitting 2-3 borderline files (control_mode.cpp at 1,129, osglu_common.cpp at 1,211) would push below 10% toward M1 score 4
