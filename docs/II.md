# Mac II Hardware Emulation Architecture

This document describes the Mac II emulation as implemented in this codebase.
It covers every device and subsystem relevant to the Mac II model, the wire bus
interconnects, the main-loop tick sequence, and the interrupt priority chain.

---

## 1. Machine Configuration (MacModel::II)

Source: `src/core/model_defs.h` — `kModelDefs` (II entry); `src/core/machine_config.cpp` — `MachineConfigForModel(MacModel::II)` reads from that table.

| Field             | Value                       | Notes                                               |
|-------------------|-----------------------------|-----------------------------------------------------|
| `use68020`        | `true`                      | 68020 CPU                                           |
| `emFPU`           | `true`                      | 68881 FPU present                                   |
| `emMMU`           | `false`                     | No 68851 PMMU (32-bit mode via software/VIA2)       |
| `ramASize`        | `0x00400000` (4 MB)         | Bank A                                              |
| `ramBSize`        | `0x00400000` (4 MB)         | Bank B (total 8 MB)                                 |
| `romSize`         | `0x00040000` (256 KB)       | Mac II ROM                                          |
| `romBase`         | `0x00800000`                | ROM at 8 MB (24-bit address)                        |
| `romFileName`     | `"MacII.ROM"`               | 256 KB ROM image                                    |
| `romMD5`          | `2a8a4c7f2a38e0ab0771f59a9a0f1ee4` | Expected ROM checksum                        |
| `extnBlockBase`   | `0x50F0C000`                | 32-bit extension block (NuBus I/O space)            |
| `emVIA1`          | `true`                      | VIA1 present (6522)                                 |
| `emVIA2`          | `true`                      | VIA2 present (6522) — Mac II only                   |
| `emADB`           | `true`                      | Apple Desktop Bus (keyboard + mouse)                |
| `emClassicKbrd`   | `false`                     | No classic shift-register keyboard                  |
| `emRTC`           | `true`                      | Real-time clock present                             |
| `emPMU`           | `false`                     | No PMU                                              |
| `emASC`           | `true`                      | Apple Sound Chip present                            |
| `emClassicSnd`    | `false`                     | No classic PWM-in-RAM sound                         |
| `emVidCard`       | `true`                      | NuBus video card (slot 9)                           |
| `includeVidMem`   | `true`                      | Dedicated VRAM (not in main RAM)                    |
| `vidMemSize`      | `0x00080000` (512 KB)       | VRAM — supports up to 8bpp at 640×480               |
| `vidROMSize`      | `0x2000` (8 KB)             | NuBus declaration ROM                               |
| `screenWidth`     | `640`                       | Default screen width                                |
| `screenHeight`    | `480`                       | Default screen height                               |
| `screenDepth`     | `3`                         | log₂ bpp: 3 = 8bpp (256 colours)                   |
| `clockMult`       | `2`                         | 16 MHz (2× the base 8 MHz timing constant)          |
| `maxATTListN`     | `20`                        | Larger ATT table for 32-bit space                   |
| `via1Config`      | `MakeVIA1Config_MacII()`    | See §2 below                                        |
| `via2Config`      | `MakeVIA2Config_MacII()`    | See §3 below                                        |

Helper predicates: `isCompactMac()` → `false`, `isIIFamily()` → `true`,
`isSEOrLater()` → `true`.

---

## 2. VIA1 Configuration (Mac II)

Source: `src/core/machine_config.cpp` — `MakeVIA1Config_MacII()`

On the Mac II, VIA1 carries the ADB bus, RTC, and SCC wait signals.
The classic keyboard and sound volume lines are absent.

### Port Masks

| Field              | Value  | Meaning                                                        |
|--------------------|--------|----------------------------------------------------------------|
| `oraFloatVal`      | `0xBF` | Port A floats high except bit 6 (pulled low)                   |
| `orbFloatVal`      | `0xFF` | Port B floats high                                             |
| `oraCanIn`         | `0x80` | Bit 7 readable (SCC wait/request)                              |
| `oraCanOut`        | `0x3F` | Bits 0–5 writable                                              |
| `orbCanIn`         | `0x09` | Bits 0 and 3 readable (RTC data, ADB_Int)                      |
| `orbCanOut`        | `0xB7` | Bits 0–2, 4–5, 7 writable (RTC serial, ADB_st0/st1, sound)    |
| `ierNever0`        | `0x00` | No IER bits forced to 0                                        |
| `ierNever1`        | `0x58` | IER bits 3, 4, 6 forced to 1                                   |
| `cb2ModesAllowed`  | `0x01` | Only mode 0                                                    |
| `ca2ModesAllowed`  | `0x01` | Only mode 0                                                    |

