# Quality Framework v2

**Date:** 2026-04-30
**Status:** Final

## Why Revise

The v1 framework (10 metrics × 1–5 = /50) exposed structural problems during the April 2026 assessment cycle:

1. **No testing metric.** Going from 0 to 260 tests / 1,619 assertions registered zero score change.
2. **Formatting-sensitive measurements.** Applying clang-format (a quality improvement) inflated M1 line counts and M2 nesting counts, creating false regressions.
3. **Coarse 1–5 scale.** Real progress (e.g., preprocessor density dropping 36%) was invisible because it fell between threshold boundaries.
4. **Binary rubric thresholds.** M6 required literally zero `#if 0` blocks for score 4 — five small blocks produced the same score as a hundred.
5. **Missing dimensions.** No metric for formatting discipline, test coverage, or runtime correctness.
6. **Duplication metric was noise.** Line-level duplicate counting is dominated by structural patterns (`break;`, `else`, `#endif`) that can't be reduced.

## Design Principles

1. **Track progress, not judge perfection.** Small improvements must be visible in the score.
2. **Formatting-immune measurements.** Use non-blank LOC and lizard (which parses AST-level constructs), not raw line counts or tab-sensitive grep.
3. **Formula-driven.** Every metric has an explicit formula mapping raw numbers to 0–100. No subjective scoring.
4. **Tool-assisted.** Use `lizard` for complexity (CCN, NLOC, function count), `llvm-cov` for coverage, `clang-format` for style compliance. Grep-based heuristics only where no better tool exists.
5. **Comparable across time.** A re-run on the same commit must produce the same score.

## Prerequisites

```sh
pip install lizard    # cyclomatic complexity analyzer
# llvm-cov: ships with Xcode / LLVM toolchain
# clang-format: ships with Xcode / LLVM toolchain
```

## Metrics

10 metrics, weighted, summing to **1000 points**.

| # | Metric | Weight | Tool |
|---|--------|--------|------|
| M1 | File Organization | 80 | grep, find |
| M2 | Function Complexity | 120 | **lizard** |
| M3 | Naming & Style | 80 | grep, **clang-format** |
| M4 | Type & Memory Safety | 120 | grep |
| M5 | Error Handling | 80 | grep |
| M6 | Preprocessor Hygiene | 80 | grep |
| M7 | Documentation | 60 | grep, find |
| M8 | Build & Tooling | 80 | ls, grep |
| M9 | Testing & Verification | 140 | doctest, **llvm-cov**, **ASan** |
| M10 | Architecture & Modularity | 80 | grep, find |
|    | **Total** | **1000** | |

**Rationale for weights:** Testing+Safety (M9 140 + M4 120 = 260) and Complexity (M2 120) are the strongest indicators of sustainable code health. Documentation (M7 60) is important but less impactful in an emulator where comments on hardware behavior matter more than API docs.

Each metric produces a raw score 0–100 which is then scaled by its weight ÷ 100.

---

### M1: File Organization (×0.8 = 0–80)

Uses non-blank LOC per file (formatting-immune). Credits module structure.

**Measurements:**
```sh
# Non-blank lines per file
for f in $(find <scope> \( -name '*.c' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) <exclusions>); do
  c=$(grep -cvE '^\s*$' "$f"); echo "$c $f"
done | sort -rn

# Count files over threshold
# ... | awk '$1 > 1000' | wc -l

# Headers without guards
find <scope> \( -name '*.h' -o -name '*.hpp' \) <exclusions> -print0 \
  | xargs -0 grep -LE '#pragma once|#ifndef' 2>/dev/null | wc -l

# Module structure: distinct first-level subdirectories
find <scope> -mindepth 1 -maxdepth 1 -type d | wc -l
```

**Sub-components:**

| Component | Weight | Formula |
|-----------|--------|---------|
| File size | 60 | `60 × max(0, 1 − pct_over_1000 / 20)` where pct_over_1000 = files >1000 nonblank / total files × 100 |
| Header guards | 20 | `20 × guarded_headers / total_headers` |
| Module structure | 20 | 20 if ≥4 subdirs with clear separation; 10 if 2–3; 0 if flat |

