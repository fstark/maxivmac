# PICT Clipboard — Detailed Design

Extends the clipboard sync system described in
[features/CLIPBOARD.md](features/CLIPBOARD.md) with bidirectional
image support.  TEXT sync is unchanged.

All code must follow [STYLE.md](STYLE.md) and [NAMING.md](NAMING.md).

---

## 1. Overview

Copy an image on the Mac, paste it on the host (and vice versa).
The Mac side renders PICT data through QuickDraw into a flat pixel
buffer; the host side converts that to/from PNG.  No PICT parsing
on the host — QuickDraw handles all rendering.

### Design Decisions (from grill session)

- **QuickDraw renders, not the host.**  The INIT allocates an
  offscreen buffer, fills it, and tells the host where the pixels
  are.  All PICT opcodes (bitmap, vector, text, patterns) are
  handled by real QuickDraw.
- **Two-pass transparency.**  Render on white background, send.
  Render on black background, send.  Host computes alpha from the
  difference.  Same buffer reused — no double allocation.
- **Depth = screen depth, simplified.**
  `screenDepth == 1` → 1-bit `BitMap`.
  `screenDepth > 1` → 32-bit `GWorld` (QuickDraw up-converts).
  No CLUT transfer ever needed.
- **If allocation fails, no sync.**  No caps, no fallbacks.
  `NewGWorld` / `NewPtr` returns nil → bail silently.
- **TEXT and PICT exported independently.**  If the scrap has both,
  both are sent.  The host clipboard gets `text/plain` + `image/png`.
  Host apps pick whichever they prefer.
- **`ClipSeqNo` becomes unified.**  One counter, bumps on text OR
  image changes on the host.  Guest polls one number.
- **Version bump to 3.**  Old INITs see version 2, never issue PICT
  commands.

---

## 2. New Commands

Three new commands added to the register block at
`extnBlockBase + $20`:

### PictExport ($109) — Guest → Host

Guest sends a pointer to the QuickDraw data structure (BitMap or
PixMap) plus a pass indicator.  Called twice per clipboard change.

```
PictExport    $109
  p0 = pointer to BitMap or PixMap struct (in guest RAM)
  p1 = pass (0 = white background, 1 = black background)
  result: 0 = ok
```

The host reads the struct from guest RAM.  For BitMap: `baseAddr`,
`rowBytes`, `bounds`.  For PixMap: same fields plus `pixelSize`
(high bit of `rowBytes` distinguishes PixMap from BitMap).

After receiving pass 1, the host:
1. Computes alpha from the two passes
2. Encodes RGBA PNG via `stb_image_write`
3. Places PNG on host clipboard via `SDL_SetClipboardData`

### PictHasImage ($10A) — Host → Guest query

Guest asks whether the host clipboard contains an image and its dimensions.

```
PictHasImage  $10A
  (no input parameters)
  p0 = 1 if host has image, 0 if not
  p1 = width in pixels  (valid only if p0 = 1)
  p2 = height in pixels (valid only if p0 = 1)
  result: 0 = ok
```

The host decodes the PNG header (via `stbi_info_from_memory`) to
get dimensions without decoding the full image.

### PictImport ($10B) — Host → Guest pixel transfer

Guest provides a buffer and its format.  Host converts the PNG to
the requested depth and writes pixels directly into guest RAM.

```
PictImport    $10B
  p0 = buffer address (guest RAM)
  p1 = rowBytes
  p2 = depth (1 or 32)
  p3 = width
  p4 = height
  result: 0 = ok
```

For depth 32: host decodes PNG → XRGB (matching Mac 32-bit PixMap
layout: `[unused][R][G][B]` per pixel), writes via `put_vm_byte`.

For depth 1: host decodes PNG → threshold to 1-bit, packs bits
MSB-first (matching Mac BitMap layout), writes via `put_vm_byte`.

---

## 3. Guest-Side Changes (macsrc/init/)

### 3.1 New file: `pict.c`

PICT clipboard operations, called from the sync loop in `clip.c`.

#### ExportPictToHost

