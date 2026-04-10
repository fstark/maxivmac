# VIDEO — Framebuffer Formats and Depth Conversion Pipeline

Status: investigation / pre-design

## 1. Overview

Every emulated Mac has a framebuffer at some address in the emulated
address space.  The emulator reads that framebuffer once per tick,
detects changes, converts it into a host ARGB8888 buffer, and uploads
it to a GL texture for display.

This document covers the entire pipeline from emulated pixels to the
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
by a separately-allocated VRAM block.  The screen hack patches dozens
of ROM offsets to set row bytes, bounds, cursor math, icon positions,
etc. — see `src/devices/screen_hack.h`.

### 2.2 SE family (SE, SEFDHD, Classic)

Same as compact Macs but using the SE/Classic ROM.  Screen hack patches
are at different ROM offsets.  Always 1 bpp.

### 2.3 Mac II family (MacII, MacIIx)

* **Location:** dedicated VRAM at `g_vidMem`, mapped by the NuBus
  address decoder
* **Default:** 640×480, 8 bpp (256-colour CLUT)
* **VRAM size:** 512 KB hard-coded in model config
* **Colour:** depths 0–5 (1/2/4/8/16/32 bpp) configurable via `--screen`
* **NuBus slot ROM:** built at runtime by `VideoDevice::init()` with
  VPBlock parameters for each active mode
* **PRAM gate:** `PRAM[0x48]` tells the Mac ROM whether the card
  supports colour (0x81) or mono only (0x80)

### 2.4 PowerBook 100

* **Location:** dedicated VRAM at `g_vidMem`
* **Address:** `0x00FA0000`
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
vMacScreenNumBytes   = NumBits / 8          (total FB size)
vMacScreenByteWidth  = (Width << Depth) / 8 (bytes per scanline)
vMacScreenMonoByteWidth = Width / 8         (B&W scanline width)
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
│    • memcmp shadow buffer (screenCompareBuff) vs live FB     │
│    • bufSize = Height × ByteWidth (colour) or MonoByteWidth │
│    • if changed: memcpy snapshot, set g_screenChanged        │
│    • also triggers on g_colorMappingChanged (palette change) │
└──────────────────┬───────────────────────────────────────────┘
                   │  g_screenChanged = true
                   ▼
┌──────────────────────────────────────────────────────────────┐
│  EmulatorShell::drawChangesAndClear()  [emulator_shell.cpp]  │
│    • calls convertFramebuffer(0, 0, H, W)                   │
│    • sets framebufferDirty_ = true                           │
└──────────────────┬───────────────────────────────────────────┘
                   ▼
┌──────────────────────────────────────────────────────────────┐
│  convertFramebuffer()                                        │
│    bpp = 4 (always ARGB8888 destination)                     │
│                                                              │
│    if depth ≤ 3 or B&W mode:                                 │
│      BuildClutTable(4)  →  populate CLUT_final               │
│      ConvertRect(4, …)  →  dispatch to ScreenMapConvert<>    │
│    else (depth 4–5, direct colour):                          │
│      ConvertRectSlow()  →  per-pixel fallback                │
└──────────────────┬───────────────────────────────────────────┘
                   ▼
┌──────────────────────────────────────────────────────────────┐
│  argbBuffer_ (Width × Height × 4 bytes, ARGB8888)            │
└──────────────────┬───────────────────────────────────────────┘
                   ▼
