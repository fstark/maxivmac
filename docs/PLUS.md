# Mac Plus Hardware Emulation Architecture

This document describes the Mac Plus emulation as implemented in this codebase.
It covers every device and subsystem relevant to the Plus model, the wire bus
interconnects, the main-loop tick sequence, and the interrupt priority chain.

---

## 1. Machine Configuration (MacModel::Plus)

Source: `src/core/machine_config.cpp` — `MachineConfigForModel(MacModel::Plus)`

| Field             | Value                  | Notes                                          |
|-------------------|------------------------|-------------------------------------------------|
| `use68020`        | `false`                | 68000 CPU                                       |
| `emFPU`           | `false`                | No FPU                                          |
| `emMMU`           | `false`                | No MMU (24-bit addressing only)                 |
| `ramASize`        | `0x00400000` (4 MB)    | Single RAM bank                                 |
| `ramBSize`        | `0`                    | No bank B                                       |
| `romSize`         | `0x00020000` (128 KB)  | Plus ROM                                        |
| `romBase`         | `0x00400000`           | ROM mapped at 4 MB                              |
| `romFileName`     | `"vMac.ROM"`           | 128 KB Plus ROM image                           |
| `extnBlockBase`   | `0x00F0C000`           | 24-bit address space extension block            |
| `emVIA1`          | `true`                 | Single VIA (6522)                               |
| `emVIA2`          | `false`                | No VIA2 (Mac II only)                           |
| `emADB`           | `false`                | No ADB (SE and later)                           |
| `emClassicKbrd`   | `true`                 | Classic keyboard shift-register protocol        |
| `emASC`           | `false`                | No Apple Sound Chip                             |
| `emClassicSnd`    | `true`                 | Classic PWM-in-RAM sound                        |
| `emVidCard`       | `false`                | No NuBus video card                             |
| `includeVidMem`   | `false`                | No dedicated VRAM (framebuffer is in main RAM)  |
| `screenWidth`     | `512`                  | Built-in CRT width                              |
| `screenHeight`    | `342`                  | Built-in CRT height                             |
| `screenDepth`     | `0`                    | 1-bit (monochrome)                              |
| `clockMult`       | `2` (default)          | Cycle multiplier for timing                     |
| `via1Config`      | `MakeVIA1Config_Plus()`| See §2 below                                   |

Helper predicates: `isCompactMac()` → `true`, `isIIFamily()` → `false`,
`isSEOrLater()` → `false`.

---

## 2. VIA1 Configuration (Plus-Specific)

Source: `src/core/machine_config.cpp` — `MakeVIA1Config_Plus()`

The Mac Plus has a single **SY6522 VIA** controlling all peripheral I/O.
There is no VIA2.

### Port Masks

| Field              | Value  | Meaning                                                     |
|--------------------|--------|-------------------------------------------------------------|
| `oraFloatVal`      | `0xFF` | Undriven port A bits float high                             |
| `orbFloatVal`      | `0xFF` | Undriven port B bits float high                             |
| `oraCanIn`         | `0x80` | Bit 7 readable (SCC wait/request)                           |
| `oraCanOut`        | `0x7F` | Bits 0-6 writable                                           |
| `orbCanIn`         | `0x79` | Bits 0,3,4,5,6 readable (RTC data, mouse btn, quadrature, H4) |
| `orbCanOut`        | `0x87` | Bits 0,1,2,7 writable (RTC data/clock/enable, sound disable) |
| `ierNever0`        | `0x02` | IER bit 1 forced to 0                                      |
| `ierNever1`        | `0x18` | IER bits 3,4 forced to 1                                   |
| `cb2ModesAllowed`  | `0x01` | Only mode 0                                                 |
| `ca2ModesAllowed`  | `0x01` | Only mode 0                                                 |

### Port A Wire Mapping

| Bit | Wire ID           | Signal            |
|-----|-------------------|-------------------|
| 0   | `Wire_SoundVolb0` | Sound volume bit 0 |
| 1   | `Wire_SoundVolb1` | Sound volume bit 1 |
| 2   | `Wire_SoundVolb2` | Sound volume bit 2 |
| 3   | `Wire_VIA1_iA3`   | SCC vSync          |
| 4   | `Wire_VIA1_iA4`   | **MemOverlay**     |
| 5   | `Wire_VIA1_iA5`   | IWM vSel           |
| 6   | `Wire_VIA1_iA6`   | (unused on Plus)   |
| 7   | `Wire_VIA1_iA7`   | SCC wait/request   |