---

### M2: Function Complexity (×1.2 = 0–120)

**Uses `lizard`** for cyclomatic complexity (CCN) and function length (NLOC). Format-immune — lizard parses the language, not whitespace.

**Measurements:**
```sh
# Full lizard analysis
lizard <scope> -x '<exclusions>' 2>&1 | tail -5
# → gives: Total nloc, Avg.NLOC, AvgCCN, Avg.token, Fun Cnt, Warning cnt

# High-complexity functions (CCN > 15)
lizard <scope> -x '<exclusions>' --CCN 15 -w 2>&1 | grep 'warning:' | wc -l

# Very high complexity (CCN > 25)
lizard <scope> -x '<exclusions>' --CCN 25 -w 2>&1 | grep 'warning:' | wc -l

# Long functions (NLOC > 100)
lizard <scope> -x '<exclusions>' -L 100 -w 2>&1 | grep 'warning:' | wc -l

# Top 15 by CCN
lizard <scope> -x '<exclusions>' -s cyclomatic_complexity 2>&1 | grep '@' | tail -15

# Top 15 by length
lizard <scope> -x '<exclusions>' -s nloc 2>&1 | grep '@' | tail -15
```

**Sub-components:**

| Component | Weight | Formula |
|-----------|--------|---------|
| Average CCN | 35 | `35 × max(0, 1 − (avg_ccn − 3) / 12)` — avg 3 = 35, avg 15 = 0 |
| High-CCN ratio | 30 | `30 × max(0, 1 − ccn_over_15 / (fun_cnt × 0.15))` — 0% over threshold = 30; 15% = 0 |
| Long function ratio | 25 | `25 × max(0, 1 − nloc_over_100 / (fun_cnt × 0.15))` — 0% over threshold = 25; 15% = 0 |
| Average NLOC | 10 | `10 × max(0, 1 − (avg_nloc − 10) / 40)` — avg 10 = 10, avg 50 = 0 |

---

### M3: Naming & Style Consistency (×0.8 = 0–80)

**Measurements:**
```sh
# Sample function definitions (50)
grep -rnE '^[a-zA-Z].*\w+\s*\(' <scope> --include='*.cpp' <exclusions> \
  | grep -vE '#include|#define|//' | head -50

# Formatting config
ls .clang-format .editorconfig 2>/dev/null

# Formatting compliance (sample 30 files)
find <scope> \( -name '*.c' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) <exclusions> \
  | shuf | head -30 | while read f; do
  clang-format "$f" | diff -q - "$f" >/dev/null 2>&1 || echo "$f"
done | wc -l
```

**Sub-components:**

| Component | Weight | Formula |
|-----------|--------|---------|
| Naming convention | 50 | Sample 50 function defs. Score = `50 × conforming / total` where "conforming" = matches declared convention (PascalCase free funcs, camelCase methods, snake_case locals) |
| Formatting discipline | 50 | .clang-format committed AND ≤10% of sample files differ: `50`; committed + ≤25%: `35`; committed + >25%: `20`; no config but consistent: `10`; inconsistent: `0` |

---

### M4: Type & Memory Safety (×1.2 = 0–120)

Covers memory management, string safety, and C++ type idioms (folding modernization items here).

