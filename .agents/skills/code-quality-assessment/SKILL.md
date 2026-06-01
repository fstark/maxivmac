---
name: code-quality-assessment
description: "Assess C/C++ codebase quality with reproducible metrics. Use when: evaluating code health, comparing quality across releases, auditing technical debt, benchmarking a C or C++ project."
argument-hint: "Optional: path to directory or file scope, e.g. src/core/"
---

# C/C++ Code Quality Assessment v2

Produce a structured, reproducible quality scorecard for this C/C++ codebase using the **v2 weighted framework** defined in `docs/QUALITY_FRAMEWORK_V2_PROPOSAL.md`. 10 metrics × 0–100 raw, weighted to a **total of /1000**.

Two runs on the same commit MUST produce the same score. Progress between assessments matters more than absolute scores.

## Principles

1. **Measure first, score second.** Gather quantitative data with tools before computing scores.
2. **Formula-driven.** Every metric has an explicit formula — no subjective judgment.
3. **Formatting-immune.** Use non-blank LOC and `lizard` (AST-level) — not raw line counts or tab-sensitive grep.
4. **Show evidence.** Every score cites raw numbers and explicit computations.
5. **Fixed scope.** Assess `.c`, `.cpp`, `.h`, `.hpp` files. Exclude vendored/generated code (libs/, macsrc/).
6. **Deterministic order.** M1–M10 in order, same commands each time.
7. **Record raw numbers.** Include all raw counts so future runs can compare deltas.

## Prerequisites

Ensure these are available before starting:

```sh
pip install lizard    # cyclomatic complexity (CCN, NLOC, function count)
# llvm-cov: ships with Xcode / LLVM
# clang-format: ships with Xcode / LLVM
```

## Procedure

### Phase 0: Check for Previous Assessment

```sh
find . -name '*.md' -exec grep -l 'Code Quality Assessment v2:' {} + 2>/dev/null
```
If found, read and note previous scores and raw numbers for delta computation.

### Phase 1: Scope and Inventory

1. Identify target directory (use argument if provided, else `src/`).
2. Record the current git commit hash: `git rev-parse --short HEAD`
3. Collect inventory:
   ```sh
   find <scope> -name '*.c' -o -name '*.cpp' | wc -l           # source files
   find <scope> -name '*.h' -o -name '*.hpp' | wc -l           # header files
   find <scope> \( -name '*.c' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) \
     -exec cat {} + | grep -cvE '^\s*$|^\s*//'                  # non-blank LOC
   lizard <scope> -x '<exclusions>' 2>&1 | tail -5              # lizard summary
   ```
4. Note exclusions in report.

### Phase 2: Data Collection (M1–M10)

Run ALL measurement commands from the framework document for each metric. Record every raw number. Do not skip any metric.

Refer to `docs/QUALITY_FRAMEWORK_V2_PROPOSAL.md` for the exact measurement commands and formulas for each metric. The metrics are:

| # | Metric | Weight | Tool |
|---|--------|--------|------|
| M1 | File Organization | ×0.8 (0–80) | grep, find |
| M2 | Function Complexity | ×1.2 (0–120) | **lizard** |
| M3 | Naming & Style | ×0.8 (0–80) | grep, **clang-format** |
| M4 | Type & Memory Safety | ×1.2 (0–120) | grep |
| M5 | Error Handling | ×0.8 (0–80) | grep |
| M6 | Preprocessor Hygiene | ×0.8 (0–80) | grep |
| M7 | Documentation | ×0.6 (0–60) | grep, find |
| M8 | Build & Tooling | ×0.8 (0–80) | ls, grep |
| M9 | Testing & Verification | ×1.4 (0–140) | doctest, **llvm-cov**, **ASan** |
| M10 | Architecture & Modularity | ×0.8 (0–80) | grep, find |
|    | **Total** | **1000** | |

### Phase 3: Compute Scores

Apply each formula from the framework document. **Show the computation explicitly** for every sub-component:

```
M2 raw: avg_ccn=4.3, ccn_over_15=132/2211, nloc_over_100=141/2211, avg_nloc=18.8
M2.a = 35 × max(0, 1 − (4.3−3)/12) = 35 × 0.892 = 31.2
M2.b = 30 × max(0, 1 − 132/(2211×0.15)) = 30 × max(0, 1 − 0.398) = 30 × 0.602 = 18.1
M2.c = 25 × max(0, 1 − 141/(2211×0.15)) = 25 × max(0, 1 − 0.425) = 25 × 0.575 = 14.4
M2.d = 10 × max(0, 1 − (18.8−10)/40) = 10 × 0.78 = 7.8
M2 raw score = 71.4 → weighted = 71.4 × 1.2 = 85.7
```

### Phase 4: Report

Write the report to `docs/CODE_QUALITY.md` in this format:

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
(if previous exists — compare weighted scores)

## Top 5 Improvement Actions
1. <actionable item with expected impact on specific metric>
...
```

**Delta rules:**
- If no previous v2 assessment exists, use `—` for all deltas.
- If a previous assessment exists, show `+N`, `−N`, or `=` for each metric (weighted scores).
- Overall delta is sum of individual deltas.

## What NOT to Do

- Do NOT skip data collection or score from memory/impressions.
- Do NOT invent data. If a command fails, note it and set that sub-component to 0.
- Do NOT adjust scores for "context." Score what's measured.
- Do NOT merge or reorder metrics.
- Do NOT produce partial reports. All 10 metrics must be scored.
- Do NOT fabricate deltas. Compare only against a real previous v2 assessment found in the workspace.
- Do NOT use v1 assessments for delta comparison — they use a different scale.