### Port B Wire Mapping

| Bit | Wire ID             | Signal                     |
|-----|---------------------|----------------------------|
| 0   | `Wire_VIA1_iB0`     | **RTC data line**          |
| 1   | `Wire_VIA1_iB1`     | **RTC clock**              |
| 2   | `Wire_VIA1_iB2`     | **RTC chip-enable (active low)** |
| 3   | `Wire_VIA1_iB3`     | **Mouse button** (1 = up)  |
| 4   | `Wire_VIA1_iB4`     | Mouse X2 / quadrature      |
| 5   | `Wire_VIA1_iB5`     | Mouse Y2 / quadrature      |
| 6   | `Wire_VIA1_iB6`     | H4 (horizontal blank)      |
| 7   | `Wire_SoundDisable`  | **Sound disable** (1 = mute) |

### CB2 and Interrupt

| Wire                      | Purpose                    |
|---------------------------|----------------------------|
| `Wire_VIA1_iCB2`         | Keyboard data (shift reg.) |
| `Wire_VIA1_InterruptRequest` | VIA1 → CPU IPL         |

---

## 3. Sound Subsystem (Classic Sound)

Source: `src/devices/sound.cpp`, `src/devices/sound.h`

The Plus uses "Classic Sound" — a DMA-less scheme where sound samples live
in the top of main RAM, and the emulator reads them during each sub-tick.

### Sound Buffer Location

```
Main buffer:      RAM[ramSize - 0x0300]   (kSnd_Main_Offset = 0x0300)
Alternate buffer: RAM[ramSize - 0x5F00]   (kSnd_Alt_Offset  = 0x5F00)
```

Buffer selection is controlled by VIA1 port A bit 3 (`SoundBuffer` wire),
though in the current code the main buffer is the default.

Each buffer contains 370 entries, each a 16-bit word (only the high byte
is used as the sample value). The buffer is split into 16 sub-ticks per
1/60th-second frame, with offset/count tables:

```c
SubTick_offset[16] = { 0, 25, 50, 90, 102, 115, 138, 161, 185, 208, 231, 254, 277, 300, 323, 346 };
SubTick_n[16]      = { 25, 25, 40, 12,  13,  23,  23,  24,  23,  23,  23,  23,  23,  23,  23,  24 };
```

### Wires Read

| Wire               | Purpose                          |
|--------------------|----------------------------------|
| `Wire_SoundDisable`| VIA1 port B bit 7; when set and T1 invert time is 0, output silence |
| `Wire_SoundVolb0`  | VIA1 port A bit 0 — volume bit 0 |
| `Wire_SoundVolb1`  | VIA1 port A bit 1 — volume bit 1 |
| `Wire_SoundVolb2`  | VIA1 port A bit 2 — volume bit 2 |

Volume (0–7) is assembled as `SoundVolb0 | (SoundVolb1 << 1) | (SoundVolb2 << 2)`.
Volume 7 is full scale; volumes 0–6 are attenuated via a lookup table:

```c
vol_mult[] = { 8192, 9362, 10922, 13107, 16384, 21845, 32768 };
```

### T1 Invert Time (Square-Wave Modulation)

`VIA1Device::getT1InvertTime()` returns the 16-bit T1 latch value when the
VIA1 ACR bits 6-7 are both set (free-running square-wave mode, `ACR & 0xC0 == 0xC0`);
otherwise it returns 0.

When `SoundInvertTime != 0`, the sound output is multiplied by a square wave
whose half-period is proportional to `SoundInvertTime * 20`. This implements
the Plus's hardware square-wave generator used for the startup chime and
alert sounds. The algorithm tracks a phase counter (`soundInvertPhase_`) and
a toggle state (`soundInvertState_`), modulating each sample by the fraction
of the sample period spent in the "on" phase.

### Classic Sound vs. ASC

