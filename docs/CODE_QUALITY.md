# Code Quality Assessment: maxivmac

**Date:** 2026-04-09
**Scope:** src/ (excluding src/macsrc)
**Files:** 58 source files, 84 header files
**Lines of Code:** 40,283 (non-blank, non-comment)
**Exclusions:** src/macsrc/ (classic Mac utility sources)

## Scorecard

| # | Metric | Score | Delta | Key Evidence |
|---|--------|-------|-------|-------------|
| M1 | File Organization | 3/5 | = | 8/142 files (5.6%) exceed 1000 lines; 83/84 headers have `#pragma once` (screen_map_inst.h is intentional include-style) |
| M2 | Function Complexity | 2/5 | = | 5,994 deeply-nested lines (4+ levels); m68k_tables.cpp (1,669), mac_roman.cpp (645), m68k.cpp (408) lead |
| M3 | Naming Consistency | 3/5 | = | Dominant PascalCase (ParseModelName, BuildMachineConfig) with C-legacy exceptions (t_act, f_act, get_vm_byte) |
| M4 | Memory & Resource Safety | 4/5 | = | 28 malloc/free refs; 2 malloc calls (both checked); 0 unsafe string funcs; 35 snprintf/fgets uses |
| M5 | Error Handling | 3/5 | = | 329 error-checking patterns vs 48 failable calls; some fwrite calls cast to (void) in logging |
| M6 | Preprocessor & Dead Code | 3/5 | = | 1,162 directives (29/KLOC); 5 actual `#if 0` dead-code blocks; 3 TODO markers |
| M7 | Documentation | 3/5 | = | 7.5% comment ratio (3,515/46,734 lines); 0 source files with zero comments |
| M8 | Build System | 4/5 | = | CMake with presets; -Wall -Wextra -Werror -Wundef; no CI |
| M9 | Portability | 4/5 | = | 0 platform ifdefs outside src/platform/; 6 platform includes all in src/platform/ |
| M10 | Duplication | 3/5 | = | dbglog_writeReturn ×46, t_act/f_act ×74 each; mostly structural CPU dispatch duplication |

**Overall: 32/50** (Delta: =)

## Detailed Findings

### M1: File Organization (3/5)
**Data:** 8/142 files exceed 1000 lines (5.6%). Largest: m68k.cpp (8,304), fpu_math.h (5,762), m68k_tables.cpp (2,720), scc.cpp (2,507), machine.cpp (1,788), disasm.cpp (1,608), sony.cpp (1,477), fpu_emdev.h (1,123). 83/84 headers have `#pragma once`; screen_map_inst.h is an intentional include-style header without a guard.
**Assessment:** Files exceeding 1000 lines dropped from 8.5% to 5.6% (11→8 files). disasm.cpp shrank from 2,812→1,608 lines. control_mode.cpp (1,049) and osglu_common.cpp (1,178) both dropped below 1000 lines. sdl.cpp no longer appears (was 1,805). Header count grew from 73→84 with good discipline. Still score 3: 5.6% is above the 5% threshold for score 4, and m68k.cpp (8,304) and fpu_math.h (5,762) remain very large.

### M2: Function Complexity (2/5)
**Data:** 5,994 lines at nesting depth ≥4 (down from 6,681). Worst offenders: m68k_tables.cpp (1,669), mac_roman.cpp (645), m68k.cpp (408), asc.cpp (405), sony.cpp (370), machine.cpp (343), video.cpp (332), scc.cpp (322), disasm.cpp (218), via_base.cpp (209), pmu.cpp (179), adb.cpp (98).
**Assessment:** Deep nesting decreased by 687 lines (10.3%) from the previous assessment. disasm.cpp improved dramatically (654→218, -67%). New entry pmu.cpp (179) appeared. However, 5,994 deeply-nested lines across ~30 files remains pervasive. The CPU decode tables and device handling switch/case blocks continue to drive the count. Score 2: deep nesting still widespread.

