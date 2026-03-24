# Detailled plan of the work on CPU Cycle Counting

## Overview

The 68000 CPU emulator has two levels of cycle-count accuracy, controlled by
two compile-time flags in `src/config/CNFUDPIC.h`:

| Flag | Current | Purpose |
|------|:-------:|---------|
| `WantCycByPriOp` | `1` | Per-opcode base cost in the 64K dispatch table |
| `WantCloserCyc`  | `1` | Runtime cycle adjustments inside instruction handlers |

Both are currently `#define`s in `src/config/CNFUDPIC.h`.

**`WantCycByPriOp` should be removed entirely** — it has zero runtime cost
(just a different value baked into a table at startup) and disabling it only
makes timing worse by replacing per-opcode costs with a flat average of 680.
The per-opcode path should be the only path; all `#if WantCycByPriOp` guards
and the disabled-case code should be deleted.

**`WantCloserCyc` should be converted to a runtime flag**, disabled by default,
and stored in golden files for reproducible comparisons.

### Rationale

Classic Mac software has no cycle-precise timing dependencies. There is no
Wozniak-style floppy controller, no Amiga-like copper/scanline following, and
Mac software is designed to run across 8 MHz to 40 MHz machines. The emulator
already intercepts low-level disk I/O (replacing the `.Sony` driver) and does
not emulate SCSI at the hardware level. Making `WantCloserCyc` a runtime option:

- Simplifies the codebase (`#if` → `if`)
- Lets golden files record which mode was used
- Allows quick toggling for comparison-testing without recompilation
- Default off = faster inner loop for normal use


## How Cycle Counting Works

### Level 1: `WantCycByPriOp` (remove the `#define`, keep the code)

At startup, `M68KITAB_setup()` pre-builds a 65536-entry dispatch table. Each
entry stores a `Cycles` field computed from the opcode's addressing mode,
operand size, and instruction type (e.g. `MOVE.L (A0),D1` gets
`12*kCycleScale + 3*RdAvgXtraCyc`).

At runtime, `DecodeNextInstruction()` does a single table lookup and subtracts
the pre-computed cost from `V_MaxCyclesToGo`. The disabled path (flat average
of 680 for every opcode) is strictly worse and has no performance benefit —
both paths do a single table read. The `#define` and all `#if WantCycByPriOp`
guards should be removed, keeping only the per-opcode path.

### Level 2: `WantCloserCyc` (convert to runtime)

Some instructions have costs that depend on **runtime state** — the table
can't know at build time whether a branch will be taken, how many registers
are in a MOVEM mask, or how many bits are set in a MUL operand.

When `WantCloserCyc` is enabled:
- The table sets `Cycles = 0` for such instructions (deferring to the handler)
- Handlers subtract the exact cost via `V_MaxCyclesToGo -= <expression>`
- A `CurDecOp` pointer is stored so handlers can *refund* a pre-charged cost

When disabled:
- The table bakes in a reasonable average/worst-case estimate
- Handlers do no extra cycle work
- Timing drifts from reality but still keeps the emulator synchronized


## Detailed Change Plan

### 1. Add runtime flag

**File: `src/cpu/m68k.cpp`**
- Add `static bool s_closerCyc = false;` next to `s_cpuConfig` (line ~257)
- Set it from the config in `MINEM68K_Init()` (line ~8950)

**File: `src/core/machine_config.h`** (or wherever `MachineConfig` is defined)
- Add `bool closerCyc = false;` field

### 2. Add CLI flag

**File: `src/core/config_loader.h`**
- Add `bool closerCyc = false;` to `LaunchConfig`

**File: `src/core/config_loader.cpp`**
- Parse `--closer-cyc` flag (boolean, no value needed)

### 3. Store in golden file

**File: `src/core/state_recorder.hpp`**
- Add `bool closerCyc = false;` to both `Config` and `HeaderInfo`

**File: `src/core/state_recorder.cpp`**
- Use `reserved1a` (byte offset 73) in `GoldenHeader` to store the flag
- Bump `kGoldenVersion` from 3 to 4
- Write/read the flag during record/verify
- On verify: check mismatch (warn + abort, like `speedValue`)
- On verify: apply golden's `closerCyc` value to override runtime default

**File: `src/core/main.cpp`**
- When verifying, read `closerCyc` from golden header and apply it
- When recording, store current `closerCyc` in recorder config

### 4. Convert m68k_tables.cpp (11 sites)

Replace `#if WantCloserCyc` / `#if ! WantCloserCyc` with runtime checks.
The table builder already receives `const MachineConfig *config`, so it can
read `config->closerCyc`.

