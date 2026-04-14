#!/bin/bash
# Debugger smoke tests — feed commands via stdin and verify output.
set -e

MAXIVMAC=./bld/macos/maxivmac
ROM=""

# Find a ROM file and determine model
for r in roms/MacPlus.ROM roms/MacII.ROM roms/MacSE.ROM MacII.ROM; do
    if [ -f "$r" ]; then
        ROM="$r"
        # Extract model name from ROM filename
        MODEL=$(basename "$r" .ROM)
        break
    fi
done

if [ -z "$ROM" ]; then
    echo "SKIP: no ROM file found"
    exit 0
fi

PASS=0
FAIL=0

check() {
    local desc="$1"
    local input="$2"
    local pattern="$3"

    output=$(printf '%s\n' "$input" | $MAXIVMAC --debugger --headless --model="$MODEL" --rom "$ROM" 2>/dev/null || true)
    if echo "$output" | grep -q "$pattern"; then
        echo "  PASS: $desc"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $desc (expected '$pattern')"
        echo "  Output: $(echo "$output" | head -10)"
        FAIL=$((FAIL + 1))
    fi
}

echo "Debugger smoke tests (ROM: $ROM)"
echo "================================"

# Test 1: startup banner and prompt
check "startup banner" "quit" "(dbg)"

# Test 2: help command
check "help command" "help
quit" "Execution:"

# Test 3: info reg
check "info reg" "info reg
quit" "D0="

# Test 4: step and see instruction
check "step command" "step
quit" "step completed"

# Test 5: set breakpoint
check "set breakpoint" "break \$400000
info break
quit" "Breakpoint"

# Test 6: info traps
check "info traps" "info traps Get
quit" "TrapWord"

# Test 7: info globals
check "info globals" "info globals Cur
quit" "CurAp"

# Test 8: print expression
check "print hex" "print \$1234
quit" "00001234"

# Test 9: examine memory
check "examine memory" "x/4b \$400000
quit" "\$00400000:"

# Test 10: watch command
check "watch command" "watch \$0900
info break
quit" "watchpoint"

echo ""
echo "Results: $PASS passed, $FAIL failed"

if [ $FAIL -gt 0 ]; then
    exit 1
fi
echo "All debugger smoke tests passed."
