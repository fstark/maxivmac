# UI Design

Implementation design for the UI functional spec ([UI.md](UI.md)).
Addresses how each feature maps to the existing ImGui+SDL3+OpenGL
architecture.

---

## 1. Window Resizability & Scaling Modes

### Current State

Windows are created without `SDL_WINDOW_RESIZABLE`.  Size is fixed at
`guestW × scale` where scale is 1 or 2 (`g_wantMagnify`).

### Design

Add `SDL_WINDOW_RESIZABLE` to the window flags in `createWindow()`.
Replace the boolean `g_wantMagnify` with a `ScalingMode` enum:

```cpp
enum class ScalingMode { Integer, Stretched };
```

Store in `EmulatorShell` alongside the current `UIState`.

#### Integer Mode (default)

The window must always be an exact multiple of guest resolution.
Approach:

1. On `SDL_EVENT_WINDOW_RESIZED`, compute the nearest integer
   multiple of `(guestW, guestH)` that fits the new size.
   ```
   int scaleX = max(1, newW / guestW);
   int scaleY = max(1, newH / guestH);
   int scale  = min(scaleX, scaleY);
   ```
2. Call `SDL_SetWindowSize(window_, guestW * scale, guestH * scale)`
   to snap the window.  This fires another resize event — guard with
   a flag (`snapping_ = true`) to avoid recursion.
3. Because the window always equals an exact multiple, the viewport
   fills it completely — no black bars, no letterboxing needed.
4. Double-click-to-zoom (macOS): handled by the same resize path;
   SDL gives us the maximized size, we snap to the largest integer
   multiple that fits.

On window creation at boot, pick `scale = 2` if `guestW * 2 ≤
displayW && guestH * 2 ≤ displayH`, else `scale = 1`.

#### Stretched Mode

No snapping.  On resize, compute the aspect-preserving viewport rect
within the new window size:

```
float scaleF = min(winW / guestW, winH / guestH);
int viewW = guestW * scaleF;
int viewH = guestH * scaleF;
int offsetX = (winW - viewW) / 2;
int offsetY = (winH - viewH) / 2;
```

Draw letterbox/pillarbox bars using `glClearColor` with the
background colour.  The existing `displayEmulatorImage()` already
records `emuViewOriginX/Y` and `emuViewW/H` — this naturally extends.

#### Mode Toggle

`Ctrl+M` or overlay button.  When switching Integer→Stretched, no
resize needed.  When switching Stretched→Integer, snap to the nearest
valid size using the same snap logic.

---

## 2. Fullscreen Rendering

### Current State

`drawViewportFullscreen()` already does aspect-preserving scaling with
an integer-multiple preference.  This mostly matches the spec.

### Changes Needed

