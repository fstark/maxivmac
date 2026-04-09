#!/bin/sh
# Non-regression test runner.
# Verifies every model that has a .golden file in test/.
# Golden files are named <MODEL>.golden where MODEL = ROM base name.
#
# Usage:  ./test/verify.sh
# Exit:   0 if all models pass, 1 on first failure.
#
# Override the emulator binary:  EMU=./bld/macos-headless/maxivmac ./test/verify.sh

set -e

DIR="$(cd "$(dirname "$0")/.." && pwd)"
EMU="${EMU:-$DIR/bld/macos-headless/maxivmac}"
ROMS="$DIR/roms"
DISK_SRC="$DIR/extras/disks/608.hfs"
TESTDIR="$DIR/test"
DISK_TMP="$TESTDIR/.608_verify.hfs"

# ── Sanity checks ──────────────────────────────────────
if [ ! -x "$EMU" ]; then
    echo "ERROR: emulator not found at $EMU"
    echo "       Run: cmake --build --preset macos"
    exit 1
fi
if [ ! -f "$DISK_SRC" ]; then
    echo "ERROR: reference disk not found at $DISK_SRC"
    exit 1
fi

# ── Run tests ──────────────────────────────────────────
# Model name = golden file stem = ROM base name (e.g. MacPlus → MacPlus.ROM)
PASS=0
FAIL=0
SKIP=0

for golden in "$TESTDIR"/*.golden; do
    [ -f "$golden" ] || continue

    MODEL="$(basename "$golden" .golden)"
    ROM_PATH="$ROMS/$MODEL.ROM"

    if [ ! -f "$ROM_PATH" ]; then
        echo "SKIP  $MODEL  ($MODEL.ROM not found)"
        SKIP=$((SKIP + 1))
        continue
    fi

    # Fresh disk copy for each model (emulator writes to disk)
    cp "$DISK_SRC" "$DISK_TMP"

    printf "%-14s " "$MODEL"
    if "$EMU" --model="$MODEL" --rom="$ROM_PATH" --scale=1 --silent\
              --verify="$golden" "$DISK_TMP" 2>&1 \
       | grep -q "^PASS"; then
        echo "PASS"
        PASS=$((PASS + 1))
    else
        echo "FAIL"
        # Re-run to show the failure details
        cp "$DISK_SRC" "$DISK_TMP"
        "$EMU" --model="$MODEL" --rom="$ROM_PATH" \
               --verify="$golden" "$DISK_TMP" 2>&1
        FAIL=$((FAIL + 1))
    fi
done

# ── Cleanup ────────────────────────────────────────────
rm -f "$DISK_TMP"

# ── Summary ────────────────────────────────────────────
echo ""
echo "────────────────────────"
echo "PASS: $PASS  FAIL: $FAIL  SKIP: $SKIP"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
echo "ALL PASS"
