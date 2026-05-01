# UI — Future Work

Items deferred from UI_ISSUES.md that need further investigation or
are out of scope for the current iteration.

---

## D1 — Ctrl+F vs macOS Green Zoom Box

Ctrl+F toggles true fullscreen. The macOS green zoom box maximizes
the window, which is a different operation. Currently both end up in
fullscreen.

**Future**: when multi-monitor support is added, Ctrl+F should go
fullscreen across all screens, while the green button maximizes on the
current display only.

---

## G1 — Ctrl+Click Blocked by macOS in Peek Mode

When the overlay is shown via Ctrl-hold (peek mode), clicking a button
requires Ctrl to stay held. macOS transforms Ctrl+Click into
right-click at the OS level before SDL receives it. The right-click is
suppressed, but the left-click may not arrive.

Workaround: use tap (sticky) mode instead of hold mode for button
interaction.

**Future**: investigate SDL3 event filtering or Cocoa-level overrides
to suppress the Ctrl+Click→right-click transformation while the
overlay is visible.