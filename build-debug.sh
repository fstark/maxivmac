#!/bin/sh
# Build the debug (cmake) version of the emulator
set -e
cd "$(cd "$(dirname "$0")" && pwd)"
cmake --preset macos -DMINIVMAC_SPEED=4
cmake --build --preset macos
echo "Build complete: bld/macos/maxivmac"
