# VIDEO_PLAN — Resolution Switching (COMPLETE)

All three phases have been implemented and committed.

## Commits

* **Phase A** (36161a7): Resolution table, multi-res slot ROM, VRAM
  sizing, ATT generalization, buffer pre-allocation, onResolutionChanged
  interface.
* **Phase B** (caf8995): SwitchMode (control csCode 10), GetModeTiming
  fix, host-side resolution-change detection.
* **Phase C** (6ee70dd): Host-derived resolutions, PRAM reboot path,
  buffer sizing from Vid_MaxResolutionSize, ASan clean.

## Deferred

* **Fullscreen hint**: Auto-switch or status-bar indicator when guest
  resolution matches host desktop.  The plumbing works; decision deferred.
* **Dynamic host-desktop re-detection**: Host display bounds are sampled
  once at startup.  Mid-session display changes are not tracked.

## Reference

The reference tables (mode numbering, VPBlock, boot depth, ASan notes)
are documented in [VIDEO.md](VIDEO.md).

