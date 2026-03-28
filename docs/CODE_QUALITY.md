# Code Quality Assessment: maxivmac

**Date:** 2026-03-28
**Scope:** src/ (excluding src/macsrc/)
**Files:** 56 source files, 73 header files
**Lines of Code:** 44,374 (non-blank, non-comment)
**Exclusions:** src/macsrc/ (4 small C files for classic Mac utilities)

## Scorecard

| # | Metric | Score | Delta | Key Evidence |
|---|--------|-------|-------|-------------|
| M1 | File Organization | 3/5 | = | 12/129 files (9.3%) exceed 1000 lines; sdl.cpp dropped 5,085→1,895; 72/73 headers guarded |
| M2 | Function Complexity | 2/5 | = | 6,957 deeply-nested lines (4+ levels); sdl.cpp nesting dropped 962→207 |
| M3 | Naming Consistency | 3/5 | = | Dominant PascalCase convention (ParseModelName, BuildMachineConfig) with C-legacy exceptions (t_act, f_act, dbglog_writeReturn) |
| M4 | Memory & Resource Safety | 4/5 | = | Only 2 malloc calls (both checked), 24 malloc/free total; 0 unsafe string funcs; 5 snprintf uses |
| M5 | Error Handling | 3/5 | = | 299 checks vs 44 failable calls; fopen returns checked in most places |
| M6 | Preprocessor & Dead Code | 4/5 | +1 | 2,410 directives (54/KLOC); 0 true `#if 0` dead-code blocks (down from 27); 0 TODO/FIXME |
| M7 | Documentation | 3/5 | = | 7.5% comment ratio (3,801/50,933 lines); 0 files with zero comments |
| M8 | Build System | 4/5 | = | CMake with presets; -Wall -Wextra -Werror -Wundef; no CI integration |
| M9 | Portability | 4/5 | = | Platform code fully isolated in src/platform/; 1 `#ifdef _WIN32` in path_utils.h; 0 platform ifdefs in core/cpu/devices |
| M10 | Duplication | 3/5 | = | Line-level duplication in CPU emulation (dbglog_writeReturn ×103, t_act/f_act ×74); break ×425; mostly structural |

**Overall: 33/50** (Delta: +1)

## Detailed Findings

### M1: File Organization (3/5)
**Data:** 12/129 files exceed 1000 lines (9.3%). Largest: m68k.cpp (8,863), fpu_math.h (6,151), m68k_tables.cpp (3,068), disasm.cpp (2,894), scc.cpp (2,507), intl_chars.cpp (1,930), sdl.cpp (1,895), machine.cpp (1,808), sony.cpp (1,557), osglu_common.cpp (1,218), fpu_emdev.h (1,189), control_mode.cpp (1,137). 72/73 headers have `#pragma once` guards (screen_map_inst.h is an intentional include-expansion file). File count increased from 116 to 129 due to sdl.cpp refactoring (split into sdl_keyboard.cpp, sdl_sound.cpp, disk_io.cpp, clipboard.cpp, param_buffers.cpp, path_utils.cpp, rom_loader.cpp, tick_timer.cpp, dbglog_platform.cpp).
**Assessment:** 9.3% of files exceed 1000 lines — now below the 10% threshold (was 10.3%). The most significant change is sdl.cpp dropping from 5,085 to 1,895 lines (-3,190), splitting into 9+ focused modules in src/platform/common/. All headers properly guarded. Score 3: still 12 large files but organization improving through modularization.

### M2: Function Complexity (2/5)
**Data:** 6,957 lines at nesting depth ≥4 (down from 7,077). Worst offenders: m68k_tables.cpp (1,704), mac_roman.cpp (645), disasm.cpp (659), m68k.cpp (420), asc.cpp (405), sony.cpp (387), machine.cpp (339), video.cpp (332), intl_chars.cpp (325), scc.cpp (322), control_mode.cpp (256), via_base.cpp (210), sdl.cpp (207), pmu.cpp (179), osglu_common.cpp (144).
**Assessment:** Deep nesting reduced 7,077→6,957 (-120 lines). sdl.cpp deep nesting dropped dramatically from 962 to 207 (-755) due to the platform refactoring. mac_roman.cpp (645) is now visible as a top offender — data table initialization. Switch/case blocks in CPU decode and device handling continue to dominate. Score 2: still pervasive.

