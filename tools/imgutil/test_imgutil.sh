#!/bin/bash
set -euo pipefail

IMGUTIL="${1:-./bld/linux/imgutil}"
TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT

echo "=== imgutil test suite ==="

# --- Helper: write big-endian values using printf + dd ---
# write_be16 <file> <offset> <value>
write_be16() {
    local hi=$(( ($3 >> 8) & 0xFF ))
    local lo=$(( $3 & 0xFF ))
    printf "\\x$(printf '%02x' $hi)\\x$(printf '%02x' $lo)" | \
        dd of="$1" bs=1 seek="$2" conv=notrunc 2>/dev/null
}
# write_be32 <file> <offset> <value>
write_be32() {
    local b0=$(( ($3 >> 24) & 0xFF ))
    local b1=$(( ($3 >> 16) & 0xFF ))
    local b2=$(( ($3 >> 8) & 0xFF ))
    local b3=$(( $3 & 0xFF ))
    printf "\\x$(printf '%02x' $b0)\\x$(printf '%02x' $b1)\\x$(printf '%02x' $b2)\\x$(printf '%02x' $b3)" | \
        dd of="$1" bs=1 seek="$2" conv=notrunc 2>/dev/null
}

# --- Create a minimal valid HFS partition (32 KB = 64 blocks) ---
MINI="$TMPDIR/mini.hfs"
dd if=/dev/zero of="$MINI" bs=512 count=64 2>/dev/null

# Boot block signature "LK" at offset 0
printf '\x4c\x4b' | dd of="$MINI" bs=1 conv=notrunc 2>/dev/null

# MDB at offset 1024:
#   +0: signature 0x4244
#   +2: crDate (4 bytes)
#   +6: mdDate (4 bytes)
#   +36: volume name Pascal string: len=4 "Test"
write_be16 "$MINI" 1024 0x4244
write_be32 "$MINI" 1026 0xA2230000
write_be32 "$MINI" 1030 0xA2230000
printf '\x04Test' | dd of="$MINI" bs=1 seek=1060 conv=notrunc 2>/dev/null

# --- Test 1: info on raw HFS ---
echo -n "Test 1: info on raw HFS partition... "
OUTPUT=$($IMGUTIL info "$MINI")
echo "$OUTPUT" | grep -q 'HFS partition' || { echo "FAIL"; exit 1; }
echo "$OUTPUT" | grep -q '"Test"' || { echo "FAIL (no volume name)"; exit 1; }
echo "OK"

# --- Test 2: mkdisk ---
echo -n "Test 2: mkdisk... "
DISK="$TMPDIR/test_disk.img"
$IMGUTIL mkdisk "$MINI" -o "$DISK"
test -f "$DISK" || { echo "FAIL (no output)"; exit 1; }
echo "OK"

# --- Test 3: info on APM disk ---
echo -n "Test 3: info on APM disk... "
OUTPUT=$($IMGUTIL info "$DISK")
echo "$OUTPUT" | grep -q 'APM disk' || { echo "FAIL"; exit 1; }
echo "$OUTPUT" | grep -q '"Test"' || { echo "FAIL (no volume name in partition)"; exit 1; }
echo "$OUTPUT" | grep -q 'Apple_HFS' || { echo "FAIL (no HFS entry)"; exit 1; }
echo "OK"

# --- Test 4: extract and round-trip ---
echo -n "Test 4: extract and round-trip... "
EXTRACTED="$TMPDIR/extracted.hfs"
# Partition layout from mkdisk: 1=pmap, 2=driver, 3=HFS, 4=free
$IMGUTIL extract "$DISK" 3 -o "$EXTRACTED"
diff "$MINI" "$EXTRACTED" || { echo "FAIL (not identical)"; exit 1; }
echo "OK"

# --- Test 5: multi-partition mkdisk ---
echo -n "Test 5: multi-partition mkdisk... "
MINI2="$TMPDIR/mini2.hfs"
dd if=/dev/zero of="$MINI2" bs=512 count=64 2>/dev/null
printf '\x4c\x4b' | dd of="$MINI2" bs=1 conv=notrunc 2>/dev/null
write_be16 "$MINI2" 1024 0x4244
write_be32 "$MINI2" 1026 0xA2230000
write_be32 "$MINI2" 1030 0xA2230000
printf '\x05Disk2' | dd of="$MINI2" bs=1 seek=1060 conv=notrunc 2>/dev/null

MULTI="$TMPDIR/multi_disk.img"
$IMGUTIL mkdisk "$MINI" "$MINI2" -o "$MULTI"
OUTPUT=$($IMGUTIL info "$MULTI")
echo "$OUTPUT" | grep -q '"Test"' || { echo "FAIL (no Test)"; exit 1; }
echo "$OUTPUT" | grep -q '"Disk2"' || { echo "FAIL (no Disk2)"; exit 1; }
echo "OK"

echo "=== All tests passed ==="
