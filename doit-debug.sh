#!/bin/sh
# Launch the debug (cmake) emulator in Mac Plus mode (reproducible: fresh disk each run)
DIR="$(cd "$(dirname "$0")" && pwd)"
cp "$DIR/reference/608.hfs.reference" "$DIR/608.hfs"
exec "$DIR/bld/macos-cocoa/minivmac.app/Contents/MacOS/minivmac" \
    --model plus \
    --rom=extras/roms/vMac.ROM \
    "$DIR/608.hfs"
