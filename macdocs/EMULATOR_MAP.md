# Emulator ↔ Documentation Cross-Reference

Maps each maxivmac source file to the relevant Apple documentation sections, and vice versa. Use this alongside [INDEX.md](INDEX.md) which has the exact line ranges for `read_file`.

## How to Use

1. Find your emulator source file in the **Code → Documentation** table below
2. Look up the documentation ID (e.g., `im029`) in [INDEX.md](INDEX.md) for the exact line range
3. Read the relevant section with `read_file`

For quick access to the most-needed hardware docs, see [ref/HARDWARE.md](ref/HARDWARE.md).

---

## Code → Documentation

### VIA (Versatile Interface Adapter)

| Source File | Description |
|-------------|-------------|
| `src/devices/via.h` | VIA1 — primary VIA |
| `src/devices/via2.h` | VIA2 — Macintosh II second VIA |
| `src/devices/via_base.h` | Shared VIA base class |

| Doc ID | File | Lines | Topic |
|--------|------|-------|-------|
| im029 §VIA | `im202.html` | 35353–35772 | VIA register A/B, peripheral control, timers, interrupts, other registers |
| im029 §Video | `im202.html` | 34806–34871 | VIA bits controlling screen buffer selection |
| im029 §Sound | `im202.html` | 34872–34947 | VIA bits controlling sound enable and volume |
| im019 §Interrupts | `im202.html` | 21850–22704 | Level-1 (VIA) and Level-2 (SCC) interrupt handling |
| tn010 | `tn405.html` | 787–1038 | Hardware pinouts including VIA connections |
| qa106–qa123 | `qa405.html` | 2646–2822 | Interrupt handling: VIA2 timer, VBL, slot interrupts |

### SCC (Serial Communications Controller)

| Source File | Description |
|-------------|-------------|
| `src/devices/scc.h` | Zilog Z8530 SCC — serial ports |

| Doc ID | File | Lines | Topic |
|--------|------|-------|-------|
| im029 §SCC | `im202.html` | 34948–35035 | SCC hardware: RS422, pinouts, clock, addresses, access timing |
| im042 | `im202.html` | 48958–49676 | Serial Drivers: baud rate, handshaking, control calls |
| tn010 | `tn405.html` | 787–1038 | DB-9 pinouts for serial ports |
| tn088 | `tn405.html` | 6307–6591 | Serial signal details |

### Keyboard & Mouse

| Source File | Description |
|-------------|-------------|
| `src/devices/keyboard.h` | Keyboard via VIA1 shift register |
| `src/devices/mouse.h` | Mouse position and button |
| `src/devices/adb.h` | Apple Desktop Bus controller (Mac II) |
| `src/devices/adb_shared.h` | Shared ADB code |

| Doc ID | File | Lines | Topic |
|--------|------|-------|-------|
| im029 §Keyboard | `im202.html` | 35036–35170 | Keyboard/keypad communication protocol via VIA |
| im029 §Mouse | `im202.html` | 35016–35035 | Mouse hardware (SCC port quadrature) |
| im010 | `im202.html` | 11405–11892 | Apple Desktop Bus: commands, registers, addressing, ADB Manager |
| im052 §Keyboard | `im202.html` | 54811–54880 | Keyboard events, key codes, Apple Extended Keyboard |
| tn160 | `tn405.html` | 15358–15522 | Key mapping tables |
| qa6–qa20 | `qa405.html` | 1136–1355 | ADB: init, addressing, timing, power, LEDs, conflict resolution |
| qa98–qa105 | `qa405.html` | 2518–2645 | Input devices: key status, mouse/cursor mechanism |

### IWM & Floppy Disk

| Source File | Description |
|-------------|-------------|
| `src/devices/iwm.h` | Integrated Woz Machine — floppy controller |
| `src/devices/sony.h` | Disk image access, Extension traps |

