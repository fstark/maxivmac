# VIDEO_PLAN — Screen Conversion Pipeline Rewrite

Implements the architectural changes described in VIDEO.md §11.

## Goals

1. **Remove Scale from the converter** — destination is always native
   Mac resolution; GL handles display magnification.
2. **Unify indexed and direct-colour conversion** into a single template
   parameterised on source depth only; destination is always ARGB8888.
3. **Remove rect parameters from the converter** — always convert the
   full screen.  The fullscreen-viewport clip moves up to the caller
   (it clips the memcpy into `argbBuffer_`, not the conversion itself).
4. **Add runtime GL filter toggle** (GL_NEAREST / GL_LINEAR) as a
   first step toward pluggable video filters.
5. **Fix bugs B1–B5** as part of the rewrite (they become impossible
   in the new structure).

## Non-goals

* Multi-depth runtime switching (D4 in VIDEO.md) — future work.
* Gamma support — untouched.
* PRAM persistence — untouched.
* Compact Mac colour support — untouched.
* GPU-side format conversion (pixel shaders).  The CPU-side conversion
  to ARGB8888 is deliberate: the buffer is tiny (~1 MB), the conversion
  is sub-millisecond, and having a canonical `uint32_t*` buffer enables
  any future code that reasons about screen content (screenshots, tests,
  clipboard, overlays) without understanding 6 Mac framebuffer formats.
  The split is: CPU owns format conversion, GPU owns presentation
  (scaling, filtering, future CRT effects).

---

## Phase 1 — Add GL filter toggle

Adds a runtime switching mechanism for the GL texture filter.
No conversion changes yet; everything else still works as-is.

### 1.1 Add filter setting to ImGuiBackend

In `imgui_backend.h`, add:

```cpp
enum class TextureFilter { Nearest, Linear };
TextureFilter textureFilter_ = TextureFilter::Linear;
```

Add public method:

```cpp
void setTextureFilter(TextureFilter f);
TextureFilter textureFilter() const;
```

### 1.2 Apply filter in uploadFramebuffer()

In `imgui_backend.cpp`, `uploadFramebuffer()` currently hard-codes
`GL_LINEAR`.  Change to read from `textureFilter_`:

```cpp
GLenum glFilter = (textureFilter_ == TextureFilter::Nearest)
    ? GL_NEAREST : GL_LINEAR;
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glFilter);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glFilter);
```

Same in `createWindow()` for the initial texture creation.

### 1.3 Wire into UI

Add the filter toggle to the **Display tab** of the Ctrl overlay
(`ControlOverlay::drawDisplayTab()` in `imgui_overlay.cpp`), alongside
the existing Zoom and Fullscreen controls:

```
Display tab:
  Zoom:  (1x) (2x)
  Filter: (Nearest) (Linear)
  [Fullscreen]
```

Use `ImGui::RadioButton`.  The overlay needs a pointer/reference to the
`ImGuiBackend` (or just the filter getter/setter) to read and write the
setting.

The developer-mode menu bar (`drawMenuBar()`) is not the right place —
this is an end-user display preference.

### 1.4 Test

* Boot Mac II with colour.  Toggle filter.  Verify visual difference
  (smooth vs. crisp at non-integer scale).
* Boot Mac Plus.  Same test.

### Gate: build + regression tests pass.

---

## Phase 2 — Simplify CLUT to a flat uint32_t palette

The current `BuildClutTable` pre-expands every possible byte value
into `PixPerByte × 4` destination bytes.  This couples the CLUT to
the destination format and byte unpacking.

### 2.1 New palette: `uint32_t clut32[256]`

Add to `DisplayState`:

```cpp
uint32_t clut32[256] = {};
```

### 2.2 New `BuildPalette()` function

Replace `BuildClutTable()` with `BuildPalette()`.  It fills `clut32`
from the CLUT_reds/greens/blues arrays.  For B&W mode:

```cpp
clut32[0] = 0xFFFFFFFF;  // white
clut32[1] = 0xFF000000;  // black
```

For colour mode (depth 1–3):

```cpp
for (int i = 0; i < (1 << (1 << depth)); ++i)
    clut32[i] = 0xFF000000 | (r[i] << 16) | (g[i] << 8) | b[i];
```

