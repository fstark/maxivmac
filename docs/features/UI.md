# User Interface

Functional specification of the maxivmac graphical user interface.

## Application Flow

```
[Launch] → no --model → [Model Selector] → Boot → [Windowed]
[Launch] → --model given → [Windowed]
[Windowed] ↔ [Fullscreen]   (via overlay or Ctrl+F)
```

Two display states plus a pre-boot model selector (see
[specs/MODEL_SELECTOR.md](specs/MODEL_SELECTOR.md)).  The Ctrl overlay
is available in both display states.

---

## Windowed State

- Standard platform window with title bar, **always resizable**.
- The platform menu bar is present (macOS requires it).  The About
  panel should show the application name, license, and a link to the
  GitHub repository — not a folder icon.
- The entire client area is the emulator viewport.
- All keyboard and mouse input is forwarded to the guest except:
  - **Ctrl** (left or right) activates the overlay / shortcuts.
  - **Escape** dismisses the overlay (when visible).

### Scaling Modes

Two modes, toggled in the overlay or via Ctrl+M:

- **Pixel Perfect** (default) — the window size is quantized to exact
  multiples of the guest resolution (e.g. 512×342, 1024×684,
  1536×1026…).  Dragging the window edge snaps to the **nearest**
  integer multiple.  There are **never** black borders — the viewport
  always fills the window exactly.  Double-clicking the title bar
  (macOS zoom) snaps to the largest integer size that fits the screen.

- **Stretched** — the window is freely resizable.  The viewport
  scales to fill the window while preserving aspect ratio.  Thin
  letterbox or pillarbox bars appear when the aspect doesn’t match
  exactly.

The initial window size at boot is 2× the guest resolution (or 1× if
2× doesn’t fit the display).

---

## Fullscreen State

- Same two scaling modes apply:
  - **Pixel Perfect** — largest integer multiple that fits, centered,
    dark gray (#1A1A1A) borders.  (Unlike windowed Pixel Perfect mode,
    the display size is not quantized, so borders are unavoidable.)
  - **Stretched** — fills the display preserving aspect ratio;
    bars only for aspect correction.
- No window chrome.
- Same Ctrl overlay behavior as Windowed.
- **Escape** does **not** exit fullscreen — it is forwarded to the
  guest like any other key.  Fullscreen is toggled via the overlay
  or Ctrl+F.

---

## Control Overlay

### Activation

- Press **Ctrl** (left or right).
- Two interaction modes, determined by press duration (~250 ms
  threshold):
  - **Hold** (peek) — overlay visible while Ctrl is held; release
    dismisses it.  Good for a quick glance at speed/status.
  - **Tap** (sticky) — overlay stays open after release; both hands
    free for clicking buttons, browsing files, etc.

- While the overlay is open, the host cursor is visible and no mouse
  or keyboard events reach the guest.

### Dismissal

- Release **Ctrl** (hold mode), or
- Press **Ctrl** again (sticky mode), or
- Press **Escape**, or
- Click a state-change button (e.g. Fullscreen toggle).

### Appearance

- The overlay is designed to fit a Mac Plus (512×342) viewport and
  remain consistent across all models.
- Semi-transparent dark scrim over the entire viewport.
- Centered panel (~400×320 px, rounded corners, nearly opaque).

### Layout

The current four-tab split puts common controls too many clicks away.
The goal is a single main panel that shows the most-used actions at a
glance, with an **Advanced** section (or collapsible area) for the
rest.  Only controls that actually work should be visible — no stubs.

**Primary controls** (always visible):

| Control         | Description                                     | Status   |
|-----------------|------------------------------------------------|----------|
| Insert Disk     | Opens a host file dialog                        | NOT IMP  |
| Fullscreen      | Toggle (Windowed ↔ Full)                        |          |
| Scaling Mode    | Pixel Perfect / Stretched toggle                |          |
| Speed           | 1×, 2×, 4×, 8×, 16×, 32×, Unlimited            |          |
| Screenshot      | Capture guest screen to clipboard               | NOT IMP  |
| Reboot          | Warm-restarts the emulated Mac                  |          |
| Power Off       | Terminates emulation                            |          |

**Advanced** (secondary section, always visible below a separator):

| Control            | Description                                     | Status   |
|--------------------|------------------------------------------------|----------|
| Interrupt          | Sends NMI to the guest CPU                      |          |
| Filter             | Nearest / Linear texture filtering              |          |
| Stopped            | Pauses emulation                                |          |
| Run in Background  | Keep running when window loses focus             |          |
| AutoSlow           | Honour guest idle hints                         | UNCLEAR  |
| About              | App name, license (GPL v2), GitHub link          |          |

