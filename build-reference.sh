#!/bin/sh
# Build the reference Mac II emulator from the old setup-tool workflow
set -e
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT/reference"

# Build setup tool if needed
if [ ! -x ./setup_t ]; then
    gcc -o setup_t setup/tool.c
fi

# Copy Mac II ROM into reference directory
cp "$ROOT/extras/roms/MacII.ROM" "$ROOT/reference/MacII.ROM"

# Generate Xcode project for Mac II
./setup_t -maintainer "egon.rath@gmail.com" \
    -homepage "https://github.com/egrath" \
    -n "minivmac-3.7-test" \
    -e xcd \
    -t mcar \
    -m II \
    -hres 640 -vres 480 \
    -magnify 1 \
    -mf 2 \
    -sound 1 \
    -sony-sum 1 -sony-tag 1 \
    -speed 4 -ta 2 -em-cpu 2 -mem 8M \
    -chr 0 -drc 1 -sss 4 \
    -fullscreen 0 \
    -var-fullscreen 1 \
    -dis 1 \
    -api cco \
    > setup.sh

. ./setup.sh
xcodebuild

echo "Build complete: $ROOT/reference/minivmac.app"
