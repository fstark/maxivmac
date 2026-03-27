# Cleanup Plan

A practical sequence for cleaning up the codebase, informed by ISSUES.md.

The cadence throughout: **1 change → 6 regression runs
(Mac512Ke, MacPlus, MacSE, Classic, MacII, MacIIx) → 1 commit**.
Nothing lands without passing.

---

## Non-Regression Testing

The test harness is complete and **must be used for every commit**.
See `test/README.md` and `docs/TRACE.md` for full details.

- **Record:** `test/record.sh` — re-records `test/<MODEL>.golden` files.
- **Verify:** `test/verify.sh` — runs all 6 golden files
  (Mac512Ke, MacPlus, MacSE, Classic, MacII, MacIIx).
- PB100 and Mac128K are not yet bootable.

Future refinements (not blocking):
- CI integration (GitHub Actions).
- Snapshot interval tuning.
- Multiple disks per model for broader ROM path coverage.

---

## Step 2 — Dead Code & Cosmetic Cleanup (partially done)

Shrink the codebase before reorganizing it.  Pure deletions and mechanical
replacements — nothing touches logic.  See ISSUES.md "Easy Wins" #1–#8
for the full list.

### Done

- ✅ `MacModel` → `enum class` (defined in `src/core/machine_config.h`;
  12 models, `Twig43` through `IIx`).  Code still uses `static_cast<int>`
  workarounds for comparisons — proper comparison operators still needed.
- ✅ `ChangeNtfy` blocks modernized — converted from dead `#ifdef` guards
  into an active event-callback system (`MemOverlay_ChangeNtfy`,
  `Addr32_ChangeNtfy`, etc. in `machine.cpp`; wire-to-callback mappings
  in `CNFUDPIC.h`).  No longer dead code.
- ✅ `Ui3rPowOf2` macro removed (no longer present in codebase).
- ✅ `src/unused/` deleted (LTOVRUDP.h, LTOVRBPF.h, SGLUALSA.h, SGLUDDSP.h
  removed).
- ✅ `#if 0` blocks stripped — 196 blocks removed across 29 files
  (-1248 lines).  Valid `#if 0 !=` / `#if 0 ==` conditionals preserved.
- ✅ `(void)` → `()` — parameter lists cleaned across m68k.cpp,
  control_mode.cpp, and all macsrc files (188 sites).
- ✅ `LOCALIPROC` killed — expanded to `static void` in m68k.cpp (158 sites)
  and fpu_emdev.h (9 sites), `#define` deleted.
- ✅ `Bit0`–`Bit7` replaced with `(1 << N)` in scc.cpp, `#define`s deleted.
- ✅ `ui5r_FromSByte`/`ui5r_FromSWord`/`ui5r_FromSLong` replaced with
  inline `static_cast` chains; `#define`s deleted from `machine.h`.
- ✅ `tMacErr` → `enum class : uint16_t` with constexpr backward-compat
  aliases; `put_vm_word` call sites cast explicitly.
- ✅ `MacModel` comparison operators added; `static_cast<int>` workarounds
  removed from `rom.cpp`, `sony.cpp`, `machine.cpp`, etc.
- ✅ `kATTA_*` → `constexpr` — 8 macros in `machine.h` converted.

### Remaining

- ❌ `MKC_*` keycodes into own header — ~100 `#define`s still in
  `platform.h`, used by `sdl.cpp` and `CNFUDOSG.h`.

---

## Step 3 — Globals Into Machine (infrastructure done, migration incomplete)

Gather all global mutable state into the `Machine` object.  This is the
"god emperor" approach: centralize first, split later.

### Done

- ✅ `Machine` class exists (`src/core/machine_obj.h`) with `WireBus`,
  `ICTScheduler`, RAM/ROM/VidMem/VidROM as members.
- ✅ Device registry implemented — `std::vector<std::unique_ptr<Device>>`
  with `findDevice<T>()` template lookup.  VIA1, VIA2, SCC, ASC, RTC,
  ADB, Video, Sound, PMU, etc. all registered as Device subclasses.
- ✅ Device classes exist for all major subsystems: `SCCDevice`,
  `ASCDevice`, `RTCDevice`, `ADBDevice`, `VIA1Device`, `VIA2Device`, etc.

