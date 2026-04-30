# User Interface

Functional specification of the maxivmac graphical user interface.

## Application Flow

```
[Launch] → no --model → [Model Selector] → Boot → [Windowed]
[Launch] → --model given → [Windowed]
[Windowed] ↔ [Fullscreen]   (via overlay or Escape)
```

Two display states plus a pre-boot model selector (see
[specs/MODEL_SELECTOR.md](specs/MODEL_SELECTOR.md)).  The Ctrl overlay
is available in both display states.

---

## Windowed State

- Borderless window sized exactly to the emulated screen at the
  current zoom level (e.g. 512×342 at 1×, 1024×684 at 2×).
- No menu bar, no window chrome beyond the platform title bar.
- The entire window is the emulator viewport.
- Dark gray background (#1A1A1A).
- All keyboard and mouse input is forwarded to the guest except:
  - **Ctrl** (left or right) toggles the control overlay.
  - **Escape** dismisses the overlay (when visible).

---

## Fullscreen State

- The emulated screen is integer-scaled and centered on the display.
- Remaining area is black.
- No window chrome.
- Same Ctrl overlay behavior as Windowed.
- **Escape** returns to Windowed.

---

## Control Overlay

### Activation

- Press **Ctrl** (left or right) to toggle.
- While Ctrl is held or the overlay is open, the host cursor is
  visible and no mouse or keyboard events reach the guest.

### Dismissal

- Press **Ctrl** again, or
- Press **Escape**, or
- Click a state-change button (e.g. Fullscreen toggle).

### Appearance

- Semi-transparent dark scrim over the entire viewport.
- Centered panel (~400×320 px, rounded corners, nearly opaque).
- Four tabs along the top.

### Machine Tab

| Button       | Action                                |
|--------------|---------------------------------------|
| Insert Disk  | Opens a host file dialog              |
| Eject All    | Ejects all mounted disk images        |
| Interrupt    | Sends NMI to the guest CPU            |
| Reboot       | Warm-restarts the emulated Mac        |
| Power Off    | Terminates emulation                  |
| Screenshot   | Captures the guest screen (planned)   |

### Display Tab

| Control     | Options                    |
|-------------|----------------------------|
| Zoom        | 1×, 2× (radio buttons)     |
| Filter      | Nearest, Linear            |
| Fullscreen  | Toggle (Windowed ↔ Full)   |

### Speed Tab

| Control            | Options / Description                        |
|--------------------|----------------------------------------------|
| Emulation Speed    | 1×, 2×, 4×, 8×, 16×, 32×, Unlimited         |
| Stopped            | Checkbox — pauses emulation                  |
| Run in Background  | Checkbox — keep running when window loses focus |
| AutoSlow           | Checkbox — honour guest idle hints           |

### Advanced Tab

About information: product name, origin project, license (GPL v2).

---

## Mouse Behavior

Three coordinate spaces are involved: host window, emulator viewport
(potentially scaled and offset), and guest screen (native Mac
resolution).  All host positions are mapped through the viewport
before reaching the guest.

### Windowed

- Absolute coordinates; host cursor hidden inside the window.
- Guest-drawn cursor is the only visible cursor.
- Host cursor reappears when the pointer leaves the window.

### Fullscreen

- Relative (grabbed) mode; raw deltas applied to guest position.
- Host cursor always hidden, even over black borders.
- Positions on borders are clamped to the nearest guest edge.

### Overlay Open

- Host cursor forced visible.
- No mouse events forwarded to the guest.

### Backgrounded or Stopped

- Host cursor always visible.
- No mouse or keyboard input reaches the guest.

---

## Keyboard

| Key             | Context        | Action                            |
|-----------------|----------------|-----------------------------------|
| Ctrl (L or R)   | Any            | Toggle control overlay            |
| Escape          | Overlay open   | Dismiss overlay                   |
| Escape          | Fullscreen     | Return to Windowed                |
| Any other key   | No overlay     | Forwarded to guest                |

The physical Ctrl key cannot reach the guest while the overlay
mechanism uses it.  A future configurable activation key (e.g. F12)
would remove this limitation.

---

## Toast Notifications (planned)

Errors currently go to stderr only.  A toast system will restore
user-visible feedback:

- Auto-dismiss after ~5 seconds; manual dismiss available.
- Stacked vertically in a screen corner.
- Color-coded by severity (info / warning / error).

Planned messages include: disk image open failures, too many disks,
ROM file errors, unsupported disk formats, and unclean shutdown
warnings.

---

## Remaining Work

| Area               | Status                                    |
|--------------------|-------------------------------------------|
| Toast notifications | Planned — errors only on stderr today    |
| Fullscreen         | Rendering identical to Windowed (placeholder) |
| Native file dialog | Insert Disk / Browse buttons are stubs    |
| Per-drive eject    | Only "Eject All" available                |
| Screenshot         | Button present, no implementation         |
| Configurable Ctrl  | Hardcoded to Ctrl; no user setting        |
| Arbitrary zoom     | Only 1× and 2×; no 3× or fractional      |
