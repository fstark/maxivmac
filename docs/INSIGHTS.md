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
| Device access | `g_rig->findDevice<T>()` (global `Rig*` for back-compat) |
| CPU feature gates | Dispatch table fixup + runtime checks |
| Memory/screen sizes | `MachineConfig` fields, allocated dynamically |

## Source Layout

```
src/
  config/       — mac_file.cpp/.h  (parse .mac bundle files)
  core/         — Machine, MachineConfig, WireBus, ICTScheduler, Rig, config_loader,
                  emulation_config, emulator_config, state_recorder, extn_* extension blocks
  cpu/          — cpu.cpp (entry), m68k.cpp (68000/68020), m68k_tables.cpp, disasm.cpp,
                  fpu_math.h, trap_counter, trap_defs, trap_tracer
  debugger/     — debugger, commands (break/exec/memory/trace/…), expression evaluator,
                  script engine, symbols, dbg_client, dbg_io
  devices/      — VIA, SCC, SCSI, IWM, RTC, ROM, ADB, Keyboard, Mouse, Sound, ASC, PMU,
                  Sony, Screen, Video, serial backends (file/pty/loopback/SLIP), slot_rom
  guest/        — guest_dialog, guest_types
  lang/         — type_registry, global_registry  (debugger variable inspection)
  platform/     — app_main (entry), ImGui backend, Headless backend, EmulatorShell,
                  SDL keyboard/sound, screen_convert, platform_macos.mm
  resources/    — App icons and resources
  storage/      — drive_manager, host_volume, appledouble, filename_encoding, text_convert,
                  icon_builder
  util/         — macroman.cpp/.h
```

## Runtime Configuration Flow

```
main(argc, argv)   [platform/app_main.cpp]
  → ProgramEarlyInit(argc, argv)         // parse CLI args; create Rig if --model given
  → EmulatorShell.run(backend)           // backend = ImGuiBackend or HeadlessBackend
     → backend drives the tick loop
        → ProgramMain()                  // trap setup, debugger scripts, InitEmulation
           → Rig::init()                 // create devices, set up WireBus, init CPU
           → LoadMacRom()               // load ROM file (size from MachineConfig)

Model not specified at startup → selector UI → SetLaunchConfig() → Rig created then.
```

## CLI Interface

```
./maxivmac --model=MacII --rom=MacII.ROM --ram=8M --screen=640x480x8 disk.img
./maxivmac --model=MacPlus disk.img
./maxivmac --model=MacSE --shared=/path/to/host/dir disk.img
./maxivmac --headless --verify=golden.bin   # non-regression testing
./maxivmac --debugger --dbg-script=foo.dbg disk.img
./maxivmac --serial-a=slip --slip-redir=tcp:8080:10.0.2.15:80 disk.img
./maxivmac -h   # show help
```

Key flags: `--model`, `--rom`, `--romdir`, `--ram`, `--screen`, `--speed`, `--scale`,
`--fullscreen`, `--headless`, `--silent`, `--shared`, `--serial-a/b`, `--slip-redir`,
`--debugger`, `--dbg-script`, `--debugserver`, `--trace-traps`, `--diag`,
`--record`, `--verify`, `--trace`, `--trace-cpu`.

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

## Compile-Time Constants

The old `CNFUDPIC.h` / `CNFUDALL.h` header files are gone.  The few remaining
constants live in `src/core/emulation_config.h`:

| Constant | Value | Purpose |
|----------|-------|--------|
| `NumDrives` | 6 | Max simultaneous disk drives |
| `NumPbufs` | 4 | Disk parameter block buffer count |

CPU feature flags (`Use68020`, `EmFPU`, `EmMMU`) are now pure runtime fields in
`MachineConfig` — no compile-time defines.  The dispatch table is fixed up in
`M68KITAB_setup()` based on the runtime values.
