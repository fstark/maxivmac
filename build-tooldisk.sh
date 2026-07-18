#!/bin/bash
# Build the Tool Disk — assembles data/system/tools.hfs from
# the INIT and pre-built utilities.
#
# Prerequisites:
#   - hfsutils installed (brew install hfsutils)
#   - build-init.sh has been run (INIT at macsrc/init/maxivmac INIT)
#
# Output:
#   data/system/tools.hfs

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TOOLDISK_DIR="$SCRIPT_DIR/macsrc/tooldisk"
INIT_DIR="$SCRIPT_DIR/macsrc/init"
OUTPUT_DIR="$SCRIPT_DIR/data/system"
OUTPUT="$OUTPUT_DIR/tools.hfs"

# Get version from git (same logic as build-init.sh)
VERSION=$(git describe --tags --match "v*" --always 2>/dev/null || echo "dev-unknown")
if [[ ! "$VERSION" =~ ^v ]]; then
    VERSION="dev-$VERSION"
fi

# Verify prerequisites
if ! command -v hformat &>/dev/null; then
    echo "Error: hfsutils not installed. Run: brew install hfsutils" >&2
    exit 1
fi

if [[ ! -f "$INIT_DIR/maxivmac INIT" ]]; then
    echo "Error: INIT not found at $INIT_DIR/maxivmac INIT" >&2
    echo "       Run build-init.sh first." >&2
    exit 1
fi

if [[ ! -f "$TOOLDISK_DIR/ImportFl" ]]; then
    echo "Error: ImportFl not found at $TOOLDISK_DIR/ImportFl" >&2
    exit 1
fi

if [[ ! -f "$TOOLDISK_DIR/ExportFl" ]]; then
    echo "Error: ExportFl not found at $TOOLDISK_DIR/ExportFl" >&2
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Remove old tool disk if present
rm -f "$OUTPUT"

# Create 800K blank image and format as HFS
dd if=/dev/zero of="$OUTPUT" bs=1024 count=800 2>/dev/null
hformat -l "maxivmac Tools $VERSION" "$OUTPUT"

# Convert README: LF -> CR, substitute version
TMPREADME=$(mktemp)
trap 'rm -f "$TMPREADME" "$INIT_DIR/maxivmac INIT.bin" "$TOOLDISK_DIR/ImportFl.bin" "$TOOLDISK_DIR/ExportFl.bin"' EXIT
sed "s/%%VERSION%%/$VERSION/" "$TOOLDISK_DIR/README" | tr '\n' '\r' > "$TMPREADME"

# Convert AppleDouble files to MacBinary for hcopy -m
AD2BIN="${SCRIPT_DIR}/bld/macos/ad2bin"
if [[ ! -x "$AD2BIN" ]]; then
    echo "Error: ad2bin not found at $AD2BIN. Build it first." >&2
    exit 1
fi

echo "Converting to MacBinary..."
"$AD2BIN" "$INIT_DIR/maxivmac INIT"
"$AD2BIN" "$TOOLDISK_DIR/ImportFl"
"$AD2BIN" "$TOOLDISK_DIR/ExportFl"

# Mount and copy artifacts
hmount "$OUTPUT"
hcopy -r "$TMPREADME" ":README"
hattrib -t TEXT -c ttxt ":README"
hcopy -m "$INIT_DIR/maxivmac INIT.bin" ":maxivmac INIT"
hcopy -m "$TOOLDISK_DIR/ImportFl.bin" ":ImportFl"
hcopy -m "$TOOLDISK_DIR/ExportFl.bin" ":ExportFl"
humount

# Boot emulator to let Finder create the Desktop file
MAXIVMAC="${SCRIPT_DIR}/bld/macos/maxivmac"
MAC_FILE="$TOOLDISK_DIR/build.mac"
DBG_SCRIPT="$TOOLDISK_DIR/build-tooldisk.dbg"

if [[ -x "$MAXIVMAC" ]]; then
    echo "Booting emulator to create Desktop file..."
#    "$MAXIVMAC" --headless --dbg-script="$DBG_SCRIPT" "$MAC_FILE" || true
    "$MAXIVMAC" --dbg-script="$DBG_SCRIPT" "$MAC_FILE" || true
else
    echo "Warning: maxivmac not found at $MAXIVMAC — skipping Desktop file creation."
    echo "         The Finder will show a warning on first mount."
fi

# Make read-only
chmod 444 "$OUTPUT"

echo "Tool Disk built: $OUTPUT"
echo "  Volume: maxivmac Tools $VERSION"
echo "  Contents: README, maxivmac INIT, ImportFl, ExportFl"
