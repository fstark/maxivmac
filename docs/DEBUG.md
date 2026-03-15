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

Both scripts set the speed to 16x (`-speed 4`). The emulator is deterministic
at any speed setting thanks to tick-based RTC (60 ticks = 1 emulated second).

---

# Crash Investigation: Illegal Instruction at $000000FF (Mac Plus)

## Symptom

Running:
```bash
./bld/macos-cocoa/minivmac.app/Contents/MacOS/minivmac \
  --model plus --rom=extras/roms/vMac.ROM \
  /Users/fred/Development/macflim/MacFlim\ Source\ Code.dsk
```
Crashes into MacsBug with **Illegal Instruction at PC = $000000FF**.

The disk `extras/disks/608.hfs` boots without crashing. The crash is
specific to the MacFlim Source Code disk (14,336,000 bytes, HFS,
drSigWord=0x4244).

---

## Crash Mechanism

The crash follows this exact sequence:

1. **ROM low-level IWM code** at `0x40A732` runs a bit-banging loop that
   reads the IWM hardware register (floppy controller). The IWM read loop
   is at `0x400256`–`0x400260`.

2. **IWM emulation returns all zeros.** `IWMDevice::access()` in
   `src/devices/iwm.cpp` only emulates register/line state. `IWM.DataIn`
   is initialized to 0 and never updated — there is no data path from
   the disk image to the IWM data register. When the ROM code reads from
   the IWM with Q6=0, Q7=0, it always gets 0.

3. **ROM disk function detects an error** (reading zeros instead of valid
   GCR data) and takes the error exit at `0x40AA74`.

4. **Error path unwinds through ROM** to `0x409EDE: JMP (A0)` where
   A0 = `0xE00250F6` (a RAM address loaded from the MacFlim disk).

5. **RTS at `0xE00250F6`** pops `0x0000000F` as the return address.
   The value `0x0F` is NOT stack corruption — it is a legitimate
   parameter already present on the stack even during the first
   (successful) call through this code path. The problem is the error
   path does not clean up the stack properly before returning.

6. **CPU executes vector table memory** (addresses `0x0F`–`0xFF`) as
   instructions. At `0xFF`, opcode `0x3CFF` is illegal → MacsBug
   catches it.

### Full Call Chain Before Crash

```
0x40A074 → 0x40A732 (IWM bit-bang function)
         → error exit 0x40AA74
         → unwind: 0x40A078 → 0x40A07C → 0x40A07E → 0x40A080
         → 0x409ECA → 0x409ED4 → 0x409ED8 → 0x409EDA → 0x409EDC
         → 0x409EDE: JMP (A0) → 0xE00250F6
         → RTS pops 0x0F → crash through vector table → illegal at 0xFF
```

## Key Evidence

| Fact | Detail |
|------|--------|
| RTS crash site | PC = `0xE00250F6` (= `0x0250F6` in 24-bit), pops `0x0F` from A7=`0x003BC6E0` |
| Last ROM instruction | `0x409EDE: JMP (A0)` where A0 = `0xE00250F6` |
| IWM data reads | Only **3 total** IWM data reads occur (all before disk insert), not during normal I/O |
| Sony extension calls | **2445+** calls, ALL extension=2 (kExtnSony), command=1 (Prime) — Sony driver handles all disk I/O correctly |
| Sony patch | Correctly installed at ROM offset `0x17D30` (468 bytes, verified) |
| DiskInsertedPseudoException | Fires once at boot with valid MountCallBack=`0x417DD8` |
| ExtnBlockBase | `0x00F0C000` (correct for Mac Plus) |
| Drive_Transfer | Buffer at `0x003E290E`, count=`0x17800`, A7=`0x001FFB40` — no overlap |
| Stack value 0x0F | Present at `[003BC6E0]` during BOTH the first successful call and the crashing call |

## What Was Ruled Out

