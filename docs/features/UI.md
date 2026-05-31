# User Interface

Functional specification of the maxivmac emulator UI (Windowed, Fullscreen, and
Control Overlay states).  For the pre-boot launcher see
[specs/LAUNCHER.md](../specs/LAUNCHER.md).

---

## Application Flow

```
[Launch] → no args / no --model  → [Launcher]  → click card → [Windowed]
[Launch] → path/to/file.mac      →                             [Windowed]
[Launch] → --model <name>        →                             [Windowed]
[Windowed] ↔ [Fullscreen]   (overlay button or Ctrl+F)
```

---

## Windowed State

- Resizable window (`SDL_WINDOW_RESIZABLE`).
- The entire client area is the emulator viewport; no menu bar chrome.
- On macOS, Cmd+Q and Cmd+W are removed from the system menu so they reach the guest.
- Closing the window (title-bar ×) terminates emulation immediately, no confirmation.
- All keyboard and mouse input goes to the guest except:
  - **Ctrl** (left or right) activates the overlay.
  - **Escape** dismisses the overlay when it is visible.

### Scaling Modes

Two modes, toggled via **Ctrl+M** or the overlay Scaling button:

| Mode | Behaviour |
|------|-----------|
| **Pixel Perfect** (default) | Window size constrained to integer multiples of the guest resolution (512×342, 1024×684, …).  Dragging snaps to the **nearest** integer multiple.  Minimum scale is 1×.  No letterbox/pillarbox bars — the viewport always fills the window exactly. |
| **Stretched** | Window freely resizable.  Viewport scales to fill the window, preserving aspect ratio.  Black bars appear as needed. |

Initial window size at boot: 2× the guest resolution if it fits the display, else 1×.

**Zoom** (Ctrl+Z / overlay button): jumps to the largest Pixel Perfect multiple that fits
the current screen, saving and restoring the previous window geometry.  Distinct from
fullscreen.

**Known issue (B3):** the macOS green zoom button maximizes the window without respecting
integer multiples.  The window ends up in a non-snapped size until the next manual resize.

---

## Fullscreen State