### Port A Wire Mapping

| Bit | Wire ID           | Signal                                |
|-----|-------------------|---------------------------------------|
| 0   | `Wire_VIA1_iA0`   | Unknown (overlay-related)             |
| 1   | `Wire_VIA1_iA1`   | Unknown                               |
| 2   | `Wire_VIA1_iA2`   | Unknown                               |
| 3   | `Wire_VIA1_iA3`   | SCC vSync                             |
| 4   | `Wire_VIA1_iA4`   | **MemOverlay**                        |
| 5   | `Wire_VIA1_iA5`   | IWM vSel                              |
| 6   | `Wire_VIA1_iA6`   | Unknown                               |
| 7   | `Wire_VIA1_iA7`   | SCC wait/request                      |

### Port B Wire Mapping

| Bit | Wire ID                      | Signal                             |
|-----|------------------------------|------------------------------------|
| 0   | `Wire_VIA1_iB0`              | **RTC data line**                  |
| 1   | `Wire_VIA1_iB1`              | **RTC clock**                      |
| 2   | `Wire_VIA1_iB2`              | **RTC chip-enable (active low)**   |
| 3   | `Wire_VIA1_iB3` (`ADB_Int`)  | **ADB interrupt** (0 = asserted)   |
| 4   | `Wire_VIA1_iB4` (`ADB_st0`)  | **ADB state bit 0**                |
| 5   | `Wire_VIA1_iB5` (`ADB_st1`)  | **ADB state bit 1**                |
| 6   | `Wire_VIA1_iB6`              | Unknown                            |
| 7   | `Wire_VIA1_iB7`              | Sound compat / unused              |

### CB2 and Interrupt

| Wire                          | Purpose                            |
|-------------------------------|------------------------------------|
| `Wire_VIA1_iCB2` (`ADB_Data`) | ADB bidirectional data line        |
| `Wire_VIA1_InterruptRequest`  | VIA1 → CPU IPL (level 1)           |

---

## 3. VIA2 Configuration (Mac II)

Source: `src/core/machine_config.cpp` — `MakeVIA2Config_MacII()`

VIA2 is unique to the Mac II family. It carries the VBL interrupt from
the video card, the 32-bit addressing mode flag, NuBus slot interrupts,
and the power-off signal.

### Port Masks

| Field              | Value  | Meaning                                                        |
|--------------------|--------|----------------------------------------------------------------|
| `oraFloatVal`      | `0xFF` | Port A floats high                                             |
| `orbFloatVal`      | `0xFF` | Port B floats high                                             |
| `oraCanIn`         | `0x01` | Bit 0 readable (VBLinterrupt from video card)                  |
| `oraCanOut`        | `0xC0` | Bits 6–7 writable                                              |
| `orbCanIn`         | `0x00` | No port B bits readable via VIA register                       |
| `orbCanOut`        | `0x8C` | Bits 2, 3, 7 writable (PowerOff, ADDR32, unknown)             |
| `ierNever0`        | `0x00` | No IER bits forced to 0                                        |
| `ierNever1`        | `0xED` | IER bits 0, 2, 3, 5, 6, 7 forced to 1                         |
| `cb2ModesAllowed`  | `0x01` | Only mode 0                                                    |
| `ca2ModesAllowed`  | `0x01` | Only mode 0                                                    |

### Port A Wire Mapping

| Bit | Wire ID             | Signal                                   |
|-----|---------------------|------------------------------------------|
| 0   | `Wire_VBLinterrupt` | **VBL interrupt** from NuBus video card  |
| 1   | `Wire_VIA2_iA1`     | NuBus slot interrupt (slot $E)           |
| 2   | `Wire_VIA2_iA2`     | NuBus slot interrupt (slot $D)           |
| 3   | `Wire_VIA2_iA3`     | NuBus slot interrupt (slot $C)           |
| 4   | `Wire_VIA2_iA4`     | NuBus slot interrupt (slot $B)           |
| 5   | `Wire_VIA2_iA5`     | NuBus slot interrupt (slot $A)           |
| 6   | `Wire_VIA2_iA6`     | Unknown (ADDR32-related)                 |
| 7   | `Wire_VIA2_iA7`     | Unknown (ADDR32-related)                 |

### Port B Wire Mapping

