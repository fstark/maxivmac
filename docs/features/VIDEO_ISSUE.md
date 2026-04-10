# VIDEO_ISSUE — Slot ROM VPBlock Patching Hack

## Summary

Resolution switching via Display Manager 2.0 requires mutating the
slot declaration ROM buffer (`g_vidROM`) at runtime to update VPBlock
entries with the new resolution's geometry.  Without this, the guest
GDevice retains the boot resolution's pixMap bounds and rowBytes after
a SwitchMode call, causing QuickDraw to paint into the wrong region
of VRAM (garbled/duplicated display).

This is a well-documented emulator workaround — Basilisk II uses
the same technique — but it is architecturally wrong and should be
replaced with a proper solution.

## Background

### How real NuBus video cards handled multiple resolutions

Real cards used **multiple video functional sResources** in their
declaration ROM, one per supported resolution.  Each sResource
contained correct VPBlock entries for that resolution at every
supported depth.  Two runtime selection mechanisms existed:

1. **Sense pins (pre-DM2):** The DB-15 monitor connector had 3 sense
   pins.  The card firmware read these at init to determine which
   monitor was attached and which sResource to activate.  The Slot
   Manager picked the matching sResource at boot.

2. **Display Manager 2.0 (System 7.1.2+):** The DM2 protocol
   (`GetNextResolution`, `GetVideoParameters`, `SwitchMode` etc.)
   allowed live resolution switching.  Cards still had multiple
   sResources; the DM2 could switch between them or use the driver
   protocol to query capabilities.

In both cases, the ROM was genuinely read-only.  Each resolution had
its own pre-built VPBlock data.

### What happens in maxivmac

Our slot ROM has a **single video functional sResource** (ID 0x80)
with VPBlocks for the boot resolution only.  This was chosen because
having multiple video sResources caused the System to treat each as
a separate physical display, freezing during boot (see "Boot freeze"
below).

When the guest calls SwitchMode to change resolution, the DM2 reads
the slot ROM via Slot Manager (`SReadStruct` / `SReadBlock`) to
rebuild the GDevice's pixMap.  Since our ROM still contains the boot
resolution's VPBlocks, the GDevice doesn't update — QuickDraw
continues rendering at the old resolution into a framebuffer sized
for the new one, producing garbled output.

The workaround: `patchSlotROMVPBlocks()` overwrites the VPBlock
entries in `g_vidROM` with the new resolution's geometry and
recomputes the ROM checksum.  The host buffer is mapped read-only
to the guest, but the host can write to it freely.

## Observed symptoms before the fix

Starting at 512×342, switching to 1024×768 via Monitors CP → Options:

- SDL window resized to 1024×768 ✓
- Guest desktop remained 512×342, rendering as two small copies in
  the top half of the larger window
- `GetCurrentMode` correctly reported `displayModeID=5` (1024×768)
- `GetVideoParameters` correctly returned 1024×768 geometry
- Despite correct driver responses, the GDevice was not updated

This confirms the System re-reads VPBlocks from the ROM, not from
the driver's status call responses.

## The boot freeze problem

An earlier attempt used multiple video sResources (one per
resolution) — the approach real cards used.  This caused the System
to treat each sResource as a separate physical display.  During boot,
the startup code tried to initialise all "displays" (draw gray
desktop on each, position menu bar, etc.), which doesn't work with a
single shared framebuffer.  The result was an infinite loop during
the boot panel display phase.

The single-sResource approach avoids this but creates the VPBlock
patching requirement.

## Research needed

### R1 — sResource flags for non-boot resolutions

Real multi-resolution cards had multiple video sResources but only
one was active at boot.  The others may have been marked with
specific flags to prevent the System from treating them as active
displays.  Candidates:

- **`fOpenAtStart` (bit 1)** — only the boot sResource should have
  this set
- **`f32BitMode` (bit 2)** — for 32-bit address space cards
- **sRsrcFlags** field in the sResource header

If non-boot sResources can be present in the directory without being
initialised as displays, the boot freeze would not occur and the ROM
patching hack could be eliminated.

### R2 — DM2 source of VPBlock data

It's unclear whether System 7.5.x's Display Manager *always* reads
VPBlocks from the ROM, or whether this only happens in certain code
paths (e.g. after reboot vs live SwitchMode).  Possibilities:

- DM2 might use `GetVideoParameters` (status csCode 18) for the
  active mode but fall back to ROM for non-active modes
- DM2 might use ROM VPBlocks only during boot/initialisation and
  driver responses thereafter
- The Slot Manager's internal caches might retain stale VPBlock data
  after SwitchMode

Disassembling the System 7.5.3 Display Manager extension could
clarify this.

### R3 — Basilisk II's approach

Basilisk II uses the same ROM-patching technique.  Their
implementation in `video_macos.cpp` / `slot_rom.cpp` patches
VPBlock entries on mode switch.  Worth studying:

- Do they use multiple sResources or single?
- How do they handle the checksum?
- Do they have a better structural solution in later versions?

### R4 — SwitchMode and GDevice rebuild sequence

The observed DM2 call sequence after SwitchMode was:

```
SwitchMode mode=0x85 modeID=5 → OK (1024x768)
SetVidMode → mode=0x85
GrayScreen
SetGray
GetCurrentMode → displayModeID=5  ✓
GetModeTiming → flags=0x000B      ✓
GetConnection → displayType=6     ✓
... (various probes)
GetVideoParameters modeID=5 → 1024x768 OK  ✓
SavePreferredConfiguration
```

All driver responses are correct.  Yet the GDevice update happens
somewhere between these calls, apparently by reading the ROM.  The
exact Slot Manager call that triggers this is unknown.

## Proper fix path

The ideal solution eliminates ROM patching entirely:

1. Determine the correct sResource flags that prevent non-boot
   sResources from being opened as active displays at startup
2. Build the slot ROM with all resolutions as separate sResources,
   each with complete VPBlock entries for all depths
3. Only the boot sResource has `fOpenAtStart` set
4. DM2 SwitchMode switches between sResources (or uses driver
   protocol) without needing ROM mutation

This requires further investigation into the Slot Manager and
Display Manager internals (items R1–R4 above).

## Files affected

- `src/devices/video.cpp` — `patchSlotROMVPBlocks()`, offset
  recording in `s_vpBlockROMOffset[]`, calls from `SwitchMode`
  and `vidReset`
- `src/devices/slot_rom.h` — `SlotROMWriter` (used for patching)