- `SDL_SetWindowFullscreen` — no window chrome.
- Previous windowed geometry is saved and restored on exit.
- Same two scaling modes:
  - **Pixel Perfect** — largest integer multiple centered; dark gray borders (#1A1A1A).
  - **Stretched** — fills the display preserving aspect ratio; bars only for aspect
    correction.
- **Escape** is forwarded to the guest and does **not** exit fullscreen.
- Fullscreen is toggled via the overlay or **Ctrl+F**.

---

## Control Overlay

### Activation

Press **Ctrl** (left or right).  Two modes, determined by press duration (250 ms threshold):

| Mode | How to trigger | Behaviour |
|------|---------------|-----------|
| **Tap** (sticky) | Release Ctrl before 250 ms | Overlay stays open; both hands free to click buttons |
| **Hold** (peek) | Keep Ctrl held ≥ 250 ms | Overlay visible while Ctrl is held; releasing dismisses it |

While the overlay is open, the host cursor is visible and no mouse or keyboard events
reach the guest.

### Dismissal

- Release **Ctrl** (hold/peek mode)
- Press **Ctrl** again (sticky mode)
- Press **Escape**
- Click a state-change button (Fullscreen toggle, Insert Disk)

### Appearance

- Semi-transparent black scrim over the entire viewport.
- Centered panel: ~400×320 px, rounded corners (12 px), nearly opaque dark background.
- Brief green flash feedback confirms triggered actions.

### Primary Controls

Always visible in the upper part of the panel.

| Button | Shortcut | Description |
|--------|----------|-------------|
| Insert Disk | I | Opens a native OS file dialog; accepts .dsk .img .hfs .dmg .iso .image .dc42 |
| Fullscreen / Windowed | F | Toggle fullscreen |
| Pixel Perfect / Stretched | M | Toggle scaling mode |
| Zoom | Z | Snap to largest Pixel Perfect multiple, centered |
| Speed: 1× 2× 4× 8× 16× 32× ∞ | ← / → / 0 | Speed presets; selected preset is highlighted |
| Screenshot | S | Capture guest screen as PNG to the host clipboard |
| Reboot | R | Warm-restart the emulated Mac |
| Power Off | — | Terminate emulation (no Ctrl shortcut) |

### Advanced Controls

Always visible below a separator.

| Control | Description |
|---------|-------------|
| Interrupt | Send NMI to the guest CPU |
| Filter: Near / Linear | Toggle GL texture filter for the viewport |
| Stopped | Checkbox — pause emulation |
| Run in Background | Checkbox — keep running when window loses focus |
| AutoSlow | Checkbox — honour guest idle hints |
| About | Version string, machine + System + INIT info, GPL v2, GitHub link |

### Ctrl Shortcuts

Shortcuts fire while the overlay is visible (any mode), **with or without Ctrl held**.
In sticky mode, bare shortcut keys (no Ctrl) work too.

| Shortcut | Action |
|----------|--------|
| Ctrl+F   | Toggle Fullscreen |
| Ctrl+M   | Toggle Scaling Mode |
| Ctrl+Z   | Zoom |
| Ctrl+S   | Screenshot |
| Ctrl+→   | Speed up (next preset) |
| Ctrl+←   | Speed down (previous preset) |
| Ctrl+0   | Speed reset to 1× |
| Ctrl+P   | Toggle Stopped (pause) |
| Ctrl+I   | Insert Disk |
| Ctrl+R   | Reboot |

---

## Mouse Behaviour

Three coordinate spaces: host window → emulator viewport (scaled/offset) → guest screen
(native resolution).  Host positions are mapped through the viewport before reaching the guest.

### Windowed

- Absolute coordinates.
- Host cursor hidden inside the viewport; the guest-drawn cursor is the only visible one.
- Host cursor reappears when the pointer leaves the window or when the overlay opens.

**Known issue:** the host cursor occasionally fails to reappear when leaving the window
(not consistently reproducible).

### Fullscreen

- Same absolute-coordinate path as windowed.
- Host cursor hidden; shown on window focus loss.

### Overlay Open

- Host cursor forced visible.
- No mouse events forwarded to the guest.
- macOS Ctrl+Click → right-click transformation suppressed while overlay is visible.

**Known issue (G1):** clicking overlay buttons while Ctrl is physically held (peek mode)
is unreliable — macOS transforms Ctrl+Click to right-click before SDL receives it; the
right-click is suppressed but the left-click may be lost.  Workaround: use tap (sticky)
mode to interact with overlay buttons.

### Backgrounded or Stopped

- Host cursor visible.
- No mouse or keyboard input reaches the guest.

---

## Keyboard

| Host key | Context | Action |
|----------|---------|--------|
| Ctrl (L or R) | Any | Overlay activation (see above) |
| Ctrl + shortcut key | Overlay visible | Fire shortcut (see table above) |
| Escape | Overlay open | Dismiss overlay |
| Any other key | No overlay | Forwarded to guest |

**Command key:** forwarded to the guest as ⌘.  Cmd+Q and Cmd+W are stripped from the
macOS menu bar so they reach the guest.  Cmd+H, Cmd+Tab, and other OS-reserved combos are
grabbed by macOS below the app level and cannot be forwarded — platform limitation.

**Guest Control key:** Ctrl is consumed by the overlay and never reaches the guest.
**Right Option** is mapped to guest Control for software that needs it (Think C, MPW, etc.).

---

## Known Gaps

| ID | Area | Description |
|----|------|-------------|
| G1 | Overlay | Ctrl+Click unreliable in peek mode (macOS right-click transform).  Use sticky mode. |
| B3 | Windowed | macOS green zoom button bypasses Pixel Perfect snap. |
| — | Notifications | User-facing errors (disk insert failure, ROM read error) currently go to stderr only.  No in-app toast.  See [UI_FUTURE.md](UI_FUTURE.md). |
| — | Activation key | Ctrl as overlay key conflicts with the guest Control key.  Configurable activation key is future work.  See [UI_FUTURE.md](UI_FUTURE.md). |