| Bit | Wire ID                        | Signal                         |
|-----|--------------------------------|--------------------------------|
| 0   | `Wire_VIA2_iB0`                | Unknown                        |
| 1   | `Wire_VIA2_iB1`                | Unknown                        |
| 2   | `Wire_VIA2_iB2` (`PowerOff`)   | **Power off** (active low)     |
| 3   | `Wire_VIA2_iB3` (`Addr32`)     | **32-bit addressing mode**     |
| 4   | `Wire_VIA2_iB4`                | Unknown                        |
| 5   | `Wire_VIA2_iB5`                | Unknown                        |
| 6   | `Wire_VIA2_iB6`                | Unknown                        |
| 7   | `Wire_VIA2_iB7`                | Unknown                        |

### CB1, CB2, CA1, and Interrupt

| Wire                          | Purpose                                |
|-------------------------------|----------------------------------------|
| `Wire_VIA2_iCB2`              | Unknown (CB2 line)                     |
| `Wire_VIA2_InterruptRequest`  | VIA2 → CPU IPL (level 2)               |
| VIA2 CA1                      | Pulsed by `VideoDevice::update()` — VBL |
| VIA2 CB1                      | Pulsed by ASC on FIFO status change    |

### 32-bit Addressing Mode

`Wire_VIA2_iB3` (`ADDR32`) controls whether the CPU uses 24-bit or 32-bit
address mode. When the ROM writes this bit, `Addr32_ChangeNtfy()` is
called → `SetUpMemBanks()` rebuilds the ATT list for the new mode. This
is equivalent to what the MMU does in hardware but implemented purely in
the emulator's address-translation table.

RAM bank-select bits are read from VIA2 port A bits 6–7 (`VIA2_iA6`,
`VIA2_iA7`) to determine the interleave boundary between the two
4 MB RAM banks.

---

## 4. ADB (Apple Desktop Bus)

Source: `src/devices/adb.cpp`, `src/devices/adb.h`

The Mac II uses ADB for both keyboard and mouse. The bus is mastered by
the ROM/ADB Manager; the emulator acts as a slave that responds to host
events.

### Wire Connections

| VIA1 Port B Bit | Wire ID                | Signal                             |
|-----------------|------------------------|------------------------------------|
| 3               | `Wire_VIA1_iB3_ADB_Int`  | ADB interrupt (0 = device ready)  |
| 4               | `Wire_VIA1_iB4_ADB_st0`  | ADB state bit 0                   |
| 5               | `Wire_VIA1_iB5_ADB_st1`  | ADB state bit 1                   |
| CB2             | `Wire_VIA1_iCB2_ADB_Data`| ADB bidirectional data             |

### ADB State Machine

The Mac ROM drives `ADB_st0` and `ADB_st1` to command the ADB transceiver.
Each state change triggers `ADBDevice::stateChangeNtfy()`, which schedules
`kICT_ADB_NewState` to fire after `348160 * clockMult / 64` cycles.

State encoding (`state = ADB_st1 × 2 + ADB_st0`):

| State | Meaning                                                   |
|-------|-----------------------------------------------------------|
| 0     | Idle                                                      |
| 1     | Send-data (data ready from Mac to device)                 |
| 2     | Talk (request data from ADB device)                       |
| 3     | Command byte being sent                                   |

When `doNewState()` fires, `ADBDevice` dispatches to the appropriate
emulated keyboard or mouse handler.

### Mouse Enable Gate

`Mouse_Enabled()` returns `!ADBMouseDisabled` (`Wire_ADBMouseDisabled`).
`ADBMouseDisabled` starts as 1 (disabled) and is cleared when the ADB
manager first polls the mouse. This prevents mouse updates from
corrupting memory before the ADB subsystem is initialized.

### Mouse Updates

When `Mouse_Enabled()` is true, `MouseDevice::update()` writes the mouse
position and button state directly into low-memory globals (same as on
Plus). On Mac II, absolute position is written to `0x0828`/`0x082A` and
`0x0830` is updated; `0x08CE` is set to `0xFF` to signal a cursor dirty
flag.

---

## 5. Sound Subsystem (ASC)

Source: `src/devices/asc.cpp`, `src/devices/asc.h`

The Mac II uses the **Apple Sound Chip** (ASC), replacing the classic
DMA-less PWM scheme. The ASC provides stereo FIFO-based sample playback
and a wavetable synthesiser.

### Memory Map

| Mode     | Base Address   | Notes                                  |
|----------|----------------|----------------------------------------|
| 32-bit   | `0x50F00000`   | Masked to 12-bit register offset       |
| 24-bit   | `0x00F14000`   | Via 24-bit I/O window (io24 namespace) |

### Register Map