| Feature           | Classic Sound (Plus/SE/Classic) | ASC (Mac II/IIx/PB100)         |
|-------------------|---------------------------------|---------------------------------|
| Sound source      | Samples in top of main RAM      | Dedicated ASC chip with FIFO    |
| Volume control    | 3-bit via VIA1 port A           | ASC register-based              |
| Square wave       | VIA1 Timer 1 free-run mode      | ASC built-in tone generator     |
| Sub-tick driven   | Yes (16 sub-ticks/frame)        | Yes (16 sub-ticks/frame)        |
| Config flag       | `emClassicSnd = true`           | `emASC = true`                  |

---

## 4. Mouse

Source: `src/devices/mouse.cpp`, `src/devices/mouse.h`

### Mouse Enable Gate

On the Plus (`emClassicKbrd = true`), `Mouse_Enabled()` returns
`SCC_InterruptsEnabled()` — i.e., `SCC.MIE` (Master Interrupt Enable).
This prevents mouse updates during the early boot-time memory test, which
would corrupt low memory. The SCC MIE bit is not set until the ROM has
finished the memory test and initialized the SCC.

On Mac II (`emADB`, `!emClassicKbrd`), `Mouse_Enabled()` returns
`!ADBMouseDisabled`, which starts as disabled and is cleared when the ADB
manager first polls the mouse.

### update() — Called at Start of Each 1/60th Tick

1. Decrements `MasterMyEvtQLock` (debounce timer preventing rapid events).
2. If `Mouse_Enabled()` and the event queue has a pending event:
   - **MouseDelta** (Plus only, `emClassicKbrd` + `EnableMouseMotion`):
     Adds delta to raw mouse position at low-memory globals
     `0x0828` (V) and `0x082A` (H), then pokes `0x08CE ← 0x08CF`
     to signal "cursor dirty".
   - **MousePos**: Writes absolute position to `0x0828` (mouse),
     `0x082C` (last mouse), and on Plus pokes `0x08CE ← 0x08CF`
     (on Mac II, writes `0x0830` and sets `0x08CE = 0xFF`).
3. **Mouse button** (Plus only, `emClassicKbrd`): If a button event is
   pending, sets `Wire_VIA1_iB3` (0 = pressed, 1 = up) and locks the
   event queue for 4 ticks (`MasterMyEvtQLock = 4`).

### endTickNotify() — Called at End of Each Tick

Reads back `CurMouseV` / `CurMouseH` from low-memory (`0x082C`/`0x082E`)
to tell the platform layer where the cursor ended up. This is only done
when `Mouse_Enabled()`.

### Mouse Quadrature (Original Hardware)

On real Plus hardware, the mouse generates quadrature signals on
VIA1 port B bits 4 (X2) and 5 (Y2). The current emulation **does not
use quadrature** — instead it directly writes the mouse position into
low-memory globals, which is the standard vMac approach and works because
emulated software reads these globals rather than decoding quadrature.

Port B bits 4 and 5 (`Wire_VIA1_iB4`, `Wire_VIA1_iB5`) are declared as
readable in `orbCanIn = 0x79` but are not actively driven by the emulator.

---

## 5. Keyboard (Classic Shift-Register Protocol)

Source: `src/devices/keyboard.cpp`, `src/devices/keyboard.h`

The Plus keyboard communicates via the VIA1 shift register (SR) and
the CB2 data line (`Wire_VIA1_iCB2`). This is a synchronous serial
protocol where the Mac is the master.

### State Machine

```
kKybdStateIdle
    │
    ├─ CB2 goes low → kKybdStateRecievingCommand
    │     (schedule kICT_Kybd_ReceiveCommand after 6800 cycles)
    │
    ├─ receiveCommand() → kKybdStateRecievedCommand
    │     (process command byte from VIA1 shift register)
    │
    └─ CB2 goes high → kKybdStateRecievingEndCommand
          (schedule kICT_Kybd_ReceiveEndCommand after 6800 cycles)
          → kKybdStateIdle (if buffered result, shift it in)
```

### Keyboard Commands

| Command | Code   | Response                                                   |
|---------|--------|------------------------------------------------------------|
| Inquiry | `0x10` | Waits up to 16 ticks for a key event; sends key or `0x7B` (null) |
| Instant | `0x14` | Returns `instantCommandData_` (default `0x7B`), resets to `0x7B` |
| Model   | `0x16` | Returns `0x0B` — keyboard model ID (Model 0, no extra devices) |
| Test    | `0x36` | Returns `0x7D` (test OK)                                   |
| `0x00`  | `0x00` | Returns `0x00`                                              |

