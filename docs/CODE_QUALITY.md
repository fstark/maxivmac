# Code Quality Assessment: maxivmac

**Date:** 2026-04-30
**Scope:** src/ (excluding src/macsrc)
**Files:** 86 source files, 108 header files
**Lines of Code:** 54,590 (non-blank, non-comment); 62,801 total
**Exclusions:** src/macsrc/ (classic Mac utility sources)

## Scorecard

| # | Metric | Score | Delta | Key Evidence |
|---|--------|-------|-------|-------------|
| M1 | File Organization | 3/5 | = | 12/194 files (6.2%) exceed 1000 lines; all 108 headers have `#pragma once` or `#ifndef` |
| M2 | Function Complexity | 2/5 | = | 7,170 deeply-nested lines (4+ levels); m68k_tables.cpp (1,706), disasm.cpp (737), m68k.cpp (589) lead |
| M3 | Naming Consistency | 3/5 | = | Dominant PascalCase (ParseModelName, BuildMachineConfig) with C-legacy exceptions (t_act, f_act, get_vm_byte) |
| M4 | Memory & Resource Safety | 4/5 | = | 28 malloc/free refs; 4 calloc/malloc calls (all checked); 0 unsafe string funcs; 84 snprintf/fgets uses |
| M5 | Error Handling | 3/5 | = | 637 error-checking patterns vs 111 failable calls; 5.7:1 check ratio |
| M6 | Preprocessor & Dead Code | 3/5 | = | 960 directives (17.6/KLOC, down from 29); 5 `#if 0` dead-code blocks; 3 TODO markers |
| M7 | Documentation | 3/5 | = | 6.3% comment ratio (3,974/62,801 lines); 2 source files with zero comments |
| M8 | Build System | 4/5 | = | CMake with presets; -Wall -Wextra -Werror; no CI |
| M9 | Portability | 4/5 | = | 0 platform ifdefs outside src/platform/; 9 platform includes outside platform/ (debugger, storage, serial) |
| M10 | Duplication | 3/5 | = | t_act/f_act ×74 each; break ×412; 11,880/29,805 non-trivial duplicate lines (39.9%) |

**Overall: 32/50** (Delta: =)

## Detailed Findings

### M1: File Organization (3/5)
**Data:** 194 total files (86 .cpp/.c, 108 .h/.hpp). 12 files exceed 1000 lines (6.2%). Largest: m68k.cpp (8,918), fpu_math.h (5,801), disasm.cpp (2,862), m68k_tables.cpp (2,851), scc.cpp (2,703), machine.cpp (2,050), sony.cpp (1,617), video.cpp (1,338), extn_extfs.cpp (1,321), fpu_emdev.h (1,243), emulator_shell.cpp (1,101), imgui_backend.cpp (1,020). All 108 headers have `#pragma once` or `#ifndef` guards.
**Assessment:** Codebase grew 36% (142→194 files, 40,283→54,590 LOC) with new features (extfs, debugger, storage, imgui backends). Files over 1000 lines grew from 8→12 (5.6%→6.2%). **Most of this is an artifact of clang-format (52d7cc6, 2026-04-11)** which reflowed code after the previous assessment: pre-format there were 8 files >1000 lines, post-format 11 — video.cpp (1,407), emulator_shell.cpp (1,030), imgui_backend.cpp (1,020) crossed the threshold purely from reformatting. Only extn_extfs.cpp (1,321) is genuinely new code. All headers remain properly guarded. Score 3: 6.2% exceeds the <5% threshold for score 4; m68k.cpp and fpu_math.h remain very large.

### M2: Function Complexity (2/5)
**Data:** 7,170 lines at nesting depth ≥4 (up from 5,994). Worst offenders: m68k_tables.cpp (1,706), disasm.cpp (737), m68k.cpp (589), asc.cpp (488), video.cpp (484), sony.cpp (348), machine.cpp (331), scc.cpp (327), via_base.cpp (228), pmu.cpp (220), adb.cpp (116), imgui_backend.cpp (104), imgui_debug_windows.cpp (102), type_registry.cpp (97), imgui_model_selector.cpp (83).
**Assessment:** **The apparent regression (5,994→7,170) is almost entirely caused by clang-format (52d7cc6).** Comparing the same files before and after the format commit: 5,347→6,415 (+1,068 lines, +20%). clang-format converted tabs to spaces and reflowed multi-line expressions, pushing many lines past the 16-space threshold that the nesting metric counts. The actual code logic and nesting depth did not change for CPU/device files. Genuine new nesting comes only from new modules (imgui_backend 104, imgui_debug_windows 102, type_registry 97, imgui_model_selector 83). Score 2: deep nesting still widespread, though the raw count is inflated by formatting.

