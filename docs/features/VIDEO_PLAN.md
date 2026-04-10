# VIDEO_PLAN — NuBus Multi-Mode Video Card — COMPLETED

**Completed:** 2026-04-10, commit 317ccc8.

All phases (0–5) implemented in a single commit:
- Phase 0: SlotROMWriter + VPBlock replace PatchA* byte-patching
- Phase 1: Multi-mode slot ROM (all depths 0..maxDepth)
- Phase 2: All status calls (GetVideoParameters, GetNextResolution,
  GetCurrentMode, GetConnection, GetModeBaseAddress,
  GetDefaultMode, GetPreferredConfiguration)
- Phase 3: Runtime depth switching via Vid_SetMode()
- Phase 4: SetEntries for all indexed depths with clutSizeForDepth()
- Phase 5: PRAM boot mode, preference save, golden re-record

Files modified: slot_rom.h (new), video.cpp, rtc.cpp,
MacII.golden, MacIIx.golden.
Inside Macintosh: Devices ch. 6 (video drivers).

**Reference sources:**
* "Designing Cards and Drivers for Macintosh II and Macintosh SE", ch. 8
* Basilisk II `slot_rom.cpp` and `video.cpp`
* Apple Tech Note DV 07 — Video SResource Changes for System Software 7.0

## Current State

The video device (`src/devices/video.cpp`) builds a NuBus slot ROM with
exactly **two modes**: 0x80 (1 bpp mono) and 0x81 (colour at the single
depth passed via `--screen`).  Mode switching is a binary toggle.

Eight status/control calls are logged but unimplemented:
`GetCurrentMode`, `GetDefaultMode`, `GetConnection`,
`GetModeBaseAddress`, `GetPreferredConfiguration`,
`GetNextResolution`, `GetVideoParameters`, `SavePreferredConfiguration`.

This causes the Monitors control panel to see only two options and the
guest OS to crash when booting into 16 or 32 bpp (it calls
unimplemented status traps during display init).

The ROM builder uses raw byte-patching (`PatchAByte`, `PatchAWord`,
`PatchALong`) with hex-encoded ASCII strings and magic constants.
This is unreadable and error-prone.  Phase 0 replaces it with proper
C++ types before any functional changes.

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
`0x80 + maxDepth` where `maxDepth` is the value from `--screen`.

### Display mode ID

We have a single resolution (timing mode), referred to as
**displayModeID = 1**.  `GetNextResolution` returns this single entry.

### VPBlock per mode

Each mode's VPBlock (already present for modes 0x80 and 0x81) will be
generated for all modes:

| Field | 1 bpp | 2 bpp | 4 bpp | 8 bpp | 16 bpp | 32 bpp |
|---|:---:|:---:|:---:|:---:|:---:|:---:|
| rowBytes | W/8 | W/4 | W/2 | W | W×2 | W×4 |
| pixelType | 0 | 0 | 0 | 0 | 0x10 | 0x10 |
| pixelSize | 1 | 2 | 4 | 8 | 16 | 32 |
| cmpCount | 1 | 1 | 1 | 1 | 3 | 3 |
| cmpSize | 1 | 2 | 4 | 8 | 5 | 8 |
| devType | 0 | 0 | 0 | 0 | 2 | 2 |

### VRAM sizing

VRAM must be large enough for the deepest configured mode.  The config
loader already auto-sizes `vidMemSize` for the configured depth.

---

## Phase 0 — Slot ROM builder rewrite

**Goal:** Replace the unreadable `PatchAByte`/`PatchALong` byte-
patching mess with proper C++ types that serialize to big-endian.
No functional change — the resulting ROM image must be byte-identical
(except for vendor string changes).

### 0.1 Big-endian serialization

Create a `SlotROMWriter` class (in a new header `src/devices/slot_rom.h`)
that writes to a byte buffer in big-endian order:

