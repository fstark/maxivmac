NOTE: never use "!" in Terminal commands, as it triggers a weird and confusing "dquote" mecanism.

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

The reference implementation prints all execution. A file run.txt with the first 100000 instruction has already been created.

The goal of the debug is to find the divergence point and investigate the reason.

# Findings:

## Current Issue: Polling Loop Iteration Variance (0.012%)

**Test Setup:**
- Both main and reference versions configured for Mac Plus with `vMac.ROM` and `disk.hfs`
- Instruction trace logging: PC address + opcode for first 100,000 instructions
- Both versions use `clockMult = 1` and `WantCloserCyc = 0`

**Results:**

| Metric | Value |
|--------|-------|
| Total instructions logged | 100,000 each |
| Instructions matching | 99,988 (99.988%) |
| Differing lines | **12 (0.012%)** |
| Divergence pattern | Periodic ±1 iteration variance |

**Divergence Pattern:**

At 6 locations throughout the trace (lines ~16176, ~20206, ~28364, ~36522, ~44680, ~93629), the versions differ by exactly one iteration of a polling loop:

```
Code sequence (both versions):
0040031A: 70FF    (MOVEQ #-1, D0)
0040031C: 4A45    (TST.W D5)
0040031E: 660C    (BNE +12 bytes → 0x40032C)
0040032C: 082D    (BTST - test bit in memory)
00400332: 56C8    (DBNE D0, -10 bytes → back to 0x40032C)
```

At each divergence point, one version executes `0040032C: 082D / 00400332: 56C8` exactly **once more** than the other before the bit test succeeds and the loop exits. Sometimes the main version has the extra iteration, sometimes the reference version does.

**Analysis:**

The `BTST` instruction at `0x40032C` reads a hardware status bit (likely a VIA timer or interrupt flag). The `DBNE` at `0x400332` decrements D0 and branches back if not zero and the Z flag is clear. Both versions test the **same bit** and exit the loop when it becomes set, but they disagree by exactly one iteration on whether the bit was set **this iteration** or **next iteration**.

This indicates a sub-cycle timing difference in when the peripheral register state (VIA IFR or similar) is updated relative to the CPU's memory read. The bit transitions from 0→1 at nearly the same time as the BTST reads it, and tiny differences in update scheduling cause one version to see the old value (0) and loop once more, while the other sees the new value (1) and exits immediately.

**Likely Causes:**

1. **ICT (interrupt/timer task) scheduling granularity** — VIA timer updates may fire at slightly different sub-cycle offsets in the two implementations
2. **Cycle accounting rounding** — With `kCycleScale = 64`, cycle costs are represented as `cycles * 64`, but rounding/truncation may differ
3. **Interrupt check timing** — One version may check for timer expiry before the memory read, the other after
4. **Random seeding or uninitialized state** — Unlikely given the deterministic pattern, but possible if PRAM or RTC initial state differs

**Status:** ⚠️ **NON-DETERMINISTIC** — 12 instructions differ out of 100,000. Requires investigation of VIA timer update scheduling and cycle accounting to achieve perfect determinism.

**Files:**
- Main version log: `main_run_fixed.txt` (100,000 lines, 1.4MB)
- Reference version log: `reference/run.txt` (100,000 lines, 1.4MB)

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