No byte-unpacking, no bpp parameter, no CLUT_final buffer.

### 2.3 Keep `CLUT_final` and `BuildClutTable` alive (temporarily)

Don't delete the old path yet — Phase 2 only adds the new palette.
The old `ConvertRect` still uses `CLUT_final`.  Phase 3 replaces it.

### Gate: build + regression tests pass (no behaviour change).

---

## Phase 3 — New unified converter

Replace `ScreenMapConvert`, `ConvertRect`, and `ConvertRectSlow` with
a single function.

### 3.1 New function signature

```cpp
void ConvertScreen(
    const uint8_t* src,     // Mac framebuffer (screenCompareBuff)
    uint32_t*      dst,     // argbBuffer_ (ARGB8888)
    const uint32_t* palette, // clut32 (or nullptr for direct-colour)
    int depth,              // vMacScreenDepth (0–5)
    int width,              // vMacScreenWidth
    int height              // vMacScreenHeight
);
```

No rect, no bpp, no Scale, no DstDepth.

### 3.2 Inner loop — indexed colour (depth 0–3)

```
srcStride  = width >> (3 - depth)       // bytes per source scanline
pixPerByte = 1 << (3 - depth)           // pixels packed in one byte
pixelMask  = (1 << (1 << depth)) - 1    // bit mask for one pixel index

for each row:
    for each source byte:
        val = *pSrc++
        for k = (pixPerByte - 1) downto 0:
            index = (val >> (k << depth)) & pixelMask
            *pDst++ = palette[index]
```

This naturally handles all four indexed depths:

| Depth | pixPerByte | pixelMask | shift per pixel |
|:---:|:---:|:---:|:---:|
| 0 | 8 | 1 | 1 bit |
| 1 | 4 | 3 | 2 bits |
| 2 | 2 | 15 | 4 bits |
| 3 | 1 | 255 | 8 bits (trivial) |

### 3.3 Inner loop — direct colour (depth 4–5)

Depth 4 (16 bpp, big-endian 5-5-5):

```
for each pixel:
    uint16_t rgb = (pSrc[0] << 8) | pSrc[1]
    r = ((rgb >> 10) & 0x1F) * 255 / 31
    g = ((rgb >>  5) & 0x1F) * 255 / 31
    b = ((rgb >>  0) & 0x1F) * 255 / 31
    *pDst++ = 0xFF000000 | (r << 16) | (g << 8) | b
    pSrc += 2
```

Depth 5 (32 bpp, big-endian xRGB):

```
for each pixel:
    *pDst++ = 0xFF000000 | (pSrc[1] << 16) | (pSrc[2] << 8) | pSrc[3]
    pSrc += 4
```

### 3.4 Implementation options

**Option A — runtime switch on depth:**

A single function with `switch (depth)` selecting the inner loop.
Simple, no templates.  The compiler may auto-vectorise the depth-3 and
depth-5 cases.

**Option B — template on depth:**

```cpp
template<int Depth> void ConvertScreen(...);
```

with a runtime dispatcher:

```cpp
switch (vMacScreenDepth) {
    case 0: ConvertScreen<0>(src, dst, pal, w, h); break;
    case 1: ConvertScreen<1>(src, dst, pal, w, h); break;
    ...
}
```

This lets the compiler fully unroll the inner pixel-extraction loop
(pixPerByte, mask, shift are all compile-time constants).

Recommend **Option B** — it matches the current approach but with much
less complexity.

### 3.5 Wire into emulator_shell.cpp

`convertFramebuffer()` becomes:

```cpp
void EmulatorShell::convertFramebuffer()
{
    int depth = (vMacScreenDepth != 0 && display_.useColorMode)
                    ? vMacScreenDepth : 0;
    const uint32_t* pal = (depth < 4) ? display_.clut32 : nullptr;

    ConvertScreen(g_screenCompareBuff,
        reinterpret_cast<uint32_t*>(argbBuffer_),
        pal, depth, vMacScreenWidth, vMacScreenHeight);
}
```

`drawChangesAndClear()` calls `BuildPalette()` when
`g_colorMappingChanged`, then `convertFramebuffer()`.

