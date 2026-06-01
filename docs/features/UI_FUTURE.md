# UI — Future Work

Items deferred from current implementation.

---

## D1 — Ctrl+F vs macOS Green Zoom Box

Ctrl+F toggles true OS fullscreen (`SDL_SetWindowFullscreen`).  The macOS green zoom
button maximizes the window, which is a different operation — the window stays windowed
but fills the screen without entering fullscreen mode.  Currently the green button
produces a non-integer-snapped window (see B3 in [UI_ISSUES.md](UI_ISSUES.md)).

**Future**: when multi-monitor support is added, Ctrl+F should go fullscreen on all
screens (or the current screen), while the green button should snap to the largest Pixel
Perfect multiple on the current display only.

---

## Configurable Overlay Activation Key

The overlay activation key is hardcoded to Ctrl (left or right).  This conflicts with the
emulated Mac's Control key: Ctrl is consumed by the overlay and never reaches the guest.
Right Option is mapped to guest Control as a workaround.

**Future**: allow the user to configure the activation key (e.g. F12, Pause, right-Ctrl
only) so the physical Ctrl key can pass through to the guest, removing the need for the
Right Option workaround.

---

## Toast Notifications

User-facing errors (disk insert failure, too many disks, ROM read error, unsupported disk
image format) currently produce output on stderr only.  The user gets no in-app feedback.

**Future**: a lightweight ImGui toast system — auto-dismiss after ~5 s, color-coded by
severity (info / warning / error), stacked in a corner, no focus steal.

Representative use cases:

| Trigger | Severity |
|---------|---------|
| Disk image open failed (not found, permissions) | Warning |
| All 6 disk slots full | Warning |
| Disk image format unsupported | Warning |
| ROM file too short / unreadable | Error |
| Quit with disks still mounted | Warning |