### Key Encoding

Keys 0–63 are encoded as `(keycode << 1)`, with bit 7 set for key-up.
Keys 64+ require a two-byte sequence: the first byte is `121` (`0x79`),
and the second byte (returned on the next Instant command) is
`((keycode - 64) << 1)` with bit 7 for key-up.

### Inquiry Timeout

`MaxKeyboardWait = 16` (16/60th of a second). The ROM resets the keyboard
if it hasn't responded in 32/60th, so the emulator times out at 16/60th
and sends the null key response `0x7B`.

### Data Flow

- **Mac → Keyboard**: Mac pulls CB2 low; VIA1 shifts out command byte.
  `KeyboardDevice::receiveCommand()` reads it via `via1()->shiftOutData()`.
- **Keyboard → Mac**: `gotKeyBoardData(v)` calls `via1()->shiftInData(v)`
  to load the response into the VIA1 SR, then raises CB2 to signal "data ready".

### Per-Tick Update

`KeyboardDevice::update()` is called each 1/60th tick. If an Inquiry
command is pending (`inquiryCommandTimer_ != 0`), it tries to find a key
event. If none is found, the timer counts down; at zero it sends `0x7B`.

---

## 6. Screen (Built-in 512×342 Monochrome)

Source: `src/devices/screen.cpp`, `src/devices/screen.h`

### Framebuffer Location

For compact Macs (Plus/SE/128K), the screen buffer lives at the **top of
main RAM**:

```c
#define kMain_Offset      0x5900
#define kAlternate_Offset 0xD900

screencurrentbuff = RAM + (ramSize - kMain_Offset);   // main screen page
```

For the Plus with 4 MB RAM: `0x400000 - 0x5900 = 0x3FA700`.

The alternate screen page at `kAlternate_Offset` can be selected via the
`SCRNvPage2` wire (not currently wired in the emulator).

For Mac II (`includeVidMem = true`), the screen comes from dedicated `VidMem`.

### Rendering

`ScreenDevice::endTickNotify()` is called once per 1/60th tick (at the end
of the tick, after all device updates). It passes the framebuffer pointer
to `Screen_OutputFrame(screencurrentbuff)`, which is the platform-specific
routine that blits the 1-bit data to the host display.

The framebuffer is 512 × 342 × 1 bpp = 21,888 bytes (≈ 21.4 KB).

---

## 7. RTC (Real-Time Clock)

Source: `src/devices/rtc.cpp`, `src/devices/rtc.h`

The Plus uses a custom serial RTC chip (similar to the Macintosh 128K)
communicating via three VIA1 port B lines.

### Wire Connections

| VIA1 Port B Bit | Wire ID                   | RTC Signal    |
|-----------------|---------------------------|---------------|
| 0               | `Wire_VIA1_iB0_RTCdataLine` | Serial data (bidirectional) |
| 1               | `Wire_VIA1_iB1_RTCclock`    | Serial clock                |
| 2               | `Wire_VIA1_iB2_RTCunEnabled`| Chip enable (active low — 0 = enabled) |

Wire change callbacks are registered in `AddrSpac_Init()`:
- `Wire_VIA1_iB0` → `RTCDevice::dataLineChangeNtfy()`
- `Wire_VIA1_iB1` → `RTCDevice::clockChangeNtfy()`
- `Wire_VIA1_iB2` → `RTCDevice::unEnabledChangeNtfy()`

### Serial Protocol

The RTC uses an 8-bit shift register (`RTC.ShiftData`) clocked by the VIA.
A 3-bit counter tracks bit position, counting down from 7 to 0.

**State machine** (`RTC.Mode`):

| Mode | Meaning                              |
|------|--------------------------------------|
| 0    | Waiting for command byte             |
| 1    | Waiting for write data byte          |
| 2    | Waiting for extended-command address  |
| 3    | Waiting for extended-command data     |

**Command format**: Command byte bits 7 (read/write), bits 6-2 (register),
bits 1-0 (unused). Bit pattern `0x38` mask signals an extended command.

