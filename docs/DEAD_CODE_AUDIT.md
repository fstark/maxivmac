# Dead/Disabled Code Audit — maxivmac `src/`

**Generated:** 2026-03-23  
**Updated:** 2026-03-25 (platform cleanup complete: only SDL backend remains)  
**Scope:** All `.cpp`, `.h` files under `src/` (excluding deleted platform backends)

---

## Summary

| Category | Blocks | ~Dead Lines |
|----------|--------|-------------|
| Pure `#if 0` blocks | ~120 | ~2,000+ |
| `#if 0 && Feature` blocks | 3 | ~20 |
| `#if 0 /* comment */` blocks (conditional disabled) | ~30 | ~200 |
| Config-gated always-false (`EmLocalTalk`, `SCC_TrackMore`, etc.) | ~292 | ~3,547 |
| Entire unused files (`src/unused/`) | 4 files | 2,686 |
| **Estimated Total** | | **~8,500 lines** |

> **Note:** ~33,000 lines of dead platform backends (Cocoa, Carbon, X11, GTK,
> Win32, DOS, NDS, Classic Mac) were removed in the SDL-only cleanup.
> The `cocoa.mm`, `win32.cpp`, `x11.cpp`, `gtk.cpp`, `carbon.cpp`,
> `classic_mac.cpp`, `nds.cpp`, and `dos.cpp` sections from the original
> audit no longer apply.

**NOTE:** `#if 0 != MACRO` patterns (e.g., `#if 0 != SDL_MAJOR_VERSION`, `#if 0 != vMacScreenDepth`) are NOT dead code — they are valid comparisons and are excluded from this report.

---

## 1. `src/unused/` — Entire Dead Files (2,686 lines)

These files are in an explicit "unused" directory and not included in any build.

| File | Lines | Description | Classification |
|------|-------|-------------|----------------|
| `src/unused/SGLUALSA.h` | 1,618 | ALSA sound driver for Linux | obsolete/removable |
| `src/unused/LTOVRUDP.h` | 457 | LocalTalk over UDP implementation | not-yet-enabled feature |
| `src/unused/LTOVRBPF.h` | 383 | LocalTalk over BPF (BSD Packet Filter) | not-yet-enabled feature |
| `src/unused/SGLUDDSP.h` | 228 | Digital DSP sound output | obsolete/removable |

---

## 2. Pure `#if 0` Blocks — By File

### `src/cpu/m68k.cpp`

| Lines | Condition | Description | Classification |
|-------|-----------|-------------|----------------|
| L222-? | `#if 0` | Dead instruction decode block | obsolete/removable |
| L3713-? | `#if 0` | Disabled CPU code | obsolete/removable |
| L4424-? | `#if 0` | Disabled CPU code | obsolete/removable |
| L6699-? | `#if 0` | Disabled CPU code | obsolete/removable |
| L8693-? | `#if 0` | Disabled CPU code | obsolete/removable |
| L8905-? | `#if 0` | Disabled CPU code | obsolete/removable |

### `src/cpu/m68k_tables.cpp`

| Lines | Condition | Description | Classification |
|-------|-----------|-------------|----------------|
| L573-? | `#if 0` | Disabled decode table entry | obsolete/removable |
| L806-? | `#if 0` | Disabled decode — `kIKindAndI` | obsolete/removable |
| L882-? | `#if 0` | Disabled decode — `kIKindOrI` | obsolete/removable |
| L916-? | `#if 0` | Disabled decode — `kIKindSubI` | obsolete/removable |
| L950-? | `#if 0` | Disabled decode — `kIKindAddI` | obsolete/removable |
| L984-? | `#if 0` | Disabled decode — `kIKindAddI` (dup) | obsolete/removable |
| L1018-? | `#if 0` | Disabled decode — `kIKindEorI` | obsolete/removable |
| L2025-? | `#if 0` | Disabled decode — `kIKindAddQ` | obsolete/removable |
| L2033-? | `#if 0` | Disabled decode — `kIKindSubQ` | obsolete/removable |
| L2176-? | `#if 0` | Disabled decode — `kIKindOrEaD` | obsolete/removable |
| L2300-? | `#if 0` | Disabled decode — `kIKindOrDEa` | obsolete/removable |
| L2330-? | `#if 0` | Disabled decode — `kIKindSubA` | obsolete/removable |
| L2359-? | `#if 0` | Disabled decode — `kIKindSubEaR` | obsolete/removable |
| L2423-? | `#if 0` | Disabled decode — `kIKindSubREa` | obsolete/removable |
| L2462-? | `#if 0` | Disabled decode — `kIKindCmpA` | obsolete/removable |
| L2482-? | `#if 0` | Disabled decode — `kIKindCmpM` | obsolete/removable |
| L2500-? | `#if 0` | Disabled decode — `kIKindEor` | obsolete/removable |
| L2530-? | `#if 0` | Disabled decode — `kIKindCmp` | obsolete/removable |
| L2581-? | `#if 0` | Disabled decode — `kIKindAndEaD` | obsolete/removable |
| L2687-? | `#if 0` | Disabled decode — `kIKindAndDEa` | obsolete/removable |

### `src/cpu/fpu_math.h`

| Lines | Condition | Description | Classification |
|-------|-----------|-------------|----------------|
| L586-? | `#if 0` | FPU math alternative implementation | obsolete/removable |
| L3760-? | `#if 0` | FPU rounding alternative | obsolete/removable |
| L3800-? | `#if 0` | FPU rounding alternative | obsolete/removable |
| L3831-? | `#if 0` | FPU rounding alternative | obsolete/removable |
| L3863-? | `#if 0` | FPU rounding alternative | obsolete/removable |
| L3910-? | `#if 0` | FPU alternative | obsolete/removable |
| L4107-? | `#if 0` | FPU alternative | obsolete/removable |
| L4193-? | `#if 0` | FPU alternative | obsolete/removable |
| L4401-? | `#if 0` | FPU alternative | obsolete/removable |
| L4892-? | `#if 0` | FPU alternative | obsolete/removable |
| L4962-? | `#if 0` | FPU alternative | obsolete/removable |
| L5157-? | `#if 0` | FPU alternative | obsolete/removable |
| L5176-? | `#if 0` | FPU alternative | obsolete/removable |
| L5278-? | `#if 0` | FPU alternative | obsolete/removable |
| L5298-? | `#if 0` | FPU alternative | obsolete/removable |
| L5373-? | `#if 0` | FPU alternative | obsolete/removable |
| L5487-? | `#if 0` | FPU alternative | obsolete/removable |