┌──────────────────────────────────────────────────────────────┐
│  ImGuiBackend::uploadFramebuffer()     [imgui_backend.cpp]   │
│    glTexSubImage2D(GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, …) │
│    → GL texture → ImGui image → rendered via OpenGL          │
│                                                              │
│  HeadlessBackend: no upload, conversion still runs for       │
│  dirty-tracking / regression tests                           │
└──────────────────────────────────────────────────────────────┘
```

## 5. Conversion paths

### 5.1 CLUT table building — `BuildClutTable(bpp)`

Pre-expands every possible source byte value into destination pixels.

For each of the 256 possible byte values, and for each pixel packed in
that byte (`PixPerByte = 8 >> depth`):

* **Colour mode (depth 1–3):** extract pixel index, look up
  `CLUT_reds/greens/blues[index]`, pack as ARGB8888
* **B&W mode (depth 0):** extract bit, map 0→white, 1→black

Output: `CLUT_final` — flat array of `256 × PixPerByte × bpp` bytes.
Maximum: `CLUT_FINAL_SZ = 256 × 8 × 4 = 8192` bytes.

### 5.2 Fast path — `ScreenMapConvert<SrcDepth, DstDepth, Scale>`

C++23 function template in `screen_map.h`.  Constraints:

* `SrcDepth ∈ {0,1,2,3}` — indexed / mono only
* `DstDepth ∈ {3,4,5}` — 8/16/32-bit destination
* `DstDepth ≥ SrcDepth`
* `Scale ≥ 1` (default 1; 2 = pixel-double)

Derived constants (all compile-time):

```
MapElSz   = Scale << (DstDepth - SrcDepth)   // dest bytes per src byte
TranLn2Sz = log₂ of widest aligned unit in MapElSz
TranN     = MapElSz >> TranLn2Sz              // number of transfer units
TranT     = uint32_t / uint16_t / uint8_t     // transfer type
```

Inner loop per source scanline: read byte → index into `mapT[]` →
copy `TranN` units to destination.  Scaling duplicates each row
`Scale - 1` additional times.

**Instantiations used (bpp=4, Scale=1):**

| Mode | SrcDepth | DstDepth | Source format |
|---|:---:|:---:|---|
| B&W | 0 | 5 | 1 bpp mono → 32-bit ARGB |
| 4-col | 1 | 5 | 2 bpp CLUT → 32-bit ARGB |
| 16-col | 2 | 5 | 4 bpp CLUT → 32-bit ARGB |
| 256-col | 3 | 5 | 8 bpp CLUT → 32-bit ARGB |

(DstDepth=3 and DstDepth=4 instantiations exist but are dead code when
`bpp` is always 4.)

### 5.3 Slow path — `ConvertRectSlow()`

Per-pixel loop for depths 4 and 5 (16/32 bpp direct colour).  No CLUT
table; reads RGB directly from the framebuffer:

* **Depth 4 (16 bpp):** reads big-endian 5-5-5 RGB, expands to 8-8-8
* **Depth 5 (32 bpp):** reads xRGB bytes, sets alpha to 0xFF

Writes directly to `argbBuffer_` using pitch and bpp.

### 5.4 Reference implementation (minivmac)

The original minivmac used `SCRNMAPR.h`, a C macro template `#include`d
multiple times with different `#define` parameters.  In `OSGLUSDL.c`
there were 12 instantiations:

| Name | SrcDepth | DstDepth | Scale |
|---|:---:|:---:|:---:|
| UpdateBWDepth3Copy | 0 | 3 | 1 |
| UpdateBWDepth4Copy | 0 | 4 | 1 |
| UpdateBWDepth5Copy | 0 | 5 | 1 |
| UpdateBWDepth3ScaledCopy | 0 | 3 | 2 |
| UpdateBWDepth4ScaledCopy | 0 | 4 | 2 |
| UpdateBWDepth5ScaledCopy | 0 | 5 | 2 |
| UpdateColorDepth3Copy | D | 3 | 1 |
| UpdateColorDepth4Copy | D | 4 | 1 |
| UpdateColorDepth5Copy | D | 5 | 1 |
| UpdateColorDepth3ScaledCopy | D | 3 | 2 |
| UpdateColorDepth4ScaledCopy | D | 4 | 2 |
| UpdateColorDepth5ScaledCopy | D | 5 | 2 |

Where D = `vMacScreenDepth` (compile-time constant in minivmac).

The maxivmac C++ template replaces all 12 with a single function
template, with `vMacScreenDepth` now a runtime value dispatched through
a `switch`.

## 6. Buffer inventory

