# UI — Open Issues

Confirmed-open bugs as of May 2026.  Resolved items have been dropped.

---

**B3 — macOS green zoom button ignores Pixel Perfect snap**

Double-clicking the macOS title bar (green zoom button) triggers `SDL_WINDOW_MAXIMIZED`,
which the snap handler deliberately skips.  The resulting window size is not an integer
multiple of the guest resolution, leaving it in an effectively stretched state until the
user manually resizes.

Expected: the zoom action should snap to the largest Pixel Perfect multiple that fits the
screen, same as Ctrl+Z / Zoom button.  This requires intercepting the macOS zoom action
before SDL reports it.

---

**G1 — Ctrl+Click in peek (hold) mode** *(fixed)*

Resolved: the macOS Ctrl+Click → right-click remapping is now intercepted before ImGui
processes the event; right-button events are remapped to left-button while the overlay is
visible, so buttons respond correctly in both peek and sticky modes.
