# VIDEO — Framebuffer and Display Pipeline

## 1. Overview

Every emulated Mac has a framebuffer at some address in the emulated
address space.  The emulator reads that framebuffer once per tick,
detects changes, converts it into a host ARGB8888 buffer, and uploads
it to a GL texture for display.

This document describes the current pipeline from emulated pixels to the
host display, across all supported models.

## 2. Framebuffer sources

### 2.1 Compact Macs (128K, 512K, 512Ke, Plus, Kanji)

* **Location:** top of main RAM, at `ramSize - 0x5900`
* **Format:** 1 bpp monochrome, 512×342, MSB-first
* **Row stride:** 64 bytes (512/8)
* **Total size:** 21,888 bytes
* **Colour:** mono only; `vMacScreenDepth` = 0

With the screen hack active (non-default resolution via `--screen`),
`ScrnBase` is patched to point at `kVidMem_Base = 0x00540000`, backed
by a separately-allocated VRAM block.

### 2.2 SE family (SE, SEFDHD, Classic)

Same as compact Macs but using the SE/Classic ROM.  Screen hack patches
are at different ROM offsets.  Always 1 bpp.

### 2.3 Mac II family (MacII, MacIIx)

* **Location:** dedicated VRAM at `g_vidMem`, mapped by the NuBus
  address decoder at `0xF9900000`
* **Default:** 640×480, 8 bpp (256-colour CLUT)
* **VRAM size:** auto-sized to fit the configured depth (minimum 512 KB)
* **Colour:** depths 0–5 (1/2/4/8/16/32 bpp) configurable via `--screen`
* **NuBus slot ROM:** built at runtime by `VideoDevice::init()` with
  mode parameter blocks for mono (0x80) and one colour mode (0x81)
* **PRAM gate:** `PRAM[0x48]` = 0x81 for any non-B&W depth, 0x80 for
  depth 0

### 2.4 PowerBook 100

* **Location:** dedicated VRAM at `g_vidMem`, address `0x00FA0000`
* **Format:** 640×400, 1 bpp
* **VRAM size:** 32 KB

### 2.5 Summary table

| Model family | FB source | Default WxH | Depth | Colour? |
|---|---|:---:|:---:|:---:|
| 128K–Plus | main RAM top | 512×342 | 0 (1 bpp) | no |
| SE–Classic | main RAM top | 512×342 | 0 (1 bpp) | no |
| Mac II/IIx | VRAM (NuBus) | 640×480 | 3 (8 bpp) | yes |
| PB100 | VRAM | 640×400 | 0 (1 bpp) | no |

## 3. Depth encoding

`vMacScreenDepth` is the log₂ of bits-per-pixel, stored in
`DisplayState::screenDepth` and used everywhere via the `vMacScreenDepth`
macro.

| vMacScreenDepth | bpp | Type | Pixels/byte | CLUT entries |
|:---:|:---:|---|:---:|:---:|
| 0 | 1 | Monochrome | 8 | 2 (B&W) |
| 1 | 2 | 4-colour indexed | 4 | 4 |
| 2 | 4 | 16-colour indexed | 2 | 16 |
| 3 | 8 | 256-colour indexed | 1 | 256 |
| 4 | 16 | Direct 5-5-5 RGB | — | — |
| 5 | 32 | Direct xRGB8888 | — | — |

Derived macros (`platform.h`):

```
vMacScreenNumPixels  = Width × Height
vMacScreenNumBits    = NumPixels << Depth
vMacScreenNumBytes   = NumBits / 8
vMacScreenByteWidth  = (Width << Depth) / 8
vMacScreenMonoByteWidth = Width / 8
```

## 4. Frame output flow

```
┌──────────────────────────────────────────────────────────────┐
│  Emulated Mac writes to framebuffer (RAM or VRAM)            │
└──────────────────┬───────────────────────────────────────────┘
                   │  once per tick
                   ▼
┌──────────────────────────────────────────────────────────────┐
│  ScreenDevice::endTickNotify()         [screen.cpp]          │
│    • selects FB pointer (g_vidMem or main RAM)               │
│    • calls Screen_OutputFrame(ptr)                           │
└──────────────────┬───────────────────────────────────────────┘
                   ▼
┌──────────────────────────────────────────────────────────────┐
│  ScreenFindChanges()                   [osglu_common.cpp]    │
│    • memcmp screenCompareBuff vs live FB                     │
│    • if changed: memcpy snapshot, set g_screenChanged        │
│    • also triggers on g_colorMappingChanged                  │
└──────────────────┬───────────────────────────────────────────┘
                   │  g_screenChanged = true
                   ▼
┌──────────────────────────────────────────────────────────────┐
│  EmulatorShell::drawChangesAndClear()  [emulator_shell.cpp]  │
│    • if indexed depth: calls BuildPalette()                  │
│    • calls ConvertScreen()                                   │
│    • sets framebufferDirty_ = true                           │
└──────────────────┬───────────────────────────────────────────┘
                   ▼
┌──────────────────────────────────────────────────────────────┐
│  argbBuffer_ (Width × Height × 4 bytes, ARGB8888)            │
└──────────────────┬───────────────────────────────────────────┘
                   ▼
┌──────────────────────────────────────────────────────────────┐
│  ImGuiBackend::uploadFramebuffer()     [imgui_backend.cpp]   │
│    • applies textureFilter_ (GL_NEAREST or GL_LINEAR)        │
│    • glTexSubImage2D(GL_BGRA, UNSIGNED_INT_8_8_8_8_REV, …)  │
│    → GL texture → ImGui image → rendered via OpenGL          │
│                                                              │
│  HeadlessBackend: no upload, conversion still runs for       │
│  dirty-tracking / regression tests                           │
└──────────────────────────────────────────────────────────────┘
```

