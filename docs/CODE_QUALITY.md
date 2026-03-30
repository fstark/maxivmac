# Code Quality Assessment: maxivmac

**Date:** 2026-03-30
**Scope:** src/ (excluding src/macsrc)
**Files:** 56 source files, 73 header files
**Lines of Code:** 41,386 (non-blank, non-comment)
**Exclusions:** src/macsrc/ (classic Mac utility sources)

## Scorecard

| # | Metric | Score | Delta | Key Evidence |
|---|--------|-------|-------|-------------|
| M1 | File Organization | 3/5 | = | 11/129 files (8.5%) exceed 1000 lines; 72/73 headers have `#pragma once` (screen_map_inst.h is intentional include-style) |
| M2 | Function Complexity | 2/5 | = | 6,681 deeply-nested lines (4+ levels); m68k_tables.cpp (1,669), disasm.cpp (654), mac_roman.cpp (645) lead |
| M3 | Naming Consistency | 3/5 | = | Dominant PascalCase (ParseModelName, BuildMachineConfig) with C-legacy exceptions (t_act, f_act, dbglog_writeReturn, get_vm_byte) |
| M4 | Memory & Resource Safety | 4/5 | = | 24 malloc/free refs; 2 malloc calls (both checked); 0 unsafe string funcs; 5 snprintf uses |
| M5 | Error Handling | 3/5 | = | 119 error-checking patterns vs 44 failable calls; some fwrite calls cast to (void) in logging |
| M6 | Preprocessor & Dead Code | 3/5 | +1 | 1,173 directives (28/KLOC, down from 69/KLOC); only 12 `#if 0` blocks (down from 211); 0 TODO/FIXME |
| M7 | Documentation | 3/5 | = | 7.7% comment ratio (3,656/47,775 lines); 0 source files with zero comments |
| M8 | Build System | 4/5 | = | CMake with presets; -Wall -Wextra -Werror -Wundef -Wconditional-uninitialized; no CI |
| M9 | Portability | 4/5 | = | 0 platform ifdefs outside src/platform/; 4 platform includes all in src/platform/ |
| M10 | Duplication | 3/5 | = | dbglog_writeReturn ×103, t_act/f_act ×74 each; mostly structural CPU dispatch duplication |

**Overall: 32/50** (Delta: +1)

## Detailed Findings

### M1: File Organization (3/5)
**Data:** 11/129 files exceed 1000 lines (8.5%). Largest: m68k.cpp (8,277), fpu_math.h (5,762), disasm.cpp (2,812), m68k_tables.cpp (2,720), scc.cpp (2,507), sdl.cpp (1,805), machine.cpp (1,725), sony.cpp (1,477), osglu_common.cpp (1,178), fpu_emdev.h (1,123), control_mode.cpp (1,049). 72/73 headers have `#pragma once`; screen_map_inst.h is an intentional include-style header without a guard.
**Assessment:** At 8.5%, files exceeding 1000 lines are now below the 10% threshold (previous assessment: 11.1%). sdl.cpp dropped from 5,128 to 1,805 lines — a major split. m68k.cpp remains the largest at 8,277 lines. Header discipline is complete. Still score 3: below 10% but well above the 5% threshold for score 4, and several very large files remain.

### M2: Function Complexity (2/5)
**Data:** 6,681 lines at nesting depth ≥4 (down from 7,518). Worst offenders: m68k_tables.cpp (1,669), disasm.cpp (654), mac_roman.cpp (645), m68k.cpp (408), asc.cpp (405), sony.cpp (370), machine.cpp (334), video.cpp (332), scc.cpp (322), control_mode.cpp (253), via_base.cpp (209), sdl.cpp (203).
**Assessment:** Deep nesting decreased by 837 lines (11%) from the previous assessment, partly from the sdl.cpp split (960→203). However, 6,681 deeply-nested lines across 32 files remains pervasive. The CPU decode and device handling switch/case blocks continue to drive the count. Score 2: deep nesting still widespread.

