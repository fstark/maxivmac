# minivmac Codebase Insights

Collected during deep analysis of the codebase. Reference for future work.

> **Note:** This document was written during initial analysis when files had their
> original names (e.g. `GLOBGLUE.c`, `MINEM68K.c`, `OSGLUAAA.h`). The codebase
> has since been restructured — see `docs/PLAN-3.md` for the complete file mapping
> from old names to new names. The new layout lives under `src/core/`, `src/cpu/`,
> `src/devices/`, `src/platform/`, and `src/config/`.

---

## Build System — 3-Stage Pipeline

1. **Stage 1:** `setup/tool.c` is compiled with host GCC into a native binary `setup_t`. This single C file `#include`s ~30 `.i` files (which are C source fragments) to form a monolithic configuration/code-generation tool.
2. **Stage 2:** `setup_t` is invoked with dozens of CLI flags (target platform, Mac model, resolution, speed, API, etc.). Its **stdout is redirected to `setup.sh`**. This generated shell script uses hundreds of `printf` statements to write out all generated config files and platform-specific build artifacts (Makefile, Xcode project, etc.). The generated `setup.sh` is ~962 lines of printf statements.
3. **Stage 3:** After sourcing `setup.sh`, the actual build runs (`make`, `xcodebuild`, etc.).

### Per-Platform Build Script Flags

| Script | `-t` (target) | `-e` (IDE) | `-api` | Model |
|--------|--------------|-----------|--------|-------|
| `build_macos.sh` | `mcar` (Apple Silicon) | `xcd` (Xcode) | `cco` (Cocoa) | Mac II |
| `build_linux.sh` | `lx64` (Linux x64) | `bgc` (GCC) | `sd3` (SDL3) | Mac IIx |
| `build_windows.sh` | `wx64` (Windows x64) | `mgw` (MinGW) | `sd2` (SDL2) | Mac II |
| `build_dos.sh` | `mdos` (MS-DOS) | `bgc` (GCC/DJGPP) | `dos` | Mac Plus |
| `build_haiku.sh` | `hx64` (Haiku x64) | `bgc` (GCC) | (default) | Mac II |
| `build_raspi64.sh` | `larm` (Linux ARM64) | `bgc` (GCC) | `sd2` (SDL2) | Mac II |

### Xcode Project (Generated)

`minivmac.xcodeproj/project.pbxproj` (~396 lines) compiles:
- **Sources**: OSGLUCCO.m, GLOBGLUE.c, M68KITAB.c, MINEM68K.c, VIAEMDEV.c, VIA2EMDV.c, IWMEMDEV.c, SCCEMDEV.c, RTCEMDEV.c, ROMEMDEV.c, SCSIEMDV.c, SONYEMDV.c, SCRNEMDV.c, VIDEMDEV.c, MOUSEMDV.c, ADBEMDEV.c, ASCEMDEV.c, PROGMAIN.c
- **Frameworks**: AppKit, AudioUnit, OpenGL
- **Include paths**: `src/`, `cfg/`

### Setup Tool Internals

- `setup/tool.c` `#include`s ~30 `.i` files
- `setup/SPBLDOPT.i` — 4,161 lines, model/option definitions
- `setup/GNBLDOPT.i` — 2,272 lines, target/IDE/generic option definitions
- `setup/BLDUTIL3.i` — 1,250 lines, build utility functions
- The `.i` files have no include guards — they are concatenated source fragments

---

## Architecture — Four Layers

### Layer 1: Platform Abstraction ("OS Glue" — `OSGLU*`)

Multiple mutually exclusive backends, one compiled per variant:

| File | Lines | Platform |
|------|-------|----------|
| `src/OSGLUCCO.m` | 5,314 | macOS Cocoa (current primary) |
| `src/OSGLUOSX.c` | 5,670 | macOS Carbon (legacy) |
| `src/OSGLUSDL.c` | 5,266 | SDL 1.2/2.0/3.x (cross-platform) |
| `src/OSGLUWIN.c` | — | Win32 |
| `src/OSGLUXWN.c` | — | X11/Xlib |
| `src/OSGLUGTK.c` | — | GTK |
| `src/OSGLUDOS.c` | — | MS-DOS |
| `src/OSGLUNDS.c` | — | Nintendo DS |
| `src/OSGLUMAC.c` | — | Classic Mac OS |

