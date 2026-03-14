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
