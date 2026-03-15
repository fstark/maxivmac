#!/bin/sh
# Test self-consistency of a single build.
# Usage: ./selftest.sh <ref|debug> <log-start> <log-count> <runs>
# Example: ./selftest.sh ref 0 5 3
#   Runs the reference build 3 times at --log-start=0 --log-count=5
#   and reports whether all outputs are identical.

set -e

if [ $# -ne 4 ]; then
    echo "Usage: $0 <ref|debug> <log-start> <log-count> <runs>" >&2
    exit 1
fi

BUILD="$1"
LOG_START="$2"
LOG_COUNT="$3"
RUNS="$4"
DIR="$(cd "$(dirname "$0")" && pwd)"
TMP="$DIR/tmp"
mkdir -p "$TMP"

run_ref() {
    # No disk for Mac II testing
    "$DIR/reference/minivmac.app/Contents/MacOS/minivmac" \
        --log-start="$LOG_START" --log-count="$LOG_COUNT" \
        2>"$1"
}

run_debug() {
    # No disk for Mac II testing
    "$DIR/bld/macos-cocoa/maxivmac.app/Contents/MacOS/maxivmac" \
        --model II --rom="$DIR/extras/roms/MacII.ROM" \
        --log-start="$LOG_START" --log-count="$LOG_COUNT" \
        2>"$1"
}

echo "=== Self-test: $BUILD  start=$LOG_START count=$LOG_COUNT runs=$RUNS ==="

i=1
while [ "$i" -le "$RUNS" ]; do
    OUT="$TMP/selftest_${BUILD}_${i}.txt"
    if [ "$BUILD" = "ref" ]; then
        run_ref "$OUT"
    else
        run_debug "$OUT"
    fi
    LINES=$(wc -l < "$OUT")
    echo "  run $i: $LINES lines"
    i=$((i + 1))
done

echo "=== Comparing all runs against run 1 ==="
ALL_OK=true
i=2
while [ "$i" -le "$RUNS" ]; do
    A="$TMP/selftest_${BUILD}_1.txt"
    B="$TMP/selftest_${BUILD}_${i}.txt"
    if diff -q "$A" "$B" > /dev/null 2>&1; then
        echo "  run 1 vs run $i: IDENTICAL"
    else
        echo "  run 1 vs run $i: DIFFERENT"
        diff "$A" "$B" | head -6
        ALL_OK=false
    fi
    i=$((i + 1))
done

if $ALL_OK; then
    echo "RESULT: ALL IDENTICAL"
else
    echo "RESULT: NON-DETERMINISM DETECTED"
fi
