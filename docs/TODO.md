* Fix keyboard (Command-Q / Ctrl...)

* Group guest/host comm logically
  [config + version]
  [raw device emulation]
  [clipboard]
  [shared drive]

* Rewrite the sony driver with our mecanism

* Add a mac side file transfer app???

* Hhost->guest commands? (lauch app, shutdown)

* Host->guest copy files? explore directories? unix fuse filesystem with init-baked running emulator?

* Shared drive "watch for changes"

* Multiple shared drives

* Control panel?

* Fix on-screen SDL interface



* WTF with the window changed code?

* Clipboard sync: apps with private scraps (e.g. THINK C) only see
  host clipboard changes after a real MultiFinder context switch
  (Finder round-trip). Same limitation applies in the Mac→Host
  direction. Desk scrap is updated correctly, but the app's private
  scrap is only refreshed on resume+convertClipboard, which requires
  a genuine app activation. Tried: jGNEFilter event injection,
  WaitNextEvent trap patch, PostEvent — none deliver app4Evt to
  apps in a way MultiFinder recognizes. Needs trap-level logging
  to understand how MultiFinder dispatches suspend/resume internally.

* At one point we may want to fix the code so misconfigured VRAM doesn't crash (throw). (Unsure what is meant there)

* Check what extnBlockBase does

* Use68020 macro weirdness

* LeaPeaEACalcCyc returns an uint8_t, but should be uint16_t (bug in reference too)

* is68020OnlyKind and 68020 table build seems weird to me (I feel it should be simpler, with a Use68020 at execution time. May be more complicated than that). The disabling from M68KITAB_setup is equally weird to me.

* Fixed date at startup

* Debug networking

* Resolution change window sizing: when the guest System restores a
  saved resolution matching the host desktop (ID 100) or half-desktop
  (ID 101), onResolutionChanged sizes the window to fill the entire
  usable area and macOS auto-maximizes it. Need a smarter strategy
  for host-derived resolutions — possibly cap at a sensible pixel-
  perfect multiple or skip the resize when the target would fill the
  screen.

* Display version string (MAXIVMAC_VERSION) in three places:
  console at startup, launcher window, and overlay (for support).

* Acceleration and 1s clock management is not user friendly anymore

* Funky kExtnVideo management

* Rename the "NewDisk" stuff in sdl.cpp. It is used for FILE export, not DISK. Also, it has nothing SDL specific, it should be in the the generic code.

* File import/export between host and emulated Mac is currently a kludge: files are tunneled through the disk drive mechanism (inserted as fake "disks", read/written as raw bytes, then ejected). This makes the code confusing and limits functionality (e.g. no resource fork support, no metadata). A proper file transfer extension — a new extension ID with explicit read/write/list commands operating on host paths — would be cleaner and allow richer integration without abusing the disk slot array.

* Get rid of UnusedParam macro

Step 9c — Extract event handling into sdl_events.cpp: HandleTheEvent() (~230 lines) is deeply coupled to sdl.cpp statics — my_renderer, UseFullScreen, RequestMacOff, gTrueBackgroundFlag, CaughtMouse, my_main_wind, plus mouse and disk-insert helpers. Extracting it would require exposing or restructuring a large number of internal globals, making the change invasive for modest payoff.

Step 9d — Simplify HaveChangedScreenBuff: The plan called for removing the per-pixel slow path (including 24-bpp support) since modern displays never use it, and simplifying the CLUT loop. This touches core rendering logic and carries regression risk that wasn't worth taking without more targeted testing of color/depth modes beyond the golden-file suite.

Headless: 4/6 PASS (Classic, Mac512Ke, MacPlus, MacSE). MacII/MacIIx FAIL due to SDL audio callback non-determinism in the golden files (pre-existing issue)
This is the expected outcome. The headless backend works correctly; the MacII/MacIIx golden files are simply not deterministic enough for cross-backend verification. This is a known limitation to fix later (by making the audio path fully deterministic in record/verify mode).



## Future Work

- Key mappings (`MKC_formac_*`) should move to their own header.
- `WantInitRunInBackground` could become `EmulatorConfig::runInBackground`.
- `EmLocalTalk` could become runtime when networking gets a device abstraction.
- `dbglog_HAVE` / `WantAbnormalReports` could become runtime if debug
  logging overhead becomes negligible.

---

## Out of Scope

- Language (`MINIVMAC_LANGUAGE`) — separate plan.
- `MKC_formac_*` key mappings — separate concern, needs its own design.
- `WantCycByPriOp` — tracked in CLEANUP.md Step 4.
- `SCC_TrackMore` — tracked in CLEANUP.md Step 4.
- Moving globals into Machine — tracked in CLEANUP.md Step 3.
