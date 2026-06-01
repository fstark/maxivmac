# maxivmac

> Just works. Finally hackable.

<!-- TODO: replace with actual logo once media/logo.png is created -->
<!-- ![maxivmac logo](media/logo.png) -->

<!-- TODO: add demo GIF/MP4 once media/demo-launcher.gif is created -->
<!-- ![Launcher demo](media/demo-launcher.gif) -->

---

## What is maxivmac?

maxivmac is a 68K Macintosh emulator built on the foundation of
[Mini vMac](https://www.gryphel.com/c/minivmac/) (by Paul C. Pratt).
A single binary covering the full classic Mac lineup — from
the original 128K through the Mac II family — with a graphical launcher,
transparent text and image clipboard sync, shared-drive file exchange, SLIP networking,
and an integrated 68K debugger.

It builds with standard CMake and C++. Just clone the repo, run one command,
and you can be reading — or changing — the code within minutes. Full
build time is less than 10 seconds on modern hardware.

**Supported platforms:** macOS (arm64 + x86_64) · Windows (x86_64 + arm64) · Linux

---

## Who is maxivmac for?

Do you want to relive the 80s? Just boot fullscreen and do a full Ultima III run under system 6. Toggle the CRT effect for an authentic Trinitron glow. Drag a folder onto the window and
it mounts on the desktop — getting files onto the Mac never gets in your way.

Or maybe you have maxivmac in a window next to your modern setup. Copy on the host, paste
in the guest, text is converted, images to, with sensible defautls. Twelve Mac models wait in the launcher so you can understand how different the Plus and SE were.

If like me, you're still writing software for Mac OS 6 or 7, this is designed for you. Your modern workflow can now apply to vintage macs. Edit in VSCode, share the folder, your code appears on the Mac ready to build — encoding handled. You can git add the AppleDouble files, everything, just work. You can script the emulator for automated builds or watch decoded trap calls stream by as the program runs, set breakpoints, and catch that nasty Ptr leak. And if you are into that kind of thing, an AI assistant can help you debug — it knows the Toolbox surprisingly well.

Or emulators fascinates you and you want to add a device, fix a bug, or extend the platform? The C++ codebase is written to be read and changed. It is not perfect, far from it, but it is now possible to improve collectively. Clone it, build in seconds, start hacking.

---

## Features

### Visual launcher

Look at models, hover for details, and click them to boot them. You can add models by creating a new ``.mac`` file, or completely skip that step via command line flags.

<!-- ![Launcher](media/screenshot-launcher.png) -->
> 📸 *Screenshot needed — see [media/MEDIA_NEEDED.md](media/MEDIA_NEEDED.md)*

---

### Transparent clipboard sync

Copy on the host, paste in the guest — and vice versa — with no manual
steps.  Supports both **text** (UTF-8 ↔ Mac OS Roman with automatic
line-ending translation) and **images** (PICT ↔ PNG with alpha compositing).

<!-- ![Clipboard demo](media/demo-clipboard.gif) -->
> 🎬 *Demo needed — see [media/MEDIA_NEEDED.md](media/MEDIA_NEEDED.md)*

---

### Shared drive

Drag any host folder onto the emulator window and it instantly mounts as
a read-write HFS volume on the Mac desktop — no disk image required.  Mount
up to six drives simultaneously.  Unmount by dragging to the Trash. Add one by dragging a folder from the host onto the emulated mac.

```sh
maxivmac --shared ~/Documents --shared /tmp disk.img
```

<!-- ![Shared drive demo](media/demo-shared-drive.gif) -->
> 🎬 *Demo needed — see [media/MEDIA_NEEDED.md](media/MEDIA_NEEDED.md)*

---

### Dynamic Mac II video

The Mac II supports resolutions up to 640×480 at depths from 1-bit
monochrome up to 32-bit true color, switchable live from the Monitors
control panel.  The host window resizes automatically.

<!-- ![Mac II resolution demo](media/demo-macii-resolution.gif) -->
> 🎬 *Demo needed — see [media/MEDIA_NEEDED.md](media/MEDIA_NEEDED.md)*

---

### SLIP networking

Connect the emulated Mac to the host's internet over a virtual serial port
using MacTCP 2.0.6 and built-in SLIP.  Runs Netscape 2.0, FTP, Telnet, and
anything else that speaks MacTCP — on System 6.0.8 and a Mac Plus.

```sh
maxivmac --serial-a=slip --slip-redir=tcp:8080:10.0.2.15:80 disk.img
```

<!-- ![SLIP demo](media/demo-slip.gif) -->
> 🎬 *Demo needed — see [media/MEDIA_NEEDED.md](media/MEDIA_NEEDED.md)*

---

### CRT post-process effect

Toggle a GLSL shader that adds scanline gaps, barrel (pincushion)
distortion, and a corner vignette — the closest a flat panel gets to a
vintage Trinitron.  Press **Ctrl** to open the overlay, then hit the CRT
button.

<!-- ![CRT effect](media/demo-crt.gif) -->
> 🎬 *Demo needed — see [media/MEDIA_NEEDED.md](media/MEDIA_NEEDED.md)*

---

### Built-in 68K debugger

Set breakpoints on addresses or Toolbox trap numbers, step through 68K
instructions, inspect memory and registers, watch trap calls with decoded
arguments, and explore Low Memory globals — all from a `(dbg)` prompt or
via `.dbg` script files.

```sh
maxivmac --debugger --dbg-script=my_script.dbg disk.img
```

<!-- ![Debugger](media/screenshot-debugger.png) -->
> 📸 *Screenshot needed — see [media/MEDIA_NEEDED.md](media/MEDIA_NEEDED.md)*

---

### Scriptable builds

maxivmac can be scripted end-to-end — boot a Mac, compile your project, grab the output,
shut down. No human in the loop. The same pipeline that builds the maxivmac INIT runs
unattended in CI.

```sh
maxivmac --dbg-script=build.dbg --verify=expected.golden disk.img
```

<!-- ![Scripted build demo](media/demo-scripted-build.gif) -->
> 🎬 *Demo needed — see [media/MEDIA_NEEDED.md](media/MEDIA_NEEDED.md)*

---

## Getting Started

### macOS — Homebrew *(coming soon)*

```sh
brew install fstark/tap/maxivmac
```

### Windows — ZIP download *(coming soon)*

Download the latest ZIP from the
[Releases](https://github.com/fstark/maxivmac/releases) page, extract, and
run `maxivmac.exe`.

### Linux — build from source

```sh
git clone https://github.com/fstark/maxivmac.git
cd maxivmac
cmake --preset linux
cmake --build --preset linux
```

The binary will be at `bld/linux/maxivmac`.  See [docs/BUILDING.md](docs/BUILDING.md)
for full build instructions.

---

## Quick Start

Launch the graphical launcher (no arguments):

```sh
maxivmac
```

Boot a specific machine directly:

```sh
# Mac Plus with System 6.0.8
maxivmac MacPlus+System6.mac

# Mac II from the command line
maxivmac --model=MacII --rom=MacII.ROM system7.img

# Share a host directory
maxivmac --model=MacPlus --shared ~/Desktop/shared MacPlus.mac
```

---

## CLI Reference

| Flag | Description |
|------|-------------|
| `--model=MODEL` | `MacPlus` `MacSE` `MacII` `MacIIx` `Classic` `PB100` `Mac128K` `Mac512Ke` … (default: `MacII`) |
| `--rom=PATH` | ROM file (auto-detected from model name if omitted) |
| `--romdir=DIR` | Additional directory to search for ROMs |
| `--ram=SIZE` | `1M` `2M` `4M` `8M` (default: model-specific) |
| `--screen=WxHxD` | e.g. `512x342x1`, `640x480x8` |
| `--speed=N` | `0`=1× `1`=2× `2`=4× `3`=8× `4`=16× `5`=32× |
| `--fullscreen` | Start in fullscreen mode |
| `--shared=PATH` | Mount host directory as HFS volume (repeatable) |
| `--serial-a=MODE` | Modem port: `loopback` `slip` `pty` `file:tx=…` |
| `--slip-redir=SPEC` | Port forward: `tcp:hostport:guestip:guestport` |
| `--debugger` | Start paused at first instruction with `(dbg)` prompt |
| `--dbg-script=FILE` | Load and execute a `.dbg` script at startup (repeatable) |
| `--record=PATH` | Record a golden file for non-regression testing |
| `--verify=PATH` | Verify against a golden file (exit 0 = pass) |
| `-h` / `--help` | Full option list |

---

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| **Ctrl** (tap) | Open control overlay (sticky — click buttons freely) |
| **Ctrl** (hold) | Peek at overlay (dismisses on release) |
| **Ctrl+F** | Toggle fullscreen |
| **Ctrl+M** | Toggle scaling mode (Pixel Perfect ↔ Stretched) |
| **Ctrl+Z** | Zoom to largest integer scale that fits the screen |
| **Ctrl+S** | Save screenshot to clipboard |
| **Escape** | Dismiss overlay |

---

## Building from Source

```sh
# macOS
./build-macos.sh          # equivalent to cmake --preset macos && cmake --build --preset macos

# Linux / Windows
cmake --preset linux      # or: windows
cmake --build --preset linux
```

See [docs/BUILDING.md](docs/BUILDING.md) for dependencies and advanced options.

---

## License & Credits

maxivmac is released under the **GNU General Public License v2** (inherited
from Mini vMac).

- **Mini vMac** — original emulator core by [Paul C. Pratt](https://www.gryphel.com/)
- **vMac** — the project Mini vMac grew from
- **UAE** — 68K CPU emulator lineage
- UI built with [Dear ImGui](https://github.com/ocornut/imgui) and
  [SDL3](https://libsdl.org/)


### Building the Kanji (Japanese Mac Plus) variant
The [recently discovered](https://web.archive.org/web/20250518175439/https://www.journaldulapin.com/2025/05/17/the-lost-japanese-rom-of-the-macintosh-plus-which-isnt-lost-anymore/) Japanese Mac Plus 256K ROM, which contains built-in KanjiTalk fonts for better performance, can now be used with Mini vMac. To emulate a Kanji model which can use this ROM, you can specify the new `-m Kanji` option in the setup tool. For example, this builds the Kanji variant for Apple Silicon, also enabling LocalTalk-over-UDP networking:

	./setuptool -n "minivmac-37.03-kanji" \  
	  -m Kanji -t mcar -lt -lto udp -sgn 0 > setup.sh

## Source Tree

```
src/
├── core/           Core emulation: machine glue, main loop, endian, defaults
├── cpu/            Motorola 68000/68020 emulator and instruction tables
├── devices/        Hardware device emulation (VIA, SCC, IWM, SCSI, ADB, etc.)
├── platform/       Platform backends (Cocoa, SDL, Win32, X11, etc.)
│   └── common/     Shared platform code: OS glue, control mode, intl chars, etc.
├── config/         Build configuration headers, language strings, Info.plist
└── resources/      Application resources (icons)
```

For detailed build instructions, see [docs/BUILDING.md](docs/BUILDING.md).

## Contributing

If you find any bugs and/or implement new features, please feel free to create a pull request. I will be more than happy to merge in any changes of general interest to keep Mini vMac alive and thriving.

## Further reference:
[Main development website](https://www.gryphel.com/)

[Mirror of main development website as of 05/25/22](https://minivmac.github.io/gryphel-mirror/index.html)

[State of affairs](https://www.emaculation.com/forum/viewtopic.php?t=11570)

