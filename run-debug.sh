#!/bin/sh
# Launch the debug (cmake) emulator in Mac II mode (no disk for now)
DIR="$(cd "$(dirname "$0")" && pwd)"
# cp "$DIR/reference/mf2.hfs.reference" "$DIR/disk.hfs"
# cp "$DIR/reference/608.hfs.reference" "$DIR/disk.hfs"
exec "$DIR/bld/macos/maxivmac" \
    --model PB100 \
    --log-start=0 --log-count=1000000
