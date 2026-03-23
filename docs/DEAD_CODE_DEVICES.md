# Dead Code Audit — Device Files

**Generated:** 2026-03-23  
**Scope:** All device files in `src/devices/` except `scc.cpp` (audited separately)

---

## Config Flags Summary

Key always-false/always-true flags affecting dead code in device files:

| Flag | Value | Source | Effect |
|------|-------|--------|--------|
| `EmLocalTalk` | **0** | `cfg/CNFUDALL.h` | All `#if EmLocalTalk` blocks dead |
| `ExtraAbnormalReports` | **0** | `src/config/CNFUDPIC.h` | All `#if ExtraAbnormalReports` blocks dead |
| `SCC_TrackMore` | **0** | `scc.cpp:54` | (scc only — excluded from this audit) |
| `WantAbnormalReports` | **0** | `cfg/CNFUDALL.h` | ReportAbnormal calls may be no-ops |
| `dbglog_HAVE` | **0** (release) / **1** (debug) | `cfg/CNFUDALL.h` / `src/config/` | `dbglog_HAVE && 0` always dead; standalone `#if dbglog_HAVE` alive in debug |
| `_VIA_Debug` | **never defined** | nowhere | All `#ifdef _VIA_Debug` blocks dead |
| `CurAltHappyMac` | **never defined** | nowhere | `hpmac_hack.h` never included |
| `ln2mtb` | **never defined** | nowhere | MTB scramble code dead |
| `UseLargeScreenHack` | **0** | `rom.cpp:38` | `screen_hack.h` never included |
| `MySoundRecenterSilence` | **0** | `cfg/CNFUDALL.h` | Always false in conditionals |

---

## Overview

| File | Lines | Dead Code Blocks | Category Summary |
|------|-------|-----------------|------------------|
| `video.cpp` | 994 | 9 | SAFELY_REMOVABLE, ALTERNATIVE_IMPL, NOT_YET_ENABLED |
| `asc.cpp` | 871 | 13 | SAFELY_REMOVABLE, DEBUG_LOGGING, ALTERNATIVE_IMPL |
| `sony.cpp` | 1,708 | 7 | SAFELY_REMOVABLE, ALTERNATIVE_IMPL |
| `via.cpp` | 818 | 3 | DEBUG_LOGGING, SAFELY_REMOVABLE |
| `via2.cpp` | 820 | 2 | SAFELY_REMOVABLE, DEBUG_LOGGING |
| `rtc.cpp` | 515 | 3 | NOT_YET_ENABLED, DEBUG_LOGGING |
| `pmu.cpp` | 438 | 3 | SAFELY_REMOVABLE, DEBUG_LOGGING |
| `sound.cpp` | 228 | 2 | DEBUG_LOGGING, ALTERNATIVE_IMPL |
| `scsi.cpp` | 150 | 1 | SAFELY_REMOVABLE |
| `rom.cpp` | 334 | 3 | NOT_YET_ENABLED, OTHER |
| `screen_hack.h` | 404 | 1 | NOT_YET_ENABLED |
| `keyboard.cpp` | 211 | 5 | DEBUG_LOGGING |
| `adb.cpp` | 214 | 8 | DEBUG_LOGGING |
| `adb_shared.h` | 287 | 1 | DEBUG_LOGGING |
| `hpmac_hack.h` | 256 | 0 (entire file unused) | NOT_YET_ENABLED |
| `iwm.cpp` | 215 | 1 | DEBUG_LOGGING |
| `mouse.cpp` | 139 | 0 | — |
| `device.cpp` | 16 | 0 | — |

**Header files with NO dead code:** `adb.h`, `asc.h`, `device.h`, `iwm.h`, `keyboard.h`, `mouse.h`, `pmu.h`, `rom.h`, `rtc.h`, `screen.h`, `scsi.h`, `sony.h`, `sound.h`, `via.h`, `via2.h`, `video.h`

---

## Per-File Details

---

