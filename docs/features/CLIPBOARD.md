# Clipboard Sync

Automatic, transparent clipboard synchronization between the emulated
Mac and the host OS.  Cut or copy on either side and the text appears
on the other — no user action required.

## Setup

The ClipSync INIT must be installed in the guest Mac's System file.
Until packaging is automated, this is a manual step — drag the INIT
into the System Folder and reboot the emulated Mac.

## Limitations

- **TEXT only.** PICT and other scrap types are not synced.
- **Private-scrap apps** (e.g. THINK C) only see host clipboard
  updates after a real MultiFinder context switch (activate Finder and
  back).  The desk scrap is updated immediately, but private-scrap
  apps cache their clipboard internally and only refresh on a genuine
  resume + `convertClipboard` event.  This is authentic Mac behavior.
- **Large clipboard** (> 4 KB) is untested.

## What it does

- **Host → Mac:** text copied on the host appears in the Mac's desk
  scrap within ~0.5 seconds.
- **Mac → Host:** text copied on the Mac is pushed to the host
  clipboard immediately.
- **Encoding:** UTF-8 on the host, Mac OS Roman on the guest.
  Conversion and line-ending translation (LF ↔ CR) happen
  automatically.
- **Data types:** TEXT only.  PICT is not yet supported.

## Architecture

```
┌──────────────────────────────────────────────────┐
│  Guest Mac                                       │
│                                                  │
│  Application ──► Desk Scrap (ScrapCount @ $0968) │
│       ▲                      │                   │
│       │                      ▼                   │
│  ClipSync INIT (jGNEFilter)                      │
│    polls ScrapCount + ClipSeqNo every 30 ticks   │
│    reads/writes desk scrap via Toolbox calls      │
│    talks to host via register block I/O           │
└──────────────┬───────────────────────────────────┘
               │  memory-mapped registers
               │  at extnBlockBase + $20
               ▼
┌──────────────────────────────────────────────────┐
│  Emulator (C++)                                  │
│                                                  │
│  extn_clip.cpp                                   │
│    dispatches clipboard commands ($100–$108)      │
│    reads/writes guest RAM directly                │
│    converts UTF-8 ↔ Mac OS Roman                 │
│              │                                   │
│              ▼                                   │
│  clipboard.cpp (platform layer)                  │
│    SDL_GetClipboardText / SDL_SetClipboardText   │
└──────────────────────────────────────────────────┘
```

## Components

### Register block I/O (`machine.cpp`)

A 32-byte register block at `extnBlockBase + $20`, alongside the
legacy 32-byte extension block at `extnBlockBase + $00`.  The ATT
handler dispatches by word offset: 0–15 → legacy path, 16–31 → new
register handler.

| Offset | Size | Name    | Direction | Description                        |
|--------|------|---------|-----------|------------------------------------|
| +$00   | word | command | W         | Function code; write triggers call |
| +$02   | word | result  | R         | Return code (0 = ok)               |
| +$04   | long | p0      | R/W       | Parameter 0                        |
| +$08   | long | p1      | R/W       | Parameter 1                        |
| +$0C   | long | p2      | R/W       | Parameter 2                        |
| +$10   | long | p3      | R/W       | Parameter 3                        |
| +$14   | long | p4      | R/W       | Parameter 4                        |
| +$18   | long | p5      | R/W       | Parameter 5                        |
| +$1C   | long | p6      | R/W       | Parameter 6                        |

The 68k side writes parameters first, then writes the command word to
trigger the call.  After the write returns, the result and output
parameters are available for reading.

Address varies by model:
- Compact Macs (Plus, SE, Classic, etc.): `$F0C020`
- Mac II / IIx: `$50F0C020`

### Clipboard commands (`extn_clip.cpp`)

| Command     | Code  | Parameters                          | Returns                    |
|-------------|-------|-------------------------------------|----------------------------|
| ClipVersion | $100  | —                                   | p0 = version (currently 2) |
| ClipExport  | $101  | p0 = buffer addr, p1 = byte count   | result                     |
| ClipImport  | $102  | p0 = buffer addr, p1 = capacity     | p1 = actual byte count     |
| ClipHasData | $103  | —                                   | p0 = 1 if host has text    |
| ClipGetLen  | $104  | —                                   | p0 = byte count            |
| ClipSeqNo   | $105  | —                                   | p0 = sequence number       |
| ClipKVSet   | $106  | p0 = key, p1 = value                | result                     |
| ClipKVGet   | $107  | p0 = key                            | p0 = value (0 if unset)    |
| ClipDbgLog  | $108  | p0 = format string addr, p1–p6 args | result                     |

