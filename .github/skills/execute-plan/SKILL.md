---
name: execute-plan
description: "Execute a *_PLAN.md file step by step with compile-and-test gates. Use when: running a migration plan, executing a refactoring plan, applying a phased rename plan, working through a TODO_PLAN or NAMING_PLAN. Handles build, test, git commit per phase, and prunes completed work from the plan."
argument-hint: "Path to a *_PLAN.md file, or enough info to fint it (ie: implementetion plan for FEATURE_X)"
---

# Execute Plan

Run a `*_PLAN.md` file phase by phase with automated build, test, and
commit gates. The agent works unattended after an initial clarification
round.

## When to Use

- A `*_PLAN.md` document exists with numbered phases/tasks
- Each phase is a set of mechanical code changes (renames, refactors, migrations)
- You want one git commit per phase with automatic testing

## Procedure

### Step 0 — Read and Clarify

1. Read the entire `*_PLAN.md` file.
2. Read any companion doc it references (e.g., `NAMING.md` for a naming plan).
3. Identify phases and their tasks.
4. If necessary, **Ask the user ONE round of questions**, for instance if the plan has ambiguities, references files that don't exist, or depends on external information. Or if the plan is too large and you suggest implementing some phases.
5. After the implemenation starts, **do not ask further questions**. Run to
   completion unattended.

### Step 1 — Execute Each Phase

For each phase (in order), repeat the following loop:

#### 1a. Announce

Log which phase and task number you are starting. Use the todo list
tool to track progress across phases.

#### 1b. Implement

Apply the code changes described in the task. Use search-and-replace,
multi-file edits, or terminal commands as appropriate. Follow these
rules:

- **Read before writing.** Always read files before modifying them.
- **Grep to find all sites.** Before renaming a symbol, `grep -rn` to
  find every occurrence. After renaming, grep again to confirm zero
  residual references.
- **Respect exclusions.** If the plan says "skip src/cpu/", do not
  touch files in that directory.
- **One phase = one logical unit.** Do not mix changes from different
  phases in the same commit.

#### 1c. Build

Run the project build command. If the build fails:

1. Read the error output.
2. Diagnose: usually a missed rename site or typo.
3. Fix the issue.
4. Rebuild. Repeat up to 3 times.
5. If still failing after 3 attempts, **revert the phase** (`git checkout -- .`),
   mark it as skipped with a reason, and move to the next phase.

#### 1d. Test

Run the test command (default: `cd test && ./verify.sh`).

If tests fail:

1. Read the test output.
2. Diagnose and fix.
3. Rebuild and retest. Repeat up to 3 times.
4. If still failing after 3 attempts, **revert the phase** (`git checkout -- .`),
   mark it as skipped with a reason, and move to the next phase.

#### 1e. Commit

```sh
git add -A
git commit -m "naming: phase N.M — <brief description>"
```

Use a descriptive commit message that identifies the plan phase and
summarizes what changed. Do not use `--no-verify`.

### Step 2 — Update the Plan

After all phases are attempted:

1. Read the `*_PLAN.md` file again.
2. **Remove all completed phases** — delete their sections entirely.
3. **Keep skipped/deferred phases** with a note explaining why they
   were skipped (e.g., "Skipped: build failed due to X", "Deferred:
   task definition was ambiguous").
4. If every phase was completed, replace the plan content with a short
   summary noting completion date and commit range.
5. `git add` and `git commit` the updated plan file.

### Step 3 — Summary

Report to the user:

- How many phases were completed
- How many were skipped (with reasons)
- The git commit range (first..last)

## Error Handling

| Situation | Action |
|-----------|--------|
| Build fails after 3 fix attempts | Revert phase, mark skipped, continue |
| Test fails after 3 fix attempts | Revert phase, mark skipped, continue |
| Plan task is ambiguous or incoherent | Skip with note, continue |
| Plan references files that don't exist | Skip task, note in plan |
| A phase depends on a skipped phase | Skip dependent phase too |

## Constraints

- **Do not ask questions after Step 0.** Run unattended.
- **Do not push.** Only local commits. The user pushes when ready.
- **Do not use `--force` or `--no-verify`.** Safety first.
- **Do not modify files outside the plan's scope.** If the plan says
  "skip src/cpu/", honor that even if grep finds matches there.
- **Preserve the test suite.** Never modify test golden files or test
  scripts to make tests pass.