**Registers**:
- Seconds (4 bytes, little-endian): real-time clock counter
- PRAM Group 1 (16 bytes @ offset 0x10): serial port config, font, keyboard
- PRAM Group 2 (4 bytes @ offset 0x08): volume, click, caret blink
- XPRAM (256 bytes total): extended parameter RAM (all models in this codebase)
- Write-protect register (bit 7 of byte written to register 13)

### Clock Cycle

On each rising edge of RTCclock (when `RTCunEnabled` is low):
1. If `DataOut`: shift data out on the data line (MSB first), decrement counter
2. Else: shift data in from VIA data line, decrement counter
3. When counter reaches 0: call `RTC_DoCmd()` to process the byte

### One-Second Interrupt

`RTCDevice::interrupt()` is called once per 1/60th tick. It compares the
current real date to `LastRealDate`, incrementing the Seconds register by
the delta. If time advanced, it pulses VIA1 CA2 (`via1->iCA2_PulseNtfy()`)
to generate the one-second interrupt.

---

## 8. SCC (Serial Communications Controller)

Source: `src/devices/scc.h`, `src/devices/scc.cpp`

The Zilog 8530 SCC provides two serial channels (modem and printer ports).

### SCC_InterruptsEnabled / MIE

```cpp
bool SCCDevice::interruptsEnabled() {
    return SCC.MIE;  // Master Interrupt Enable in WR9
}
```

`SCC.MIE` is the Master Interrupt Enable bit from SCC Write Register 9.
It controls whether the SCC drives its interrupt output.

### Role in Mouse Gating

On the Plus, `Mouse_Enabled()` returns `scc->interruptsEnabled()`. This
means mouse updates are suppressed until the ROM initializes the SCC and
sets MIE — which happens *after* the memory test. This prevents the mouse
update code from corrupting low-memory addresses during the self-test.

### SCC Interrupt Wire

The SCC drives `Wire_SCCInterruptRequest`, which feeds into the interrupt
priority encoder (see §11).

---

## 9. Main Loop Tick Sequence

Source: `src/core/main.cpp`

Each emulated 1/60th-second frame follows this sequence:

### SixtiethSecondNotify() — Start of Tick

| Order | Action                                    | Condition              |
|-------|-------------------------------------------|------------------------|
| 1     | `MouseDevice::update()`                  | Always                 |
| 2     | `InterruptReset_Update()`                | Always (NMI/reset)     |
| 3     | `KeyboardDevice::update()`               | `emClassicKbrd` (Plus) |
| 4     | `ADBDevice::update()`                    | `emADB` (not Plus)     |
| 5     | `VIA1Device::iCA1_PulseNtfy()`           | Always — **VBL interrupt** |
| 6     | `SonyDevice::update()`                   | Always — floppy motor  |
| 7     | `SCCDevice::localTalkTick()`             | `EmLocalTalk` only     |
| 8     | `RTCDevice::interrupt()`                 | Always — 1-sec update  |
| 9     | `VideoDevice::update()`                  | `emVidCard` (not Plus) |
| 10    | `SubTickTaskStart()`                     | Starts 16 sub-ticks    |

### Sub-Ticks (16 per frame)

Each sub-tick calls `SoundDevice::subTick(i)` (classic sound) or
`ASCDevice::subTick(i)` (ASC sound). Sub-ticks are scheduled via the ICT
scheduler at intervals of `CyclesScaledPerSubTick`.

```
CyclesScaledPerTick    = 130240 * clockMult * kCycleScale
CyclesScaledPerSubTick = CyclesScaledPerTick / 16
```

### SixtiethEndNotify() — End of Tick

| Order | Action                                |
|-------|---------------------------------------|
| 1     | `SubTickTaskEnd()` — final sub-tick   |
| 2     | `MouseDevice::endTickNotify()`        |
| 3     | `ScreenDevice::endTickNotify()`       |

### Extra Time (between ticks)

`ExtraTimeBeginNotify()` / `ExtraTimeEndNotify()` pause/resume VIA timers
to avoid timer drift during extra emulation cycles between ticks.

---

## 10. Interrupt Priority Chain (Plus / Compact Mac)

Source: `src/core/machine.cpp` — `VIAorSCCinterruptChngNtfy()`

The Plus uses a simple priority encoding that maps three interrupt sources
into the 68000's 3-bit IPL (Interrupt Priority Level):

