---
name: feature-design
description: "Turn a feature specification (XXX.md) into a detailed design document (XXX_DESIGN.md). Use when: a feature spec exists and needs a concrete design covering directory layout, object model, integration points, algorithms, data structures, build integration, and testing. Also use when: reviewing or extending an existing design document."
argument-hint: "Path to the feature spec, e.g. docs/features/DEBUGGER.md"
---

# Feature Design

Transform a feature specification into a detailed, implementation-ready
design document.  The input is `XXX.md` (what the feature does); the
output is `XXX_DESIGN.md` (how it is built).

## When to Use

- A `XXX.md` feature spec already exists in `docs/`
- The user wants a design document before implementation
- The user wants to review or extend an existing `XXX_DESIGN.md`

## Ground Rules

- **Read the spec first.** Read the entire `XXX.md` before writing
  anything.  Understand every command, behavior, and constraint.
- **Read the codebase.** Explore the integration points the feature
  needs.  Find the exact files, functions, and line numbers where
  hooks will be inserted.  Do not guess — verify.
- **Reuse what exists.** Search for existing infrastructure (tables,
  APIs, subsystems) that the feature can build on.  Never duplicate
  data or logic that already exists.  Call it out explicitly when
  reusing something.
- **Follow project conventions.** All code must adhere to
  [STYLE.md](docs/STYLE.md) and [NAMING.md](docs/NAMING.md).
  Read both files before writing any code snippets.
- **This is a C++23 project.** Use modern C++ in all interfaces:
  `std::string_view` (not `const char *`) for read-only string
  parameters, `enum class` (not bare ints), `constexpr`, return
  values (not output pointers) where feasible.  See the Language
  Standard section of STYLE.md.
- **Minimize integration surface.** The design should touch as few
  existing files as possible.  Prefer a single public header that
  the rest of the codebase sees.  Guard hooks so they have zero cost
  when the feature is inactive.
- **Don't document rejected ideas.** Only describe what will be built.
  Do not add "Non-goals" or "alternatives considered" sections unless
  a rejected approach is a likely implementation trap.
- **Don't plan the work.** The design describes *what* to build and
  *where* it connects.  Implementation ordering, phasing, and task
  breakdown belong in a separate `XXX_PLAN.md` — not here.

## Procedure

### Step 1 — Read the Spec

1. Read the entire `XXX.md` file.
2. Identify: user-facing behaviors, command/API surface, data inputs
   and outputs, performance constraints.
3. Note anything ambiguous — ask the user before proceeding if the
   ambiguity affects the design.

### Step 2 — Explore the Codebase

Before writing a single line of design, answer these questions by
reading actual source files:

- **Where does the feature hook in?**  Find the exact functions,
  loops, or dispatch points where the feature connects to existing
  code.  Record file paths and line numbers.
- **What existing APIs can it call?**  Look for public headers that
  already provide what the feature needs (register access, memory
  access, symbol tables, etc.).
- **What data already exists?**  Search for tables, dictionaries, or
  config structs that the feature would otherwise have to duplicate.
- **What are the performance constraints?**  If the feature hooks
  into a hot path, measure or estimate the cost.
- **What is the build structure?**  Check CMakeLists.txt and directory
  layout to understand where new files belong.

Use subagents for thorough exploration when needed.  Do not proceed
to writing until you have concrete file paths and function signatures.

### Step 3 — Write the Design Document

Create `XXX_DESIGN.md` in the same directory as `XXX.md`.  Follow this
structure (skip sections that don't apply):

```markdown
# Feature Name — Detailed Design

Implements the specification in [XXX.md](XXX.md).

All code must follow [STYLE.md](STYLE.md) and [NAMING.md](NAMING.md).

---

## 1. Directory Layout
Where new files live.  Show the tree.  Explain the split rationale.

## 2. Public Interface
The header that external code includes.  Show the class/struct/API
with doc comments.  This is the contract.

## 3. Integration Points
Exactly which existing files are modified, with code snippets showing
the insertion.  Number each point.  State the performance cost.

## 4. Internal State
Data structures, with field-level comments.  Show the actual structs.

## 5. Key Algorithms
Pseudocode for the non-obvious logic.  One subsection per algorithm.

## 6. Reused Infrastructure
Explicitly list every existing API, table, or subsystem the feature
builds on.  Explain how it's used and that nothing is duplicated.

## 7. Build Integration
CMakeLists.txt changes.  Dependencies between new and existing files.

## 8. Dependency Diagram
ASCII art or brief description showing which modules depend on what.
Arrows must point in one direction — no circular dependencies.

## 9. Testing
What to test, where test files live, which framework to use.

## N. (Additional sections as needed)
```

### Step 4 — Verify Consistency

After writing, check:

- Every behavior in `XXX.md` has a corresponding design element
- Every integration point references a real file and function
- No data is duplicated from existing tables or subsystems
- The dependency graph has no cycles
- The file summary line counts are plausible
- **Naming check**: audit every identifier in code snippets against
  NAMING.md.  Specifically:
  - Class methods → `camelCase`
  - Free functions → `PascalCase` (not `snake_case`, not `camelCase`)
  - Member variables → `camelCase_` (trailing underscore)
  - Globals → `g_camelCase`
  - File-scope statics → `s_camelCase`
  - Constants → `kPascalCase`
- **C++ check**: audit every `const char *` in function signatures —
  should it be `std::string_view`?  Audit raw pointers — should they
  be references or smart pointers?  Audit C-style casts, `#define`
  constants, and output parameters.

### Step 5 — Present to the User

Write the file.  Give a brief summary of the key design decisions
in the chat response (not in the document).

## Style Guidelines for the Design Document

- **Be concrete.** Show code snippets, struct definitions, and
  pseudocode — not prose descriptions of what code might look like.
- **Be precise about integration.** "Add a hook in the CPU loop"
  is insufficient.  "Insert after line N of `m68k_go_MaxCycles()` in
  `src/cpu/m68k.cpp`, guarded by `if (g_flag)`" is correct.
- **State costs.** For every hot-path hook, state the cost when the
  feature is off and when it is on.
- **Keep sections short.** Each section should answer one question.
  If a section exceeds ~40 lines, split it.
- **Use fenced code blocks** for all code, structs, and pseudocode.
  Specify the language (`cpp`, `cmake`, or no language for pseudocode).
- **Cross-reference the spec.** When a design element implements a
  specific spec behavior, mention which section of `XXX.md` it covers.
