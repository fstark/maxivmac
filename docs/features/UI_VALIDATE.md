# UI — Manual Validation Test Plan

Tests derived from [UI.md](UI.md).  Mark **PASS** / **FAIL** / **SKIP**.

Pre-requisites: build with `cmake --preset macos && cmake --build --preset macos`.
Launch without arguments unless stated otherwise.

---

## A. Launcher

| #   | Test | Expected |
|-----|------|----------|
| A1  | Launch without arguments. | Launcher window (700×500, gray background) appears with card grid. |
| A2  | Click a valid card (bold, full opacity). | Launcher closes; emulator boots and window appears. |
| A3  | Hover an invalid card (greyed, 35% opacity). | Tooltip shows validation error. Card is not clickable. |
| A4  | Click the ⓘ button on a card. | Info popup opens. Click outside → dismisses. |
| A5  | Launch with `--model MacPlus`. | Boots directly to windowed state; no launcher. |
| A6  | Launch with `path/to/file.mac`. | Boots directly; no launcher. |
| A7  | Remove all .mac files from `data/macs/`. Launch. | Launcher shows "No .mac files found in data/macs/". |

---

## B. Window — Pixel Perfect Scaling (default)

| #   | Test | Expected |
|-----|------|---------|
| B1  | Boot Mac Plus. Inspect window size. | 1024×684 (2× of 512×342). |
| B2  | Drag window edge. | Snaps to nearest integer multiple (1×, 2×, 3×, …). No black bars. Snaps up as well as down. |
| B3  | Double-click title bar (macOS green zoom button). | **Known issue (B3):** window ends up non-integer-snapped. Mark FAIL until fixed. |
| B4  | Resize below 1× guest resolution. | Window stays at 1×; does not go smaller. |
| B5  | Boot on a small display where 2× doesn't fit. | Initial size is 1×. |

---

## C. Window — Stretched Scaling

| #   | Test | Expected |
|-----|------|----------|
| C1  | Toggle to Stretched (Ctrl+M or overlay button). | Button label flips. Window can now be freely resized. |
| C2  | Drag window freely. | Viewport scales with aspect ratio preserved; black bars appear as needed. |
| C3  | Switch back to Pixel Perfect (Ctrl+M). | Window snaps to nearest integer size. |

---

## D. Zoom

| #   | Test | Expected |
|-----|------|----------|
| D1  | Press Ctrl+Z (or overlay Zoom button). | Window jumps to largest Pixel Perfect multiple that fits the screen, centered. |
| D2  | Press Ctrl+Z again. | Previous window geometry restored. |

---

## E. Fullscreen

| #   | Test | Expected |
|-----|------|----------|
| E1  | Ctrl+F or overlay Fullscreen button. | Window goes fullscreen; no window chrome. |
| E2  | Fullscreen + Pixel Perfect mode. | Guest centered, dark gray (#1A1A1A) borders. |
| E3  | Fullscreen + Stretched mode. | Guest fills display, bars only for aspect correction. |
| E4  | Press Escape in fullscreen. | Key forwarded to guest; does NOT exit fullscreen. |
| E5  | Ctrl+F again. | Returns to windowed at previous size. |

---

## F. Overlay — Activation & Dismissal

| #   | Test | Expected |
|-----|------|----------|
| F1  | Hold Ctrl ≥ 250 ms. | Overlay appears (peek mode). Release Ctrl → overlay dismisses. |
| F2  | Tap Ctrl quickly (< 250 ms). | Overlay stays open (sticky). |
| F3  | Sticky overlay: tap Ctrl again. | Overlay dismisses. |
| F4  | Sticky overlay: press Escape. | Overlay dismisses. |
| F5  | Click Fullscreen or Insert Disk button. | Overlay dismisses. |
| F6  | While overlay open: move/click mouse. | Host cursor visible; no input reaches guest. |

---

## G. Overlay — Panel Layout

| #   | Test | Expected |
|-----|------|----------|
| G1  | Open overlay. | Single flat panel (~400×320), centered, dark background, scrim over viewport. |
| G2  | Primary row: Insert Disk (I), Fullscreen (F), Pixel Perfect/Stretched (M), Zoom (Z). | All visible; buttons show shortcut keys. |
| G3  | Speed row: 1× 2× 4× 8× 16× 32× ∞. | Current speed highlighted. |
| G4  | Screenshot (S), Reboot (R), Power Off. | All visible. |
| G5  | Separator below primary. Advanced controls below: Interrupt, Filter, Stopped, Run in Background, AutoSlow, About. | Always visible without expanding. |

---

## H. Overlay — Controls

| #   | Test | Expected |
|-----|------|----------|
| H1  | Insert Disk → native file dialog. Select .dsk → mounts. | |
| H2  | Insert Disk → cancel dialog. | Nothing happens. |
| H3  | Fullscreen toggle button. | Switches display state. |
| H4  | Scaling Mode toggle. | Switches Pixel Perfect ↔ Stretched. |
| H5  | Speed buttons 1×…∞. | Emulation speed changes; selected button highlighted. |
| H6  | Screenshot. | Guest screen captured as PNG to clipboard. Paste in a host app to verify. |
| H7  | Reboot. | Guest warm-restarts. |
| H8  | Power Off. | Emulation terminates; app exits. |
| H9  | Close window (title-bar ×). | App quits immediately; no dialog. |
| H10 | Interrupt. | NMI sent (if Programmer's Key handler installed, debugger appears). |
| H11 | Filter toggle. | Nearest ↔ Linear. Visible difference on a scaled viewport. |
| H12 | Stopped checkbox. | Emulation pauses. Uncheck → resumes. |
| H13 | Run in Background. | App backgrounded → emulation keeps running. |
| H14 | About section. | Shows version, machine info, GPL v2, GitHub link. |

---

## I. Ctrl Shortcuts

All shortcuts work while the overlay is visible (any mode).  In sticky mode, bare keys
(without Ctrl held) also fire the action.

| #   | Shortcut | Expected |
|-----|----------|---------|
| I1  | Ctrl+F | Toggle fullscreen. |
| I2  | Ctrl+M | Toggle scaling mode. |
| I3  | Ctrl+Z | Zoom. |
| I4  | Ctrl+S | Screenshot. |
| I5  | Ctrl+→ | Speed up one step. |
| I6  | Ctrl+← | Speed down one step. |
| I7  | Ctrl+0 | Speed reset to 1×. |
| I8  | Ctrl+P | Toggle paused. |
| I9  | Ctrl+I | Insert Disk dialog. |
| I10 | Ctrl+R | Reboot. |
| I11 | Shortcut key does not make overlay sticky. | Overlay dismisses on Ctrl release (hold mode). |
| I12 | Sticky overlay + bare "F" (no Ctrl). | Toggles fullscreen. |

---

## J. Mouse — Windowed

| #   | Test | Expected |
|-----|------|----------|
| J1  | Move mouse inside window. | Host cursor hidden; guest-drawn cursor tracks position. |
| J2  | Move mouse outside window. | Host cursor reappears. |
| J3  | Click-drag inside guest, release button outside window. | Guest receives the mouse-up event. |

---

## K. Mouse — Fullscreen

| #   | Test | Expected |
|-----|------|----------|
| K1  | Move mouse in fullscreen. | Host cursor hidden; guest cursor tracks correctly. |
| K2  | Move mouse to border area. | Guest cursor clamped to nearest screen edge. |

---

## L. Keyboard

| #   | Test | Expected |
|-----|------|----------|