| Lines | Function | Category | Change |
|-------|----------|----------|--------|
| 1151 | `MoveAvgN` macro | MOVEM average | Replace with `const int MoveAvgN = config->closerCyc ? 0 : 3;` |
| 1928 | `DeCode5` (DBcc) | Cycles=0 | `if (closerCyc) p->Cycles = 0; else p->Cycles = 11*...;` |
| 1962 | `DeCode5` (Scc reg) | Scc base | `if (closerCyc) p->Cycles = 4*...; else p->Cycles = 5*...;` |
| 2089 | `DeCode6` (BccW) | Cycles=0 | `if (closerCyc) p->Cycles = 0; else p->Cycles = 11*...;` |
| 2105 | `DeCode6` (BccB) | Cycles=0 | `if (closerCyc) p->Cycles = 0; else p->Cycles = 9*...;` |
| 2148 | `DeCode8` (DivU) | DIV cost | `if (!closerCyc) p->Cycles += 133*kCycleScale;` |
| 2162 | `DeCode8` (DivS) | DIV cost | `if (!closerCyc) p->Cycles += 150*kCycleScale;` |
| 2562 | `DeCodeC` (MUL) | MUL min | `if (closerCyc) p->Cycles = 38*...; else 54*...;` |
| 2869 | `DeCodeE` (shift mem) | Shift +2 | `if (!closerCyc) p->Cycles += 2*kCycleScale;` |
| 2890 | `DeCodeE` (shift imm) | Shift imm | `if (!closerCyc) p->Cycles += octdat(rg9(p))*2*kCycleScale;` |
| 2906 | `DeCodeE` (shift reg) | Shift reg | `if (closerCyc) base only; else base + avg*2*...;` |

### 5. Convert m68k.cpp (54 sites)

Replace `#if WantCloserCyc` with `if (s_closerCyc)`. Group by category:

#### A. CurDecOp pointer (2 sites)

| Line | Change |
|------|--------|
| 165 | Keep `CurDecOp` field unconditionally (tiny — one pointer) |
| 780 | `if (s_closerCyc) V_regs.CurDecOp = p;` |

Rationale: the field is 8 bytes in a large struct; always having it avoids
layout changes. The store is the only runtime cost.

#### B. Branch taken/not-taken — Bcc, DBcc (7 sites)

| Lines | Function | Expression |
|-------|----------|------------|
| 4105 | `DoCodeBccB_t` | `10*kCycleScale + 2*RdAvgXtraCyc` |
| 4117 | `DoCodeBccB_f` | `8*kCycleScale + RdAvgXtraCyc` |
| 4140 | `DoCodeBccW_t` | `10*kCycleScale + 2*RdAvgXtraCyc` |
| 4150 | `DoCodeBccW_f` | `12*kCycleScale + 2*RdAvgXtraCyc` |
| 4183 | `DoCodeDBF` (expired) | `14*kCycleScale + 3*RdAvgXtraCyc` |
| 4188 | `DoCodeDBF` (taken) | `10*kCycleScale + 2*RdAvgXtraCyc` |
| 4195 | `DoCodeDBcc_t` | `12*kCycleScale + 2*RdAvgXtraCyc` |

Note: When `WantCloserCyc` is off, `DoCodeBccB_t` is `#define`d to
`DoCodeBraB` (no wrapper). With the runtime flag, always use the wrapper
and make the subtraction conditional: `if (s_closerCyc) V_MaxCyclesToGo -= ...;`

#### C. Shift/Rotate count (24 sites)

All use `V_MaxCyclesToGo -= (cnt * 2 * kCycleScale)` where `cnt` is
`V_regs.SrcVal & 63`. Functions: `DoCodeAslB/W/L`, `DoCodeAsrB/W/L`,
`DoCodeLslB/W/L`, `DoCodeLsrB/W/L`, `DoCodeRxlB/W/L`, `DoCodeRxrB/W/L`,
`DoCodeRolB/W/L`, `DoCodeRorB/W/L`.

Lines: 4945, 4981, 5017, 5089, 5121, 5153, 5198, 5233, 5268, 5300, 5329,
5358, 5408, 5435, 5462, 5489, 5517, 5545, 5570, 5602, 5634, 5666, 5699, 5732.

Change: wrap each in `if (s_closerCyc)`.

#### D. MOVEM register count (8 sites, inside loops)