**Measurements:**
```sh
# Manual allocation
grep -rn 'malloc\|calloc\|realloc\|free(' <scope> --include='*.c' --include='*.cpp' <exclusions> | wc -l

# Unsafe string functions
grep -rn 'strcpy\|strcat\|sprintf\|gets(' <scope> --include='*.c' --include='*.cpp' <exclusions> \
  | grep -v 'snprintf\|fgets\|strncpy' | wc -l

# Safe string alternatives
grep -rn 'strncpy\|strncat\|snprintf\|fgets(' <scope> --include='*.c' --include='*.cpp' <exclusions> | wc -l

# Smart pointers
grep -rn 'unique_ptr\|shared_ptr\|make_unique\|make_shared' <scope> --include='*.cpp' --include='*.h' <exclusions> | wc -l

# enum class vs bare enum
grep -rn 'enum class ' <scope> --include='*.h' --include='*.hpp' <exclusions> | wc -l
grep -rnE '^enum [A-Z]|^\s+enum [A-Z]' <scope> --include='*.h' --include='*.hpp' <exclusions> | grep -v 'enum class' | wc -l

# C-style casts in .cpp files (rough count)
grep -rnE '\(\s*(u?int\d+_t|uint8_t|uint16_t|uint32_t|char|void|bool)\s*\*?\s*\)' <scope> --include='*.cpp' <exclusions> | wc -l

# C++ casts
grep -rn 'static_cast\|reinterpret_cast\|const_cast' <scope> --include='*.cpp' <exclusions> | wc -l
```

**Sub-components:**

| Component | Weight | Formula |
|-----------|--------|---------|
| Memory safety | 40 | Start at 40. −5 per unchecked malloc. −8 per unsafe string func. −1 per raw malloc/free ref if total > 10 (cap −10). |
| String safety | 15 | `15 × safe_calls / max(1, safe_calls + unsafe_calls)` |
| Type modernization | 25 | Enum: `15 × enum_class / max(1, enum_class + bare_enum)`. Cast: `10 × cpp_casts / max(1, cpp_casts + c_casts)`. |
| Ownership | 20 | 20 if smart_ptr > 0 and malloc_count ≤ 5; 15 if smart_ptr > 0; 10 if all mallocs checked; 5 if some unchecked; 0 if widespread unchecked |

---

### M5: Error Handling (×0.8 = 0–80)

**Measurements:**
```sh
# Error-checking patterns
grep -rn 'if.*==.*NULL\|if.*==.*nullptr\|if.*<.*0\|if.*err\|if.*fail\|if.*!.*(' <scope> --include='*.c' --include='*.cpp' <exclusions> | wc -l

# Failable external calls
grep -rn 'fopen\|fread\|fwrite\|socket\|connect\|bind\|listen\|accept\|recv\|send' <scope> --include='*.c' --include='*.cpp' <exclusions> | wc -l

# Intentionally suppressed returns
grep -rn '(void)' <scope> --include='*.c' --include='*.cpp' <exclusions> | wc -l
```

**Formula:**
```
check_ratio = error_checks / max(1, failable_calls)
base = min(85, check_ratio × 12)        # ratio 7:1 ≈ 85
bonus = +5 if project uses consistent error type (grep for custom error enum/class)
penalty = −min(15, max(0, void_suppressions − 3) × 3)
score = clamp(0, 100, base + bonus + penalty)
```

---

### M6: Preprocessor Hygiene (×0.8 = 0–80)

**Measurements:**
```sh
# Preprocessor directives
grep -rn '#if\|#ifdef\|#ifndef\|#else\|#elif\|#endif' <scope> --include='*.c' --include='*.cpp' --include='*.h' --include='*.hpp' <exclusions> | wc -l

# Non-blank LOC
find <scope> \( -name '*.c' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) <exclusions> -exec cat {} + | grep -cvE '^\s*$|^\s*//'

# Dead code blocks
grep -rn '#if 0' <scope> --include='*.c' --include='*.cpp' --include='*.h' --include='*.hpp' <exclusions> | grep -v '#if 0 !=' | wc -l

# TODO/FIXME/HACK markers
grep -rn 'TODO\|FIXME\|HACK' <scope> --include='*.c' --include='*.cpp' --include='*.h' --include='*.hpp' <exclusions> | wc -l
```

**Formula:**
```
density = directives / (nonblank_loc / 1000)     # per KLOC
density_score = clamp(0, 80, 80 − (density − 10) × 2)
dead_penalty = min(15, if_0_count × 3)
marker_penalty = min(5, marker_count)
score = max(0, density_score − dead_penalty − marker_penalty)
```

---

### M7: Documentation (×0.6 = 0–60)

