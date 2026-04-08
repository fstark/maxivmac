#!/bin/sh
# Generate an HTML code-coverage report for the whole codebase.
#
# Prerequisites (macOS):
#   brew install ninja cmake    (build tools)
#   Xcode command-line tools    (clang with llvm-cov / llvm-profdata)
#
# Usage:
#   ./scripts/coverage.sh           # build, test, report
#   open coverage.html              # view results
#
# The script:
#  1. Builds the headless backend with -fprofile-instr-generate -fcoverage-mapping
#  2. Runs the non-regression test suite (test/verify.sh)
#  3. Merges raw profiles with llvm-profdata
#  4. Generates an HTML report with llvm-cov → coverage/ directory
#  5. Creates coverage.html as a symlink to coverage/index.html

set -e

DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$DIR"

BLD="bld/macos-coverage"
EMU="$BLD/maxivmac"
PROFRAW_DIR="$BLD/profraw"
PROFDATA="$BLD/coverage.profdata"
COV_DIR="coverage"
COV_HTML="coverage.html"

# Resolve LLVM tools (Xcode ships them inside the toolchain)
LLVM_PROFDATA="${LLVM_PROFDATA:-$(xcrun -f llvm-profdata 2>/dev/null || echo llvm-profdata)}"
LLVM_COV="${LLVM_COV:-$(xcrun -f llvm-cov 2>/dev/null || echo llvm-cov)}"

# ── 1. Build with coverage ────────────────────────────
echo "=== Building with coverage instrumentation ==="
cmake --preset macos-coverage
cmake --build --preset macos-coverage

# ── 2. Run tests ──────────────────────────────────────
echo ""
echo "=== Running non-regression tests ==="
rm -rf "$PROFRAW_DIR"
mkdir -p "$PROFRAW_DIR"

# Each test run produces a separate .profraw keyed by PID
export LLVM_PROFILE_FILE="$DIR/$PROFRAW_DIR/run_%p.profraw"
EMU="$DIR/$EMU" "$DIR/test/verify.sh" || true  # continue even if a model fails

# ── 3. Merge raw profiles ─────────────────────────────
echo ""
echo "=== Merging profiles ==="
PROFRAW_FILES=$(find "$PROFRAW_DIR" -name '*.profraw' 2>/dev/null)
if [ -z "$PROFRAW_FILES" ]; then
    echo "ERROR: no .profraw files found – tests may not have run."
    exit 1
fi
"$LLVM_PROFDATA" merge -sparse $PROFRAW_FILES -o "$PROFDATA"
echo "  merged → $PROFDATA"

# ── 4. Generate HTML report ───────────────────────────
echo ""
echo "=== Generating HTML report ==="
rm -rf "$COV_DIR"

# Only include project sources (not system headers or libs/)
"$LLVM_COV" show "$DIR/$EMU" \
    -instr-profile="$PROFDATA" \
    -format=html \
    -output-dir="$COV_DIR" \
    -show-line-counts-or-regions \
    -show-instantiations=false \
    -ignore-filename-regex='libs/|/usr/|/opt/' \
    -Xdemangler=c++filt

# ── 5. Symlink convenience entry point ────────────────
rm -f "$COV_HTML"
ln -s "$COV_DIR/index.html" "$COV_HTML"

echo ""
echo "=== Coverage report ready ==="
echo "  open $COV_HTML"

# Print a quick summary to stdout
"$LLVM_COV" report "$DIR/$EMU" \
    -instr-profile="$PROFDATA" \
    -ignore-filename-regex='libs/|/usr/|/opt/'