| Line | Function | Expression (per register) |
|------|----------|----|
| 6387 | `DoCodeMOVEMApRW` | `4*kCycleScale + RdAvgXtraCyc` |
| 6418 | `DoCodeMOVEMRmMW` | `4*kCycleScale + WrAvgXtraCyc` |
| 6439 | `DoCodeMOVEMrmW` | `4*kCycleScale + WrAvgXtraCyc` |
| 6458 | `DoCodeMOVEMrmL` | `8*kCycleScale + 2*WrAvgXtraCyc` |
| 6477 | `DoCodeMOVEMmrW` | `4*kCycleScale + RdAvgXtraCyc` |
| 6496 | `DoCodeMOVEMmrL` | `8*kCycleScale + 2*RdAvgXtraCyc` |
| 4705 | `DoCodeMOVEMRmML` | `8*kCycleScale + 2*WrAvgXtraCyc` |
| 4728 | `DoCodeMOVEMApRL` | `8*kCycleScale + 2*RdAvgXtraCyc` |

Change: wrap each in `if (s_closerCyc)`.

#### E. MUL bit-pattern (2 sites)

| Line | Function | Expression |
|------|----------|------------|
| 6122 | `DoCodeMulU` | `2*kCycleScale` per set bit in srcvalue |
| 6153 | `DoCodeMulS` | `2*kCycleScale` per transition in `srcvalue << 1` |

#### F. DIV fixed cost (4 sites)

| Line | Function | Expression |
|------|----------|------------|
| 6183 | `DoCodeDivU` (÷0) | `38*kCycleScale + 3*RdAvgXtraCyc + 3*WrAvgXtraCyc` |
| 6194 | `DoCodeDivU` (normal) | `133*kCycleScale` |
| 6226 | `DoCodeDivS` (÷0) | `38*kCycleScale + 3*RdAvgXtraCyc + 3*WrAvgXtraCyc` |
| 6237 | `DoCodeDivS` (normal) | `150*kCycleScale` |

#### G. Scc true to register (1 site)

| Line | Function | Expression |
|------|----------|------------|
| 5948 | `DoCodeScc_t` | `2*kCycleScale` if dest is register (`kAMdRegB`) |

#### H. Privilege violation / exception refund (2 sites)

| Line | Function | Expression |
|------|----------|------------|
| 6289 | `DoPrivilegeViolation` | Refund `+= GetDcoCycles(CurDecOp)`, then charge `34*kCycleScale + 4*RdAvgXtraCyc + 3*WrAvgXtraCyc` |
| 6813 | `DoCodeTrapV` (overflow) | Same refund + charge pattern |

#### I. CHK exception (2 sites)

| Line | Function | Expression |
|------|----------|------------|
| 6783 | `DoCodeChk` (N=1) | `30*kCycleScale + 3*RdAvgXtraCyc + 3*WrAvgXtraCyc` |
| 6792 | `DoCodeChk` (N=0) | same |

#### J. Interrupt and trace (2 sites)

| Line | Function | Expression |
|------|----------|------------|
| 8672 | `DoCheckExternalInterruptPending` | `44*kCycleScale + 5*RdAvgXtraCyc + 3*WrAvgXtraCyc` |
| 8699 | `m68k_go_nCycles` (trace) | `34*kCycleScale + 4*RdAvgXtraCyc + 3*WrAvgXtraCyc` |


## Summary

| Area | Sites | Effort |
|------|:-----:|--------|
| m68k_tables.cpp | 11 | Mechanical: `#if` → `if (closerCyc)` on local bool |
| m68k.cpp (handlers) | 54 | Mechanical: `#if` → `if (s_closerCyc)` |
| Golden file header | 1 field | Use `reserved1a` byte, bump version to 4 |
| CLI parsing | 1 flag | `--closer-cyc` in config_loader |
| LaunchConfig / MachineConfig | 1 field each | `bool closerCyc` |
| state_recorder | ~10 lines | Record, verify, apply from golden |
| main.cpp | ~3 lines | Wire CLI → config → recorder |
| CNFUDPIC.h | remove both | Drop `#define WantCycByPriOp 1` and `#define WantCloserCyc 1` |
| m68k_tables.cpp | delete dead code | Remove `#if WantCycByPriOp` guards and the disabled-case flat-average fallback |
| m68k.cpp | delete dead code | Remove `#if WantCycByPriOp` guards around `DeCodeOneOp` |
| Golden re-record | all 6 models | With `closerCyc=false` (new default) |

Total: ~65 `#if WantCloserCyc` → `if` conversions + delete all
`#if WantCycByPriOp` guards (keep only the enabled path) + ~20 lines of
plumbing. No algorithmic changes.