**Measurements:**
```sh
# Comment lines
find <scope> \( -name '*.c' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) <exclusions> -exec cat {} + | grep -cE '^\s*/[/*]|^\s*\*'

# Total lines
find <scope> \( -name '*.c' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) <exclusions> -exec cat {} + | wc -l

# Files with zero comments
for f in $(find <scope> \( -name '*.c' -o -name '*.cpp' \) <exclusions>); do
  [ "$(grep -cE '/[/*]' "$f")" -eq 0 ] && echo "$f"
done | wc -l

# Architecture/design docs
find . -name '*.md' -path '*/docs/*' | wc -l
```

**Sub-components:**

| Component | Weight | Formula |
|-----------|--------|---------|
| Comment ratio | 50 | `min(50, ratio_pct × 5)` — 10% = 50, 6% = 30, 2% = 10 |
| Coverage | 30 | `30 × (1 − zero_comment_files / total_source_files)` |
| Architecture docs | 20 | 20 if docs/ has design docs, README explains project, and build docs exist; 10 if partial; 0 if none |

---

### M8: Build & Tooling (×0.8 = 0–80)

**Measurements:**
```sh
# Build system
ls CMakeLists.txt Makefile meson.build 2>/dev/null

# Warning flags
grep -rn '\-Wall\|\-Wextra\|\-Werror\|\-Wundef\|\-pedantic' CMakeLists.txt cmake/ 2>/dev/null

# Presets
ls CMakePresets.json 2>/dev/null

# Formatting / static analysis
ls .clang-format .clang-tidy 2>/dev/null

# Sanitizer preset
grep -l 'sanitize\|SANITIZE\|asan\|ASAN' CMakeLists.txt CMakePresets.json 2>/dev/null

# CI
ls .github/workflows/*.yml .gitlab-ci.yml Jenkinsfile 2>/dev/null
```

**Sub-components (additive):**

| Component | Points | Criteria |
|-----------|--------|----------|
| Modern build system | 20 | CMake/Meson = 20; Makefile = 10; none = 0 |
| Compiler warnings | 25 | -Wall = 10; +Wextra = +8; +Werror = +7 |
| Build presets | 10 | CMakePresets.json or equivalent |
| Formatting tools | 15 | .clang-format committed = 10; +.clang-tidy = +5 |
| Sanitizer support | 15 | ASan+UBSan preset = 10; +coverage preset = +5 |
| CI | 15 | CI with build+test = 15; build only = 10; none = 0 |

---

### M9: Testing & Verification (×1.4 = 0–140)

The highest-weighted metric. Covers unit tests, runtime correctness (ASan), code coverage, and integration/non-regression tests.

**Measurements:**
```sh
# Unit test framework and count
grep -rn 'TEST_CASE\|TEST_F\|BOOST_AUTO_TEST\|SUBCASE' test --include='*.cpp' 2>/dev/null | wc -l

# Assertion count
grep -rn 'CHECK\|REQUIRE\|ASSERT\|EXPECT' test --include='*.cpp' 2>/dev/null | wc -l

# Test LOC
find test -name '*.cpp' 2>/dev/null -exec cat {} + | wc -l

# Run unit tests
cmake --build <build_dir> --target tests 2>&1 | tail -3
./<build_dir>/tests 2>&1 | tail -5

# Run unit tests under ASan (if ASan build exists)
cmake --build <asan_build_dir> --target tests 2>&1 | tail -3
./<asan_build_dir>/tests 2>&1 | tail -5

# Coverage: extract overall line coverage percentage
# (parse from llvm-cov report HTML or run llvm-cov report)
grep 'Totals' coverage/index.html | grep -oE '[0-9]+\.[0-9]+%' | head -2

# Non-regression tests: golden files
ls test/*.golden 2>/dev/null | wc -l

# Non-regression test runner
ls test/verify.sh test/record.sh 2>/dev/null
```

**Sub-components:**

