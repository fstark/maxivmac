# maxivmac Glossary

Canonical terminology for all project documentation, code comments,
plans, skills, and conversations.  This is the single source of truth.
When in doubt, use the term as defined here.

Each entry follows this schema:

- **Definition** — 1–3 sentences.  Present tense.  What it *is*.
- **Code:** symbol(s) — optional, only when a code identifier exists.
- **Examples:** list — optional, only when instances are domain terms.
- **See also:** related terms — optional.
- **Avoid:** rejected synonyms — optional.

---

## maxivmac

Terms invented or defined by this project — architecture, features,
build, test, and distribution concepts.  Litmus test: "we named this."

### ATT (Address Translation Table)

Maps 68k memory addresses to devices and memory buffers.  Determines
which device handles which address range.

Code: `ATTer` struct in `src/core/machine.h`

### Backend

The platform-specific rendering and I/O layer.  Implements the
`PlatformBackend` interface.  Three implementations: SDL3 (primary),
ImGui+SDL3+OpenGL3 (overlay UI), Headless (CI testing).

Code: `PlatformBackend` in `src/platform/platform_backend.h`

Avoid: renderer, driver, platform layer

### Device

A virtual hardware component implementing the `Device` interface.
Each device has `access()` (memory-mapped I/O), `reset()`, and
`zap()` methods.  Owned by the Rig.

Code: `Device` base class in `src/devices/device.h`

Examples: VIA1, VIA2, SCC, IWM, SCSI, ASC, RTC, ADB

### EmulatorShell

Platform-independent orchestration layer.  Holds state machines,
timing, framebuffer, and event routing.  Sits between the backend
and the emulation core.

Code: `EmulatorShell` in `src/platform/emulator_shell.h`

### Extension Interface

The host↔guest communication protocol.  Uses a register block at
`extnBlockBase` discovered via `SonyVarsPtr` ($0134).  Commands are
32-byte parameter blocks written to a fixed address.

Code: `extnBlockBase` in `src/devices/extn.h`

### Golden Test

A recorded reference output (screen framebuffer hash sequence) from
a known-good emulation run.  The headless backend replays the same
input and compares output against the golden file.

Avoid: golden file (when referring to the test), regression test

### Homebrew Tap

A third-party Homebrew repository hosting the maxivmac formula.
macOS users install via `brew install maxivmac`.  Builds from source,
bypassing Gatekeeper/notarization.

### ICTScheduler

Cycle-based task scheduler.  Devices schedule future interrupts by
cycle count.

Code: `ICTScheduler` in `src/core/ict_scheduler.h`

Avoid: timer, interrupt dispatcher

### Launcher

The pre-boot screen showing available Macintoshes as clickable cards.
Greyed-out cards show why they can't boot (ROM missing, no boot disk).

Avoid: model selector, start screen

### Macintosh

A saved, user-owned configuration based on a Model.  Specifies boot
disk, RAM size, shared drive folder, and other per-instance settings.

See also: Model, Rig

Avoid: VM, emulator instance, profile

### Rig

The live runtime emulation engine.  Created when a Macintosh boots,
destroyed when it shuts down.  Owns memory buffers, devices, wire bus,
ICT scheduler.  One Rig = one running emulated Mac.

Code: `Rig` class in `src/core/rig.h`, global `g_rig`

See also: Macintosh, Model

Avoid: emulator instance, VM, session

### Model

A static, immutable description of a Macintosh hardware platform —
CPU, devices, ROM, screen geometry, compatible OS versions.  Ships
with the app.  The user never edits a Model.

Code: `MacModel` enum, `MachineConfig` struct in `src/core/machine_config.h`

Examples: Mac Plus, Macintosh II

Avoid: profile, template, configuration

### Overlay

The Ctrl-activated UI panel drawn over the emulator viewport.  Shows
controls (Insert Disk, Fullscreen, Speed, etc.) and status.  Two
activation modes: hold (peek) and tap (sticky).

Avoid: HUD, menu, toolbar

### Shared Drive

A host folder mounted as a read-only or read-write Mac volume inside
the guest.  The INIT patches File Manager traps to redirect file
operations to the host filesystem via the extension interface.

See also: Extension Interface

Avoid: shared folder, host mount, network drive

### Tools Disk

A read-only HFS disk image bundled with the distribution, containing
the INIT ready to be drag-installed onto any guest OS.  Mounted
automatically when the host detects the INIT is not installed.

### WireBus

Inter-device signal routing.  Devices read/write named wires instead
of calling each other directly.

Code: `WireBus` in `src/core/wire_bus.h`

Avoid: signal bus, event bus

---

## Classic Mac

Terms defined by Apple — hardware chips, Mac OS concepts, and system
software vocabulary.  Litmus test: "you could find this in Inside
Macintosh."

### ADB (Apple Desktop Bus)

Serial bus connecting keyboards, mice, and other input devices on
Mac SE and later.

### ASC (Apple Sound Chip)

Custom sound synthesis and playback chip in the Macintosh II family.

### HFS (Hierarchical File System)

The Mac OS filesystem used on hard disks and large floppies.
Organizes files into folders with a B-tree catalog.

### INIT

A classic Mac OS system extension (code resource) that loads at boot
from the System Folder.

Avoid: plugin, driver, extension (ambiguous with Extension Interface)

### IWM (Integrated Woz Machine)

Floppy disk controller chip.  Handles low-level read/write to 400K
and 800K floppy drives.

Avoid: floppy drive, disk controller (too generic)

### MacRoman

The character encoding used by classic Mac OS.  Maps bytes 0x80–0xFF
to accented letters and typographic symbols.  All guest-visible
strings are MacRoman; the host uses UTF-8.

Code: `src/util/macroman.h`

### RTC (Real-Time Clock)

Battery-backed clock chip providing date, time, and a small amount
of parameter RAM (PRAM).

### SCC (Serial Communications Controller)

Zilog 8530 dual-channel serial controller.  Drives the modem and
printer ports.

### SCSI (Small Computer System Interface)

Parallel bus for hard disks and other peripherals on Mac Plus and
later.

### Trap

A 68k A-line exception used by Mac OS to dispatch Toolbox and OS
calls.  The trap number identifies the routine.

### VIA (Versatile Interface Adapter)

General-purpose I/O and timer chip.  Mac Plus has one VIA; Mac II
has two (VIA1 and VIA2).
