#!/bin/sh
# Launch the reference Mac Plus emulator (reproducible: fresh disk each run)
ROOT="$(cd "$(dirname "$0")" && pwd)"
cp "$ROOT/reference/608.hfs.reference" "$ROOT/reference/608.hfs"
exec "$ROOT/reference/minivmac.app/Contents/MacOS/minivmac" "$ROOT/reference/608.hfs"