| Component | Weight | Formula |
|-----------|--------|---------|
| Unit test breadth | 25 | `min(25, test_case_count / 12)` — 300 cases = 25 |
| Assertion density | 10 | `min(10, assertions / max(1, test_cases) × 1.5)` — ratio ~7 = 10 |
| All tests pass | 10 | 10 if all pass; 0 if any fail |
| ASan-clean | 15 | 15 if unit tests pass under ASan; 10 if ASan build exists but not tested; 0 if no ASan |
| Code coverage | 20 | `min(20, line_coverage_pct / 4)` — 80% = 20; 42% = 10; 0% = 0 |
| Non-regression tests | 10 | `min(10, golden_file_count × 2)` — 5+ models = 10 |
| Test infrastructure | 10 | CTest integration = 5; test runner scripts (verify.sh) = +5 |

---

### M10: Architecture & Modularity (×0.8 = 0–80)

Merges Portability (platform isolation) with meaningful duplication measurement. Structural noise (`break;`, `else`, `#endif`) is filtered.

**Measurements:**
```sh
# Platform ifdefs outside platform/
grep -rn '#ifdef _WIN32\|#ifdef __linux__\|#ifdef __APPLE__' <scope> --include='*.c' --include='*.cpp' --include='*.h' <exclusions> | grep -v '<platform_dir>' | wc -l

# Platform includes outside platform/
grep -rn '#include.*<windows.h>\|#include.*<unistd.h>\|#include.*<sys/' <scope> --include='*.c' --include='*.cpp' --include='*.h' <exclusions> | grep -v '<platform_dir>' | wc -l

# Meaningful duplication (filter structural noise)
find <scope> \( -name '*.c' -o -name '*.cpp' \) <exclusions> -exec cat {} + \
  | grep -vE '^\s*$|^\s*[{}]|^\s*#|^\s*break;|^\s*else|^\s*return|^\s*continue;|^\s*default:|^\s*case |^\s*/[/*]|^\s*\*' \
  | sort | uniq -cd | awk '{s+=$1} END {print s}'

# Total meaningful lines (same filter)
find <scope> \( -name '*.c' -o -name '*.cpp' \) <exclusions> -exec cat {} + \
  | grep -cvE '^\s*$|^\s*[{}]|^\s*#|^\s*break;|^\s*else|^\s*return|^\s*continue;|^\s*default:|^\s*case |^\s*/[/*]|^\s*\*'

# Module coupling: files including from >3 distinct src/ subdirectories
for f in $(find <scope> -name '*.cpp' <exclusions>); do
  dirs=$(grep '#include' "$f" | grep -oE '"[^"]*"' | sed 's|"||g;s|/[^/]*$||' | sort -u | wc -l)
  [ "$dirs" -gt 3 ] && echo "$dirs $f"
done | wc -l
```

**Sub-components:**

| Component | Weight | Formula |
|-----------|--------|---------|
| Platform isolation | 40 | `40 × max(0, 1 − leaks / 10)` where leaks = platform ifdefs + platform includes outside platform dir |
| Meaningful duplication | 35 | `35 × max(0, 1 − dup_pct / 15)` where dup_pct = meaningful_dup_lines / meaningful_lines × 100 |
| Module coupling | 25 | `25 × max(0, 1 − wide_coupling_files / total_cpp_files)` where wide_coupling = files using >3 subdirs |

---

## Scoring Procedure

### Phase 0: Previous Assessment
```sh
find . -name '*.md' -exec grep -l 'Code Quality Assessment v2:' {} + 2>/dev/null
```

### Phase 1: Inventory
```sh
find <scope> -name '*.c' -o -name '*.cpp' | wc -l           # source files
find <scope> -name '*.h' -o -name '*.hpp' | wc -l           # header files
find <scope> \( -name '*.c' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) \
  -exec cat {} + | grep -cvE '^\s*$|^\s*//'                  # non-blank LOC
lizard <scope> -x '<exclusions>' 2>&1 | tail -5              # lizard summary
```

### Phase 2: Collect Data
Run all measurement commands for M1–M10 in order. Record every raw number.

