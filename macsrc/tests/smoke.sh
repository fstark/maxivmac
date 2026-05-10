#!/bin/bash
# Smoke test for the maxivmac INIT.
#
# Injects the INIT into test HFS images, creates BUILD.txt in the
# shared directory, then boots 4 Mac configurations headless and
# verifies each can read BUILD.txt with the correct content.
#
# Prerequisites:
#   - bld/macos/maxivmac and bld/macos/ad2bin built
#   - hfsutils installed (hmount, hcopy, humount)
#   - macsrc/tests/608.hfs and 701.hfs base images

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$ROOT_DIR"
MAXIVMAC="$ROOT_DIR/bld/macos/maxivmac"
AD2BIN="$ROOT_DIR/bld/macos/ad2bin"
INIT_DIR="$ROOT_DIR/macsrc/init"
TEST_DIR="$SCRIPT_DIR"
SHARED_DIR="$TEST_DIR/shared"
TMP_DIR="$TEST_DIR/tmp"

INIT_NAME="maxivmac INIT"
# Destination inside the HFS images (System 6 vs 7)
HFS_DEST_608=":System Folder:"
HFS_DEST_701=":System Folder:Extensions:"

# Shared drive name as seen by the guest
SHARED_DRIVE="shared"

CONFIGS=(
    plus-608
    plus-701
    macii-608
    macii-701
)

# Override with command-line argument if provided
if [ $# -gt 0 ]; then
    CONFIGS=("$@")
fi

# ── Sanity checks ──────────────────────────────────────

for tool in hmount hcopy humount; do
    command -v "$tool" >/dev/null 2>&1 || { echo "Error: $tool not found"; exit 1; }
done
[ -x "$MAXIVMAC" ] || { echo "Error: maxivmac not built ($MAXIVMAC)"; exit 1; }
[ -x "$AD2BIN" ]   || { echo "Error: ad2bin not built ($AD2BIN)"; exit 1; }

# ── Build ID ───────────────────────────────────────────

BUILD_ID=$(git -C "$ROOT_DIR" describe --tags --match "v*" --always 2>/dev/null || echo "unknown")
if [[ ! "$BUILD_ID" =~ ^v ]]; then
    BUILD_ID="dev-$BUILD_ID"
fi
echo "Build ID: $BUILD_ID"

# ── Prepare shared directory ───────────────────────────

mkdir -p "$SHARED_DIR" "$TMP_DIR"
echo "$BUILD_ID" > "$SHARED_DIR/BUILD.txt"
echo "Created $SHARED_DIR/BUILD.txt"

# ── Convert INIT to MacBinary and inject into HFS images ──

echo "Converting INIT to MacBinary..."
cp "$INIT_DIR/$INIT_NAME" "$TMP_DIR/$INIT_NAME"
cp "$INIT_DIR/._$INIT_NAME" "$TMP_DIR/._$INIT_NAME"
(cd "$TMP_DIR" && "$AD2BIN" "$INIT_NAME")

for img in 608 701; do
    echo "Injecting INIT into ${img}.hfs..."
    # Work on a copy so the base images stay clean
    cp "$TEST_DIR/${img}.hfs" "$TMP_DIR/${img}.hfs"
    hmount "$TMP_DIR/${img}.hfs"
    if [ "$img" = "608" ]; then
        HFS_DEST="$HFS_DEST_608$INIT_NAME"
    else
        HFS_DEST="$HFS_DEST_701$INIT_NAME"
    fi
    hcopy -m "$TMP_DIR/${INIT_NAME}.bin" "$HFS_DEST"
    # hcopy returns 0 even on failure; verify the file exists
    if ! hls "$HFS_DEST" >/dev/null 2>&1; then
        echo "Error: INIT not found at $HFS_DEST in ${img}.hfs"
        humount
        exit 1
    fi
    humount
done

# ── Run each configuration ─────────────────────────────

PASS=0
FAIL=0

for cfg in "${CONFIGS[@]}"; do
    MAC_FILE="$TEST_DIR/${cfg}.mac"
    echo ""
    echo "━━━ Testing: $cfg ━━━"

    # Generate per-config .dbg script from template
    DBG_SCRIPT="$TMP_DIR/${cfg}.dbg"
    sed -e "s|%%BUILD_ID%%|${BUILD_ID}|g" \
        -e "s|%%SHARED_DRIVE%%|${SHARED_DRIVE}|g" \
        "$TEST_DIR/${cfg}.dbg.in" > "$DBG_SCRIPT"

    # Point the .mac file at the tmp copy of the disk
    # (create a temp .mac that overrides the disk path)
    TMP_MAC="$TMP_DIR/${cfg}.mac"
    if [[ "$cfg" == *608* ]]; then
        DISK_IMG="$TMP_DIR/608.hfs"
    else
        DISK_IMG="$TMP_DIR/701.hfs"
    fi
    sed "s|^disk = .*|disk = ${DISK_IMG}|;s|^shared = .*|shared = ${SHARED_DIR}|" "$MAC_FILE" > "$TMP_MAC"

    if "$MAXIVMAC"  --diag=INIT,ExtFS --silent \
        --dbg-script="$DBG_SCRIPT" "$TMP_MAC" 2>"$TMP_DIR/${cfg}.log"; then
        echo "  ✓ PASS"
        PASS=$((PASS + 1))
    else
        echo "  ✗ FAIL (exit code $?)"
        echo "  Log: $TMP_DIR/${cfg}.log"
        tail -5 "$TMP_DIR/${cfg}.log" | sed 's/^/    /'
        FAIL=$((FAIL + 1))
    fi
done

# ── Summary ────────────────────────────────────────────

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Results: $PASS passed, $FAIL failed (of ${#CONFIGS[@]})"

if [ "$FAIL" -gt 0 ]; then
    echo "SMOKE TEST FAILED"
    exit 1
fi

echo "ALL SMOKE TESTS PASSED"
