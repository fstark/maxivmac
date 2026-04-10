# VIDEO_PLAN — Resolution Switching

## Prerequisites (implemented)

The NuBus multi-mode video card is fully implemented (commits 317ccc8
through 4ec0e01).  It supports depths 1–32 bpp, runtime depth
switching via Monitors CP, and boots at 8 bpp when max depth ≥ 16 bpp.

### Reference sources

* "Designing Cards and Drivers for Macintosh II and Macintosh SE", ch. 8
* Basilisk II `slot_rom.cpp` and `video.cpp`
* Apple Tech Note DV 07 — Video SResource Changes for System Software 7.0
* Inside Macintosh: Devices ch. 6 (video drivers)

### Mode numbering

| Mode ID | Depth | bpp | Type | devType |
|:---:|:---:|:---:|---|:---:|
| 0x80 | 0 | 1 | Monochrome | 0 (CLUT) |
| 0x81 | 1 | 2 | 4-colour indexed | 0 |
| 0x82 | 2 | 4 | 16-colour indexed | 0 |
| 0x83 | 3 | 8 | 256-colour indexed | 0 |
| 0x84 | 4 | 16 | Direct 5-5-5 | 2 (direct) |
| 0x85 | 5 | 32 | Direct xRGB | 2 (direct) |

### VPBlock per mode

| Field | 1 bpp | 2 bpp | 4 bpp | 8 bpp | 16 bpp | 32 bpp |
|---|:---:|:---:|:---:|:---:|:---:|:---:|
| rowBytes | W/8 | W/4 | W/2 | W | W×2 | W×4 |
| pixelType | 0 | 0 | 0 | 0 | 0x10 | 0x10 |
| pixelSize | 1 | 2 | 4 | 8 | 16 | 32 |
| cmpCount | 1 | 1 | 1 | 1 | 3 | 3 |
| cmpSize | 1 | 2 | 4 | 8 | 5 | 8 |
| devType | 0 | 0 | 0 | 0 | 2 | 2 |

**Note:** `physBlockSize` (0x2E) is a ROM sBlock header — written to the
slot ROM but **not** part of the guest VPBlock struct.

### Boot depth vs max depth

Direct color depths (16/32 bpp) require 32-Bit QuickDraw (System 7.0+).
The card boots at 8 bpp when max depth ≥ 4 to avoid a hang inherited
from the original Mini vMac.  The user switches via Monitors CP after boot.

### ASan/UBSan testing

The `macos-asan` preset enables AddressSanitizer + UBSan:
```
cmake --build --preset macos-asan
./bld/macos-asan/maxivmac --model=MacII 608.hfs --screen=640x480x16
```
No memory errors in the video pipeline.  Pre-existing UBSan findings
in CPU emulation (null-pointer offset, integer overflow, shift
exponent) are unrelated.

---

## Goal

The video card advertises multiple resolutions (displayModeIDs).  The
user can switch between them at runtime via the Monitors / Displays
control panel without rebooting the guest.  This requires Display
Manager 2.0 (System 7.1.2+, or System 7.1 with the Display Enabler
2.0 extension).

Older Systems (6.x–7.1) see multiple resolutions in Monitors CP but
require a reboot to apply.  The card must handle both paths.

## Resolution table

The card advertises a fixed set of standard Macintosh resolutions
plus up to two host-derived resolutions.

### Classic resolutions (always present)

