#!/bin/bash
# test.sh — Compare unsit output against unar reference
# Run from macsrc/unsit/test/
set -e

UNSIT=../unsit
TESTDIR=$(dirname "$0")
PASS=0
FAIL=0

cd "$TESTDIR"

for sit in *.sit; do
    [ -f "$sit" ] || continue
    name="${sit%.sit}"
    rm -rf "/tmp/unsit_$name" "/tmp/unar_$name"
    mkdir -p "/tmp/unsit_$name" "/tmp/unar_$name"

    # Extract with unsit
	echo "[${name}]"
    (cd "/tmp/unsit_$name" && "$OLDPWD/$UNSIT" "$OLDPWD/$sit") > "/tmp/unsit_${name}_out.txt" 2>&1

    # Extract with unar (reference)
    unar -f -o "/tmp/unar_$name" "$sit" >/dev/null 2>&1 || true

    # Compare data forks only (unar doesn't write .rsrc/.finderinfo in same format)
    # Find the unar output root (it may nest under archive name)
    UNAR_ROOT="/tmp/unar_$name"
    if [ -d "$UNAR_ROOT/$name" ]; then
        UNAR_ROOT="$UNAR_ROOT/$name"
    fi

    # Compare: check that all files in unar output exist in unsit output
    # and match byte-for-byte (excluding .rsrc files which are AppleDouble)
    DIFFS=0
    UNSIT_ROOT="/tmp/unsit_$name/$name"
    if [ ! -d "$UNSIT_ROOT" ]; then
        # Try without nesting
        UNSIT_ROOT="/tmp/unsit_$name"
    fi

    while IFS= read -r -d '' unar_file; do
        rel="${unar_file#$UNAR_ROOT/}"
        # Skip AppleDouble rsrc files
        case "$rel" in *.rsrc) continue ;; esac
        
        unsit_file="$UNSIT_ROOT/$rel"
        if [ ! -f "$unsit_file" ]; then
            # Check if it's a method we don't support (MW=8)
            if grep -q "unsupported compression method" /tmp/unsit_${name}_out.txt 2>/dev/null; then
                continue  # Expected skip
            fi
            echo "  MISSING: $rel"
            DIFFS=$((DIFFS + 1))
        elif ! cmp -s "$unar_file" "$unsit_file"; then
            echo "  DIFFER: $rel"
            DIFFS=$((DIFFS + 1))
        fi
    done < <(find "$UNAR_ROOT" -type f -print0 2>/dev/null)

    if [ "$DIFFS" -eq 0 ]; then
        echo "PASS: $name"
        PASS=$((PASS + 1))
		#rm "$sit"
    else
        echo "FAIL: $name ($DIFFS differences)"
        FAIL=$((FAIL + 1))
    fi
done

echo "---"
echo "$PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