| Buffer | Type | Size | Owner | Lifetime |
|---|---|---|---|---|
| `screenCompareBuff` | Shadow of Mac FB | `vMacScreenNumBytes` | DisplayState | init→shutdown |
| `CLUT_final` | Expanded CLUT | 8192 bytes | DisplayState | init→shutdown |
| `argbBuffer_` | Host ARGB8888 | W × H × 4 | EmulatorShell | init→shutdown |
| `scalingBuff` | Alias to argbBuffer_ | — | DisplayState (ptr) | per-frame |

`screenCompareBuff` is initialised to 0xFF so the entire screen is
considered dirty on the first tick.

## 7. Mac II video card emulation

The Mac II video card (`VideoDevice`) builds a NuBus declaration ROM
containing:

* Board resource (vendor "Paul C. Pratt", board "Mini vMac video card")
* **Mode 0x80** — 1 bpp monochrome (always present)
* **Mode 0x81** — colour at configured depth (only when depth 1–3 and
  `g_colorModeWorks`)

### 7.1 Video driver traps

The driver handles Mac OS control/status calls:

| csCode | Name | Implemented? |
|:---:|---|:---:|
| 0 | Reset | ✅ |
| 2 | SetVidMode / GetMode | ✅ |
| 3 | SetEntries / GetEntries | ✅ / stub |
| 4 | GetPages | ✅ |
| 5 | GetPageAddr | ✅ |
| 6 | SetGray / GetGray | ✅ |
| 7 | SetGamma | stub |
| 8 | GetGamma | stub |
| 9 | SetDefaultMode / GetDefaultMode | **not handled** |
| 10 | GetCurrentMode | **not handled** |
| 12 | GetConnection | **not handled** |
| 14 | GetModeBaseAddress | **not handled** |
| 16 | Save/GetPreferredConfiguration | **not handled** |
| 17 | GetNextResolution | **not handled** |
| 18 | GetVideoParameters | **not handled** |

### 7.2 Mode switching

`Vid_SetMode(v)` toggles `g_useColorMode` between mode 128 (B&W) and
129 (colour).  `VideoDevice::vidReset()` always resets to mode 128.

The mode change propagates via `g_colorMappingChanged` → full-screen
CLUT rebuild + redraw on the next tick.

## 8. Host pixel format

`argbBuffer_` is uploaded to GL via:

```c
glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h,
    GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, argbBuffer_);
```

With `GL_BGRA` + `GL_UNSIGNED_INT_8_8_8_8_REV`, the byte order in
memory is **[A, R, G, B]** (little-endian: the "REV" reverses the
uint32 component order so byte 0 = blue in GL terms, but the BGRA
swizzle makes byte 0 = alpha).

The `BuildClutTable` packing `(0xFF << 24) | (r << 16) | (g << 8) | b`
produces the uint32 value `0xFFRRGGBB`, which on a little-endian host
is stored as bytes `[BB, GG, RR, FF]`.  This matches the GL upload
format.

## 9. Known bugs

### B1 — CLUT pixel mask wrong for 2-bpp and 4-bpp

`BuildClutTable()` uses `& (CLUT_size - 1)` (= 255) to extract pixel
indices.  The correct mask is `(1 << (1 << depth)) - 1`: 3 for 2 bpp,
15 for 4 bpp.

**Symptom:** garbled pixel artifacts at `--screen=…x2` and `…x4`.

### B2 — No input validation on `--screen` depth

`parseScreenSpec()` accepts any integer and silently truncates.
`--screen=640x480x24` → depth 4 (same as x16).

### B3 — VRAM too small for depth ≥ 4 on Mac II

`vidMemSize = 0x80000` (512 KB) is hard-coded.  640×480×16 needs
614,400 bytes.

**Symptom:** garbled screen / hang / crash at x16 or x32.

### B4 — PRAM blocks colour for direct depths (16/32 bpp)

`PRAM[0x48] = 0x80` when depth ≥ 4, so the Mac ROM never offers the
Monitors colour option.