### M3: Naming Consistency (3/5)
**Data:** Sampled 50 function definitions. Dominant convention is PascalCase for public free functions (ParseModelName, BuildMachineConfig, MachineConfigForModel, PrintUsage, ResolveRomPath, DefaultRomFileName, ModelToString, ParseCommandLine, BuildEmulatorConfig). Static helpers use camelCase (parseRAMSize, parseScreenSpec, fileExists, toLower). C++ class methods use camelCase (init, reset, zap, addDevice, recalcIPL, setCycleAccessors, registerTask, set, onChange, onPulse). Legacy patterns: t_act/f_act (abbreviated), get_vm_byte/put_vm_byte (snake_case), HaveSetUpFlags (PascalCase macro-style), pbRead/pbWrite (abbreviation prefix).
**Assessment:** Clear dominant convention in modernized C++ code (PascalCase public free functions, camelCase methods and private helpers), consistent with previous assessment. Inherited C emulation code continues to introduce exceptions. Score 3: dominant convention with frequent exceptions.

### M4: Memory & Resource Safety (4/5)
**Data:** 28 malloc/calloc/realloc/free references total (stable). 4 actual allocation calls: emulator_shell.cpp calloc (checked), osglu_common.cpp calloc (checked via *p), param_buffers.cpp calloc (checked), path_utils.cpp malloc (checked by caller). Zero unsafe string functions (strcpy/strcat/sprintf/gets). 84 safe string function uses (snprintf/fgets, up from 35). Project is primarily C++ with stack/RAII allocation.
**Assessment:** All allocations checked. Zero unsafe string operations. Safe string function usage more than doubled (35→84), indicating new code consistently uses snprintf/fgets. Very low manual memory management footprint relative to 54.6 KLOC. Score 4: all allocations checked; safe alternatives used; clear ownership model.

### M5: Error Handling (3/5)
**Data:** 637 error-checking patterns (NULL/nullptr checks, < 0, error/fail tests, negated function returns) — up from 329. 111 failable external calls (fopen, fread, fwrite, socket, connect, bind, listen) — up from 48. Check-to-failable ratio: 5.7:1. New failable calls come from debugger socket code (dbg_io.cpp, dbg_client.cpp using socket/connect/bind/listen) and storage (host_volume.cpp using fopen/fread/fwrite).
**Assessment:** Error checking scaled proportionally with new code. The new debugger and storage modules introduce socket and filesystem calls with checking. The (void) fwrite pattern in dbglog_platform.cpp remains intentional suppression for logging. Score 3: most external calls checked, reasonable error propagation.

### M6: Preprocessor & Dead Code (3/5)
**Data:** 960 preprocessor directives across 54.6 KLOC = 17.6 per KLOC (down from 1,162 at 29/KLOC — a 36% density reduction). Actual dead-code `#if 0` blocks: 5 (main.cpp ×3, adb_shared.h ×1, sound.cpp ×1) — unchanged. 3 TODO markers (imgui_model_selector.cpp ×1, imgui_overlay.cpp ×2) — stable. Key conditionals: SCC_dolog ×92, SCC_TrackMore ×71, EmLocalTalk ×29, SONY_DO_LOG ×21, ASC_dolog ×21.
**Assessment:** Preprocessor density improved massively: directives dropped 17% in absolute terms (1,162→960) while LOC grew 36%, yielding 17.6/KLOC vs 27.2/KLOC previously (measured from the same git baseline). New code is nearly preprocessor-free. Remaining directives are concentrated in SCC device debug logging and are legitimate feature toggles. The 5 `#if 0` dead-code blocks are the sole blocker for score 4 (rubric requires "no dead code"). Removing them would cross the threshold. Score 3: rubric requires zero dead code for 4; density itself is excellent.