| Offset  | Name                    | Access | Description                              |
|---------|-------------------------|--------|------------------------------------------|
| 0x000–0x3FF | FIFO Channel A      | W      | 1 KB sample buffer, channel A            |
| 0x400–0x7FF | FIFO Channel B      | W      | 1 KB sample buffer, channel B            |
| 0x800   | Version             | R      | ASC version (reads 0)                    |
| 0x801   | Mode                | R/W    | `0` = wavetable, `1` = FIFO              |
| 0x802   | Control             | R/W    | Bit 1 = stereo enable                    |
| 0x803   | FIFO Mode           | R/W    | FIFO configuration                       |
| 0x804   | FIFO IRQ Status     | R/W    | Half-full/full flags; write triggers VIA2 CB1 |
| 0x805   | Wave Control        | W      | Wavetable control                        |
| 0x806   | Volume              | R/W    | Bits 7–5 = volume level (0–7)            |
| 0x807   | Clock Rate          | R/W    | Must be 0 (standard rate)                |
| 0x810–0x82F | Wavetable Channels | R/W  | 4 channels × frequency (4 B) + phase (4 B) |

### FIFO Operation

In FIFO mode (`s_soundReg801 == 1`), the host writes PCM samples to the
channel A and channel B buffers. Each buffer is a 16-bit ring:
`s_ascFifoInA` and `s_ascFifoInB` track the write pointers;
`s_ascFifoOut` is the common read pointer.

Status flags in register `0x804`:
- Bit 0 = channel A half-full (≥ 0x200 samples pending)
- Bit 1 = channel A full (≥ 0x400 samples pending)
- Bit 2 = channel B half-full
- Bit 3 = channel B full

Writing a non-zero value to register `0x804` pulses VIA2 CB1
(`via2->iCB1_PulseNtfy()`), generating a VIA2 interrupt.

### Sub-Tick Playback

`ASCDevice::subTick(i)` is called 16 times per 1/60th-second frame.
Each call drains one batch of samples from the FIFO and outputs them to
the host audio layer. This is the same 16-sub-tick schedule as classic
sound (§3 in PLUS.md), but driven from the FIFO instead of RAM.

---

## 6. Screen (NuBus Video Card)

Source: `src/devices/video.cpp`, `src/devices/video.h`

Unlike compact Macs, the Mac II has **no built-in screen**. The display
is provided by a NuBus video card emulated in slot 9.

### VRAM Layout

VRAM (`g_vidMem`, 512 KB) is mapped at different addresses depending on
the addressing mode:

| Mode   | VRAM Base       | Notes                                       |
|--------|-----------------|---------------------------------------------|
| 32-bit | `0xF9900000`    | NuBus super-slot 9 (up to 6 × 1 MB banks)  |
| 24-bit | `0x900000`      | 24-bit window; up to 4 MB usable            |

For depths ≥ 16bpp, 32-bit QuickDraw is required, so only 32-bit mode
is used for those depths in practice.

### NuBus Declaration ROM

`g_vidROM` (8 KB) contains the NuBus declaration ROM for the emulated
card. It is built at init time using `SlotROMWriter` and describes the
card's driver, `sResource` list, and one `VPBlock` per depth mode
(depths 0–5 = 1, 2, 4, 8, 16, 32 bpp).

The `ChecksumSlotROM()` function recomputes the CRC field each time the
ROM is written.

### Resolution and Depth Switching

`buildResolutionTable()` creates a table of supported resolutions:
`{512×342, 512×384, 640×480, 832×624, 1024×768, 1152×870}` plus up to
two host-derived resolutions (IDs 100/101) based on the host desktop size.

`Vid_SetMode(modeID)` changes depth; `SwitchMode(displayModeID)` changes
resolution. Both rewrite all `VPBlock` entries in `g_vidROM` in-place
(via `patchSlotROMVPBlocks()`), then recompute the ROM checksum. This is
the standard emulator technique to communicate resolution changes through
the Slot Manager API — the same approach used by Basilisk II.

### VBL Interrupt

`VideoDevice::update()` is called once per tick from `SixtiethSecondNotify()`.
If the VBL interrupt is not masked (`!VID_VBL_INT_UNENBL`):
1. Sets `Wire_VBLinterrupt` low (active low)
2. Pulses VIA2 CA1 (`via2->iCA1_PulseNtfy()`)

`Wire_VBLinterrupt` is mapped to VIA2 port A bit 0, so the CA1 edge
latches a VIA2 interrupt request → VIA2 fires IPL 2.

The `VBLintunenbl` flag is maintained in guest memory (via `Vid_SetVBLInt`)
and written back to `Wire_VBLintunenbl`.

### Framebuffer Access

