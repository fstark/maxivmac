# Mac-Side Source Code

This directory contains source code for Mac applications that run
**inside** the emulated Mac, not on the host.  These are 68k programs
built with MPW or THINK C.

They were extracted from the HFS disk images in `extras/utils/` for
reference.  The original minivmac project distributed them only as
pre-built applications inside HFS images.

## Contents

### clipin/
**ClipIn** — Desk accessory.  Imports the host clipboard into the Mac
scrapbook.  Calls `HTCImport` via the extension mechanism, copies the
result into the Mac scrap with `PutScrap`.

### clipout/
**ClipOut** — Desk accessory.  Exports the Mac scrapbook to the host
clipboard.  Calls `GetScrap` to read the Mac scrap, then `HTCExport`
to send it to the host.

### clipsync/
**ClipSync** — Console app (THINK C ANSI project).  Imports the host
clipboard into the Mac scrap using the new register-block I/O
interface at `extnBlockBase + $20`.  No pbufs — the host writes
directly to guest RAM.  See `CLIPBOARD_PLAN.md` for the protocol.

### importfl/
**ImportFl** — Application.  Imports a file from the host into the
emulated Mac filesystem.  Uses the disk and pbuf extensions.
Full GUI with progress bar and drag-and-drop support.

### exportfl/
**ExportFl** — Application.  Exports a file from the emulated Mac to
the host.  Uses the disk and pbuf extensions.  Full GUI with progress
bar and drag-and-drop support.

## Key file: ExtnGlue.i

Shared glue library (included by all four programs) that implements
the 68k side of the extension calling convention:

- Reads `SonyVarsPtr` (`$0134`) to find the extension block base
  address (`pokeaddr`).
- Discovers extension IDs via `kExtnFindExtn` using magic numbers.
- Provides wrappers for pbufs (`PbufNew`, `PbufDispose`,
  `PbufTransfer`, `PBufGetSize`) and clipboard (`HTCExport`,
  `HTCImport`).
- Calling convention: fill a 32-byte parameter block, write its
  address to `pokeaddr` — a single memory-mapped write triggers the
  host-side dispatch.
