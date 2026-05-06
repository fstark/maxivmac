#!/bin/sh
# Integration tests for the scripting system.
# Requires a bootable disk image at test/fixtures/boot.hfs.
set -e

MAXIVMAC="${1:-./bld/macos/maxivmac}"

echo "=== boot_wait ==="
"$MAXIVMAC" --headless --mac "Mac Plus" \
    --disk test/fixtures/boot.hfs \
    --dbg-script=test/scripts/boot_wait.dbg

echo "=== must_timeout (expect failure) ==="
! "$MAXIVMAC" --headless --mac "Mac Plus" \
    --disk test/fixtures/boot.hfs \
    --dbg-script=test/scripts/must_timeout.dbg

echo "All script tests passed."