```c
static void ExportPictToHost(char *regBase)
{
    Handle    h;
    long      offset, length;
    short     depth;
    Rect      picFrame;
    BitMap    offBits;
    GWorldPtr gw;
    PixMapHandle pm;
    Ptr       baseAddr;

    /* Get PICT from desk scrap */
    h = NewHandle(0);
    if (h == NULL) return;
    length = GetScrap(h, 'PICT', &offset);
    if (length <= 0) { DisposHandle(h); return; }

    /* Get picFrame from PICT header (bytes 2..9) */
    picFrame = *(Rect *)(*h + 2);

    /* Determine depth */
    depth = ScreenDepth();  /* see 3.3 below */

    if (depth == 1)
    {
        /* 1-bit: plain BitMap + GrafPort */
        short rowBytes = ((picFrame.right - picFrame.left + 15) / 16) * 2;
        long  bufSize  = (long)rowBytes * (picFrame.bottom - picFrame.top);
        Ptr   bits     = NewPtr(bufSize);
        if (bits == NULL) { DisposHandle(h); return; }

        offBits.baseAddr = bits;
        offBits.rowBytes = rowBytes;
        offBits.bounds   = picFrame;

        /* --- Pass 0: white background --- */
        SetUpBitMapPort(&offBits, 0xFF);  /* fill white */
        DrawPicture((PicHandle)h, &picFrame);
        reg_set(regBase, 0, (unsigned long)&offBits);
        reg_set(regBase, 1, 0);  /* pass = white */
        reg_command(regBase, kPictExport);

        /* --- Pass 1: black background --- */
        FillBitMap(&offBits, 0x00);       /* fill black */
        DrawPicture((PicHandle)h, &picFrame);
        reg_set(regBase, 0, (unsigned long)&offBits);
        reg_set(regBase, 1, 1);  /* pass = black */
        reg_command(regBase, kPictExport);

        ClosePort(/* the temp port */);
        DisposPtr(bits);
    }
    else
    {
        /* Color: 32-bit GWorld */
        QDErr err = NewGWorld(&gw, 32, &picFrame, NULL, NULL, 0);
        if (err != noErr) { DisposHandle(h); return; }

        pm = GetGWorldPixMap(gw);
        LockPixels(pm);

        /* --- Pass 0: white background --- */
        SetGWorld(gw, NULL);
        EraseRect(&picFrame);             /* white (default) */
        DrawPicture((PicHandle)h, &picFrame);
        reg_set(regBase, 0, (unsigned long)*pm);
        reg_set(regBase, 1, 0);
        reg_command(regBase, kPictExport);

        /* --- Pass 1: black background --- */
        ForeColor(whiteColor);
        BackColor(blackColor);
        EraseRect(&picFrame);             /* now fills black */
        ForeColor(blackColor);
        BackColor(whiteColor);
        DrawPicture((PicHandle)h, &picFrame);
        reg_set(regBase, 0, (unsigned long)*pm);
        reg_set(regBase, 1, 1);
        reg_command(regBase, kPictExport);

        UnlockPixels(pm);
        DisposeGWorld(gw);
    }

    DisposHandle(h);
}
```

#### ImportPictFromHost