The glue contract is defined in `src/OSGLUAAA.h` (462 lines).

Common code shared by all backends:
- `src/OSGCOMUI.h` — user-option-independent includes
- `src/OSGCOMUD.h` — user-option-dependent includes
- `src/COMOSGLU.h`, `src/CONTROLM.h` — shared implementation (included as headers but contain code)

### Layer 2: Core Emulation Glue (`GLOBGLUE`)

`src/GLOBGLUE.c` (1,767 lines) + `src/GLOBGLUE.h` (280 lines)

Contains:
- **Address Translation Table (ATT)**: Maps 68K address ranges to host memory or device handlers
- **Wires array**: `ui3b Wires[kNumWires]` — models physical signal wires between chips
- **Interrupt Cycle Timer (ICT)**: Timer/task scheduler for cycle-accurate device timing
- **MMDV_Access()**: Memory-mapped device I/O dispatch (switch on device index)

### Layer 3: Emulated Hardware Devices

Each device follows pattern: `.c` implementation + `.h` header exporting `Reset()`, `Access()`, and device-specific functions.

| Device | File | What it Emulates |
|--------|------|-----------------|
| CPU | `src/MINEM68K.c` (8,933 lines) | Motorola 68000/68020 with FPU |
| CPU decode tables | `src/M68KITAB.c` | Instruction decode/dispatch tables |
| VIA 1 | `src/VIAEMDEV.c` | MOS 6522 VIA |
| VIA 2 | `src/VIA2EMDV.c` | Second VIA (Mac II/IIx) |
| IWM | `src/IWMEMDEV.c` | Integrated Woz Machine (floppy) |
| SCC | `src/SCCEMDEV.c` | Zilog 8530 Serial (+ LocalTalk) |
| SCSI | `src/SCSIEMDV.c` | NCR 5380 SCSI |
| RTC | `src/RTCEMDEV.c` | Real Time Clock + PRAM |
| ROM | `src/ROMEMDEV.c` | ROM loading and checksum |
| Video | `src/VIDEMDEV.c` | Mac II video card (NuBus) |
| Screen | `src/SCRNEMDV.c` | Screen update tracking |
| Sound | `src/SNDEMDEV.c` | Classic Mac sound buffer |
| ASC | `src/ASCEMDEV.c` | Apple Sound Chip (Mac II) |
| Sony | `src/SONYEMDV.c` | Floppy disk driver (trap patches) |
| Keyboard | `src/KBRDEMDV.c` | Classic Mac keyboard protocol |
| ADB | `src/ADBEMDEV.c` | Apple Desktop Bus (Mac II+) |
| PMU | `src/PMUEMDEV.c` | Power Manager Unit (PowerBook) |
| Mouse | `src/MOUSEMDV.c` | Mouse position/button tracking |

### Layer 4: Main Program Loop

`src/PROGMAIN.c` (567 lines):

```
ProgramMain()
  └→ InitEmulation()
  └→ MainEventLoop()
       └→ WaitForNextTick()                [OS glue, blocks until 1/60s]
       └→ RunEmulatedTicksToTrueTime()
            └→ DoEmulateOneTick()
                 └→ SixtiethSecondNotify()  [mouse, kbd, VBL, Sony, RTC]
                 └→ m68k_go_nCycles_1()     [run CPU with ICT scheduling]
                 └→ SixtiethEndNotify()     [screen, mouse end]
       └→ DoEmulateExtraTime()             [>1x speed extra cycles]
```

Tick rate: 60 Hz. Each tick runs `CyclesScaledPerTick = 130240 * kMyClockMult * kCycleScale` scaled CPU cycles, divided into sub-ticks for sound timing.

---

## Configuration System