### B5 — Stray `w` character in screen_convert.cpp line 91

### B6 — Dead DstDepth=3 and DstDepth=4 instantiations

`convertFramebuffer()` always passes `bpp=4`, making the DstDepth=3 and
DstDepth=4 branches unreachable.

## 10. Limitations

### L1 — Only one colour depth per session

Slot ROM is built once.  Mode-enumeration traps are unimplemented.

### L2 — Direct-colour modes use slow per-pixel path

Depths 4/5 go through `ConvertRectSlow()`.

### L3 — No gamma support

### L4 — PRAM not persisted

Guest Monitors preference survives via the disk image, not PRAM.

### L5 — Scaling is unused

`ScreenMapConvert` supports `Scale > 1` but it is never invoked.  The
reference minivmac used it for 2× magnification.  maxivmac relies on GL
texture scaling instead.

### L6 — Compact Macs are always 1 bpp

The screen hack patches geometry only; `--screen=512x342x8` on a
MacPlus would silently misbehave.

## 11. Architectural critique

### 11.1 Scaling belongs in the GPU, not the converter

The `ScreenMapConvert` template has a `Scale` parameter inherited from
minivmac's `SCRNMAPR.h`.  In minivmac, SDL 1.x had no efficient texture
scaling, so the converter pixel-doubled into a 2× buffer which was then
blitted.  six of the 12 original instantiations were `Scale=2` variants.

In maxivmac, the output goes to a GL texture drawn by ImGui.  GL already
scales the texture to fill the window.  A 512×342 texture scaled via
`GL_NEAREST` produces identical crisp pixels.  The converter's Scale
path is dead code — it doubles the ARGB buffer size, doubles conversion
work, and is never called.

**The correct approach:** `argbBuffer_` should always be at native Mac
resolution (512×342 for compact, 640×480 for Mac II, etc.).  Display
scaling is a presentation concern handled by the GL magnification
filter:

* `GL_NEAREST` — crisp pixel art (correct for emulated CRT)
* `GL_LINEAR` — smooth (current default, avoids Retina moiré)

This eliminates `Scale` from the template, the dead `bpp=1`/`bpp=2`
DstDepth paths, and the `scalingBuff` alias.

### 11.2 The template conflates three things that should be separate

The current `ScreenMapConvert<SrcDepth, DstDepth, Scale>` is structured
around a very specific operation: "read one source byte → look it up in
a pre-expanded CLUT → write N destination units."  This only works for
packed indexed formats (depths 0–3) where one source byte always maps to
a fixed number of output pixels via a 256-entry table.

It cannot handle direct-colour depths (16/32 bpp) because:
* Depth 4 (16 bpp): each pixel spans 2 source bytes → not indexable
  from a single byte
* Depth 5 (32 bpp): each pixel spans 4 source bytes → the "CLUT" would
  need 2³² entries

This forces direct-colour through `ConvertRectSlow()`, a per-pixel
fallback that is structurally a different code path with its own address
calculations, endian handling, and output logic.

**The conversion is really just three steps:**

1. **Read** a source unit (1 byte containing N packed pixels, OR 2/4
   bytes containing one pixel)
2. **Decode** each pixel in that unit into ARGB8888 (via CLUT for
   indexed, or inline conversion for direct)
3. **Write** the ARGB8888 pixels to the destination scanline

Reframed this way, every depth fits the same loop structure:

| Depth | Source unit | Pixels per unit | Decode method |
|:---:|:---:|:---:|---|
| 0 (1 bpp) | 1 byte | 8 | B&W table (2 entries) |
| 1 (2 bpp) | 1 byte | 4 | CLUT[index & 0x03] |
| 2 (4 bpp) | 1 byte | 2 | CLUT[index & 0x0F] |
| 3 (8 bpp) | 1 byte | 1 | CLUT[byte] |
| 4 (16 bpp) | 2 bytes | 1 | 5-5-5 → 8-8-8 expansion |
| 5 (32 bpp) | 4 bytes | 1 | Set alpha to 0xFF |