| Doc ID | File | Lines | Topic |
|--------|------|-------|-------|
| im029 §Disk | `im202.html` | 35171–35288 | IWM hardware: state-control lines, disk registers, read/write |
| im021 | `im202.html` | 24193–24720 | Disk Driver: direct access, control calls |
| im023 | `im202.html` | 25017–30929 | File Manager: volumes, directories, HFS |
| tn036 | `tn405.html` | 2351–2498 | Drive queue element data structure |
| tn070 | `tn405.html` | 4664–4725 | Forcing 400K or 800K disk format |
| qa50–qa59 | `qa405.html` | 1853–2037 | Disk driver: System 7 compat, floppy access, 800K sector mapping |

### SCSI

| Source File | Description |
|-------------|-------------|
| `src/devices/scsi.h` | SCSI bus controller |

| Doc ID | File | Lines | Topic |
|--------|------|-------|-------|
| im029 §SCSI | `im202.html` | 35289–35352 | SCSI hardware: NCR 5380 chip, base addresses |
| im040 | `im202.html` | 47915–48618 | SCSI Manager: SCSIGet/Select/Read/Write, transfer modes, disk partitioning |
| tn096 | `tn405.html` | 8087–8360 | Known SCSI Manager bugs and workarounds |

### Display & Video

| Source File | Description |
|-------------|-------------|
| `src/devices/screen.h` | Framebuffer-to-host display refresh |
| `src/devices/video.h` | Video mode management, Extension traps |
| `src/devices/screen_hack.h` | ROM patches for non-standard screen sizes |

| Doc ID | File | Lines | Topic |
|--------|------|-------|-------|
| im029 §Video | `im202.html` | 34806–34871 | Video interface: scanning, timing, screen buffers, pixel clock |
| im006 | `im202.html` | 3735–6499 | QuickDraw: coordinate plane, GrafPort, bit maps, drawing routines |
| im007 | `im202.html` | 6500–9331 | Color QuickDraw: CGrafPort, pixel maps, color cursors |
| im008 | `im202.html` | 9332–9826 | Graphics Devices: GDevice records, multiple screens |
| tn041 | `tn405.html` | 2602–2890 | Drawing into an off-screen bitmap |
| tn100 | `tn405.html` | 8393–8421 | Compatibility with large-screen displays |
| tn117 | `tn405.html` | 9200–9760 | Compatibility — screen memory assumptions |
| tn120 | `tn405.html` | 9786–11917 | Principia Off-Screen Graphics Environments (comprehensive) |
| qa65–qa87 | `qa405.html` | 2083–2378 | Display & Video: video RAM, CLUT, multiple displays, NuBus video |

### Sound

| Source File | Description |
|-------------|-------------|
| `src/devices/asc.h` | Apple Sound Chip |

| Doc ID | File | Lines | Topic |
|--------|------|-------|-------|
| im029 §Sound | `im202.html` | 34872–34947 | Sound hardware: sound buffer, PWM encoding, volume control, square-wave |
| im045 | `im202.html` | 51122–51848 | Sound Driver: square-wave, four-tone, free-form synthesizers |
| im046 | `im202.html` | 51849–53035 | Sound Manager: note/wave-table/sampled-sound synthesizers, snd resources |

### Real-Time Clock

| Source File | Description |
|-------------|-------------|
| `src/devices/rtc.h` | Real-Time Clock via VIA serial bit-bang |

| Doc ID | File | Lines | Topic |
|--------|------|-------|-------|
| im029 §RTC | `im202.html` | 35289–35352 | RTC hardware: accessing clock chip, one-second interrupt |
| im033 | `im202.html` | 39919–41062 | OS Utilities: date/time operations, Parameter RAM |

### ROM

| Source File | Description |
|-------------|-------------|
| `src/devices/rom.h` | ROM device |
| `src/devices/hpmac_hack.h` | ROM patches for Happy Mac icon variants |

| Doc ID | File | Lines | Topic |
|--------|------|-------|-------|
| im029 §ROM | `im202.html` | 34793–34805 | ROM base address, size, system traps in ROM |
| im037 | `im202.html` | 43199–44778 | Resource Manager: resources in ROM, overriding ROM resources |
| im048 | `im202.html` | 53724–54175 | Start Manager: system startup, hardware diagnostics, ROM patches |
| tn139 | `tn405.html` | 14048–14085 | Macintosh Plus ROM versions |

