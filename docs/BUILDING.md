# Building maxivmac

## Source Layout

```
src/core/        — Core emulation (machine glue, main loop, endian helpers)
src/cpu/         — 68000/68020 CPU emulator and instruction decode tables
src/devices/     — Hardware device emulation (VIA, SCC, IWM, SCSI, ADB, etc.)
src/platform/    — Platform backend (sdl.cpp)
  common/        — Shared platform code compiled as separate translation units:
                   osglu_common, intl_chars, param_buffers, control_mode
src/config/      — Build configuration headers and language strings
src/resources/   — Application icon resources
```

## Quick Start (macOS)

Clean:
```bash
rm -rf bld/
```

```bash
cmake --preset macos
cmake --build --preset macos
```

The binary is at `bld/macos/maxivmac`. Place a Mac ROM file in the working directory and pass a System disk image on the command line to boot.

## Runtime Model Selection

The emulator is a single binary supporting multiple Mac models. Use command-line
flags to select the model at launch:

```bash
# Mac II (default)
./maxivmac --rom=MacII.ROM disk.hfs

# Mac Plus
./maxivmac --model=MacPlus --rom=MacPlus.ROM disk.dsk

# Mac SE
./maxivmac --model=MacSE --rom=MacSE.ROM disk.hfs

# Custom RAM and screen
./maxivmac --model=MacII --ram=8M --screen=1024x768x8
```

### Command-Line Options

| Flag | Description |
|------|-------------|
| `--model=MODEL` | Mac model (= ROM base name): `MacPlus`, `MacSE`, `MacII`, `MacIIx`, `Classic`, `PB100`, `SEFDHD`, `Mac128K`, `Mac512Ke` |
| `--rom=PATH` | Path to ROM file (overrides model default) |
| `--ram=SIZE` | RAM size: `1M`, `2M`, `4M`, `8M`, etc. |
| `--screen=WxHxD` | Screen: `512x342x1`, `640x480x8`, etc. (D = log2 bpp) |
| `--speed=N` | Speed multiplier (1 = 1×, 4 = 4×, 0 = all-out) |
| `--fullscreen` | Start in fullscreen mode |
| `-h`, `--help` | Show help |
| positional args | Disk image paths |

## Requirements

- **CMake** ≥ 3.20
- **Ninja** (recommended) or Make
- **SDL2** or **SDL3** development libraries (all platforms)
- **macOS:** Xcode command-line tools (provides Clang)
- **Linux:** X11 development libraries (optional, for X11 support via SDL)
- **Windows:** MinGW or MSVC

## Build Presets

| Preset | Platform | Backend | Notes |
|--------|----------|---------|-------|
| `macos` | macOS | SDL | Default for macOS. |
| `linux` | Linux | SDL | Default for Linux. |
| `windows` | Windows | SDL | Default for Windows. |

Usage:

```bash
cmake --preset <preset>
cmake --build --preset <preset>
```

## Manual Configuration

If you don't want to use presets:

```bash
cmake -B bld -G Ninja \
    -DCMAKE_BUILD_TYPE=Release
cmake --build bld
```

## CMake Options

### Display (build-time window defaults)

| Option | Default | Description |
|--------|---------|-------------|
| `MINIVMAC_MAGNIFY_ENABLE` | `1` | Allow window magnification |
| `MINIVMAC_MAGNIFY_INIT` | `1` | Start magnified |
| `MINIVMAC_WINDOW_SCALE` | `2` | Window scale factor |
| `MINIVMAC_FULLSCREEN_VAR` | `1` | Variable fullscreen |
| `MINIVMAC_FULLSCREEN_INIT` | `0` | Start in fullscreen |
| `MINIVMAC_SPEED` | `4` | Speed (0–5, where 4 = 8×) |

Note: Screen resolution, bit depth, and RAM size are now runtime — use `--screen` and `--ram` flags.

### Audio & Drives

| Option | Default | Description |
|--------|---------|-------------|
| `MINIVMAC_SOUND` | `1` | Enable sound |
| `MINIVMAC_NUM_DRIVES` | `6` | Number of floppy drives |

### Localization

| Option | Default | Description |
|--------|---------|-------------|
| `MINIVMAC_LANGUAGE` | `English` | UI language: English, French, German, Italian, Spanish, Dutch, PortugueseBrazilian, Polish, Czech, Serbian |

### Debug

| Option | Default | Description |
|--------|---------|-------------|
| `MINIVMAC_DBGLOG` | `1` | Enable debug logging |
| `MINIVMAC_ABNORMAL_REPORTS` | `0` | Enable abnormal-condition reports |
| `MINIVMAC_LOCALTALK` | `0` | Enable LocalTalk emulation |

## Legacy Build

The original build scripts and `setup/` tool remain in the `reference/` directory. They use a 3-stage pipeline:

1. Compile `setup/tool.c` → `setup_t`
2. Run `./setup_t` → generates `setup.sh` and config headers in `cfg/`
3. Run `setup.sh` → invokes `xcodebuild` or `make`

The CMake build replaces this pipeline with a single step. The `cfg/` directory still contains the original generated headers as a reference.