| Hypothesis | Result |
|------------|--------|
| IWM SENSE bit (added `\| 0x80` to status register) | Did NOT fix the crash — same exact PC trail |
| ROM patch at `0xA732` (IWM bit-bang function → immediate return) | Did NOT fix the crash — same PC trail, just shorter |
| ROM patch at `0x9E7E` (disk dispatcher → immediate return with D0=0) | Did NOT fix the crash |
| Buffer-stack overlap in Drive_Transfer | No overlap detected — buffer is in high RAM, stack is low |
| ExceptionTo dispatching to low address | Never triggered — PC does not reach 0xFF through the exception mechanism |
| IWM as primary I/O path | Only 3 IWM reads total; all normal disk I/O goes through the Sony extension |

## Open Questions

1. **Why does the ROM's low-level IWM code execute at all?** The Sony
   replacement driver handles all normal disk I/O. Something on the
   MacFlim disk triggers a code path that bypasses the .Sony driver
   and calls ROM disk controller primitives directly. This does NOT
   happen with `608.hfs`.

2. **The code at `0xE00250xx`** is loaded from the MacFlim disk into
   RAM. It calls ROM disk primitives through a calling convention that
   pushes parameters (including the value `0x0F`) onto the stack. The
   error return path doesn't pop these parameters, so RTS picks up
   `0x0F` as a return address.

3. **Why does the original minivmac work?** It has the exact same stub
   IWM emulation (IWM.DataIn always 0) and the same Sony replacement
   driver. Either: (a) the original minivmac never reaches this code
   path due to a timing or behavioral difference, or (b) it also
   crashes with this specific disk (untested).

---

# Root Cause Analysis: Configuration Differences Found

## BUG 1 (HIGH): Uninitialized `result` in `Sony_Control(kDriveInfo)` for Mac Plus

**File:** `src/devices/sony.cpp`, line ~1429

In the reference, the `case kDriveInfo:` is inside `#if CurEmMd >= kEmMd_SE`,
so on a Plus it doesn't exist — the `default:` case returns `mnvm_controlErr`.

In the current version, the case always exists but the body is guarded by
`if (g_machine->config().isSEOrLater())`. On Plus, this condition is false,
so `result` is never assigned and an uninitialized value is returned.

**Impact:** Large (non-floppy) disk images like the 14MB MacFlim disk get
`kQType = 1`, which triggers a `kDriveInfo` control call. On Plus, the
current version returns garbage instead of `-17 (controlErr)`. This may
confuse the driver state machine and is a likely contributor to the crash.

**Fix:** Added `else { result = mnvm_controlErr; }` to match the reference
behavior where pre-SE models fall through to `default:` returning controlErr.

## ~~BUG 2~~ (RETRACTED): `Sony_SupportTags` / `Sony_WantChecksumsUpdated`

Initially appeared to differ between versions. After verifying the *actual*
reference build output (which is regenerated by `build-reference.sh` via
`setup.sh`), the reference cfg/CNFUDPIC.h **does** have
`Sony_SupportTags 1` and `Sony_WantChecksumsUpdated 1` — matching the
current version. The checked-in `reference/cfg/CNFUDPIC.h` was stale. 
**No fix needed.**

---

# Peripheral I/O Logging Infrastructure

Added `MMDV_IO_LOG` to both versions for differential debugging:

- **Current:** `src/core/machine.cpp` — `#define MMDV_IO_LOG 0`
- **Reference:** `reference/src/GLOBGLUE.c` — `#define MMDV_IO_LOG 0`

When set to `1`, both produce identical format on stderr:
```
IOR VIA1 00EFE1FF 7E
IOW SCC  009FFFF9 09
IOR IWM  00DFE1FF 00
```

To use:
```bash
# Current version - add -DMMDV_IO_LOG=1 to CXXFLAGS or edit machine.cpp
# Reference version - edit GLOBGLUE.c

# Then capture:
./path/to/minivmac ... 2>/tmp/io_trace.log
# Compare:
diff <(grep '^IO' /tmp/trace_main.log) <(grep '^IO' /tmp/trace_ref.log)
```