### Power Management Unit

| Source File | Description |
|-------------|-------------|
| `src/devices/pmu.h` | PMU for Portable Macs |

| Doc ID | File | Lines | Topic |
|--------|------|-------|-------|
| qa88 | `qa405.html` | 2379–2392 | Power Manager BatteryStatus: voltage and charger details |
| im043 | `im202.html` | 49677–49837 | Shutdown Manager: power-off, restart |

### Core Architecture

| Source File | Description |
|-------------|-------------|
| `src/core/machine_obj.h` | Machine object: owns all devices, CPU, memory |
| `src/core/wire_bus.h` | Inter-device signal routing |
| `src/core/ict_scheduler.h` | Cycle-based task scheduler |
| `src/core/device.h` | Abstract Device base class |

| Doc ID | File | Lines | Topic |
|--------|------|-------|-------|
| im029 | `im202.html` | 34656–35772 | Full hardware chapter — address space, all I/O devices |
| im004 | `im202.html` | 3022–3254 | Memory management intro: stack, heap, pointers, handles |
| im030 | `im202.html` | 35773–37396 | Memory Manager: heap zones, block structure |
| im051 | `im202.html` | 54615–54750 | Time Manager: millisecond wakeup service |
| im053 | `im202.html` | 55749–56039 | Vertical Retrace Manager: 60Hz VBL task scheduling |
| im005 | `im202.html` | 3255–3734 | trap dispatch, trap word format, calling conventions |
| im058 | `im202.html` | 59557–61449 | System Traps: complete trap number table |
| im059 | `im202.html` | 61450–61689 | Global Variables: low-memory addresses |
| tn176 | `tn405.html` | 17148–17524 | Macintosh memory configurations by model |

---

## Documentation → Code (Reverse Lookup)

For when you're reading documentation and want to find the corresponding emulator code.

| Doc Topic | Emulator Files |
|-----------|---------------|
| Address space / memory map | `src/core/machine_obj.h`, `src/core/machine.h` |
| VIA registers | `src/devices/via.h`, `src/devices/via_base.h`, `src/devices/via2.h` |
| SCC serial ports | `src/devices/scc.h` |
| Keyboard protocol | `src/devices/keyboard.h` |
| Mouse quadrature | `src/devices/mouse.h` |
| Apple Desktop Bus | `src/devices/adb.h`, `src/devices/adb_shared.h` |
| IWM floppy disk | `src/devices/iwm.h` |
| Sony disk drive | `src/devices/sony.h` |
| SCSI bus | `src/devices/scsi.h` |
| Video interface | `src/devices/screen.h`, `src/devices/video.h` |
| Sound hardware/ASC | `src/devices/asc.h` |
| Real-time clock | `src/devices/rtc.h` |
| Power management | `src/devices/pmu.h` |
| ROM | `src/devices/rom.h` |
| System traps | `src/devices/sony.h` (Extension traps), `src/devices/video.h` (Extension traps) |
| Interrupt dispatch | `src/core/ict_scheduler.h`, `src/core/wire_bus.h` |
| Screen buffers | `src/devices/screen.h`, `src/platform/screen_convert.h` |
| Sound buffers | `src/devices/asc.h`, `src/platform/sdl_sound.h` |

---

## Maintenance

When emulator code changes, update this file to keep cross-references valid:

- **Adding a new device**: Add an entry in the Code → Documentation section with the relevant doc IDs
- **Renaming/moving a device file**: Update all references to that file in both tables
- **Adding documentation**: Update [INDEX.md](INDEX.md) with line ranges, then add cross-references here
- **Changing device responsibilities**: Verify the doc links still match what the device actually emulates

The line ranges in this file come from [INDEX.md](INDEX.md). If the HTML files are regenerated or modified, re-verify the line numbers in INDEX.md and propagate changes here.