### M3: Naming Consistency (3/5)
**Data:** Sampled 50 function definitions. Dominant convention is PascalCase for public functions (ParseModelName, BuildMachineConfig, MachineConfigForModel, PrintUsage, ResolveRomPath, DefaultRomFileName, ModelToString). Static helpers use camelCase (parseRAMSize, parseScreenSpec, fileExists, hashToHex, toLower). C++ class methods use camelCase (init, reset, zap, addDevice, recalcIPL, setCycleAccessors, registerTask). Legacy patterns: t_act/f_act (abbreviated), HaveSetUpFlags (PascalCase macro-style).
**Assessment:** Clear dominant convention in modernized C++ code (PascalCase public free functions, camelCase methods and private helpers), but inherited C emulation code introduces frequent exceptions. Score 3: dominant convention with frequent exceptions.

### M4: Memory & Resource Safety (4/5)
**Data:** 28 malloc/calloc/realloc/free references total (up from 24 — additional free() calls from new code). 2 actual malloc calls: path_utils.cpp (checked by caller) and clipboard.cpp (explicit `nullptr ==` check). Zero unsafe string functions (strcpy/strcat/sprintf/gets). 35 safe string function uses (snprintf/fgets, up from 5). Project is primarily C++ with stack/RAII allocation.
**Assessment:** All allocations checked. Zero unsafe string operations. Significant increase in safe string function usage (5→35 snprintf/fgets). Very low manual memory management footprint. Score 4: all allocations checked; safe alternatives used; clear ownership model.

### M5: Error Handling (3/5)
**Data:** 329 error-checking patterns (NULL/nullptr checks, < 0, error/fail tests, negated function returns). 48 failable external calls (fopen, fread, fwrite). Sample: config_loader.cpp checks fopen; state_recorder.cpp checks fread returns; disk_io.cpp checks fopen and fwrite; rom_loader.cpp checks fopen; dbglog_platform.cpp casts fwrite to (void) for logging.
**Assessment:** Most external calls are checked. The `(void) fwrite(...)` pattern in dbglog_platform.cpp is intentional suppression for logging. Error propagation has improved — more bool-returning functions with consistent checking. Score 3: most external calls checked, reasonable error propagation.

### M6: Preprocessor & Dead Code (3/5)
**Data:** 1,162 preprocessor directives across 40.3 KLOC = 29 per KLOC (stable vs 28/KLOC previous). Actual dead-code `#if 0` blocks: 5 (main.cpp ×3, adb_shared.h ×1, sound.cpp ×1). Note: rtc.cpp has 6 `#if 0 != pr_HilCol*` comparison patterns — these are not dead code. 3 TODO markers appeared (imgui_model_selector.cpp ×1, imgui_overlay.cpp ×2). Key conditionals: SCC_dolog ×99, EmLocalTalk ×63.
**Assessment:** Preprocessor density stable at ~29/KLOC. Actual dead-code `#if 0` blocks reduced to 5 (previous count of 12 included rtc.cpp comparison patterns). 3 new TODO markers appeared — minor. SCC_dolog and EmLocalTalk remain legitimate feature toggles. Score 3: moderate preprocessor use; few dead code blocks.

### M7: Documentation (3/5)
**Data:** 3,515 comment lines out of 46,734 total = 7.5% comment ratio. Zero source files completely lack comments. Comment style is mostly inline `/* */` blocks explaining hardware behavior and protocol details (particularly in SCC, VIA, and CPU code).
**Assessment:** Comment ratio falls in the 5–15% band (7.5%, slightly down from 7.7%). Comments are present throughout and explain hardware quirks and emulation logic. No formal API documentation or doxygen-style comments. Score 3: comment ratio adequate, public APIs mostly documented via inline comments.

### M8: Build System (4/5)
**Data:** CMakeLists.txt with CMakePresets.json (Ninja generator). Compiler flags: `-Wall -Wextra -Werror -Wundef`. cmake/ directory for model-specific build configuration. No CI integration detected.
**Assessment:** Modern CMake with presets and comprehensive warnings including -Werror. Well-organized targets. Lacks CI integration for score 5. Score 4: modern build system with strict warnings.

### M9: Portability (4/5)
**Data:** 1 platform `#ifdef` directive total, inside src/platform/. 0 platform ifdefs in src/core/, src/cpu/, or src/devices/. 6 platform-specific includes (sys/stat.h ×2, sys/time.h, sys/param.h ×2, unistd.h), all in src/platform/. Platform code cleanly isolated in src/platform/ with src/platform/common/ for shared abstractions.
**Assessment:** Excellent platform separation. All platform-specific code lives in src/platform/. The core emulator, CPU, and device code contain zero platform dependencies. Platform includes increased from 4→6 due to new platform files (headless_backend.cpp, platform_defs.h), all properly contained. Score 4: clean platform abstraction.