All configuration is **compile-time via `#define`s** in 6 generated headers in `cfg/`:

| File | Scope | Content |
|------|-------|---------|
| `CNFUIALL.h` | All code, user-independent | Integer types, compiler attributes, inline hints |
| `CNFUIOSG.h` | OS glue only, user-independent | System includes, app name, variation string, `WantOSGLUCCO` |
| `CNFUIPIC.h` | Platform-independent code, user-independent | Usually empty |
| `CNFUDALL.h` | All code, user-dependent | Sound, drives, screen size/depth, ROM size, features |
| `CNFUDOSG.h` | OS glue, user-dependent | ROM filename, key mappings, UI options, magnify, fullscreen, speed |
| `CNFUDPIC.h` | Platform-independent, user-dependent | **The big one**: hardware config — VIAs, CPU, FPU, MMU, sound, model, clock, RAM, Wire defs, VIA pins |
| `STRCONST.h` | All code | Language string file selector (e.g., `#include "STRCNENG.h"`) |

### CNFUDPIC.h — Complete Catalog

| Define | Current Value | Controls |
|--------|---------------|----------|
| **Device Inclusion** | | |
| `EmClassicKbrd` | 0 | Classic (non-ADB) keyboard emulation |
| `EmADB` | 1 | Apple Desktop Bus keyboard/mouse |
| `EmRTC` | 1 | Real Time Clock chip |
| `EmPMU` | 0 | Power Management Unit (PowerBook) |
| `EmVIA1` | 1 | VIA1 chip |
| `EmVIA2` | 1 | VIA2 chip (Mac II/SE) |
| `EmClassicSnd` | 0 | Classic sound hardware |
| `EmASC` | 1 | Apple Sound Chip |
| **CPU** | | |
| `Use68020` | 1 | 68020 instruction set |
| `EmFPU` | 1 | Floating Point Unit |
| `EmMMU` | 0 | Memory Management Unit |
| **Model** | | |
| `CurEmMd` | `kEmMd_II` | Mac model being emulated |
| **Clock** | | |
| `kMyClockMult` | 2 | Clock speed multiplier |
| `WantCycByPriOp` | 1 | Cycle-accurate per primary op |
| `WantCloserCyc` | 1 | More accurate cycle counting |
| `kAutoSlowSubTicks` | 16384 | SubTick threshold for auto-slow |
| `kAutoSlowTime` | 60 | Time threshold for auto-slow |
| **RAM** | | |
| `kRAMa_Size` | 0x00400000 | RAM bank A size (4 MB) |
| `kRAMb_Size` | 0x00400000 | RAM bank B size (4 MB) |
| **Video** | | |
| `IncludeVidMem` | 1 | Include video memory |
| `kVidMemRAM_Size` | 0x00080000 | Video RAM size (512 KB) |
| `EmVidCard` | 1 | Emulate NuBus video card |
| `kVidROM_Size` | 0x000800 | Video card ROM size |
| **ATT** | | |
| `MaxATTListN` | 20 | Max entries in address translation table |
| **Extensions** | | |
| `IncludeExtnPbufs` | 1 | Parameter buffer extension |
| `IncludeExtnHostTextClipExchange` | 1 | Host clipboard exchange |
| **Sony/Floppy** | | |
| `Sony_SupportDC42` | 1 | DC42 disk image format |
| `Sony_SupportTags` | 1 | Disk image tag support |
| `Sony_WantChecksumsUpdated` | 1 | Checksum updates |
| `Sony_VerifyChecksums` | 0 | Checksum verification |
| **Wire Variables** | | ~30 entries for inter-chip signal routing |
| **VIA Port Masks** | | I/O register bit masks for VIA1/VIA2 |
| **Hardware Addresses** | | |
| `kExtn_Block_Base` | 0x50F0C000 | Extensions I/O base |
| `kExtn_ln2Spc` | 5 | Extensions address space (log2) |
| `kROM_Base` | 0x00800000 | ROM base address |
| `kROM_ln2Spc` | 20 | ROM address space (log2) |
| **Debug** | | |
| `WantDisasm` | 0 | Disassembly support |
| `ExtraAbnormalReports` | 0 | Extra abnormality reporting |

