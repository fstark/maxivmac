# Dead Code Audit — `src/`

Remaining disabled/dead code in `src/`. For historical detail, see git log.

**Last updated:** 2026-05-31

---

## `src/cpu/m68k.cpp` — `WantDumpTable` / `WantDumpAJump`

Both flags are `#define`d to 0. The guarded blocks are dead
instruction-dispatch profiling.

| Lines      | Condition           | Description |
|------------|---------------------|-------------|
| L225       | `#define WantDumpTable 0` | Remove define + all guarded blocks |
| L228–L230  | `#if WantDumpTable` | `DumpTable` array declaration |
| L687       | `#if WantDumpTable` | Increment counter in decode |
| L699       | `#if WantDumpTable` | Decrement counter in undecode |
| L4399      | `#define WantDumpAJump 0` | Remove define + guarded block |
| L4404–L4416 | `#if WantDumpAJump` | `DumpAJump` function |
| L8793–L8826 | `#if WantDumpTable` | Dump output at end of run |

---

## `src/devices/sound.cpp` L98 — 1 `#if 0`

Alternative silence fill (`0x00` vs `kCenterSound`). Active path is correct;
delete the alternative.

---

## `src/devices/adb_shared.h` L154 — 1 `#if dbglog_HAVE && 0`

Disabled "Got a KeyDown" log — dead even in debug builds.

---

## `src/devices/scc.cpp` — `SCC_dolog` (~93 blocks)

`#define SCC_dolog 0` (L41). ~93 `#if SCC_dolog` blocks are always dead.
Detailed SCC register tracing.

**Options:** Convert `SCC_dolog` to `dbglog_HAVE` (activate in debug), or
delete all blocks.

---

## `src/devices/asc.cpp` — `ASC_dolog` (~20 blocks)

`#define ASC_dolog 0` (L44). ~20 `#if ASC_dolog` / `#if ASC_dolog && 1`
blocks dead.

Same options as `SCC_dolog`.

---

## Keep — Not-Yet-Enabled Features

| Flag | Files | ~Lines | Purpose |
|------|-------|--------|---------|
| `EmLocalTalk` | `scc.cpp`, `scc.h`, `rtc.cpp` | ~600 | LocalTalk — complete feature, enable with `EmLocalTalk 1` |
| `SCC_TrackMore = 0` | `scc.cpp` | ~500 | SCC register tracking for serial device work |
| `WantAbnormalReports = 0` | `machine.h`, `machine.cpp`, `platform.h`, `osglu_common.cpp` | ~80 | Abnormal condition diagnostics |
