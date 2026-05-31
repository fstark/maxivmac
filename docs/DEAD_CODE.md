# Dead Code — maxivmac

Remaining disabled/dead code in `src/` that still needs action.

**Last updated:** 2026-05-31

---

## To Remove (Safe, No Behavioral Change)

### `src/cpu/m68k.cpp` — `WantDumpTable` / `WantDumpAJump` stubs

Both are `#define`d to 0 (L225, L4399). Four guarded blocks are dead
instruction-dispatch profiling code.

Lines: L228–L230 (array decl), L687 (increment), L699 (decrement),
L4404–L4416 (`DumpAJump` function), L8793–L8826 (dump output).

**Action:** Remove both `#define`s and all guarded blocks.

---

### `src/devices/sound.cpp` L98 — 1 `#if 0` block

Alternative silence: writes `0x00` instead of `kCenterSound`. Comment says
it is more accurate but causes clicks. The active path is correct.

---

### `src/devices/adb_shared.h` L154 — 1 `#if dbglog_HAVE && 0` block

Logs "Got a KeyDown" — explicitly disabled even in debug builds.

---

## To Convert or Delete (Logging)

### `src/devices/scc.cpp` — `SCC_dolog` (~93 blocks)

`#define SCC_dolog 0` (L41). ~93 `#if SCC_dolog` blocks are always dead.
Detailed SCC register trace logs.

- **Convert:** change to `(dbglog_HAVE)` so they activate in debug builds.
- **Delete:** remove all 93 blocks and the define.

---

### `src/devices/asc.cpp` — `ASC_dolog` (~20 blocks)

`#define ASC_dolog 0` (L44). ~20 `#if ASC_dolog` and `#if ASC_dolog && 1`
blocks are all dead.

Same choice as `SCC_dolog`.

---

## Keep (Not-Yet-Enabled Features)

These are intentionally disabled — do not remove.

| Flag | Files | ~Lines | Purpose |
|------|-------|--------|---------|
| `EmLocalTalk` | `scc.cpp`, `scc.h`, `rtc.cpp` | ~600 | LocalTalk networking — enable with `EmLocalTalk 1` |
| `SCC_TrackMore = 0` | `scc.cpp` | ~500 | Detailed SCC register tracking for serial device work |
| `WantAbnormalReports = 0` | `machine.h`, `machine.cpp`, `platform.h`, `osglu_common.cpp` | ~80 | Diagnostics for abnormal emulation conditions |