### CNFUDALL.h — Screen Constants

- `vMacScreenWidth` = 640
- `vMacScreenHeight` = 480
- `vMacScreenDepth` = 3 (8-bit color)
- These are **pure compile-time constants** — no runtime screen resolution change

### CNFUDOSG.h — Runtime-ish Features

- `VarFullScreen` = 1, `MayFullScreen` = 1, `MayNotFullScreen` = 1
- `EnableMagnify` = 1, `WantInitMagnify` = 1
- `MyWindowScale` = 2 (compile-time magnification factor)
- Full-screen and magnification on/off are runtime-toggleable, but the **scale factor** and **screen resolution** are compile-time

---

## CPU Emulator (`MINEM68K.c`)

### Compile-Time Switches

| Define | Purpose |
|--------|---------|
| `Use68020` | 68020 instructions (long branches, bit-field ops, MOVEC, CAS, PACK, etc.) |
| `EmFPU` | FPU (68881/68882) coprocessor instruction decoding |
| `EmMMU` | MMU (68851/PMMU) instruction decoding |

### Key Structures

- `regstruct` (line ~199): CPU register state — `d[8]`, `a[7]`, `pc`, `sr`, `usp`, `isp`. Under `Use68020` adds: `t0`, `m`, `msp`, `sfc`, `dfc`, `vbr`, `cacr`, `caar`.
- `OpDispatch[]` array: Function pointer table indexed by instruction kind (`IKind`). 68020 adds ~20 extra entries, FPU adds 9, MMU adds 1.
- There are **50+ `#if Use68020` blocks** throughout the file.
- Memory Address Translation Cache (`MATCr`): One record each for read-byte, write-byte, read-word, write-word — accelerates repeated accesses to same page.

### CPU Entry Points

- `m68k_go_nCycles(n)` (line 8656): Runs CPU for N scaled cycles via instruction fetch/decode/execute loop
- `m68k_go_MaxCycles()`: Inner loop — fetch/decode/execute until cycle budget exhausted
- `m68k_reset()` (line ~8870): Resets CPU, conditionally zeroes 68020 registers
- `FindATTel(addr)` (line 8722): Walks ATT linked list to find matching address entry
- `SetHeadATTel(h)` (line 8800): Sets ATT list head, invalidates MATC cache

---

## Address Translation Table (ATT)

### Structure (GLOBGLUE.h lines 250-263)

```c
struct ATTer {
    struct ATTer *Next;
    ui5r cmpmask;   // mask for address comparison
    ui5r cmpvalu;   // value the masked address must match
    ui5r Access;    // bit flags: read-ready, write-ready, mmdv, ntfy
    ui5r usemask;   // mask to get offset into usebase
    ui3p usebase;   // host memory pointer (RAM/ROM/VidMem), or nullpr for MMIO
    ui3r MMDV;      // memory-mapped device index
    ui3r Ntfy;      // notification index
};
```

### Access Flags

- `kATTA_readreadymask` — direct memory read
- `kATTA_writereadymask` — direct memory write
- `kATTA_mmdvmask` — dispatches to memory-mapped device
- `kATTA_ntfymask` — triggers notification (e.g., overlay off)

### Memory-Mapped Device Enum (GLOBGLUE.c lines 668-683)

```c
enum {
    kMMDV_VIA1, kMMDV_VIA2, kMMDV_SCC, kMMDV_Extn,
    kMMDV_ASC, kMMDV_SCSI, kMMDV_IWM, kNumMMDVs
};
```

### Address Space Setup

- Non-II models: `SetUp_address()` at line 1179 — 24-bit addressing only
- Mac II/IIx: dispatch to `SetUp_address24()` or `SetUp_address32()` based on `Addr32` wire
- `SetUpMemBanks()` at line 1284: calls `InitATTList()` → `SetUp_address()` → `FinishATTList()` → `SetHeadATTel()`
- `MemAccessNtfy()` at line 1536: handles notification ATT entries (currently only `kMAN_OverlayOff`)