### `src/cpu/fpu_emdev.h`

| Lines | Condition | Description | Classification |
|-------|-----------|-------------|----------------|
| L127-? | `#if 0` | Alternative FPU device code | obsolete/removable |
| L250-? | `#if 0` | Alternative FPU device code | obsolete/removable |

### `src/cpu/disasm.cpp`

| Lines | Condition | Description | Classification |
|-------|-----------|-------------|----------------|
| L158-? | `#if 0` | Disassembler disabled code | obsolete/removable |
| L186-? | `#if 0` | Disassembler disabled code | obsolete/removable |
| L2858-? | `#if 0` | Disassembler disabled code | obsolete/removable |
| L2878-? | `#if 0` | Disassembler disabled code | obsolete/removable |
| L2897-? | `#if 0` | Disassembler disabled code | obsolete/removable |

### `src/core/main.cpp`

| Lines | Condition | Description | Classification |
|-------|-----------|-------------|----------------|
| L98-? | `#if 0` | Disabled main loop code | obsolete/removable |
| L185-? | `#if 0` | Disabled code | obsolete/removable |
| L197-? | `#if 0` | Disabled code | obsolete/removable |

### `src/core/machine.cpp`

| Lines | Condition | Description | Classification |
|-------|-----------|-------------|----------------|
| L939-? | `#if 0` | Disabled machine init code | obsolete/removable |
| L963-? | `#if 0` | Disabled machine code | obsolete/removable |
| L1089-? | `#if 0` | Disabled machine code | obsolete/removable |
| L1107-? | `#if 0` | Disabled machine code | obsolete/removable |
| L1148-? | `#if 0` | Disabled machine code | obsolete/removable |
| L1360-? | `#if 0` | Disabled machine code | obsolete/removable |

### `src/core/endian.h`

| Lines | Condition | Description | Classification |
|-------|-----------|-------------|----------------|
| L54-? | `#if 0` | Alternative endian implementation | obsolete/removable |
| L61-? | `#if 0` | Alternative endian implementation | obsolete/removable |

### `src/devices/scc.cpp`

Pure `#if 0` blocks (in addition to the massive config-gated blocks listed in Section 4):

| Lines | Condition | Description | Classification |
|-------|-----------|-------------|----------------|
| L518-521 | `#if 0` | ReadPrint/ReadModem counters | obsolete/removable |
| L538-562 | `#if 0 /* ReceiveAInterrupt always false */` | Disabled interrupt logic | other (simplification) |
| L563-569 | `#if 0` | TransmitAInterrupt logic | other (simplification) |
| L570-588 | `#if 0` | ExtStatusAInterrupt combined logic | other (simplification) |
| L627-635 | `#if 0` | External Interrupt Enable logic | other (simplification) |
| L833-839 | `#if 0` | ReadPrint/ReadModem reset | obsolete/removable |
| L966-978 | `#if 0` | `SCC_Interrupt` function | obsolete/removable |
| L980-991 | `#if 0` | `SCC_Int` function | obsolete/removable |
| L1013-1084 | `#if 0` | Incoming data check (72 lines) | obsolete/removable |
| L1279-1288 | `#if 0` | Read register 2 (channel A) | obsolete/removable |
| L1380-1397 | `#if 0` | Read register 15 | obsolete/removable |
| L1508-1513 | `#if 0` | TxUnderrun reset | obsolete/removable |
| L2572-2583 | `#if 0` | Baud rate setting | obsolete/removable |
| L2610-2613 | `#if 0` | Baud rate setting | obsolete/removable |

Many `#if 0 /* comment */` blocks mark SCC features that are "always true/false":

| Lines | Condition | Description | Classification |
|-------|-----------|-------------|----------------|
| L442-444 | `#if 0 /* AllSent always true */` | AllSent field | other (simplification) |
| L445-447 | `#if 0 /* CTS always false */` | CTS field | other (simplification) |
| L448-454 | `#if 0 /* DCD always false */` | DCD field | other (simplification) |
| L459-461 | `#if 0 /* RxOverrun always false */` | RxOverrun field | other (simplification) |
| L462-464 | `#if 0 /* CRCFramingErr always false */` | CRCFramingErr field | other (simplification) |
| L469-471 | `#if 0 /* ParityErr always false */` | ParityErr field | other (simplification) |
| L472-474 | `#if 0 /* ZeroCount always false */` | ZeroCount field | other (simplification) |
| L475-477 | `#if 0 /* BreakAbort always false */` | BreakAbort field | other (simplification) |
| L478-480 | `#if 0 /* SyncHuntIE usually false */` | SyncHuntIE field | other (simplification) |
| L490-492 | `#if 0 /* don't care about DCD_IE */` | DCD_IE field | other (simplification) |
| L511-513 | `#if 0 /* StatusHiLo always false */` | StatusHiLo field | other (simplification) |
| ...and ~40 more similar through the file | | SCC status register bits | other (simplification) |

### `src/devices/asc.cpp`

| Lines | Condition | Description | Classification |
|-------|-----------|-------------|----------------|
| L107-110 | `#if 0 /* seems to happen in tetris */` | Abnormal report for channel overflow | other (known issue) |
| L124-126 | `#if 0 /* doesn't seem necessary */` | SoundReg804 flag set | other |
| L165-167 | `#if 0 /* doesn't seem necessary */` | SoundReg804 flag set | other |
| L198-204 | `#if 0` | Address range check | obsolete/removable |
| L263-266 | `#if 0` | Abnormal report for CONTROL change | obsolete/removable |
| L297-300 | `#if 0 /* happens in system 6 */` | Abnormal report for FIFO clear | other (known issue) |
| L322-327 | `#if 0` | Abnormal report for FIFO IRQ STATUS | obsolete/removable |
| L345-351 | `#if 0` | Abnormal report for interrupt handling | obsolete/removable |
| L361-363 | `#if 0` | Data != 0 check | obsolete/removable |