### 3.6 Fullscreen viewport

The fullscreen-viewport clipping (the old rect logic in
`convertFramebuffer()`) is removed entirely from the conversion
pipeline.  The converter always fills the complete `argbBuffer_` at
native Mac resolution.

Viewport clipping is an OpenGL/ImGui concern: the presentation layer
chooses which portion of the texture to display (via UV coordinates,
image offset, or scissor rect).  This is already how `drawViewportWindowed()`
and `displayEmulatorImage()` work — they select what to show from the
full texture.

The `useFullScreen_` rect-clipping code in `convertFramebuffer()` is
deleted along with the `top`/`left`/`bottom`/`right` parameters.

### Gate: build + regression tests pass.  Visually verify:
* Mac Plus B&W
* Mac II 256-colour
* Mac II x2 (2 bpp) — was previously broken by B1
* Mac II x4 (4 bpp) — was previously broken by B1
* Mac II x16 (16 bpp) — was previously broken by B3
* Mac II x32 (32 bpp) — was previously broken by B3

---

## Phase 4 — Delete dead code

### 4.1 Remove old files / functions

* Delete `ScreenMapConvert` template (`screen_map.h`)
* Delete `screen_map_inst.h`
* Delete `BuildClutTable()`, `ConvertRect()`, `ConvertRectSlow()`
* Delete `CLUT_final` buffer from `DisplayState` and `CLUT_FINAL_SZ`
* Delete `scalingBuff` pointer from `DisplayState`
* Remove `bpp` parameter from everywhere
* Remove `screen_convert.h` declarations for deleted functions

### 4.2 Move remaining code

`BuildPalette()` and `ConvertScreen()` can live in `screen_convert.cpp`
or a new `screen_convert.cpp` if the old one is gutted enough.

### 4.3 Clean up screen_convert.h

Update to declare only `BuildPalette()` and `ConvertScreen()`.

### Gate: build + regression tests pass.

---

## Phase 5 — Fix remaining bugs

Some bugs are already fixed by the rewrite (B1, B5, B6).  Handle the
rest:

### 5.1 B2 — parseScreenSpec validation

Reject non-power-of-2 depth values in `parseScreenSpec()`.
(Already fixed in working tree — verify it's present.)

### 5.2 B3 — VRAM auto-sizing for Mac II

Grow `vidMemSize` when configured depth overflows the default 512 KB.
(Already fixed in working tree — verify it's present.)

### 5.3 B4 — PRAM for direct-colour depths

Set `PRAM[0x48] = 0x81` for depths 4 and 5.  Test with System 7 to
verify the Monitors control panel offers the colour option.

### Gate: build + regression tests pass.  Manual test of x16 and x32
booting to colour.

---

## Summary of what each phase removes

| Phase | Removes |
|:---:|---|
| 1 | — (additive only) |
| 2 | — (additive only) |
| 3 | `ConvertRect`, `ConvertRectSlow`, rect params, `bpp` plumbing |
| 4 | `ScreenMapConvert`, `screen_map.h`, `screen_map_inst.h`, `CLUT_final`, `scalingBuff`, `BuildClutTable`, old `screen_convert.h` |
| 5 | Remaining bug workarounds |

## File impact

| File | Change |
|---|---|
| `src/platform/common/screen_map.h` | **deleted** (Phase 4) |
| `src/platform/screen_map_inst.h` | **deleted** (Phase 4) |
| `src/platform/screen_convert.cpp` | Rewritten: `BuildPalette` + `ConvertScreen` |
| `src/platform/screen_convert.h` | Simplified to new API |
| `src/platform/display_state.h` | Remove `clutFinal`, `scalingBuff`; add `clut32[256]` |
| `src/platform/emulator_shell.cpp` | Simplified `convertFramebuffer()` |
| `src/platform/emulator_shell.h` | Remove rect params from `convertFramebuffer` |
| `src/platform/imgui_backend.cpp` | GL filter toggle, filter menu |
| `src/platform/imgui_backend.h` | `TextureFilter` enum, setter/getter |
| `src/core/config_loader.cpp` | Depth validation (Phase 5, if not already present) |
| `src/devices/rtc.cpp` | PRAM fix for direct-colour (Phase 5) |
