# maxivmac Codebase Insights

Architecture reference for the current (post-Phase-5) codebase.

> **History:** The original minivmac codebase used a 3-stage build pipeline,
> custom type aliases (`ui3b` → `uint8_t`, etc.), visibility macros
> (`LOCALPROC` → `static void`, etc.), and per-model compile-time `#define`s.
> All of that has been replaced.  For the original analysis and the complete
> old→new file mapping, see `docs/done/PLAN-3.md`.
>
> The CPU emulator descends from the **Un*x Amiga Emulator (UAE)** by Bernd
> Schmidt (adapted by Philip Cummins via vMac).  FPU emulation by Ross Martin.
> License: GPL v2.

---

## Architecture

The emulator is a single-binary multi-model build.

| Aspect | Design |
|--------|--------|
| Device ownership | `Rig` object owns all devices |
| Inter-device signals | `WireBus` + `findDevice<T>()` cross-refs |
| Model selection | Runtime (`--model=` flag), 12 models in one binary |
| Configuration | `MachineConfig` struct with runtime fields |
| Scheduling | `ICTScheduler` class with cycle-based task dispatch |
| Device access | `Machine::findDevice<>()` (no global device pointers) |
| CPU feature gates | Dispatch table fixup + runtime checks |
| Memory/screen sizes | `MachineConfig` fields, allocated dynamically |

## Source Layout

```
src/
  config/       — CNFUDPIC.h, CNFUDALL.h, CNFUIALL.h, etc. (mostly legacy, thinned out)
  core/         — Machine, MachineConfig, WireBus, ICTScheduler, main loop, config_loader
  cpu/          — m68k.cpp (68000/68020), m68k_tables.cpp, disasm.cpp, fpu_math.h
  devices/      — VIA, SCC, SCSI, IWM, RTC, ROM, ADB, Keyboard, Mouse, Sound, ASC, PMU,
                  Sony, Screen, Video — each a Device subclass
  platform/     — sdl.cpp (sole backend, cross-platform)
  lang/         — Localized string headers
  resources/    — App icons and resources
```

## Runtime Configuration Flow

```
main(argc, argv)
  → ProgramEarlyInit(argc, argv)         // parse CLI args into LaunchConfig
  → BuildMachineConfig(LaunchConfig)      // merge CLI overrides with model defaults
  → Machine::init(MachineConfig)          // create devices, set up WireBus, init CPU
  → LoadMacRom()                          // load ROM file (size from config)
  → MainEventLoop()                       // 60 Hz tick loop
```

## CLI Interface

```
./maxivmac --model=MacII --rom=MacII.ROM --ram=8M --screen=640x480x8 disk.img
./maxivmac --model=MacPlus --rom=MacPlus.ROM --ram=4M disk.img
./maxivmac --model=MacSE --rom=MacSE.ROM disk.img
./maxivmac --model=PB100 --rom=PB100.ROM disk.img
./maxivmac -h   # show help
```

## Supported Models

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

## MachineConfig Key Fields

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

## Remaining Compile-Time Defines

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