1. **Background colour**: Set `glClearColor` to dark grey
   `(0.102, 0.102, 0.102, 1.0)` (#1A1A1A) before clearing, only in
   fullscreen.
2. **ScalingMode applies in fullscreen too**:
   - `Integer` → use the largest integer multiple that fits the
     display, centered.  The existing logic already does this.
   - `Stretched` → allow fractional scaling; aspect bars only for
     aspect correction.  Replace the integer-preference branch with
     a simple float scale.
3. **Escape is NOT fullscreen-exit** — already correct; Escape
   dismisses the overlay, not fullscreen.

---

## 3. Control Overlay — Hold vs Tap

### Current State

Toggle-on-key-down.  A comment explains why: Ctrl+Click maps to
right-click on macOS, breaking overlay button interaction.

### Design

The right-click problem only matters in **hold** (peek) mode because
the user holds Ctrl while clicking.  Solution: while overlay is
visible and Ctrl is held, suppress the macOS Ctrl+Click→right-click
mapping by consuming the `SDL_EVENT_MOUSE_BUTTON_DOWN` with
`button == SDL_BUTTON_RIGHT` when Ctrl is physically held.
Alternatively, since ImGui receives these events via its own SDL
processing, simply ignore right-click events during overlay display.

#### State Machine

```
Idle → CtrlDown → [timer 250ms] → HoldMode (overlay visible)
                                         ↓ CtrlUp → dismiss
Idle → CtrlDown → CtrlUp (< 250ms) → StickyMode (overlay stays)
                                         ↓ CtrlDown or Escape → dismiss
```

Implementation in `runLoop()`:

```cpp
enum class OverlayMode { Hidden, PeekPending, Peek, Sticky };
OverlayMode overlayMode_ = OverlayMode::Hidden;
uint64_t    ctrlDownTick_ = 0;
static constexpr uint64_t kPeekThresholdMs = 250;
```

On `KEY_DOWN Ctrl`:
- If `Hidden`: set `PeekPending`, record `ctrlDownTick_`, show overlay.
- If `Sticky`: hide overlay, return to `Hidden`.

On `KEY_UP Ctrl`:
- If `PeekPending`: transition to `Sticky` (was a tap).
- If `Peek`: hide overlay, return to `Hidden`.

Each frame, if `PeekPending` and elapsed > 250 ms, transition to
`Peek`.

#### Ctrl Shortcuts

While in `PeekPending` or `Peek`, a second key press (e.g. `F`)
fires the shortcut, briefly flashes confirmation, and stays in the
same hold mode (dismissed on Ctrl release).

Implementation: maintain a lookup table `ctrl_shortcuts[]` mapping
scancodes to actions.  On keydown while overlay is visible and Ctrl
held, dispatch the action:

```cpp
static const std::unordered_map<SDL_Scancode, Action> kShortcuts = {
    {SDL_SCANCODE_F,     Action::ToggleFullscreen},
    {SDL_SCANCODE_M,     Action::ToggleScaling},
    {SDL_SCANCODE_S,     Action::Screenshot},
    {SDL_SCANCODE_RIGHT, Action::SpeedUp},
    {SDL_SCANCODE_LEFT,  Action::SpeedDown},
    {SDL_SCANCODE_0,     Action::SpeedReset},
    {SDL_SCANCODE_P,     Action::TogglePaused},
    {SDL_SCANCODE_I,     Action::InsertDisk},
    {SDL_SCANCODE_R,     Action::Reboot},
};
```

---

## 4. Overlay Panel Layout Redesign

### Current State

Four ImGui tabs (Machine, Display, Speed, Advanced) using
`ImGui::BeginTabBar`.

### Design

Replace the tab bar with a single flat panel.  The panel has two
zones separated by a thin horizontal line:

```
┌─────────────────────────────────────┐
│  Insert Disk    Fullscreen   Scaling │  Primary row 1
│  Speed: [1×][2×][4×]...[∞]         │  Primary row 2
│  Screenshot     Reboot    Power Off  │  Primary row 3
├─────────────────────────────────────┤
│  ▸ Advanced                          │  Collapsible
│    Interrupt  Filter  Stopped        │
│    Background  AutoSlow  About       │
└─────────────────────────────────────┘
```

Use `ImGui::CollapsingHeader("Advanced")` for the lower section —
closed by default.

Panel is positioned with `ImGui::SetNextWindowPos()` centered in the
viewport and sized to approximately 400×320 px (scaled to current DPI
via `ImGui::GetIO().FontGlobalScale`).

The semi-transparent scrim is drawn first as a fullscreen
`ImGui::GetBackgroundDrawList()->AddRectFilled()` with
`IM_COL32(0, 0, 0, 160)`.

Speed control: use `ImGui::RadioButton` or a row of small toggle
buttons highlighting the current speed.

### Sizing for 512×342

The panel must never exceed 400×320 logical pixels.  On a Mac Plus
(512×342 viewport at 1×), this leaves 56 px horizontal and 11 px
vertical margin — tight but workable.  The design favours compact
buttons (icon-sized where possible) to stay within this budget.

---

## 5. Screenshot to Clipboard

### Current State

Button exists, body is a TODO comment.

### Design

1. Grab raw framebuffer: `shell_->getFramebuffer()` returns
   `uint32_t*` in BGRA8888 layout, size `emuTexW_ × emuTexH_`.
2. Encode to PNG in memory using `stb_image_write.h` (already
   available via the ImGui dependency tree):
   ```cpp
   #define STB_IMAGE_WRITE_IMPLEMENTATION  // in one .cpp
   #include "stb_image_write.h"
   // ...
   stbi_write_png_to_func(writeCallback, &pngBuf, w, h, 4, pixels, w*4);
   ```
   The pixel order needs a BGRA→RGBA swizzle pass before encoding
   (stb expects RGBA).  A tight loop over `w*h` pixels with byte
   swap is sufficient.
3. Copy PNG data to clipboard using `SDL_SetClipboardData()`
   (SDL 3.0+), which handles platform differences internally
   (NSPasteboard on macOS, Win32 on Windows, X11/Wayland on Linux).
   No Obj-C++ or platform-specific code required.

   Encapsulate behind:
   ```cpp
   void HostClipSetImage(const uint8_t* pngData, size_t len);
   ```
   Single implementation in `clipboard_image.cpp` using
   `SDL_SetClipboardData` with MIME type `"image/png"`.

4. Overlay feedback: flash "Copied!" text for ~1 second after
   capture.

---

## 6. Insert Disk — Native File Dialog

### Current State

Button is a stub.

### Design

Use SDL3's async file dialog API:

```cpp
SDL_ShowOpenFileDialog(
    fileDialogCallback, userdata, window_,
    filters, numFilters, defaultPath, allowMultiple);
```

Filter list:
```cpp
SDL_DialogFileFilter filters[] = {
    {"Disk Images", "dsk;img;hfs;dmg;iso;image;dc42"},
    {"All Files",   "*"},
};
```

The callback receives a path (or null on cancel).  On success, call
`Sony_Insert1a(path)` — the same codepath used for drag-and-drop.

While the dialog is open, the emulation continues running (SDL3 file
dialogs are non-blocking).  The overlay should dismiss when the dialog
opens (the user has committed to an action).

---

## 7. Mouse Behaviour

### 7a. Windowed Absolute Mode

Already implemented.  The cursor is hidden inside the window and the
guest cursor tracks the host.  The `mouseInEmuView` lambda handles
coordinate mapping correctly for both 1× and scaled viewports.

With the new Stretched mode, the same formula works because
`emuViewOriginX/Y` and `emuViewW/H` are set each frame by
`displayEmulatorImage()`.

### 7b. Fullscreen Relative Mode

Already implemented via `SDL_SetWindowRelativeMouseMode`.  Deltas are
forwarded directly to the guest.

### 7c. Mouse-Up Outside Window (Bug Fix)

**Problem**: SDL doesn't report button-up events once the cursor
leaves the window.

**Solution**: Use `SDL_CaptureMouse(true)` on `MouseButtonDown`.
This tells SDL to deliver mouse events even when the cursor is outside
the window.  On `MouseButtonUp`, call `SDL_CaptureMouse(false)`.

```cpp
case SDL_EVENT_MOUSE_BUTTON_DOWN:
    SDL_CaptureMouse(true);
    // ... existing handling ...
    break;
case SDL_EVENT_MOUSE_BUTTON_UP:
    SDL_CaptureMouse(false);
    // ... existing handling ...
    break;
```

Additionally, on `SDL_EVENT_WINDOW_FOCUS_LOST`, synthesize a
button-up event if a button was held — the user Alt-Tabbed away.

### 7d. Cursor Re-appear on Window Leave

**Problem**: Host cursor occasionally fails to reappear.

**Root cause** (likely): `SDL_ShowCursor` is called but the window
still has grab or relative mode active, or the show call races with
the hide call from the next frame.

**Fix**: On `SDL_EVENT_MOUSE_LEAVE_WINDOW`, unconditionally call
`SDL_ShowCursor()` and clear any pending hide state.  Use a
`cursorHidden_` bool to coalesce redundant show/hide calls:

```cpp
case SDL_EVENT_WINDOW_MOUSE_LEAVE:
    if (cursorHidden_) { SDL_ShowCursor(); cursorHidden_ = false; }
    break;
```

---

## 8. Keyboard — Command Key Passthrough

### Current State

Command (⌘) is already forwarded to the guest as `MKC_formac_Command`.
However, macOS intercepts some Command combos at the OS level
(Cmd+H, Cmd+Tab).

### Design

Prevent the app from handling `Cmd+Q` as quit:

- SDL3: The app doesn't call `SDL_Quit()` on
  `SDL_EVENT_QUIT` — instead, treat the event as ignorable (or only
  honour it from the overlay's Power Off button).  Already partially
  in place.
- macOS menu bar: Remove or neuter the Quit menu item's key equivalent
  (set it to nothing in the Cocoa setup code).  The `Cmd+Q` scancode
  will then flow through to SDL as a normal key.

For truly OS-reserved combos (Cmd+Tab, Cmd+H), there is no fix at the
application level — document this as a platform limitation.

### Right Option → Guest Control

Already mapped in `sdl_keyboard.cpp`:
```cpp
t[SDL_SCANCODE_RALT] = MKC_formac_Control;
```

No changes needed.

---

## 9. About Panel

### Current State

Shows a folder icon and presumably minimal info.

### Design

Replace the Advanced/About section content with:

```
┌──────────────────────────────────────┐
│         Maxi vMac                    │
│   Classic Macintosh Emulator         │
│                                      │
│   Licensed under GNU GPL v2          │
│   github.com/user/maxivmac  [link]   │
└──────────────────────────────────────┘
```

Implementation: `ImGui::TextWrapped()` for the description,
`ImGui::TextLinkOpenURL()` (ImGui 1.91+) for the GitHub link.
No icon needed — just text.

The macOS About menu item (`orderFrontStandardAboutPanel:`) should be
overridden to show the same panel in-overlay rather than a system
sheet, or simply open the overlay to the About section.

---

## 10. Scaling Mode — Texture Filtering Interaction

The existing Filter control (Nearest/Linear) maps to
`glTexParameteri(GL_TEXTURE_MIN/MAG_FILTER)`.

- **Integer mode + Nearest** (default): pixel-perfect, no blurring.
- **Integer mode + Linear**: identical to Nearest (integer multiples
  produce no fractional texel coordinates).
- **Stretched mode + Nearest**: sharp but pixelated at non-integer
  scales.
- **Stretched mode + Linear**: bilinear smoothing across pixels.

No additional design needed — the existing `setTextureFilter()` call
works regardless of scaling mode.

---

## 11. Speed Controls

### Current State

Speed values stored in globals, buttons in the Speed tab.

### Design

Model speed as a list of presets:

```cpp
static constexpr int kSpeedPresets[] = {1, 2, 4, 8, 16, 32, 0};
// 0 = unlimited
int speedIndex_ = 0;  // index into kSpeedPresets
```

`Ctrl+→` increments index (clamped), `Ctrl+←` decrements,
`Ctrl+0` resets to index 0.

The overlay renders these as a horizontal button row.  The active
preset is highlighted.

---

## 12. Paused State (Stopped)

### Current State

`g_speedStopped` pauses emulation.

### Design

`Ctrl+P` toggles `g_speedStopped`.  When stopped:
- No emulation ticks run (already handled in the shell tick loop).
- The screen remains visible (static framebuffer).
- The overlay remains accessible.
- Mouse/keyboard are not forwarded (already gated by dispatchEvent).

Add a visual indicator (small "PAUSED" text in the overlay title or a
dimming effect on the viewport border) so the user knows emulation
is halted.

---

## 13. Run in Background

### Current State

`g_runInBackground` global controls whether emulation continues when
the window loses focus.

### Design

On `SDL_EVENT_WINDOW_FOCUS_LOST`:
- If `g_runInBackground` is false, set an internal
  `backgrounded_ = true` flag.  The tick loop skips emulation.
  Show host cursor.
- If true, emulation continues but input is not forwarded (already
  gated).

On `SDL_EVENT_WINDOW_FOCUS_GAINED`:
- Clear `backgrounded_`.  Re-hide cursor if appropriate.

No changes to the rendering path — the last frame stays on screen.

---

## 14. Implementation Order

Suggested sequence (each phase is independently shippable):

| Phase | Feature                        | Difficulty |
|-------|--------------------------------|------------|
| 1     | Resizable window + Integer snap | Medium     |
| 2     | Stretched scaling mode          | Easy       |
| 3     | Fullscreen dark border colour   | Trivial    |
| 4     | Overlay hold/tap state machine  | Medium     |
| 5     | Ctrl shortcuts dispatch         | Easy       |
| 6     | Overlay layout redesign         | Medium     |
| 7     | Insert Disk file dialog         | Easy       |
| 8     | Screenshot to clipboard         | Medium     |
| 9     | Mouse-up outside window fix     | Easy       |
| 10    | Cursor reappear fix             | Easy       |
| 11    | Command key / About panel       | Easy       |

---

## 15. Files Affected

| File                          | Changes                                      |
|-------------------------------|----------------------------------------------|
| `src/platform/imgui_backend.h`  | ScalingMode enum, overlay state machine fields, snapping flag |
| `src/platform/imgui_backend.cpp`| Window creation flags, resize handler, viewport drawing, hold/tap logic, shortcut dispatch, screenshot, file dialog, mouse capture |
| `src/platform/imgui_overlay.h`  | Removed tabs, new flat layout API             |
| `src/platform/imgui_overlay.cpp`| Full rewrite of panel layout                  |
| `src/platform/emulator_shell.h` | ScalingMode storage, speed preset index       |
| `src/platform/emulator_shell.cpp`| Speed preset logic, paused indicator         |
| `src/platform/clipboard_image.cpp`  | New: SDL3 clipboard image (all platforms) |
| `CMakeLists.txt`              | Add clipboard_image.cpp, stb_image_write  |
