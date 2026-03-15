#!/bin/sh
# Launch the debug (cmake) emulator in Mac II mode (no disk for now)
DIR="$(cd "$(dirname "$0")" && pwd)"
# cp "$DIR/reference/mf2.hfs.reference" "$DIR/disk.hfs"
# cp "$DIR/reference/608.hfs.reference" "$DIR/disk.hfs"
exec "$DIR/bld/macos-cocoa/minivmac.app/Contents/MacOS/minivmac" \
    --model II \
    --rom=extras/roms/MacII.ROM