### `src/devices/video.cpp` (994 lines)

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 1 | L121-L125 | `#if 0` | `PatchAOSLstEntry0()` — unused Slot ROM patching helper function | SAFELY_REMOVABLE |
| 2 | L176-L178 | `#if 0` | `pTo_MajorBase`, `pTo_MajorLength` local variable declarations | SAFELY_REMOVABLE |
| 3 | L264-L267 | `#if 0` | `pTo_MajorBase`, `pTo_MajorLength` — reserve patch entries for MajorBase/MajorLength | SAFELY_REMOVABLE |
| 4 | L302-L308 | `#if 0` | Patch MajorBase/MajorLength into Slot ROM structure (unused resources) | SAFELY_REMOVABLE |
| 5 | L706-L710 | `#if 0` | SetGamma: `csTable` read but unused; not implemented stub | NOT_YET_ENABLED |
| 6 | L712-L716 | `#if 0` | SetGamma: suppress ReportAbnormal, return noErr instead | ALTERNATIVE_IMPL |
| 7 | L726-L730 | `#if 0` | GrayScreen: `csPage` read but unused; not implemented stub | NOT_YET_ENABLED |
| 8 | L817-L826 | `#if 0` | GetEntries: `csTable/csStart/csCount` read but unused; not implemented | NOT_YET_ENABLED |
| 9 | L903-L914 | `#if 0` | GetCurrentMode: read mode/data/page/baseAddr but not implemented | NOT_YET_ENABLED |

**Additional:** `VID_dolog` (L47) = `dbglog_HAVE && 0` — all `#if VID_dolog` blocks are debug logging, dead in release and debug builds. Multiple instances throughout the file (L700+). Two `#if dbglog_HAVE` blocks at L780, L979 are live in debug builds only.

---

### `src/devices/asc.cpp` (871 lines)

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 1 | L107-L110 | `#if 0` | Channel A overflow ReportAbnormal (happens in Tetris, so suppressed) | SAFELY_REMOVABLE |
| 2 | L124-L126 | `#if 0` | Set half-empty flag A when below 0x200 (not necessary) | ALTERNATIVE_IMPL |
| 3 | L165-L167 | `#if 0` | Set half-empty flag B when below 0x200 (not necessary) | ALTERNATIVE_IMPL |
| 4 | L198-L205 | `#if 0` | `ASC_Access SampBuff` debug filter — suppress middle-range address logs | DEBUG_LOGGING |
| 5 | L247 | `#if 1` | CONTROL path: always active — not dead code (just wrapped in `#if 1`) | OTHER |
| 6 | L263-L265 | `#if 0` | Suppress ReportAbnormal for "changing CONTROL while ENABLEd" | SAFELY_REMOVABLE |
| 7 | L297-L299 | `#if 0` | Suppress ReportAbnormal for FIFO clear when not in FIFO mode (System 6) | SAFELY_REMOVABLE |
| 8 | L322-L326 | `#if 0` | Suppress ReportAbnormal for "set FIFO IRQ STATUS when not 0" | SAFELY_REMOVABLE |
| 9 | L345-L349 | `#if 0` | Suppress ReportAbnormal for "read STATUS when not FIFO" | SAFELY_REMOVABLE |
| 10 | L361-L363 | `#if 0` | Filter log output to only non-zero status reads | DEBUG_LOGGING |
| 11 | L730 | `#if 1` | Wavetable synth phase rounding (always active, `#else` at L737 is the dead alternative) | ALTERNATIVE_IMPL |
| 12 | L803 | `#if 1` | FIFO half/full flag recalculation after sound output (always active) | OTHER |

**Additional:** `ASC_dolog` (L60) = `dbglog_HAVE && 0` — all `#if ASC_dolog` blocks dead. Multiple `#if ASC_dolog && 1` blocks also dead since `ASC_dolog` = 0.

---