## 5. Palette and conversion

### 5.1 Palette — `BuildPalette()`

Builds a flat `uint32_t clut32[256]` palette in `DisplayState` from the
16-bit `clutReds/clutGreens/clutBlues` arrays.

* **B&W mode (depth 0):** `clut32[0] = white`, `clut32[1] = black`
* **Indexed colour (depth 1–3):** packs `(R,G,B) >> 8` into ARGB8888

Called every frame when the screen is dirty and depth < 4.

### 5.2 Conversion — `ConvertScreen()`

Single unified function in `screen_convert.cpp`.  Always converts the
full Mac framebuffer to ARGB8888.  No rect parameters, no bpp parameter,
no scaling.

```cpp
void ConvertScreen(
    const uint8_t* src, uint32_t* dst,
    const uint32_t* palette, int depth,
    int width, int height);
```

Implementation uses a template-on-depth dispatcher for indexed colour
(`ConvertScreenIndexed<Depth>`) and direct functions for 16/32 bpp.

| Depth | Method | Source unit | Decode |
|:---:|---|:---:|---|
| 0 | `ConvertScreenIndexed<0>` | 1 byte → 8 px | `palette[bit]` |
| 1 | `ConvertScreenIndexed<1>` | 1 byte → 4 px | `palette[2-bit index]` |
| 2 | `ConvertScreenIndexed<2>` | 1 byte → 2 px | `palette[4-bit index]` |
| 3 | `ConvertScreenIndexed<3>` | 1 byte → 1 px | `palette[byte]` |
| 4 | `ConvertScreenDepth4` | 2 bytes → 1 px | 5-5-5 → 8-8-8 expansion |
| 5 | `ConvertScreenDepth5` | 4 bytes → 1 px | set alpha to 0xFF |

### 5.3 GL texture filter

The `ImGuiBackend` has a runtime-switchable `TextureFilter` (Nearest or
Linear), toggled from the Display tab of the Ctrl overlay:

* **GL_NEAREST** — crisp pixel art, integer-scale look
* **GL_LINEAR** — smooth, avoids moiré on Retina at non-integer scales

Default is Linear.

## 6. Buffer inventory

| Buffer | Type | Size | Owner | Lifetime |
|---|---|---|---|---|
| `screenCompareBuff` | Shadow of Mac FB | `vMacScreenNumBytes` | DisplayState | init→shutdown |
| `clut32[256]` | ARGB8888 palette | 1024 bytes | DisplayState | init→shutdown |
| `argbBuffer_` | Host ARGB8888 | W × H × 4 | EmulatorShell | init→shutdown |

`screenCompareBuff` is initialised to 0xFF so the entire screen is
dirty on the first tick.

## 7. Mac II video card emulation

The Mac II video card (`VideoDevice`) builds a NuBus declaration ROM
containing:

* Board resource (vendor "Paul C. Pratt", board "Mini vMac video card")
* **Mode 0x80** — 1 bpp monochrome (always present)
* **Mode 0x81** — colour at the single configured depth (only when
  depth > 0 and `g_colorModeWorks`)

The slot ROM is built once at `VideoDevice::init()` and checksummed.
The mode parameter blocks (VPBlock) describe resolution, row bytes,
pixel size, component count/size, and device type (CLUT vs direct).

### 7.1 Video driver traps (current state)

| csCode | Name | Status |
|:---:|---|:---:|
| 0 | VidReset | ✅ returns mode 128 |
| 1 | KillIO | ✅ |
| 2 | SetVidMode / GetMode | ✅ binary toggle (128 ↔ 129) |
| 3 | SetEntries / GetEntries | ✅ / stub |
| 4 | GetPages | ✅ returns 1 |
| 5 | GetPageAddr | ✅ returns VidBaseAddr |
| 6 | SetGray / GetGray | ✅ |
| 7, 8 | SetGamma / GetGamma | stubs |
| 9 | SetDefaultMode | **stub** (logged, no-op) |
| 10 | GetCurrentMode | **not handled** |
| 12 | GetConnection | **not handled** |
| 14 | GetModeBaseAddress | **not handled** |
| 16 | SavePreferredConfiguration | **stub** (logged, no-op) |
| 17 | GetNextResolution | **not handled** |
| 18 | GetVideoParameters | **not handled** |

