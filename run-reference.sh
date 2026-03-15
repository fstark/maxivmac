#!/bin/sh
# Launch the reference Mac II emulator (no disk for now)
ROOT="$(cd "$(dirname "$0")" && pwd)"
# cp "$ROOT/reference/mf2.hfs.reference" "$ROOT/reference/disk.hfs"
exec "$ROOT/reference/minivmac.app/Contents/MacOS/minivmac"
