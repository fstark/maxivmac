#!/bin/sh
# Record golden files for all listed models.
# Re-run this whenever you intentionally change emulator behaviour.
# Model name = ROM base name (e.g. MacPlus → MacPlus.ROM).
#
# Usage:  ./test/record.sh                    # record all listed models
#         ./test/record.sh MacPlus             # record only MacPlus
#         ./test/record.sh MacPlus MacII MacIIx  # record specific models

set -e

DIR="$(cd "$(dirname "$0")/.." && pwd)"
EMU="$DIR/bld/macos-cocoa/maxivmac.app/Contents/MacOS/maxivmac"
ROMS="$DIR/roms"
DISK_SRC="$DIR/extras/disks/608.hfs"
TESTDIR="$DIR/test"
DISK_TMP="$TESTDIR/.608_record.hfs"

# ── Models to record ──────────────────────────────────
# Add new models here as they become bootable.
ALL_MODELS="MacPlus MacSE MacII MacIIx Classic Mac512Ke"
# PB100 Mac128K

if [ $# -gt 0 ]; then
    MODELS="$*"
else
    MODELS="$ALL_MODELS"
fi

# ── Sanity checks ──────────────────────────────────────
if [ ! -x "$EMU" ]; then
    echo "ERROR: emulator not found at $EMU"
    echo "       Run: cmake --build --preset macos-cocoa"
    exit 1
fi
if [ ! -f "$DISK_SRC" ]; then
    echo "ERROR: reference disk not found at $DISK_SRC"
    exit 1
fi

# ── Record ─────────────────────────────────────────────
for MODEL in $MODELS; do
    ROM_PATH="$ROMS/$MODEL.ROM"
    if [ ! -f "$ROM_PATH" ]; then
        echo "ERROR: ROM not found: $ROM_PATH"
        exit 1
    fi

    GOLDEN="$TESTDIR/$MODEL.golden"

    # Fresh disk copy (emulator writes to disk)
    cp "$DISK_SRC" "$DISK_TMP"

    printf "Recording %-14s → %s ... " "$MODEL" "$GOLDEN"
    "$EMU" --model="$MODEL" --rom="$ROM_PATH" \
           --record="$GOLDEN" "$DISK_TMP" 2>&1 \
    | grep -E "^StateRecorder: recorded" || true
done

# ── Cleanup ────────────────────────────────────────────
rm -f "$DISK_TMP"

echo ""
echo "Done. Verify with: ./test/verify.sh"