### `src/devices/video.cpp`

| Lines | Condition | Description | Classification |
|-------|-----------|-------------|----------------|
| L121-126 | `#if 0` | `PatchAOSLstEntry0` function | obsolete/removable |
| L176-179 | `#if 0` | pTo_MajorBase/Length fields | obsolete/removable |
| L264-267 | `#if 0` | Patch reserve calls | obsolete/removable |
| L302-308 | `#if 0` | PatchAReservedOSLstEntry calls | obsolete/removable |
| L706-710 | `#if 0` | SetGamma - not implemented | not-yet-enabled feature |
| L712-714 | `#if 0` | SetGamma abnormal report | obsolete/removable |
| L726-730 | `#if 0` | GrayPage - not implemented | not-yet-enabled feature |
| L817-826 | `#if 0` | SetEntries - not implemented | not-yet-enabled feature |
| L903-914 | `#if 0` | GetCurrentMode response | not-yet-enabled feature |

### `src/devices/sony.cpp`

| Lines | Condition | Description | Classification |
|-------|-----------|-------------|----------------|
| L415-426 | `#if 0 /* don't look at boot blocks */` | Boot block checking | obsolete/removable |
| L946-959 | `#if 0` | MyDriverDat_R struct definition | obsolete/removable |
| L1197-1250 | `#if 0` | PosMode/newMode disk positioning (54 lines) | obsolete/removable |
| L1230-? | `#if 0` | Inside the above block | obsolete/removable |
| L1336-1349 | `#if 0` | Control call argument handling | obsolete/removable |
| L1390-1393 | `#if 0` | TwoSideFmt initialization | obsolete/removable |
| L1542-1545 | `#if 0` | ParamBlk/DeviceCtl reading | obsolete/removable |

### `src/devices/sound.cpp`

| Lines | Condition | Description | Classification |
|-------|-----------|-------------|----------------|
| L147-148 | `#if 0` | Silence fill with 0x00 (more accurate) vs avoiding clicks | other |

### `src/devices/scsi.cpp`

| Lines | Condition | Description | Classification |
|-------|-----------|-------------|----------------|
| L81-84 | `#if 0` | SCSI register reset | obsolete/removable |

### `src/devices/pmu.cpp`

| Lines | Condition | Description | Classification |
|-------|-----------|-------------|----------------|
| L98-101 | `#if 0` | PMU downloadStatus case | obsolete/removable |
| L215-223 | `#if 0` | PMU debug dump loop | obsolete/removable |

### `src/devices/via.cpp`

| Lines | Condition | Description | Classification |
|-------|-----------|-------------|----------------|
| L609-611 | `#if 0` | `setInterruptFlag(kIntSR)` call | other (behavior question) |

### `src/devices/via2.cpp`

| Lines | Condition | Description | Classification |
|-------|-----------|-------------|----------------|
| L609-611 | `#if 0 /* possibly should do this */` | `setInterruptFlag(kIntSR)` call | other (behavior question) |

### `src/devices/screen_hack.h`

| Lines | Condition | Description | Classification |
|-------|-----------|-------------|----------------|
| L266-290 | `#if 0` | Screen hack alert drawing (25 lines) | not-yet-enabled feature |

### `src/platform/sdl.cpp` (3 pure `#if 0` + many `#if 0 != SDL_MAJOR_VERSION`)

Pure `#if 0` blocks:

| Lines | Condition | Description | Classification |
|-------|-----------|-------------|----------------|
| L3695-? | `#if 0 && UseMotionEvents` | Motion event handling | not-yet-enabled feature |
| L3884-? | `#if 0` | Disabled event handling code | obsolete/removable |
| L4293-? | `#if 0` | Disabled code | obsolete/removable |
| L4392-? | `#if 0` | Disabled code | obsolete/removable |

**Note:** SDL has many `#if 0 != SDL_MAJOR_VERSION` and `#if 0 == SDL_MAJOR_VERSION` blocks. These are NOT dead code — they're conditional on the actual SDL version. However, the `#if 0 == SDL_MAJOR_VERSION` blocks at L1853 and L4066-4699 (634 lines!) are dead if SDL is present (SDL_MAJOR_VERSION > 0). The L4066-4699 block is a massive 634-line `CreateMainWindow` implementation for the "no SDL" case that is likely obsolete.

### `src/platform/common/control_mode.cpp`

| Lines | Condition | Description | Classification |
|-------|-----------|-------------|----------------|
| L496-498 | `#if 0 && (UseActvCode \|\| EnableDemoMsg)` | Registration string msg | not-yet-enabled feature |
| L597-606 | `#if 0` | CopyRegistrationStr declaration | obsolete/removable |
| L680-685 | `#if 0 && (UseActvCode \|\| EnableDemoMsg)` | Key handler for copy reg string | not-yet-enabled feature |
| L956-966 | `#if 0` | Registration string display | obsolete/removable |

### `src/platform/common/osglu_common.cpp`

| Lines | Condition | Description | Classification |
|-------|-----------|-------------|----------------|
| L1238-1239 | `#if 0` | LT_NodeHint collision test | obsolete/removable |

### `src/platform/common/screen_translate.h`

| Lines | Condition | Description | Classification |
|-------|-----------|-------------|----------------|
| L109-116 | `#if 0` | 15-bit color conversion alternative | obsolete/removable |

---

## 3. `#if 0 && Feature` Blocks (Always Dead)

These use `0 &&` which always evaluates to false regardless of the feature flag:

| File | Line | Condition | Description | Classification |
|------|------|-----------|-------------|----------------|
| `src/devices/scc.cpp` | L1334 | `#if 0 && EmLocalTalk` | LocalTalk value | obsolete/removable |
| `src/platform/sdl.cpp` | L3695 | `#if 0 && UseMotionEvents` | Motion events | not-yet-enabled feature |
| `src/platform/common/control_mode.cpp` | L496 | `#if 0 && (UseActvCode \|\| EnableDemoMsg)` | Reg string message | not-yet-enabled feature |
| `src/platform/common/control_mode.cpp` | L680 | `#if 0 && (UseActvCode \|\| EnableDemoMsg)` | Key handler copy reg | not-yet-enabled feature |