### M3: Naming Consistency (3/5)
**Data:** Sampled 50 function definitions. Dominant convention is PascalCase for public functions (ParseModelName, BuildMachineConfig, MachineConfigForModel, ResolveRomPath) and camelCase for class methods (WireBus::set, ICTScheduler::add, Machine::init). Legacy C code uses mixed patterns: single-letter vars in CPU emulation (t_act, f_act, p), macro-style names from original minivmac (dbglog_writeReturn, HaveSetUpFlags).
**Assessment:** Clear dominant convention in modernized C++ code, but the inherited C codebase introduces frequent exceptions. Score 3: dominant convention with frequent exceptions.

### M4: Memory & Resource Safety (4/5)
**Data:** 24 malloc/calloc/realloc/free references total (down from 26). Only 2 actual malloc calls (path_utils.cpp and clipboard.cpp, both checked for NULL). Zero unsafe string functions (strcpy/strcat/sprintf/gets). 5 uses of safe alternatives (snprintf). Project is primarily C++ with stack/RAII allocation.
**Assessment:** All allocations are checked. Zero unsafe string operations. Malloc calls moved from sdl.cpp to dedicated utility files (path_utils.cpp, clipboard.cpp) during refactoring, improving locality. Score 4.

### M5: Error Handling (3/5)
**Data:** 299 error-checking patterns (NULL checks, <0, error/fail tests). 44 failable external calls (fopen, fread, fwrite). Sample inspection: config_loader.cpp checks fopen; state_recorder.cpp checks fread return values; disk_io.cpp has mixed checking patterns.
**Assessment:** Most external calls are checked. Error propagation is inconsistent — some functions return bool, others use fprintf+continue. Score 3.

### M6: Preprocessor & Dead Code (4/5)
**Data:** 2,410 preprocessor directives across 44.4 KLOC = 54.3 per KLOC (down from 2,723 / 60 per KLOC). 0 true `#if 0` dead-code blocks (down from 27 in sdl.cpp). The 7 grep matches for `#if 0` are all false positives: rtc.cpp uses `#if 0 != pr_HilCol*` (compile-time constant checks) and screen_map.h uses `#if 0 == (ScrnMapr_MapElSz & 3)` (size check). Conditional compilation flags still present: WantCycByPriOp ×113, SCC_dolog ×92, Use68020 ×83, SCC_TrackMore ×71, WantCloserCyc ×61. 0 TODO/FIXME markers.
**Assessment:** Dead code cleanup is complete: all 27 true `#if 0` blocks in sdl.cpp have been removed via the platform refactoring. Preprocessor density dropped from 60 to 54 per KLOC. Remaining directives are feature flags for CPU instruction set variants and debug logging — legitimate conditional compilation. Score 4: no dead code; remaining preprocessor use is for feature flags and include guards.

### M7: Documentation (3/5)
**Data:** 3,801 comment lines out of 50,933 total = 7.5% comment ratio (up from 7.1%). 0 source files completely lack comments (improved from 2). Comment style is mostly inline `/* */` blocks explaining hardware behavior and protocol details.
**Assessment:** Comment ratio improved slightly (7.1%→7.5%) and the previously uncommented via.cpp/via2.cpp now have comments. Still in the 5–15% "adequate" band. Score 3.

### M8: Build System (4/5)
**Data:** CMakeLists.txt with CMakePresets.json (version 6, Ninja generator). Compiler flags: `-Wall -Wextra -Werror -Wundef -Wconditional-uninitialized -Wno-write-strings`. cmake/models/ directory for multi-target builds. No CI integration detected.
**Assessment:** Modern CMake with presets and comprehensive warning flags. Lacks CI integration for score 5. Score 4.

