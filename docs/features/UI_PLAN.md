# UI — Implementation Plan

**Status: COMPLETED** — 30 April 2026

All 11 phases implemented and committed (1eb2518..65ed933).
Unit tests added in `test/test_ui.cpp`.

Design: [UI_DESIGN.md](UI_DESIGN.md)
Spec: [UI.md](UI.md)

| Phase | Description                                 | Status    |
|-------|---------------------------------------------|-----------|
| 1     | ScalingMode enum + resizable window         | Done      |
| 2     | Integer-snap resize handler                 | Done      |
| 3     | Stretched scaling mode (windowed)           | Done      |
| 4     | Fullscreen rendering (both modes)           | Done      |
| 5     | Overlay hold/tap state machine              | Done      |
| 6     | Ctrl shortcut dispatch                      | Done      |
| 7     | Overlay panel layout redesign               | Done      |
| 8     | Insert Disk native file dialog              | Done      |
| 9     | Screenshot to clipboard                     | Done      |
| 10    | Mouse fixes (capture + cursor reappear)     | Done      |
| 11    | Command key passthrough + About panel       | Done      |

Build gate: `cmake --preset macos && cmake --build --preset macos`
Test gate:  `./bld/macos/tests`

