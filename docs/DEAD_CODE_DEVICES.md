# Dead Code — Device Files

Remaining dead code in `src/devices/`. For historical detail, see git log.

**Last updated:** 2026-05-31

---

## Safely Removable

### `sound.cpp` L98 — 1 `#if 0` block

Alternative silence implementation (`0x00` vs `kCenterSound`). Comment says
more accurate but causes audible clicks. Active path is correct; remove the
alternative.

---

### `adb_shared.h` L154 — 1 `#if dbglog_HAVE && 0` block

Explicitly disabled "Got a KeyDown" log. Dead even in debug builds.

---

## Convert or Delete (Logging)

### `scc.cpp` — `SCC_dolog` (~93 blocks)

`#define SCC_dolog 0` at L41. ~93 `#if SCC_dolog` blocks throughout the file
are always dead. Detailed SCC register tracing.

**Options:**
- **Convert:** change to `(dbglog_HAVE)` to activate in debug builds.
- **Delete:** remove all 93 blocks and the define.

---

### `asc.cpp` — `ASC_dolog` (~20 blocks)

`#define ASC_dolog 0` at L44. ~20 `#if ASC_dolog` and `#if ASC_dolog && 1`
blocks dead.

Same choice as `SCC_dolog`.

---

## Keep — Not-Yet-Enabled Features

### `scc.cpp` / `scc.h` / `rtc.cpp` — `EmLocalTalk` (~600 lines)

`EmLocalTalk = 0` in `CNFUDALL.h`. Complete LocalTalk/LLAP networking
implementation. Enable with `#define EmLocalTalk 1`. Do not remove.

---

### `scc.cpp` — `SCC_TrackMore` (~500 lines)

`#define SCC_TrackMore 0` at scc.cpp L42. Detailed SCC register field
tracking for baud rate, parity, CRC, modem control, etc. Scaffolding for
serial device support (ImageWriter, modem). Keep.
