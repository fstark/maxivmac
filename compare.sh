#!/bin/sh
# Compare reference and debug build outputs for a given instruction window.
# Usage: ./compare.sh <log-start> <log-count>
# Example: ./compare.sh 50000 10000

set -e

if [ $# -ne 2 ]; then
    echo "Usage: $0 <log-start> <log-count>" >&2
    exit 1
fi

LOG_START="$1"
LOG_COUNT="$2"
DIR="$(cd "$(dirname "$0")" && pwd)"
TMP="$DIR/tmp"
mkdir -p "$TMP"

REF_OUT="$TMP/compare_ref.txt"
DBG_OUT="$TMP/compare_dbg.txt"

echo "=== Reference build: --log-start=$LOG_START --log-count=$LOG_COUNT ==="
cp "$DIR/reference/mf2.hfs.reference" "$DIR/reference/disk.hfs"
"$DIR/reference/minivmac.app/Contents/MacOS/minivmac" \
    --log-start="$LOG_START" --log-count="$LOG_COUNT" \
    "$DIR/reference/disk.hfs" 2>"$REF_OUT"
echo "  $(wc -l < "$REF_OUT") lines"

echo "=== Debug build: --log-start=$LOG_START --log-count=$LOG_COUNT ==="
cp "$DIR/reference/mf2.hfs.reference" "$DIR/disk.hfs"
"$DIR/bld/macos-cocoa/minivmac.app/Contents/MacOS/minivmac" \
    --model plus --rom="$DIR/extras/roms/vMac.ROM" \
    --log-start="$LOG_START" --log-count="$LOG_COUNT" \
    "$DIR/disk.hfs" 2>"$DBG_OUT"
echo "  $(wc -l < "$DBG_OUT") lines"

echo "=== Diff ==="
if diff "$REF_OUT" "$DBG_OUT"; then
    echo "IDENTICAL"
else
    echo "---"
    echo "Files: $REF_OUT  $DBG_OUT"
fi
