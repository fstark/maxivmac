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

**G1 — Ctrl+Click unreliable in peek (hold) mode**

When the overlay is opened by holding Ctrl (peek mode), the user must keep Ctrl held while
clicking buttons.  macOS converts Ctrl+Click to right-click at the OS level before SDL
receives it.  The right-click is suppressed, but the left-click may not arrive, so buttons
may not respond.

Workaround: use tap (sticky) mode — tap and release Ctrl quickly, then click buttons
freely.

Tracked as a future improvement in [UI_FUTURE.md](UI_FUTURE.md).
