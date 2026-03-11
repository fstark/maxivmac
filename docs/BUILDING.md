# Building Mini vMac

## Quick Start (macOS)

```bash
cmake --preset macos-cocoa
cmake --build --preset macos-cocoa
```

The app bundle is at `bld/macos-cocoa/minivmac.app`. Drop a Mac II ROM file (`MacII.ROM`) next to the app and a System 7 disk image onto the window to boot.

## Requirements

- **CMake** ≥ 3.20
- **Ninja** (recommended) or Make
- **macOS:** Xcode command-line tools (provides Clang, AppKit, AudioUnit, OpenGL)
- **Linux:** SDL2 or SDL3 development libraries, X11
- **Windows:** SDL2, MinGW or MSVC

## Build Presets

| Preset | Platform | Backend | Notes |
|--------|----------|---------|-------|
| `macos-cocoa` | macOS | Cocoa/OpenGL | Default. Native macOS experience. |
| `macos-sdl` | macOS | SDL | For cross-platform testing. |
| `linux-sdl` | Linux | SDL | Default on Linux. |
| `windows-sdl` | Windows | SDL | Default on Windows. |

Usage:

```bash
cmake --preset <preset>
cmake --build --preset <preset>
```

## Manual Configuration

If you don't want to use presets:

```bash
cmake -B bld -G Ninja \
    -DMINIVMAC_BACKEND=cocoa \
    -DCMAKE_BUILD_TYPE=Release
cmake --build bld
```

## CMake Options

### Core

| Option | Default | Description |
|--------|---------|-------------|
| `MINIVMAC_BACKEND` | `auto` | `cocoa`, `sdl`, or `auto` (Cocoa on macOS, SDL elsewhere) |
| `MINIVMAC_MODEL` | `II` | Mac model. Currently: `II` |

### Display

| Option | Default | Description |
|--------|---------|-------------|
| `MINIVMAC_SCREEN_WIDTH` | `640` | Screen width in pixels |
| `MINIVMAC_SCREEN_HEIGHT` | `480` | Screen height in pixels |
| `MINIVMAC_SCREEN_DEPTH` | `3` | Bit depth (log2): 0=1bpp, 3=8bpp |
| `MINIVMAC_MAGNIFY_ENABLE` | `1` | Allow window magnification |
| `MINIVMAC_MAGNIFY_INIT` | `1` | Start magnified |
| `MINIVMAC_WINDOW_SCALE` | `2` | Window scale factor |
| `MINIVMAC_FULLSCREEN_VAR` | `1` | Variable fullscreen |
| `MINIVMAC_FULLSCREEN_INIT` | `0` | Start in fullscreen |
| `MINIVMAC_SPEED` | `4` | Speed (0–5, where 4 = 8×) |

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
| `MINIVMAC_DBGLOG` | `0` | Enable debug logging |
| `MINIVMAC_ABNORMAL_REPORTS` | `0` | Enable abnormal-condition reports |
| `MINIVMAC_LOCALTALK` | `0` | Enable LocalTalk emulation |

## Legacy Build

The original build scripts (`build_macos.sh`, `build_linux.sh`, etc.) and the `setup/` tool remain in the repository. They use a 3-stage pipeline:

1. Compile `setup/tool.c` → `setup_t`
2. Run `./setup_t` → generates `setup.sh` and config headers in `cfg/`
3. Run `setup.sh` → invokes `xcodebuild` or `make`

The CMake build replaces this pipeline with a single step. The `cfg/` directory still contains the original generated headers (for the Mac II / Cocoa / macOS target) as a reference.