```cpp
/* Compact Mac priority encoding */
uint8_t VIAandNotSCC = VIA1_InterruptRequest & ~SCCInterruptRequest;
NewIPL = VIAandNotSCC
       | (SCCInterruptRequest << 1)
       | (InterruptButton << 2);
```

| IPL | Source                | 68000 Level |
|-----|-----------------------|-------------|
| 7   | InterruptButton (NMI) | N/A (uses bit 2 OR'd) |
| 4   | InterruptButton alone | Level 4     |
| 2   | SCC interrupt         | Level 2     |
| 1   | VIA1 (when SCC clear) | Level 1     |
| 0   | No interrupt          | —           |

Key difference from Mac II: There is **no VIA2**, so the VIA1 interrupt is
at level 1 and the SCC is at level 2. On the Mac II, the priority is:
NMI(7) > SCC(4) > VIA2(2) > VIA1(1).

Wire change callbacks are registered for all three sources:
```cpp
g_wires.onChange(Wire_VIA1_InterruptRequest, VIAorSCCinterruptChngNtfy);
g_wires.onChange(Wire_VIA2_InterruptRequest, VIAorSCCinterruptChngNtfy);
g_wires.onChange(Wire_SCCInterruptRequest, VIAorSCCinterruptChngNtfy);
```

When the IPL changes, `g_cpu.iplChangeNotify()` is called to update the
68000's pending-interrupt state.

---

## 11. ROM Handling

Source: `src/devices/rom.cpp`, `src/devices/rom.h`

### ROM Loading

The ROM image (`vMac.ROM`, 128 KB) is loaded into the `ROM` buffer. The
device's `init()` method then applies patches:

1. **Skip ROM checksum** (at offset `0xD7A`): `NOP` over the checksum loop
2. **Shorten RAM test** (offsets 3728, 3752): `NOP` out read/write delay loops
3. **Sony driver patch**: Replaces the built-in `.Sony` floppy driver with a
   custom driver that traps to the emulator's disk subsystem. The Sony driver
   is installed at ROM offset `0x17D30` (for Plus).
4. **Disk icon**: A custom floppy-disk icon is patched into ROM after the
   Sony driver.

### Memory Overlay Mechanism

At reset, the 68000 expects to read vectors from address 0. On the Plus,
ROM is at `0x400000`, so a "memory overlay" maps ROM into the low address
space at boot:

- `Wire_VIA1_iA4` = `Wire_VIA1_iA4_MemOverlay` (VIA1 port A bit 4)
- At reset: overlay is **on** — ROM appears at both `0x000000` and `0x400000`;
  RAM is mapped at `0x600000`.
- The ROM startup code clears overlay by writing to VIA1 port A bit 4,
  which triggers `MemOverlay_ChangeNtfy()` → `SetUpMemBanks()`.
- With overlay **off**: RAM at `0x000000`, ROM at `0x400000`.

On SE and later models, an additional `kMAN_OverlayOff` notification handler
auto-clears the overlay bit on the first write to the RAM region.

---

## 12. IWM / Sony Floppy

Source: `src/devices/sony.h`, `src/devices/iwm.h`

### IWM (Integrated Woz Machine)

The IWM handles low-level floppy drive signaling. It is memory-mapped at
`0xC00000` (for compact Macs, `0xF60000` on PB100) and is accessed via the
address translation table in `machine.cpp`.

### Sony Device

`SonyDevice` manages the high-level floppy disk emulation:
- `update()` — called each tick for motor control
- `extnDiskAccess()` / `extnSonyAccess()` — extension trap handlers
- `ejectAllDisks()` — ejects all mounted floppies
- The custom Sony driver patched into ROM (see §11) traps to `extnBlockBase`
  to communicate with `SonyDevice`.

For Plus boot: The Sony driver is **essential**. The ROM's disk driver is
replaced with the patched version that intercepts disk I/O and redirects
it to the emulator's host file system.

---

## 13. SCSI

Source: `src/devices/scsi.h`, `src/devices/scsi.cpp`

The Mac Plus was the first Mac to include a SCSI port (NCR 5380 controller).

### Memory Map

SCSI is mapped at `0x580000` with a 512 KB address space (`ln2Spc = 19`) for
compact Macs.

### Device Interface

```cpp
class SCSIDevice : public Device {
    uint32_t access(uint32_t data, bool writeMem, uint32_t addr) override;
    void reset() override;
};
```

The SCSI device handles register reads/writes from the emulated 68000.
Hard disk images are accessed through extension traps similar to the Sony
floppy mechanism.

---

## 14. Address Space Map (Plus / Compact Mac)

Source: `src/core/machine.cpp`

| Address Range           | Device / Region           |
|-------------------------|---------------------------|
| `0x000000`–`0x3FFFFF`  | RAM (4 MB, overlay off)   |
| `0x400000`–`0x41FFFF`  | ROM (128 KB, mirrored)    |
| `0x580000`–`0x5FFFFF`  | SCSI                      |
| `0x600000`–`0x7FFFFF`  | RAM (overlay on only)     |
| `0x800000`–`0x9FFFFF`  | SCC Read                  |
| `0xA00000`–`0xBFFFFF`  | SCC Write                 |
| `0xC00000`–`0xDFFFFF`  | IWM (floppy)              |
| `0xE80000`–`0xEFFFFF`  | VIA1                      |
| `0xF0C000`              | Extension block (traps)   |

---

## 15. Wire Bus Summary (Plus-Relevant Wires)

Source: `src/core/wire_ids.h`, `src/core/wire_bus.h`

The wire bus is a lightweight publish-subscribe system. Devices set wires
via `g_wires.set(id, value)` and register change callbacks via
`g_wires.onChange(id, callback)`.

| Wire ID                        | Direction  | Producer          | Consumer(s)                 |
|--------------------------------|------------|-------------------|-----------------------------|
| `Wire_SoundDisable`            | VIA1 → Snd | VIA1 port B[7]   | SoundDevice::subTick        |
| `Wire_SoundVolb0/1/2`         | VIA1 → Snd | VIA1 port A[0-2] | SoundDevice::subTick        |
| `Wire_VIA1_iA4` (MemOverlay)  | VIA1 → Mem | VIA1 port A[4]   | MemOverlay_ChangeNtfy       |
| `Wire_VIA1_iA5` (IWMvSel)     | VIA1 → IWM | VIA1 port A[5]   | IWM device                  |
| `Wire_VIA1_iA7` (SCCwaitrq)   | SCC → VIA1 | SCC               | VIA1 port A read            |
| `Wire_VIA1_iB0` (RTCdataLine) | Bidir      | VIA1/RTC          | RTCDevice::dataLineChangeNtfy |
| `Wire_VIA1_iB1` (RTCclock)    | VIA1 → RTC | VIA1 port B[1]   | RTCDevice::clockChangeNtfy  |
| `Wire_VIA1_iB2` (RTCunEnabled)| VIA1 → RTC | VIA1 port B[2]   | RTCDevice::unEnabledChangeNtfy |
| `Wire_VIA1_iB3` (MouseBtnUp)  | Mouse → VIA| MouseDevice       | VIA1 port B read            |
| `Wire_VIA1_iCB2` (KbdData)    | Bidir      | VIA1/Keyboard     | KeyboardDevice::dataLineChngNtfy |
| `Wire_VIA1_InterruptRequest`   | VIA1 → CPU | VIA1              | VIAorSCCinterruptChngNtfy   |
| `Wire_SCCInterruptRequest`     | SCC → CPU  | SCC               | VIAorSCCinterruptChngNtfy   |

---

## 16. ICT Scheduler Tasks (Plus)

Source: `src/core/main.cpp` — `InitEmulation()`

| ICT Task                      | Handler                          | Registered When      |
|-------------------------------|----------------------------------|----------------------|
| `kICT_SubTick`                | `SubTickTaskDo()`                | Always               |
| `kICT_Kybd_ReceiveCommand`    | `KeyboardDevice::receiveCommand()` | `emClassicKbrd`    |
| `kICT_Kybd_ReceiveEndCommand` | `KeyboardDevice::receiveEndCommand()` | `emClassicKbrd` |
| `kICT_VIA1_Timer1Check`       | `VIA1Device::doTimer1Check()`    | `emVIA1`             |
| `kICT_VIA1_Timer2Check`       | `VIA1Device::doTimer2Check()`    | `emVIA1`             |

Tasks **not** registered on Plus (no VIA2, no ADB, no PMU):
`kICT_VIA2_Timer1Check`, `kICT_VIA2_Timer2Check`, `kICT_ADB_NewState`, `kICT_PMU_Task`.

---

## 17. Boot Sequence Summary

1. CPU reset: reads vectors from address 0 (ROM overlaid at 0x000000)
2. ROM checksum (skipped by patch)
3. RAM test (shortened by patch)
4. RTC initialized via VIA1 port B serial protocol
5. SCC initialized → `SCC.MIE` set → `Mouse_Enabled()` becomes true
6. ROM clears `MemOverlay` (VIA1 port A bit 4) → RAM appears at 0x000000
7. Sony driver loaded (patched) → disk I/O through extension traps
8. VIA1 CA1 pulse each tick → VBL interrupt drives the 60 Hz display refresh
9. Keyboard polling via VIA1 shift register / CB2 protocol
10. Finder / System loaded from disk image

---

## 18. Known Issues and Debugging History

### Fixed Issues (commit 18e320a)

| Issue | Root Cause | Fix |
|-------|-----------|-----|
| Mouse permanently disabled on Plus | `Mouse_Enabled()` used `!ADBMouseDisabled` for all models; should use `SCC.MIE` for Plus | Runtime function in `mouse.cpp` dispatching on `emClassicKbrd` |
| Mouse button clicks ignored | VIA1 `orbCanIn=0x00` — bit 3 (MouseBtnUp) never read from wires | Fixed to `0x79` |
| Keyboard hangs at boot | CB2 onChange callback only registered for ADB; Plus keyboard never notified | Added `emClassicKbrd` CB2 callback for `KeyboardDevice` |
| No startup sound | VIA1 port B[7] wrote to `Wire_VIA1_iB7` but sound.cpp reads `Wire_SoundDisable` | `portBWires[7] = Wire_SoundDisable`, `portAWires[0-2] = Wire_SoundVolb0/1/2` |
| VIA1 IER behavior wrong | `ierNever0=0x00, ierNever1=0x00` instead of `0x02, 0x18` | Fixed to match original SPOTHRCF.i values |
| Wire defaults wrong | VBLintunenbl, VBLinterrupt, SoundDisable forced to 0 | Reverted to original `fill(1)` + only interrupt request overrides |

### Outstanding Issues

#### Plus crashes (bomb dialog) when reaching the Finder

The Plus boots, shows the "insert disk" screen with a working cursor,
accepts the 608.hfs SCSI disk, but crashes with a Mac bomb dialog when
the Finder is loading.

**Possible causes to investigate:**

1. **CPU instruction issue (68000 mode)**: The CPU dispatch table fixup
   (step 5.5) demotes 68020-only instructions to illegal. If a 68000
   instruction is incorrectly demoted, the Plus ROM or System could hit
   an unexpected illegal instruction exception.

2. **SCSI data corruption**: The Finder reads from SCSI during launch.
   If the SCSI emulation has a data transfer bug, it could corrupt the
   Finder code or resources in memory, leading to a crash.

3. **VIA1 timer inaccuracy**: T1 and T2 timers drive sound timing and
   potentially other system services. If timer interrupts fire at the
   wrong rate, timing-sensitive code could fail.

4. **ROM patch side effects**: The emulator patches the ROM for Sony
   driver and RAM test shortcuts. If a patch corrupts adjacent code,
   it could manifest as a crash during Finder load.

5. **Memory mapping issue**: If the ATT (address translation table) has
   a hole or an incorrect mapping in the 24-bit address space, reading
   from certain addresses during Finder load could return wrong data.

6. **Stack or heap corruption**: If the interrupt handler or a device
   access routine corrupts the application stack or the System heap,
   the crash would appear when the corrupted data is first used (which
   could be during Finder initialization).

**Debugging strategy:**

- Add a 68000 exception vector logger that captures the exception type,
  PC, SR, and stack frame when a bomb-dialog exception fires (vectors
  2–11: bus error, address error, illegal instruction, etc.)
- Compare memory layout with a known-working minivmac build
- Run the original upstream minivmac 37.03 compiled for Plus to verify
  the ROM and disk image work correctly