---

## Mac Model Enum (GLOBGLUE.h lines 24-35)

```c
#define kEmMd_Twig43   0    // Macintosh prototype (Twiggy 4.3)
#define kEmMd_Twiggy   1    // Macintosh prototype (Twiggy)
#define kEmMd_128K     2    // Macintosh 128K
#define kEmMd_512Ke    3    // Macintosh 512Ke
#define kEmMd_Kanji    4    // Macintosh (Kanji variant)
#define kEmMd_Plus     5    // Macintosh Plus
#define kEmMd_SE       6    // Macintosh SE
#define kEmMd_SEFDHD   7    // Macintosh SE FDHD
#define kEmMd_Classic  8    // Macintosh Classic
#define kEmMd_PB100    9    // PowerBook 100
#define kEmMd_II       10   // Macintosh II
#define kEmMd_IIx      11   // Macintosh IIx
```

### How Model Affects Code

Used in **ordered comparisons** (`<=`, `>=`) to group models by generation:
- Early Macs (≤512Ke): simple address space, no overlay switching
- Plus-era (≤Plus): ROM overlay on reset
- Compact Macs (≤Classic): 24-bit addressing only
- Portable (PB100): PMU, unique I/O addresses (GLOBGLUE.c lines 237-295)
- Mac II family (II, IIx): 24/32-bit address switching via `Addr32` wire, 4-level interrupt priority

Key locations of `CurEmMd` / `kEmMd_*` usage:
- `GLOBGLUE.c` lines 103, 636-664, 1101-1110, 1179-1283, 1639-1658
- `ROMEMDEV.c` lines 183-195, 280-310 (ROM checksums and patches per model)
- `IWMEMDEV.c` line 103 (SWIM floppy controller: SE through IIx only)
- `SCRNHACK.h` line 24 (128K screen hack)
- `PROGMAIN.c` — device init varies by model

---

## OS Glue Interface Contract (OSGLUAAA.h)

### Glue Layer Must Provide

**Memory:**
- `ReserveAllocOneBlock(ui3p *p, uimr n, ui3r align, blnr FillOnes)`
- `MyMoveBytes(anyp src, anyp dst, si5b count)`
- `ROM` — pointer to ROM buffer

**Disk I/O:**
- `vSonyWritableMask`, `vSonyInsertedMask` — drive state bitmasks
- `vSonyTransfer()`, `vSonyEject()`, `vSonyGetSize()` — disk operations
- `AnyDiskInserted()`, `DiskRevokeWritable()`
- Optional: `vSonyRawMode`, `vSonyNewDiskWanted`, `vSonyNewDiskSize`, `vSonyEjectDelete()`, `vSonyNewDiskName`, `vSonyGetName()`

**Clipboard:**
- `HTCEexport(tPbuf i)`, `HTCEimport(tPbuf *r)` (if `IncludeHostTextClipExchange`)

**Parameter Buffers** (if `IncludePbufs`):
- `CheckPbuf()`, `PbufGetSize()`, `PbufNew()`, `PbufDispose()`, `PbufTransfer()`

**Timing:**
- `OnTrueTime` — wall-clock tick counter
- `CurMacDateInSeconds`, `CurMacLatitude`, `CurMacLongitude`, `CurMacDelta`
- `ExtraTimeNotOver()`, `WaitForNextTick()`

**Video:**
- `vMacScreenNumPixels`, `vMacScreenNumBytes`, `vMacScreenByteWidth` (derived from compile-time width/height/depth)
- `UseColorMode`, `ColorModeWorks`, `ColorMappingChanged` (if depth ≠ 0)
- `CLUT_reds[]`, `CLUT_greens[]`, `CLUT_blues[]` (if depth 1-3)
- `EmVideoDisable`, `EmLagTime`
- `Screen_OutputFrame(ui3p screencurrentbuff)`
- `DoneWithDrawingForTick()`

