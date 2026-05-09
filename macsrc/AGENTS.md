# Mac-Side Source Code

This directory contains source code for Mac applications that run
**inside** the emulated Mac, not on the host.  These are 68k programs
built with THINK C.

## Contents

### init/
**maxivmac INIT** (resource type `INIT`, ID 128) — The unified
host-integration INIT.  Provides clipboard synchronisation and shared
drive management via a single jGNEFilter.

| File     | Responsibility |
|----------|---------------|
| `init.c` | INIT entry point, jGNEFilter, trap stub generation, boot-time init |
| `clip.c` | Clipboard sync: export Mac scrap to host, import host scrap to Mac |
| `drive.c` | Shared drive trap handlers, FCB/VCB/DQE management, mount logic |
| `comm.c` | Extension discovery, register access helpers, debug logging |
| `defs.h` | Shared definitions, constants, `Globals` struct, prototypes |

See `init/INIT.md` for design details.

### importfl/
**ImportFl** — Application.  Imports a file from the host into the
emulated Mac filesystem.  Uses the disk and pbuf extensions.
Full GUI with progress bar and drag-and-drop support.

### exportfl/
**ExportFl** — Application.  Exports a file from the emulated Mac to
the host.  Uses the disk and pbuf extensions.  Full GUI with progress
bar and drag-and-drop support.

## Build Infrastructure

- **build.hfs** — A bootable Mac II / System 6.0.8 disk image with
  THINK C installed, used for compiling the guest-side code.
- **build.mac** — Machine config that launches the build disk with
  `macsrc/init` as the shared folder, so edits on the host are
  immediately visible inside the emulator.
