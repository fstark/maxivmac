# Dead Code Audit — maxivmac

Comprehensive analysis of `#if`-disabled code, always-false config guards, unused modules,
and other dead code in the maxivmac codebase.

> **Update (2026-03):** The platform frontend cleanup is complete. All backends
> except SDL have been removed (Cocoa, Carbon, X11, GTK, Win32, DOS, NDS,
> Classic Mac — ~33K lines deleted). Sections about those deleted files have
> been removed from this audit.

**Bottom line: ~5,500+ lines of dead code in compiled sources, plus ~2,700 lines in `src/unused/`.**

---

## Table of Contents

1. [Always-False Config Flags](#1-always-false-config-flags)
2. [Category A — Not-Yet-Enabled Features (KEEP)](#2-category-a--not-yet-enabled-features-keep)
3. [Category B — Safely Removable Now](#3-category-b--safely-removable-now)
4. [Category C — Other Dead Code](#4-category-c--other-dead-code)
5. [Per-File Summary Table](#5-per-file-summary-table)
6. [Recommendations](#6-recommendations)

---

## 1. Always-False Config Flags

These defines are hardcoded to `0` and gate most of the dead code:

| Flag | Value | Where Defined | What It Gates |
|------|-------|---------------|---------------|
| `EmLocalTalk` | `0` | `src/config/CNFUDALL.h:37` | LocalTalk networking (~600 lines) |
| `WantAbnormalReports` | `0` | `src/config/CNFUDALL.h:16` | Error reporting in CPU/core |
| `ExtraAbnormalReports` | `0` | (defaults) | Extended error reporting |
| `UseActvCode` | `0` | `src/config/CNFUDOSG.h:59` | Commercial activation system (~374 lines) |
| `EnableDemoMsg` | `0` | `src/config/CNFUDOSG.h:60` | Demo watermark messages |
| `EnableAltKeysMode` | `0` | `src/config/CNFUDOSG.h:18` | Alternate keyboard mode |
| `NeedIntlChars` | `0` | `src/config/CNFUDOSG.h:64` | International character conversion (~979 lines) |
| `CheckRomCheckSum` | `0` | `src/config/CNFUDOSG.h:15` | ROM checksum verification |
| `SmallGlobals` | `0` | `src/config/CNFUIALL.h:14` | Heap-allocated globals workaround |
| `cIncludeUnused` | `0` | `src/config/CNFUIALL.h:13` | Unused FPU math functions |
| `Sony_VerifyChecksums` | `0` | `src/config/CNFUDPIC.h:30` | Disk image checksum verification |
| `SCC_dolog` | `dbglog_HAVE && 0` → `0` | `src/devices/scc.cpp:53` | SCC debug tracing |
| `SCC_TrackMore` | `0` | `src/devices/scc.cpp:54` | SCC register field tracking |
| `_VIA_Debug` | never defined | — | VIA/ADB/keyboard debug logging |

---

## 2. Category A — Not-Yet-Enabled Features (KEEP)

Code that represents real features from minivmac that could be enabled in the future.
**~2,400 lines. Do not remove.**

### A1. LocalTalk Networking (`EmLocalTalk`) — ~600 lines
- **`src/devices/scc.cpp`**: 47 `#if EmLocalTalk` blocks — complete LocalTalk/LLAP packet handling,
  channel B receive/transmit, address search, interrupt logic
- **`src/unused/LTOVRBPF.h`** (383 lines): LocalTalk transport over BPF (macOS/BSD)
- **`src/unused/LTOVRUDP.h`** (457 lines): LocalTalk transport over UDP
- **`src/core/main.cpp`**: LocalTalk initialization/tick hooks
- **`src/platform/common/osglu_common.cpp`**: LocalTalk platform glue
- **Status**: Complete, coherent feature by Michael Fort (2011-2012). Enable with `EmLocalTalk 1`.

### A2. SCC Register Tracking (`SCC_TrackMore`) — ~500 lines
- **`src/devices/scc.cpp`**: 84 blocks tracking baud rate, parity, CRC, modem control lines,
  clock sources, character size, stop bits, etc.
- **Status**: Scaffolding for serial port emulation (ImageWriter, modem). Keep if any serial
  device support is planned.

### A3. International Character Conversion (`NeedIntlChars`) — ~979 lines
- **`src/platform/common/intl_chars.cpp`**: Full MacRoman↔Unicode conversion tables
- **Status**: Needed for proper text clipboard exchange with non-ASCII content.

### A4. FPU Unused Math Functions (`cIncludeFPUUnused`) — ~400 lines
- **`src/cpu/fpu_math.h`**: 14 blocks of SoftFloat functions (extended precision multiply,
  divide, sqrt variants) not currently called
- **Status**: May be needed for full FPU instruction coverage. Keep behind the flag.

### A5. ALSA Sound Backend (`src/unused/SGLUALSA.h`) — 1,619 lines
- Complete ALSA audio backend for Linux
- **Status**: High value for Linux SDL fallback or dedicated ALSA support.

### A6. ROM Checksum Verification (`CheckRomCheckSum`) — ~20 lines
- **`src/devices/rom.cpp`**: Validates ROM against known checksums
- **Status**: Useful feature, trivially re-enabled.

### A7. Alternate Keyboard Mode (`EnableAltKeysMode`) — ~50 lines
- **`src/platform/common/control_mode.cpp`**: Alternate key mapping for control mode
- **Status**: Minor feature, keep for accessibility.

### A8. Sony Disk Checksums (`Sony_VerifyChecksums`) — ~30 lines
- **`src/devices/sony.cpp`**: Verify disk image sector checksums
- **Status**: Useful integrity check, keep.

---

## 3. Category B — Safely Removable Now

Code that has no future value: obsolete workarounds, abandoned implementations,
deprecated API paths, debug scaffolding, and truly dead stubs.
**~3,100+ lines in compiled sources. Safe to delete.**

### B1. Activation Code System (`UseActvCode`) — 374 lines → DELETE
- **`src/platform/common/actv_code.h`**: Entire file. Commercial software activation
  system with registration codes, date checking, demo mode. Maxivmac is open source.
  This will never be re-enabled.

### B2. Demo Watermark (`EnableDemoMsg`) — ~100 lines → DELETE
- **`src/platform/common/control_mode.cpp`**: ~70 lines of demo message display
- **`src/platform/common/intl_chars.cpp`**: ~30 lines of demo watermark text data
- Related to `UseActvCode`; both are remnants of the commercial minivmac era.

### B3. SCC "Always Constant" Register Stubs — ~120 lines → DELETE
- **`src/devices/scc.cpp`**: ~50 blocks of `#if 0` stubs for register bits hardcoded
  to idle values (AllSent, CTS, DCD, RxOverrun, CRCFramingErr, ParityErr, ZeroCount,
  BreakAbort, SyncHuntIE, DCD_IE, StatusHiLo, TxBufferEmpty-when-not-LT).
  Replace with a single comment: *"Unconnected SCC pins are hardcoded to idle values."*

### B4. SCC Alternative Implementations — ~290 lines → DELETE
- **`src/devices/scc.cpp`**: 14 `#if 0` blocks:
  - `SCC_Interrupt()` — unused interrupt dispatch (14 lines)
  - `SCC_Int()` — unused 90-line polling function
  - `ReceiveAInterrupt` / `TransmitAInterrupt` / `ExtStatusAInterrupt` — duplicated logic
  - `StatusHiLo` encoding path — unreachable
  - `SCC_SetBaud` — references non-existent function
  - TxUnderrun/EOM alternatives — "works better without this"

### B5. SDL `#if 0` Blocks — ~100 lines → DELETE
Pure `#if 0` blocks and `#if 0 && Feature` blocks in `src/platform/sdl.cpp`:
- `UseMotionEvents` motion event handling
- Disabled event handling code
- The `#if 0 == SDL_MAJOR_VERSION` block (L4066-4699, 634 lines) is a headless/no-SDL
  fallback that is dead when SDL is present.

### B6. VIA Debug Logging (`_VIA_Debug`) — ~50 lines → DELETE
- **`src/devices/adb.cpp`**, **`keyboard.cpp`**, **`via.cpp`**, **`via2.cpp`**:
  23 `#ifdef _VIA_Debug` blocks. This symbol is **never defined anywhere** in the project.
  These are leftover debug prints with no enable path.

### B7. Screen/Video Hacks — ~660 lines → DELETE (or archive)
- **`src/devices/screen_hack.h`** (404 lines): `UseLargeScreenHack` — Large screen hack
  for Macs that don't natively support it. Guard is always 0.
- **`src/devices/hpmac_hack.h`** (256 lines): `CurAltHappyMac` — Alternative happy Mac
  icon hack during boot. Never included/enabled.
- These are novelty hacks, not core emulation features.

### B8. CPU/Core Dead Code — ~250 lines → DELETE
- **`src/cpu/m68k.cpp`**: `SmallGlobals` heap workaround (dead), `WantDumpTable`/
  `WantDumpAJump` debug tables (always 0), `ResetPending` dead field, always-true/false
  `#if 0` conditions
- **`src/cpu/m68k_tables.cpp`**: ~20 `#if 0` blocks of old instruction decode logic
  superseded by `CheckValidAddrMode` + cycle counting
- **`src/cpu/fpu_emdev.h`**: 2 `#if 0` blocks (old `read_single`/`write_single`, old
  `FPU_Save`)
- **`src/cpu/disasm.cpp`**: 5 `#if 0` blocks (unfinished address mode handlers, register dump)
- **`src/core/machine.cpp`**: ATT verification alternative, address space alternatives,
  `WantAbnormalReports` blocks
- **`src/core/main.cpp`**: `SmallGlobals` blocks
- **`src/cpu/endian.h`**: 2 alternative byte-swap implementations

### B9. OSS Sound Backend (`src/unused/SGLUDDSP.h`) — 229 lines → DELETE
- `/dev/dsp` OSS audio. OSS has been deprecated on Linux for over a decade.
  No modern platform uses it.

### B10. `ExtraAbnormalReports` Blocks — ~30 lines → DELETE
- Scattered across `src/devices/sony.cpp`, `scsi.cpp`, and others.
  Extended error reporting that's always disabled and adds nothing over
  `WantAbnormalReports`.

---

## 4. Category C — Other Dead Code

### C1. SCC Debug Logging (`SCC_dolog`) — ~300 lines → CONVERT
- **`src/devices/scc.cpp`**: 93 blocks of trace-level logging using `dbglog_*`.
  Currently disabled by `SCC_dolog = dbglog_HAVE && 0`.
- **Recommendation**: Convert to a runtime-configurable `SCC_TRACE()` macro
  (like `--trace-scc` flag). This eliminates 93 `#if`/`#endif` pairs while
  preserving diagnostic value.

### C2. `src/unused/` Files — ~2,700 lines → ARCHIVE or DELETE

| File | Lines | Target Platform | Status |
|------|-------|----------------|--------|
| `LTOVRBPF.h` | 383 | LocalTalk over BPF | Potential future use (LocalTalk) |
| `LTOVRUDP.h` | 457 | LocalTalk over UDP | Potential future use (LocalTalk) |
| `SGLUALSA.h` | 1,619 | ALSA sound (Linux) | Potential future use |
| `SGLUDDSP.h` | 229 | OSS sound (Linux) | Dead (OSS deprecated) |

**Note:** All non-SDL platform backends (Cocoa, Carbon, X11, GTK, Win32, DOS,
NDS, Classic Mac — ~33K lines) have already been removed from the codebase.

### C3. Internal Dead Code in `src/unused/` Files
- **`LTOVRBPF.h`**: 10 lines internal `#if 0` (minor)
- **`SGLUALSA.h`**: 233 lines of `#if 0` static-linking alternative code

### C4. Device-Level Debug Logging — Scattered
Various `*_dolog = dbglog_HAVE && 0` patterns in device files (IWM, SCSI, etc.).
Similar to SCC — should be converted to runtime-configurable tracing.

---

## 5. Per-File Summary Table

| File | Dead Lines | Category | Action |
|------|-----------|----------|--------|
| **`src/devices/scc.cpp`** | ~1,760 | A1+A2+B3+B4+C1 | Keep LT+Track, delete stubs+alts, convert logging |
| **`src/platform/common/intl_chars.cpp`** | ~979 | A3+B2 | Keep intl chars, delete demo msg |
| **`src/platform/common/actv_code.h`** | 374 | B1 | **Delete entire file** |
| **`src/devices/screen_hack.h`** | 404 | B7 | **Delete entire file** |
| **`src/devices/hpmac_hack.h`** | 256 | B7 | **Delete entire file** |
| **`src/platform/sdl.cpp`** | ~100 | B5 | Delete `#if 0` blocks, headless fallback |
| **`src/cpu/m68k.cpp`** | ~120 | B8 | Delete dead stubs |
| **`src/cpu/m68k_tables.cpp`** | ~100 | B8 | Delete old decode logic |
| **`src/cpu/fpu_math.h`** | ~600 | A4+B8 | Keep `cIncludeFPUUnused`, delete `#if 0` |
| **`src/cpu/fpu_emdev.h`** | ~30 | B8 | Delete `#if 0` blocks |
| **`src/cpu/disasm.cpp`** | ~40 | B8 | Delete `#if 0` blocks |
| **`src/core/machine.cpp`** | ~60 | B8 | Delete alternatives |
| **`src/core/main.cpp`** | ~40 | A1+B8 | Keep LT hooks, delete SmallGlobals |
| **`src/platform/common/control_mode.cpp`** | ~100 | A7+B2 | Keep alt keys, delete demo |
| **`src/devices/via.cpp`** | ~15 | B6 | Delete `_VIA_Debug` |
| **`src/devices/via2.cpp`** | ~10 | B6 | Delete `_VIA_Debug` |
| **`src/devices/adb.cpp`** | ~15 | B6 | Delete `_VIA_Debug` |
| **`src/devices/keyboard.cpp`** | ~10 | B6 | Delete `_VIA_Debug` |
| **`src/devices/sony.cpp`** | ~30 | A8+B10 | Keep checksums, delete extra reports |
| **`src/devices/scsi.cpp`** | ~10 | B10 | Delete extra reports |
| **`src/devices/rom.cpp`** | ~20 | A6 | Keep (useful feature) |
| **`src/cpu/endian.h`** | ~15 | B8 | Delete alt byte-swaps |
| **`src/unused/SGLUDDSP.h`** | 229 | B9 | **Delete entire file** |
| **Uncompiled platforms** (7 files) | ~27,000 | C2 | ~~Archive or delete~~ **Done** — all removed |

---

## 6. Recommendations

### Priority 1 — Quick Wins (delete, no risk)
1. **Delete `actv_code.h`** — commercial activation, 374 lines, open source project
2. **Delete `SGLUDDSP.h`** — OSS audio, deprecated everywhere
3. **Delete `screen_hack.h`** and **`hpmac_hack.h`** — novelty hacks never used
4. **Delete all `_VIA_Debug` blocks** — symbol never defined, ~50 lines across 4 files
5. **Delete `UseActvCode`/`EnableDemoMsg` code** in `control_mode.cpp` and `intl_chars.cpp`

### Priority 2 — Moderate Cleanup (~800 lines)
6. **Delete 50 SCC "always constant" stubs** — replace with one-line comment
7. **Delete 14 SCC alternative implementation blocks** — 290 lines of dead logic
8. **Delete `#if 0` blocks in `sdl.cpp`** — headless fallback and disabled code
9. **Delete CPU/core `#if 0` blocks** — old decode tables, SmallGlobals, alternatives

### Priority 3 — Structural
10. **Convert SCC debug logging** to runtime-configurable tracing — eliminates 93 `#if`/`#endif` pairs
11. **Convert other device `*_dolog`** patterns similarly

### Priority 4 — Leave Alone
12. **Keep `EmLocalTalk` code** — complete, valuable feature
13. **Keep `SCC_TrackMore` code** — serial port scaffolding
14. **Keep `NeedIntlChars` code** — needed for proper clipboard
15. **Keep `cIncludeFPUUnused` code** — may be needed for full FPU coverage
16. **Keep `SGLUALSA.h`** — Linux audio backend
17. **Keep `LTOVRBPF.h` and `LTOVRUDP.h`** — LocalTalk transports