### M3: Naming Consistency (3/5)
**Data:** Sampled 50 function definitions. Dominant convention is PascalCase for public functions (ParseModelName, BuildMachineConfig, MachineConfigForModel, PrintUsage). Static helpers use camelCase (parseRAMSize, parseScreenSpec, fileExists, hashToHex). Legacy C code uses mixed patterns: get_vm_byte/put_vm_word (snake_case), dbglog_writeReturn/dbglog_StartLine (mixed), t_act/f_act (abbreviated), HaveSetUpFlags (PascalCase macro-style).
**Assessment:** Clear dominant convention in modernized C++ code (PascalCase public, camelCase private), but inherited C emulation code introduces frequent exceptions. Score 3: dominant convention with frequent exceptions.

### M4: Memory & Resource Safety (4/5)
**Data:** 24 malloc/calloc/realloc/free references total (down from 26). 2 actual malloc calls: path_utils.cpp (checked by caller pattern) and clipboard.cpp (explicit `nullptr ==` check). Zero unsafe string functions (strcpy/strcat/sprintf/gets). 5 snprintf uses. Project is primarily C++ with stack/RAII allocation.
**Assessment:** All allocations checked. Zero unsafe string operations. Very low manual memory management footprint with C++ RAII idioms. Score 4: all allocations checked; safe alternatives used; clear ownership model.

### M5: Error Handling (3/5)
**Data:** 119 error-checking patterns (NULL/nullptr checks, < 0, error/fail tests). 44 failable external calls (fopen, fread, fwrite). Sample: config_loader.cpp checks fopen; state_recorder.cpp checks fread returns; disk_io.cpp checks fopen and fwrite; dbglog_platform.cpp casts fwrite to (void) for logging paths.
**Assessment:** Most external calls are checked. The `(void) fwrite(...)` casts in dbglog_platform.cpp indicate intentional suppression for logging. Error propagation is inconsistent — some functions return bool, others use fprintf+continue. Score 3: most external calls checked, some internal error propagation.

### M6: Preprocessor & Dead Code (3/5)
**Data:** 1,173 preprocessor directives across 41.4 KLOC = 28 per KLOC (down from 3,290 / 69 per KLOC — a 64% reduction). Only 12 `#if 0` blocks remain (down from 211), concentrated in rtc.cpp (6) and main.cpp (3). Key conditionals: SCC_dolog ×93, EmLocalTalk ×53. WantCycByPriOp (previously ×113) completely removed. 0 TODO/FIXME/HACK/XXX markers.
**Assessment:** Massive improvement. Preprocessor density dropped from 69 to 28 per KLOC. `#if 0` dead code blocks dropped from 211 to 12 (94% reduction). The WantCycByPriOp flag was entirely eliminated. Remaining conditionals (SCC_dolog, EmLocalTalk) are legitimate feature toggles. Score 3: moderate preprocessor use; few dead code blocks; zero markers.

### M7: Documentation (3/5)
**Data:** 3,656 comment lines out of 47,775 total = 7.7% comment ratio. Zero source files completely lack comments. Comment style is mostly inline `/* */` blocks explaining hardware behavior and protocol details (particularly in SCC, VIA, and CPU code).
**Assessment:** Comment ratio falls in the 5–15% band (7.7%). Comments are present throughout and tend to explain hardware quirks and emulation logic. No formal API documentation or doxygen-style comments. Score 3: comment ratio adequate, public APIs mostly documented via inline comments.

### M8: Build System (4/5)
**Data:** CMakeLists.txt with CMakePresets.json (Ninja generator). Compiler flags: `-Wall -Wextra -Werror -Wundef -Wconditional-uninitialized -Wno-write-strings`. cmake/ directory for model-specific build configuration. No CI integration detected.
**Assessment:** Modern CMake with presets and comprehensive warnings including -Werror. Well-organized targets. Lacks CI integration for score 5. Score 4: modern build system with strict warnings.

