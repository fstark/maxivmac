NOTE: never use "!" in Terminal commands, as it triggers a weird and confusing "dquote" mecanism.

Codebase

In reference/ there is an old version of the codebase, that is very tricky to build, but works correctly. It generates a different binary for every type of option.
At the root, there is what we call the "debug" version, which is a modernized CMake version, that generates a single binary that can run all the versions. We want to limit the use of #define in this build.

# Goal

Have the new maxivmac emulator (top-level source code) run identically to the original minivmac emulator (in reference/)

Current status:

At the root level:

A "build-debug.sh" script builds the new emulator
A "run-debug.sh" script runs the new emulator on a clean disk

A "build-reference.sh" script builds the reference emulator for emulating a Plus
A "run-reference.sh" script runs the old emulator on a clean disk

The run-debug.sh crashes with the message:

```
Abnormal Situation

The emulated computer is attemting an operation that wasn't
expected to happen in normal use

0803
```

# HOW TO DEBUG

## Deterministic Instruction Logging

Both builds support `--log-start=N --log-count=M` CLI arguments that log
instructions and IO operations to stderr for the instruction range [N, N+M).
The emulator calls `exit(0)` when it reaches instruction N+M, enabling
unattended comparison runs.

Log format (stderr):
```
12345 004002B4: 56C8          # instruction: {insn#} {PC}: {opcode}
12345 IOR VIA1 00EFFBFE 82    # IO read:     {insn#} IOR {device} {addr} {value}
12345 IOW VIA1 00EFFBFE 02    # IO write:    {insn#} IOW {device} {addr} {value}
DISK_INSERT drive=0 locked=0  # disk insertion event
```

## Comparison Scripts

### selftest.sh — Self-consistency (single build)

Tests whether a single build produces identical output across multiple runs:
```bash
./selftest.sh ref 0 300000 5      # reference build, first 300K insns, 5 runs
./selftest.sh debug 0 300000 5    # debug build, first 300K insns, 5 runs
./selftest.sh debug 50000 10000 3 # debug build, insns 50000-60000, 3 runs
```
Reports `ALL IDENTICAL` or `NON-DETERMINISM DETECTED` with the first diff.

### compare.sh — Cross-build comparison (ref vs debug)

Runs reference and debug builds with the same parameters and diffs the output:
```bash
./compare.sh 0 10000     # compare first 10K instructions
./compare.sh 0 300000    # compare first 300K instructions
./compare.sh 50000 5000  # compare instructions 50000-55000
```
Reports `IDENTICAL` or shows the diff. Output files are saved in `tmp/`.

### Workflow

1. **Verify determinism first** — run `selftest.sh` for both builds. If a build
   is not self-consistent, fix that before cross-comparing.
2. **Compare builds** — run `compare.sh` with increasing ranges to find the
   first divergence point.
3. **Narrow down** — use binary search on the start/count parameters to isolate
   the exact instruction where behavior diverges.

### Build Scripts

```bash
./build-debug.sh       # builds debug (cmake) version
./build-reference.sh   # builds reference (setup tool) version
```

Both scripts set the speed to 16x (`-speed 4`). The emulator must be deterministic
at any speed setting thanks to tick-based RTC (60 ticks = 1 emulated second).
