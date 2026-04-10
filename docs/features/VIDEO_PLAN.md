# VIDEO_PLAN — NuBus Multi-Mode Video Card — COMPLETED

**Implementation:** 2026-04-10, commits 317ccc8 through current.

## Summary

All phases (0–5) implemented.  Additional post-implementation fixes
resolved several bugs discovered during testing with System 6.0.8
and System 7.5.3.

### Implementation commits

| Commit | Description |
|---|---|
| 317ccc8 | Main implementation: Phases 0–5 |
| 12e72dd | Fix VidReset handler — don't reset state on csCode 0 |
| 21b918b | Fix g_useColorMode not set during init for direct depths |
| (current) | Fix VPBlock guest write, boot depth cap, ASan preset |

### Files modified

| File | Changes |
|---|---|
| `src/devices/slot_rom.h` | **NEW:** `SlotROMWriter`, `VPBlock` |
| `src/devices/video.cpp` | Complete rewrite: multi-mode ROM, all status/control handlers, runtime depth switching |
| `src/devices/rtc.cpp` | PRAM[0x48] boot mode (capped at 8bpp for direct depths) |
| `src/core/config_loader.cpp` | vidMemSize auto-sizing for deep modes |
| `CMakeLists.txt` | `MAXIVMAC_SANITIZE` option (ASan + UBSan) |
| `CMakePresets.json` | `macos-asan` preset |
| `test/MacII.golden` | Re-recorded (4 times during development) |
| `test/MacIIx.golden` | Re-recorded (4 times during development) |

### Reference sources

* "Designing Cards and Drivers for Macintosh II and Macintosh SE", ch. 8
* Basilisk II `slot_rom.cpp` and `video.cpp`
* Apple Tech Note DV 07 — Video SResource Changes for System Software 7.0
* Inside Macintosh: Devices ch. 6 (video drivers)

---

## Architecture

### Mode numbering

| Mode ID | Depth | bpp | Type | devType |
|:---:|:---:|:---:|---|:---:|
| 0x80 | 0 | 1 | Monochrome | 0 (CLUT) |
| 0x81 | 1 | 2 | 4-colour indexed | 0 |
| 0x82 | 2 | 4 | 16-colour indexed | 0 |
| 0x83 | 3 | 8 | 256-colour indexed | 0 |
| 0x84 | 4 | 16 | Direct 5-5-5 | 2 (direct) |
| 0x85 | 5 | 32 | Direct xRGB | 2 (direct) |

`modeID = 0x80 + depth`.  The slot ROM contains modes from 0x80 up to
`0x80 + maxDepth` where `maxDepth` comes from `--screen=WxHxD`.

### Boot depth vs max depth

When `--screen` specifies a direct color depth (16bpp or 32bpp), the
card advertises all modes up to that depth but **boots at 8bpp**
(depth 3, the highest indexed mode).  This is necessary because:

* Direct color (Thousands/Millions) requires **32-Bit QuickDraw**,
  which is only available in System 7.0+ (or System 6 with the
  separate 32-Bit QuickDraw INIT)
* The System's display initialization calls status traps before
  QuickDraw is fully configured for direct color
* The original Mini vMac at depth 4/5 has the same boot hang — it has
  **never worked** for direct boot

The user can switch to Thousands or Millions via the Monitors control
panel after the desktop is up (System 7+ only).

### Display mode ID

Single resolution (timing mode), **displayModeID = 1**.
`GetNextResolution` returns this single entry, then
`kDisplayModeIDNoMoreResolutions` (0xFFFFFFFE).

### VPBlock per mode

| Field | 1 bpp | 2 bpp | 4 bpp | 8 bpp | 16 bpp | 32 bpp |
|---|:---:|:---:|:---:|:---:|:---:|:---:|
| rowBytes | W/8 | W/4 | W/2 | W | W×2 | W×4 |
| pixelType | 0 | 0 | 0 | 0 | 0x10 | 0x10 |
| pixelSize | 1 | 2 | 4 | 8 | 16 | 32 |
| cmpCount | 1 | 1 | 1 | 1 | 3 | 3 |
| cmpSize | 1 | 2 | 4 | 8 | 5 | 8 |
| devType | 0 | 0 | 0 | 0 | 2 | 2 |

**Important:** `physBlockSize` (0x2E) is a ROM sBlock header. It is
written to the slot ROM but is **not** part of the guest VPBlock struct.
`writeVPBlockToGuest()` must not include it (see Bugs Fixed below).

### VRAM sizing

The config loader auto-sizes `vidMemSize` to the next power of two
that fits `width × height × (1 << maxDepth) / 8`.

---

## Implemented status calls (kCmndVideoStatus)

