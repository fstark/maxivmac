# Cleanup Plan

A practical sequence for cleaning up the codebase, building on the completed
Phases 1–5 from FULL_PLAN.md and informed by ISSUES.md.

The cadence throughout: **1 change → 2 regression runs (Plus, Mac II) →
1 commit**.  Nothing lands without passing.

---

## Step 1 — Non-Regression Test Harness ✅

> **Status: Complete.**  The `StateRecorder` class (`src/core/state_recorder.hpp`,
> `state_recorder.cpp`) implements record and verify modes with binary snapshots,
> rolling CRC32, ROM/disk hash validation, and field-by-field mismatch output.
> CLI flags `--record=`, `--verify=`, `--snapshot-interval=`, and
> `--max-instructions=` are all wired up in `config_loader.cpp`.
> Golden files use `<MODEL>.golden` naming where MODEL = ROM base name:
> `test/MacPlus.golden`, `test/MacII.golden`.  Both verified against `extras/disks/608.hfs`.
> The test runner is `test/verify.sh`; recording via `test/record.sh`.
> See `test/README.md` for details and `docs/TRACE.md` for the full design.

Everything else depends on this.  No refactoring is safe without a fast,
reliable way to detect regressions.  This step is itself broken into phases.

### 1a. Choose what to capture ✅

The current per-instruction text trace (`--log-start`/`--log-count`, dumped
to stderr) is useful for debugging but unsuitable for regression testing:
the files are huge, slow to generate, and require external diffing.

Instead, capture a **compact binary snapshot** at regular intervals.  Every
N instructions (e.g., every 100,000), record a fixed-size record:

```
struct Snapshot {             // 48 bytes, packed
    uint32_t instructionCount;
    uint32_t pc;
    uint16_t sr;
    uint16_t pad;
    uint32_t d[8];            // D0–D7
    uint32_t a7;              // SP — most useful address register
};
```

The exact fields are negotiable, but the principle is: capture enough to
detect divergence, not enough to reconstruct the full trace.  The snapshot
should be **deterministic** for a given (model, ROM, disk, N) tuple —
no wall-clock timestamps, no pointer values, nothing host-dependent.

At N = 100K, a 50 M-instruction boot produces ~500 records = ~24 KB.
Small enough to check into the repo alongside the ROM + disk combo.

**No I/O state needed.**  Any device I/O divergence will immediately land
in a register (the return value of the I/O instruction), so it will show
up in the next CPU-state snapshot.  This keeps the format simple.

### 1b. Golden file format ✅

The golden file starts with a **header** that identifies the configuration.
This makes it impossible to accidentally verify against the wrong golden:

```
struct GoldenHeader {         // fixed size, at offset 0
    uint32_t magic;           // e.g., 0x474F4C44 ("GOLD")
    uint32_t version;         // format version (1)
    uint32_t snapshotInterval;// N (instructions between snapshots)
    uint32_t maxInstructions; // instruction budget
    uint32_t snapshotCount;   // number of Snapshot records following
    uint32_t modelId;         // MacModel enum value
    uint8_t  romHash[16];    // MD5 of the ROM file
    uint8_t  diskHash[16];   // MD5 of the disk image (0 if no disk)
};
```

On `--verify`, the emulator **checks the header first**: if the running
model doesn't match `modelId`, or the loaded ROM's hash doesn't match
`romHash`, or the disk hash doesn't match `diskHash`, it refuses to run
and prints what's wrong.  This catches misconfigured test invocations
before they waste time on a meaningless diff.

On `--record`, the header is populated automatically from the running
configuration.

### 1c. Record mode ✅

Add a `--record=<file>` flag.  When set:

1. The emulator boots headless-ish (window can open, but no user input is
   needed — the disk auto-mounts, the ROM runs to steady state).
2. Write the `GoldenHeader`.
3. Every N instructions, append a `Snapshot` to the file.
4. After a fixed instruction budget (e.g., 50 M instructions, configurable
   via `--max-instructions=`), the emulator calls `exit(0)`.