The CLUT lookup for depths 0–3 and the inline conversion for depths 4–5
are both just "how to turn source bits into ARGB".  The outer scanline
loop, dirty-rect clipping, stride calculation, and destination write are
identical.

A unified template would look something like:

```
for each scanline in [top, bottom):
    pSrc = src + row * srcStride + leftUnit
    pDst = dst + row * dstStride + leftUnit * pixPerUnit * 4
    for each source unit in [leftUnit, rightUnit):
        read srcUnitBytes from pSrc
        for each pixel k in unit:
            argb = decode(srcValue, k)    // CLUT or inline
            write argb to pDst
```

This eliminates:
* The `DstDepth` parameter (destination is always ARGB8888 = 4 bytes)
* The `Scale` parameter (GPU handles this)
* The `TranT` / `TranN` / `MapElSz` machinery (destination is always
  uint32_t)
* The separate `ConvertRectSlow` code path
* The `bpp` parameter plumbed through `BuildClutTable` and `ConvertRect`

The CLUT table itself (`CLUT_final`) also simplifies: instead of
pre-expanding every byte into `PixPerByte × bpp` destination bytes, it
becomes a straight `uint32_t[256]` palette (or `uint32_t[2]` for B&W).
The byte→pixels unpacking moves from `BuildClutTable` into the scanline
loop where it's more readable.

For direct-colour, the "decode" step is:
* Depth 4: `uint16_t rgb555 = big_endian_read(pSrc); expand_555_to_8888(rgb555)`
* Depth 5: `uint32_t xrgb = big_endian_read(pSrc); xrgb | 0xFF000000`

## 12. Possible directions

### D1 — Fix bugs B1–B5

Localised fixes.  No architecture changes.

### D2 — Remove Scale and DstDepth from the converter

The destination is always ARGB8888 (DstDepth=5, bpp=4).  Remove the
`Scale`, `DstDepth`, and `bpp` plumbing.  The `argbBuffer_` stays at
native Mac resolution; GL handles display scaling.

### D3 — Unify the conversion template

Replace `ScreenMapConvert` + `ConvertRectSlow` + `BuildClutTable` with
a single template parameterised on source depth only.  All depths 0–5
use the same scanline loop; the pixel decode is depth-specific:

* Depths 0–3: CLUT lookup (`uint32_t palette[N]`)
* Depth 4: inline 5-5-5 → ARGB8888
* Depth 5: inline xRGB → ARGB8888

This removes the `CLUT_final` pre-expansion buffer entirely (the CLUT
becomes a simple `uint32_t[]` palette) and eliminates the slow-path
fallback.

### D4 — Multi-depth mode enumeration (Mac II)

Implement video driver traps (csCode 9, 16, 17, 18) so the Monitors
control panel can discover multiple indexed depths at runtime.

### D5 — Fix PRAM for direct-colour depths

### D6 — Validate `--screen` against model capabilities

Reject colour depths on compact Macs.  Warn when VRAM would overflow.

### D7 — Persisted PRAM

### D8 — Document / fix byte-order assumptions

Clarify the ARGB8888 naming vs. the actual BGRA byte order.  Ensure the
pipeline works correctly on big-endian hosts (if that's a goal).

## 13. Open questions

1. **Which depths are actually useful?**  The real Mac II TFB card
   supported 1 and 8 bpp only.  Depths 2, 4, 16, 32 are maxivmac
   additions with limited testing.
2. **Is runtime depth switching worth the complexity?**
3. **Should compact Macs support non-1bpp depth?**  The screen hack
   would need colour-aware ROM patching.
4. **Byte-order portability:**  is big-endian host support a goal?
5. **GL_NEAREST vs GL_LINEAR:**  GL_NEAREST gives crisp pixels but the
   existing code uses GL_LINEAR to avoid a moiré artefact on Retina
   displays (see comment in `imgui_backend.cpp`).  Is the moiré
   fixable another way (e.g., integer scaling only)?