```cpp
class SlotROMWriter {
public:
    SlotROMWriter(uint8_t* buf, size_t capacity);

    // Absolute position
    size_t pos() const;
    void seek(size_t offset);

    // Primitive writes (big-endian)
    void writeByte(uint8_t v);
    void writeWord(uint16_t v);
    void writeLong(uint32_t v);
    void writeBytes(const uint8_t* data, size_t len);

    // C string (null-terminated, padded to 4-byte alignment)
    void writeString(const char* s);

    // sResource list entries
    //   OSlst entry: ID + self-relative offset to target
    size_t reserve();               // reserve 4 bytes, return position
    void patchOffset(size_t reserved, uint8_t id);  // write ID + (current - reserved)
    //   DatLst entry: ID + 24-bit literal data
    void writeDataEntry(uint8_t id, uint32_t data);
    void writeEndOfList();          // 0xFF000000

    bool overflowed() const;
};
```

The key insight: `PatchAOSLstEntry` wrote `(ID << 24) | ((target - self) & 0x00FFFFFF)`.
`patchOffset` does the same thing but the target is implicitly "current position".
`reserve()` saves the position and advances by 4 bytes; `patchOffset()` fills it in later.

### 0.2 VPBlock struct

```cpp
struct VPBlock {
    uint32_t physBlockSize = 0x2E;  // always 46
    uint32_t baseOffset    = 0;
    uint16_t rowBytes;
    uint16_t boundsTop     = 0;
    uint16_t boundsLeft    = 0;
    uint16_t boundsBottom;          // height
    uint16_t boundsRight;           // width
    uint16_t version       = 0;
    uint16_t packType      = 0;
    uint32_t packSize      = 0;
    uint32_t hRes          = 0x00480000;  // 72 dpi
    uint32_t vRes          = 0x00480000;
    uint16_t pixelType;     // 0=indexed, 0x10=direct
    uint16_t pixelSize;     // 1,2,4,8,16,32
    uint16_t cmpCount;      // 1 or 3
    uint16_t cmpSize;       // 1,2,4,8 or 5
    uint32_t planeBytes    = 0;

    static VPBlock forMode(int depth, uint16_t width, uint16_t height);
    void writeTo(SlotROMWriter& w) const;
};
```

`VPBlock::forMode(depth, w, h)` encapsulates the depth→VPBlock
parameter logic that is currently scattered across the init code.
This same factory will be reused by `GetVideoParameters` (Phase 2)
and the slot ROM builder.

### 0.3 Rewrite `VideoDevice::init()`

Using the writer, `init()` becomes readable:

```cpp
bool VideoDevice::init() {
    SlotROMWriter w(g_vidROM, cfg.vidROMSize);

    // --- sResource directory ---
    auto sRsrcDir = w.pos();
    auto rBoard = w.reserve();
    auto rVideo = w.reserve();
    w.writeEndOfList();

    // --- Board sResource ---
    w.patchOffset(rBoard, 0x01);
    auto rBoardType = w.reserve();
    auto rBoardName = w.reserve();
    w.writeDataEntry(0x20, 0x00764D);  // BoardId: 'vM'
    auto rVendorInfo = w.reserve();
    w.writeEndOfList();

    w.patchOffset(rBoardType, 0x01);
    w.writeLong(0x00010000);    // catDisplay/typVideo
    w.writeLong(0x00000000);

    w.patchOffset(rBoardName, 0x02);
    w.writeString("maxivmac video card");

    // --- Vendor info ---
    w.patchOffset(rVendorInfo, 0x24);
    auto rVendorID = w.reserve();
    auto rRevLevel = w.reserve();
    auto rPartNum  = w.reserve();
    w.writeEndOfList();

    w.patchOffset(rVendorID, 0x01);
    w.writeString("maxivmac");

    w.patchOffset(rRevLevel, 0x03);
    w.writeString("2.0");

    w.patchOffset(rPartNum, 0x04);
    w.writeString("MVMv-1");

    // --- Video sResource ---
    w.patchOffset(rVideo, 0x80);
    // ... type, name, driver, mode entries in a loop ...
    for (int d = 0; d <= maxDepth; d++) {
        rModes[d] = w.reserve();          // mode 0x80+d
    }
    w.writeEndOfList();

    // ... emit VPBlock for each mode ...
    for (int d = 0; d <= maxDepth; d++) {
        w.patchOffset(rModes[d], 0x80 + d);
        auto rVP = w.reserve();
        w.writeDataEntry(0x03, 1);        // mVidParams = 1 page
        w.writeDataEntry(0x04, (d < 4) ? 0 : 2);  // mDevType
        w.writeEndOfList();

        w.patchOffset(rVP, 0x01);
        VPBlock::forMode(d, width, height).writeTo(w);
    }

    // ... driver, trailer, checksum ...
}
```