> **Note:** Entries for deleted platform files (cocoa.mm, gtk.cpp, dos.cpp,
> nds.cpp, x11.cpp) have been removed — those files no longer exist in the codebase.

---

## 4. Config-Gated Dead Code (Always-False Defines)

These defines are set to 0 in the current config and gate code across the codebase:

### `EmLocalTalk` = 0 (in `CNFUDALL.h`)
- **57 blocks, ~1,134 lines** of LocalTalk networking code
- Files: `scc.cpp` (majority), `scc.h`, `rtc.cpp`, `main.cpp`, `platform.h`, `osglu_common.cpp`, `osglu_common.h`
- **Classification:** not-yet-enabled feature (LocalTalk networking support)

### `SCC_TrackMore` = 0 (local in `scc.cpp`)
- **71 blocks, ~632 lines** of detailed SCC register tracking
- File: `scc.cpp` only
- **Classification:** not-yet-enabled feature (debug/verbose SCC tracking)

### `SCC_dolog` = 0 (local in `scc.cpp`, `dbglog_HAVE && 0`)
- **92 blocks, ~403 lines** of SCC debug logging
- File: `scc.cpp` only
- **Classification:** not-yet-enabled feature (debug logging)

### `NeedIntlChars` = 0 (in `CNFUDOSG.h`)
- **9 blocks, ~979 lines** of international character support
- File: `intl_chars.cpp`, `intl_chars.h`
- **Classification:** not-yet-enabled feature (international character tables and mappings)

### `WantAbnormalReports` = 0 (in `CNFUDALL.h`)
- **9 blocks, ~76 lines** of abnormal condition reporting
- Files: `machine.cpp`, `control_mode.cpp`, `osglu_common.cpp`, `osglu_common.h`, `platform.h`
- **Classification:** not-yet-enabled feature (debug diagnostic reports)

### `ExtraAbnormalReports` = 0 (in `CNFUDPIC.h`)
- **11 blocks, ~70 lines** of extra diagnostic warnings
- Files: `machine.cpp`, `m68k.cpp`, `sony.cpp`, `via.cpp`, `via2.cpp`
- **Classification:** not-yet-enabled feature (verbose debug diagnostics)

### `EnableDemoMsg` = 0 (in `CNFUDOSG.h`)
- **16 blocks, ~105 lines** of demo message display
- Files: `control_mode.cpp`, `control_mode.h`, `intl_chars.cpp`, `intl_chars.h`, `sdl.cpp`
- **Classification:** not-yet-enabled feature (demo/registration messaging)

### `UseActvCode` = 0 (in `CNFUDOSG.h`)
- **11 blocks, ~48 lines** of activation code support
- Files: `control_mode.cpp`, `control_mode.h`, `sdl.cpp`
- **Classification:** not-yet-enabled feature (software activation system)

### `EnableAltKeysMode` = 0 (in `CNFUDOSG.h`)
- **7 blocks, ~40 lines** of alternate keyboard mode
- Files: `control_mode.cpp`, `control_mode.h`, `intl_chars.cpp`, `intl_chars.h`
- **Classification:** not-yet-enabled feature (alternate key mapping mode)

### `CheckRomCheckSum` = 0 (in `CNFUDOSG.h`)
- **3 blocks, ~36 lines** of ROM checksum verification
- File: `control_mode.cpp`
- **Classification:** not-yet-enabled feature (ROM validation)

### `SmallGlobals` = 0 (in `CNFUIALL.h`)
- **6 blocks, ~24 lines** of compact global variable layout
- Files: `main.cpp`, `cpu.cpp`, `cpu.h`, `m68k.cpp`, `m68k.h`
- **Classification:** not-yet-enabled feature (68K Mac memory optimization)

### `Sony_VerifyChecksums` = 0 (in `CNFUDPIC.h`)
- Guarded in `sony.cpp` L361 — disk checksum verification code
- **Classification:** not-yet-enabled feature (disk integrity checking)

---

## 5. Notable Large Dead Blocks

Biggest concentrations of dead code by file:

| File | ~Dead Lines | Primary Cause |
|------|-------------|---------------|
| `src/devices/scc.cpp` | ~2,700+ | `SCC_dolog`=0, `SCC_TrackMore`=0, `EmLocalTalk`=0, + many `#if 0` |
| `src/platform/common/intl_chars.cpp` | ~979 | `NeedIntlChars`=0 |
| `src/cpu/fpu_math.h` | ~200+ | Alternative FPU implementations |
| `src/cpu/m68k_tables.cpp` | ~100+ | Disabled instruction decode entries |
| `src/platform/sdl.cpp` | ~100+ | Pure `#if 0` blocks (excl. SDL version gates) |
| `src/unused/*` | 2,686 | Entire abandoned files |

---

## 6. Classification Summary

### Obsolete/Removable (~60% of total)
- Pure `#if 0` blocks with no feature flag reference
- Debug fprintf blocks
- Alternative implementations that were replaced

### Not-Yet-Enabled Features (~35% of total)
- `EmLocalTalk` — LocalTalk networking (biggest single feature)
- `NeedIntlChars` — international character support
- `SCC_TrackMore` — detailed SCC register tracking
- `SCC_dolog` — SCC debug logging
- `UseActvCode` / `EnableDemoMsg` — activation/demo system
- `WantAbnormalReports` / `ExtraAbnormalReports` — diagnostic reporting
- `EnableAltKeysMode` — alternate keyboard mode
- `CheckRomCheckSum` — ROM validation
- Video features (SetGamma, GetCurrentMode, SetEntries)

### Other (~5% of total)
- SCC simplification blocks (`/* always true/false */` comments)
- Known crash/issue workarounds (`/* crashes on some machines */`)
- Behavioral alternatives (`/* possibly should do this */`)
- The `src/unused/` files (LocalTalk implementations, sound drivers)

---

## 7. Detailed Audit: `src/core/` and `src/cpu/`

**Generated:** 2026-03-23

### Config Flag Values (verified from source)

