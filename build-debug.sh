#!/bin/sh
# Build the debug (cmake) version of the emulator
set -e
cd "$(cd "$(dirname "$0")" && pwd)"
cmake --preset macos-cocoa
cmake --build --preset macos-cocoa
echo "Build complete: bld/macos-cocoa/minivmac.app"