The platform layer reads from `g_vidMem` directly.
`ScreenDevice::endTickNotify()` is still called each tick and blits
from `g_vidMem` (not from main RAM) to the host display. The screen
colour depth is configured by `s_currentDepth` (updated on mode switch).

---

## 7. RTC (Real-Time Clock)

Source: `src/devices/rtc.cpp`, `src/devices/rtc.h`

The Mac II uses the same hardware RTC serial protocol as the Plus (three
VIA1 port B lines: data, clock, enable). The wire connections are
identical to those documented in PLUS.md §7.

### Mac II-Specific PRAM Defaults

The RTC device has model-conditional PRAM defaults (`isIIFamily()` checks):

| Parameter          | Plus value  | Mac II value |
|--------------------|-------------|--------------|
| `DISK_CACHE_SZ`    | 4           | 1            |
| `CaretBlinkTime`   | `0x03`      | `0x08`       |
| `DoubleClickTime`  | `0x05`      | `0x08`       |

Additional Mac II PRAM groups differ in layout from compact Macs due to
larger extended PRAM storage and different default boot device records.
The `isIIFamily()` branches in `rtc.cpp` handle these differences for
PRAM read/write commands at registers 0x08–0x0B, 0x10–0x1F, and the
extended XPRAM region.

---

## 8. SCC (Serial Communications Controller)

Source: `src/devices/scc.h`, `src/devices/scc.cpp`

Identical to Plus (§8 of PLUS.md). The Mac II SCC is mapped at a different
address (see §14), but the device behaviour is the same.

On the Mac II, `Mouse_Enabled()` does **not** use the SCC MIE latch;
it uses `!ADBMouseDisabled` (see §4 above).

---

## 9. Main Loop Tick Sequence

Source: `src/core/main.cpp`

Each emulated 1/60th-second frame follows this sequence.

### SixtiethSecondNotify() — Start of Tick

| Order | Action                                       | Condition               |
|-------|----------------------------------------------|-------------------------|
| 1     | `MouseDevice::update()`                     | Always                  |
| 2     | `InterruptReset_Update()`                   | Always (NMI/reset)      |
| 3     | *(keyboard update skipped)*                 | `emClassicKbrd` = false |
| 4     | `ADBDevice::update()`                       | `emADB` (Mac II)        |
| 5     | `VIA1Device::iCA1_PulseNtfy()`              | Always — VBL via VIA1   |
| 6     | `SonyDevice::update()`                      | Always — floppy motor   |
| 7     | `SCCDevice::serialTick()`                   | Always — serial I/O     |
| 8     | `RTCDevice::interrupt()`                    | Always — 1-sec update   |
| 9     | `VideoDevice::update()`                     | `emVidCard` (Mac II) — also fires VIA2 CA1 VBL |
| 10    | `SubTickTaskStart()`                        | Starts 16 sub-ticks     |

**Note on VBL**: Both VIA1 CA1 (step 5) and VIA2 CA1 (step 9, via
`VideoDevice::update()`) are pulsed each tick. The ROM VBL task registers
to VIA2 CA1 (level 2 interrupt); VIA1 CA1 is still used for the 60 Hz
system tick on compact Macs but is not the primary VBL source on Mac II.

### Sub-Ticks (16 per frame)

Each sub-tick calls `ASCDevice::subTick(i)` (Mac II always uses ASC).

```
CyclesScaledPerTick    = 130240 * 2 * kCycleScale   (clockMult = 2)
CyclesScaledPerSubTick = CyclesScaledPerTick / 16
```

### SixtiethEndNotify() — End of Tick

| Order | Action                               |
|-------|--------------------------------------|
| 1     | `SubTickTaskEnd()` — final sub-tick  |
| 2     | `MouseDevice::endTickNotify()`       |
| 3     | `ScreenDevice::endTickNotify()`      |

### Extra Time

`ExtraTimeBeginNotify()` / `ExtraTimeEndNotify()` pause **both** VIA1
and VIA2 timers to avoid drift between ticks.

---

## 10. Interrupt Priority Chain (Mac II)

Source: `src/core/machine.cpp` — `VIAorSCCinterruptChngNtfy()`

The Mac II uses a four-level priority chain (one more level than compact
Macs, because VIA2 is added between VIA1 and SCC):

```cpp
/* Mac II priority: NMI > SCC > VIA2 > VIA1 */
if (g_interruptButton)        NewIPL = 7;  // NMI
else if (SCCInterruptRequest) NewIPL = 4;  // SCC
else if (VIA2_InterruptRequest) NewIPL = 2; // VIA2 (VBL, ASC, NuBus slots)
else if (VIA1_InterruptRequest) NewIPL = 1; // VIA1 (RTC, ADB, IWM)
else                          NewIPL = 0;
```

