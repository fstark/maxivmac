#!/bin/bash
set -euo pipefail

TOOL="./bld/macos/icon-extract"
FIXTURE="test/fixtures/icon_128k_sidecar.bin"
OUTDIR=$(mktemp -d)

trap "rm -rf $OUTDIR" EXIT

"$TOOL" -v -o "$OUTDIR" "$FIXTURE"

# At least one PNG was produced
COUNT=$(find "$OUTDIR" -name '*.png' | wc -l | tr -d ' ')
if [ "$COUNT" -lt 1 ]; then
    echo "FAIL: no PNGs produced"
    exit 1
fi

# Each PNG starts with the PNG magic bytes
for f in "$OUTDIR"/*.png; do
    MAGIC=$(xxd -l 4 -p "$f")
    if [ "$MAGIC" != "89504e47" ]; then
        echo "FAIL: $f is not a valid PNG"
        exit 1
    fi

    # Verify iTXt chunk with "Title" keyword is present
    if ! grep -q "Title" "$f"; then
        echo "FAIL: $f missing Title iTXt chunk"
        exit 1
    fi

    # Verify iTXt chunk with "Source" keyword is present
    if ! grep -q "Source" "$f"; then
        echo "FAIL: $f missing Source iTXt chunk"
        exit 1
    fi
done

echo "PASS: extracted $COUNT icon(s) with metadata"