```c
static void ImportPictFromHost(char *regBase)
{
    unsigned long hasImg, width, height;
    short         depth;
    Rect          r;
    PicHandle     pic;
    GWorldPtr     gw;
    PixMapHandle  pm;

    reg_command(regBase, kPictHasImage);
    hasImg = reg_get(regBase, 0);
    if (!hasImg) return;
    width  = reg_get(regBase, 1);
    height = reg_get(regBase, 2);

    depth = ScreenDepth();
    SetRect(&r, 0, 0, (short)width, (short)height);

    if (depth == 1)
    {
        /* 1-bit BitMap */
        short rowBytes = ((width + 15) / 16) * 2;
        long  bufSize  = (long)rowBytes * height;
        Ptr   bits     = NewPtr(bufSize);
        BitMap offBits;
        GrafPort port;
        if (bits == NULL) return;

        offBits.baseAddr = bits;
        offBits.rowBytes = rowBytes;
        offBits.bounds   = r;

        /* Tell host to fill our buffer */
        reg_set(regBase, 0, (unsigned long)bits);
        reg_set(regBase, 1, (unsigned long)rowBytes);
        reg_set(regBase, 2, 1);       /* depth */
        reg_set(regBase, 3, width);
        reg_set(regBase, 4, height);
        reg_command(regBase, kPictImport);
        if (reg_result(regBase) != 0) { DisposPtr(bits); return; }

        /* Create PICT from bitmap via OpenPicture/CopyBits */
        OpenPort(&port);
        pic = OpenPicture(&r);
        CopyBits(&offBits, &port.portBits, &r, &r, srcCopy, NULL);
        ClosePicture();
        ClosePort(&port);
        DisposPtr(bits);
    }
    else
    {
        /* 32-bit GWorld */
        QDErr err = NewGWorld(&gw, 32, &r, NULL, NULL, 0);
        if (err != noErr) return;

        pm = GetGWorldPixMap(gw);
        LockPixels(pm);

        reg_set(regBase, 0, (unsigned long)GetPixBaseAddr(pm));
        reg_set(regBase, 1, (unsigned long)((**pm).rowBytes & 0x3FFF));
        reg_set(regBase, 2, 32);
        reg_set(regBase, 3, width);
        reg_set(regBase, 4, height);
        reg_command(regBase, kPictImport);
        if (reg_result(regBase) != 0)
        {
            UnlockPixels(pm);
            DisposeGWorld(gw);
            return;
        }

        /* Create PICT from GWorld */
        CGrafPtr  savePort;
        GDHandle  saveDevice;
        GetGWorld(&savePort, &saveDevice);
        SetGWorld(gw, NULL);
        pic = OpenPicture(&r);
        CopyBits((BitMap *)*pm,
                 (BitMap *)*GetGWorldPixMap(gw),
                 &r, &r, srcCopy, NULL);
        ClosePicture();
        SetGWorld(savePort, saveDevice);

        UnlockPixels(pm);
        DisposeGWorld(gw);
    }

    if (pic != NULL && GetHandleSize((Handle)pic) > 10)
    {
        /* Don't ZeroScrap here — clip.c does it for TEXT.
           If no TEXT, we need ZeroScrap. */
        ZeroScrap();
        HLock((Handle)pic);
        PutScrap(GetHandleSize((Handle)pic), 'PICT', *pic);
        HUnlock((Handle)pic);
        KillPicture(pic);
    }
}
```

### 3.2 CQD detection

```c
/* Returns 1 if Color QuickDraw is available, 0 otherwise. */
static short HasColorQD(void)
{
    long qdVersion;
    if (Gestalt(gestaltQuickdrawVersion, &qdVersion) != noErr)
        return 0;
    return (qdVersion >= gestalt8BitQD) ? 1 : 0;
}
```

### 3.3 Screen depth helper

```c
/* Returns pixel depth of main screen (1 on compact Macs). */
static short ScreenDepth(void)
{
    GDHandle mainDev;
    if (!HasColorQD()) return 1;
    mainDev = GetMainDevice();
    if (mainDev == NULL) return 1;
    return (**(**mainDev).gdPMap).pixelSize;
}
```

### 3.4 New constants in `defs.h`

```c
#define kPictExport   0x0109
#define kPictHasImage 0x010A
#define kPictImport   0x010B
```

### 3.5 Sync loop changes in `clip.c`

In `SyncClipboard()`, after the existing `ExportMacToHost()` call:

```c
/* Mac -> Host: also export PICT if present */
if ((unsigned long)scrapCnt != lastCnt)
{
    ExportMacToHost(g->regBase);         /* TEXT — existing */
    ExportPictToHost(g->regBase);        /* PICT — new */
    kv_set(g->regBase, key, (unsigned long)scrapCnt);
}
```

In the host→mac path, after `ImportHostToMac()`:

```c
/* Host -> Mac: also import PICT if available */
if (hostSeq != lastSeq)
{
    ImportHostToMac(g->regBase);         /* TEXT — existing */
    ImportPictFromHost(g->regBase);      /* PICT — new */
    kv_set(g->regBase, key, hostSeq);
    kv_set(g->regBase, ...);             /* feedback prevention */
}
```

---

## 4. Host-Side Changes (src/)

### 4.1 New file: `src/core/extn_clip_pict.cpp`

PICT export/import command handlers.  Separate file to keep
`extn_clip.cpp` from growing.

#### State

```cpp
// Two pixel buffers for white/black pass
static std::vector<uint8_t> s_passWhite;
static std::vector<uint8_t> s_passBlack;
static int s_passWidth  = 0;
static int s_passHeight = 0;
static int s_passDepth  = 0;  // 1 or 32
static bool s_haveWhitePass = false;
```

#### ReadBitMapFromGuest

Reads a 14-byte `BitMap` struct from guest RAM:

```cpp
struct GuestBitMap {
    uint32_t baseAddr;
    uint16_t rowBytes;   // high bit clear
    int16_t  top, left, bottom, right;
};

static GuestBitMap ReadBitMapFromGuest(uint32_t ptr)
{
    GuestBitMap bm;
    bm.baseAddr = get_vm_long(ptr + 0);
    bm.rowBytes = get_vm_word(ptr + 4);
    bm.top      = static_cast<int16_t>(get_vm_word(ptr + 6));
    bm.left     = static_cast<int16_t>(get_vm_word(ptr + 8));
    bm.bottom   = static_cast<int16_t>(get_vm_word(ptr + 10));
    bm.right    = static_cast<int16_t>(get_vm_word(ptr + 12));
    return bm;
}
```

The high bit of `rowBytes` distinguishes PixMap (set) from BitMap
(clear).  If it's a PixMap, `pixelSize` is at offset +32 from
the struct base.

#### HandlePictExport

```cpp
void HandlePictExport(uint32_t regParam[], uint16_t &regResult)
{
    uint32_t structPtr = regParam[0];
    uint32_t pass      = regParam[1];  // 0=white, 1=black

    uint16_t rawRowBytes = get_vm_word(structPtr + 4);
    bool isPixMap = (rawRowBytes & 0x8000) != 0;
    uint16_t rowBytes = rawRowBytes & 0x3FFF;

    int16_t top    = static_cast<int16_t>(get_vm_word(structPtr + 6));
    int16_t left   = static_cast<int16_t>(get_vm_word(structPtr + 8));
    int16_t bottom = static_cast<int16_t>(get_vm_word(structPtr + 10));
    int16_t right  = static_cast<int16_t>(get_vm_word(structPtr + 12));

    int width  = right - left;
    int height = bottom - top;
    int depth  = isPixMap
        ? get_vm_word(structPtr + 32)   // PixMap.pixelSize
        : 1;

    uint32_t baseAddr = get_vm_long(structPtr);

    // Read pixel data from guest RAM
    size_t bufSize = static_cast<size_t>(rowBytes) * height;
    auto &target = (pass == 0) ? s_passWhite : s_passBlack;
    target.resize(bufSize);
    for (size_t i = 0; i < bufSize; ++i)
        target[i] = get_vm_byte(baseAddr + i);

    if (pass == 0)
    {
        // First pass — store metadata, wait for second
        s_passWidth  = width;
        s_passHeight = height;
        s_passDepth  = depth;
        s_haveWhitePass = true;
    }
    else if (s_haveWhitePass)
    {
        // Second pass — composite and encode PNG
        CompositeAndSetClipboard();
        s_haveWhitePass = false;
    }

    regResult = 0;
}
```

#### CompositeAndSetClipboard

Alpha compositing from two passes, then PNG encode.

```cpp
static void CompositeAndSetClipboard()
{
    int w = s_passWidth;
    int h = s_passHeight;
    std::vector<uint8_t> rgba(w * h * 4);

    if (s_passDepth == 1)
        Composite1Bit(rgba, s_passWhite, s_passBlack, w, h);
    else
        Composite32Bit(rgba, s_passWhite, s_passBlack, w, h);

    // Encode PNG
    int pngLen = 0;
    uint8_t *png = stbi_write_png_to_mem(
        rgba.data(), w * 4, w, h, 4, &pngLen);
    if (png)
    {
        HostClipSetImage(png, pngLen);
        STBIW_FREE(png);
    }

    s_passWhite.clear();
    s_passBlack.clear();
}
```

#### Composite1Bit

```
For each pixel (x, y):
    wBit = bit from white-pass at (x, y)    // 0=black, 1=white in Mac convention
    bBit = bit from black-pass at (x, y)

    if wBit == 0 && bBit == 0  →  opaque black   (R=0, G=0, B=0, A=255)
    if wBit == 1 && bBit == 1  →  opaque white   (R=255, G=255, B=255, A=255)
    if wBit == 1 && bBit == 0  →  transparent     (R=0, G=0, B=0, A=0)
    if wBit == 0 && bBit == 1  →  shouldn't happen (treat as opaque black)

Note: Mac 1-bit bitmaps are packed MSB-first, 0 = black, 1 = white
(inverted from the convention you might expect).
Wait — actually Mac convention: 0 = white, 1 = black in terms
of "ink".  Verify during implementation against real data.
```

#### Composite32Bit