**Input:**
- `CurMouseV`, `CurMouseH`
- `MyEvtQOutP()`, `MyEvtQOutDone()` — event dequeue
- `MyEvtQEl` struct — 8-byte event records
- Complete Mac keycode table: `MKC_A` through `MKC_Pause` (~100 defines)

**Control:**
- `ForceMacOff`, `WantMacInterrupt`, `WantMacReset`
- `SpeedValue`, `WantNotAutoSlow`, `QuietTime`, `QuietSubTicks`

**Sound** (if `MySoundEnabled`):
- `MySound_BeginWrite(ui4r n, ui4r *actL)` → `tpSoundSamp`
- `MySound_EndWrite(ui4r actL)`

**LocalTalk** (if `EmLocalTalk`):
- `LT_TxBuffer`, `LT_TxBuffSz`, `LT_TransmitPacket()`
- `LT_RxBuffer`, `LT_RxBuffSz`, `LT_ReceivePacket()`

**Debug** (if `dbglog_HAVE`):
- `dbglog_writeCStr()`, `dbglog_writeReturn()`, `dbglog_writeHex()`, etc.

---

## Timing Constants

Defined in `PROGMAIN.c` line 126:
```c
#define CyclesScaledPerTick    (130240UL * kMyClockMult * kCycleScale)
#define CyclesScaledPerSubTick (CyclesScaledPerTick / kNumSubTicks)
```

Where:
- `kCycleScale = (1 << kLn2CycleScale) = (1 << 6) = 64` (GLOBGLUE.h line 207)
- `kNumSubTicks = 16` (GLOBGLUE.h line 212)
- `kMyClockMult = 2` (CNFUDPIC.h line 25, Mac II = 2x base clock)
- So: `CyclesScaledPerTick = 130240 * 2 * 64 = 16,670,720`

Sub-tick processing fires `ASC_SubTick()` or `MacSound_SubTick()` for sound timing.

---

## Cocoa Backend Runtime Features (OSGLUCCO.m)

| Feature | Runtime? | Mechanism |
|---------|----------|-----------|
| Full-screen toggle | **Yes** | `UseFullScreen` / `ToggleWantFullScreen()` (line 4255) |
| Magnification on/off | **Yes** | `UseMagnify` toggled with full-screen |
| Magnify scale factor | **No** | `MyWindowScale` = `#define 2` |
| Screen resolution | **No** | `vMacScreenWidth`/`Height` are compile-time |
| Color depth | **No** | `vMacScreenDepth` is compile-time |

---

## Supported Platforms & Targets

~40 target platforms in `setup/GNBLDOPT.i`:
- **macOS**: 68K, PPC, Mach-O PPC, Intel, x64, Apple Silicon (`mcar`)
- **Windows**: x86, x64, CE/ARM, CE/x86
- **Linux**: x86, x64, PPC, ARM, SPARC
- **BSD**: FreeBSD (x86/x64/PPC), OpenBSD, NetBSD, Dragonfly BSD
- **Haiku**: x64
- **Solaris/OpenIndiana**: SPARC, Intel, x64
- **Other**: MS-DOS, Nintendo DS, IRIX/MIPS, Cygwin, Minix, generic X11

13 API families: Classic Mac, Carbon, Cocoa, Windows, X11, DOS, NDS, GTK, SDL 1.2/2.0/3.x, Cocoa (cco), Port

17 IDE/toolchains: MPW, Metrowerks, GCC, Sun tools, MSVC, lcc, Dev-C++, Xcode, Digital Mars, Pelles C, MinGW, Cygwin, devkitpro, generic cc, MvC, Port

---

## Code Patterns & Conventions

### Custom Type System