### M9: Portability (4/5)
**Data:** 1 platform `#ifdef` in platform code (src/platform/common/path_utils.h:7 `#ifdef _WIN32`). 4 platform-specific includes (sdl_config.h: sys/param.h, sys/time.h; sdl.cpp: sys/stat.h; disk_io.h: sys/stat.h) — all in src/platform/. Zero platform ifdefs in src/core/, src/cpu/, or src/devices/. Platform code cleanly isolated in src/platform/ with src/platform/common/ for shared abstractions.
**Assessment:** Excellent platform separation. The `#ifdef _WIN32` moved from sdl.cpp to path_utils.h during refactoring — still properly in platform/. All platform-specific code lives in src/platform/. Core, CPU, and device code contain zero platform dependencies. Score 4.

### M10: Duplication (3/5)
**Data:** Top duplicate lines: `dbglog_writeReturn()` ×103 (unchanged), `break;` ×425 (down from 432), `t_act()`/`f_act()` ×74 each (unchanged), `#if WantCycByPriOp` ×113, `#if SCC_dolog` ×92. Much duplication is in CPU emulation and disassembly dispatch code.
**Assessment:** Meaningful duplication is concentrated in CPU emulation code (m68k.cpp, disasm.cpp) where instruction decode patterns repeat structurally. Slight reduction in break statements. Score 3: some repeated patterns exist.

## Progress Since Last Assessment

**Previous date:** 2026-03-28 (scope: src/ including macsrc)
**Score change:** 32/50 → 33/50 (+1)

| Metric | Previous | Current | Change | Notes |
|--------|----------|---------|--------|-------|
| M1 | 3 | 3 | = | 12/129 (9.3%) vs 12/116 (10.3%); sdl.cpp 5,085→1,895 (-3,190 lines); 9 new platform modules |
| M2 | 2 | 2 | = | Deep nesting 7,077→6,957 (-120); sdl.cpp nesting 962→207 (-755) |
| M3 | 3 | 3 | = | Same naming patterns observed |
| M4 | 4 | 4 | = | 2 checked mallocs; malloc refs 26→24; calls moved to utility files |
| M5 | 3 | 3 | = | 299 checks vs 44 failable calls (essentially unchanged) |
| M6 | 3 | 4 | +1 | All 27 true `#if 0` blocks removed (sdl.cpp split); directives 2,723→2,410 (-313); density 60→54/KLOC |
| M7 | 3 | 3 | = | Comment ratio 7.1%→7.5%; zero-comment files 2→0 (via.cpp, via2.cpp now commented) |
| M8 | 4 | 4 | = | Same warning flags; still no CI |
| M9 | 4 | 4 | = | Platform isolation maintained; `#ifdef _WIN32` moved from sdl.cpp to path_utils.h |
| M10 | 3 | 3 | = | dbglog_writeReturn ×103, t_act/f_act ×74 (unchanged); break ×425 (slightly down) |

**Key improvements:**
- M6: All 27 true `#if 0` dead-code blocks eliminated via sdl.cpp refactoring; preprocessor density dropped from 60 to 54 per KLOC
- M1 (partial): sdl.cpp dropped 3,190 lines (5,085→1,895) by splitting into 9+ focused platform modules; percentage of oversized files improved from 10.3% to 9.3%
- M2 (partial): sdl.cpp deep nesting dropped 755 lines (962→207); total nesting down 120 lines
- M7 (minor): via.cpp and via2.cpp now have comments (2→0 zero-comment files); comment ratio up 7.1%→7.5%

**Regressions:**
- None

## Top 5 Improvement Actions
1. **Reduce files over 1000 lines** — 12 files (9.3%) still exceed 1000 lines; splitting control_mode.cpp (1,137) and osglu_common.cpp (1,218) would push below 8% toward M1 score 4
2. **Flatten deep nesting in device code** — asc.cpp (405), sony.cpp (387), scc.cpp (322) have complex switch/case; extracting case handlers into functions would reduce M2 nesting
3. **Add CI integration** (GitHub Actions or similar) — one-time setup; with -Werror already in place, CI would catch regressions automatically (+1 to M8)
4. **Reduce conditional compilation flags** — WantCycByPriOp (×113), SCC_dolog (×92), SCC_TrackMore (×71) could use runtime config or constexpr to reduce preprocessor count toward M6 score 5
5. **Improve comment coverage** — 7.5% ratio is adequate but adding module-level documentation to newly split platform files would push toward 10%+ and M7 score 4