Compare this with the current code.  The string "maxivmac" replaces
"Paul C. Pratt" and "Mini vMac".

### 0.4 Delete dead code

Remove all `PatchA*` functions and the `ReservePatchOSLstEntry` /
`PatchAReservedOSLstEntry` global-pointer machinery.  The global
`pPatch` cursor is replaced by `SlotROMWriter`.

### 0.5 Guest driver binary

The embedded `VidDrvr_contents[]` stays as-is.  It is a pre-compiled
68020 binary (source in `extras/mydriver/video/firmware.a`) that acts
as a thin shim: Open/Close manage interrupt handlers, Control/Status
just forward to the emulator's extension trap.  The driver does **not**
filter modes or interpret csCode values — it passes the full PB
pointer through `TailData` to the host handler.  This means the binary
is mode-agnostic and needs no changes for multi-mode support.

The driver name string (`.Display_Video_Sample`) embedded in the binary
is visible to tools like ResEdit but not to the end user.  Renaming it
would require reassembling from the `.a` source.  This is a cosmetic
issue — defer to a later plan if desired.

### 0.6 Checksum

`ChecksumSlotROM()` stays the same — it operates on the final
`g_vidROM` buffer regardless of how it was built.

### Build & test gate

* `cmake --build --preset macos` — must compile
* `cd test && ./verify.sh` — all 6 models pass (ROM is byte-compatible
  except vendor string, which doesn't affect golden hashes since they
  test display output not ROM bytes)
* **Manual test:** boot Mac II normally, verify "About This Macintosh"
  or Monitors CP still works

---

## Phase 1 — Multi-mode slot ROM

**Goal:** Build slot ROM entries for all depths 0 through maxDepth.
Phase 0 already restructures `init()` with the mode loop.  This phase
is about verifying the multi-mode ROM actually works with the guest OS.

### 1.1 Emit modes 0x80 through 0x80+maxDepth

The Phase 0 rewrite emits the loop.  Phase 1 verifies the resulting
ROM image is accepted by the Slot Manager.  Each mode sResource entry
must contain:
* `mVidParams` (ID 0x01) → VPBlock
* `mPageCnt` (ID 0x03) → 1
* `mDevType` (ID 0x04) → 0 (CLUT) or 2 (direct)

### 1.2 vidROMSize check

More modes = more bytes.  Each mode adds ~60 bytes (entry + VPBlock).
6 modes ≈ 360 extra bytes.  Current `vidROMSize = 0x800` (2 KB) should
suffice, but verify the `UsedSoFar` check.

### 1.3 VRAM auto-sizing

Already done in config_loader.cpp — `vidMemSize` is rounded up to
the next power of two to fit `width × height × (1 << maxDepth) / 8`.
No changes needed.

### Build & test gate

* `cmake --build --preset macos` — must compile
* `cd test && ./verify.sh` — all 6 models pass
* **Manual test:** boot Mac II with `--screen=640x480x8`, open Monitors
  control panel → should list 1, 2, 4, and 8 bpp options in the
  colour depth selector.