### Remaining — dual-ownership pattern

The old globals still coexist alongside `Machine` members:

- ❌ `g_wires` — global `WireBus` in `wire_bus.cpp` AND `Machine::wireBus_`.
- ❌ `g_ict` — global `ICTScheduler` in `main.cpp` AND `Machine::ict_`.
- ❌ `RAM` / `VidROM` / `VidMem` — global raw pointers in `machine.cpp`
  AND `Machine::ram_` (unique_ptr).  Macro accessors like `get_ram_byte`
  still use the global pointer.
- ❌ SCC state — still file-scope `static SCC_Ty SCC` in `scc.cpp`;
  `SCCDevice` is a thin wrapper.  Comment says: "due to heavy conditional
  compilation" (`#if EmLocalTalk`, `#if SCC_TrackMore`).
- ❌ ASC state — still file-scope statics (`ASC_SampBuff`, `ASC_ChanA`,
  FIFO buffers) in `asc.cpp`; `ASCDevice` is a thin wrapper.
- ❌ RTC state — still file-scope `static RTC_Ty RTC` in `rtc.cpp`;
  `RTCDevice` is a thin wrapper.
- ❌ CPU `regstruct` — still file-scope static in `m68k.cpp` (~100 fields);
  `CPU` class in `cpu.h`/`cpu.cpp` is a forwarding wrapper; global
  `g_cpu` in `cpu.cpp`.

The one exception worth watching: the CPU `regstruct` in `m68k.cpp` (~100
fields) is performance-critical.  Profile before and after moving it behind
an indirection.  Everything else (device state, wire bus, scheduler) is
accessed orders of magnitude less often and is safe to centralize.

---

## Step 4 — Kill All `#define`s (partially done)

Convert every remaining compile-time `#define` to either a constant, a
runtime check, or deletion.

### Done

- ✅ `EmFPU` — partially converted.  Always compiled to 1 now (all FPU
  code compiles in); runtime `MachineConfig::emFPU` bool controls 68000
  vs. 68020 FPU availability via dispatch table fixup in `M68KITAB_setup()`.

### Remaining

- ❌ `SCC_TrackMore` — still `#define 0` in `scc.cpp`; gates ~500 lines
  of register tracking.  (DEAD_CODE.md recommends keeping as scaffolding.)
- ❌ `EmLocalTalk` — still `#define 0` in `CNFUDALL.h`; gates ~600 lines
  of LocalTalk networking.  (DEAD_CODE.md: complete, valuable feature.)
- ❌ `WantCycByPriOp` — still `#define 1` in `CNFUDPIC.h`; gates
  cycle-counting dispatch in `m68k_tables.cpp` / `m68k.cpp`.
  (CYCLES.md notes it should be removable — both paths do a single
  table read.)
- ❌ ~200+ `#define`s remain in config headers (`CNFUDALL.h`, `CNFUDPIC.h`,
  `CNFUDOSG.h`, `CNFUIOSG.h`) — mostly backward-compat wire aliases,
  model-specific constants, and GUI config.

The mechanical ones (constants, bit masks) are easy.  The scary ones are
those that change struct layouts.  Converting them to runtime means structs
get bigger (fields are always present) and previously compiled-away code
now runs behind `if (!flag) return;` guards.  The golden-file harness
catches any divergence immediately.

This will further grow the `Machine` class (more config fields).  That's
fine — step 5 will address it.

---

## Step 5 — Split Machine

By this point all state lives in `Machine` and the natural seams are
visible.  Split it into focused subsystems: CPU, memory bus, device
registry, config.  The details will be obvious when we get here.

---

## Ground Rules

- **One concern per commit.**  Don't mix a rename with a logic change.
- **Six verify runs per commit.**  Mac512Ke, MacPlus, MacSE, Classic,
  MacII, MacIIx — all must pass.
- **Don't refactor `m68k.cpp` yet.**  It's the CPU hot path.  Splitting it
  needs benchmark validation.  Steps 2–4 only touch it for `LOCALIPROC`,
  `ui5r_From*`, and the snapshot hook.
- **If in doubt, delete.**  Git has the history.
