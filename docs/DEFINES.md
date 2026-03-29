# Preprocessor Defines Survey

**Date:** 2026-03-29
**Scope:** `src/` (excluding `src/macsrc/`)
**Total `#define` directives:** ~853

This document catalogs every `#define` in the maxivmac source tree, grouped by
purpose. Each entry notes the file of origin, the value (or pattern), and a
brief description.

---

## Table of Contents

1. [Build-System & CMake Defines](#1-build-system--cmake-defines)
2. [Feature Flags (Compile-Time)](#2-feature-flags-compile-time)
3. [Debug & Logging Flags](#3-debug--logging-flags)
4. [Machine Model Constants](#4-machine-model-constants)
5. [Hardware Register & Address Constants](#5-hardware-register--address-constants)
6. [Wire Signal Accessors](#6-wire-signal-accessors)
7. [Wire Change-Notification Aliases](#7-wire-change-notification-aliases)
8. [CPU Internals](#8-cpu-internals)
9. [FPU Math Constants & Helpers](#9-fpu-math-constants--helpers)
10. [Memory Access Macros](#10-memory-access-macros)
11. [Screen & Video Constants and Macros](#11-screen--video-constants-and-macros)
12. [Sound Constants](#12-sound-constants)
13. [Timing Constants](#13-timing-constants)
14. [Platform / OS-Glue Configuration](#14-platform--os-glue-configuration)
15. [UI / Control Mode Constants](#15-ui--control-mode-constants)
16. [String Constants (Localization)](#16-string-constants-localization)
17. [Sony (Disk) Driver Constants](#17-sony-disk-driver-constants)
18. [Extension Interface Constants](#18-extension-interface-constants)
19. [Utility Macros](#19-utility-macros)
20. [Include Guards](#20-include-guards)

---

## 1. Build-System & CMake Defines

These are injected by CMake via `target_compile_definitions` in `CMakeLists.txt`
or set in release flags. They are the top-level knobs that control what code is
compiled.

| Define | Value | Source | Description |
|--------|-------|--------|-------------|
| `MAXIVMAC_VERSION` | git tag string | CMake `-D` | Version string from `git describe`. Falls back to `"dev-unknown"` in `platform_config.h`. |
| `WantAbnormalReports` | `0` or `1` | CMake option `MINIVMAC_ABNORMAL_REPORTS` | Enables runtime abnormal-situation reporting (guest doing unexpected things). |
| `EmLocalTalk` | `0` or `1` | CMake option `MINIVMAC_LOCALTALK` | Enables LocalTalk networking emulation. |
| `dbglog_HAVE` | `1` | CMake (hardcoded) | Enables the debug-log subsystem (write trace data to file/stderr). |
| `NDEBUG` | (defined) | CMake release flags `-Os -DNDEBUG` | Standard C/C++ release-mode flag; disables `assert()`. |

---

## 2. Feature Flags (Compile-Time)

Defined in `src/core/emulation_config.h` (the canonical source) and
the legacy `src/config/CNFUDPIC.h`. These control which emulation features
are compiled in.

### CPU & Coprocessor

| Define | Value | File | Description |
|--------|-------|------|-------------|
| `USE_68020` | `1` | `emulation_config.h` | Compile 68020 instruction support. Runtime selection via `MachineConfig::use68020`. |
| `EM_FPU` | `1` | `emulation_config.h` | Compile FPU (68881/68882) emulation code. |
| `EM_MMU` | `1` | `emulation_config.h` | Compile MMU (68851/built-in) emulation code. |
| `WANT_CYC_BY_PRI_OP` | `1` | `emulation_config.h` | Track cycle counts per opcode for accurate timing. |
| `WANT_CLOSER_CYC` | `1` | `emulation_config.h` | Use more precise cycle counting (per `DecOpR`). |
| `WANT_DISASM` | `1` | `emulation_config.h` | Compile the built-in disassembler (`disasm.cpp`). |

### Extensions

| Define | Value | File | Description |
|--------|-------|------|-------------|
| `INCLUDE_EXTN_PBUFS` | `1` | `emulation_config.h` | Include parameter-buffer extension (host↔guest data transfer). |
| `INCLUDE_EXTN_HOST_TEXT_CLIP_EXCHANGE` | `1` | `emulation_config.h` | Include host clipboard exchange extension. |

### Sony (Disk) Support

| Define | Value | File | Description |
|--------|-------|------|-------------|
| `SONY_SUPPORT_DC42` | `1` | `emulation_config.h` | Support DiskCopy 4.2 disk image format. |
| `SONY_SUPPORT_TAGS` | `1` | `emulation_config.h` | Support tag data in disk images. |
| `SONY_WANT_CHECKSUMS_UPDATED` | `1` | `emulation_config.h` | Update DC42 checksums on disk writes. |

---

## 3. Debug & Logging Flags

Per-device and per-subsystem debug logging switches. All follow the pattern
`(dbglog_HAVE && 0)` — compiled to `0` (disabled) by default but trivially
enabled by changing the `0` to `1`.

| Define | Value | File | Description |
|--------|-------|------|-------------|
| `dbglog_HAVE` | `1` | CMake | Master switch: debug logging subsystem available. |
| `SCC_dolog` | `(dbglog_HAVE && 0)` | `scc.cpp` | SCC serial chip debug tracing. |
| `SCC_TrackMore` | `0` | `scc.cpp` | Extra-verbose SCC state tracking. |
| `ASC_dolog` | `(dbglog_HAVE && 0)` | `asc.cpp` | Apple Sound Chip debug tracing. |
| `IWM_dolog` | `(dbglog_HAVE && 0)` | `iwm.cpp` | IWM floppy controller debug tracing. |
| `Sony_dolog` | `(dbglog_HAVE && 0)` | `sony.cpp` | Sony disk driver debug tracing. |
| `VID_dolog` | `(dbglog_HAVE && 0)` | `video.cpp` | Video device debug tracing. |
| `m68k_logExceptions` | `(dbglog_HAVE && 0)` | `m68k.cpp` | CPU exception debug tracing. |
| `dbglog_SoundStuff` | `(0 && dbglog_HAVE)` | `sdl_sound.cpp` | Sound subsystem debug tracing. |
| `dbglog_SoundBuffStats` | `(0 && dbglog_HAVE)` | `sdl_sound.cpp` | Sound buffer statistics logging. |
| `DBGLOG_OSG_INIT` | `(0 && dbglog_HAVE)` | `sdl.cpp`, `sdl_sound.cpp`, `tick_timer.cpp` | OS-glue initialization step logging. |
| `dbglog_TimeStuff` | `(0 && dbglog_HAVE)` | `tick_timer.cpp` | Timer/timing debug tracing. |
| `dbglog_ToStdErr` | `0` | `dbglog_platform.cpp` | Send debug log to stderr instead of file. |
| `dbglog_ToSDL_Log` | `0` | `dbglog_platform.cpp` | Send debug log via `SDL_Log()`. |
| `DisasmIncludeCycles` | `0` | `disasm.cpp` | Include cycle counts in disassembly output. |
| `WantDumpTable` | `0` | `m68k.cpp` | Dump opcode dispatch frequency table. |
| `WantBreakPoint` | `0` | `m68k.cpp` | Enable CPU breakpoint support. |
| `BreakPointAddress` | `0xD198` | `m68k.cpp` | Address for CPU breakpoint (when `WantBreakPoint` = 1). |
| `WantDumpAJump` | `0` | `m68k.cpp` | Log absolute jumps for debugging. |

---

## 4. Machine Model Constants

Defined in `src/core/machine.h`. Each constant identifies a supported Mac model
in the emulator's model-selection system.

| Define | Value | Description |
|--------|-------|-------------|
| `kEmMd_Twig43` | `0` | Macintosh prototype (Twiggy 4.3) |
| `kEmMd_Twiggy` | `1` | Macintosh prototype (Twiggy) |
| `kEmMd_128K` | `2` | Macintosh 128K |
| `kEmMd_512Ke` | `3` | Macintosh 512Ke |
| `kEmMd_Kanji` | `4` | Macintosh Plus (Kanji ROM variant) |
| `kEmMd_Plus` | `5` | Macintosh Plus |
| `kEmMd_SE` | `6` | Macintosh SE |
| `kEmMd_SEFDHD` | `7` | Macintosh SE FDHD |
| `kEmMd_Classic` | `8` | Macintosh Classic |
| `kEmMd_PB100` | `9` | PowerBook 100 |
| `kEmMd_II` | `10` | Macintosh II |
| `kEmMd_IIx` | `11` | Macintosh IIx |

---

## 5. Hardware Register & Address Constants

### Memory Map (machine.cpp)

| Define | Value | File | Description |
|--------|-------|------|-------------|
| `kRAM_Overlay_Top` | `0x00800000` | `machine.cpp` | Top of RAM overlay region (boot-time ROM mirroring). |
| `kSCCWr_Block_Top` | `0x00C00000` | `machine.cpp` | Top of SCC write-register address block. |
| `kROM_cmpmask` | `0x00F00000 \| ...` | `machine.cpp` | Comparison mask for ROM address decoding. |
| `RAMSafetyMarginFudge` | `4` | `machine.h` | Extra bytes allocated past RAM end for safety. |

### IWM Floppy Registers (iwm.cpp)

| Define | Value | Description |
|--------|-------|-------------|
| `kph0L` – `kq7H` | `0x00` – `0x0F` | IWM register addresses (CA0/1/2, LSTRB, motor, drive, Q6, Q7). |
| `kph0` – `kq7` | `0x01` – `0x80` | IWM register bitmasks (for combined state byte). |

### SCSI Registers (scsi.cpp)

| Define | Value | Description |
|--------|-------|-------------|
| `scsiRd` / `scsiWr` | `0x00` / `0x01` | SCSI read/write direction flags. |
| `sCDR` | `0x00` | Current SCSI Data Register (read-only). |
| `sODR` | `0x00` | Output Data Register (write-only). |
| `sICR` | `0x02` | Initiator Command Register (r/w). |
| `sMR` | `0x04` | Mode Register (r/w). |
| `sTCR` | `0x06` | Target Command Register (r/w). |
| `sCSR` | `0x08` | Current SCSI Bus Status (read-only). |
| `sSER` | `0x08` | Select Enable Register (write-only). |
| `sBSR` | `0x0A` | Bus and Status Register (read-only). |
| `sDMAtx` | `0x0A` | Start DMA Send (write-only). |
| `sIDR` | `0x0C` | Input Data Register (read-only). |
| `sTDMArx` | `0x0C` | Start DMA Target Receive (write-only). |
| `sRESET` | `0x0E` | Reset Parity/Interrupt (read-only). |
| `sIDMArx` | `0x0E` | Start DMA Initiator Receive (write-only). |

### SCC Interrupt Vectors (scc.cpp)

| Define | Value | Description |
|--------|-------|-------------|
| `SCC_A_Rx` | `8` | Channel A Rx Char Available. |
| `SCC_A_Rx_Spec` | `7` | Channel A Rx Special Condition. |
| `SCC_A_Tx_Empty` | `6` | Channel A Tx Buffer Empty. |
| `SCC_A_Ext` | `5` | Channel A External/Status Change. |
| `SCC_B_Rx` | `4` | Channel B Rx Char Available. |
| `SCC_B_Rx_Spec` | `3` | Channel B Rx Special Condition. |
| `SCC_B_Tx_Empty` | `2` | Channel B Tx Buffer Empty. |
| `SCC_B_Ext` | `1` | Channel B External/Status Change. |

### RTC / PRAM Constants (rtc.cpp)

| Define | Value | Description |
|--------|-------|-------------|
| `PARAMRAMSize` | `256` | Size of parameter RAM in bytes. |
| `Group1Base` | `0x10` | RTC Group 1 base address. |
| `Group2Base` | `0x08` | RTC Group 2 base address. |
| `RTCinitPRAM` | `1` | Initialize PRAM with defaults on startup. |
| `TrackSpeed` | `0` | Default mouse tracking speed. |
| `AlarmOn` | `0` | Default alarm state (off). |
| `DiskCacheSz` | model-dependent | Disk cache size (1 for II family, 4 otherwise). |
| `StartUpDisk` | `0` | Default startup disk preference. |
| `DiskCacheOn` | `0` | Disk cache enabled state. |
| `MouseScalingOn` | `0` | Mouse scaling state. |
| `SpeakerVol` | `0x07` | Default speaker volume. |
| `MenuBlink` | `0x03` | Menu blink count. |
| `AutoKeyThresh` | `0x06` | Auto-key threshold. |
| `AutoKeyRate` | `0x03` | Auto-key repeat rate. |
| `pr_HilColRed/Green/Blue` | `0x0000` | Default highlight color (black). |
| `CaretBlinkTime` | model-dependent | Caret blink interval. |
| `DoubleClickTime` | model-dependent | Double-click interval. |
| `prb_*` | computed | Packed PRAM byte values (composites of the above constants). |
| `pr_HilCol*Hi/Lo` | shifted | High/low bytes of highlight color components. |

### Video Device Constants (video.cpp)

| Define | Value | Description |
|--------|-------|-------------|
| `kCmndVideoFeatures` | `1` | Video extension command: query features. |
| `CntrlParam_csCode` | `0x1A` | Control/status code offset in parameter block. |
| `CntrlParam_csParam` | `0x1C` | Parameters offset in parameter block. |
| `VDPageInfo_csMode` | `0` | Video mode field offset. |
| `VDPageInfo_csData` | `2` | Video data field offset. |
| `VDPageInfo_csPage` | `6` | Video page field offset. |
| `VDPageInfo_csBaseAddr` | `8` | Video base address field offset. |
| `VDSetEntryRecord_*` | various | CLUT set-entry record field offsets. |
| `VDGammaRecord_csGTable` | `0` | Gamma table field offset. |
| `VidBaseAddr` | `0xF9900000` | Mac II video base address. |

### Screen Constants

| Define | Value | File | Description |
|--------|-------|------|-------------|
| `kAlternate_Offset` | `0xD900` | `screen.cpp` | Offset to alternate screen buffer. |
| `kSnd_Alt_Offset` | `0x5F00` | `sound.cpp` | Offset to alternate sound buffer. |

### ADB

| Define | Value | File | Description |
|--------|-------|------|-------------|
| `ADB_MaxSzDatBuf` | `8` | `adb_shared.h` | Maximum ADB data buffer size. |

### Keyboard

| Define | Value | File | Description |
|--------|-------|------|-------------|
| `MAX_KEYBOARD_WAIT` | `16` | `keyboard.cpp` | Max keyboard wait time (1/60th second units). |

---

## 6. Wire Signal Accessors

Defined in `src/core/wire_macros.h`. These `#define` aliases map symbolic signal
names to indexed reads from the global `g_wiresData[]` array. They provide
backward-compatible access to the `WireBus` signal state.

### VIA1 Port A Lines

| Define | Maps to |
|--------|---------|
| `VIA1_iA0` – `VIA1_iA7` | `g_wiresData[Wire_VIA1_iA0]` – `…iA7` |

### VIA1 Port B Lines

| Define | Maps to |
|--------|---------|
| `VIA1_iB0` – `VIA1_iB7` | `g_wiresData[Wire_VIA1_iB0]` – `…iB7` |

### VIA1 Control

| Define | Maps to |
|--------|---------|
| `VIA1_iCB2` | `g_wiresData[Wire_VIA1_iCB2]` |

### VIA2 Lines (Mac II family)

| Define | Maps to |
|--------|---------|
| `VIA2_iA0`, `VIA2_iA6`, `VIA2_iA7` | `g_wiresData[Wire_VIA2_iA*]` |
| `VIA2_iB2`, `VIA2_iB3`, `VIA2_iB7` | `g_wiresData[Wire_VIA2_iB*]` |
| `VIA2_iCB2` | `g_wiresData[Wire_VIA2_iCB2]` |

### Named Signal Aliases

| Define | Wire | Description |
|--------|------|-------------|
| `SoundDisable` | `Wire_SoundDisable` | Sound output enable/disable. |
| `SoundVolb0` – `SoundVolb2` | `Wire_SoundVolb*` | Sound volume bits. |
| `MemOverlay` | `Wire_MemOverlay` | ROM overlay active (boot mode). |
| `IWMvSel` | `Wire_VIA1_iA5` | IWM drive select. |
| `SCCwaitrq` | `Wire_VIA1_iA7` | SCC wait/request. |
| `RTCdataLine` | `Wire_VIA1_iB0` | RTC serial data. |
| `RTCclock` | `Wire_VIA1_iB1` | RTC serial clock. |
| `RTCunEnabled` | `Wire_VIA1_iB2` | RTC chip-select (active low). |
| `ADB_Int` | `Wire_VIA1_iB3` | ADB interrupt. |
| `ADB_st0`, `ADB_st1` | `Wire_VIA1_iB4/5` | ADB state bits. |
| `ADB_Data` | `Wire_VIA1_iCB2` | ADB data line. |
| `Addr32` | `Wire_VIA2_iB3` | 32-bit addressing mode. |

### Interrupt Request Lines

| Define | Wire | Description |
|--------|------|-------------|
| `VIA1_InterruptRequest` | `Wire_VIA1_InterruptRequest` | VIA1 interrupt pending. |
| `VIA2_InterruptRequest` | `Wire_VIA2_InterruptRequest` | VIA2 interrupt pending. |
| `SCCInterruptRequest` | `Wire_SCCInterruptRequest` | SCC interrupt pending. |
| `ADBMouseDisabled` | `Wire_ADBMouseDisabled` | ADB mouse disabled flag. |
| `Vid_VBLinterrupt` | `Wire_VBLinterrupt` | Video VBL interrupt. |
| `Vid_VBLintunenbl` | `Wire_VBLintunenbl` | VBL interrupt enable. |

---

## 7. Wire Change-Notification Aliases

Also in `src/core/wire_macros.h`. These map VIA pin-change callbacks to the
actual handler functions.

| Define | Maps to | Description |
|--------|---------|-------------|
| `VIA1_iA4_ChangeNtfy` | `MemOverlay_ChangeNtfy` | Overlay bit changed → rebuild ATT. |
| `VIA2_iA7_ChangeNtfy` | `Addr32_ChangeNtfy` | 32-bit mode changed → rebuild ATT. |
| `VIA2_iA6_ChangeNtfy` | `Addr32_ChangeNtfy` | 32-bit mode changed → rebuild ATT. |
| `VIA2_iB2_ChangeNtfy` | `PowerOff_ChangeNtfy` | Power-off request. |
| `VIA2_iB3_ChangeNtfy` | `Addr32_ChangeNtfy` | 32-bit mode changed → rebuild ATT. |
| `VIA2_interruptChngNtfy` | `VIAorSCCinterruptChngNtfy` | VIA2 interrupt state changed. |
| `VIA1_interruptChngNtfy` | `VIAorSCCinterruptChngNtfy` | VIA1 interrupt state changed. |
| `SCCinterruptChngNtfy` | `VIAorSCCinterruptChngNtfy` | SCC interrupt state changed. |

---

## 8. CPU Internals

Defined in `src/cpu/m68k.cpp` and `src/cpu/m68k_tables.cpp`. These are internal
to the 68000 emulator and largely inherited from the original minivmac.

### Register Access

| Define | Expansion | Description |
|--------|-----------|-------------|
| `V_regs` | `regs` | CPU register struct access. |
| `V_pc_p` | `V_regs.pc_p` | Program counter pointer. |
| `V_MaxCyclesToGo` | `V_regs.MaxCyclesToGo` | Remaining cycles in current slice. |
| `V_pc_pHi` | `V_regs.pc_pHi` | Upper bound of current PC block. |

### CPU Status Flag Shortcuts

| Define | Expansion | Description |
|--------|-----------|-------------|
| `ZFLG` | `V_regs.z` | Zero flag. |
| `NFLG` | `V_regs.n` | Negative flag. |
| `CFLG` | `V_regs.c` | Carry flag. |
| `VFLG` | `V_regs.v` | Overflow flag. |
| `XFLG` | `V_regs.x` | Extend flag. |
| `m68k_dreg(num)` | `V_regs.regs[(num)]` | Data register access. |
| `m68k_areg(num)` | `V_regs.regs[(num) + 8]` | Address register access. |

### Opcode Decode Helpers

| Define | File | Description |
|--------|------|-------------|
| `b76(p)` / `Disasm_b76` | `m68k_tables.cpp` / `disasm.cpp` | Bits 7-6 of opcode. |
| `b8(p)` / `Disasm_b8` | `m68k_tables.cpp` / `disasm.cpp` | Bit 8 of opcode. |
| `mode(p)` / `Disasm_mode` | `m68k_tables.cpp` / `disasm.cpp` | Addressing mode field (bits 5-3). |
| `reg(p)` / `Disasm_reg` | `m68k_tables.cpp` / `disasm.cpp` | Register field (bits 2-0). |
| `md6(p)` / `Disasm_md6` | `m68k_tables.cpp` / `disasm.cpp` | Mode field at bits 8-6. |
| `rg9(p)` / `Disasm_rg9` | `m68k_tables.cpp` / `disasm.cpp` | Register field at bits 11-9. |

### Address Validation Masks (m68k_tables.cpp)

| Define | Value | Description |
|--------|-------|-------------|
| `kAddrValidMaskAny` | `(1 << 0)` | Any addressing mode. |
| `kAddrValidMaskData` | `(1 << 1)` | Data addressing modes. |
| `kAddrValidMaskDataAlt` | `(1 << 2)` | Data alterable modes. |
| `kAddrValidMaskControl` | `(1 << 3)` | Control addressing modes. |
| `kAddrValidMaskControlAlt` | `(1 << 4)` | Control alterable modes. |
| `kAddrValidMaskAltMem` | `(1 << 5)` | Alterable memory modes. |
| `kAddrValidMaskDataNoCn` | `(1 << 6)` | Data modes excluding control. |
| `CheckInSet(v, m)` | `(0 != ((1 << (v)) & (m)))` | Test if addressing mode `v` is in set `m`. |
| `kMyAvgCycPerInstr` | `10 * kCycleScale + ...` | Average cycles per instruction estimate. |

### Decode Table Access (m68k_tables.h)

| Define | Description |
|--------|-------------|
| `GetDcoCycles(p)` | Get cycle count from decoded opcode. |
| `SetDcoMainClas(p, xx)` | Set main class in decoded opcode. |
| `SetDcoCycles(p, xx)` | Set cycle count in decoded opcode. |

### Miscellaneous CPU

| Define | Value | Description |
|--------|-------|-------------|
| `USE_PCLIMIT` | `1` | Use PC limit checking for block boundaries. |
| `AKMemory` / `AKRegister` | `0` / `1` | Argument-kind constants (memory vs register). |
| `disp_table_sz` | `256 * 256` | Size of opcode dispatch table (65536 entries). |
| `ui5r_MSBisSet(x)` | sign test | Test if MSB of 32-bit value is set. |
| `Bool2Bit(x)` | ternary | Convert boolean to 0/1 integer. |
| `Ui5rASR(x, s)` | arithmetic shift right | Portable arithmetic shift right. |
| `CCdispSz` | `16 * kNumLazyFlagsKinds` or `16` | Condition-code dispatch table size. |
| `ui5b_lo(x)` / `ui5b_hi(x)` | mask/shift | Extract low/high 16 bits of 32-bit value. |
| `MoveAvgN` | `0` or `3` | Average extra MOVE cycles (by cycle precision). |
| `Ln2SavedPCs` / `NumSavedPCs` / `SavedPCsMask` | `4` / `16` / `15` | PC history ring buffer (disasm.cpp). |
| `Em_Enter` / `Em_Exit` | no-op | Entry/exit hooks (no-op since global-register mode removed). |
| `NeedDefaultLazyAllFlags` | `NeedDefaultLazyAllFlags0` | Lazy flag setup. |
| `HaveSetUpFlags` | no-op | Flag setup acknowledgment (no-op). |
| `WillSetZFLG` | alias to `NeedDefaultLazyAllFlags` | Signal that Z flag will be set. |
| `DoCodeBccB_t` / `DoCodeBccW_t` | `DoCodeBraB` / `SkipiWord` | Branch-taken aliases. |
| `DoCodeBccW_f` / `DoCodeDBcc_t` | `SkipiWord` | Branch-not-taken / DBcc-true aliases. |
| `LocalMemAccessNtfy` | `MemAccessNtfy` | Memory-access notification (direct alias since global-register mode removed). |
| `LocalMMDV_Access` | `MMDV_Access` | Memory-mapped device access (direct alias since global-register mode removed). |

---

## 9. FPU Math Constants & Helpers

Defined in `src/cpu/fpu_math.h`. These are mathematical constants and
configuration for the SoftFloat-based FPU emulator.

### Configuration

| Define | Value | Description |
|--------|-------|-------------|
| `FLOATX80` | (defined) | Enable 80-bit extended precision type. |
| `FLOAT128` | (defined) | Enable 128-bit quad precision type. |
| `HaveUi5to6Mul` | `1` | 32→64 bit multiply available. |
| `HaveUi6Div` | `0` | 64-bit divide not natively available. |
| `Ui6Div(x, y)` | `(x) / (y)` | 64-bit division (software fallback). |
| `USE_estimateDiv128To64` | (defined) | Use estimated 128÷64 division. |

### NaN Defaults

| Define | Value | Description |
|--------|-------|-------------|
| `floatx80_default_nan_high` | `0xFFFF` | Default NaN exponent (80-bit). |
| `floatx80_default_nan_low` | `0xC000000000000000` | Default NaN significand (80-bit). |
| `floatx80_default_nan_exp` | `0xFFFF` | Alternate NaN exponent constant. |
| `floatx80_default_nan_fraction` | `0xC000000000000000` | Alternate NaN fraction constant. |
| `float128_default_nan_high/low` | `0xFFFF800…` / `0x0` | Default NaN for 128-bit. |

### Packing/Unpacking

| Define | Description |
|--------|-------------|
| `packFloatx80m(zSign, zExp, zSig)` | Pack 80-bit float components into struct. |
| `packFloat2x128m(zHi, zLo)` | Pack 128-bit float halves into struct. |
| `PACK_FLOAT_128(hi, lo)` | Pack 128-bit float from hex literals. |

### Mathematical Constants

| Define | Value | Description |
|--------|-------|-------------|
| `EXP_BIAS` | `0x3FFF` | Extended-precision exponent bias. |
| `int16_indefinite` | `0x8000` | Indefinite value for int16 conversion. |
| `float_flag_denormal` | `0x02` | Denormal exception flag. |
| `FLOATX80_PI_EXP` | `0x4000` | Pi exponent. |
| `FLOAT_PI_HI/LO` | 64-bit significand | Pi significand (high/low precision). |
| `FLOATX80_PI2_EXP` | `0x3FFF` | Pi/2 exponent. |
| `FLOATX80_PI4_EXP` | `0x3FFE` | Pi/4 exponent. |
| `FLOATX80_3PI4_EXP` | `0x4000` | 3*Pi/4 exponent. |
| `FLOAT_3PI4_HI/LO` | 64-bit | 3*Pi/4 significand. |
| `FLOAT_LN2INV_EXP` | `0x3FFF` | 1/ln(2) exponent. |
| `FLOAT_LN2INV_HI/LO` | 64-bit | 1/ln(2) significand. |
| `SQRT2_HALF_SIG` | 64-bit | sqrt(2)/2 significand. |
| `LN2_SIG` | 64-bit | ln(2) significand. |

### Table Sizes

| Define | Value | Description |
|--------|-------|-------------|
| `L2_ARR_SIZE` | `9` | Log base 2 polynomial approximation terms. |
| `EXP_ARR_SIZE` | `15` | Exponential polynomial approximation terms. |
| `SIN_ARR_SIZE` | `9` | Sine polynomial approximation terms. |
| `COS_ARR_SIZE` | `9` | Cosine polynomial approximation terms. |
| `FPATAN_ARR_SIZE` | `11` | Arctangent polynomial approximation terms. |

---

## 10. Memory Access Macros

### RAM Access (machine.h)

Two variants depending on whether `ln2mtb` (log2 of memory translation block) is
defined. When undefined (normal case), direct pointer arithmetic is used:

| Define | Expansion | Description |
|--------|-----------|-------------|
| `get_ram_byte(addr)` | `do_get_mem_byte((addr) + g_ram)` | Read byte from RAM. |
| `get_ram_word(addr)` | `do_get_mem_word((addr) + g_ram)` | Read word from RAM. |
| `get_ram_long(addr)` | `do_get_mem_long((addr) + g_ram)` | Read long from RAM. |
| `put_ram_byte(addr, b)` | `do_put_mem_byte((addr) + g_ram, (b))` | Write byte to RAM. |
| `put_ram_word(addr, w)` | `do_put_mem_word((addr) + g_ram, (w))` | Write word to RAM. |
| `put_ram_long(addr, l)` | `do_put_mem_long((addr) + g_ram, (l))` | Write long to RAM. |
| `get_ram_address(addr)` | `((addr) + g_ram)` | Get host pointer for RAM address. |

When `ln2mtb` is defined, these redirect to the full virtual-memory path (`get_vm_byte`, etc.).

### Type Conversion (machine.h)

| Define | Description |
|--------|-------------|
| `ui5r_FromUByte(x)` | Cast to uint32_t from uint8_t. |
| `ui5r_FromUWord(x)` | Cast to uint32_t from uint16_t. |
| `ui5r_FromULong(x)` | Cast to uint32_t from uint32_t. |

### ATT Management (machine.cpp)

| Define | Condition | Description |
|--------|-----------|-------------|
| `AddToATTListWithMTB` | → `AddToATTList` | When `ln2mtb` undefined, no MTB overhead. |

---

## 11. Screen & Video Constants and Macros

### Runtime Screen Dimensions (platform.h)

These are macros wrapping runtime globals set from `MachineConfig` at startup:

| Define | Expansion | Description |
|--------|-----------|-------------|
| `vMacScreenWidth` | `(long)g_screenWidth` | Emulated screen width in pixels. |
| `vMacScreenHeight` | `(long)g_screenHeight` | Emulated screen height in pixels. |
| `vMacScreenDepth` | `(int)g_screenDepth` | Emulated screen bit depth. |
| `vMacScreenNumPixels` | `height * width` | Total pixel count. |
| `vMacScreenNumBits` | `pixels << depth` | Total bits in framebuffer. |
| `vMacScreenNumBytes` | `bits / 8` | Total bytes in framebuffer. |
| `vMacScreenBitWidth` | `width << depth` | Bit width per scanline. |
| `vMacScreenByteWidth` | `bitWidth / 8` | Byte width per scanline. |
| `vMacScreenMonoNumBytes` | `pixels / 8` | Mono framebuffer size. |
| `vMacScreenMonoByteWidth` | `width / 8` | Mono scanline byte width. |
| `CLUT_size` | `256` | Color lookup table size. |

### Screen Comparison (osglu_common.cpp)

| Define | Value | Description |
|--------|-------|-------------|
| `uibb` / `uibr` | `uint32_t` or `uint8_t` | Block-comparison types (depends on alignment support). |
| `ln2uiblockn` | `2` or `0` | Log2 of block size for screen comparison. |
| `uiblockn` | `1 << ln2uiblockn` | Block size in bytes. |
| `ln2uiblockbitsn` | `3 + ln2uiblockn` | Log2 of block size in bits. |
| `uiblockbitsn` | `8 * uiblockn` | Block size in bits. |
| `FlipCheckMonoBits` | `uiblockbitsn - 1` or `7` | Bit mask for mono pixel dirty check. |
| `FlipCheckBits` | `FlipCheckMonoBits >> depth` | Bit mask for current-depth dirty check. |

### Screen Mapper Template (screen_map.h)

This header is a C-preprocessor template — included multiple times with
different parameter defines to generate pixel-format conversion functions.

**Required parameters (must be `#define`d before including):**

| Define | Description |
|--------|-------------|
| `ScrnMapr_DoMap` | Name of the generated function. |
| `ScrnMapr_Src` | Source buffer pointer. |
| `ScrnMapr_Dst` | Destination buffer pointer. |
| `ScrnMapr_SrcDepth` | Source pixel depth (0–3). |
| `ScrnMapr_DstDepth` | Destination pixel depth (≥ SrcDepth). |
| `ScrnMapr_Map` | Color lookup table pointer. |

**Optional/computed parameters:**

| Define | Default/Value | Description |
|--------|---------------|-------------|
| `ScrnMapr_Scale` | `1` | Scale factor. |
| `ScrnMapr_MapElSz` | `Scale << (DstDepth - SrcDepth)` | Map element size in bytes. |
| `ScrnMapr_TranT` | `uint32_t`/`uint16_t`/`uint8_t` | Transfer type (chosen by alignment). |
| `ScrnMapr_TranLn2Sz` | `2`/`1`/`0` | Log2 of transfer type size. |
| `ScrnMapr_TranN` | `MapElSz >> TranLn2Sz` | Transfers per map element. |
| `ScrnMapr_ScrnWB` | `width >> (3 - SrcDepth)` | Source scanline width in bytes. |

**Instance parameters (screen_map_inst.h):**

| Define | Value | Description |
|--------|-------|-------------|
| `ScrnMapr_Src` | `GetCurDrawBuff()` | Source: current draw buffer. |
| `ScrnMapr_Dst` | `ScalingBuff` | Destination: scaling buffer. |
| `ScrnMapr_Map` | `CLUT_final` | Map: final CLUT. |

**Generated functions (sdl.cpp):**

The screen mapper template is instantiated 12 times for all
source-depth → destination-depth combinations:

| Function | SrcDepth | DstDepth | Description |
|----------|----------|----------|-------------|
| `UpdateBWDepth3Copy` | 0 | 3 | B&W → 8bpp |
| `UpdateBWDepth4Copy` | 0 | 4 | B&W → 16bpp |
| `UpdateBWDepth5Copy` | 0 | 5 | B&W → 32bpp |
| `UpdateColorSrc1Dst3Copy` | 1 | 3 | 2-bit → 8bpp |
| `UpdateColorSrc1Dst4Copy` | 1 | 4 | 2-bit → 16bpp |
| `UpdateColorSrc1Dst5Copy` | 1 | 5 | 2-bit → 32bpp |
| `UpdateColorSrc2Dst3Copy` | 2 | 3 | 4-bit → 8bpp |
| `UpdateColorSrc2Dst4Copy` | 2 | 4 | 4-bit → 16bpp |
| `UpdateColorSrc2Dst5Copy` | 2 | 5 | 4-bit → 32bpp |
| `UpdateColorSrc3Dst3Copy` | 3 | 3 | 8-bit → 8bpp |
| `UpdateColorSrc3Dst4Copy` | 3 | 4 | 8-bit → 16bpp |
| `UpdateColorSrc3Dst5Copy` | 3 | 5 | 8-bit → 32bpp |

### Screen Translate Template (screen_translate.h)

| Define | Default | Description |
|--------|---------|-------------|
| `ScrnTrns_Scale` | `1` | Translation scale factor. |
| `ScrnTrns_DstZLo` | `0` | Destination zero-level low byte. |

### CLUT Size (sdl.cpp)

| Define | Value | Description |
|--------|-------|-------------|
| `CLUT_FINAL_SZ` | `256 * 8 * 4` | Final CLUT buffer size (all depths × 4 bytes). |

### Color Validity

| Define | Value | Description |
|--------|-------|-------------|
| `WantColorTransValid` | `1` | Track whether color translation tables are valid. |

---

## 12. Sound Constants

Defined in `src/platform/sdl_sound.cpp`.

### Buffer Architecture

| Define | Value | Description |
|--------|-------|-------------|
| `kLn2SoundBuffers` | `4` | Log2 of sound buffer count (16 buffers). |
| `kSoundBuffers` | `16` | Number of sound buffers (ring). |
| `kSoundBuffMask` | `15` | Mask for ring-buffer indexing. |
| `DesiredMinFilledSoundBuffs` | `3` | Target minimum filled buffers (latency vs. underrun). |
| `kLnOneBuffLen` | `9` | Log2 of one buffer length (512 samples). |
| `kLnAllBuffLen` | `13` | Log2 of total buffer length. |
| `kOneBuffLen` | `512` | One buffer in samples. |
| `kAllBuffLen` | `8192` | All buffers in samples. |
| `kLnOneBuffSz` | `10` | Log2 of one buffer size in bytes. |
| `kLnAllBuffSz` | `14` | Log2 of all buffers size in bytes. |
| `kOneBuffSz` | `1024` | One buffer in bytes (16-bit samples). |
| `kAllBuffSz` | `16384` | All buffers in bytes. |
| `kOneBuffMask` | `511` | Mask for within-buffer indexing. |
| `kAllBuffMask` | `8191` | Mask for cross-buffer indexing. |
| `dbhBufferSize` | `kAllBuffSz + kOneBuffSz` | Total DMA host buffer size. |

### Sample Format

| Define | Value | Description |
|--------|-------|-------------|
| `SOUND_SAMPLERATE` | `22255` | Output sample rate (= 7833600 × 2 / 704). |
| `kCenterSound` | `0x8000` | Center value for unsigned 16-bit audio. |
| `K_CENTER_TEMP_SOUND` | `0x8000` | Temp center value (same). |
| `AUDIO_STEP_VAL` | `0x0040` | Step value for volume ramping. |
| `CONVERT_TEMP_SOUND_SAMPLE_FROM_NATIVE(v)` | `v + kCenterSound` | Native → unsigned offset. |
| `CONVERT_TEMP_SOUND_SAMPLE_TO_NATIVE(v)` | `v - kCenterSound` | Unsigned offset → native. |

---

## 13. Timing Constants

### Cycle Scaling (machine.h)

| Define | Value | Description |
|--------|-------|-------------|
| `kLn2CycleScale` | `6` | Log2 of cycle scale factor. |
| `kCycleScale` | `64` | Internal cycles per hardware cycle (precision multiplier). |
| `RdAvgXtraCyc` | `kCycleScale + kCycleScale/4` | Average extra cycles per read (80). Only when `WANT_CYC_BY_PRI_OP`. |
| `WrAvgXtraCyc` | `kCycleScale + kCycleScale/4` | Average extra cycles per write (80). Only when `WANT_CYC_BY_PRI_OP`. |
| `kNumSubTicks` | `16` | Sub-ticks per tick (determines interrupt delivery granularity). |

### Tick Timer (main.cpp)

| Define | Value | Description |
|--------|-------|-------------|
| `CyclesScaledPerTick` | `130240 * clockMult * kCycleScale` | Total scaled cycles per 1/60.15s tick. |
| `CyclesScaledPerSubTick` | `CyclesScaledPerTick / kNumSubTicks` | Scaled cycles per sub-tick. |

### VIA Timer (via_base.cpp)

| Define | Value | Description |
|--------|-------|-------------|
| `CYCLES_PER_VIA_TIME` | `10 * clockMult` | CPU cycles per VIA timer tick. |
| `CYCLES_SCALED_PER_VIA_TIME` | `kCycleScale * CYCLES_PER_VIA_TIME` | Scaled cycles per VIA timer tick. |

### Platform Tick Timer (tick_timer.cpp)

| Define | Value | Description |
|--------|-------|-------------|
| `MyInvTimeDivPow` | `16` | Precision bits for timing division. |
| `MyInvTimeDiv` | `65536` | Fixed-point divisor for timing. |
| `MyInvTimeDivMask` | `65535` | Mask for timing fractional part. |
| `MyInvTimeStep` | `1089590` | Step per tick (≈ 1000/60.15 × 65536). |

---

## 14. Platform / OS-Glue Configuration

### SDL Backend (sdl_config.h)

| Define | Value | Description |
|--------|-------|-------------|
| `EnableDragDrop` | `1` | Enable drag-and-drop disk image mounting. |
| `kStrAppName` | `"Maxi vMac"` | Display name of the application. |
| `kAppVariationStr` | `"maxivmac-sdl"` | Variation identifier string. |
| `kStrCopyrightYear` | `"2026"` | Copyright year. |
| `kMaintainerName` | `"Fred Stark"` | Current maintainer. |
| `kStrHomePage` | `"https://github.com/fstark/maxivmac"` | Project URL. |

### Platform Config (platform_config.h)

| Define | Value | Description |
|--------|-------|-------------|
| `SaveDialogEnable` | `1` | Enable native save dialogs. |
| `WantEnblCtrlInt` | `1` | Enable Control-Mode interrupt command. |
| `WantEnblCtrlRst` | `1` | Enable Control-Mode reset command. |
| `WantEnblCtrlKtg` | `1` | Enable Control-Mode key toggle command. |
| `UseControlKeys` | `1` | Use control-key combinations. |
| `kBldOpts` | `"maxivmac " + version` | Build options string shown in About. |

### OS-Glue Common (osglu_common.h)

| Define | Value | Description |
|--------|-------|-------------|
| `ENABLE_FS_MOUSE_MOTION` | `1` | Enable full-screen mouse motion tracking. |
| `ENABLE_RECREATE_W` | `1` | Enable window recreation on mode change. |
| `ENABLE_MOVE_MOUSE` | `1` | Enable programmatic mouse positioning. |
| `GRAB_KEYS_FULL_SCREEN` | `1` | Grab keyboard in full-screen mode. |

### SDL Platform (sdl.cpp)

| Define | Value | Description |
|--------|-------|-------------|
| `HAVE_WORKING_WARP` | `1` | SDL cursor warp is functional. |
| `USE_MOTION_EVENTS` | `1` | Use SDL motion events (vs. polling). |
| `kMagStateAuto` | `kNumMagStates` | Auto-detect magnification state. |
| `INIT_STEP(name, expr)` | macro | Init-step helper with error reporting. |

### Debug Log Output

| Define | Value | File | Description |
|--------|-------|------|-------------|
| `dbglog_open` | `dbglog_open0` | `osglu_common.cpp` | Alias for platform-specific log open. |
| `dbglog_close` | `dbglog_close0` | `osglu_common.h` | Alias for platform-specific log close. |
| `dbglog_write` | `dbglog_write0` | `osglu_common.h` | Alias for platform-specific log write. |
| `dbglog_bufsz` | `POW_OF_2(dbglog_buflnsz)` | `osglu_common.cpp` | Debug log buffer size (when buffered). |

### Path Separator

| Define | Value | Condition | Description |
|--------|-------|-----------|-------------|
| `MyPathSep` | `'\\'` | `_WIN32` | Windows path separator. |
| `MyPathSep` | `'/'` | else | Unix path separator. |

---

## 15. UI / Control Mode Constants

Defined in `src/platform/common/control_mode.cpp` and `control_mode.h`.

| Define | Value | Description |
|--------|-------|-------------|
| `ControlBoxh0` | `0` | Control-mode overlay horizontal origin. |
| `ControlBoxw` | `62` | Control-mode overlay width (characters). |
| `ControlBoxv0` | `0` | Control-mode overlay vertical origin. |
| `hLimit` | `ControlBoxh0 + ControlBoxw - 1` | Right edge of control box. |
| `hStart` | `ControlBoxh0 + 1` | First content column. |
| `SpecialModeSet(i)` | bitwise OR | Set special-mode bit. |
| `SpecialModeClr(i)` | bitwise AND NOT | Clear special-mode bit. |
| `SpecialModeTst(i)` | bitwise test | Test special-mode bit. |
| `MacMsgDisplayed` | `SpecialModeTst(SpclModeMessage)` | Test if a message is displayed. |
| `Keyboard_UpdateKeyMap1` | `Keyboard_UpdateKeyMap` | Keyboard update alias. |
| `DisconnectKeyCodes1` | `DisconnectKeyCodes` | Key disconnect alias. |

### Keep-Mask Constants (osglu_common.h)

| Define | Value | Description |
|--------|-------|-------------|
| `kKeepMaskControl` | `1 << 0` | Keep Control key across disconnect. |
| `kKeepMaskCapsLock` | `1 << 1` | Keep CapsLock key across disconnect. |
| `kKeepMaskCommand` | `1 << 2` | Keep Command key across disconnect. |
| `kKeepMaskOption` | `1 << 3` | Keep Option key across disconnect. |
| `kKeepMaskShift` | `1 << 4` | Keep Shift key across disconnect. |

### Event Queue (osglu_common.h)

| Define | Value | Description |
|--------|-------|-------------|
| `MyEvtQLg2Sz` | `4` | Log2 of event queue size. |
| `MyEvtQSz` | `16` | Event queue capacity. |
| `MyEvtQIMask` | `15` | Event queue index mask. |

### Parameter Buffers (param_buffers.h / osglu_common.h)

| Define | Value | Description |
|--------|-------|-------------|
| `PbufHaveLock` | `1` | Parameter buffers support locking. |
| `PbufUnlock(i)` | no-op | Unlock parameter buffer (currently no-op). |
| `PbufIsAllocated(i)` | bitmask test | Test if pbuf index is allocated. |
| `NOT_A_PBUF` | `0xFFFF` | Sentinel value for invalid pbuf index. |

### International Characters (intl_chars.h)

| Define | Default | Description |
|--------|---------|-------------|
| `NeedCell2MacAsciiMap` | `1` | Compile Mac-ASCII mapping table. |
| `NeedCell2PlainAsciiMap` | `1` | Compile plain-ASCII mapping table. |
| `NeedCell2UnicodeMap` | `1` | Compile Unicode mapping table. |
| `NeedRequestInsertDisk` | `1` | Compile disk-insert request support. |
| `NeedDoMoreCommandsMsg` | `1` | Compile "More Commands" message. |
| `NeedDoAboutMsg` | `1` | Compile About message. |
| `NeedRequestIthDisk` | `1` | Compile indexed disk request. |
| `kStrCntrlKyName` | `"control"` | Display name of the control key. |
| `kControlModeKey` | `kStrCntrlKyName` | Key name shown in Control Mode prompts. |
| `kUnMappedKey` | `kStrCntrlKyName` | Name for unmapped emulated key. |
| `ClStrMaxLength` | `512` | Maximum substitution string length. |

---

## 16. String Constants (Localization)

Defined in `src/lang/localization_keys.h` and `src/lang/strings_english.h`.
The two files define identical sets of `kStr*` keys — `localization_keys.h`
provides the canonical key names, while `strings_english.h` provides fallback
English text. The localization system overrides these at runtime.

~66 string defines covering:

- **Dialog titles & messages**: `kStrAboutTitle`, `kStrNoROMTitle`, `kStrCorruptedROMTitle`, `kStrQuitWarningTitle`, etc.
- **Command names**: `kStrCmdAbout`, `kStrCmdQuit`, `kStrCmdReset`, `kStrCmdSpeedControl`, etc.
- **State labels**: `kStrOn`/`kStrOff`, `kStrPressed`/`kStrReleased`, `kStrSpeedAllOut`, etc.
- **Menu items**: `kStrMenuFile`, `kStrMenuSpecial`, `kStrMenuItemAbout`, `kStrMenuItemOpen`, etc.
- **Mode labels**: `kStrModeControlBase`, `kStrModeSpeedControl`, `kStrModeConfirmReset`, etc.
- **macOS app menu**: `kStrAppMenuItemHide`, `kStrAppMenuItemQuit`, etc.
- **Meta**: `kStrProgramInfo`, `kStrWorkOfMany`, `kStrLicense`, `kStrDisclaimer`, `kStrHomePage`.

All strings use `^p` (program name), `^c` (control key), `^v` (version),
`^r` (ROM filename), `^w` (website URL) as substitution tokens, and `;]`…`;}`
for emphasis, `;[`…`;{` for links, `;ll` for ellipsis, `;ls` for semicolon.

---

## 17. Sony (Disk) Driver Constants

Defined in `src/devices/sony.cpp`.

### Status Macros

| Define | Expansion | Description |
|--------|-----------|-------------|
| `vSonyIsLocked(driveNo)` | bitmask test | Test if drive is write-locked. |
| `vSonyIsMounted(driveNo)` | bitmask test | Test if drive has disk mounted. |
| `vSonyIsInserted(driveNo)` | bitmask test | Test if drive has disk inserted (platform.h). |

### DC42 Image Format Offsets

| Define | Value | Description |
|--------|-------|-------------|
| `kDC42offset_tagChecksum` | `76` | Tag checksum offset in DC42 header. |
| `kDC42offset_diskFormat` | `80` | Disk format byte offset. |
| `kDC42offset_formatByte` | `81` | Format byte offset. |
| `ChecksumBlockSize` | `1024` | Block size for checksum computation. |
| `SizeCheckSumsToUpdate` | `8` or `4` | Number of checksums to update (tag-dependent). |
| `checkheaderblocks` | `64` | Header blocks to verify. |
| `checkheaderoffset` | `0` | Header verification start offset. |
| `checkheadersize` | `64 * 512` | Header verification size. |
| `Sony_SupportOtherFormats` | `SONY_SUPPORT_DC42` | Alias for DC42 support flag. |

### Trap Parameter Block Offsets

Mac OS trap parameter block field offsets (matching Inside Macintosh):

| Define | Offset | Description |
|--------|--------|-------------|
| `kqLink` | `0` | Queue link. |
| `kqType` | `4` | Queue type. |
| `kioTrap` | `6` | Trap word. |
| `kioCmdAddr` | `8` | Command address. |
| `kioCompletion` | `12` | Completion routine. |
| `kioResult` | `16` | Result code. |
| `kioNamePtr` | `18` | Name pointer. |
| `kioVRefNum` | `22` | Volume reference number. |
| `kioRefNum` | `24` | File reference number. |
| `kcsCode` | `26` | Control/status code. |
| `kcsParam` | `28` | Control/status parameters. |
| `kioBuffer` | `32` | Buffer pointer. |
| `kioReqCount` | `36` | Requested byte count. |
| `kioActCount` | `40` | Actual byte count. |
| `kioPosMode` | `44` | Positioning mode. |
| `kioPosOffset` | `46` | Position offset. |

### Positioning Modes

| Define | Value | Description |
|--------|-------|-------------|
| `kfsAtMark` | `0` | At mark (ignore offset). |
| `kfsFromStart` | `1` | From start of file. |
| `kfsFromLEOF` | `2` | From logical end of file. |
| `kfsFromMark` | `3` | From current mark. |

### Driver Commands

| Define | Value | Description |
|--------|-------|-------------|
| `kTrack` | `0` | Get current track. |
| `kGetIconID` | `20` | Get icon resource ID. |
| `kMediaIcon` | `22` | Get media icon. |
| `kFormatCopy` | `21315` | Format disk copy. |
| `kReturnFormatList` | `6` | Return format list. |
| `kMFMStatus` | `10` | MFM format status. |
| `kDuplicatorVersionSupport` | `17494` | Duplicator version support query. |
| `kdCtlPosition` | `16` | Get drive position. |
| `MinTicksBetweenInsert` | `240` | Minimum ticks between disk inserts (debounce). |

### Runtime Pointers (patched ROM offsets)

| Define | Description |
|--------|-------------|
| `SonyVarsPtr` | Pointer to Sony driver variables (ROM-patched). |
| `FirstDriveVarsOffset` | Offset to first drive variables. |
| `EachDriveVarsSize` | Size of per-drive variable block. |
| `MinSonVarsSize` | Minimum Sony variables block size. |
| `Sony_DriverBase` | Base address of patched Sony driver in ROM. |
| `kcom_checkval` | `0x841339E2` — Sony extension validation magic number. |

### ROM Patching (rom.cpp)

| Define | Value | Description |
|--------|-------|-------------|
| `UseSonyPatch` | `1` | Apply Sony driver patch to ROM. |
| `DisableRomCheck` | `1` | Skip ROM checksum verification. |
| `DisableRamTest` | `1` | Skip RAM test on boot (faster startup). |
| `HappyMacBase` | model-dependent | ROM offset for Happy Mac icon (patched). |

---

## 18. Extension Interface Constants

Defined in `src/core/machine.h`. These define the data layout for the
guest-to-host trap interface used by maxivmac extensions.

| Define | Value | Description |
|--------|-------|-------------|
| `ExtnDat_checkval` | `0` | Offset: check value (validates extension call). |
| `ExtnDat_extension` | `2` | Offset: extension ID. |
| `ExtnDat_commnd` | `4` | Offset: command code. |
| `ExtnDat_result` | `6` | Offset: result code. |
| `ExtnDat_params` | `8` | Offset: parameter data start. |
| `kCmndVersion` | `0` | Command: get version. |
| `ExtnDat_version` | `8` | Offset: version response. |
| `kcom_callcheck` | `0x5B17` | Extension call validation magic number. |

---

## 19. Utility Macros

### Power-of-2 Arithmetic (osglu_common.h)

| Define | Description |
|--------|-------------|
| `POW_OF_2(p)` | `1 << p` — Power of 2. |
| `POW2_MASK(p)` | `POW_OF_2(p) - 1` — Bitmask for p bits. |
| `MOD_POW2(i, p)` | `i & POW2_MASK(p)` — Modulo power of 2. |
| `FLOOR_DIV_POW2(i, p)` | `i >> p` — Floor division by power of 2. |
| `FLOOR_POW2_MULT(i, p)` | Round down to power-of-2 multiple. |
| `CEIL_POW2_MULT(i, p)` | Round up to power-of-2 multiple. |

### Bit Operations (via_base.cpp)

| Define | Description |
|--------|-------------|
| `BIT_MASK(p)` | `1 << p` — Single bit mask. |
| `TEST_BIT(i, p)` | Test if bit `p` is set in `i`. |
| `SetContains(s, i)` | Test if element `i` is in set `s` (bitmask). |

### Compiler Hints (types.h)

| Define | Value | Description |
|--------|-------|-------------|
| `UNUSED(p)` | `(void)(p)` | Suppress unused-parameter warning. |
| `BIG_ENDIAN_UNALIGNED` | `0` | Big-endian unaligned access available (default: no). |
| `LITTLE_ENDIAN_UNALIGNED` | `0` | Little-endian unaligned access available (default: no). |
| `HAVE_ASR` | `0` | Compiler guarantees arithmetic shift right (default: no). |
| `HAVE_SWAP_UI5R` | `0` | Byte-swap intrinsic available (default: no). |
| `LIT64(a)` | `a##ULL` | 64-bit integer literal suffix. |

### Quick Exit (platform.h)

| Define | Description |
|--------|-------------|
| `QuietEnds()` | Reset quiet-time counters (macro wrapping two assignments). |

### Abnormal Reports (machine.h)

| Define | Condition | Description |
|--------|-----------|-------------|
| `ReportAbnormalID(id, s)` | `WantAbnormalReports=0` → no-op | Report abnormal situation by ID. |
| `ReportAbnormalID` | `WantAbnormalReports=1` → `DoReportAbnormalID` | Calls handler with ID (and string if logging). |
| `dbglog_StartLine()` | `WANT_DISASM=0` → no-op | Start a disassembly log line. |

---

## 20. Include Guards

The following use `#define` for traditional include guards (most headers use
`#pragma once` instead):

| Define | File |
|--------|------|
| `DBGLOG_PLATFORM_H` | `dbglog_platform.h` |
| `PATH_UTILS_H` | `path_utils.h` |
| `SDL_KEYBOARD_H` | `sdl_keyboard.h` |
| `SDL_SOUND_H` | `sdl_sound.h` |
| `TICK_TIMER_H` | `tick_timer.h` |

---

## Origin Summary

| Origin | Count (approx.) | Description |
|--------|-----------------|-------------|
| **minivmac legacy** | ~450 | CPU internals, device registers, Sony driver, wire signals, type helpers. Inherited from the original minivmac C codebase. |
| **SoftFloat / Bochs** | ~80 | FPU math constants, NaN definitions, polynomial table sizes. Derived from SoftFloat library and Bochs FPU code. |
| **maxivmac modernization** | ~100 | Feature flags in `emulation_config.h`, CMake-injected defines, runtime screen macros, platform config, WireBus aliases. |
| **Localization** | ~130 | String constants (duplicated across `localization_keys.h` and `strings_english.h`). |
| **Screen mapper template** | ~50 | Template parameters `#define`d and `#undef`d for each depth-conversion function instantiation. |
| **Platform/SDL glue** | ~45 | SDL-specific config, sound buffers, event queue, keep-masks, debug-log routing. |