### Ctrl Shortcuts

Ctrl + another key fires a shortcut immediately and the overlay does
**not** become sticky.  Shortcuts work both during the initial hold
window and while the overlay is already visible in peek mode.

When the overlay is in **sticky** mode, bare shortcut keys (without
Ctrl held) also fire the corresponding action.

The overlay flashes briefly to confirm the action, then disappears on
Ctrl release (hold mode) or stays open (sticky mode).

Buttons show their shortcut key for discoverability.

| Shortcut     | Action                          |
|--------------|---------------------------------|
| Ctrl + F     | Toggle Fullscreen               |
| Ctrl + M     | Toggle Scaling Mode (PP/Str)   |
| Ctrl + S     | Screenshot to clipboard         |
| Ctrl + →     | Speed up (next preset)          |
| Ctrl + ←     | Speed down (previous preset)    |
| Ctrl + 0     | Speed 1× (reset)                |
| Ctrl + P     | Toggle paused (Stopped)         |
| Ctrl + I     | Insert Disk                     |
| Ctrl + R     | Reboot                          |

Shortcuts are a fast path; every shortcut action is also available as
a button in the overlay panel.

---

## Mouse Behavior

Three coordinate spaces are involved: host window, emulator viewport
(potentially scaled and offset), and guest screen (native Mac
resolution).  All host positions are mapped through the viewport
before reaching the guest.

**Known issue:** mouse-up events outside the window are not reported
to the guest.  A drag started inside the guest continues even after
the button is released outside.  The guest should receive the release.

### Windowed

- Absolute coordinates; host cursor hidden inside the window.
- Guest-drawn cursor is the only visible cursor.
- Host cursor reappears when the pointer leaves the window.

Known edge case: the host cursor occasionally fails to reappear when
leaving the window (not consistently reproducible).

### Fullscreen

- Relative (grabbed) mode; raw deltas applied to guest position.
- Host cursor always hidden, even over borders.
- Positions on borders are clamped to the nearest guest edge.

### Overlay Open

- Host cursor forced visible.
- No mouse events forwarded to the guest.

### Backgrounded or Stopped

- Host cursor always visible.
- No mouse or keyboard input reaches the guest.

---

## Keyboard

### Host Key Mapping

| Host key          | Context        | Action                            |
|-------------------|----------------|-----------------------------------|
| Ctrl (L or R)     | Any            | Overlay (hold = peek, tap = sticky) |
| Ctrl + key        | Within hold    | Shortcut (see above); no sticky   |
| Escape            | Overlay open   | Dismiss overlay                   |
| Command (L or R)  | Any            | Forwarded to guest as **⌘**       |
| Right Option      | No overlay     | Sent to guest as **Control**      |
| Any other key     | No overlay     | Forwarded to guest                |

### Command Key

Command is forwarded to the guest as the Mac ⌘ key.  The emulator
overrides macOS default handling: Command+Q does **not** quit the
emulator — it sends ⌘Q to the guest (e.g. Quit in guest apps).
Command+H, Command+Tab, and other OS-reserved combos are grabbed by
macOS at a level below the app and cannot be forwarded — this is a
platform limitation that may need revisiting.

The emulator can only be terminated via the overlay (Power Off button)
or the Ctrl+Q shortcut (if added later).  The goal — especially in
fullscreen — is "I am on an old Mac."

### Guest Control

Ctrl is consumed by the overlay and never reaches the guest.  For
guest software that needs Control (Think C, MPW, terminal utilities),
**Right Option** is mapped to guest Control.  Left Option remains
available as the host Option/Alt key.

On external keyboards with a physical Right Ctrl, Right Ctrl could
alternatively pass through to the guest (making Left Ctrl the sole
overlay key).  This is a future refinement.

---

## Remaining Work

| Area               | Status                                    |
|--------------------|-------------------------------------------|
| Overlay layout     | Redesign: single panel + Advanced section |
| Resizable window   | Window is not resizable today             |
| Integer snap       | Window-size snapping not implemented      |
| Stretched scaling  | Not implemented                           |
| Fullscreen         | Rendering identical to Windowed (placeholder) |
| Native file dialog | Insert Disk / Browse buttons are stubs    |
| Screenshot         | Not implemented                           |
| About panel        | Shows folder icon instead of app info     |
| Mouse-up outside   | Release events lost outside window        |
| Ctrl shortcuts     | Not implemented                           |
| Hold/tap overlay   | Only toggle today; no hold-peek           |
