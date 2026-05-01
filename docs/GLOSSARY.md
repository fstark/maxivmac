# maxivmac Glossary

Common language for all project documentation, code comments, issues,
and conversations.

---

## Core Concepts

### Model

What Apple designed. A static, immutable description of a Macintosh
hardware platform — CPU, devices, ROM, screen geometry, compatible OS
versions. Ships with the app. The user never edits a Model.

Examples: Macintosh Plus, Macintosh II.

In code: `MacModel` enum, `MachineConfig` struct (factory-produced
from model).

### Macintosh

What's on your desk. A saved, user-owned configuration based on a
Model. Specifies boot disk, RAM size, shared drive folder, and other
per-instance settings. In v1.0, each Model produces exactly one
hardcoded Macintosh. In v1.1+, users can create and customize their
own.

In code: not yet implemented as a class (v1.0 uses hardcoded
defaults; v1.1 will introduce a `Macintosh` struct/class).

### Rig

The live runtime emulation engine. Created when a Macintosh boots,
destroyed when it shuts down. Owns memory buffers, devices, wire bus,
ICT scheduler. One Rig = one running emulated Mac.

In code: `Rig` class (currently named `Machine` in `machine_obj.h`,
rename pending). Global: `g_rig`.

---

## Hardware Emulation

### Device

A virtual hardware component implementing the `Device` interface.
Each device has `access()` (memory-mapped I/O), `reset()`, and
`zap()` methods. Owned by the Rig.

Examples: VIA1, VIA2, SCC, IWM, SCSI, ASC, RTC, ADB.

### WireBus

Inter-device signal routing. Replaces direct function calls between
devices with a shared signal bus. Devices read/write named wires.

### ICTScheduler

Cycle-based task scheduler. Devices schedule future interrupts by
cycle count. Replaces the original global ICT dispatch.

### ATT (Address Translation Table)

Maps 68k memory addresses to devices and memory buffers. The central
nervous system of the emulator — determines which device handles
which address.

---

## Guest-Side

### INIT

A classic Mac OS system extension (code resource) that loads at boot
from the System Folder. maxivmac ships a merged INIT combining
clipboard sync and shared drive functionality. Must be installed on
the guest boot disk.

### Extension Interface

The host↔guest communication protocol. Uses a register block at
`extnBlockBase` discovered via `SonyVarsPtr` ($0134). Commands are
32-byte parameter blocks written to a fixed address. This interface
must be stable after v1.0 — changing it breaks deployed INITs.

### Shared Drive

A host folder mounted as a read-only or read-write Mac volume inside
the guest. The INIT patches ~40 File Manager traps to redirect file
operations to the host filesystem via the extension interface.

### Tools Disk

A read-only HFS disk image bundled with the distribution, containing
the INIT ready to be drag-installed onto any guest OS. Mounted
automatically when the host detects the INIT is not installed.
Planned for v1.1.

### MacRoman

The character encoding used by classic Mac OS (pre-OS X). Maps bytes
0x00–0x7F to ASCII and 0x80–0xFF to 128 non-ASCII characters
(accented letters, typographic symbols, etc.). All guest-visible
strings — filenames, clipboard text, disk image names — are MacRoman.
The host uses UTF-8. Conversions live in `src/util/macroman.h`; this
is the single canonical location — do not add conversion code
elsewhere.

---

## Platform Layer

### Backend

The platform-specific rendering and I/O layer. Implements
`PlatformBackend` interface. Three implementations: SDL3 (primary),
ImGui+SDL3+OpenGL3 (overlay UI), Headless (CI testing).

### EmulatorShell

Platform-independent orchestration. Holds state machines, timing,
framebuffer, event routing. Sits between the backend and the
emulation core.

### Overlay

The Ctrl-activated UI panel drawn over the emulator viewport. Shows
controls (Insert Disk, Fullscreen, Speed, etc.) and status. Two
activation modes: hold (peek) and tap (sticky).

### Launcher

The pre-boot screen showing available Macintoshes as clickable cards.
Replaces the previous "model selector" concept. In v1.0, shows only
hardcoded Macintoshes. Greyed-out cards show why they can't boot
(ROM missing, no boot disk).

---

## Build and Distribution

### Golden File / Golden Test

A recorded reference output (screen framebuffer hash sequence) from a
known-good emulation run. Used for non-regression testing — the
headless backend replays the same input and compares output against
the golden file.

### Homebrew Tap

A third-party Homebrew repository hosting the maxivmac formula.
macOS users install via `brew install maxivmac`. Builds from source,
bypassing Gatekeeper/notarization entirely.

---

## Versioning

- **1.0.0** — First public release.
- **1.0.x** — Bug fixes on the 1.0 line.
- **1.1.0** — Feature release (Macintosh concept, tools disk, etc.).
- Version numbers serve communication, not strict semver semantics.
