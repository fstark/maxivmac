---
name: feature-planning
description: "Turn a detailed design document (XXX_DESIGN.md) into a phased implementation plan (XXX_PLAN.md). Use when: a design document exists and needs to be broken into buildable, testable, committable phases. Also use when: reviewing or extending an existing implementation plan."
argument-hint: "Path to the design document, e.g. docs/features/DEBUGGER_DESIGN.md"
---

# Feature Planning

Transform a detailed design document into a phased implementation plan.
The input is `XXX_DESIGN.md` (how the feature is built); the output is
`XXX_PLAN.md` (the exact sequence of work to build it).

The plan is executed by the **execute-plan** skill, which expects
numbered phases, build/test gates, and status tracking.

## When to Use

- A `XXX_DESIGN.md` exists with directory layout, interfaces, data
  structures, algorithms, and integration points
- The user wants to start implementation
- The user wants to review or extend an existing `XXX_PLAN.md`

## Ground Rules

- **Read the glossary first.**  Read [docs/GLOSSARY.md](docs/GLOSSARY.md)
  for project terminology.  Use the preferred terms throughout.
- **Read the design first.**  Read the entire `XXX_DESIGN.md`.  Read
  the spec `XXX.md` too — you need both.
- **Read the codebase.**  Verify every integration point in the design
  still matches the current code.  Files move, functions get renamed.
- **Every phase must compile and pass tests.**  No phase may leave the
  build broken.  This means careful ordering: data structures before
  algorithms, internal code before integration hooks.
- **Every phase gets a commit.**  One logical unit of work = one commit.
  If a phase has sub-tasks, they all go in one commit.
- **Be maximally concrete.**  Name every file to create or modify.
  Name every function, struct, and enum.  Show signatures.  Specify
  algorithms in pseudocode or describe them precisely.  The executor
  should not need to re-read the design for clarification.
- **Reference the design, don't repeat it.**  The plan says *what to
  do and in what order*.  For the full rationale, point back to the
  design document's section numbers.
- **Tests accompany code.**  Each phase that adds logic should include
  its unit tests.  Don't defer all testing to a final phase.
- **Follow project conventions.**  Code must follow
  [STYLE.md](docs/STYLE.md) and [NAMING.md](docs/NAMING.md).
  Read both files before writing any code snippets.
- **This is a C++23 project.**  All new interfaces must use modern
  C++: `std::string_view` (not `const char *`), `enum class`,
  `constexpr`, return values over output pointers.  See the
  Language Standard section of STYLE.md.

## Procedure

### Step 1 — Read Design and Spec

1. Read the entire `XXX_DESIGN.md`.
2. Read the matching `XXX.md` spec.
3. Read any referenced project files (STYLE.md, NAMING.md, existing
   source files at integration points).
4. Note if anything in the design is stale (file moved, API changed).
   If so, flag it to the user before proceeding.

### Step 2 — Determine Phase Order

Break the design into phases following this dependency order:

1. **Data types and constants** — enums, structs, static tables.
   No dependencies, always compiles.
2. **Internal modules with unit tests** — parser, expression evaluator,
   symbol tables.  Self-contained; testable without the rest.
3. **Core object skeleton** — the main class/struct with stub methods.
   Compiles but doesn't do anything yet.
4. **Algorithm implementation** — fill in the real logic, one algorithm
   per phase when they're independent.  Each comes with its tests.
5. **Integration hooks** — the surgical insertions into existing code.
   This is where the feature connects to the rest of the codebase.
   Keep each hook in its own phase when possible.
6. **CLI / config wiring** — command-line flags, config parsing.
7. **End-to-end smoke test** — verify the feature works as a whole.

Within each phase, order sub-tasks so the code compiles after each one.

### Step 3 — Write the Plan Document

Create `XXX_PLAN.md` next to the design document.  Follow this format
exactly — the execute-plan skill depends on it:

````markdown
# Feature Name — Implementation Plan

Design: [XXX_DESIGN.md](XXX_DESIGN.md)
Spec: [XXX.md](XXX.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Brief one-line summary | |
| 2 | Brief one-line summary | |
| ... | ... | |

Build gate: `cmake --preset macos && cmake --build --preset macos`
Test gate:  `./bld/macos/tests && cd test && ./verify.sh`

---

## Phase 1 — Title

One-paragraph summary of what this phase achieves and why it's first.

### 1.1 — Sub-task title

Exact instructions: which file to create or modify, what to put in it.
Show struct definitions, function signatures, key constants.

```cpp
// code snippet showing the exact interface or data
```

### 1.2 — Sub-task title

More exact instructions.

### 1.3 — Tests

Which test file to create, what test cases to add.

### Fence

What must be true after this phase:
- [ ] `file.cpp` exists with functions X, Y, Z
- [ ] Unit tests pass: `./bld/macos/tests --test-case="feature*"`
- [ ] Full build clean
- [ ] Commit: `"feature: phase 1 — data types and tables"`

---

## Phase 2 — Title
...
````

### Step 4 — Write Each Phase

For every phase, include:

#### What to build
- Files to create (full path)
- Files to modify (full path, what to change)
- Structs, enums, functions — by name, with signatures
- Algorithms — pseudocode or precise description
- Integration snippets — show the exact insertion with context lines

#### Tests
- Test file path
- Test case names and what they verify
- Expected behavior for edge cases

#### Fence (checkpoint)
A checklist that the executor uses to verify the phase is complete:
- Specific files exist or were modified
- Specific tests pass
- Build is clean
- Commit message format: `"feature: phase N — brief description"`

### Step 5 — Verify the Plan

Before presenting, check:

- **Completeness** — every element from the design (every file, struct,
  function, hook, algorithm) appears in exactly one phase
- **Order** — no phase depends on a later phase
- **Compilability** — after each phase the code compiles and tests pass
- **Concreteness** — a developer (or the execute-plan agent) can
  implement each sub-task without re-reading the design
- **Test coverage** — every phase with logic has tests in the same
  or immediately following phase
- **Naming check** — audit every identifier in code snippets against
  NAMING.md.  Specifically:
  - Class methods → `camelCase`
  - Free functions → `PascalCase` (not `snake_case`, not `camelCase`)
  - Member variables → `camelCase_` (trailing underscore)
  - Globals → `g_camelCase`
  - File-scope statics → `s_camelCase`
  - Constants → `kPascalCase`
- **C++ check** — audit every `const char *` in function signatures —
  should it be `std::string_view`?  Audit raw pointers, C-style casts,
  `#define` constants, and output parameters.  New interfaces must use
  modern C++ (see Language Standard section of STYLE.md).

### Step 6 — Present to the User

Write the file.  In the chat response, give a brief summary: how many
phases, the overall progression, and any design issues found.

## Phase Sizing Guidelines

- **Target: 30–90 minutes of work per phase.**  If a phase would take
  longer, split it.  If shorter than 15 minutes, merge with an
  adjacent phase.
- **Maximum sub-tasks per phase: 5.**  More than that means the phase
  is too big.
- **One integration hook per phase** when hooking into existing hot
  paths — these are high-risk and deserve isolated commits.

## Status Tracking

The execute-plan skill updates the status column as it works:

| Status | Meaning |
|--------|---------|
| *(empty)* | Not started |
| **In progress** | Currently being implemented |
| **Done** | Phase complete, committed |
| **Skipped: reason** | Could not complete, with explanation |

When a phase is Done, the executor does **not** delete it — it marks
it Done so the plan remains a historical record of what was built.