### M7: Documentation (3/5)
**Data:** 3,974 comment lines out of 62,801 total = 6.3% comment ratio (down from 7.5%). 2 source files with zero comments: macroman.cpp, dbglog_platform.cpp. Comment style continues to be inline `/* */` blocks explaining hardware behavior and protocol details.
**Assessment:** Comment ratio fell from 7.5%→6.3% as new code was added with fewer comments relative to existing commented code. Absolute comment count increased (3,515→3,974, +459 lines). Two files now have zero comments (was 0). Still in the 5–15% adequate band. Score 3: comment ratio adequate.

### M8: Build System (4/5)
**Data:** CMakeLists.txt with CMakePresets.json (Ninja generator). Compiler flags: `-Wall -Wextra -Werror`. Dedicated test target with `-Wall -Wextra -Werror -UNDEBUG` and CTest integration. 260 unit tests (doctest framework) with 1,619 assertions — all passing. .clang-format committed for codebase-wide formatting consistency. cmake/ directory for model-specific configuration. No CI integration detected.
**Assessment:** Modern CMake with presets, strict warnings including -Werror, and a comprehensive test suite (260 test cases / 1,619 assertions via doctest + CTest) — up from zero tests at the previous assessment. The test suite covers debugger, tracing, SLIP codec, AppleDouble, host volume, drive manager, and type registry. A committed .clang-format enforces consistent style. The only gap for score 5 is CI integration (rubric: "presets; CI integration"). Score 4: modern build system with strict warnings and well-organized targets including tests.

### M9: Portability (4/5)
**Data:** 1 platform `#ifdef` directive total (inside src/platform/). 0 platform ifdefs outside src/platform/. 15 platform-specific includes total; 9 outside src/platform/: core/main.cpp (unistd.h), storage/host_volume.cpp (unistd.h), debugger/dbg_io.cpp (sys/socket.h, sys/un.h, unistd.h), debugger/dbg_client.cpp (sys/socket.h, sys/un.h, unistd.h), devices/serial_pty.h (unistd.h). 6 includes in src/platform/ (sys/stat.h ×2, sys/time.h, sys/param.h ×2, unistd.h).
**Assessment:** #ifdef discipline remains perfect: 0 platform ifdefs outside platform/. However, platform-specific includes leaked significantly (6→15 total, 0→9 outside platform/). The debugger socket code (POSIX-only), serial PTY support, and host filesystem code introduce POSIX dependencies outside the platform layer. These are inherently platform-specific features that should ideally have platform abstractions. Score 4: clean platform abstraction overall, but POSIX includes spreading into non-platform modules is a concern.

### M10: Duplication (3/5)
**Data:** 11,880 duplicate lines out of 29,805 non-trivial source lines (39.9%). Top duplicates: `break;` ×412 (up from 362), `#endif` ×390, `else` ×332, `t_act()`/`f_act()` ×74 each (stable), `#if SCC_dolog` ×92, `#if SCC_TrackMore` ×71, `0xFF,` ×125, `uint32_t *p = &V_regs.regs[ArgDat]` ×35, `HaveSetUpFlags()` ×31, `DecodeGetSrcSetDstValue()` ×29, `DisasmModeRegister(...)` ×27.
**Assessment:** Structural duplication continues from CPU decode tables and device switch/case blocks. t_act/f_act remain at 74 (unchanged). break increased 362→412 from new switch/case code. New: DisasmModeRegister ×27 (disassembler growth). Duplication ratio is high at 39.9% of non-trivial lines, but most is structural (break, endif, else) rather than copy-paste logic. Score 3: some repeated patterns; CPU/device emulation structure makes further reduction difficult.

## Progress Since Last Assessment

**Previous date:** 2026-04-09
**Score change:** 32/50 → 32/50 (=)