The existing `g_InstructionCount` and the `std::exit(0)` at `g_LogEnd` in
`m68k.cpp` already prove this is feasible — the new code hooks into the
same spot but writes binary instead of text.

The golden file is small enough to load entirely into memory in verify
mode (~24 KB), so I/O is trivial.

### 1d. Verify mode ✅

Add a `--verify=<file>` flag.  When set:

1. Load the golden file into memory.  Validate the header against the
   current model/ROM/disk — abort with a clear message if anything
   doesn't match.
2. The emulator boots identically to record mode.
3. Every N instructions, compute the same `Snapshot` and compare it
   byte-for-byte against the next record in the golden file.
4. **On first mismatch:** print a human-readable textual diff of the
   expected vs. actual snapshot — all fields, named and formatted:

   ```
   MISMATCH at instruction 4200000 (snapshot #42):
     pc:  expected 0x00409E12  actual 0x00409E14
     sr:  expected 0x2700      actual 0x2704
     d0:  expected 0x00000000  actual 0x00000001
     ...
   ```

   Then exit with nonzero status.
5. If all records match and the instruction budget is reached: print
   `PASS` and `exit(0)`.

This gives a single exit code: 0 = no regression, nonzero = regression.
No temp files, no external diff, no manual inspection.  The textual
mismatch output gives enough context to start debugging immediately.

### 1e. Debugging a mismatch ✅

When a mismatch is found at snapshot K (instruction K×N), the workflow is:

1. Re-run with `--log-start=<(K-1)×N>` `--log-count=<2×N>` to get the
   full per-instruction text trace around the divergence point.
2. The existing text trace (PC, opcode, all D/A registers per instruction)
   is already detailed enough for debugging.

This gives you the best of both worlds: **binary for fast pass/fail**
(milliseconds, no disk I/O), **text for diagnosis** (only generated on
demand, only around the divergence point).  The binary golden file tells
you *where* to look; the text trace tells you *what happened*.

### 1f. Initial golden files: Plus and Mac II ✅

Record two golden files:

| Golden file | Model | ROM | Disk | Covers |
|-------------|-------|-----|------|--------|
| `test/MacPlus.golden` | MacPlus | `MacPlus.ROM` | `608.hfs` | 24-bit, VIA1-only, classic keyboard, IWM, compact screen |
| `test/MacII.golden` | MacII | `MacII.ROM` | `608.hfs` | 32-bit, VIA2, ADB, ASC, SCSI, NuBus video |

> Golden files are named `<MODEL>.golden` where MODEL = ROM base name
> (e.g. `MacPlus.golden` for `--model=MacPlus`, ROM `MacPlus.ROM`).
> The reference disk is `extras/disks/608.hfs` (copied
> fresh before each run by `test/verify.sh` since the emulator writes to it).

Together these hit every major `if (isIIFamily())` /
`if (model <= Plus)` branch.  The SE/Classic/PB100 paths are minor
variations — add them later when the code is stable.

### 1g. Test scripts ✅

Implemented as `test/verify.sh` (runs all `.golden` files) and
`test/record.sh` (re-records golden files for listed models).
See `test/README.md` for usage.

### 1h. Refinements (later, not blocking) — partially done

> **Note:** The rolling CRC32 refinement described below was implemented
> from the start (`ioCrc_` field in `StateRecorder`).

- **More models.**  Once the remaining models boot cleanly, add golden
  files for each.  The framework supports it trivially — it's just more
  `--record` invocations.
- **CI integration.**  The `test.sh` script is already CI-ready (single
  exit code).  Just add it to a GitHub Actions workflow.
- **Snapshot interval tuning.**  Start with N = 100K.  If golden files are
  too big or verification too slow, increase N.  If regressions slip
  through (divergence happens between snapshots and self-corrects), decrease
  N.