| Flag | Value | Source |
|------|-------|--------|
| `dbglog_HAVE` | **1** (enabled) | `src/config/CNFUDALL.h:15` |
| `WantAbnormalReports` | 0 | `src/config/CNFUDALL.h:16` |
| `ExtraAbnormalReports` | 0 | `src/config/CNFUDPIC.h:127` |
| `EmLocalTalk` | 0 | `src/config/CNFUDALL.h:35` |
| `SmallGlobals` | 0 | `src/config/CNFUIALL.h:12` |
| `cIncludeUnused` / `cIncludeFPUUnused` | 0 | `src/config/CNFUIALL.h:13` |
| `WantDisasm` | 1 (dev) / 0 (cmake) | `src/config/CNFUDPIC.h:126` |
| `WantDumpTable` | 0 (default) | `src/cpu/m68k.cpp:308` |
| `WantDumpAJump` | 0 (default) | `src/cpu/m68k.cpp:4399` |
| `m68k_logExceptions` | 0 (`dbglog_HAVE && 0`) | `src/cpu/m68k.cpp:4397` |

**Note:** Contrary to the initial assumption, `dbglog_HAVE` is **1**. Pure `#if dbglog_HAVE` blocks are LIVE code. Only `#if dbglog_HAVE && 0` blocks are dead.

### Files with NO dead code

These files in `src/core/` and `src/cpu/` contain no `#if 0`, no always-false conditions, and no commented-out code blocks:

- `src/core/common.h`
- `src/core/config_loader.cpp`
- `src/core/config_loader.h`
- `src/core/defaults.h`
- `src/core/ict_scheduler.cpp`
- `src/core/ict_scheduler.h`
- `src/core/machine_config.cpp`
- `src/core/machine_config.h`
- `src/core/machine_obj.cpp`
- `src/core/machine_obj.h`
- `src/core/md5.h`
- `src/core/state_recorder.cpp`
- `src/core/state_recorder.hpp`
- `src/core/wire_bus.cpp`
- `src/core/wire_bus.h`
- `src/core/wire_ids.h`
- `src/cpu/disasm.h`
- `src/cpu/m68k_tables.h`

---

### `src/core/endian.h`

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 1 | L54–59 | `#if 0` | Alternative `do_get_mem_long` using individual shift+mask byte-swap (inside `#if LittleEndianUnaligned`). Was replaced by the 16-bit pair swap below it. | ALTERNATIVE_IMPL |
| 2 | L61–70 | `#if 0` | Second alternative `do_get_mem_long` using `<<24`/`<<8` shifts. Comment: "no, this doesn't do well with apple tools". Replaced by 16-bit pair swap. | ALTERNATIVE_IMPL |

---

### `src/core/main.cpp`

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 3 | L98–102 | `#if 0` | Debug logging in `SubTickNotify`: prints "ending sub tick N". 5 lines. | DEBUG_LOGGING |
| 4 | L146–149 | `#if dbglog_HAVE && 0` | `dbglog_WriteNote("begin new Sixtieth")` in `SixtiethSecondNotify`. Dead because `&& 0`. 4 lines. | DEBUG_LOGGING |
| 5 | L163–165 | `#if EmLocalTalk` | `SCC::localTalkTick()` call in per-tick update. EmLocalTalk=0. 3 lines. | NOT_YET_ENABLED |
| 6 | L178–180 | `#if dbglog_HAVE && 0` | `dbglog_WriteNote("end Sixtieth")` in `SixtiethEndNotify`. Dead because `&& 0`. 3 lines. | DEBUG_LOGGING |
| 7 | L185–188 | `#if 0` | Debug logging "begin extra time" in `ExtraTimeBeginNotify`. 4 lines. | DEBUG_LOGGING |
| 8 | L197–200 | `#if 0` | Debug logging "end extra time" in `ExtraTimeEndNotify`. 4 lines. | DEBUG_LOGGING |
| 9 | L213–215 | `#if SmallGlobals` | `g_cpu.reserveAlloc()` call in `EmulationReserveAlloc`. SmallGlobals=0. 3 lines. | NOT_YET_ENABLED |
| 10 | L270–279 | `#if dbglog_HAVE && 0` | Verbose cycle-counter debug logging before `m68k_go_nCycles`: prints nextCount, n2, n. Dead because `&& 0`. 10 lines. | DEBUG_LOGGING |

---

### `src/core/machine.h`

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 11 | L135–145 | `#else` branch of `#if ! WantAbnormalReports` | `DoReportAbnormalID` declaration and `ReportAbnormalID` macro with logging. WantAbnormalReports=0, so this `#else` branch is dead. The active branch (L133–134) defines `ReportAbnormalID` as empty. 11 lines. | NOT_YET_ENABLED |

---

### `src/core/machine.cpp`

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 12 | L173–201 | `#if WantAbnormalReports` | `GotOneAbnormal` flag, `DoReportAbnormalID` function implementation with logging and `WarnMsgAbnormalID` call. WantAbnormalReports=0. ~29 lines. | NOT_YET_ENABLED |
| 13 | L750–770 | `#if 0 /* verify list. not for final version */` | ATTListA verification loop: checks entries for mask consistency and conflicts. Debug validation code. 21 lines. | SAFELY_REMOVABLE |
| 14 | L939–955 | `#if 0` | Case 14 fallback in address space setup: handles accesses to unmapped addresses (0x1DA00, 0x1DC00). "fail, nothing supposed to be here, but rom accesses it anyway". 17 lines. | SAFELY_REMOVABLE |
| 15 | L963–967 | `#if 0` | `ReportAbnormalID` for "Overlay with 24 bit addressing" inside `SetUp_address24`. 5 lines. | SAFELY_REMOVABLE |
| 16 | L1089–1097 | `#if 0` | NuBus super space (0x90000000) VidMem mapping. Comment: "haven't persuaded emulated computer to look here yet." 9 lines. | NOT_YET_ENABLED |
| 17 | L1107–1114 | `#if 0` | Alternative NuBus standard space VidMem mapping at 0xF9000000 with 8MB mask. Replaced by the 1MB mapping at 0xF9900000 below. 8 lines. | ALTERNATIVE_IMPL |
| 18 | L1148–1152 | `#if 0` | Test hardware check at address 0x58000000. "test hardware. fail". 5 lines. | SAFELY_REMOVABLE |
| 19 | L1360–1369 | `#if 0` | `get_fail_realblock` function: sets up a catch-all fail ATT entry. Not called anywhere. 10 lines. | SAFELY_REMOVABLE |
| 20 | L1554–1560 | `#if ExtraAbnormalReports` | "access IWM word" report in `MMDV_Access` IWM handler. ExtraAbnormalReports=0. Comment: "This happens when quitting Glider 3.1.2". 7 lines. | NOT_YET_ENABLED |