---

## Phase 2 — Status calls (mode enumeration)

**Goal:** Implement the status csCode handlers that the guest OS and
Monitors control panel use to discover available modes.

### 2.1 GetVideoParameters (csCode 18)

The most important call.  The guest OS calls this during boot to get
the VPBlock for the current mode.

**Input (VDVideoParametersInfoRec):**

| Offset | Size | Field |
|:---:|:---:|---|
| +0 | long | csDisplayModeID (display mode = 1) |
| +4 | word | csDepthMode (mode ID, e.g. 0x83) |
| +6 | long | csVPBlockPtr (guest pointer to fill) |
| +10 | long | csPageCount (output) |

**Action:**
1. Validate `csDisplayModeID` = 1 (our only resolution)
2. Extract depth from `csDepthMode - 0x80`
3. Validate depth is in 0..maxDepth
4. Write VPBlock into the guest buffer at `csVPBlockPtr`
5. Write `csPageCount = 1` (single page)
6. Return `noErr`

Create a helper `writeVPBlock(uint32_t guestPtr, int depth)` that
writes the 46-byte VPBlock to guest memory, reusing the same
calculation as the slot ROM builder.

### 2.2 GetNextResolution (csCode 17)

Called by Monitors CP "Options" to enumerate resolutions.

**Input (VDResolutionInfoRec):**

| Offset | Size | Field |
|:---:|:---:|---|
| +0 | long | csPreviousDisplayModeID (input) |
| +4 | long | csRIDisplayModeID (output) |
| +8 | long | csHorizontalPixels (output) |
| +12 | long | csVerticalLines (output) |
| +16 | long | csRefreshRate (output, Fixed 16.16) |
| +20 | word | csMaxDepthMode (output) |

**Action:**
* If `csPreviousDisplayModeID` = 0 → "give me the first resolution"
  → return displayModeID = 1, width, height, 0x00420000 (≈67 Hz fixed),
  maxDepthMode = 0x80 + maxDepth
* If `csPreviousDisplayModeID` = 1 → "give me the next after 1"
  → return displayModeID = 0xFFFFFFFE (kDisplayModeIDNoMoreResolutions)
* Otherwise → return `paramErr`

### 2.3 GetCurrentMode (csCode 10)

**Input (VDSwitchInfoRec):**

| Offset | Size | Field |
|:---:|:---:|---|
| +0 | word | csMode (output: current mode ID) |
| +2 | long | csData (output: displayModeID) |
| +6 | word | csPage (output: 0) |
| +8 | long | csBaseAddr (output: VidBaseAddr) |

**Action:** Return current mode (0x80 + current depth), displayModeID = 1,
page = 0, baseAddr = 0xF9900000.

### 2.4 GetConnection (csCode 12)

**Input (VDDisplayConnectInfoRec):**

| Offset | Size | Field |
|:---:|:---:|---|
| +0 | word | csDisplayType |
| +2 | byte | csConnectTaggedType |
| +3 | byte | csConnectTaggedData |
| +4 | long | csConnectFlags |
| +8 | long | csDisplayComponent |
| +12 | long | csConnectReserved |

**Action:** Return a fixed-frequency built-in display:
* `csDisplayType = 6` (kVGAConnect — multi-sync display)
* `csConnectFlags = 0x000E` (kAllModes | kAllFlags)
* Others = 0

### 2.5 GetModeBaseAddress (csCode 14)

Same as GetPageAddr — return VidBaseAddr.  All modes share the same
VRAM base.

### 2.6 GetDefaultMode / GetPreferredConfiguration (csCode 9 / 16)

* **GetDefaultMode (status 9):** return saved default mode ID (or 0x80)
* **GetPreferredConfiguration (status 16):** return saved mode + displayModeID

Store the preferred depth in a static variable (not persisted to PRAM
for now).  Initial value = configured depth from `--screen`.

