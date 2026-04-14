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

# --- Debug Server Tests ---

echo ""
echo "Debug server tests (ROM: $ROM)"
echo "=============================="

SERVER_PID=""
cleanup() {
    rm -f /tmp/maxivmac-dbg-*.sock
    [ -n "$SERVER_PID" ] && kill $SERVER_PID 2>/dev/null || true
}
trap cleanup EXIT

# Find a disk image for the server tests
DISK=""
for d in 608.hfs disk.hfs; do
    if [ -f "$d" ]; then
        DISK="$d"
        break
    fi
done

if [ -z "$DISK" ]; then
    echo "SKIP: no disk image found for server tests"
    exit 0
fi

# Start emulator in server mode
$MAXIVMAC --debugserver --headless --model="$MODEL" "$DISK" &
SERVER_PID=$!

# Wait for socket to appear
SOCK="/tmp/maxivmac-dbg-${SERVER_PID}.sock"
for i in $(seq 1 30); do
    [ -S "$SOCK" ] && break
    sleep 0.1
done

if [ ! -S "$SOCK" ]; then
    echo "  FAIL: debug server socket did not appear"
    exit 1
fi

SPASS=0
SFAIL=0

scheck() {
    local desc="$1"
    local cmd="$2"
    local pattern="$3"

    output=$($MAXIVMAC debug --socket="$SOCK" "$cmd" 2>/dev/null || true)
    if echo "$output" | grep -q "$pattern"; then
        echo "  PASS: $desc"
        SPASS=$((SPASS + 1))
    else
        echo "  FAIL: $desc (expected '$pattern')"
        echo "  Output: $(echo "$output" | head -10)"
        SFAIL=$((SFAIL + 1))
    fi
}

# Test: one-shot info reg
scheck "server info reg" "info reg" "D0="

# Test: step and verify PC changes
PC1=$($MAXIVMAC debug --socket="$SOCK" "print pc" 2>/dev/null | head -1)
$MAXIVMAC debug --socket="$SOCK" "step" >/dev/null 2>&1
PC2=$($MAXIVMAC debug --socket="$SOCK" "print pc" 2>/dev/null | head -1)
if [ "$PC1" != "$PC2" ]; then
    echo "  PASS: step advances PC"
    SPASS=$((SPASS + 1))
else
    echo "  FAIL: PC did not change after step ($PC1 -> $PC2)"
    SFAIL=$((SFAIL + 1))
fi

# Test: set and read breakpoint
scheck "server break command" "break \$400000" "Breakpoint"
scheck "server info break" "info break" "400000"

# Test: auto-discovery (no --socket)
output=$($MAXIVMAC debug "info reg" 2>/dev/null || true)
if echo "$output" | grep -q "D0="; then
    echo "  PASS: auto-discovery"
    SPASS=$((SPASS + 1))
else
    echo "  FAIL: auto-discovery (expected 'D0=')"
    echo "  Output: $(echo "$output" | head -10)"
    SFAIL=$((SFAIL + 1))
fi

# Test: quit (terminates server)
$MAXIVMAC debug --socket="$SOCK" "quit" >/dev/null 2>&1 || true
wait $SERVER_PID 2>/dev/null || true
SERVER_PID=""

# Verify socket cleaned up
if [ -S "$SOCK" ]; then
    echo "  WARN: socket not cleaned up"
    rm -f "$SOCK"
fi

echo ""
echo "Server results: $SPASS passed, $SFAIL failed"

TOTAL_PASS=$((PASS + SPASS))
TOTAL_FAIL=$((FAIL + SFAIL))
echo "Total results: $TOTAL_PASS passed, $TOTAL_FAIL failed"

if [ $TOTAL_FAIL -gt 0 ]; then
    exit 1
fi
echo "All debugger + server smoke tests passed."