### 7.2 Mode switching

`Vid_SetMode(v)` toggles `g_useColorMode` between mode 128 (B&W) and
129 (colour at the configured depth).  The mode change propagates via
`g_colorMappingChanged` → palette rebuild + full-screen redraw.

There is **no runtime depth switching**.  The slot ROM contains exactly
two modes, and the mode-enumeration traps (csCode 9, 10, 17, 18) are
absent or stubbed.  The Monitors control panel cannot discover
additional depths.

### 7.3 VRAM mapping

```
NuBus slot 9:
  0xF9F00000  Slot declaration ROM  (vidROMSize bytes)
  0xF9900000  Video RAM             (vidMemSize, mirrored to 1 MB window)
  0xF9A00000  VRAM bank 2           (if vidMemSize ≥ 2 MB)
  0xF9B00000  VRAM bank 3           (if vidMemSize ≥ 3 MB)
```

### 7.4 VBL interrupt flow

On compact Macs (Plus, SE), the 60 Hz VBL heartbeat comes directly
from VIA1 CA1.  On the Mac II, it comes from the **video card** via a
NuBus slot interrupt.  The guest video driver is responsible for
acknowledging the interrupt and calling the system VBL task manager.

The full chain:

```
Host tick (60 Hz)
  → VideoDevice::update()
    → sets Wire_VBLinterrupt = 0 (active-low)
    → pulses VIA2 CA1  (NuBus slot interrupt, IRQ level 2)
      → 68020 takes interrupt
      → Slot Manager walks slot interrupt queue
      → calls our driver's BeginIH handler
        → host extension trap: kCmndVideoClearInt
          → sets Wire_VBLinterrupt = 1 (acknowledged)
        → extracts slot # from DCE
        → JSR (JVBLTask)  ← runs all VBL tasks for this slot
      → RTS with D0 = 1 (interrupt serviced)
```

The guest driver (source: `extras/mydriver/video/firmware.a`) is a thin
shim.  Its interrupt handler does exactly two things:

1. **Acknowledge** the interrupt by calling the host via extension trap
   (`kCmndVideoClearInt`), which de-asserts the VIA2 IRQ line.
2. **Dispatch VBL tasks** by calling `JVBLTask` (low-memory global
   `$0D28`) with the slot number in D0.  This is how the Mac II runs
   cursor blinking, `Delay()`, animation, and all time-based tasks.

This design seems backwards — the host generates the interrupt, the
guest acknowledges it, then calls the system VBL handler — but it
mirrors real Mac II hardware, where the video card's vertical retrace
interrupt was the only source of the 60 Hz heartbeat.  Without this
chain, VBL tasks would not run and the system would freeze.

The driver's **Open** allocates 4 bytes of private storage (just to
save the slot queue element pointer for later removal) and installs
the interrupt handler via `_SIntInstall`.  **Close** removes it.
**Control** and **Status** simply forward the parameter block to the
host extension trap without interpreting csCode values — making the
binary mode-agnostic.

## 8. Host pixel format

`argbBuffer_` is uploaded via:

```c
glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h,
    GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, argbBuffer_);
```

The packing `0xFF000000 | (r << 16) | (g << 8) | b` produces uint32
`0xFFRRGGBB`.  On little-endian this is bytes `[BB, GG, RR, FF]`,
matching `GL_BGRA + UNSIGNED_INT_8_8_8_8_REV`.

## 9. Limitations

### L1 — Only one colour depth per session

The slot ROM is built once with a single colour mode.  Mode-enumeration
traps are unimplemented.  The Monitors control panel sees only 1 bpp
and one colour depth (the one passed at launch).

### L2 — Direct-colour modes (16/32 bpp) crash

Depths 4 and 5 can be configured and the converter handles them
correctly, but the guest OS crashes during startup.  The likely cause
is incomplete video driver emulation: the ROM's display init code may
call unimplemented status traps (GetCurrentMode, GetVideoParameters)
or expect mode-enumeration to succeed.

### L3 — No gamma support

### L4 — PRAM not persisted

Guest Monitors preference survives via the disk image, not PRAM.

### L5 — Compact Macs are always 1 bpp

The screen hack patches geometry only; `--screen=512x342x8` on a
MacPlus would silently misbehave.

## 10. Future: multi-depth NuBus card

The correct fix for L1 and L2 is to implement a proper multi-mode
video card:

1. Build the slot ROM with mode entries for all supported depths
   (0x80–0x85 for 1/2/4/8/16/32 bpp)
2. Implement mode-enumeration traps (csCode 10, 17, 18) so the
   Monitors CP can discover depths
3. Implement `SetVidMode` with actual depth switching (update
   `vMacScreenDepth` at runtime, resize/reallocate ARGB buffer)
4. Direct-colour would then work because the guest OS properly
   negotiates the mode instead of being forced via PRAM

See `VIDEO_PLAN.md` for the implementation plan.