| IPL | Source              | 68020 Level | Typical use                        |
|-----|---------------------|-------------|-------------------------------------|
| 7   | NMI button          | NMI         | Debugger / interrupt key            |
| 4   | SCC interrupt       | Level 4     | Serial I/O                          |
| 2   | VIA2 interrupt      | Level 2     | VBL (video card), ASC FIFO, NuBus  |
| 1   | VIA1 interrupt      | Level 1     | RTC one-second, ADB, IWM            |
| 0   | No interrupt        | —           |                                     |

Wire change callbacks registered for all four sources:
```cpp
g_wires.onChange(Wire_VIA1_InterruptRequest, VIAorSCCinterruptChngNtfy);
g_wires.onChange(Wire_VIA2_InterruptRequest, VIAorSCCinterruptChngNtfy);
g_wires.onChange(Wire_SCCInterruptRequest,   VIAorSCCinterruptChngNtfy);
```
The interrupt button is handled separately by `SetInterruptButton()`.

When the IPL changes, `g_cpu.iplChangeNotify()` updates the 68020's
pending-interrupt state.

---

## 11. ROM Handling

Source: `src/devices/rom.cpp`

### ROM Loading

The ROM image (`MacII.ROM`, 256 KB) is loaded into `g_rom`. Patches are
then applied:

1. **Skip ROM checksum** (offset `0x2AB0`): patch `0x6008` to skip checksum loop
2. **Shorten RAM test** (offsets `0xEE`, `0x1AA`): `0x6002` to skip read/write
   delay loops
3. **Sony driver patch**: installed at ROM offset `0x2D72C` (differs from
   the Plus offset `0x17D30`). See §12 for details.

### Memory Overlay Mechanism

At reset the 68020 reads its initial stack pointer and PC from address 0.
The Mac II ROM is at `0x800000` (24-bit) / `0x40000000` (32-bit), so at
power-on the ROM is overlaid at the low end of address space.

**24-bit mode** (default at reset):
- Overlay maps ROM at `0x000000` (same as compact Macs, via
  `GetOverlayROMCmpZeroMask()`)
- VIA1 port A bit 4 (`Wire_VIA1_iA4`) cleared → `MemOverlay_ChangeNtfy()`
  → `SetUpMemBanks()` removes overlay

**32-bit mode** (after `ADDR32` bit set in VIA2 port B bit 3):
- Overlay maps ROM at `0x00000000` via mask `~((1 << 30) - 1)`, `cmpvalu = 0`
- The same VIA1 port A bit 4 clears the overlay

`Addr32_ChangeNtfy()` is called when `Wire_VIA2_iB3` changes; it calls
`SetUpMemBanks()` to switch between 24-bit and 32-bit ATT layouts.

---

## 12. IWM / Sony Floppy

Source: `src/devices/sony.h`, `src/devices/iwm.h`

The Mac II uses the same IWM and Sony driver mechanism as the Plus.

### Sony Driver Location

The Sony driver is patched into ROM at offset `0x2D72C` (vs. `0x17D30`
for the Plus). The patched driver traps to `extnBlockBase = 0x50F0C000`
(in 32-bit mode) to communicate with `SonyDevice`.

### IWM Address

In 32-bit mode, IWM is at `0x50016000`; in 24-bit mode at `0x00F16000`.

---

## 13. SCSI

Source: `src/devices/scsi.h`, `src/devices/scsi.cpp`

Same NCR 5380 SCSI as on the Plus. On the Mac II:
- 32-bit address: `0x50010000`
- 24-bit address: `0x00F10000`

---

## 14. Address Space Maps

Source: `src/core/machine.cpp` — `SetUp_address32()`, `SetUp_address24()`

The Mac II supports both 24-bit and 32-bit addressing modes. The active
mode is determined by `ADDR32` (read from `Wire_VIA2_iB3`) and switched
dynamically by the ROM/OS.

### I/O Registers (both modes)

In 32-bit mode, all I/O is in the `0x50000000` range. In 24-bit mode it
is in the `0x00F00000` range. The offsets within the range are the same:

| Offset    | Device    | 32-bit Address | 24-bit Address |
|-----------|-----------|----------------|----------------|
| `+0x00000` | VIA1     | `0x50000000`   | `0x00F00000`   |
| `+0x02000` | VIA2     | `0x50002000`   | `0x00F02000`   |
| `+0x04000` | SCC      | `0x50004000`   | `0x00F04000`   |
| `+0x0C000` | Extension| `0x5000C000`   | `0x00F0C000`   |
| `+0x10000` | SCSI     | `0x50010000`   | `0x00F10000`   |
| `+0x14000` | ASC      | `0x50014000`   | `0x00F14000`   |
| `+0x16000` | IWM      | `0x50016000`   | `0x00F16000`   |