### M9: Portability (4/5)
**Data:** 0 platform `#ifdef` directives in non-platform code (improved from 1 in previous assessment). 4 platform-specific includes, all in src/platform/ (sys/time.h, sys/stat.h ×2). Zero platform ifdefs in src/core/, src/cpu/, or src/devices/. Platform code cleanly isolated in src/platform/ with src/platform/common/ for shared abstractions.
**Assessment:** Excellent platform separation. All platform-specific code lives in src/platform/. The core emulator, CPU, and device code contain zero platform dependencies. The previous `#ifdef _WIN32` in sdl.cpp was resolved during the sdl.cpp split. Score 4: clean platform abstraction.

### M10: Duplication (3/5)
**Data:** Top duplicate lines: `break;` ×425 (structural), `dbglog_writeReturn()` ×103, `#if SCC_dolog` ×92, `t_act()`/`f_act()` ×74 each, `#if SCC_TrackMore` ×71, `uint32_t *p = &V_regs.regs[ArgDat]` ×35, `HaveSetUpFlags()` ×31, `DecodeGetSrcSetDstValue()` ×29.
**Assessment:** Meaningful duplication remains concentrated in CPU emulation (m68k.cpp, disasm.cpp) where instruction decode patterns repeat. This is common in interpreter-style emulators. The dbglog_writeReturn pattern (103 uses, down from 106) is a logging idiom. Score 3: some repeated patterns; abstractions difficult due to emulator structure.

## Progress Since Last Assessment

**Previous date:** 2026-03-27
**Score change:** 31/50 → 32/50 (+1)

| Metric | Previous | Current | Change | Notes |
|--------|----------|---------|--------|-------|
| M1 | 3 | 3 | = | Files >1000 lines: 12→11 (11.1%→8.5%); sdl.cpp split from 5,128→1,805 lines |
| M2 | 2 | 2 | = | Deep nesting: 7,518→6,681 (-837 lines, -11%); sdl.cpp nesting 960→203 |
| M3 | 3 | 3 | = | Same naming patterns observed |
| M4 | 4 | 4 | = | 26→24 malloc/free refs; still 0 unsafe string funcs |
| M5 | 3 | 3 | = | Same error checking patterns; 44 failable calls unchanged |
| M6 | 2 | 3 | +1 | Directives: 3,290→1,173 (-64%); #if 0: 211→12 (-94%); WantCycByPriOp eliminated |
| M7 | 3 | 3 | = | Comment ratio 7.2%→7.7% (scope changed to exclude macsrc) |
| M8 | 4 | 4 | = | Same build configuration |
| M9 | 4 | 4 | = | Platform isolation unchanged; _WIN32 ifdef removed during sdl split |
| M10 | 3 | 3 | = | dbglog_writeReturn 106→103; t_act/f_act 76→74; marginal change |

**Key improvements:**
- M6: Preprocessor directives dropped 64% (3,290→1,173, from 69/KLOC to 28/KLOC). `#if 0` dead code blocks reduced 94% (211→12). WantCycByPriOp flag (113 uses) completely eliminated.
- M1 (partial): sdl.cpp split from 5,128→1,805 lines; files over 1000 dropped from 11.1% to 8.5%. New files created during split (sdl_keyboard.cpp, sdl_sound.cpp, etc.) are well-sized.
- M2 (partial): Deep nesting decreased 11% (7,518→6,681) partly from sdl.cpp restructuring.

**Regressions:**
- None

## Top 5 Improvement Actions
1. **Remove remaining 12 `#if 0` blocks** — concentrated in rtc.cpp (6) and main.cpp (3); git preserves history; could push M6 toward 4/5
2. **Break up m68k.cpp (8,277 lines)** — largest file by far; splitting instruction groups into separate files would improve both M1 and M2
3. **Split fpu_math.h (5,762 lines)** — second largest file; could be organized by FPU operation category; direct impact on M1
4. **Add CI integration** (GitHub Actions) — with -Werror already in place, CI catches regressions automatically; would push M8 to 5/5
5. **Extract common CPU dispatch patterns** — dbglog_writeReturn (×103), t_act/f_act (×74 each), DecodeGetSrcSetDstValue (×29) could use macros or inline helpers to reduce M10 duplication