| Custom | Standard Equivalent |
|--------|-------------------|
| `ui3b` | `uint8_t` |
| `ui3r` | `uint8_t` (register-sized) |
| `ui4b` | `uint16_t` |
| `ui4r` | `uint16_t` |
| `ui5b` | `uint32_t` |
| `ui5r` | `uint32_t` |
| `si3b` | `int8_t` |
| `si4b` | `int16_t` |
| `si5b` | `int32_t` |
| `si5r` | `int32_t` |
| `blnr` | `bool` |
| `trueblnr` | `true` |
| `falseblnr` | `false` |
| `nullpr` | `nullptr` / `NULL` |
| `CPTR` | `uint32_t` (68K address) |
| `anyp` | `void*` |
| `uimr` | `uintptr_t` |

### Visibility Macros

| Macro | Meaning |
|-------|---------|
| `LOCALVAR` | `static` variable |
| `GLOBALVAR` | non-static variable |
| `EXPORTVAR` | `extern` variable |
| `LOCALFUNC` | `static` function (returns value) |
| `LOCALPROC` | `static void` function |
| `GLOBALFUNC` | non-static function |
| `GLOBALPROC` | non-static void function |
| `EXPORTFUNC` | `extern` function |
| `EXPORTPROC` | `extern void` function |
| `IMPORTFUNC` | `extern` function (import) |
| `IMPORTPROC` | `extern void` function (import) |

### Code-as-Headers (files #included into .c files)

- `COMOSGLU.h` — shared OS glue implementation
- `CONTROLM.h` — in-emulator control mode UI
- `SCRNMAPR.h` — screen pixel mapping
- `SCRNTRNS.h` — screen transfer functions
- `FPCPEMDV.h` — FPU emulation implementation
- `ADBSHARE.h` — shared ADB code
- `ALTKEYSM.h` — alternate key mapping
- `HPMCHACK.h` — HP Mac hack

### Heritage

- CPU emulator descends from the **Un*x Amiga Emulator (UAE)** by Bernd Schmidt (adapted by Philip Cummins via vMac)
- Cocoa port derived from SDL by Sam Lantinga's Cocoa SDL port
- FPU emulation added by Ross Martin
- License: GPL v2

---

## What Does NOT Exist

- **No tests** — no test files, directories, or framework references anywhere
- **No CI** — no GitHub Actions, Travis, or other CI config
- **No debugger** — only `ReportAbnormalID` assertions and `dbglog_*` debug logging
- **No external control API** — no sockets, no IPC, no scripting

---

## Post-Phase-5 Architecture (Current State)

After Phases 3-5, the codebase has been restructured and the emulator is now a single-binary multi-model build.

### Key Architectural Changes

| Before (Phase 1-2) | After (Phase 4-5) |
|--------------------|--------------------|
| All state in global variables | `Machine` object owns all devices |
| Devices wired via `#if` and global `switch` | `WireBus` for inter-device signals, `findDevice<T>()` for cross-refs |
| Model selected at compile time (`CurEmMd`) | Model selected at runtime (`--model=` flag) |
| One binary = one Mac model | One binary emulates 12 Mac models |
| `CNFUDPIC.h` with ~200 lines of `#define`s | `MachineConfig` struct with runtime fields |
| ICT scheduling via global arrays | `ICTScheduler` class with cycle-based task dispatch |
| 17 global device pointers (`g_via1`, etc.) | All access through `Machine::findDevice<>()` |
| CPU features via `#if Use68020` | Dispatch table fixup + runtime checks |
| Memory sizes as compile-time constants | `MachineConfig` fields allocated dynamically |
| Screen size as compile-time constants | Global variables set from `MachineConfig` at init |

### Source Layout

```
src/
  config/       — CNFUDPIC.h, CNFUDALL.h, CNFUIALL.h, etc. (mostly legacy, thinned out)
  core/         — Machine, MachineConfig, WireBus, ICTScheduler, main loop, config_loader
  cpu/          — m68k.cpp (68000/68020), m68k_tables.cpp, disasm.cpp, fpu_math.h
  devices/      — VIA, SCC, SCSI, IWM, RTC, ROM, ADB, Keyboard, Mouse, Sound, ASC, PMU,
                  Sony, Screen, Video — each a Device subclass
  platform/     — cocoa.mm (macOS), sdl.cpp, win32.cpp, x11.cpp, etc.
  lang/         — Localized string headers
  resources/    — App icons and resources
```