### 32-bit Address Space

| Address Range              | Device / Region                          |
|----------------------------|------------------------------------------|
| `0x00000000`–`0x3FFFFFFF`  | RAM (8 MB default; bank select via VIA2) |
| `0x40000000`–`0x4003FFFF`  | ROM (256 KB, mirrored up to 256 MB)      |
| `0x50000000`–`0x5001FFFF`  | I/O (VIA1, VIA2, SCC, SCSI, ASC, IWM)   |
| `0x50F0C000`               | Extension block (traps)                  |
| `0xF9900000`–`0xF9EFFFFF`  | NuBus VRAM (slot 9, up to 6 × 1 MB)     |
| `0xF9F00000`–`0xF9F01FFF`  | NuBus declaration ROM (slot 9)           |

### 24-bit Address Space

| Address Range           | Device / Region                          |
|-------------------------|------------------------------------------|
| `0x000000`–`0x7FFFFF`   | RAM (overlay off) / ROM (overlay on)     |
| `0x800000`–`0x83FFFF`   | ROM (256 KB, mirrored)                   |
| `0x900000`–`0x9FFFFF`   | VRAM bank 0 (1 MB window)                |
| `0xA00000`–`0xAFFFFF`   | VRAM bank 1 (if vidMemSize ≥ 2 MB)       |
| `0xB00000`–`0xCFFFFF`   | VRAM banks 2–3 (if vidMemSize ≥ 4 MB)   |
| `0xF00000`–`0xF1FFFF`   | I/O (VIA1, VIA2, SCC, SCSI, ASC, IWM)   |
| `0xF0C000`              | Extension block (traps, 24-bit alias)    |

---

## 15. Wire Bus Summary (Mac II-Relevant Wires)

Source: `src/core/wire_ids.h`, `src/core/wire_bus.h`

| Wire ID                         | Direction      | Producer            | Consumer(s)                    |
|---------------------------------|----------------|---------------------|--------------------------------|
| `Wire_VIA1_iA4` (MemOverlay)   | VIA1 → Mem     | VIA1 port A[4]      | `MemOverlay_ChangeNtfy`        |
| `Wire_VIA1_iA5` (IWMvSel)      | VIA1 → IWM     | VIA1 port A[5]      | IWM device                     |
| `Wire_VIA1_iA7` (SCCwaitrq)    | SCC → VIA1     | SCC                 | VIA1 port A read               |
| `Wire_VIA1_iB0` (RTCdataLine)  | Bidir          | VIA1/RTC            | `RTCDevice::dataLineChangeNtfy`|
| `Wire_VIA1_iB1` (RTCclock)     | VIA1 → RTC     | VIA1 port B[1]      | `RTCDevice::clockChangeNtfy`   |
| `Wire_VIA1_iB2` (RTCunEnabled) | VIA1 → RTC     | VIA1 port B[2]      | `RTCDevice::unEnabledChangeNtfy`|
| `Wire_VIA1_iB3` (ADB_Int)      | ADB → VIA1     | `ADBDevice`         | VIA1 port B read               |
| `Wire_VIA1_iB4` (ADB_st0)      | VIA1 → ADB     | VIA1 port B[4]      | `ADBDevice::stateChangeNtfy`   |
| `Wire_VIA1_iB5` (ADB_st1)      | VIA1 → ADB     | VIA1 port B[5]      | `ADBDevice::stateChangeNtfy`   |
| `Wire_VIA1_iCB2` (ADB_Data)    | Bidir          | VIA1/ADB            | `ADBDevice::dataLineChngNtfy`  |
| `Wire_VIA1_InterruptRequest`    | VIA1 → CPU     | VIA1                | `VIAorSCCinterruptChngNtfy`    |
| `Wire_VIA2_InterruptRequest`    | VIA2 → CPU     | VIA2                | `VIAorSCCinterruptChngNtfy`    |
| `Wire_VIA2_iB3` (Addr32)       | VIA2 → Mem     | VIA2 port B[3]      | `Addr32_ChangeNtfy`            |
| `Wire_VBLinterrupt`             | Video → VIA2   | `VideoDevice`       | VIA2 port A[0] (CA1 latch)     |
| `Wire_VBLintunenbl`             | Guest → Video  | guest memory write  | `VideoDevice::update`          |
| `Wire_ADBMouseDisabled`         | ADB → Mouse    | `ADBDevice`         | `Mouse_Enabled()`              |
| `Wire_SCCInterruptRequest`      | SCC → CPU      | SCC                 | `VIAorSCCinterruptChngNtfy`    |