```
For each pixel (x, y):
    Mac 32-bit PixMap layout: [unused][R][G][B] per pixel (4 bytes)
    Read R_w, G_w, B_w from white pass
    Read R_b, G_b, B_b from black pass

    Per channel:
        alpha_c = 255 - (W_c - B_c)
        color_c = (alpha_c > 0) ? (B_c * 255 / alpha_c) : 0

    A = min(alpha_R, alpha_G, alpha_B)  — or average; refine later
    Write (R, G, B, A) to output
```

#### HandlePictHasImage

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

#### HandlePictImport

```cpp
void HandlePictImport(uint32_t regParam[], uint16_t &regResult)
{
    uint32_t bufAddr  = regParam[0];
    uint32_t rowBytes = regParam[1];
    uint32_t depth    = regParam[2];
    uint32_t width    = regParam[3];
    uint32_t height   = regParam[4];

    // Decode PNG from host clipboard
    auto pixels = HostClipGetImagePixels();  // returns RGBA
    if (pixels.empty()) { regResult = 1; return; }

    // Convert RGBA to requested depth and write to guest RAM
    if (depth == 1)
        WritePixels1Bit(bufAddr, rowBytes, width, height, pixels);
    else
        WritePixels32Bit(bufAddr, rowBytes, width, height, pixels);

    regResult = 0;
}
```

### 4.2 New file: `src/core/extn_clip_pict.h`

```cpp
#pragma once
#include <cstdint>

void HandlePictExport(uint32_t regParam[], uint16_t &regResult);
void HandlePictHasImage(uint32_t regParam[], uint16_t &regResult);
void HandlePictImport(uint32_t regParam[], uint16_t &regResult);
```

### 4.3 Changes to `src/core/extn_clip.cpp`

Add dispatch cases for the three new commands.  Bump version to 3.

```cpp
// At top: new command constants
static constexpr uint16_t kPictExport   = 0x109;
static constexpr uint16_t kPictHasImage = 0x10A;
static constexpr uint16_t kPictImport   = 0x10B;

// In ExtnClipDispatch switch:
case kPictExport:
    HandlePictExport(regParam, regResult);
    break;
case kPictHasImage:
    HandlePictHasImage(regParam, regResult);
    break;
case kPictImport:
    HandlePictImport(regParam, regResult);
    break;

// kClipVersion now returns 3
```

### 4.4 Changes to `ClipSeqNo` in `extn_clip.cpp`

Make the sequence number bump on image changes too:

```cpp
case kClipSeqNo:
{
    std::string currentText = hostClipGetTextMacRoman();
    bool currentHasImage = hostClipHasImage();

    if (currentText != s_lastClipText || currentHasImage != s_lastHasImage)
    {
        s_lastClipText = currentText;
        s_lastHasImage = currentHasImage;
        s_clipSeqNo++;
    }
    regParam[0] = s_clipSeqNo;
    regResult = 0;
}
```

### 4.5 Platform layer additions

New functions in `src/platform/common/clipboard.h`:

```cpp
bool HostClipHasImage(int *width, int *height);
std::vector<uint8_t> HostClipGetImageRGBA(int *width, int *height);
```

Implemented in `src/platform/common/clipboard.cpp` using SDL3:

- `SDL_HasClipboardData("image/png")` to detect
- `SDL_GetClipboardData("image/png", &size)` to read PNG bytes
- `stbi_load_from_memory()` to decode to RGBA
- `stbi_info_from_memory()` to get dimensions without full decode

### 4.6 stb_image implementation

`stb_image.h` is already in `libs/stb/`.  Add `STB_IMAGE_IMPLEMENTATION`
alongside the existing `STB_IMAGE_WRITE_IMPLEMENTATION` in
`src/platform/stb_impl.cpp`:

```cpp
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
```

---

## 5. Data Flow

### Guest → Host (PICT Export)

```
Mac App copies image
  → Scrap gets PICT + possibly TEXT
  → ScrapCount increments
  → INIT detects change in SyncClipboard()
  → INIT calls GetScrap('PICT', ...) → PicHandle
  → INIT reads picFrame from PICT header
  → INIT checks ScreenDepth()
  → depth == 1:
      → NewPtr for BitMap buffer
      → Fill white, DrawPicture, reg PictExport(pass=0)
      → Fill black, DrawPicture, reg PictExport(pass=1)
      → DisposPtr
  → depth > 1:
      → NewGWorld(32, picFrame)
      → EraseRect (white), DrawPicture, reg PictExport(pass=0)
      → BackColor(black), EraseRect, DrawPicture, reg PictExport(pass=1)
      → DisposeGWorld
  → Host reads BitMap/PixMap struct + pixel data from guest RAM
  → Host composites alpha from two passes
  → Host encodes RGBA PNG via stb_image_write
  → Host places on clipboard via SDL_SetClipboardData("image/png")
```