### `src/devices/sony.cpp` (1,708 lines)

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 1 | L415-L428 | `#if 0` | Boot block validation code (don't look at boot blocks — skipped) | SAFELY_REMOVABLE |
| 2 | L946-L960 | `#if 0` | `MyDriverDat_R` struct definition — unused driver data structure | SAFELY_REMOVABLE |
| 3 | L1197-L1252 | `#if 0` | Full positioning mode handling (`PosMode`, `PosOffset` switch) — Apple TN FL24 says Device Manager handles this | SAFELY_REMOVABLE |
| 4 | L1230-L1237 | `#if 0` (nested) | `kfsFromLEOF` case — not valid for device drivers | SAFELY_REMOVABLE |
| 5 | L1336-L1351 | `#if 0` | `kTrackCacheControl` — track cache enable/disable/install/remove logic | NOT_YET_ENABLED |
| 6 | L1390-L1393 | `#if 0` | `kEjectDisk`: clear TwoSideFmt on eject (commented out) | SAFELY_REMOVABLE |
| 7 | L1542-L1546 | `#if 0` | `Sony_Close`: reading ParamBlk/DeviceCtl (unused, function returns closErr) | SAFELY_REMOVABLE |

**Additional:** `Sony_dolog` (L994) = `dbglog_HAVE && 0` — all `#if Sony_dolog` blocks dead. Multiple `#if ExtraAbnormalReports` blocks (L1201, L1272, L1470, L1523) dead since `ExtraAbnormalReports` = 0.

---

### `src/devices/via.cpp` (818 lines)

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 1 | L81 | `#define VIA1_dolog (dbglog_HAVE && 0)` | All `#if VIA1_dolog` blocks are dead debug logging | DEBUG_LOGGING |
| 2 | L252-L258 | `#if ExtraAbnormalReports` | Extra shift-in abnormal report (ExtraAbnormalReports=0) | DEBUG_LOGGING |
| 3 | L609-L611 | `#if 0` | `setInterruptFlag(kIntSR)` after shift register operation — possibly should do this | ALTERNATIVE_IMPL |

**Additional:** 6x `#ifdef _VIA_Debug` blocks (L243, L338, L408, L586, L602) — all dead, `_VIA_Debug` never defined. ~15 lines total.

---

### `src/devices/via2.cpp` (820 lines)

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 1 | L81 | `#define VIA2_dolog (dbglog_HAVE && 0)` | All `#if VIA2_dolog` blocks are dead debug logging | DEBUG_LOGGING |
| 2 | L609-L611 | `#if 0` | `setInterruptFlag(kIntSR)` — possibly should do this, seems not to affect anything | ALTERNATIVE_IMPL |

**Additional:** `#if ExtraAbnormalReports` at L252 — dead. `#ifdef _VIA_Debug` at L338, L408, L586, L602 — all dead. Identical pattern to `via.cpp`.

---

### `src/devices/rtc.cpp` (515 lines)

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 1 | L166-L168 | `#if EmLocalTalk` | Set LocalTalk node hint in PRAM (EmLocalTalk=0) | NOT_YET_ENABLED |
| 2 | L176-L179 | `#if EmLocalTalk` | Set serial port config for AppleTalk (EmLocalTalk=0) | NOT_YET_ENABLED |
| 3 | L266-L289 | `#if 0 != pr_HilCol*` (6 blocks) | Highlight color PRAM init — values evaluate at compile time, not `#if 0` dead code per se; these are active when the color constants are nonzero | OTHER |

**Additional:** `#if dbglog_HAVE` blocks at L122-L137 (`DumpRTC` function) and L300, L493 — live in debug builds only; dead in release. The `EmLocalTalk` check at L195 (`if (... || EmLocalTalk)`) is a runtime check that's always-false for the `EmLocalTalk` part but the `isIIFamily()` part makes it live code.

---

### `src/devices/pmu.cpp` (438 lines)

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 1 | L98-L101 | `#if 0` | `kPMUdownloadStatus` (0xE2) case — empty, commented out | SAFELY_REMOVABLE |
| 2 | L215-L223 | `#if 0` | Zero-fill `buffA_` array before `kPMUpramRead` response (unnecessary) | SAFELY_REMOVABLE |
| 3 | L239-L258 | `#if dbglog_HAVE` | Debug dump of unknown PMU op and buffer contents | DEBUG_LOGGING |

---

### `src/devices/sound.cpp` (228 lines)

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 1 | L133-L140 | `#if dbglog_HAVE && 0` | Log sound buffer read range — explicitly disabled even in debug | DEBUG_LOGGING |
| 2 | L147-L150 | `#if 0` | Write 0x00 (silence) instead of kCenterSound — alternative impl believed more accurate but causes clicks | ALTERNATIVE_IMPL |

---

### `src/devices/scsi.cpp` (150 lines)

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 1 | L81-L84 | `#if 0` | Init two extra SCSI registers (`sODR+dackWr`, `sIDR+dackRd`) | SAFELY_REMOVABLE |

---

### `src/devices/rom.cpp` (334 lines)

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 1 | L38 | `UseLargeScreenHack` = 0 | `screen_hack.h` never `#include`d (L224-L230) | NOT_YET_ENABLED |
| 2 | L244 | `#ifdef CurAltHappyMac` | `hpmac_hack.h` never `#include`d (never defined) | NOT_YET_ENABLED |
| 3 | L248-L268 | `#ifdef ln2mtb` | `ROMscrambleForMTB()` function — never compiled (ln2mtb never defined) | NOT_YET_ENABLED |

Note: `DisableRomCheck` (L237) = 1 and `DisableRamTest` (L241) = 1 — these are **always-true**, meaning the rom check skip and ram test skip code is **always active**. The code inside `#if DisableRomCheck`/`#if DisableRamTest` is live, not dead.

---

### `src/devices/screen_hack.h` (404 lines)

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 1 | L266-L291 | `#if 0` | Unfinished screen position patches for disk alert and DSAlertRect — contains `?` placeholder values | NOT_YET_ENABLED |

Note: **The entire file is dead code** because it is only `#include`d within `#if UseLargeScreenHack` which is 0. Even within the file, there is an additional `#if 0` block.

---

### `src/devices/keyboard.cpp` (211 lines)

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 1 | L35 | `#ifdef _VIA_Debug` | Include `<stdio.h>` (never defined) | DEBUG_LOGGING |
| 2 | L151 | `#ifdef _VIA_Debug` | Log `enter DoKybd_ReceiveEndCommand` | DEBUG_LOGGING |
| 3 | L155 | `#ifdef _VIA_Debug` | Log `HaveKeyBoardResult` value | DEBUG_LOGGING |
| 4 | L171 | `#ifdef _VIA_Debug` | Log `posting kICT_Kybd_ReceiveCommand` | DEBUG_LOGGING |
| 5 | L185 | `#ifdef _VIA_Debug` | Log `posting kICT_Kybd_ReceiveEndCommand` | DEBUG_LOGGING |

All ~12 dead lines; `_VIA_Debug` is never defined anywhere.

---

### `src/devices/adb.cpp` (214 lines)

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 1 | L32 | `#ifdef _VIA_Debug` | Include `<stdio.h>` | DEBUG_LOGGING |
| 2 | L48 | `#ifdef _VIA_Debug` | Log `ADB_DoNewState` | DEBUG_LOGGING |
| 3 | L64 | `#ifdef _VIA_Debug` | Log new command | DEBUG_LOGGING |
| 4 | L89 | `#ifdef _VIA_Debug` | Log talk state | DEBUG_LOGGING |
| 5 | L114 | `#ifdef _VIA_Debug` | Log talk one | DEBUG_LOGGING |
| 6 | L127 | `#ifdef _VIA_Debug` | Log listen one | DEBUG_LOGGING |
| 7 | L161 | `#ifdef _VIA_Debug` | Log `ADBstate_ChangeNtfy` | DEBUG_LOGGING |
| 8 | L183 | `#ifdef _VIA_Debug` | Log `ADB_DataLineChngNtfy` | DEBUG_LOGGING |

All ~20 dead lines; `_VIA_Debug` never defined.

---

### `src/devices/adb_shared.h` (287 lines)

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 1 | L146-L152 | `#if dbglog_HAVE && 0` | Log "Got a KeyDown" — explicitly disabled even in debug | DEBUG_LOGGING |

---

### `src/devices/iwm.cpp` (215 lines)

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 1 | L39 | `#define IWM_dolog (dbglog_HAVE && 0)` | Debug logging define — always 0. No `#if IWM_dolog` blocks found in file so no actual dead blocks, just unused define | DEBUG_LOGGING |

---

### `src/devices/hpmac_hack.h` (256 lines)

**Entire file is dead code.** Only `#include`d under `#ifdef CurAltHappyMac` in `rom.cpp` (L244), and `CurAltHappyMac` is never defined. Contains 15 alternative Happy Mac icon bitmaps selectable by compile-time constant. Category: **NOT_YET_ENABLED**.

---

### `src/devices/mouse.cpp` (139 lines) — **NO DEAD CODE**

Only conditional is `#if EnableMouseMotion` (L80) which is **1** — live code.

---

### `src/devices/device.cpp` (16 lines) — **NO DEAD CODE**

No preprocessor conditionals.

---

## Dead Code by Category

### SAFELY_REMOVABLE (can delete with no behavioral change)

| File | Lines | Description |
|------|-------|-------------|
| `video.cpp` | L121-L125 | Unused `PatchAOSLstEntry0()` function |
| `video.cpp` | L176-L178, L264-L267, L302-L308 | MajorBase/MajorLength Slot ROM resources (unused) |
| `asc.cpp` | L107-L110 | Channel A overflow report (Tetris triggers it) |
| `asc.cpp` | L263-L265 | Suppress "changing CONTROL while ENABLEd" |
| `asc.cpp` | L297-L299 | Suppress "clear FIFO when not FIFO mode" |
| `asc.cpp` | L322-L326 | Suppress "set FIFO IRQ STATUS when not 0" |
| `asc.cpp` | L345-L349 | Suppress "read STATUS when not FIFO" |
| `sony.cpp` | L415-L428 | Boot block validation (skipped) |
| `sony.cpp` | L946-L960 | `MyDriverDat_R` struct (unused) |
| `sony.cpp` | L1197-L1252 | PosMode handling (Device Manager does this) |
| `sony.cpp` | L1390-L1393 | Clear TwoSideFmt on eject |
| `sony.cpp` | L1542-L1546 | Read ParamBlk/DeviceCtl in Close (unused) |
| `pmu.cpp` | L98-L101 | `kPMUdownloadStatus` empty case |
| `pmu.cpp` | L215-L223 | Zero-fill buffA before pramRead |
| `scsi.cpp` | L81-L84 | Extra SCSI register init |

### NOT_YET_ENABLED (features intended but not yet working)

| File | Lines | Description |
|------|-------|-------------|
| `video.cpp` | L706-L710 | SetGamma (not implemented) |
| `video.cpp` | L726-L730 | GrayScreen csPage (not implemented) |
| `video.cpp` | L817-L826 | GetEntries (not implemented) |
| `video.cpp` | L903-L914 | GetCurrentMode (not implemented) |
| `rtc.cpp` | L166-L168, L176-L179 | LocalTalk PRAM init (EmLocalTalk=0) |
| `rom.cpp` | L224-L230 | Large screen hack (UseLargeScreenHack=0) |
| `rom.cpp` | L244-L246 | Alt Happy Mac icons (CurAltHappyMac undef) |
| `rom.cpp` | L248-L268 | MTB ROM scramble (ln2mtb undef) |
| `screen_hack.h` | L266-L291 | Alert rect positioning (unfinished, has `?` values) |
| `screen_hack.h` | entire file | Never included (UseLargeScreenHack=0) |
| `hpmac_hack.h` | entire file | Never included (CurAltHappyMac undef) |
| `sony.cpp` | L1336-L1351 | Track cache control logic |

### DEBUG_LOGGING (logging behind always-false flags)

| Flag Pattern | Files | Instances |
|-------------|-------|-----------|
| `#ifdef _VIA_Debug` (never defined) | `adb.cpp` (8), `keyboard.cpp` (5), `via.cpp` (6), `via2.cpp` (4) | ~23 blocks, ~50 dead lines total |
| `XXX_dolog` = `dbglog_HAVE && 0` | `via.cpp`, `via2.cpp`, `asc.cpp`, `sony.cpp`, `video.cpp`, `iwm.cpp` | Many blocks throughout |
| `#if dbglog_HAVE && 0` | `sound.cpp` (L133), `adb_shared.h` (L146) | 2 blocks |
| `#if ExtraAbnormalReports` (=0) | `via.cpp` (L252), `via2.cpp` (L252), `sony.cpp` (L1201, L1272, L1470, L1523) | 6 blocks |
| `#if dbglog_HAVE` (alive in debug) | `rtc.cpp` (L122, L300, L493), `pmu.cpp` (L239), `video.cpp` (L780, L979) | 6 blocks — NOT dead in debug builds |

### ALTERNATIVE_IMPL (commented-out alternative approaches)

| File | Lines | Description |
|------|-------|-------------|
| `video.cpp` | L712-L716 | SetGamma: ReportAbnormal vs return noErr |
| `asc.cpp` | L124-L126, L165-L167 | Half-empty flag setting alternatives |
| `asc.cpp` | L730-L740 | Wavetable phase rounding (`#if 1` / `#else`) |
| `sound.cpp` | L147-L150 | Silence value: 0x00 (accurate) vs kCenterSound (fewer clicks) |
| `via.cpp` | L609-L611 | Interrupt flag after shift reg operation |
| `via2.cpp` | L609-L611 | Same as via.cpp — possibly should do this |

---

## Recommendations

### High Priority — Remove `_VIA_Debug` dead code

`_VIA_Debug` is never defined anywhere. All 23 `#ifdef _VIA_Debug` / `fprintf(stderr, ...)` blocks across `adb.cpp`, `keyboard.cpp`, `via.cpp`, `via2.cpp` should be converted to use `dbglog_HAVE` or removed entirely. ~50 lines.

### High Priority — Remove `ExtraAbnormalReports` dead blocks

`ExtraAbnormalReports` = 0 and never changed. The 6 guarded blocks in `via.cpp`, `via2.cpp`, `sony.cpp` are dead. Either enable the flag or delete the blocks.

### Medium Priority — Clean SAFELY_REMOVABLE blocks

~15 `#if 0` blocks across `video.cpp`, `asc.cpp`, `sony.cpp`, `pmu.cpp`, `scsi.cpp` totaling ~120 dead lines. These are old commented-out code that can be safely deleted.

### Medium Priority — Clean `dbglog_HAVE && 0` pattern

The `XXX_dolog` defines (VIA1_dolog, VIA2_dolog, ASC_dolog, Sony_dolog, VID_dolog, IWM_dolog) are all `dbglog_HAVE && 0`. The `&& 0` makes them dead even in debug builds. Either:
- Remove the `&& 0` to make them live in debug builds, or
- Delete the logging blocks entirely

### Low Priority — NOT_YET_ENABLED features

The `UseLargeScreenHack`, `CurAltHappyMac`, `ln2mtb` features were never completed. `screen_hack.h` (404 lines) and `hpmac_hack.h` (256 lines) are entirely dead. Could be removed or moved to `src/unused/` if historical preservation is desired. The unfinished video driver stubs (SetGamma, GetEntries, GetCurrentMode) should stay as they document the intended Mac II video card API.