---

## 16. ICT Scheduler Tasks (Mac II)

Source: `src/core/main.cpp` — `InitEmulation()`

| ICT Task                      | Handler                            | Registered When     |
|-------------------------------|------------------------------------|---------------------|
| `kICT_SubTick`                | `SubTickTaskDo()`                  | Always              |
| `kICT_ADB_NewState`           | `ADBDevice::doNewState()`          | `emADB` (Mac II)    |
| `kICT_VIA1_Timer1Check`       | `VIA1Device::doTimer1Check()`      | `emVIA1`            |
| `kICT_VIA1_Timer2Check`       | `VIA1Device::doTimer2Check()`      | `emVIA1`            |
| `kICT_VIA2_Timer1Check`       | `VIA2Device::doTimer1Check()`      | `emVIA2` (Mac II)   |
| `kICT_VIA2_Timer2Check`       | `VIA2Device::doTimer2Check()`      | `emVIA2` (Mac II)   |

Tasks **not** registered on Mac II (no classic keyboard, no PMU):
`kICT_Kybd_ReceiveCommand`, `kICT_Kybd_ReceiveEndCommand`, `kICT_PMU_Task`.

---

## 17. Boot Sequence Summary

1. CPU reset: reads vectors from address 0 (ROM overlaid at `0x00000000`)
2. ROM checksum (skipped by patch at offset `0x2AB0`)
3. RAM test (shortened by patches at offsets `0xEE` and `0x1AA`)
4. ROM initialises VIA2; writes `ADDR32 = 0` → 24-bit mode initially
5. RTC initialized via VIA1 port B serial protocol
6. ADB manager starts; `ADBMouseDisabled` cleared → `Mouse_Enabled()` becomes true
7. ROM clears `MemOverlay` (VIA1 port A bit 4) → RAM appears at `0x000000`
8. ROM switches to 32-bit mode (VIA2 port B bit 3 set) → `Addr32_ChangeNtfy()`
9. NuBus probe: Slot Manager reads declaration ROM at `0xF9F00000`, discovers
   the emulated video card (slot 9)
10. Video card driver loaded; VBL interrupt enabled via VIA2 CA1 / `Wire_VBLinterrupt`
11. Sony driver patched at `0x2D72C` → disk I/O through extension traps at `0x50F0C000`
12. ASC initialized; FIFO ready for Sound Manager use
13. Finder / System loaded from SCSI disk image

---

## 18. Key Differences from Mac Plus

| Feature              | Mac Plus                              | Mac II                                        |
|----------------------|---------------------------------------|-----------------------------------------------|
| CPU                  | 68000 @ 8 MHz                         | 68020 @ 16 MHz (`clockMult = 2`)              |
| FPU                  | None                                  | 68881 (`emFPU = true`)                        |
| RAM                  | 4 MB (single bank)                    | 4 MB + 4 MB (two banks, bank select via VIA2) |
| ROM                  | 128 KB at `0x400000`                  | 256 KB at `0x800000`                          |
| VIA2                 | No                                    | Yes — VBL, ADDR32, NuBus interrupts           |
| Keyboard             | Classic shift-register (VIA1 SR/CB2)  | ADB (VIA1 portB + CB2)                        |
| Mouse gating         | SCC MIE one-shot latch                | ADB `ADBMouseDisabled` flag                   |
| Sound                | Classic PWM from top of RAM           | ASC FIFO (`emASC = true`)                     |
| Screen               | Built-in 512×342 1bpp in main RAM     | NuBus card, 640×480 8bpp, dedicated VRAM      |
| VBL interrupt        | VIA1 CA1 → IPL 1                      | VIA2 CA1 (video card) → IPL 2                |
| Interrupt priority   | NMI(7) > SCC(2) > VIA1(1)            | NMI(7) > SCC(4) > VIA2(2) > VIA1(1)          |
| Addressing mode      | 24-bit only                           | 24-bit or 32-bit (switched via VIA2 port B)   |
| Address space        | Compact 24-bit map                    | 32-bit I/O at `0x50000000`, NuBus at `0xF9xxxxx` |
| Sony driver offset   | ROM `0x17D30`                         | ROM `0x2D72C`                                 |
| ATT list size        | 16 entries                            | 20 entries (`maxATTListN = 20`)               |