- **Inter-snapshot rolling hash.**  A CRC32 updated with (PC, SR, D0–D7)
  on *every* instruction, stored in the snapshot record (~4 bytes extra).
  This catches divergences that self-correct before the next checkpoint
  (e.g. an I/O read returns a wrong value but the register gets
  overwritten before the snapshot fires).  Not needed initially — the
  100K interval is tight enough for most regressions — but worth adding
  if false-pass bugs start slipping through.
- **Multiple disks per model.**  Different disks exercise different ROM
  code paths (HFS vs. MFS, different drivers).  Add more golden files as
  needed to increase coverage.

---

## Step 2 — Dead Code & Cosmetic Cleanup (not started)

Shrink the codebase before reorganizing it.  Pure deletions and mechanical
replacements — nothing touches logic.  See ISSUES.md "Easy Wins" #1–#8
for the full list.

Highlights: delete `src/unused/` (2,686 lines), strip all `#if 0` blocks
(~2,000 lines), `(void)` → `()` (1,745 sites), kill `LOCALIPROC`, replace
legacy macros (`Bit0`–`Bit7`, `Ui3rPowOf2`, `ui5r_From*`), remove dead
`#ifdef ChangeNtfy` blocks.

Also do the compiler-verified type-safety wins: `tMacErr` → `enum class`,
`MacModel` comparison operators, `kATTA_*` → `constexpr`, `MKC_*` keycodes
into their own header.  These make step 3 safer because the compiler
catches more mistakes when moving globals around.

---

## Step 3 — Globals Into Machine

Gather all global mutable state into the `Machine` object.  This is the
"god emperor" approach: centralize first, split later.

Move `g_wires`, `g_ict`, `RAM`, the SCC static state, the ASC channel
registers, and the RTC state into `Machine`.  Alias the old global names
to accessors during the transition so this can be done incrementally.

The one exception worth watching: the CPU `regstruct` in `m68k.cpp` (~100
fields) is performance-critical.  Profile before and after moving it behind
an indirection.  Everything else (device state, wire bus, scheduler) is
accessed orders of magnitude less often and is safe to centralize.

---

## Step 4 — Kill All `#define`s

Convert every remaining compile-time `#define` to either a constant, a
runtime check, or deletion.

The mechanical ones (constants, bit masks) are easy.  The scary ones are
those that change struct layouts: `SCC_TrackMore`, `EmLocalTalk`, `EmFPU`,
`WantCycByPriOp`.  Each gates fields in device structs or instruction
tables.  Converting them to runtime means structs get bigger (fields are
always present) and previously compiled-away code now runs behind
`if (!flag) return;` guards.  The golden-file harness catches any
divergence immediately.

This will further grow the `Machine` class (more config fields).  That's
fine — step 6 will address it.

---

## Step 5 — Frontend Simplification ✅

> **Status: Complete.** All backends except **SDL** have been removed.
> Dropped: Cocoa, Carbon, X11, GTK, Win32, DOS, NDS, Classic Mac — ~33K lines
> gone. The platform directory now contains only `sdl.cpp` and the shared
> code in `platform/common/`. The CMake build is SDL-only on all platforms.

---

## Step 6 — Split Machine

By this point all state lives in `Machine` and the natural seams are
visible.  Split it into focused subsystems: CPU, memory bus, device
registry, config.  The details will be obvious when we get here.

---

## Ground Rules

- **One concern per commit.**  Don't mix a rename with a logic change.
- **Two verify runs per commit.**  Plus + Mac II, both must pass.
- **Don't refactor `m68k.cpp` yet.**  It's the CPU hot path.  Splitting it
  needs benchmark validation.  Steps 1–4 only touch it for `LOCALIPROC`,
  `ui5r_From*`, and the snapshot hook.
- **Don't touch platform backends until step 5.**  *(Step 5 is now complete — only SDL remains.)*
- **If in doubt, delete.**  Git has the history.