---

### `src/cpu/cpu.cpp`

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 21 | L27–30 | `#if SmallGlobals` | `CPU::reserveAlloc()` method forwarding to `MINEM68K_ReserveAlloc()`. SmallGlobals=0. 4 lines. | NOT_YET_ENABLED |

---

### `src/cpu/cpu.h`

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 22 | L28–30 | `#if SmallGlobals` | `CPU::reserveAlloc()` declaration. SmallGlobals=0. 3 lines. | NOT_YET_ENABLED |

---

### `src/cpu/m68k.h`

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 23 | L23–24 | `#if SmallGlobals` | `MINEM68K_ReserveAlloc` function declaration. SmallGlobals=0. 2 lines. | NOT_YET_ENABLED |

---

### `src/cpu/m68k.cpp`

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 24 | L222–224 | `#if 0` | `bool ResetPending` field in `regs` struct. Removed feature — reset is done inline now. 3 lines. | SAFELY_REMOVABLE |
| 25 | L248–250 | `#if SmallGlobals` | `DecOpR *disp_table` (pointer, heap alloc) vs `DecOpR disp_table[disp_table_sz]` (static array in `#else`). SmallGlobals=0. 2 lines. | NOT_YET_ENABLED |
| 26 | L307–308 | `#define WantDumpTable 0` | Default definition. Makes all `#if WantDumpTable` blocks dead. | — |
| 27 | L311–313 | `#if WantDumpTable` | `static uint32_t DumpTable[kNumIKinds]` array declaration. WantDumpTable=0. 3 lines. | DEBUG_LOGGING |
| 28 | L786–788 | `#if WantDumpTable` | `DumpTable[MainClas]++` counter increment in `DecodeNextInstruction`. WantDumpTable=0. 3 lines. | DEBUG_LOGGING |
| 29 | L798–807 | `#if WantDumpTable` | `DumpTable[MainClas]--` counter decrement in `UnDecodeNextInstruction`. WantDumpTable=0. 10 lines. | DEBUG_LOGGING |
| 30 | L1034–1039 | `#if ExtraAbnormalReports` | Report for Extension Word with scale factor. ExtraAbnormalReports=0. Comment: "apparently can happen in Sys 7.5.5 boot on 68000". 6 lines. | NOT_YET_ENABLED |
| 31 | L2366–2374 | `#if 0 /* always true */` | `cctrue_TstL_CC` function: tests `(uint32_t) >= 0` which is always true for unsigned. Correctly disabled. 9 lines. | SAFELY_REMOVABLE |
| 32 | L2377–2386 | `#if 0 /* always false */` | `cctrue_TstL_CS` function: tests `(uint32_t) < 0` which is always false for unsigned. Correctly disabled. 10 lines. | SAFELY_REMOVABLE |
| 33 | L3713–3735 | `#if 0` | `NeedDefaultLazyFlagsAddCommon` function: generic add-flags computation using flag decomposition. Replaced by type-specific `NeedDefaultLazyFlagsAddL`/`W`/`B`. 23 lines. | ALTERNATIVE_IMPL |
| 34 | L4397 | `#define m68k_logExceptions (dbglog_HAVE && 0)` | Evaluates to 0. Makes all `#if m68k_logExceptions` blocks dead (exception tracing). | — |
| 35 | L4399 | `#define WantDumpAJump 0` | Default definition. Makes all `#if WantDumpAJump` blocks dead. | — |
| 36 | L4404–4416 | `#if WantDumpAJump` | `DumpAJump` function: logs address jumps. WantDumpAJump=0. 13 lines. | DEBUG_LOGGING |
| 37 | L4424–4430 | `#if 0` | Debugger breakpoint check in `m68k_setpc`: triggers at specific PC values (0xBD50). 7 lines. | SAFELY_REMOVABLE |
| 38 | L6699–6705 | `#if 0` | Testing code in `DoCodeMoveP0`: triggers `op_illg()` on `(Displacement & 0x8000) != 0`. Comment: "for testing only". 7 lines. | SAFELY_REMOVABLE |
| 39 | L7072–7078 | `#if ExtraAbnormalReports` | "illegal opsize in CHK2 or CMP2" report. ExtraAbnormalReports=0. 7 lines. | NOT_YET_ENABLED |
| 40 | L7123–7128 | `#if ExtraAbnormalReports` | "illegal opsize in DoCAS" report. ExtraAbnormalReports=0. 6 lines. | NOT_YET_ENABLED |
| 41 | L8620–8624 | `#if ExtraAbnormalReports` | "Recalc_PC_Block fails" report. ExtraAbnormalReports=0. Comment: "happens on Restart". 5 lines. | NOT_YET_ENABLED |
| 42 | L8693–8696 | `#if 0` | `V_regs.ResetPending` check in `m68k_go_nCycles`. Pairs with removed `ResetPending` field (#24). 4 lines. | SAFELY_REMOVABLE |
| 43 | L8905–8907 | `#if 0` | `V_regs.ResetPending = true; NeedToGetOut()` in `m68k_reset`. The `#else` branch (L8908–L8940) contains the actual inline reset. 3 lines. | SAFELY_REMOVABLE |
| 44 | L8942–8945 | `#if SmallGlobals` | `MINEM68K_ReserveAlloc` function: allocates `disp_table` from heap. SmallGlobals=0. 4 lines. | NOT_YET_ENABLED |

---

### `src/cpu/disasm.cpp`

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 45 | L158–161 | `#if 0` | Address mode 6 disassembly: `ArgKind = AKMemory; ArgAddr.mem = get_disp_ea(...)`. Replaced by `dbglog_writeCStr("???")`. 4 lines. | ALTERNATIVE_IMPL |
| 46 | L186–188 | `#if 0` | Address mode 7/3 disassembly: `ArgKind = AKMemory; s = get_disp_ea(Disasm_pc)`. Replaced by `dbglog_writeCStr("???")`. 3 lines. | ALTERNATIVE_IMPL |
| 47 | L2858–2860 | `#if 0` | `bool Skipped = false` variable in `DisasmSavedPCs`. 3 lines. | SAFELY_REMOVABLE |
| 48 | L2878–2880 | `#if 0` | `Skipped = true` assignment when overflow detected. 3 lines. | SAFELY_REMOVABLE |
| 49 | L2897–2916 | `#if 0` | Register dump loop (D0–D7, A0–A7) when Skipped is true. Debug feature. 20 lines. | DEBUG_LOGGING |

---

### `src/cpu/fpu_emdev.h`

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 50 | L127–136 | `#if 0` | `read_single` and `write_single` functions for FPU single-precision memory I/O. Not used by any FPU instruction handler. 10 lines. | SAFELY_REMOVABLE |
| 51 | L250–253 | `#if 0` | Old `DecodeModeRegister(4); SetArgValueL(0)` for null state frame in `DoCodeFPU_Save`. Replaced by 28-byte IDLE frame below. 4 lines. | ALTERNATIVE_IMPL |

**Note:** `fpu_emdev.h` has **34 `#if dbglog_HAVE` blocks** (at L234, L258, L274, L291, L304, L313, L404, L449, L459, L496, L502, L541, L562, L609, L650, L659, L980, L1002, L1014, L1026, L1038, L1053, L1065, L1077, L1106, L1116, L1126, L1136, L1147, L1157, L1167, L1176, L1212). Since `dbglog_HAVE=1`, these are **LIVE code**, not dead.

---

### `src/cpu/fpu_math.h`

#### Pure `#if 0` blocks

| # | Lines | Condition | Description | Category |
|---|-------|-----------|-------------|----------|
| 52 | L586–588 | `#if 0` | Old `Ui6fromHiLo` implementation using struct members `z->lo`, `z->hi`. Replaced by `(ui6b)hi << 32 + lo`. 3 lines. | ALTERNATIVE_IMPL |
| 53 | L3760–3765 | `#if 0` | `floatx80_abs` inline function. Unused, replaced by direct sign-bit manipulation. 6 lines. | SAFELY_REMOVABLE |
| 54 | L3800–3812 | `#if 0` (inside `#if cIncludeFPUUnused`) | `floatx80_is_unsupported` check in `floatx80_to_int16`. Doubly dead: cIncludeFPUUnused=0 AND `#if 0`. 13 lines. | SAFELY_REMOVABLE |
| 55 | L3831–3841 | `#if 0` (inside `#if cIncludeFPUUnused`) | Same check in `floatx80_to_int16_round_to_zero`. Doubly dead. 11 lines. | SAFELY_REMOVABLE |
| 56 | L3863–3870 | `#if 0` | `floatx80_is_unsupported` in `floatx80_extract`. 8 lines. | SAFELY_REMOVABLE |
| 57 | L3910–3915 | `#if 0` | `floatx80_is_unsupported` in `floatx80_scale`. 6 lines. | SAFELY_REMOVABLE |
| 58 | L4107–4110 | `#if 0` | `floatx80_is_unsupported` in comparison function. 4 lines. | SAFELY_REMOVABLE |
| 59 | L4193–4199 | `#if 0` | `floatx80_is_unsupported` in `do_fprem`. 7 lines. | SAFELY_REMOVABLE |
| 60 | L4401–4403 | `#if 0` | `assert(n > 1)` in `EvalPoly`. 3 lines. | SAFELY_REMOVABLE |
| 61 | L4892–4898 | `#if 0` | `floatx80_is_unsupported` in `f2xm1`. 7 lines. | SAFELY_REMOVABLE |
| 62 | L4962–4963 | `#if 0` | Unused `floatx80_one` constant definition. 2 lines. | SAFELY_REMOVABLE |
| 63 | L5157–5163 | `#if 0` | `floatx80_is_unsupported` in `sincos` (first check). 7 lines. | SAFELY_REMOVABLE |
| 64 | L5176–5178 | `#if 0` | Dead `invalid:` label in `sincos`. 3 lines. | SAFELY_REMOVABLE |
| 65 | L5278–5284 | `#if 0` | `floatx80_is_unsupported` in `ftan`. 7 lines. | SAFELY_REMOVABLE |
| 66 | L5298–5300 | `#if 0` | Dead `invalid:` label in `ftan`. 3 lines. | SAFELY_REMOVABLE |
| 67 | L5373–5375 | `#if 0` | Unused `float128_one` constant definition. 3 lines. | SAFELY_REMOVABLE |
| 68 | L5487–5492 | `#if 0` | `floatx80_is_unsupported` in `fpatan`. 6 lines. | SAFELY_REMOVABLE |

**Pattern:** Blocks #54–68 (except #52, #53, #60, #62, #67) all disable checks for `floatx80_is_unsupported()` — an x87 FPU concept not relevant to the 68882 FPU emulation. These were systematically disabled.

#### `cIncludeFPUUnused` = 0 gated blocks

| # | Lines | Description | Category |
|---|-------|-------------|----------|
| 69 | L916+ | floatx80 round-to-int operations | NOT_YET_ENABLED |
| 70 | L1863+ | floatx80 additional operations | NOT_YET_ENABLED |
| 71 | L2508+ | floatx80 conversion utilities | NOT_YET_ENABLED |
| 72 | L2540+ | floatx80 normalization | NOT_YET_ENABLED |
| 73 | L2575+ | floatx80 denormalization | NOT_YET_ENABLED |
| 74 | L2610+ | floatx80 rounding utilities | NOT_YET_ENABLED |
| 75 | L2639+ | floatx80 additional rounding | NOT_YET_ENABLED |
| 76 | L2677+ | floatx80 square root utilities | NOT_YET_ENABLED |
| 77 | L3797–3826 | `floatx80_to_int16` (inside cIncludeFPUUnused) | NOT_YET_ENABLED |
| 78 | L3828–3856 | `floatx80_to_int16_round_to_zero` (inside cIncludeFPUUnused) | NOT_YET_ENABLED |
| 79 | L3984+ | `floatx80_to_int32` variants | NOT_YET_ENABLED |
| 80 | L4023+ | `floatx80_to_int32_round_to_zero` variants | NOT_YET_ENABLED |
| 81 | L4092+ | `floatx80_compare` function | NOT_YET_ENABLED |
| 82 | L4296+ | `do_fprem` helper variant | NOT_YET_ENABLED |

These collectively comprise several hundred lines of unused FPU helper functions.

---

### `src/cpu/m68k_tables.cpp`

All 20 `#if 0` blocks in this file follow the same pattern: they contain the **old, simpler** instruction decode (using `CheckDataAltAddrMode` or `IsValidAddrMode`) that was superseded by the more detailed `CheckValidAddrMode` + `WantCycByPriOp` cycle-counting implementation below them. All are **ALTERNATIVE_IMPL**.

| # | Lines | Old Code | New Code Below | Category |
|---|-------|----------|----------------|----------|
| 83 | L573–585 | Old `FindOpSizeFromb76` switch stmt | `p->opsize = 1 << b76(p)` one-liner | ALTERNATIVE_IMPL |
| 84 | L806–812 | `CheckDataAltAddrMode` → `kIKindCmpI` | `CheckValidAddrMode` + `CmpB+offset` | ALTERNATIVE_IMPL |
| 85 | L882–884 | `CheckDataAltAddrMode` → `kIKindOrI` | `CheckValidAddrMode` + cycle calc | ALTERNATIVE_IMPL |
| 86 | L916–918 | `CheckDataAltAddrMode` → `kIKindAndI` | `CheckValidAddrMode` + cycle calc | ALTERNATIVE_IMPL |
| 87 | L950–952 | `CheckDataAltAddrMode` → `kIKindSubI` | `CheckValidAddrMode` + `SubB+offset` | ALTERNATIVE_IMPL |
| 88 | L984–986 | `CheckDataAltAddrMode` → `kIKindAddI` | `CheckValidAddrMode` + `AddB+offset` | ALTERNATIVE_IMPL |
| 89 | L1018–1020 | `CheckDataAltAddrMode` → `kIKindEorI` | `CheckValidAddrMode` + cycle calc | ALTERNATIVE_IMPL |
| 90 | L2025–2028 | `CheckDataAltAddrMode` → `kIKindAddQ` | Direct `kIKindAddB+offset` | ALTERNATIVE_IMPL |
| 91 | L2033–2036 | `CheckDataAltAddrMode` → `kIKindSubQ` | Direct `kIKindSubB+offset` | ALTERNATIVE_IMPL |
| 92 | L2176–2179 | `CheckDataAddrMode` → `kIKindOrEaD` | `CheckValidAddrMode` + cycle calc | ALTERNATIVE_IMPL |
| 93 | L2300–2303 | `CheckDataAltAddrMode` → `kIKindOrDEa` | `CheckValidAddrMode` + cycle calc | ALTERNATIVE_IMPL |
| 94 | L2330–2335 | `IsValidAddrMode` → `kIKindSubA` | `CheckValidAddrMode` + cycle calc | ALTERNATIVE_IMPL |
| 95 | L2359–2362 | `IsValidAddrMode` → `kIKindSubEaR` | `CheckValidAddrMode` + cycle calc | ALTERNATIVE_IMPL |
| 96 | L2423–2426 | `CheckAltMemAddrMode` → `kIKindSubREa` | `CheckValidAddrMode` + `SubB+offset` | ALTERNATIVE_IMPL |
| 97 | L2462–2465 | `IsValidAddrMode` → `kIKindCmpA` | `CheckValidAddrMode` + cycle calc | ALTERNATIVE_IMPL |
| 98 | L2482–2484 | Direct `kIKindCmpM` | `CheckValidAddrMode` + `CmpB+offset` | ALTERNATIVE_IMPL |
| 99 | L2500–2503 | `CheckDataAltAddrMode` → `kIKindEor` | `CheckValidAddrMode` + cycle calc | ALTERNATIVE_IMPL |
| 100 | L2530–2534 | `IsValidAddrMode` → `kIKindCmp` | `CheckValidAddrMode` + `CmpB+offset` | ALTERNATIVE_IMPL |
| 101 | L2581–2584 | `CheckDataAddrMode` → `kIKindAndEaD` | `CheckValidAddrMode` + cycle calc | ALTERNATIVE_IMPL |
| 102 | L2687–2690 | `CheckAltMemAddrMode` → `kIKindAndDEa` | `CheckValidAddrMode` + cycle calc | ALTERNATIVE_IMPL |

---

### Core/CPU Summary

| Category | Blocks | ~Dead Lines |
|----------|--------|-------------|
| SAFELY_REMOVABLE | 24 | ~175 |
| NOT_YET_ENABLED | 24 | ~160 (+ ~hundreds in cIncludeFPUUnused) |
| DEBUG_LOGGING | 13 | ~75 |
| ALTERNATIVE_IMPL | 25 | ~145 |
| **Total** | **86** (excluding cIncludeFPUUnused detail) | **~555+** |

#### Recommendations

1. **Immediate removal candidates** (SAFELY_REMOVABLE): `ResetPending` dead field and its 3 usage sites (#24, #42, #43); the always-true/false condition functions (#31, #32); `get_fail_realblock` (#19); ATT verification loop (#13); test-hardware & breakpoint checks (#18, #37, #38); `floatx80_abs` and unused constants (#53, #62, #67); all `floatx80_is_unsupported` checks (#54–68); disasm `Skipped` variable (#47, #48, #49).

2. **Keep but document** (NOT_YET_ENABLED): `SmallGlobals` support (embedded target), `EmLocalTalk` (networking), `ExtraAbnormalReports` (diagnostics), `cIncludeFPUUnused` (FPU completeness).

3. **Consider promoting** (DEBUG_LOGGING): The `dbglog_HAVE && 0` blocks could be converted to runtime-toggled logging since `dbglog_HAVE` is already 1.

4. **Safe to remove** (ALTERNATIVE_IMPL): All 20 old decode blocks in `m68k_tables.cpp` (#83–102) — they're superseded by the code immediately following each one. The endian.h alternatives (#1, #2) and `fpu_emdev.h` old FPU_Save (#51) are also safe.
