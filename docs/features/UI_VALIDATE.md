# UI — Manual Validation Test Plan

Tests derived from [UI.md](UI.md) and [UI_DESIGN.md](UI_DESIGN.md).
Number each test for easy reference. Mark **PASS** / **FAIL** / **SKIP**.

Pre-requisites: build with `cmake --preset macos && cmake --build --preset macos`.
Launch without `--model` unless stated otherwise.

---

## A. Model Selector

| #   | Test | Expected |
|-----|------|----------|
| A1  | Launch without `--model`. Model selector appears. | |
| A2  | Select a model (e.g. Mac Plus). Emulator boots. | |
| A3  | Launch with `--model MacPlus`. Boots directly, no selector. | |

---

## B. Window — Pixel Perfect Scaling (default)

| #   | Test | Expected |
|-----|------|---------|
| B1  | Boot Mac Plus. Window is 1024×684 (2× of 512×342). | |
| B2  | Drag window edge. Size snaps to nearest integer multiple (1×, 2×, 3×). | Snaps to closest, not always down. No black bars. |
| B3  | Double-click title bar (macOS zoom). Snaps to largest integer multiple fitting the screen. | |
| B4  | Shrink below 1× guest resolution. Window stays at 1×. | |
| B5  | Boot on a small display where 2× doesn't fit. Initial size is 1×. | |

---

## C. Window — Stretched Scaling

| #   | Test | Expected |
|-----|------|----------|
| C1  | Toggle to Stretched (Ctrl+M or overlay button). Button reads "Pixel Perfect (M)"/"Stretched (M)". | |
| C2  | Drag window freely. Viewport scales with aspect ratio preserved. | Letterbox/pillarbox bars appear as needed. |
| C3  | Switch back to Pixel Perfect (Ctrl+M). Window snaps to nearest integer size. | |

---

## D. Fullscreen

| #   | Test | Expected |
|-----|------|----------|
| D1  | Ctrl+F or overlay button. Window goes fullscreen. | No window chrome. |
| D2  | Fullscreen + Pixel Perfect mode. Guest centered, dark gray (#1A1A1A) borders. | |
| D3  | Fullscreen + Stretched mode. Guest fills display, bars only for aspect correction. | |
| D4  | Press Escape in fullscreen. Key forwarded to guest, does NOT exit fullscreen. | |
| D5  | Ctrl+F again. Returns to windowed. | |

---

## E. Overlay — Activation & Dismissal

| #   | Test | Expected |
|-----|------|----------|
| E1  | Hold Ctrl > 250 ms. Overlay appears (peek mode). Release Ctrl → overlay dismisses. | |
| E2  | Tap Ctrl quickly (< 250 ms). Overlay stays open (sticky). | |
| E3  | Sticky overlay open. Tap Ctrl again → dismisses. | |
| E4  | Sticky overlay open. Press Escape → dismisses. | |
| E5  | Click a state-change button (e.g. Fullscreen). Overlay dismisses. | |
| E6  | While overlay is open: host cursor visible, no input reaches guest. | |

---

## F. Overlay — Panel Layout

| #   | Test | Expected |
|-----|------|----------|
| F1  | Open overlay. Single flat panel visible (~400×320), centered, semi-transparent scrim behind. | |
| F2  | Primary controls visible: Insert Disk (I), Fullscreen (F), Scaling Mode (M), Speed, Screenshot (S), Reboot (R), Power Off. | Buttons show shortcut keys. |
| F3  | Advanced section visible below separator (not collapsed). | Shows: Interrupt, Filter, Stopped, Run in Background, AutoSlow, About. |
| F4  | Overlay fits within a Mac Plus viewport (512×342 at 1×). | |

---

## G. Overlay — Primary Controls

| #   | Test | Expected |
|-----|------|----------|
| G1  | Insert Disk → native file dialog opens. Select a disk image → mounts. | |
| G2  | Insert Disk → cancel dialog. Nothing happens. | |
| G3  | Fullscreen toggle button. Switches display state. | |
| G4  | Scaling Mode toggle. Switches Pixel Perfect ↔ Stretched. | |
| G5  | Speed buttons (1×, 2×, 4×, 8×, 16×, 32×, Unlimited). Each changes emulation speed. | |
| G6  | Screenshot. Guest screen captured to clipboard. Paste in another app to verify. | |
| G7  | Reboot. Guest warm-restarts. | |
| G8  | Power Off. Emulation terminates, app closes. | |
| G9  | Close window (title bar X / Cmd+W). App quits. | |

---

## H. Overlay — Advanced Controls

| #   | Test | Expected |
|-----|------|----------|
| H1  | Interrupt. NMI sent (if Programmer's Key handler installed, debugger appears). | |
| H2  | Filter toggle. Nearest ↔ Linear. Visible on scaled viewports. | |
| H3  | Stopped toggle. Emulation pauses. Toggle again → resumes. | |
| H4  | Run in Background toggle. Lose window focus → emulation keeps running. | |
| H5  | About. Shows app name, GPL v2 license, GitHub link. No folder icon. | |

---

## I. Ctrl Shortcuts

All shortcuts: hold Ctrl, press key. Overlay flashes confirmation, dismisses on Ctrl release.
In sticky mode, bare keys (without Ctrl) also fire the shortcut.

| #   | Test | Expected |
|-----|------|---------|
| I1  | Ctrl+F | Toggle fullscreen. |
| I2  | Ctrl+M | Toggle scaling mode. |
| I3  | Ctrl+S | Screenshot to clipboard. |
| I4  | Ctrl+→ | Speed up one step. |
| I5  | Ctrl+← | Speed down one step. |
| I6  | Ctrl+0 | Speed reset to 1×. |
| I7  | Ctrl+P | Toggle paused. |
| I8  | Ctrl+I | Insert Disk dialog. |
| I9  | Ctrl+R | Reboot. |
| I10 | Shortcut does NOT make overlay sticky. | Overlay dismisses when Ctrl released. |
| I11 | Sticky overlay open. Press bare "F" (no Ctrl). | Toggles fullscreen. |
| I12 | Sticky overlay open. Press bare "S". | Screenshot taken. |

---

## J. Mouse — Windowed

| #   | Test | Expected |
|-----|------|----------|
| J1  | Move mouse inside window. Host cursor hidden, guest cursor tracks. | |
| J2  | Move mouse outside window. Host cursor reappears. | |
| J3  | Click-drag inside guest, release outside window. Guest receives mouse-up. | |
| J4  | Guest cursor not movable by clicking on background (no viewport dragging). | |

---

## K. Mouse — Fullscreen

| #   | Test | Expected |
|-----|------|----------|
| K1  | Mouse in relative mode. Raw deltas applied to guest position. | |
| K2  | Host cursor hidden everywhere, including over borders. | |
| K3  | Move to border area. Position clamped to nearest guest edge. | |

---

## L. Keyboard

| #   | Test | Expected |
|-----|------|----------|
| L1  | Type normal keys (no overlay). All forwarded to guest. | |
| L2  | Cmd+Q. Sends ⌘Q to guest, does NOT quit emulator. | |
| L3  | Left/Right Command. Forwarded as ⌘. | |
| L4  | Right Option. Sent to guest as Control. | Verify in Think C or MPW. |
| L5  | Ctrl (left or right). Activates overlay, never reaches guest. | |

---

## M. Window Chrome & About

| #   | Test | Expected |
|-----|------|----------|
| M1  | macOS menu bar present. | |
| M2  | About panel: app name, license, GitHub link. No folder icon. | |
| M3  | Cmd+Tab, Cmd+H grabbed by macOS (platform limitation — document only). | |