| csCode | Name | Description |
|:---:|---|---|
| 2 | GetMode | Current mode ID, page 0, base address |
| 3 | GetEntries | Stub (logs abnormal) |
| 4 | GetPages | Always 1 page |
| 5 | GetPageAddr | VidBaseAddr (0xF9900000) |
| 6 | GetGray | Gray-tone flag |
| 8 | GetGamma | Stub (returns statusErr) |
| 9 | GetDefaultMode | Returns preferred depth mode |
| 10 | GetCurrentMode | Mode, displayModeID, page, base |
| 12 | GetConnection | kVGAConnect, kAllModes |
| 13 | GetModeTiming | Stub (returns statusErr) |
| 14 | GetModeBaseAddress | VidBaseAddr |
| 16 | GetPreferredConfiguration | Preferred mode + displayModeID |
| 17 | GetNextResolution | Enumerates single resolution |
| 18 | GetVideoParameters | VPBlock + page count for any mode |

## Implemented control calls (kCmndVideoControl)

| csCode | Name | Description |
|:---:|---|---|
| 0 | VidReset | Returns current mode (does NOT reset state) |
| 1 | KillIO | No-op, returns noErr |
| 2 | SetVidMode | Switches depth, updates CLUT |
| 3 | SetEntries | CLUT update for indexed modes, ignored for direct |
| 4 | SetGamma | Stub, returns noErr |
| 5 | GrayScreen | Fills VRAM with gray pattern |
| 6 | SetGray | Sets gray-tone flag |
| 9 | SetDefaultMode | Saves preferred depth |
| 16 | SavePreferredConfiguration | Saves preferred depth |

---

## Bugs found and fixed

### 1. VidReset handler resetting state (commit 12e72dd)

**Symptom:** Only 2 modes visible in Monitors CP.

The `vidReset()` function was being called from the csCode 0 handler,
which actively reset `s_currentDepth` to the preferred depth.  The Mac
ROM calls VidReset during display init — resetting state broke the
mode negotiation.  Fixed to just return the current mode passively,
matching the original code's behavior.

### 2. g_useColorMode not set during init (commit 21b918b)

**Symptom:** Black screen at 16bpp, then further investigation showed
white screen with green dots.

`VideoDevice::init()` set `s_currentDepth = maxDepth` but never set
`g_useColorMode = true`.  When the guest called `SetVidMode(0x84)`
matching the current depth, `Vid_SetMode` early-returned without
ever setting `g_useColorMode`.  The display pipeline stayed in B&W
mode.  Fixed by adding `g_useColorMode = (maxDepth > 0)` in init().

### 3. physBlockSize written to guest VPBlock (current)

**Symptom:** Monitors CP in System 7.5.3 shows empty mode list.

`writeVPBlockToGuest()` was writing the 46-byte VPBlock starting with
`physBlockSize` (0x2E), which is a ROM sBlock length header — not part
of the guest-side VPBlock struct.  This shifted every field by 4 bytes,
making the guest read garbage.  System 6 was unaffected because it
reads VPBlocks from the slot ROM directly (where `physBlockSize` is
correct as the sBlock header).  System 7's Display Manager calls
`GetVideoParameters` which uses `writeVPBlockToGuest()`.

Fixed by removing `physBlockSize` from the guest write.  The ROM
`VPBlock::writeTo()` still includes it (correct for the slot ROM
format).

### 4. Boot hang at direct color depths (inherited)

**Symptom:** White screen with rectangles when booting at 16bpp or
switching to Thousands in System 6.0.8.

This is an **inherited limitation from Mini vMac** — the reference
build at depth 4 has the same behavior.  Direct color (16bpp/32bpp)
requires 32-Bit QuickDraw, available only in System 7.0+.

**Workaround:** Boot at 8bpp (depth 3) when max depth >= 4.  The card
still advertises all modes; user switches via Monitors CP after boot.
Confirmed working in System 7.5.3.

### 5. CLUT not initialized for boot depth (current)

When `maxDepth >= 4`, the CLUT initialization was gated on
`maxDepth < 4` and was skipped.  But the boot depth is now 3 (8bpp),
which needs a CLUT.  Fixed to use `bootDepth` instead of `maxDepth`.

---

## Testing

### Automated

All 6 regression models pass: Classic, Mac512Ke, MacII, MacIIx,
MacPlus, MacSE.

### ASan/UBSan

New `macos-asan` preset enables AddressSanitizer + UBSan:
```
cmake --build --preset macos-asan
./bld/macos-asan/maxivmac --model=MacII 608.hfs --screen=640x480x16
```
No heap-buffer-overflow or use-after-free detected in the video
pipeline.  Pre-existing UBSan findings in CPU emulation code
(null pointer offset, integer overflow, shift exponent) are unrelated.

### Manual (verified)

| System | Depth | Result |
|---|---|---|
| 6.0.8 | 8bpp | Boots, 256 colors ✓ |
| 6.0.8 | 16bpp (switch) | Broken (no 32-Bit QD) — expected |
| 7.5.3 | 8bpp boot, card=16bpp | Boots 256 colors, Monitors shows B&W/4/16/256/Thousands ✓ |
| 7.5.3 | Switch to Thousands | Works ✓ |
| 7.5.3 | Switch depths via Monitors | Works for all indexed depths ✓ |
