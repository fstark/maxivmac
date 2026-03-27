# Code Quality Assessment: minivmac (reference/src)

**Date:** 2026-03-27
**Scope:** reference/src
**Files:** 29 source files, 56 header files
**Lines of Code:** 72,416 (non-blank, non-comment); 83,433 total
**Exclusions:** None (all files in reference/src are original project code)

## Scorecard

| # | Metric | Score | Delta | Key Evidence |
|---|--------|-------|-------|-------------|
| M1 | File Organization | 1/5 | — | 23/85 files (27%) exceed 1000 lines; all 56 headers lack include guards |
| M2 | Function Complexity | 2/5 | — | 8,895 deeply nested lines (14.3% of 62K source lines); ~1912 functions |
| M3 | Naming Consistency | 3/5 | — | Dominant PascalCase convention via LOCALPROC/LOCALFUNC macros; frequent exceptions (dbglog_write0, Sony_Insert1) |
| M4 | Memory & Resource Safety | 3/5 | — | 66 malloc/free calls; only 2 unsafe string uses (lstrcpy, sprintf); 0 safer alternatives; most mallocs checked |
| M5 | Error Handling | 3/5 | — | 647 error-check patterns vs 131 failable calls; reasonable coverage |
| M6 | Preprocessor & Dead Code | 2/5 | — | 7,932 preprocessor directives (109.5/KLOC); 340 dead code markers incl. 137 #if 0 blocks |
| M7 | Documentation | 3/5 | — | 5,264 comment lines = 6.3% ratio; no files with zero comments |
| M8 | Build System | 2/5 | — | Custom setup tool generates Xcode/Makefile; no -Wall/-Wextra; no modern build system |
| M9 | Portability | 4/5 | — | Platform code isolated in 7 OSGLU*.c files; only 2 platform #ifdefs in core code |
| M10 | Duplication | 1/5 | — | 20,106 duplicate lines / 43,798 non-trivial = 45.9%; 7 platform backends mirror structure |

**Overall: 24/50** (Delta: —)

## Detailed Findings

### M1: File Organization (1/5)
**Data:** 85 total files (29 .c, 56 .h). 23 files exceed 1000 lines (27%). Largest: MINEM68K.c (8,959), OSGLUWIN.c (6,299), FPMATHEM.h (6,262), OSGLUXWN.c (5,754), OSGLUOSX.c (5,669). All 56 headers lack `#pragma once` or `#ifndef` include guards. All files are in a flat directory structure (single `reference/src/` directory).
**Assessment:** Over a quarter of files exceed 1000 lines, with several exceeding 5000. The complete absence of include guards on all 56 headers is a critical deficiency. The flat file layout with no subdirectories makes navigation difficult for an 85-file codebase.

### M2: Function Complexity (2/5)
**Data:** 8,895 lines with 4+ levels of nesting indentation out of 62,184 source lines (14.3%). Approximately 1,912 function-level braces detected. Average function length ~32 lines, but distribution is heavily skewed — the OSGLU platform files and MINEM68K.c contain very long functions with deep nesting. The CPU emulation code (MINEM68K.c at 8,959 lines) contains deeply nested switch/case blocks.
**Assessment:** Deep nesting is widespread at 14.3% of source lines. While average function length is reasonable, the CPU emulation and platform glue files contain massive functions. The use of macros (LOCALPROC/LOCALFUNC) for visibility rather than actual language features contributes to the monolithic feel.

### M3: Naming Consistency (3/5)
**Data:** Codebase uses custom macros (LOCALPROC, LOCALFUNC, LOCALVAR, GLOBALPROC, GLOBALFUNC, GLOBALVAR) for declaration. Dominant naming is PascalCase (InitDrives, LoadMacRom, ForceShowCursor) with exceptions: snake_case-adjacent names (dbglog_write0, dbglog_open0), prefixed names (Sony_Insert1, vSonyEject0), and single-letter/abbreviated names (tMacErr, blnr, ui3p, ui5r). Type names use abbreviated conventions (tMacErr, blnr=boolean, ui3p=pointer).
**Assessment:** There is a recognizable dominant convention (PascalCase for functions) but with frequent deviations. The custom type abbreviations (blnr, ui3p, ui5r) are terse and non-obvious. The numbered suffix pattern (Sony_Insert0/1/1a/2) suggests incremental development without refactoring.

### M4: Memory & Resource Safety (3/5)
**Data:** 66 malloc/calloc/realloc/free calls across the codebase. Only 2 unsafe string function uses: `lstrcpy` in OSGLUWIN.c:4603 and `sprintf` in OSGLUWIN.c:4844. Zero uses of safer alternatives (strncpy, snprintf, etc.). malloc calls in OSGLUXWN.c show NULL checks (lines 5493, 5508, 5543, 5576). Other platform files show mixed checking.
**Assessment:** Memory management volume is low (66 calls in 72K LOC), mostly in platform glue. Near-zero unsafe string functions is good. However, no safer alternatives are used anywhere — the codebase either avoids string operations or uses unsafe ones. Most malloc calls appear checked but verification is inconsistent across platform files.