| Metric | Previous | Current | Change | Notes |
|--------|----------|---------|--------|-------|
| M1 | 3 | 3 | = | Files >1000: 8→12 (5.6%→6.2%); clang-format pushed 3 files over threshold; 1 genuinely new (extn_extfs) |
| M2 | 2 | 2 | = | Deep nesting: 5,994→7,170 (+1,176); **+1,068 from clang-format whitespace change**; real nesting ~stable |
| M3 | 3 | 3 | = | Same naming patterns; new code follows conventions |
| M4 | 4 | 4 | = | malloc/free refs stable at 28; safe string funcs 35→84 (+140%) |
| M5 | 3 | 3 | = | Error checks 329→637; failable calls 48→111; ratio 6.9:1→5.7:1 |
| M6 | 3 | 3 | = | Directives 1,162→960 (-17%); density 27.2→17.6/KLOC (-36%); #if 0 stable at 5 (only blocker for score 4) |
| M7 | 3 | 3 | = | Comment ratio 7.5%→6.3%; absolute comments 3,515→3,974 (+459); 0→2 zero-comment files |
| M8 | 4 | 4 | = | Same build config + **260 unit tests added** (0→260 test cases, 1,619 assertions); only CI blocks score 5 |
| M9 | 4 | 4 | = | Platform ifdefs still 0 outside platform/; platform includes 6→15 (9 outside platform/) |
| M10 | 3 | 3 | = | t_act/f_act stable ×74; break 362→412; new DisasmModeRegister ×27 |

**Key improvements:**
- M6 (raw): Preprocessor density dropped 39% (29→17.6/KLOC). Absolute directive count fell 17% (1,162→960) despite LOC growing 36%. New code is nearly preprocessor-free.
- M4 (raw): Safe string function usage more than doubled (35→84 snprintf/fgets). All new code uses safe alternatives.
- M5 (raw): Error-checking patterns nearly doubled (329→637) tracking the growth in failable calls (48→111).
- M7 (raw): Absolute comment count increased by 459 lines (3,515→3,974).

**Regressions:**
- M1 (raw): Files >1000 lines grew 8→12 (5.6%→6.2%); **mostly clang-format whitespace inflation** — pre-format 8, post-format 11 files crossed threshold. Only extn_extfs.cpp (1,321) is genuinely new code.
- M2 (raw): Deep nesting increased 5,994→7,170 (+1,176 lines); **+1,068 of this is clang-format artifact** (tab→space + line reflow pushed lines past 16-space threshold). Genuine new nesting only from new imgui/type_registry modules.
- M7 (raw): Comment ratio fell 7.5%→6.3%; 2 files now have zero comments (was 0)
- M9 (raw): Platform-specific includes leaked outside platform/: 0→9 (debugger POSIX sockets, serial PTY, host filesystem)

## Framework Limitations

This assessment cycle revealed blind spots in the 10-metric framework:

1. **No Testing metric.** The project went from 0 to 260 unit tests (1,619 assertions) with doctest + CTest. This is arguably the most significant quality improvement since the last assessment, but no metric captures it. Testing is only tangentially reflected in M8 (build system).
2. **Formatting penalizes M1/M2.** Applying clang-format across the codebase is a quality improvement (consistent style, committed .clang-format), but the whitespace changes artificially inflate line counts (+1,254 lines in disasm.cpp alone) and nesting measurements (+1,068 deep-nesting lines globally). The metrics punish the project for being better formatted.
3. **Threshold gaps.** Several metrics improved substantially in raw numbers but didn't cross rubric thresholds: M6 preprocessor density dropped 36% but 5 `#if 0` blocks block the upgrade; M4 safe string usage grew 140% but was already at score 4; M7 absolute comments grew 30% but the ratio fell because code grew faster.

## Top 5 Improvement Actions
1. **Split m68k.cpp (8,918 lines) and fpu_math.h (5,801 lines)** — these two files alone skew M1 and M2 heavily; splitting by instruction group would also reduce deep nesting. disasm.cpp (2,862) is also a candidate.
2. **Add platform abstraction for POSIX socket/PTY/filesystem calls** — the 9 platform includes outside src/platform/ (debugger sockets, serial_pty, host_volume, main.cpp) should route through platform abstractions to maintain M9 discipline and enable future portability.
3. **Remove 5 remaining `#if 0` dead-code blocks** — main.cpp (3), adb_shared.h (1), sound.cpp (1); git preserves history; would clean up M6.
4. **Flatten deep nesting in device switch/case blocks** — video.cpp (484), asc.cpp (488), sony.cpp (348), machine.cpp (331), scc.cpp (327) could use early-return or dispatch tables to reduce M2 count.
5. **Add CI integration** (GitHub Actions) — with -Werror already in place, CI catches regressions automatically; would push M8 to 5/5.