### Phase 3: Compute Scores
Apply each formula. Show the computation explicitly:
```
M2 raw: avg_ccn=4.3, ccn_over_15=132/2211, nloc_over_100=141/2211, avg_nloc=18.8
M2.a = 35 × max(0, 1 − (4.3−3)/12) = 35 × 0.892 = 31.2
M2.b = 30 × max(0, 1 − 132/(2211×0.15)) = 30 × max(0, 1 − 0.398) = 30 × 0.602 = 18.1
M2.c = 25 × max(0, 1 − 141/(2211×0.15)) = 25 × max(0, 1 − 0.425) = 25 × 0.575 = 14.4
M2.d = 10 × max(0, 1 − (18.8−10)/40) = 10 × 0.78 = 7.8
M2 raw score = 71.4 → weighted = 71.4 × 1.2 = 85.7
```

### Phase 4: Report

```markdown
# Code Quality Assessment v2: <project>

**Date:** <date>
**Commit:** <short hash>
**Scope:** <directory>
**Files:** <N> source, <M> headers
**Non-blank LOC:** <L>
**Lizard:** <fun_cnt> functions, avg CCN <X>, avg NLOC <Y>
**Exclusions:** <list>

## Scorecard

| # | Metric | Raw | Weighted | Δ | Key Evidence |
|---|--------|-----|----------|---|-------------|
| M1 | File Organization | XX/100 | XX/80 | — | <1-line> |
| M2 | Function Complexity | XX/100 | XX/120 | — | <1-line> |
| M3 | Naming & Style | XX/100 | XX/80 | — | <1-line> |
| M4 | Type & Memory Safety | XX/100 | XX/120 | — | <1-line> |
| M5 | Error Handling | XX/100 | XX/80 | — | <1-line> |
| M6 | Preprocessor Hygiene | XX/100 | XX/80 | — | <1-line> |
| M7 | Documentation | XX/100 | XX/60 | — | <1-line> |
| M8 | Build & Tooling | XX/100 | XX/80 | — | <1-line> |
| M9 | Testing & Verification | XX/100 | XX/140 | — | <1-line> |
| M10 | Architecture | XX/100 | XX/80 | — | <1-line> |

**Total: XXX/1000** (Δ: —)

## Detailed Findings
### M1: File Organization (XX/100 → XX/80)
**Raw data:** ...
**Computation:** ...
**Assessment:** ...
(repeat for M2–M10)

## Progress Since Last Assessment
(if previous exists)

## Top 5 Improvement Actions
1. ...
```

**Delta rules:** `+N`, `−N`, or `=` per metric, computed on weighted scores. Overall delta is sum.

## What NOT to Do

- Do NOT skip data collection or score from memory.
- Do NOT invent data. If a command fails, note it and set that sub-component to 0.
- Do NOT adjust scores for "context." Score what's measured.
- Do NOT merge or reorder metrics.
- Do NOT produce partial reports.
- Do NOT fabricate deltas.

## v1 → v2 Mapping

| v1 Metric | v2 Metric | Key Changes |
|-----------|-----------|-------------|
| M1 File Organization | M1 File Organization | Non-blank LOC (format-immune); module structure credit |
| M2 Function Complexity | M2 Function Complexity | **lizard CCN+NLOC** replaces grep-based nesting; format-immune |
| M3 Naming Consistency | M3 Naming & Style | Adds clang-format compliance sub-component |
| M4 Memory & Resource Safety | M4 Type & Memory Safety | Adds enum class, smart pointers, C-cast ratio |
| M5 Error Handling | M5 Error Handling | Continuous formula; (void) tracking |
| M6 Preprocessor & Dead Code | M6 Preprocessor Hygiene | Linear #if 0 penalty (not binary gate) |
| M7 Documentation | M7 Documentation | Lower ratio targets; architecture doc credit |
| M8 Build System | M8 Build & Tooling | Adds formatting tools, sanitizer presets; CI not a gate |
| M9 Portability | M10 Architecture | Merged into platform isolation sub-component |
| M10 Duplication | M10 Architecture | Structural noise filtered; meaningful dup only |
| *(none)* | **M9 Testing & Verification** | **NEW** — unit tests, ASan, coverage, non-regression, infrastructure |