### Runtime Configuration Flow

```
main(argc, argv)
  → ProgramEarlyInit(argc, argv)         // parse CLI args into LaunchConfig
  → BuildMachineConfig(LaunchConfig)      // merge CLI overrides with model defaults
  → Machine::init(MachineConfig)          // create devices, set up WireBus, init CPU
  → LoadMacRom()                          // load ROM file (size from config)
  → MainEventLoop()                       // 60 Hz tick loop
```

### CLI Interface

```
./minivmac --model=II --rom=MacII.ROM --ram=8M --screen=640x480x8 disk.img
./minivmac --model=Plus --rom=vMac.ROM --ram=4M disk.img
./minivmac --model=SE --rom=MacSE.ROM disk.img
./minivmac --model=PB100 --rom=PB100.ROM disk.img
./minivmac -h   # show help
```

### Supported Models

| Model | CPU | ROM Size | Screen | Sound | Keyboard |
|-------|-----|---------|--------|-------|----------|
| Twig43 | 68000 | 64 KB | 512×342×1 | Classic | Classic serial |
| Twiggy | 68000 | 64 KB | 512×342×1 | Classic | Classic serial |
| 128K | 68000 | 64 KB | 512×342×1 | Classic | Classic serial |
| 512Ke | 68000 | 128 KB | 512×342×1 | Classic | Classic serial |
| Kanji | 68000 | 256 KB | 512×342×1 | Classic | Classic serial |
| Plus | 68000 | 128 KB | 512×342×1 | Classic | Classic serial |
| SE | 68000 | 256 KB | 512×342×1 | Classic | ADB |
| SEFDHD | 68000 | 256 KB | 512×342×1 | Classic | ADB |
| Classic | 68000 | 512 KB | 512×342×1 | Classic | ADB |
| PB100 | 68000 | 256 KB | 640×400×1 | ASC | PMU |
| II | 68020+FPU | 256 KB | 640×480×8 | ASC | ADB |
| IIx | 68030+FPU | 256 KB | 640×480×8 | ASC | ADB |

### MachineConfig Key Fields

```cpp
struct MachineConfig {
    MacModel model;
    bool use68020, emFPU, emMMU;                  // CPU features
    uint32_t ramASize, ramBSize;                   // memory banks
    uint32_t romSize, romBase;                     // ROM geometry
    const char* romFileName;                       // ROM file to load
    uint32_t extnBlockBase;                        // extension block (24-bit or 32-bit)
    uint8_t extnLn2Spc;
    bool emVIA1, emVIA2, emADB, emClassicKbrd;    // device enables
    bool emPMU, emASC, emClassicSnd, emRTC;
    bool emVidCard, includeVidMem;
    uint32_t vidMemSize, vidROMSize;
    uint32_t maxATTListN;                          // address translation table size
    uint32_t screenWidth, screenHeight, screenDepth;
    uint32_t clockMult;                            // clock speed multiplier
    uint32_t autoSlowSubTicks, autoSlowTime;
    VIAConfig via1Config, via2Config;              // VIA port wiring
};
```

### Remaining Compile-Time Defines

These are still in `CNFUDPIC.h` / `CNFUDALL.h` but do not vary per model in the current build:

| Define | Value | Purpose |
|--------|-------|---------|
| `Use68020` | 1 | Always 1; runtime dispatch handles 68000 vs 68020 |
| `EmFPU` | 1 | Always 1; runtime check skips FPU for 68000 models |
| `EmMMU` | 0 | Always 0; MMU not emulated |
| `WantCycByPriOp` | 1 | Cycle-accurate per primary op |
| `WantCloserCyc` | 1 | More accurate cycle counting |
| `MySoundEnabled` | 1 | Sound support enabled |
| `NumDrives` | 6 | Max simultaneous disk drives |
| `Sony_SupportDC42` | 1 | DC42 disk image format support |
| `WantDisasm` | 0 | Disassembly support (debug) |