### Build & test gate

* Build + verify
* **Manual test 1:** Boot Mac II `--screen=640x480x8`.  Check dbglog
  for GetVideoParameters, GetCurrentMode calls being handled (no more
  "not handled yet" messages).
* **Manual test 2:** Open Monitors CP → Options → should show resolution
  and depth list.

---

## Phase 3 — Runtime depth switching

**Goal:** `SetVidMode` switches to any available depth, not just
mono ↔ colour toggle.

### 3.1 Enhance `Vid_SetMode()`

Currently toggles a bool `g_useColorMode`.  Replace with:

```cpp
static tMacErr Vid_SetMode(uint16_t modeID) {
    int newDepth = modeID - 0x80;
    if (newDepth < 0 || newDepth > maxDepth) return paramErr;
    if (newDepth == currentDepth) return noErr;

    // Update the live depth
    vMacScreenDepth = newDepth;
    g_useColorMode = (newDepth > 0);
    g_colorMappingChanged = true;

    // Fill screen with gray pattern for new depth
    FillScreenWithGrayPattern();
    return noErr;
}
```

### 3.2 Track current mode

Add a `static int s_currentDepth` (initialized from `screenDepth`).
`Vid_GetMode()` returns `0x80 + s_currentDepth`.

### 3.3 Adapt screenCompareBuff

`screenCompareBuff` is allocated once at init for `vMacScreenNumBytes`.
After a depth switch, `vMacScreenNumBytes` changes.

**Option A — Pre-allocate for max depth:**
Allocate `screenCompareBuff` as `width × height × 4` (deepest mode)
at init.  Safe but wastes memory at shallow depths.

**Option B — Reallocate on switch:**
`realloc()` + memset to 0xFF on mode change.

Prefer **Option A** — simpler, no realloc path, max waste is 1.2 MB
(640×480×4).

Similarly, `argbBuffer_` in EmulatorShell is already `width × height × 4`
regardless of depth — no change needed.

### 3.4 Update macros

`vMacScreenNumBytes`, `vMacScreenByteWidth` etc. are derived from
`vMacScreenDepth` via macros.  Since `vMacScreenDepth` points to
`DisplayState::screenDepth`, updating `screenDepth` automatically
updates the derived values.  Verify this is the case.

### 3.5 SetDefaultMode / SavePreferredConfiguration (control)

* **SetDefaultMode (control 9):** save the mode ID in `s_preferredDepth`
* **SavePreferredConfiguration (control 16):** save mode + displayModeID

### 3.6 Update `vidReset()`

Currently resets to mode 128 unconditionally.  Should reset to the
saved preferred mode, or at least to the configured default depth.

### Build & test gate

* Build + verify (regression tests are B&W / 8-bpp, should still pass)
* **Manual test 1:** Boot Mac II `--screen=640x480x32`.  If the crash
  is fixed by proper mode negotiation, the desktop should appear in
  32bpp.  If it still crashes, the issue is elsewhere (QuickDraw 32-bit
  addressing or similar).
* **Manual test 2:** Boot `--screen=640x480x8`, open Monitors CP,
  switch to 4-bpp (16 colours) → screen should redraw in 4-bpp mode.
  Switch to 1-bpp → should get B&W.  Switch back to 8-bpp → colour.
* **Manual test 3:** Boot `--screen=640x480x32`, switch depths in
  Monitors from 1→2→4→8→16→32 if possible.

---

## Phase 4 — SetEntries for all indexed depths

**Goal:** `SetEntries` currently only works when
`vMacScreenDepth > 0 && vMacScreenDepth < 4 && g_useColorMode`.  After
mode switching, it must work for whichever indexed depth is currently
active.

### 4.1 Fix SetEntries guard

Change from:
```cpp
if ((0 != vMacScreenDepth) && (vMacScreenDepth < 4) && g_useColorMode)
```
to:
```cpp
if (s_currentDepth > 0 && s_currentDepth < 4)
```