### Host → Guest (PICT Import)

```
User copies image on host
  → ClipSeqNo bumps (host detects image on clipboard)
  → INIT polls ClipSeqNo, sees change
  → INIT calls PictHasImage → gets hasImage, width, height
  → INIT checks ScreenDepth()
  → depth == 1:
      → NewPtr for 1-bit buffer
      → reg PictImport(addr, rowBytes, 1, w, h)
      → Host decodes PNG, thresholds to 1-bit, writes to guest RAM
  → depth > 1:
      → NewGWorld(32, rect)
      → LockPixels, get baseAddr + rowBytes
      → reg PictImport(baseAddr, rowBytes, 32, w, h)
      → Host decodes PNG to XRGB, writes to guest RAM
  → INIT does OpenPicture/CopyBits/ClosePicture → PicHandle
  → ZeroScrap + PutScrap('PICT', pic)
```

---

## 6. Build Integration

### New source files

| File | Purpose |
|------|---------|
| `src/core/extn_clip_pict.cpp` | PICT command handlers |
| `src/core/extn_clip_pict.h` | Public interface |

### CMakeLists.txt changes

Add `src/core/extn_clip_pict.cpp` to the `MAXIVMAC_SOURCES` list.

No new dependencies — `stb_image.h` and `stb_image_write.h` already
in `libs/stb/`, already on the include path.

### Guest-side build

No CMake changes.  `macsrc/init/pict.c` is compiled with THINK C
and linked into the INIT code resource (manual build on guest).

---

## 7. Testing

### Host-side unit tests (doctest)

New file: `test/test_clip_pict.cpp`

| Test | Description |
|------|-------------|
| Composite1Bit basic | White-on-white → opaque white, black-on-black → opaque black |
| Composite1Bit transparent | White-on-white-pass, black-on-black-pass → transparent |
| Composite32Bit opaque | Same color both passes → opaque, full color |
| Composite32Bit transparent | White/black differs → correct alpha |
| ReadBitMapFromGuest | Parse synthetic BitMap struct bytes |
| WritePixels1Bit | RGBA → 1-bit threshold packing |
| WritePixels32Bit | RGBA → XRGB Mac layout |
| PNG roundtrip | Encode RGBA → PNG → decode → compare |

### Manual integration tests

| Test | Steps |
|------|-------|
| MacPaint B&W export | Copy selection in MacPaint → paste in host app |
| HyperCard export | Copy card art → verify transparency on host |
| Photoshop 1.0 export (Mac II) | Copy 24-bit image → verify color PNG on host |
| Host→Mac 1-bit import | Copy screenshot on host → paste in MacPaint |
| Host→Mac 32-bit import | Copy photo on host → paste in guest color app |
| TEXT+PICT combo | Copy styled text → verify both on host clipboard |

---

## 8. Phasing

| Phase | Scope | Shippable? |
|-------|-------|------------|
| 1 | Guest→Host, 1-bit (compact Macs): PictExport + BitMap path + alpha + PNG | Yes |
| 2 | Guest→Host, 32-bit (CQD): GWorld path, same export flow | Yes |
| 3 | Host→Guest: PictHasImage + PictImport + unified ClipSeqNo + GWorld import | Yes |
| 4 | Version bump to 3 + doc updates | Yes |

---

## 9. Source Files (final)

| File | Purpose |
|------|---------|
| `src/core/extn_clip_pict.cpp` | PICT command handlers (host) |
| `src/core/extn_clip_pict.h` | Public interface |
| `src/core/extn_clip.cpp` | Dispatch + version bump + unified SeqNo (modified) |
| `src/platform/common/clipboard.cpp` | Image clipboard query/decode (modified) |
| `src/platform/common/clipboard.h` | New image functions (modified) |
| `src/platform/stb_impl.cpp` | Add STB_IMAGE_IMPLEMENTATION (modified) |
| `macsrc/init/pict.c` | PICT export/import guest code (new) |
| `macsrc/init/clip.c` | Call PICT export/import from sync loop (modified) |
| `macsrc/init/defs.h` | New command constants (modified) |
| `test/test_clip_pict.cpp` | Host-side unit tests (new) |
