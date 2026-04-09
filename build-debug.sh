#!/bin/sh
# Build the debug (cmake) version of the emulator
set -e
cd "$(cd "$(dirname "$0")" && pwd)"
cmake --preset macos
cmake --build --preset macos
echo "Build complete: bld/macos/maxivmac"

# Also build headless for comparison/selftest scripts
cmake --preset macos-headless
cmake --build --preset macos-headless
echo "Build complete: bld/macos-headless/maxivmac"