### M10: Duplication (3/5)
**Data:** Top duplicate lines: `break;` ×362 (structural, down from 425), `dbglog_writeReturn()` ×46 (down from 103), `#if SCC_dolog` ×92, `t_act()`/`f_act()` ×74 each, `#if SCC_TrackMore` ×71, `uint32_t *p = &V_regs.regs[ArgDat]` ×35, `HaveSetUpFlags()` ×31, `DecodeGetSrcSetDstValue()` ×29.
**Assessment:** Meaningful duplication improvement: dbglog_writeReturn halved from 103→46 uses. Structural break statements also decreased (425→362). Remaining duplication is concentrated in CPU emulation (m68k.cpp, m68k_tables.cpp) where instruction decode patterns repeat structurally. Score 3: some repeated patterns; abstractions difficult due to emulator structure.

## Progress Since Last Assessment

**Previous date:** 2026-03-30
**Score change:** 32/50 → 32/50 (=)

| Metric | Previous | Current | Change | Notes |
|--------|----------|---------|--------|-------|
| M1 | 3 | 3 | = | Files >1000 lines: 11→8 (8.5%→5.6%); disasm.cpp 2,812→1,608; control_mode.cpp and osglu_common.cpp dropped below 1000 |
| M2 | 2 | 2 | = | Deep nesting: 6,681→5,994 (-687 lines, -10.3%); disasm.cpp 654→218 (-67%) |
| M3 | 3 | 3 | = | Same naming patterns; C++ class methods consistently camelCase |
| M4 | 4 | 4 | = | 24→28 malloc/free refs (more free calls); safe string funcs 5→35 |
| M5 | 3 | 3 | = | Error checks 119→329 (broader pattern); failable calls 44→48 |
| M6 | 3 | 3 | = | Directives 1,173→1,162; actual #if 0 dead code 6→5; 3 new TODOs |
| M7 | 3 | 3 | = | Comment ratio 7.7%→7.5%; total lines 47,775→46,734 |
| M8 | 4 | 4 | = | Same build configuration |
| M9 | 4 | 4 | = | Platform ifdefs still 0 outside platform/; includes 4→6 (new files) |
| M10 | 3 | 3 | = | dbglog_writeReturn 103→46 (-55%); t_act/f_act stable at 74 |

**Key improvements:**
- M1 (raw): Files over 1000 lines dropped from 11 to 8 (8.5%→5.6%). Three files dropped below the threshold: control_mode.cpp, osglu_common.cpp, sdl.cpp. disasm.cpp shrank by 1,204 lines. Approaching the <5% threshold for score 4.
- M2 (raw): Deep nesting decreased 10.3% (6,681→5,994). disasm.cpp improved 67% (654→218 nested lines).
- M10 (raw): dbglog_writeReturn halved from 103→46 uses — significant deduplication effort.
- M4 (raw): Safe string function usage grew 5→35, indicating more code using snprintf/fgets.

**Regressions:**
- 3 new TODO markers appeared in imgui overlay/model selector code (was 0 previously)

## Top 5 Improvement Actions
1. **Split m68k.cpp (8,304 lines) and fpu_math.h (5,762 lines)** — reducing these two files alone would push M1 below 5% threshold (score 4); splitting by instruction group also helps M2
2. **Remove 5 remaining `#if 0` dead-code blocks** — concentrated in main.cpp (3), adb_shared.h (1), sound.cpp (1); git preserves history; reduces M6 noise
3. **Add CI integration** (GitHub Actions) — with -Werror already in place, CI catches regressions automatically; would push M8 to 5/5
4. **Flatten deep nesting in device switch/case blocks** — asc.cpp (405), sony.cpp (370), machine.cpp (343), video.cpp (332), scc.cpp (322) could use early-return or dispatch tables to reduce M2 count
5. **Extract remaining CPU dispatch patterns** — t_act/f_act (×74 each), HaveSetUpFlags (×31), DecodeGetSrcSetDstValue (×29) are candidates for macros or inline helpers to further reduce M10 duplication