### M5: Error Handling (3/5)
**Data:** 647 error-checking patterns (NULL checks, <0 checks, error/fail conditions). 131 calls to failable functions (fopen, fread, fwrite, socket, etc.). Ratio of ~5:1 checks-to-failable-calls suggests reasonable coverage. The codebase uses a custom error type `tMacErr` with explicit return-value checking.
**Assessment:** Error handling is present and uses a custom error code system (tMacErr). The ratio of checks to failable calls is adequate. The pattern of returning error codes via tMacErr functions and checking with `if (err == mnvm_noErr)` is consistent where used. However, there is no unified cleanup-on-failure pattern (no goto-cleanup or RAII equivalent).

### M6: Preprocessor & Dead Code (2/5)
**Data:** 7,932 preprocessor directives = 109.5 per KLOC. 340 dead code markers: 137 `#if 0` blocks, plus TODO/FIXME/HACK/XXX instances. Top preprocessor conditionals: `#if VarFullScreen` (142x), `#if MayFullScreen` (134x), `#if WantCycByPriOp` (109x), `#if Use68020` (103x), `#if EnableMagnify` (101x). The codebase uses preprocessor directives extensively for feature configuration.
**Assessment:** Preprocessor usage is heavy at 109.5 directives per KLOC. The 137 `#if 0` blocks represent substantial dead code. Feature configuration is done entirely through preprocessor conditionals rather than runtime configuration or build-system-level selection, resulting in deeply interleaved conditional compilation throughout the source.

### M7: Documentation (3/5)
**Data:** 5,264 comment lines out of 83,433 total = 6.3% comment ratio. No source files have zero comments. Comments are present but sparse — primarily copyright headers, occasional inline explanations. No API-level documentation for the emulation interfaces.
**Assessment:** Comment ratio of 6.3% falls in the 5–15% range for "adequate." Every file has some comments, but these are mostly boilerplate copyright headers. Internal APIs (device emulation interfaces, memory management, CPU dispatch) lack documentation. The custom type system (blnr, ui3p, tMacErr) is undocumented.

### M8: Build System (2/5)
**Data:** No CMakeLists.txt, Makefile, or meson.build in reference directory. Uses a custom setup tool (`setup/tool.c`) that generates platform-specific build files (Xcode projects, Makefiles). Six platform-specific shell scripts (build_dos.sh, build_haiku.sh, build_linux.sh, build_macos.sh, build_raspi64.sh, build_windows.sh). No compiler warning flags (-Wall, -Wextra, -Werror, -pedantic) found in any build script.
**Assessment:** The custom code-generation build system is unique but non-standard. Developers must first compile `setup/tool.c`, run it with platform-specific flags, then build the generated project. No compiler warnings are enabled in any build configuration. The system works for its supported platforms but is opaque and undocumented.

### M9: Portability (4/5)
**Data:** 7 platform-specific OSGLU*.c files: DOS, GTK, Mac (Classic), NDS, OSX, SDL, Win, X11. Only 2 platform-specific `#ifdef` occurrences in non-OSGLU code. Only 3 platform-specific `#include` directives in core code. Common interface defined in COMOSGLU.h (1,390 lines) and OSGCOMUI.h. The setup tool selects which OSGLU file to compile.
**Assessment:** Platform abstraction is well-designed. Core emulation code is platform-independent, with all platform dependencies isolated in dedicated OSGLU files. The COMOSGLU.h header defines the platform abstraction interface. Only the build-time selection mechanism (setup tool) couples platforms. This is a strong point of the architecture — the emulator core is cleanly portable.

### M10: Duplication (1/5)
**Data:** 20,106 duplicate lines out of 43,798 non-trivial source lines (45.9%). Highest line-level duplicates: `#endif` (3,057x), `break;` (629x), `#else` (228x). Meaningful duplicates: `#if VarFullScreen` (142x), `#if 0` (137x), `dbglog_writeReturn()` (106x), `tMacErr err;` (81x), `t_act()/f_act()` (76x each). The 7 OSGLU platform files (totaling ~28K lines) mirror the same interface pattern with duplicated structure.
**Assessment:** Line-level duplication is extreme. While some is structural (preprocessor guards, break statements), the 7 platform backend files replicate significant logic — each implements the same set of functions (disk I/O, display, sound, input) with platform-specific calls but largely identical control flow. The preprocessor-heavy style amplifies duplication as the same feature guards appear hundreds of times.

## Top 5 Improvement Actions
1. **Add include guards to all 56 headers** — immediate M1 improvement; prevents multiple-inclusion bugs and enables future modularization.
2. **Eliminate #if 0 blocks** (137 instances) — direct M6 and M10 improvement; dead code should be removed or version-controlled, not commented out.
3. **Extract common platform logic into shared functions** — the 7 OSGLU files duplicate disk I/O, display setup, and input handling patterns; factoring common code into COMOSGLU could cut 5–10K lines and improve M10.
4. **Enable -Wall -Wextra in all build configurations** — M8 improvement with cascading benefits to M4/M5 as the compiler catches unchecked returns and unsafe patterns.
5. **Split large files** (MINEM68K.c at 8,959 lines, OSGLU*.c files at 5–6K lines) — M1 and M2 improvement; the CPU emulator and platform files should be split into logical sub-modules.