The `g_useColorMode` check is redundant — if we're in an indexed mode,
CLUT updates must work.

### 4.2 CLUT_size must track current depth

`CLUT_size` is `1 << (1 << depth)`.  Currently a macro derived from
the compile-time config depth.  After mode switching, it must reflect
`s_currentDepth`.  Either make it a function or compute inline.

### 4.3 White/black pinning

The current code pins index 0 to white and index `CLUT_size-1` to
black.  This is correct for Apple's CLUT convention.  Ensure it uses
the depth-appropriate CLUT_size.

### Build & test gate

* Build + verify
* **Manual test:** Boot 8-bpp, switch to 4-bpp, verify colours
  (not all black, CLUT populated correctly).

---

## Phase 5 — Polish and edge cases

### 5.1 PRAM depth preference

Currently `PRAM[0x48] = 0x81` for all non-B&W depths.  After mode
switching, the PRAM value should reflect the initial configured depth:
`PRAM[0x48] = 0x80 + configuredDepth`.

This tells the Mac ROM which mode to boot into.

### 5.2 Regression test update

If the Mac II now boots differently (different depth, different PRAM),
the golden file may change.  Re-record `test/MacII.golden` and
`test/MacIIx.golden` if needed.

### 5.3 GetGamma / SetGamma

Still stubs.  Not blocking, but log a note if the guest requests gamma.

### 5.4 Guest driver (VidDrvr) — no changes needed

The 68020 assembly source is in `extras/mydriver/video/firmware.a`.
The driver is a thin shim:
* **Open:** allocates private storage, installs VBL interrupt handler
* **Control/Status:** forwards the PB pointer to the host via the
  extension trap (`TailData` → `kCmndVideoControl`/`kCmndVideoStatus`)
* **Close:** removes interrupt handler, disposes storage

The driver does **not** interpret csCode values or filter modes — it
passes everything through.  This means the compiled binary is already
multi-mode compatible.  No reassembly needed.

The driver name `.Display_Video_Sample` in the binary is cosmetic.
Renaming it would require an MPW or Retro68 assembly workflow.  Not
worth the effort for this plan.

### Build & test gate

* Build + verify
* **Manual test:** full workflow — boot 8-bpp → Monitors CP → switch
  depths → close CP → verify preference saved → reboot → verify it
  remembers the depth.

---

## Risk assessment

| Risk | Impact | Mitigation |
|---|---|---|
| Slot ROM too large for vidROMSize | Won't boot | Check `UsedSoFar` after ROM build |
| Guest OS depends on mode ordering | Garbled display | Follow Apple convention: modes ascending by depth |
| Phase 0 ROM not byte-compatible | Regression fail | Compare hex dumps of old vs new ROM before/after |
| 16/32 bpp crash persists | Can't use direct colour | Isolate: if GetVideoParameters works but display still crashes, the issue is in QuickDraw 32-bit |
| screenCompareBuff size mismatch | Corruption | Pre-allocate for max depth (phase 3.3) |
| Regression test golden files change | Test failure | Re-record after verifying correct behaviour |

## Files to modify

| File | Changes |
|---|---|
| `src/devices/slot_rom.h` | **NEW** Phase 0: `SlotROMWriter`, `VPBlock` |
| `src/devices/video.cpp` | Phases 0–5: rewrite init(), status/control handlers, mode state |
| `src/devices/video.h` | Minor: possibly expose mode query for overlay |
| `src/platform/display_state.h` | Phase 3: allocBuffers for max depth |
| `src/platform/platform.h` | Phase 3: verify CLUT_size macro adapts to runtime depth |
| `src/devices/rtc.cpp` | Phase 5: PRAM boot mode |
| `test/MacII.golden` | Phase 5: re-record if boot sequence changes |
| `test/MacIIx.golden` | Phase 5: re-record if boot sequence changes |
