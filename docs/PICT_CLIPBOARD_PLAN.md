# PICT Clipboard — Implementation Plan

Design: [PICT_CLIPBOARD_DESIGN.md](PICT_CLIPBOARD_DESIGN.md)
Spec: [features/CLIPBOARD.md](features/CLIPBOARD.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Host-side 1-bit pixel extraction + alpha compositing + PNG encode + unit tests | |
| 2 | Host-side PictExport command handler + dispatch wiring | |
| 3 | Guest-side PICT export (1-bit compact Mac path) | |
| 4 | Human test gate: 1-bit guest→host (MacPaint → host paste) | |
| 5 | Host-side 32-bit pixel extraction + alpha compositing + unit tests | |
| 6 | Guest-side PICT export (32-bit GWorld path + CQD detection) | |
| 7 | Human test gate: 32-bit guest→host (Mac II color app → host paste) | |
| 8 | Host-side image query + import (PictHasImage + PictImport) + unit tests | |
| 9 | Unified ClipSeqNo (bump on text OR image) | |
| 10 | Guest-side PICT import (host→guest) | |
| 11 | Human test gate: host→guest (host copy → paste in Mac app) | |
| 12 | Version bump to 3, documentation update | |

Build gate: `cmake --build bld/macos`
Test gate: `bld/macos/tests`

---

## Phase 1 — 1-bit pixel extraction + alpha compositing + PNG encode

Self-contained host-side module.  Pure functions, no guest RAM access,
no SDL.  Fully testable.

### 1.1 — Create `src/core/pict_convert.h`

Public interface for pixel extraction and compositing.  These are free
functions (PascalCase per NAMING.md).

```cpp
/*
	pict_convert.h — Convert raw Mac pixel data to RGBA for PNG encoding.

	Two-pass alpha compositing: render on white, render on black.
	Pixels that differ between passes are transparent.
*/

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

/*
	Extract RGBA pixels from two 1-bit Mac BitMap buffers (white-bg
	and black-bg passes).

	Mac 1-bit format: packed MSB-first, 1 = black (ink), 0 = white.
	Each row is `rowBytes` bytes wide (padded to word boundary).

	Output: width * height * 4 bytes (R, G, B, A) top-to-bottom,
	left-to-right.
*/
std::vector<uint8_t> Composite1Bit(
	const uint8_t *whitePass, const uint8_t *blackPass,
	int width, int height, int rowBytes);

/*
	Extract RGBA pixels from two 32-bit Mac PixMap buffers (white-bg
	and black-bg passes).

	Mac 32-bit PixMap format: 4 bytes per pixel [X][R][G][B],
	big-endian.  Each row is `rowBytes` bytes wide.

	Output: width * height * 4 bytes (R, G, B, A).
*/
std::vector<uint8_t> Composite32Bit(
	const uint8_t *whitePass, const uint8_t *blackPass,
	int width, int height, int rowBytes);

/*
	Encode RGBA pixel data as a PNG blob in memory.

	Returns empty vector on failure.
*/
std::vector<uint8_t> EncodeRGBAPng(
	const uint8_t *rgba, int width, int height);

/*
	Convert RGBA pixels to 1-bit Mac BitMap format.
	Thresholds at 50% luminance.  Packs MSB-first.
	Returns: rowBytes * height bytes.
	Sets outRowBytes to the row stride used.
*/
std::vector<uint8_t> RGBATo1Bit(
	const uint8_t *rgba, int width, int height, int &outRowBytes);

/*
	Convert RGBA pixels to 32-bit Mac PixMap format (XRGB).
	Returns: rowBytes * height bytes.
	Sets outRowBytes to the row stride used (width * 4, rounded up
	to even).
*/
std::vector<uint8_t> RGBATo32Bit(
	const uint8_t *rgba, int width, int height, int &outRowBytes);
```

### 1.2 — Create `src/core/pict_convert.cpp`

Implementation of the compositing and conversion functions.

#### `Composite1Bit` algorithm

```
for each row y in [0, height):
    rowOff = y * rowBytes
    for each pixel x in [0, width):
        byteIdx = rowOff + (x / 8)
        bitMask = 0x80 >> (x % 8)

        wBit = (whitePass[byteIdx] & bitMask) != 0   // 1 = black ink
        bBit = (blackPass[byteIdx] & bitMask) != 0

        outIdx = (y * width + x) * 4

        if wBit and bBit:
            // Both passes drew black ink → opaque black
            rgba[outIdx+0] = 0;   rgba[outIdx+1] = 0;
            rgba[outIdx+2] = 0;   rgba[outIdx+3] = 255;
        else if !wBit and !bBit:
            // Both passes show white → opaque white
            rgba[outIdx+0] = 255; rgba[outIdx+1] = 255;
            rgba[outIdx+2] = 255; rgba[outIdx+3] = 255;
        else if !wBit and bBit:
            // White on white-pass, black on black-pass → transparent
            // (nothing was drawn; we see the background through)
            rgba[outIdx+0] = 0;   rgba[outIdx+1] = 0;
            rgba[outIdx+2] = 0;   rgba[outIdx+3] = 0;
        else:
            // wBit=1, bBit=0: shouldn't happen with normal QD drawing.
            // Treat as opaque black (conservative).
            rgba[outIdx+0] = 0;   rgba[outIdx+1] = 0;
            rgba[outIdx+2] = 0;   rgba[outIdx+3] = 255;
```

Note on Mac 1-bit convention:
- Bit = 1 → black (ink/foreground)
- Bit = 0 → white (paper/background)
- Packed MSB-first: bit 7 of byte 0 is the leftmost pixel.
This matches Inside Macintosh Volume I, chapter on BitMap.

#### `EncodeRGBAPng`

```cpp
std::vector<uint8_t> EncodeRGBAPng(const uint8_t *rgba, int width, int height)
{
    int pngLen = 0;
    // stbi_write_png_to_mem: stride = width * 4, components = 4
    uint8_t *png = stbi_write_png_to_mem(
        rgba, width * 4, width, height, 4, &pngLen);
    if (!png) return {};
    std::vector<uint8_t> result(png, png + pngLen);
    STBIW_FREE(png);
    return result;
}
```

#### `RGBATo1Bit` algorithm

```
outRowBytes = ((width + 15) / 16) * 2    // word-aligned
for each row y:
    for each pixel x:
        lum = (rgba[R] * 77 + rgba[G] * 150 + rgba[B] * 29) >> 8
        if lum < 128:  set bit (= black ink)
        else:          clear bit (= white)
    pack MSB-first into output bytes
```

#### `RGBATo32Bit` algorithm

```
outRowBytes = width * 4   // already even since 4 bytes/pixel
for each pixel:
    output[0] = 0x00      // X (unused byte)
    output[1] = rgba[R]
    output[2] = rgba[G]
    output[3] = rgba[B]
    // Alpha is discarded (Mac PixMap has no alpha)
```

### 1.3 — Create `test/test_clip_pict.cpp`

Unit tests using doctest.  Test the compositing and conversion
functions in isolation — these don't need guest RAM or SDL.

```cpp
#include <doctest/doctest.h>
#include "core/pict_convert.h"
```

Test cases:

| Test Case | Description |
|-----------|-------------|
| `Composite1Bit opaque black` | Both passes have bit=1 → RGBA (0,0,0,255) |
| `Composite1Bit opaque white` | Both passes have bit=0 → RGBA (255,255,255,255) |
| `Composite1Bit transparent` | White-pass bit=0, black-pass bit=1 → RGBA (0,0,0,0) |
| `Composite1Bit mixed row` | Row with black, white, transparent pixels → verify all |
| `Composite1Bit multi-row` | 3×2 bitmap, verify row stride handling |
| `Composite1Bit row padding` | Width=9 (rowBytes=2), verify padding bits ignored |
| `EncodeRGBAPng roundtrip` | Encode small RGBA image → decode with stb_image → compare |
| `RGBATo1Bit threshold` | Black pixel → bit=1, white pixel → bit=0, gray boundary |
| `RGBATo1Bit row alignment` | Width=9 → rowBytes=2, verify padding zero |
| `RGBATo32Bit layout` | Verify [X][R][G][B] byte order |
| `RGBATo32Bit alpha discard` | Input has alpha, output X byte is 0 |

For the PNG roundtrip test, `stb_image.h` decode is needed in the
test binary.  Add a `test/test_stb_image_impl.cpp` that defines
`STB_IMAGE_IMPLEMENTATION` (separate from the main build's
`stb_impl.cpp`, which only has the write implementation).

### 1.4 — Build integration

**CMakeLists.txt** changes:

Add to `MINIVMAC_SOURCES` (after `src/core/extn_clip.cpp`, line 51):
```cmake
    src/core/pict_convert.cpp
```

Add to `tests` executable (after the existing source list):
```cmake
    test/test_clip_pict.cpp
    src/core/pict_convert.cpp
    test/test_stb_image_impl.cpp
```

Add include path for stb in tests (already set for main target):
```cmake
target_include_directories(tests PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/libs"
    "${CMAKE_SOURCE_DIR}/libs/stb"     # ← add if not present
    "${CMAKE_SOURCE_DIR}/tools/icon_extract"
)
```

The `pict_convert.cpp` file needs `stb_image_write.h` for
`stbi_write_png_to_mem`.  It includes the header only (not the
implementation — that's in `stb_impl.cpp`).

### Fence

- [ ] `src/core/pict_convert.h` exists with 5 function declarations
- [ ] `src/core/pict_convert.cpp` exists with implementations
- [ ] `test/test_clip_pict.cpp` exists with ≥10 test cases
- [ ] `test/test_stb_image_impl.cpp` exists (STB_IMAGE_IMPLEMENTATION)
- [ ] `cmake --build bld/macos` clean
- [ ] `bld/macos/tests --test-case="*1Bit*"` passes
- [ ] `bld/macos/tests --test-case="*PNG*"` passes
- [ ] `bld/macos/tests --test-case="*32Bit*"` passes (RGBATo32Bit)
- [ ] Commit: `"clipboard: phase 1 — 1-bit compositing + PNG encode"`

---

## Phase 2 — PictExport command handler + dispatch wiring

Connect the pixel extraction to the register I/O dispatch.
The handler reads BitMap/PixMap structs from guest RAM, extracts
pixels, and calls the compositing functions from Phase 1.

### 2.1 — Create `src/core/extn_clip_pict.h`

```cpp
/*
	extn_clip_pict.h — PICT clipboard command handlers.

	Called from ExtnClipDispatch() for commands $109–$10B.
	Implements guest↔host image transfer via the register block.
*/

#pragma once

#include <cstdint>

/* Guest → Host: receive rendered pixels from Mac-side DrawPicture. */
void HandlePictExport(uint32_t regParam[], uint16_t &regResult);

/* Host → Guest: report whether host clipboard has an image. */
void HandlePictHasImage(uint32_t regParam[], uint16_t &regResult);

/* Host → Guest: write decoded pixels into guest-allocated buffer. */
void HandlePictImport(uint32_t regParam[], uint16_t &regResult);

/* Reset PICT clipboard state on guest reboot. */
void ExtnPictReset();
```

### 2.2 — Create `src/core/extn_clip_pict.cpp`

Implements `HandlePictExport` only in this phase.  The other two
handlers are stubs (return error) until Phase 8.

#### State variables

```cpp
/* Pixel data from the white-background pass, held until the
   black-background pass arrives. */
static std::vector<uint8_t> s_passWhite;

/* Metadata from the first pass. */
static int  s_passWidth     = 0;
static int  s_passHeight    = 0;
static int  s_passDepth     = 0;   // 1 or 32
static int  s_passRowBytes  = 0;
static bool s_haveWhitePass = false;
```

#### ReadPixelsFromGuest algorithm

The guest sends a pointer to a BitMap or PixMap struct.  The host
reads the struct fields from guest RAM, then bulk-reads pixel data.

Mac struct layouts (big-endian in guest RAM):

```
BitMap (14 bytes):
  +0  long   baseAddr
  +4  word   rowBytes      (high bit = 0)
  +6  word   bounds.top
  +8  word   bounds.left
  +10 word   bounds.bottom
  +12 word   bounds.right

PixMap (extends BitMap, 50 bytes total):
  +0  long   baseAddr
  +4  word   rowBytes      (high bit = 1, mask 0x3FFF for actual value)
  +6  Rect   bounds (8 bytes)
  +14 word   pmVersion
  +16 word   packType
  +20 long   packSize
  +24 long   hRes (Fixed)
  +28 long   vRes (Fixed)
  +32 word   pixelType
  +34 word   pixelSize     ← bits per pixel (1, 2, 4, 8, 16, 32)
  +36 word   cmpCount
  +38 word   cmpSize
  ...
```

Discrimination: if `rawRowBytes & 0x8000` → PixMap, read `pixelSize`
at offset +34.  Otherwise → BitMap, depth = 1.

```cpp
static void ReadPixelsFromGuest(uint32_t structPtr,
                                std::vector<uint8_t> &pixels,
                                int &width, int &height,
                                int &depth, int &rowBytes)
{
    uint16_t rawRB = get_vm_word(structPtr + 4);
    bool isPixMap  = (rawRB & 0x8000) != 0;
    rowBytes       = rawRB & 0x3FFF;

    int16_t top    = static_cast<int16_t>(get_vm_word(structPtr + 6));
    int16_t left   = static_cast<int16_t>(get_vm_word(structPtr + 8));
    int16_t bottom = static_cast<int16_t>(get_vm_word(structPtr + 10));
    int16_t right  = static_cast<int16_t>(get_vm_word(structPtr + 12));

    width  = right - left;
    height = bottom - top;
    depth  = isPixMap ? get_vm_word(structPtr + 34) : 1;

    uint32_t baseAddr = get_vm_long(structPtr);
    size_t bufSize = static_cast<size_t>(rowBytes) * height;

    pixels.resize(bufSize);
    for (size_t i = 0; i < bufSize; ++i)
        pixels[i] = get_vm_byte(baseAddr + static_cast<uint32_t>(i));
}
```

#### HandlePictExport

```cpp
void HandlePictExport(uint32_t regParam[], uint16_t &regResult)
{
    uint32_t structPtr = regParam[0];
    uint32_t pass      = regParam[1];  // 0 = white bg, 1 = black bg

    std::vector<uint8_t> pixels;
    int width, height, depth, rowBytes;
    ReadPixelsFromGuest(structPtr, pixels, width, height, depth, rowBytes);

    if (width <= 0 || height <= 0 || pixels.empty())
    {
        regResult = 1;
        return;
    }

    if (pass == 0)
    {
        // White-background pass — stash for later
        s_passWhite    = std::move(pixels);
        s_passWidth    = width;
        s_passHeight   = height;
        s_passDepth    = depth;
        s_passRowBytes = rowBytes;
        s_haveWhitePass = true;
        regResult = 0;
        return;
    }

    // Black-background pass — composite and push to host clipboard
    if (!s_haveWhitePass || width != s_passWidth || height != s_passHeight)
    {
        s_haveWhitePass = false;
        regResult = 1;
        return;
    }

    std::vector<uint8_t> rgba;
    if (depth == 1)
        rgba = Composite1Bit(s_passWhite.data(), pixels.data(),
                             width, height, rowBytes);
    else
        rgba = Composite32Bit(s_passWhite.data(), pixels.data(),
                              width, height, rowBytes);

    auto png = EncodeRGBAPng(rgba.data(), width, height);
    if (!png.empty())
        HostClipSetImage(png.data(), png.size());

    s_passWhite.clear();
    s_haveWhitePass = false;
    regResult = 0;
}
```

#### Stub handlers

```cpp
void HandlePictHasImage(uint32_t regParam[], uint16_t &regResult)
{
    regParam[0] = 0;  // no image support yet
    regResult = 0;
}

void HandlePictImport(uint32_t regParam[], uint16_t &regResult)
{
    (void)regParam;
    regResult = 0xFFFF;  // not implemented
}

void ExtnPictReset()
{
    s_passWhite.clear();
    s_passBlack.clear();  // if any
    s_haveWhitePass = false;
}
```

### 2.3 — Wire into `extn_clip.cpp`

Add three new command constants and dispatch cases.

At top of `extn_clip.cpp`, after existing constants:
```cpp
#include "core/extn_clip_pict.h"

static constexpr uint16_t kPictExport   = 0x109;
static constexpr uint16_t kPictHasImage = 0x10A;
static constexpr uint16_t kPictImport   = 0x10B;
```

In `ExtnClipDispatch()`, before the `default:` case:
```cpp
case kPictExport:
    HandlePictExport(regParam, regResult);
    break;
case kPictHasImage:
    HandlePictHasImage(regParam, regResult);
    break;
case kPictImport:
    HandlePictImport(regParam, regResult);
    break;
```

In `ExtnClipReset()`, add:
```cpp
ExtnPictReset();
```

### 2.4 — Build integration

Add `src/core/extn_clip_pict.cpp` to `MINIVMAC_SOURCES` in
`CMakeLists.txt` (after `extn_clip.cpp`).

### Fence

- [ ] `src/core/extn_clip_pict.h` and `.cpp` exist
- [ ] `extn_clip.cpp` dispatches $109/$10A/$10B
- [ ] `ExtnClipReset` calls `ExtnPictReset`
- [ ] `cmake --build bld/macos` clean
- [ ] `bld/macos/tests` all pass (no regressions)
- [ ] Commit: `"clipboard: phase 2 — PictExport command handler"`

---

## Phase 3 — Guest-side PICT export (1-bit BitMap path)

Mac-side THINK C code.  The INIT detects PICT scrap, renders it
into an offscreen BitMap, and sends both passes to the host via
PictExport.

### 3.1 — Add constants to `macsrc/init/defs.h`

After the existing `kClipDbgLog` define:
```c
#define kPictExport   0x0109
#define kPictHasImage 0x010A
#define kPictImport   0x010B
```

### 3.2 — Create `macsrc/init/pict.c`

The 1-bit path only.  Color QuickDraw is not used; this compiles
and works on all systems from System 3.2+.

#### `ExportPictToHost` — 1-bit path

```c
/*
	maxivmac INIT — pict.c
	PICT clipboard export/import.
	Renders PICT through QuickDraw into an offscreen buffer
	and sends raw pixels to the host for PNG conversion.
*/

#include "defs.h"

/* ---- helpers ---- */

/*
	Return pixel depth of main screen.
	Returns 1 on compact Macs (no Color QuickDraw).
*/
static short ScreenDepth(void)
{
	long qdVersion;
	GDHandle mainDev;

	if (Gestalt(gestaltQuickdrawVersion, &qdVersion) != noErr)
		return 1;
	if (qdVersion < gestalt8BitQD)
		return 1;

	mainDev = GetMainDevice();
	if (mainDev == NULL) return 1;
	return (**(**mainDev).gdPMap).pixelSize;
}

/* ---- Guest → Host: PICT export ---- */

/*
	Export PICT scrap to host clipboard as rendered pixels.
	Two-pass rendering (white bg, black bg) for alpha detection.
	The host composites the two passes and encodes PNG.
	For depth == 1: uses plain BitMap + GrafPort.
	For depth > 1: uses 32-bit GWorld.  (Phase 6 adds this path.)
*/
void ExportPictToHost(char *regBase)
{
	Handle   h;
	long     offset, length;
	Rect     picFrame;
	short    depth;

	/* Get PICT data from desk scrap */
	h = NewHandle(0);
	if (h == NULL) return;

	length = GetScrap(h, 'PICT', &offset);
	if (length <= 0)
	{
		DisposHandle(h);
		return;
	}

	/*
		picFrame is at offset 2 in the PICT data.
		Bytes 0–1 are the picture size (word, may be inaccurate
		for PICT2), bytes 2–9 are the bounding Rect.
	*/
	picFrame = *(Rect *)(*h + 2);

	depth = ScreenDepth();

	if (depth == 1)
	{
		/* 1-bit: offscreen BitMap + temporary GrafPort */
		GrafPort offPort;
		BitMap   offBits;
		GrafPtr  savePort;
		short    rowBytes;
		long     bufSize;
		Ptr      bits;

		rowBytes = ((picFrame.right - picFrame.left + 15) / 16) * 2;
		bufSize  = (long)rowBytes * (picFrame.bottom - picFrame.top);
		bits     = NewPtr(bufSize);
		if (bits == NULL)
		{
			dbg_log(regBase, "pict: export alloc failed");
			DisposHandle(h);
			return;
		}

		offBits.baseAddr = bits;
		offBits.rowBytes = rowBytes;
		offBits.bounds   = picFrame;

		GetPort(&savePort);
		OpenPort(&offPort);
		SetPortBits(&offBits);
		offPort.portRect = picFrame;
		RectRgn(offPort.visRgn, &picFrame);
		RectRgn(offPort.clipRgn, &picFrame);

		/* --- Pass 0: white background --- */
		EraseRect(&picFrame);                   /* fills white (default) */
		DrawPicture((PicHandle)h, &picFrame);
		reg_set(regBase, 0, (unsigned long)&offBits);
		reg_set(regBase, 1, 0);                 /* pass = white */
		reg_command(regBase, kPictExport);

		/* --- Pass 1: black background --- */
		FillRect(&picFrame, &qd.black);
		DrawPicture((PicHandle)h, &picFrame);
		reg_set(regBase, 0, (unsigned long)&offBits);
		reg_set(regBase, 1, 1);                 /* pass = black */
		reg_command(regBase, kPictExport);

		SetPort(savePort);
		ClosePort(&offPort);
		DisposPtr(bits);
	}
	else
	{
		/* Color path — implemented in Phase 6 */
		dbg_log(regBase, "pict: color export not yet implemented");
	}

	DisposHandle(h);
}
```

### 3.3 — Wire into `clip.c`

In `SyncClipboard()`, modify the Mac→Host export block.  Currently:
```c
if ((unsigned long)scrapCnt != lastCnt)
{
    dbg_log2(g->regBase, "Sync: mac->host cnt %ld != %ld",
             (unsigned long)scrapCnt, lastCnt);
    if (ExportMacToHost(g->regBase) < 0)
        dbg_log(g->regBase, "clip: export error (ignored)");
    kv_set(g->regBase, key, (unsigned long)scrapCnt);
}
```

Change to:
```c
if ((unsigned long)scrapCnt != lastCnt)
{
    dbg_log2(g->regBase, "Sync: mac->host cnt %ld != %ld",
             (unsigned long)scrapCnt, lastCnt);
    if (ExportMacToHost(g->regBase) < 0)
        dbg_log(g->regBase, "clip: export error (ignored)");
    ExportPictToHost(g->regBase);
    kv_set(g->regBase, key, (unsigned long)scrapCnt);
}
```

Add `extern void ExportPictToHost(char *regBase);` at the top of
`clip.c`, or add the prototype to `defs.h` alongside the other
function prototypes.

### Fence

- [ ] `macsrc/init/pict.c` exists with `ExportPictToHost` (1-bit path)
- [ ] `macsrc/init/defs.h` has `kPictExport`, `kPictHasImage`, `kPictImport`
- [ ] `clip.c` calls `ExportPictToHost` on scrap change
- [ ] Guest code compiles in THINK C
- [ ] `cmake --build bld/macos` clean (host side)
- [ ] Commit: `"clipboard: phase 3 — guest-side 1-bit PICT export"`

---

## Phase 4 — Human test gate: 1-bit guest→host

**This is a manual test phase.  No code changes.**

### Test procedure

1. Boot a compact Mac configuration (e.g. Mac Plus, System 6)
2. Open MacPaint
3. Select an area with the lasso tool (irregular selection)
4. Copy (Cmd-C)
5. Switch to a host application (e.g. Preview, GIMP, or any image editor)
6. Paste (Cmd-V)

### Expected results

- [ ] A PNG image appears on the host clipboard
- [ ] Black pixels are opaque black
- [ ] White pixels are opaque white
- [ ] Lasso'd transparent areas have alpha = 0
- [ ] Image dimensions match the PICT's picFrame

### Additional tests

- [ ] Copy a rectangular selection in MacPaint → paste on host
- [ ] Copy text in MacWrite (which puts both TEXT + PICT) →
      host gets both text and image
- [ ] Copy a HyperCard card → paste on host (if available)
- [ ] Copy something with no PICT (pure text) → image clipboard
      not affected, text still works

### Fence

- [ ] Manual test pass confirmed
- [ ] Any bugs filed or fixed
- [ ] Commit (if fixes): `"clipboard: phase 4 — 1-bit test fixes"`

---

## Phase 5 — 32-bit pixel extraction + alpha compositing + tests

Add the `Composite32Bit` implementation and tests.

### 5.1 — Implement `Composite32Bit` in `pict_convert.cpp`

#### Algorithm

```
Mac 32-bit PixMap: each pixel is 4 bytes [X][R][G][B], big-endian.
rowBytes may be larger than width * 4 (padded).

For each pixel (x, y):
    off = y * rowBytes + x * 4

    // Read channels from white-bg pass
    wR = whitePass[off + 1]
    wG = whitePass[off + 2]
    wB = whitePass[off + 3]

    // Read channels from black-bg pass
    bR = blackPass[off + 1]
    bG = blackPass[off + 2]
    bB = blackPass[off + 3]

    // Alpha per channel: alpha = 255 - (white - black)
    // When fully opaque: white == black, so alpha = 255
    // When fully transparent: white = 255, black = 0, alpha = 0
    aR = 255 - (wR - bR)
    aG = 255 - (wG - bG)
    aB = 255 - (wB - bB)

    // Use minimum alpha (most transparent channel wins)
    a = min(aR, aG, aB)

    // Recover premultiplied color
    if a > 0:
        R = clamp(bR * 255 / a, 0, 255)
        G = clamp(bG * 255 / a, 0, 255)
        B = clamp(bB * 255 / a, 0, 255)
    else:
        R = G = B = 0

    output: (R, G, B, a)
```

### 5.2 — Add tests to `test/test_clip_pict.cpp`

| Test Case | Description |
|-----------|-------------|
| `Composite32Bit opaque red` | Both passes show (0,R,0,0) → opaque red, A=255 |
| `Composite32Bit opaque white` | Both passes show (0,255,255,255) → opaque white |
| `Composite32Bit fully transparent` | White=(0,255,255,255), Black=(0,0,0,0) → A=0 |
| `Composite32Bit semi-transparent` | 50% alpha pixel → verify recovered color |
| `Composite32Bit row padding` | rowBytes > width*4, verify padding skipped |
| `RGBATo1Bit roundtrip` | A simple black/white image → 1-bit → verify bits |

### Fence

- [ ] `Composite32Bit` implemented
- [ ] All new test cases pass
- [ ] `cmake --build bld/macos` clean
- [ ] `bld/macos/tests` all pass
- [ ] Commit: `"clipboard: phase 5 — 32-bit compositing + tests"`

---

## Phase 6 — Guest-side PICT export (32-bit GWorld path)

Add the Color QuickDraw path to `ExportPictToHost` in
`macsrc/init/pict.c`.

### 6.1 — Fill in the `else` branch in `ExportPictToHost`

Replace the "not yet implemented" stub:

```c
else
{
    /* Color: 32-bit GWorld */
    GWorldPtr    gw;
    PixMapHandle pm;
    CGrafPtr     savePort;
    GDHandle     saveDevice;
    QDErr        err;

    err = NewGWorld(&gw, 32, &picFrame, NULL, NULL, 0);
    if (err != noErr)
    {
        dbg_log1(regBase, "pict: NewGWorld err=%d", (int)err);
        DisposHandle(h);
        return;
    }

    pm = GetGWorldPixMap(gw);
    if (!LockPixels(pm))
    {
        DisposeGWorld(gw);
        DisposHandle(h);
        return;
    }

    GetGWorld(&savePort, &saveDevice);
    SetGWorld(gw, NULL);

    /* --- Pass 0: white background --- */
    BackColor(whiteColor);
    ForeColor(blackColor);
    EraseRect(&picFrame);
    DrawPicture((PicHandle)h, &picFrame);

    /*
        Send PixMap pointer.  The host reads the struct from
        guest RAM: baseAddr, rowBytes (with 0x8000 flag),
        bounds, and pixelSize at offset +34.
    */
    reg_set(regBase, 0, (unsigned long)StripAddress((Ptr)(*pm)));
    reg_set(regBase, 1, 0);
    reg_command(regBase, kPictExport);

    /* --- Pass 1: black background --- */
    BackColor(blackColor);
    EraseRect(&picFrame);
    BackColor(whiteColor);
    DrawPicture((PicHandle)h, &picFrame);

    reg_set(regBase, 0, (unsigned long)StripAddress((Ptr)(*pm)));
    reg_set(regBase, 1, 1);
    reg_command(regBase, kPictExport);

    SetGWorld(savePort, saveDevice);
    UnlockPixels(pm);
    DisposeGWorld(gw);
}
```

**Note:** `StripAddress` is needed because on 24-bit addressing Macs,
the PixMap pointer has flags in the high byte.  The host needs a
clean 24-bit address to index into guest RAM.  On 32-bit clean
systems it's a no-op.

### Fence

- [ ] GWorld path compiles in THINK C
- [ ] `cmake --build bld/macos` clean (host already handles 32-bit)
- [ ] Commit: `"clipboard: phase 6 — guest-side 32-bit PICT export"`

---

## Phase 7 — Human test gate: 32-bit guest→host

**Manual test phase.  No code changes.**

### Test procedure

1. Boot a Mac II configuration with color display (256 or millions)
2. Open a color-capable application:
   - Photoshop 1.0
   - PixelPaint
   - ClarisWorks drawing
   - SuperPaint
3. Select a region and Copy (Cmd-C)
4. Paste on the host

### Expected results

- [ ] Color PNG appears on host clipboard
- [ ] Colors are correct (not shifted, not garbled)
- [ ] Image dimensions match the selection
- [ ] Transparency works for irregular selections (if applicable)

### Additional tests

- [ ] Copy on compact Mac (1-bit path) still works after GWorld changes
- [ ] Copy from a 256-color display → verify colors
      (GWorld is 32-bit, so QD up-converts; should look correct)
- [ ] Large image (full screen 640×480) → verify no crash/truncation

### Fence

- [ ] Manual test pass confirmed for color images
- [ ] 1-bit path regression check passed
- [ ] Any bugs filed or fixed
- [ ] Commit (if fixes): `"clipboard: phase 7 — color test fixes"`

---

## Phase 8 — Host-side image query + import + tests

Implement `HandlePictHasImage` and `HandlePictImport` on the host
side.  Add platform functions for reading images from the host
clipboard.

### 8.1 — Add platform image clipboard functions

In `src/platform/common/clipboard.h`, add:
```cpp
/*
	Check if the host clipboard contains an image.
	If yes, sets width and height and returns true.
	Uses stbi_info_from_memory to read PNG header without
	full decode.
*/
bool HostClipHasImage(int *width, int *height);

/*
	Decode the host clipboard image to RGBA pixels.
	Returns empty vector on failure.
	Sets width and height on success.
*/
std::vector<uint8_t> HostClipGetImageRGBA(int *width, int *height);
```

In `src/platform/common/clipboard.cpp`, implement:
```cpp
bool HostClipHasImage(int *width, int *height)
{
#ifdef HAVE_SDL
    size_t dataLen = 0;
    void *data = SDL_GetClipboardData("image/png", &dataLen);
    if (!data || dataLen == 0) return false;

    int w, h, comp;
    bool ok = stbi_info_from_memory(
        static_cast<const uint8_t *>(data),
        static_cast<int>(dataLen), &w, &h, &comp);
    SDL_free(data);
    if (!ok) return false;

    *width = w;
    *height = h;
    return true;
#else
    (void)width; (void)height;
    return false;
#endif
}

std::vector<uint8_t> HostClipGetImageRGBA(int *width, int *height)
{
#ifdef HAVE_SDL
    size_t dataLen = 0;
    void *data = SDL_GetClipboardData("image/png", &dataLen);
    if (!data || dataLen == 0) return {};

    int w, h, comp;
    uint8_t *pixels = stbi_load_from_memory(
        static_cast<const uint8_t *>(data),
        static_cast<int>(dataLen), &w, &h, &comp, 4);
    SDL_free(data);
    if (!pixels) return {};

    *width = w;
    *height = h;
    std::vector<uint8_t> result(pixels, pixels + w * h * 4);
    stbi_image_free(pixels);
    return result;
#else
    (void)width; (void)height;
    return {};
#endif
}
```

This requires `stb_image.h` in `clipboard.cpp`.  Add the
implementation define to `stb_impl.cpp`:
```cpp
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
```

(Place before the existing `STB_IMAGE_WRITE_IMPLEMENTATION` block,
inside the same diagnostic-suppression pragma region.)

### 8.2 — Implement `HandlePictHasImage` in `extn_clip_pict.cpp`

Replace the stub from Phase 2:

```cpp
void HandlePictHasImage(uint32_t regParam[], uint16_t &regResult)
{
    int w = 0, h = 0;
    bool has = HostClipHasImage(&w, &h);

    regParam[0] = has ? 1 : 0;
    regParam[1] = static_cast<uint32_t>(w);
    regParam[2] = static_cast<uint32_t>(h);
    regResult = 0;
}
```

### 8.3 — Implement `HandlePictImport` in `extn_clip_pict.cpp`

Replace the stub from Phase 2:

```cpp
void HandlePictImport(uint32_t regParam[], uint16_t &regResult)
{
    uint32_t bufAddr  = regParam[0];
    uint32_t rowBytes = regParam[1];
    uint32_t depth    = regParam[2];
    uint32_t width    = regParam[3];
    uint32_t height   = regParam[4];

    /* Decode PNG from host clipboard to RGBA */
    int imgW = 0, imgH = 0;
    auto rgba = HostClipGetImageRGBA(&imgW, &imgH);
    if (rgba.empty())
    {
        regResult = 1;
        return;
    }

    /*
        Scale mismatch: the guest requested specific dimensions.
        For V1 we require exact match (guest allocates based on
        PictHasImage dimensions).  If they differ, fail.
    */
    if (static_cast<uint32_t>(imgW) != width ||
        static_cast<uint32_t>(imgH) != height)
    {
        regResult = 2;
        return;
    }

    /* Convert RGBA to guest format and write into guest RAM */
    if (depth == 1)
    {
        int outRB = 0;
        auto bits = RGBATo1Bit(rgba.data(), imgW, imgH, outRB);

        /* Write row-by-row in case rowBytes differs from outRB */
        for (int y = 0; y < imgH; ++y)
        {
            int copyBytes = (outRB < static_cast<int>(rowBytes))
                            ? outRB : static_cast<int>(rowBytes);
            for (int x = 0; x < copyBytes; ++x)
            {
                put_vm_byte(
                    bufAddr + y * rowBytes + x,
                    bits[y * outRB + x]);
            }
        }
    }
    else
    {
        int outRB = 0;
        auto xrgb = RGBATo32Bit(rgba.data(), imgW, imgH, outRB);

        for (int y = 0; y < imgH; ++y)
        {
            int copyBytes = (outRB < static_cast<int>(rowBytes))
                            ? outRB : static_cast<int>(rowBytes);
            for (int x = 0; x < copyBytes; ++x)
            {
                put_vm_byte(
                    bufAddr + y * rowBytes + x,
                    xrgb[y * outRB + x]);
            }
        }
    }

    regResult = 0;
}
```

### 8.4 — Unit tests

Add to `test/test_clip_pict.cpp`:

| Test Case | Description |
|-----------|-------------|
| `RGBATo1Bit black pixel` | RGBA(0,0,0,255) → bit = 1 |
| `RGBATo1Bit white pixel` | RGBA(255,255,255,255) → bit = 0 |
| `RGBATo1Bit gray threshold` | RGBA(127,...) → bit = 1 (below 50%) |
| `RGBATo32Bit red pixel` | RGBA(255,0,0,255) → [0x00, 0xFF, 0x00, 0x00] |
| `WritePixels row stride` | rowBytes > minimum → padding zeroed |

### Fence

- [ ] `HandlePictHasImage` and `HandlePictImport` implemented
- [ ] `HostClipHasImage` and `HostClipGetImageRGBA` implemented
- [ ] `stb_image.h` implementation activated in `stb_impl.cpp`
- [ ] All new tests pass
- [ ] `cmake --build bld/macos` clean
- [ ] `bld/macos/tests` all pass
- [ ] Commit: `"clipboard: phase 8 — host-side image import"`

---

## Phase 9 — Unified ClipSeqNo

Make the host clipboard sequence number bump on image changes, not
just text changes.

### 9.1 — Modify `ClipSeqNo` handler in `extn_clip.cpp`

Add a `static bool s_lastHasImage = false;` alongside
`s_lastClipText`.

Change the `kClipSeqNo` case:

```cpp
case kClipSeqNo:
{
    std::string currentText = hostClipGetTextMacRoman();
    bool currentHasImage = HostClipHasImage(nullptr, nullptr);

    if (currentText != s_lastClipText ||
        currentHasImage != s_lastHasImage)
    {
        s_lastClipText = currentText;
        s_lastHasImage = currentHasImage;
        s_clipCache = currentText;
        s_clipSeqNo++;
    }
    regParam[0] = s_clipSeqNo;
    regResult = 0;
}
```

Note: `HostClipHasImage` with null width/height pointers needs to
handle that gracefully.  Update the implementation to accept nullptrs:

```cpp
bool HostClipHasImage(int *width, int *height)
{
    // ... existing code ...
    if (width)  *width  = w;
    if (height) *height = h;
    return true;
}
```

### 9.2 — Reset new state in `ExtnClipReset`

```cpp
s_lastHasImage = false;
```

### 9.3 — Update anti-feedback in `PictExport`

After the host processes a `PictExport` (pass 1), the host clipboard
now has an image.  The next `ClipSeqNo` poll must not see this as
a "new" change.  Add after `HostClipSetImage`:

```cpp
s_lastHasImage = true;
```

This requires either making `s_lastHasImage` accessible from
`extn_clip_pict.cpp`, or providing a setter function:

```cpp
// in extn_clip.h:
void ExtnClipMarkImageExported();

// in extn_clip.cpp:
void ExtnClipMarkImageExported()
{
    s_lastHasImage = true;
}
```

Call `ExtnClipMarkImageExported()` from `HandlePictExport` after
placing the PNG on the clipboard.

### Fence

- [ ] `ClipSeqNo` bumps on image clipboard changes
- [ ] Anti-feedback: guest export doesn't trigger re-import
- [ ] `cmake --build bld/macos` clean
- [ ] `bld/macos/tests` all pass
- [ ] Commit: `"clipboard: phase 9 — unified ClipSeqNo"`

---

## Phase 10 — Guest-side PICT import (host→guest)

Mac-side THINK C code.  The INIT detects host image availability,
allocates an offscreen buffer, receives pixels from the host, and
creates a PICT via QuickDraw.

### 10.1 — `ImportPictFromHost` in `macsrc/init/pict.c`

```c
/*
	Import host clipboard image to Mac desk scrap as PICT.
	Queries host for image presence + dimensions, allocates
	an offscreen buffer at screen depth, receives pixels,
	then uses OpenPicture/CopyBits/ClosePicture to create
	a valid PICT and puts it on the desk scrap.
*/
void ImportPictFromHost(char *regBase)
{
    unsigned long hasImg, width, height;
    short         depth;
    Rect          r;

    /* Ask host if it has an image */
    reg_command(regBase, kPictHasImage);
    hasImg = reg_get(regBase, 0);
    if (!hasImg) return;
    width  = reg_get(regBase, 1);
    height = reg_get(regBase, 2);
    if (width == 0 || height == 0 || width > 4096 || height > 4096)
        return;

    depth = ScreenDepth();
    SetRect(&r, 0, 0, (short)width, (short)height);

    if (depth == 1)
    {
        /* 1-bit: plain BitMap + GrafPort */
        BitMap   offBits;
        GrafPort offPort;
        GrafPtr  savePort;
        PicHandle pic;
        short rowBytes;
        long  bufSize;
        Ptr   bits;

        rowBytes = ((width + 15) / 16) * 2;
        bufSize  = (long)rowBytes * height;
        bits = NewPtr(bufSize);
        if (bits == NULL)
        {
            dbg_log(regBase, "pict: import alloc failed");
            return;
        }

        /* Tell host to fill our buffer with 1-bit pixels */
        reg_set(regBase, 0, (unsigned long)bits);
        reg_set(regBase, 1, (unsigned long)rowBytes);
        reg_set(regBase, 2, 1);           /* depth */
        reg_set(regBase, 3, width);
        reg_set(regBase, 4, height);
        reg_command(regBase, kPictImport);

        if (reg_result(regBase) != 0)
        {
            dbg_log(regBase, "pict: import host error");
            DisposPtr(bits);
            return;
        }

        offBits.baseAddr = bits;
        offBits.rowBytes = rowBytes;
        offBits.bounds   = r;

        /* Create PICT by recording a CopyBits */
        GetPort(&savePort);
        OpenPort(&offPort);
        SetPortBits(&offBits);
        offPort.portRect = r;
        RectRgn(offPort.visRgn, &r);
        RectRgn(offPort.clipRgn, &r);

        pic = OpenPicture(&r);
        CopyBits(&offBits, &offPort.portBits, &r, &r, srcCopy, NULL);
        ClosePicture();

        SetPort(savePort);
        ClosePort(&offPort);
        DisposPtr(bits);

        if (pic != NULL && GetHandleSize((Handle)pic) > 10)
        {
            ZeroScrap();
            HLock((Handle)pic);
            PutScrap(GetHandleSize((Handle)pic), 'PICT', *pic);
            HUnlock((Handle)pic);
            KillPicture(pic);
        }
    }
    else
    {
        /* 32-bit: GWorld */
        GWorldPtr    gw;
        PixMapHandle pm;
        CGrafPtr     savePort;
        GDHandle     saveDevice;
        PicHandle    pic;
        QDErr        err;
        Ptr          baseAddr;
        long         rb;

        err = NewGWorld(&gw, 32, &r, NULL, NULL, 0);
        if (err != noErr)
        {
            dbg_log1(regBase, "pict: import NewGWorld err=%d", (int)err);
            return;
        }

        pm = GetGWorldPixMap(gw);
        if (!LockPixels(pm))
        {
            DisposeGWorld(gw);
            return;
        }

        baseAddr = GetPixBaseAddr(pm);
        rb = (**pm).rowBytes & 0x3FFF;

        /* Tell host to fill the PixMap buffer with XRGB pixels */
        reg_set(regBase, 0, (unsigned long)StripAddress(baseAddr));
        reg_set(regBase, 1, (unsigned long)rb);
        reg_set(regBase, 2, 32);
        reg_set(regBase, 3, width);
        reg_set(regBase, 4, height);
        reg_command(regBase, kPictImport);

        if (reg_result(regBase) != 0)
        {
            dbg_log(regBase, "pict: import host error (32-bit)");
            UnlockPixels(pm);
            DisposeGWorld(gw);
            return;
        }

        /* Create PICT by recording a CopyBits from the GWorld */
        GetGWorld(&savePort, &saveDevice);
        SetGWorld(gw, NULL);

        pic = OpenPicture(&r);
        CopyBits((BitMap *)*pm,
                 (BitMap *)*pm,
                 &r, &r, srcCopy, NULL);
        ClosePicture();

        SetGWorld(savePort, saveDevice);
        UnlockPixels(pm);
        DisposeGWorld(gw);

        if (pic != NULL && GetHandleSize((Handle)pic) > 10)
        {
            ZeroScrap();
            HLock((Handle)pic);
            PutScrap(GetHandleSize((Handle)pic), 'PICT', *pic);
            HUnlock((Handle)pic);
            KillPicture(pic);
        }
    }
}
```

### 10.2 — Wire into `clip.c`

In the Host→Mac import block, add PICT import call:

```c
if (hostSeq != lastSeq)
{
    dbg_log2(g->regBase, "Sync: host->mac seq %lx != %lx",
             hostSeq, lastSeq);
    if (ImportHostToMac(g->regBase) < 0)
        dbg_log(g->regBase, "clip: import error (ignored)");
    ImportPictFromHost(g->regBase);
    kv_set(g->regBase, key, hostSeq);
    kv_set(g->regBase, (unsigned long)appId * 2 + 1,
           (unsigned long)*(short *)kScrapCount);
}
```

Add `extern void ImportPictFromHost(char *regBase);` to `defs.h`
alongside `ExportPictToHost`.

### Fence

- [ ] `ImportPictFromHost` implemented (both 1-bit and 32-bit paths)
- [ ] `clip.c` calls it on host seq change
- [ ] Guest code compiles in THINK C
- [ ] `cmake --build bld/macos` clean
- [ ] Commit: `"clipboard: phase 10 — guest-side PICT import"`

---

## Phase 11 — Human test gate: host→guest

**Manual test phase.  No code changes.**

### Test procedure: 1-bit (compact Mac)

1. On the host, copy a small image (e.g. screenshot of an icon)
2. In the emulator (Mac Plus, System 6), open MacPaint or similar
3. Paste (Cmd-V)
4. Verify the image appears, dithered/thresholded to B&W

### Test procedure: 32-bit (Mac II)

1. On the host, copy a color photo
2. In the emulator (Mac II, System 7), open a color painting app
3. Paste (Cmd-V)
4. Verify the image appears in color

### Expected results

- [ ] Image pastes correctly in at least one 1-bit app
- [ ] Image pastes correctly in at least one color app
- [ ] Dimensions match the original
- [ ] No crash on large images (try a 1024×768 screenshot)
- [ ] No crash when host has no image (text-only copy)
- [ ] TEXT import still works (no regression)
- [ ] Bidirectional test: copy on Mac → paste on host → copy on
      host again → paste on Mac → same image round-trips

### Fence

- [ ] All manual tests pass
- [ ] Any bugs filed or fixed
- [ ] Commit (if fixes): `"clipboard: phase 11 — import test fixes"`

---

## Phase 12 — Version bump + documentation

### 12.1 — Bump ClipVersion to 3

In `extn_clip.cpp`, `kClipVersion` handler:
```cpp
case kClipVersion:
    regParam[0] = 3;
    regResult = 0;
    break;
```

### 12.2 — Update INIT version check

In `macsrc/init/` startup code, the INIT checks `ClipVersion >= 2`.
Change to check `>= 3` and gracefully degrade (skip PICT sync if
version < 3, keep TEXT sync working for old hosts).

### 12.3 — Update `docs/features/CLIPBOARD.md`

- Remove "TEXT only. PICT is not yet supported."
- Add PICT support documentation:
  - Guest→Host: any PICT becomes PNG on host clipboard
  - Host→Guest: PNG on host becomes PICT on Mac scrap
  - Transparency: two-pass rendering for alpha detection
  - Depth: matches main screen (1-bit or 32-bit)
- Update architecture diagram to show PICT path
- Update command table with $109/$10A/$10B
- Add `pict_convert.cpp`, `extn_clip_pict.cpp`, `pict.c` to
  source files table

### 12.4 — Update `docs/TODO_CLIPBOARD.md`

- Mark "Consider PICT scrap type support" as done
- Remove from remaining work list

### 12.5 — Add code comments

Audit all new files and ensure they have:
- File-level header comment (purpose, relationship to other files)
- Function-level comments for all public functions (what, not how)
- Inline comments for non-obvious logic:
  - Bit manipulation in 1-bit compositing
  - Alpha recovery formula derivation
  - Mac struct offsets and field meanings
  - PixMap vs BitMap discrimination
  - StripAddress usage rationale
  - Port save/restore sequences
  - Feedback prevention in ClipSeqNo

### Fence

- [ ] `ClipVersion` returns 3
- [ ] `CLIPBOARD.md` updated with PICT support
- [ ] `TODO_CLIPBOARD.md` updated
- [ ] All files have appropriate comments
- [ ] `cmake --build bld/macos` clean
- [ ] `bld/macos/tests` all pass
- [ ] Commit: `"clipboard: phase 12 — version 3 + docs"`
