# Non-Regression Tests

Binary golden-file tests that verify the emulator produces identical CPU state
across builds.  Each `.golden` file contains compact snapshots (CPU registers
every 100K instructions for 20M instructions) recorded against a specific
model, ROM and disk combination.

## Prerequisites

- The emulator must be built: `cmake --build --preset macos`
- ROMs must exist in `roms/` (one per model)
- The reference disk `extras/disks/608.hfs` must exist

## Quick Start

```sh
./test/verify.sh           # verify all models
./test/record.sh           # re-record all models
./test/record.sh Plus      # re-record one model
```

`verify.sh` discovers all `test/*.golden` files, copies a fresh `608.hfs`
for each run, and reports PASS/FAIL per model (exit nonzero on any failure).

`record.sh` re-records golden files for the models listed in the script
(currently Plus and II).  Pass model names as arguments to record specific ones.

## Golden File Naming

Golden files are named `<MODEL>.golden` where `<MODEL>` is the canonical
model name (= ROM base name), matching the `--model=` CLI argument:

| File | `--model=` | ROM |
|------|-----------|-----|
| `MacPlus.golden` | `MacPlus` | `roms/MacPlus.ROM` |
| `MacII.golden` | `MacII` | `roms/MacII.ROM` |

To add a new model:

1. Add it to the `ALL_MODELS` list in `record.sh`
2. Run `./test/record.sh <MODEL>`
3. Verify: `./test/verify.sh`

## Recording Options

| Flag | Default | Purpose |
|------|---------|---------|
| `--record=PATH` | — | Record golden file |
| `--verify=PATH` | — | Verify against golden file (exit 0=pass) |
| `--snapshot-interval=N` | 100000 | Instructions between snapshots |
| `--max-instructions=N` | 20000000 | Total instruction budget |

## How It Works

The `StateRecorder` (`src/core/state_recorder.hpp`) captures a fixed-size
snapshot of CPU state (PC, SR, D0–D7, SP) plus a rolling CRC32 of all I/O
operations at regular intervals.  The golden file header includes MD5
hashes of the ROM and disk image, so misconfigured runs are caught
immediately.

On verify, any mismatch prints a field-by-field diff and exits nonzero.
Use `--log-start=N --log-count=M` to get a full text trace around the
divergence point for debugging.

## Event Queue Coverage

Golden tests implicitly validate event queue delivery — keyboard and mouse
input during boot affects the state recorder's I/O CRC output.  The event
queue refactor (`src/platform/common/event_queue.h`) was validated by
confirming all 6 model golden files pass unchanged.