| displayModeID | Width | Height | Notes |
|:---:|:---:|:---:|---|
| 1 | 512 | 342 | Compact Mac native |
| 2 | 512 | 384 | Classic VGA-ish |
| 3 | 640 | 480 | Mac II default, VGA |
| 4 | 832 | 624 | 16" Apple RGB |
| 5 | 1024 | 768 | Apple 21" / XGA |
| 6 | 1152 | 870 | Apple Two-Page (21") |

### Host-derived resolutions (detected at startup)

| displayModeID | Width | Height | Notes |
|:---:|:---:|:---:|---|
| 100 | host W | host H | Host desktop size — for 1:1 fullscreen |
| 101 | host W/2 | host H/2 | Half desktop — usable windowed size |

Only added if they differ from the classic set (within a tolerance
of ±8 pixels on each axis).  If the host desktop is 2560×1440,
we'd add 2560×1440 (ID 100) and 1280×720 (ID 101).

### VRAM sizing

At startup, compute VRAM for the **largest** resolution at 32 bpp:

```
maxPixels = max(W × H) over all resolutions
vidMemSize = nextPowerOfTwo(maxPixels × 4)
```

For 2560×1440×32 bpp that's 14.7 MB → 16 MB.  Large but acceptable
on modern hardware.  Smaller host desktops (e.g. 1920×1080) yield
8 MB.

### ATT memory mapping

VRAM is exposed to the guest at `0xF9900000` via the ATT (Address
Translation Table).  The current code maps 1 MB windows:

| Address | Condition | Maps to |
|---|---|---|
| 0xF9900000 | always | g_vidMem + 0 |
| 0xF9A00000 | vidMemSize ≥ 2 MB | g_vidMem + 1 MB |
| 0xF9B00000 | vidMemSize ≥ 4 MB | g_vidMem + 2 MB |
| 0xF9C00000 | vidMemSize ≥ 4 MB | g_vidMem + 3 MB |

(See `machine.cpp` L1070–1098.)

For large host-derived resolutions (e.g. 2560×1440×32 bpp = 14.7 MB)
we'd need 15 banks.  The NuBus super-slot space for slot 9 is
`0xF9000000–0xF9FFFFFF` (16 MB), with ROM at `0xF9F00000`.  So the
maximum mappable VRAM is ~15 MB (addresses `0xF9000000–0xF9EFFFFF`),
which fits.  The ATT loop in `machine.cpp` must be generalized to map
`ceil(vidMemSize / 1MB)` banks starting at `0xF9000000` or `0xF9900000`.

**Cap:** If the host desktop would require > 15 MB of VRAM (e.g.
4K at 32 bpp = 33 MB), skip that host-derived resolution.  The
classic resolutions all fit comfortably (1152×870×32 bpp = 4 MB).

### Framerate note

Large resolutions at deep colour depths produce big framebuffers.
At 2560×1440×32 bpp, `ConvertScreen` processes 14.7 MB per frame.
This may affect framerate on low-end hardware.  Mitigation:

* The dirty-check (`ScreenFindChanges`) skips frames where VRAM
  hasn't changed — no conversion cost for static screens.
* The conversion loop is simple and cache-friendly; modern CPUs
  handle it in well under 1 ms even at 4K.
* If profiling shows a bottleneck, partial-screen dirty tracking
  (row-range granularity) can be added later.

## Guest-side changes

### GetNextResolution (csCode 17)

Currently returns a single resolution (displayModeID = 1).  Change to
iterate through the resolution table:

* `prevID = 0` → return first resolution
* `prevID = N` → return resolution at index N+1, or
  `kDisplayModeIDNoMoreResolutions` if N is the last

Each resolution entry returns its own `csMaxDepthMode` reflecting the
maximum depth that fits in VRAM at that resolution.

### GetVideoParameters (csCode 18)

Currently validates `displayModeID == 1`.  Change to look up the
resolution by displayModeID and return the appropriate VPBlock with
that resolution's width, height, and rowBytes.

### SwitchMode (control csCode 4 — Display Manager 2.0)

New control handler.  Display Manager 2.0 calls this instead of
`SetVidMode` when changing resolution+depth simultaneously.

**Input (VDSwitchInfoRec):**

| Offset | Size | Field |
|:---:|:---:|---|
| +0 | word | csMode (depth mode ID, e.g. 0x83) |
| +2 | long | csData (displayModeID, e.g. 3 for 640×480) |
| +6 | word | csPage (0) |
| +8 | long | csBaseAddr (output: VidBaseAddr) |

**Action:**
1. Look up resolution by `csData` (displayModeID) — return `paramErr`
   if not in the resolution table
2. Extract depth from `csMode - 0x80` — return `paramErr` if out of
   range or if `newW × newH × bpp / 8 > vidMemSize`
3. Update `s_currentWidth`, `s_currentHeight`, `s_currentDepth`,
   `s_currentDisplayModeID`
4. Write through to DisplayState: `g_screenWidth = newW`,
   `g_screenHeight = newH`, `g_screenDepth = newDepth`
5. Update `g_useColorMode` if depth changed
6. Reinitialize CLUT for new depth if switching to an indexed mode
7. Set a `s_resolutionChanged` flag for the host to pick up
8. Fill VRAM with gray pattern at new `rowBytes × height`
9. Return `noErr` with `csBaseAddr = VidBaseAddr`

**Note:** `SetVidMode` (csCode 2) remains for depth-only changes at
the current resolution.  `SwitchMode` handles both axes.

### GetCurrentMode (csCode 10)

Must return the current displayModeID (not hardcoded to 1).

### SavePreferredConfiguration (control csCode 16)

Must save both preferred depth and preferred displayModeID.

### PRAM

No PRAM changes needed — the video card is the source of truth for
available modes.  The System writes its own PRAM preferences
internally.

## Host-side changes

### Notification path

The guest's `SwitchMode` handler (video.cpp) sets the new width,
height, and depth in `DisplayState` (`g_screenWidth`, `g_screenHeight`,
`g_screenDepth`), then signals a resolution-change flag.  The
host picks this up on the next tick (emulator_shell.cpp), calls
`backend_->onResolutionChanged(newW, newH)`, and marks the screen
dirty so the next frame is a full redraw.

There is **no thread-safety concern** — everything runs on the same
thread (emulation tick → screen output → ImGui frame).

### Buffer pre-allocation

Allocate all buffers for the largest resolution at startup to avoid
runtime realloc:

```
maxW = max width over all resolutions
maxH = max height over all resolutions
screenCompareBuff = calloc(maxW * maxH * 4)   // 32 bpp worst case
argbBuffer_       = calloc(maxW * maxH * 4)
vidMemSize        = nextPowerOfTwo(maxW * maxH * 4)
```

Currently `display_.allocBuffers(vMacScreenNumBytes)` in
`EmulatorShell::allocMyMemory()` uses the boot resolution's byte
count.  Change to use `maxW × maxH × 4`.  Similarly,
`argbBuffer_ = calloc(vMacScreenWidth * vMacScreenHeight * 4, 1)`
must use the max dimensions.

After a resolution switch, no buffer realloc is needed — only the
active region within the pre-allocated buffer changes.

### Dirty tracking (ScreenFindChanges)

`ScreenFindChanges()` in `osglu_common.cpp` compares
`screencurrentbuff` (g_vidMem) against `g_screenCompareBuff` using
`vMacScreenHeight * vMacScreenByteWidth` as the byte count.  Since
these macros read live from `DisplayState`, they will automatically
reflect the new resolution after `SwitchMode` updates the fields.

**Edge case:** After a resolution switch, the compare buffer contains
stale data from the old resolution.  Must `memset(screenCompareBuff,
0xFF, newByteCount)` to force a full redraw on the first frame, as
`allocBuffers()` already does at init.

### ConvertScreen

`ConvertScreen(src, dst, pal, depth, width, height)` takes explicit
width/height parameters (from `vMacScreenWidth`/`vMacScreenHeight`).
It reads `width × height × bpp/8` bytes from `src` and writes
`width × height × 4` bytes to `dst`.  Both are within the
pre-allocated max-size buffers, so no changes needed to the
conversion routines themselves.

### GL texture recreation

`emuTexW_` / `emuTexH_` track the current GL texture size.  On
resolution change, the texture must be destroyed and recreated
because `glTexSubImage2D` cannot resize an existing texture.

```cpp
void ImGuiBackend::onResolutionChanged(uint16_t newW, uint16_t newH) {
    emuTexW_ = newW;
    emuTexH_ = newH;
    glDeleteTextures(1, &emuTextureId_);
    glGenTextures(1, &emuTextureId_);
    glBindTexture(GL_TEXTURE_2D, emuTextureId_);
    GLenum f = (textureFilter_ == TextureFilter::Nearest)
        ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, newW, newH, 0,
        GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, nullptr);
}
```

Do **not** tear down the SDL window or ImGui context — only the GL
texture changes.  The existing `destroyWindow()` path is too heavy.

### SDL window resize — per UI state

The window resize behavior depends on the current UI state:

**UIState::Windowed** — The SDL window is sized exactly to
`emuW × scale` (emulator_shell.cpp L727).  On resolution change:
* Call `SDL_SetWindowSize(window_, newW * windowScale_, newH * windowScale_)`
* The ImGui viewport fills the entire window (`drawViewportWindowed`
  sets `ImGui::SetNextWindowSize(displaySize)` and draws the image
  at `displaySize.x × displaySize.y`).
* The window title bar stays; no re-centering needed (the WM handles
  the resize in-place).

**UIState::Fullscreen** — The SDL window is fullscreen-desktop
(`SDL_WINDOW_FULLSCREEN`), so `SDL_SetWindowSize` is ignored.  On
resolution change:
* Do **nothing** to the SDL window.
* The `drawViewportFullscreen()` code already computes aspect-ratio
  preserving integer scaling from `emuTexW_`/`emuTexH_` into the
  display size, with letterboxing.  As long as `emuTexW_`/`emuTexH_`
  are updated, the next frame automatically scales + centers the
  new resolution.  A switch from 640×480 to 1024×768 just means a
  different integer scale is picked.

**UIState::Developer** — The SDL window is enlarged (80% of screen
or 1400×900).  On resolution change:
* Do **nothing** to the SDL window.
* The emulator viewport is an ImGui window ("Macintosh") with
  `AlwaysAutoResize` — it will auto-resize to fit the new
  `emuTexW_` × `emuTexH_` image via `displayEmulatorImage()`.
* If the new resolution is larger than the developer window, the
  ImGui panel may overflow.  But this is acceptable: the developer
  window is meant for debugging, and the user can resize it.

### Mouse coordinate mapping

Mouse events are translated from window-pixel space to emulator-pixel
space in `translateSdlEvent()` using `emuViewOriginX_/Y_` and
`emuViewW_/H_` (set by `displayEmulatorImage()` each frame).  The
formula:

```
ex = (wx - emuViewOriginX_) * emuTexW_ / emuViewW_
ey = (wy - emuViewOriginY_) * emuTexH_ / emuViewH_
```

Since `emuTexW_`/`emuTexH_` and `emuViewW_`/`emuViewH_` are updated
each frame by the ImGui drawing code, mouse mapping is automatically
correct after a resolution change.  No special handling needed.

### Headless backend

The headless backend (`headless_backend.h`) has no SDL window, no GL
texture, and no-op `createWindow()`.  `onResolutionChanged()` should
be a no-op.  `ConvertScreen` still runs (converting to `argbBuffer_`
that nobody reads), which is fine — it's used for golden-file testing
where resolution doesn't change mid-run.

### Fullscreen hint

When the guest switches to a resolution that matches the host desktop
size (displayModeID 100), the emulator could offer to go fullscreen.
However, auto-switching is potentially surprising.  Better options:

* **Status bar indicator:** show "Fullscreen available (F11)" when
  resolution matches host desktop.
* **Config flag:** `--auto-fullscreen` opt-in for users who want it.
* **Do nothing special:** the user can always press the fullscreen
  key.  The 1:1 pixel mapping will look correct because the
  resolution matches.

Defer the auto-fullscreen decision — the plumbing works either way.

## Slot ROM structure for multiple resolutions

The current ROM builds one video sResource with one mode entry per
depth.  For multiple resolutions, we need one **video functional
sResource per resolution**, each containing mode entries for all
depths at that resolution.  This is the standard Mac II pattern (see
"Designing Cards and Drivers", ch. 8.4: "Multiple timing modes").

Structure:
```
sResource Directory
  ├─ 0x01: Board sResource (unchanged)
  ├─ 0x80: Video sResource for Resolution 1 (512×342)
  │    ├─ sRsrcType: catDisplay/typVideo
  │    ├─ sRsrcName: "Display_Video_Apple_TFB"
  │    ├─ minorBase / minorLength
  │    ├─ mVidParams entries: 0x80..0x85 (one VPBlock per depth)
  │    └─ mPageCnt, mDevType per mode
  ├─ 0x81: Video sResource for Resolution 2 (512×384)
  │    └─ ... same structure, different VPBlock width/height ...
  ├─ 0x82: Video sResource for Resolution 3 (640×480)
  │    └─ ...
  └─ ... up to 0x87 (8 resolutions max)
```

Each sResource ID in the directory corresponds to a displayModeID.
The System's Slot Manager calls `sNextsRsrc` to enumerate them.

**ROM size:** 6 resolutions × 6 depths = 36 mode entries, plus 6
video sResources with headers.  Estimate ~3–4 KB.  The current
`vidROMSize` should be increased to **8 KB** to allow headroom for
host-derived resolutions.  The ROM is mapped at `0xF9F00000` with
its own ATT entry; the size is already configurable via
`cfg.vidROMSize`.

**VPBlock::forMode(depth, w, h)** is already parameterized by
width/height — no changes to VPBlock itself, just call it with
each resolution's dimensions.

## Implementation phases

### Phase A — Resolution table, VRAM sizing, and slot ROM

1. Define a `ResolutionEntry` struct: `{ uint32_t displayModeID;
   uint16_t width, height; }`
2. Build the resolution table at init (classic + host-derived),
   using `getDisplayBounds()` for host desktop size
3. Compute `vidMemSize` from largest resolution × 32 bpp
4. Generalize ATT mapping in `machine.cpp` to map
   `ceil(vidMemSize / 1MB)` banks
5. Build slot ROM with one video sResource per resolution, each
   containing all depth modes
6. Increase `vidROMSize` to 8 KB
7. Update `GetNextResolution` to iterate the table
8. Update `GetVideoParameters` to look up by displayModeID
9. Update `GetCurrentMode` to return current displayModeID
10. Pre-allocate `screenCompareBuff` and `argbBuffer_` for max
    resolution

**Test gate:** Boot Mac II in System 7.5.3, open Monitors CP →
Options → should show multiple resolution choices.  Verify slot ROM
parses without errors (check boot log for "vidROMSize too small").

### Phase B — SwitchMode and host resize

1. Implement `SwitchMode` (control csCode 4) in video.cpp
2. Add `s_resolutionChanged` flag, checked each tick in emulator_shell
3. Add `onResolutionChanged()` in `ImGuiBackend`: recreate GL
   texture (not the window/context), resize SDL window in Windowed
   mode only, no-op in Fullscreen/Developer
4. Add `onResolutionChanged()` no-op stub in headless backend
5. Reset `screenCompareBuff` to 0xFF on resolution change to force
   full first-frame redraw
6. Update `SavePreferredConfiguration` to save displayModeID

**Test gate:** Switch resolution in Monitors CP → screen should
resize live without reboot (System 7.5.3 with Display Manager 2.0).
Test all three UI states (Windowed, Fullscreen, Developer).

### Phase C — Polish and edge cases

1. Handle the reboot path for older Systems (pre-DM2): read PRAM
   at boot, select matching displayModeID
2. Cap host-derived resolutions to stay within 15 MB VRAM limit
3. Fullscreen hint when resolution matches host desktop
4. Test at multiple resolutions and depths
5. Update regression goldens if needed (boot resolution may change)
6. Run under ASan preset at several resolutions

## Files to modify

| File | Changes |
|---|---|
| `src/devices/video.cpp` | Resolution table, multi-res slot ROM, GetNextResolution loop, SwitchMode handler, s_resolutionChanged flag |
| `src/devices/video.h` | ResolutionEntry struct, resolution change notification |
| `src/devices/slot_rom.h` | No changes (VPBlock already parameterized) |
| `src/core/machine.cpp` | Generalize ATT VRAM mapping loop for > 4 MB |
| `src/core/config_loader.cpp` | VRAM sizing for max resolution, vidROMSize bump to 8 KB, host desktop query |
| `src/platform/emulator_shell.cpp` | Buffer pre-allocation for max res, tick-level resolution-change check, screenCompareBuff reset |
| `src/platform/imgui_backend.cpp` | `onResolutionChanged()`: GL texture recreate, SDL window resize (Windowed only) |
| `src/platform/imgui_backend.h` | Declare `onResolutionChanged()` |
| `src/platform/headless_backend.h` | No-op `onResolutionChanged()` stub |
| `src/platform/platform_backend.h` | Add `onResolutionChanged()` to virtual interface |
| `src/platform/display_state.h` | Max-resolution sizing in `allocBuffers()` |

## Risk assessment

| Risk | Impact | Mitigation |
|---|---|---|
| Large VRAM at high res (16 MB) | Memory pressure | Acceptable on modern hardware; compact Macs unaffected |
| Framerate at 4K×32 bpp | Slow conversion | Dirty-check skips unchanged frames; add row-range tracking if needed |
| Display Manager 2.0 protocol mismatch | Resolution switch fails | Test with System 7.5.3 (known DM2); fall back to reboot path |
| Slot ROM with many res×depth combos | ROM overflow | 6 res × 6 depths = 36 entries ≈ 3 KB; vidROMSize bumped to 8 KB |
| ATT banks for large VRAM | NuBus address space limit | Cap at 15 MB (slot 9 super-slot space); skip oversized host resolutions |
| Host desktop size changes mid-session | Stale resolution entries | Detect once at startup; dynamic re-detection is a stretch goal |
| Stale screenCompareBuff after switch | First frame shows garbage | memset to 0xFF on resolution change forces full redraw |
| Developer mode panel overflow | Emu image exceeds window | ImGui panel auto-resizes; user can manually resize developer window |