`ClipSeqNo` increments whenever the host clipboard changes.  The guest
polls it to detect new content without reading the full clipboard.
After a `ClipExport`, the sequence number is not bumped — this
prevents feedback loops.

`ClipKVSet` / `ClipKVGet` provide a host-side key-value store that
the INIT uses to track per-app sync state under MultiFinder (avoids
A5/globals issues across partitions).

### Platform layer (`clipboard.h` / `clipboard.cpp`)

Three functions, backed by SDL3:

- `hostClipHasText()` — wraps `SDL_HasClipboardText()`
- `hostClipGetTextMacRoman()` — gets UTF-8, converts to Mac OS Roman,
  swaps LF → CR
- `HostClipSetText(buf, len)` — swaps CR → LF, converts Mac OS Roman
  to UTF-8, calls `SDL_SetClipboardText()`

Without SDL (`HAVE_SDL` undefined), all functions return false/empty.

### ClipSync INIT (`macsrc/clipsync/init.c`)

A small 68k INIT (THINK C code resource) loaded at boot from the
System file.  This is the recommended Mac-side component for all
configurations including MultiFinder.

**Install sequence:**

1. Finds extension register base via the `SonyVarsPtr` chain ($0134)
2. Verifies `ClipVersion` ≥ 2
3. Allocates `FilterGlobals` in the system heap
4. Stores globals pointer at low-memory $0B00
5. Saves previous `jGNEFilter` ($029A), installs its own
6. Seeds per-app KV state for the startup app
7. `DetachResource` + `HLock` to stay resident

**Sync loop (fires on every `GetNextEvent` / `WaitNextEvent`):**

- Throttled to every 30 ticks (~0.5 s)
- Uses `CurApRefNum` ($0900) as per-app key
- **Host → Mac:** compares `ClipSeqNo` vs KV-stored last sequence;
  if different → `ClipGetLen` + `ClipImport` + `ZeroScrap` +
  `PutScrap('TEXT', ...)`
- **Mac → Host:** compares low-memory `ScrapCount` ($0968) vs
  KV-stored last count; if different → `GetScrap('TEXT', ...)` +
  `ClipExport`
- Properly chains to any previous jGNEFilter

Because the jGNEFilter runs in each app's partition context, it
bypasses MultiFinder's per-partition scrap isolation.

### ClipSync console app (`macsrc/clipsync/main.c`)

A standalone THINK C console application that performs the same
bidirectional sync in a polling loop.  Useful for testing or for
single-app (non-MultiFinder) environments.

### Legacy DAs (`macsrc/clipin/`, `macsrc/clipout/`)

`ClipIn` and `ClipOut` are classic desk accessories using the older
parameter-block extension interface.  They perform one-shot
host→Mac and Mac→host transfers respectively, then auto-close.

## 68k calling convention

Four instructions per call:

```asm
; ClipExport: push Mac clipboard to host
;   a0 = pointer to text data
;   d0 = byte count
    move.l  a0, $F0C024       ; p0 = buffer address
    move.l  d0, $F0C028       ; p1 = byte count
    move.w  #$0101, $F0C020   ; command = ClipExport (triggers call)
    move.w  $F0C022, d0       ; read result
```

## Source files

| File | Purpose |
|------|---------|
| `src/core/extn_clip.h` | Dispatcher interface |
| `src/core/extn_clip.cpp` | Command handlers + debug console |
| `src/platform/common/clipboard.h` | Platform abstraction |
| `src/platform/common/clipboard.cpp` | SDL3 implementation + encoding |
| `src/core/machine.cpp` | Register block ATT dispatch |
| `src/core/machine_config.h` | Extension block base address + size |
| `macsrc/clipsync/init.c` | jGNEFilter INIT (recommended) |
| `macsrc/clipsync/main.c` | Bidirectional sync console app |
| `macsrc/clipin/ClipIn.c` | Host→Mac desk accessory (legacy) |
| `macsrc/clipout/ClipOut.c` | Mac→Host desk accessory (legacy) |
