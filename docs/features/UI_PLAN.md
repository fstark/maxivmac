# UI — Implementation Plan

Design: [UI_DESIGN.md](UI_DESIGN.md)
Spec: [UI.md](UI.md)

| Phase | Description                                 | Status |
|-------|---------------------------------------------|--------|
| 1     | ScalingMode enum + resizable window         |        |
| 2     | Integer-snap resize handler                 |        |
| 3     | Stretched scaling mode (windowed)           |        |
| 4     | Fullscreen rendering (both modes)           |        |
| 5     | Overlay hold/tap state machine              |        |
| 6     | Ctrl shortcut dispatch                      |        |
| 7     | Overlay panel layout redesign               |        |
| 8     | Insert Disk native file dialog              |        |
| 9     | Screenshot to clipboard                     |        |
| 10    | Mouse fixes (capture + cursor reappear)     |        |
| 11    | Command key passthrough + About panel       |        |

Build gate: `cmake --preset macos && cmake --build --preset macos`
Test gate:  `./bld/macos/tests`

---

## Phase 1 — ScalingMode Enum + Resizable Window

Introduce the `ScalingMode` enum and make the emulation window
resizable.  No snap logic yet — that comes in Phase 2.

### 1.1 — Add `ScalingMode` enum

File: `src/platform/imgui_backend.h`

Add before the `ImGuiBackend` class:

```cpp
enum class ScalingMode : uint8_t { Integer, Stretched };
```

Add member to `ImGuiBackend` private section:

```cpp
ScalingMode scalingMode_ = ScalingMode::Integer;
```

Add public accessor/mutator:

```cpp
ScalingMode scalingMode() const { return scalingMode_; }
void setScalingMode(ScalingMode m);
```

### 1.2 — Make window resizable

File: `src/platform/imgui_backend.cpp`, in `createWindow()`

Change window flags from:
```cpp
Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY;
```
to:
```cpp
Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE;
```

Only the emulation window gets `SDL_WINDOW_RESIZABLE` — leave the
model selector window (`createSelectorWindow()`) unchanged.

### 1.3 — Remove `g_wantMagnify` usage

The `g_wantMagnify` boolean and its associated `windowScale_` member
become obsolete.  The window scale is now determined dynamically from
the window size.  In this phase, replace the magnification paths:

- Remove the `g_wantMagnify` extern from `imgui_overlay.cpp`
- Remove the Zoom 1×/2× radio buttons from the overlay (they are
  replaced by ScalingMode toggle in Phase 7)
- In `EmulatorShell::createMainWindow()`, compute the initial window
  size as `guestW * 2, guestH * 2` (or `guestW * 1` if 2× doesn't
  fit), and pass those dimensions directly.  Remove the
  `windowScale_` multiplication.

### 1.4 — Add `setScalingMode()` implementation

File: `src/platform/imgui_backend.cpp`

```cpp
void ImGuiBackend::setScalingMode(ScalingMode m)
{
    if (scalingMode_ == m) return;
    scalingMode_ = m;
    // Phase 2 adds snap-on-switch logic here
}
```

### Fence

- [ ] `ScalingMode` enum exists in `imgui_backend.h`
- [ ] Emulation window is resizable (can drag edges freely)
- [ ] `g_wantMagnify` is no longer referenced by the overlay
- [ ] Build clean: `cmake --build --preset macos`
- [ ] Commit: `"ui: phase 1 — ScalingMode enum, resizable window"`

---

## Phase 2 — Integer-Snap Resize Handler

Add the logic that snaps the window to integer multiples of guest
resolution in Integer scaling mode.

### 2.1 — Add snap state

File: `src/platform/imgui_backend.h`, private section:

```cpp
bool snapping_ = false;   // guards recursive resize events
int currentScale_ = 2;    // current integer multiple (1, 2, 3…)
```

### 2.2 — Handle `SDL_EVENT_WINDOW_RESIZED`

File: `src/platform/imgui_backend.cpp`, in the event loop.

Currently the resize event sets `pEvt.type = PlatformEvent::Type::WindowResized`
and does nothing else.  Add snap logic:

```cpp
case SDL_EVENT_WINDOW_RESIZED:
{
    if (snapping_) break;  // avoid recursion
    if (uiState_ == UIState::Windowed && scalingMode_ == ScalingMode::Integer)
    {
        int newW = event.window.data1;
        int newH = event.window.data2;
        int scaleX = std::max(1, newW / emuTexW_);
        int scaleY = std::max(1, newH / emuTexH_);
        int scale  = std::min(scaleX, scaleY);
        int snapW  = emuTexW_ * scale;
        int snapH  = emuTexH_ * scale;
        if (snapW != newW || snapH != newH)
        {
            snapping_ = true;
            SDL_SetWindowSize(window_, snapW, snapH);
            snapping_ = false;
        }
        currentScale_ = scale;
    }
    break;
}
```

Key details:
- `event.window.data1` / `data2` provide the new pixel dimensions
  in SDL3.
- The snapping flag prevents the `SDL_SetWindowSize` call from
  triggering infinite recursion through a second resize event.
- In Stretched mode, no snapping occurs — the window stays at
  whatever size the user dragged to.

### 2.3 — Initial scale computation at boot

File: `src/platform/imgui_backend.cpp` or
`src/platform/emulator_shell.cpp` (wherever `createMainWindow` is).

```cpp
PlatformDisplayBounds bounds;
backend_->getDisplayBounds(&bounds);
int scale = 2;
if (emuTexW_ * 2 > bounds.w || emuTexH_ * 2 > bounds.h)
    scale = 1;
currentScale_ = scale;
```

Pass `emuTexW_ * scale, emuTexH_ * scale` to `createWindow`.

### 2.4 — Snap on mode switch (Stretched → Integer)

In `setScalingMode()`, if switching to `Integer` while windowed:

```cpp
void ImGuiBackend::setScalingMode(ScalingMode m)
{
    if (scalingMode_ == m) return;
    scalingMode_ = m;
    if (m == ScalingMode::Integer && uiState_ == UIState::Windowed)
    {
        int w, h;
        SDL_GetWindowSize(window_, &w, &h);
        int scaleX = std::max(1, w / emuTexW_);
        int scaleY = std::max(1, h / emuTexH_);
        int scale  = std::min(scaleX, scaleY);
        snapping_ = true;
        SDL_SetWindowSize(window_, emuTexW_ * scale, emuTexH_ * scale);
        snapping_ = false;
        currentScale_ = scale;
    }
}
```

### Fence

- [ ] Dragging window edge in Integer mode snaps to discrete sizes
- [ ] Window never has black bars in Integer+Windowed mode
- [ ] Switching Stretched→Integer snaps to nearest valid size
- [ ] Boot opens at 2× (or 1× if 2× exceeds display)
- [ ] Build clean
- [ ] Commit: `"ui: phase 2 — integer-snap resize handler"`

---

## Phase 3 — Stretched Scaling Mode (Windowed)

In Stretched mode the viewport preserves aspect ratio within the
freely-resized window.

### 3.1 — Modify `drawViewportWindowed()`

File: `src/platform/imgui_backend.cpp`

Replace the current unconditional full-window viewport with a
mode-dependent path:

```cpp
void ImGuiBackend::drawViewportWindowed()
{
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(displaySize);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoSavedSettings;
    emuViewportHovered_ = false;

    if (scalingMode_ == ScalingMode::Stretched)
    {
        // Dark background for letterbox/pillarbox bars
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    }

    if (ImGui::Begin("Macintosh", nullptr, flags))
    {
        emuViewportHovered_ = ImGui::IsWindowHovered();
        if (scalingMode_ == ScalingMode::Integer)
        {
            // Integer mode: viewport fills window exactly (guaranteed by snap)
            ImGui::SetCursorPos(ImVec2(0, 0));
            displayEmulatorImage(displaySize.x, displaySize.y);
        }
        else
        {
            // Stretched mode: aspect-preserving centered viewport
            float emuAspect = static_cast<float>(emuTexW_) / emuTexH_;
            float winAspect = displaySize.x / displaySize.y;
            float viewW, viewH;
            if (emuAspect > winAspect)
            {
                viewW = displaySize.x;
                viewH = displaySize.x / emuAspect;
            }
            else
            {
                viewH = displaySize.y;
                viewW = displaySize.y * emuAspect;
            }
            float offsetX = (displaySize.x - viewW) * 0.5f;
            float offsetY = (displaySize.y - viewH) * 0.5f;
            ImGui::SetCursorPos(ImVec2(offsetX, offsetY));
            displayEmulatorImage(viewW, viewH);
        }
    }
    ImGui::End();

    if (scalingMode_ == ScalingMode::Stretched)
    {
        ImGui::PopStyleColor();
    }
}
```

### 3.2 — Mouse coordinate mapping

No changes needed.  The existing `mouseInEmuView` lambda already uses
`emuViewOriginX_/Y_` and `emuViewW_/H_` which are set every frame by
`displayEmulatorImage()`.  Stretched mode naturally produces correct
mapping because the origin and size change with the viewport rect.

### Fence

- [ ] In Stretched mode, viewport preserves aspect ratio
- [ ] Letterbox/pillarbox bars are black
- [ ] Mouse clicks correctly map in both modes
- [ ] Build clean
- [ ] Commit: `"ui: phase 3 — stretched scaling mode (windowed)"`

---

## Phase 4 — Fullscreen Rendering (Both Modes)

Apply ScalingMode to fullscreen and fix the background colour.

### 4.1 — Dark grey border for fullscreen

File: `src/platform/imgui_backend.cpp`, in `drawViewportFullscreen()`

Change the window background from solid black:
```cpp
ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1));
```
to dark grey:
```cpp
ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.102f, 0.102f, 0.102f, 1.0f));
```

### 4.2 — ScalingMode in fullscreen

In `drawViewportFullscreen()`, the existing integer-preference logic
only applies when `scalingMode_ == ScalingMode::Integer`.  In
Stretched mode, skip the integer snap:

```cpp
float scaledW, scaledH;
if (emuAspect > dispAspect)
{
    scaledW = displaySize.x;
    scaledH = displaySize.x / emuAspect;
}
else
{
    scaledH = displaySize.y;
    scaledW = displaySize.y * emuAspect;
}

if (scalingMode_ == ScalingMode::Integer)
{
    int intScale = static_cast<int>(scaledW / emuTexW_);
    if (intScale >= 1)
    {
        float intW = emuTexW_ * intScale;
        float intH = emuTexH_ * intScale;
        if (intW <= displaySize.x && intH <= displaySize.y)
        {
            scaledW = intW;
            scaledH = intH;
        }
    }
}
// else: Stretched — use fractional scaledW/scaledH as-is
```

### Fence

- [ ] Fullscreen borders are dark grey (#1A1A1A)
- [ ] Integer mode in fullscreen shows largest integer multiple, centered
- [ ] Stretched mode fills display (aspect bars only)
- [ ] Build clean
- [ ] Commit: `"ui: phase 4 — fullscreen rendering for both scaling modes"`

---

## Phase 5 — Overlay Hold/Tap State Machine

Replace the toggle-on-keydown overlay with a hold (peek) vs tap
(sticky) state machine.

### 5.1 — Add overlay state to ImGuiBackend

File: `src/platform/imgui_backend.h`, private section.

Replace:
```cpp
bool overlayVisible_ = false;
```
with:
```cpp
enum class OverlayMode : uint8_t { Hidden, PeekPending, Peek, Sticky };
OverlayMode overlayMode_ = OverlayMode::Hidden;
uint64_t ctrlDownTick_ = 0;
static constexpr uint64_t kPeekThresholdMs = 250;
```

Add a helper:
```cpp
bool isOverlayVisible() const
{
    return overlayMode_ != OverlayMode::Hidden;
}
```

### 5.2 — Implement state transitions

File: `src/platform/imgui_backend.cpp`, in the event loop.

Replace the existing Ctrl-toggle block with:

```cpp
// --- Ctrl key for overlay ---
if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
    (event.key.scancode == SDL_SCANCODE_LCTRL ||
     event.key.scancode == SDL_SCANCODE_RCTRL))
{
    switch (overlayMode_)
    {
    case OverlayMode::Hidden:
        overlayMode_ = OverlayMode::PeekPending;
        ctrlDownTick_ = SDL_GetTicks();
        shell_->forceShowCursor();
        break;
    case OverlayMode::Sticky:
        overlayMode_ = OverlayMode::Hidden;
        break;
    default:
        break;
    }
    continue;
}
if (event.type == SDL_EVENT_KEY_UP &&
    (event.key.scancode == SDL_SCANCODE_LCTRL ||
     event.key.scancode == SDL_SCANCODE_RCTRL))
{
    switch (overlayMode_)
    {
    case OverlayMode::PeekPending:
        overlayMode_ = OverlayMode::Sticky;  // tap → sticky
        break;
    case OverlayMode::Peek:
        overlayMode_ = OverlayMode::Hidden;  // hold released → dismiss
        break;
    default:
        break;
    }
    continue;
}
// Escape dismisses all modes
if (isOverlayVisible() && event.type == SDL_EVENT_KEY_DOWN &&
    event.key.scancode == SDL_SCANCODE_ESCAPE)
{
    overlayMode_ = OverlayMode::Hidden;
    continue;
}
```

### 5.3 — PeekPending → Peek timer transition

At the top of the frame loop (before event processing), add:

```cpp
if (overlayMode_ == OverlayMode::PeekPending &&
    (SDL_GetTicks() - ctrlDownTick_) >= kPeekThresholdMs)
{
    overlayMode_ = OverlayMode::Peek;
}
```

### 5.4 — Suppress Ctrl+Click → right-click

While the overlay is visible and Ctrl is held (PeekPending or Peek),
suppress right-click events to avoid the macOS Ctrl+Click mapping
interfering with overlay interactions:

```cpp
if (isOverlayVisible() && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
    event.button.button == SDL_BUTTON_RIGHT)
{
    continue;  // swallow Ctrl+Click
}
```

### 5.5 — Replace `overlayVisible_` references

All existing checks for `overlayVisible_` in `runLoop()` become
`isOverlayVisible()`.  The overlay draw call is now gated on
`isOverlayVisible()` instead of the boolean.

### Fence

- [ ] Tapping Ctrl (quick press+release) opens sticky overlay
- [ ] Holding Ctrl >250ms shows peek overlay; releasing dismisses
- [ ] Escape dismisses in any mode
- [ ] Ctrl+Click does not fire a right-click in the overlay
- [ ] Build clean
- [ ] Commit: `"ui: phase 5 — overlay hold/tap state machine"`

---

## Phase 6 — Ctrl Shortcut Dispatch

Wire up keyboard shortcuts that fire while Ctrl is held.

### 6.1 — Define actions enum

File: `src/platform/imgui_backend.h`

```cpp
enum class UIAction : uint8_t
{
    None,
    ToggleFullscreen,
    ToggleScaling,
    Screenshot,
    SpeedUp,
    SpeedDown,
    SpeedReset,
    TogglePaused,
    InsertDisk,
    Reboot,
};
```

### 6.2 — Shortcut lookup table

File: `src/platform/imgui_backend.cpp`, file-scope:

```cpp
#include <array>

struct ShortcutEntry { SDL_Scancode scancode; UIAction action; };

static constexpr std::array kShortcuts = {
    ShortcutEntry{SDL_SCANCODE_F,     UIAction::ToggleFullscreen},
    ShortcutEntry{SDL_SCANCODE_M,     UIAction::ToggleScaling},
    ShortcutEntry{SDL_SCANCODE_S,     UIAction::Screenshot},
    ShortcutEntry{SDL_SCANCODE_RIGHT, UIAction::SpeedUp},
    ShortcutEntry{SDL_SCANCODE_LEFT,  UIAction::SpeedDown},
    ShortcutEntry{SDL_SCANCODE_0,     UIAction::SpeedReset},
    ShortcutEntry{SDL_SCANCODE_P,     UIAction::TogglePaused},
    ShortcutEntry{SDL_SCANCODE_I,     UIAction::InsertDisk},
    ShortcutEntry{SDL_SCANCODE_R,     UIAction::Reboot},
};
```

### 6.3 — Dispatch on keydown while overlay visible

In the event loop, after the Ctrl key handling but before the Escape
check:

```cpp
if (isOverlayVisible() && event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
{
    UIAction action = UIAction::None;
    for (const auto &s : kShortcuts)
    {
        if (event.key.scancode == s.scancode)
        {
            action = s.action;
            break;
        }
    }
    if (action != UIAction::None)
    {
        executeAction(action);
        continue;
    }
}
```

### 6.4 — `executeAction()` method

File: `src/platform/imgui_backend.cpp`

```cpp
void ImGuiBackend::executeAction(UIAction action)
{
    switch (action)
    {
    case UIAction::ToggleFullscreen:
        if (uiState_ == UIState::Fullscreen) enterWindowed();
        else enterFullscreen();
        overlayMode_ = OverlayMode::Hidden;
        break;
    case UIAction::ToggleScaling:
        setScalingMode(scalingMode_ == ScalingMode::Integer
                       ? ScalingMode::Stretched : ScalingMode::Integer);
        break;
    case UIAction::Screenshot:
        captureScreenshot();  // Phase 9
        break;
    case UIAction::SpeedUp:
        adjustSpeed(+1);
        break;
    case UIAction::SpeedDown:
        adjustSpeed(-1);
        break;
    case UIAction::SpeedReset:
        setSpeed(0);  // index 0 = 1×
        break;
    case UIAction::TogglePaused:
        g_speedStopped = !g_speedStopped;
        break;
    case UIAction::InsertDisk:
        openFileDialog();  // Phase 8
        overlayMode_ = OverlayMode::Hidden;
        break;
    case UIAction::Reboot:
        g_wantMacReset = true;
        overlayMode_ = OverlayMode::Hidden;
        break;
    default:
        break;
    }
}
```

Add declaration in `imgui_backend.h` private section:
```cpp
void executeAction(UIAction action);
```

### 6.5 — Speed preset helpers

File: `src/platform/imgui_backend.cpp`

```cpp
static constexpr uint8_t kSpeedPresets[] = {1, 2, 4, 8, 16, 32, 0};
static constexpr int kSpeedPresetCount = 7;

void ImGuiBackend::adjustSpeed(int delta)
{
    int idx = 0;
    for (int i = 0; i < kSpeedPresetCount; ++i)
    {
        if (kSpeedPresets[i] == g_speedValue) { idx = i; break; }
    }
    idx = std::clamp(idx + delta, 0, kSpeedPresetCount - 1);
    g_speedValue = kSpeedPresets[idx];
}

void ImGuiBackend::setSpeed(int idx)
{
    idx = std::clamp(idx, 0, kSpeedPresetCount - 1);
    g_speedValue = kSpeedPresets[idx];
}
```

Add stubs for `captureScreenshot()` and `openFileDialog()` (filled in
later phases):

```cpp
void ImGuiBackend::captureScreenshot() { /* Phase 9 */ }
void ImGuiBackend::openFileDialog()    { /* Phase 8 */ }
```

### Fence

- [ ] `Ctrl+F` toggles fullscreen
- [ ] `Ctrl+M` toggles scaling mode
- [ ] `Ctrl+→` / `Ctrl+←` cycle speed presets
- [ ] `Ctrl+0` resets to 1×
- [ ] `Ctrl+P` pauses/unpauses
- [ ] `Ctrl+R` reboots
- [ ] Shortcuts work during peek mode without making overlay sticky
- [ ] Build clean
- [ ] Commit: `"ui: phase 6 — Ctrl shortcut dispatch"`

---

## Phase 7 — Overlay Panel Layout Redesign

Replace the 4-tab overlay with a single flat panel.

### 7.1 — Update `ControlOverlay` class

File: `src/platform/imgui_overlay.h`

Replace the tab-drawing methods:

```cpp
class ControlOverlay
{
public:
    // Returns true if a UIState change was requested.
    bool draw(UIState currentState, EmulatorShell *shell, ImGuiBackend *backend,
              UIState &requestedState);

private:
    void drawPrimaryControls(UIState currentState, EmulatorShell *shell,
                             ImGuiBackend *backend, UIState &requestedState);
    void drawAdvancedControls(ImGuiBackend *backend);
    void drawAbout();

    // Flash feedback
    const char *flashMsg_ = nullptr;
    uint64_t flashExpiry_ = 0;
};
```

### 7.2 — Rewrite `draw()` body

File: `src/platform/imgui_overlay.cpp`

New layout structure (pseudocode → real ImGui calls):

```cpp
bool ControlOverlay::draw(UIState currentState, EmulatorShell *shell,
                          ImGuiBackend *backend, UIState &requestedState)
{
    bool stateChanged = false;
    requestedState = currentState;

    // Full-viewport scrim
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        ImVec2(0, 0), ds, IM_COL32(0, 0, 0, 160));

    // Centered panel
    float panelW = 400, panelH = 320;
    ImVec2 panelPos((ds.x - panelW) * 0.5f, (ds.y - panelH) * 0.5f);
    ImGui::SetNextWindowPos(panelPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.16f, 0.95f));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("##OverlayPanel", nullptr, flags))
    {
        drawPrimaryControls(currentState, shell, backend, requestedState);
        if (requestedState != currentState) stateChanged = true;
        ImGui::Separator();
        drawAdvancedControls(backend);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    return stateChanged;
}
```

### 7.3 — Primary controls

```cpp
void ControlOverlay::drawPrimaryControls(UIState currentState, EmulatorShell *shell,
                                         ImGuiBackend *backend, UIState &requestedState)
{
    float btnW = 110, btnH = 32, sp = 8;

    // Row 1: Insert Disk, Fullscreen, Scaling
    if (ImGui::Button("Insert Disk", ImVec2(btnW, btnH)))
        backend->executeAction(UIAction::InsertDisk);
    ImGui::SameLine(0, sp);
    bool isFS = (currentState == UIState::Fullscreen);
    if (ImGui::Button(isFS ? "Windowed" : "Fullscreen", ImVec2(btnW, btnH)))
        requestedState = isFS ? UIState::Windowed : UIState::Fullscreen;
    ImGui::SameLine(0, sp);
    const char *scaleLabel = (backend->scalingMode() == ScalingMode::Integer)
                             ? "Integer" : "Stretched";
    if (ImGui::Button(scaleLabel, ImVec2(btnW, btnH)))
        backend->executeAction(UIAction::ToggleScaling);

    ImGui::Spacing();

    // Row 2: Speed
    ImGui::Text("Speed:");
    ImGui::SameLine();
    static constexpr uint8_t kPresets[] = {1, 2, 4, 8, 16, 32, 0};
    static constexpr const char *kLabels[] = {"1x","2x","4x","8x","16x","32x","∞"};
    for (int i = 0; i < 7; ++i)
    {
        if (i > 0) ImGui::SameLine(0, 4);
        bool selected = (g_speedValue == kPresets[i]);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
        if (ImGui::SmallButton(kLabels[i])) g_speedValue = kPresets[i];
        if (selected) ImGui::PopStyleColor();
    }

    ImGui::Spacing();

    // Row 3: Screenshot, Reboot, Power Off
    if (ImGui::Button("Screenshot", ImVec2(btnW, btnH)))
        backend->executeAction(UIAction::Screenshot);
    ImGui::SameLine(0, sp);
    if (ImGui::Button("Reboot", ImVec2(btnW, btnH)))
        backend->executeAction(UIAction::Reboot);
    ImGui::SameLine(0, sp);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
    if (ImGui::Button("Power Off", ImVec2(btnW, btnH)))
        g_requestMacOff = true;
    ImGui::PopStyleColor();

    (void)shell;
}
```

### 7.4 — Advanced (collapsible)

```cpp
void ControlOverlay::drawAdvancedControls(ImGuiBackend *backend)
{
    if (ImGui::CollapsingHeader("Advanced"))
    {
        float btnW = 100, btnH = 28, sp = 8;
        // Row: Interrupt, Filter, Stopped
        if (ImGui::Button("Interrupt", ImVec2(btnW, btnH)))
            g_wantMacInterrupt = true;
        ImGui::SameLine(0, sp);

        bool isNearest = (backend->textureFilter() == TextureFilter::Nearest);
        const char *fLabel = isNearest ? "Filter: Near" : "Filter: Linear";
        if (ImGui::Button(fLabel, ImVec2(btnW, btnH)))
            backend->setTextureFilter(isNearest ? TextureFilter::Linear
                                                : TextureFilter::Nearest);
        ImGui::SameLine(0, sp);
        ImGui::Checkbox("Stopped", &g_speedStopped);

        // Row: Background, AutoSlow, About
        ImGui::Checkbox("Run in Background", &g_runInBackground);
        ImGui::SameLine(0, sp);
        bool autoSlow = !g_wantNotAutoSlow;
        if (ImGui::Checkbox("AutoSlow", &autoSlow))
            g_wantNotAutoSlow = !autoSlow;

        ImGui::Spacing();
        drawAbout();
    }
}
```

### 7.5 — About section

```cpp
void ControlOverlay::drawAbout()
{
    ImGui::Separator();
    ImGui::TextDisabled("maxivmac — Classic Macintosh Emulator");
    ImGui::TextDisabled("Licensed under GNU GPL v2");
    if (ImGui::TextLink("github.com/InvisibleUp/minivmac"))
    {
        SDL_OpenURL("https://github.com/InvisibleUp/minivmac");
    }
}
```

Note: `ImGui::TextLink()` was added in ImGui 1.91.  If the project
uses an older version, fall back to `ImGui::TextDisabled()` with a
comment URL.  Check `libs/imgui/imgui.h` version.

### Fence

- [ ] Overlay shows single flat panel (no tabs)
- [ ] Primary controls always visible
- [ ] Advanced section collapsed by default
- [ ] All buttons dispatch correct actions
- [ ] Panel fits within 400×320 px
- [ ] Build clean
- [ ] Commit: `"ui: phase 7 — overlay panel layout redesign"`

---

## Phase 8 — Insert Disk Native File Dialog

Use SDL3's asynchronous file dialog to let the user pick a disk image.

### 8.1 — Implement `openFileDialog()`

File: `src/platform/imgui_backend.cpp`

```cpp
static void fileDialogCallback(void *userdata, const char *const *filelist, int filter)
{
    (void)filter;
    auto *backend = static_cast<ImGuiBackend *>(userdata);
    if (filelist && filelist[0])
    {
        backend->shell()->insertDiskOrRom(filelist[0], false);
    }
}

void ImGuiBackend::openFileDialog()
{
    static const SDL_DialogFileFilter filters[] = {
        {"Disk Images", "dsk;img;hfs;dmg;iso;image;dc42"},
        {"All Files", "*"},
    };
    SDL_ShowOpenFileDialog(fileDialogCallback, this, window_,
                          filters, 2, nullptr, false);
    overlayMode_ = OverlayMode::Hidden;
}
```

Add `EmulatorShell *shell() { return shell_; }` as a public accessor
in `imgui_backend.h` if not already present.

### 8.2 — Wire existing "Insert Disk" overlay button

The existing `g_requestInsertDisk` path uses a different mechanism
(the old control-mode dialog).  Replace references to
`g_requestInsertDisk = true` in the overlay with calls to
`backend->executeAction(UIAction::InsertDisk)`.

### 8.3 — Thread safety note

`SDL_ShowOpenFileDialog` is non-blocking.  The callback is invoked on
the main thread (SDL guarantees this for dialog callbacks).
`insertDiskOrRom()` must therefore be safe to call from the main
thread — it already is (it just queues the path for the next tick).

### Fence

- [ ] Insert Disk button opens a native file dialog
- [ ] Selecting a `.hfs` / `.dsk` file mounts it
- [ ] Cancelling the dialog does nothing
- [ ] Overlay dismisses when dialog opens
- [ ] Build clean
- [ ] Commit: `"ui: phase 8 — native file dialog for Insert Disk"`

---

## Phase 9 — Screenshot to Clipboard

Capture the emulator framebuffer as PNG and place it on the system
clipboard.

### 9.1 — Add stb_image_write

File: `src/platform/stb_impl.cpp` (new file)

```cpp
// Single-file implementation of stb_image_write
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
```

stb_image_write.h is header-only.  Download from
https://github.com/nothings/stb or check if already present in
`libs/`.  If not, add to `libs/stb/stb_image_write.h`.

Add `src/platform/stb_impl.cpp` to `MINIVMAC_SOURCES` in
`CMakeLists.txt`.  Add include path `libs/stb` if needed.

### 9.2 — Clipboard image helper (SDL3, all platforms)

File: `src/platform/clipboard_image.h` (new)

```cpp
#pragma once
#include <cstdint>
#include <cstddef>

// Copy PNG data to the system clipboard via SDL3.
void HostClipSetImage(const uint8_t *pngData, size_t len);
```

File: `src/platform/clipboard_image.cpp` (new)

SDL3's `SDL_SetClipboardData()` handles platform differences
internally (NSPasteboard on macOS, Win32 clipboard on Windows,
X11/Wayland on Linux).  No Obj-C++ or platform `#ifdef`s needed.

```cpp
#include "platform/clipboard_image.h"
#include <SDL3/SDL.h>
#include <vector>
#include <cstring>

static std::vector<uint8_t> s_clipBuffer;

static const void *clipDataCallback(void *userdata, const char *mime,
                                    size_t *size)
{
    (void)userdata;
    if (std::strcmp(mime, "image/png") == 0)
    {
        *size = s_clipBuffer.size();
        return s_clipBuffer.data();
    }
    *size = 0;
    return nullptr;
}

void HostClipSetImage(const uint8_t *pngData, size_t len)
{
    s_clipBuffer.assign(pngData, pngData + len);
    static const char *mimes[] = {"image/png"};
    SDL_SetClipboardData(clipDataCallback, nullptr, nullptr, mimes, 1);
}
```

Add `src/platform/clipboard_image.cpp` to `MINIVMAC_SOURCES` in
`CMakeLists.txt`.  No additional framework linking required — SDL3
already links AppKit internally on macOS.

### 9.3 — Implement `captureScreenshot()`

File: `src/platform/imgui_backend.cpp`

```cpp
#include "platform/clipboard_image.h"
#include "stb_image_write.h"

static void pngWriteCallback(void *context, void *data, int size)
{
    auto *buf = static_cast<std::vector<uint8_t> *>(context);
    auto *bytes = static_cast<const uint8_t *>(data);
    buf->insert(buf->end(), bytes, bytes + size);
}

void ImGuiBackend::captureScreenshot()
{
    if (!shell_ || !shell_->getFramebuffer()) return;

    int w = emuTexW_;
    int h = emuTexH_;
    const uint32_t *src = reinterpret_cast<const uint32_t *>(shell_->getFramebuffer());

    // BGRA → RGBA swizzle
    std::vector<uint8_t> rgba(w * h * 4);
    for (int i = 0; i < w * h; ++i)
    {
        uint32_t px = src[i];
        rgba[i * 4 + 0] = (px >> 16) & 0xFF; // R (was in byte 2 of BGRA)
        rgba[i * 4 + 1] = (px >>  8) & 0xFF; // G
        rgba[i * 4 + 2] = (px >>  0) & 0xFF; // B
        rgba[i * 4 + 3] = 0xFF;               // A
    }

    std::vector<uint8_t> pngBuf;
    stbi_write_png_to_func(pngWriteCallback, &pngBuf, w, h, 4,
                           rgba.data(), w * 4);

    if (!pngBuf.empty())
    {
        HostClipSetImage(pngBuf.data(), pngBuf.size());
    }
}
```

### 9.4 — Flash feedback (optional)

After `captureScreenshot()` returns, set a flash message in the
overlay (visible for ~1 s).  Use `flashMsg_ = "Copied!"` and
`flashExpiry_ = SDL_GetTicks() + 1000`.  Draw it in `draw()` if
`SDL_GetTicks() < flashExpiry_`.

### Fence

- [ ] `Ctrl+S` or Screenshot button captures guest screen
- [ ] PNG data is on the system clipboard (paste into Preview.app)
- [ ] Pixel colours are correct (BGRA→RGBA swizzle verified)
- [ ] Build clean on macOS (Obj-C++ compiles)
- [ ] Commit: `"ui: phase 9 — screenshot to clipboard"`

---

## Phase 10 — Mouse Fixes

Fix the mouse-up-outside-window bug and the cursor reappear issue.

### 10.1 — Mouse capture for button tracking

File: `src/platform/imgui_backend.cpp`, mouse button events.

Add `SDL_CaptureMouse(true)` on button down:

```cpp
case SDL_EVENT_MOUSE_BUTTON_DOWN:
    SDL_CaptureMouse(true);
    if (relativeMouseMode_)
    { /* existing code */ }
    // ...
    break;
```

Add `SDL_CaptureMouse(false)` on button up:

```cpp
case SDL_EVENT_MOUSE_BUTTON_UP:
    SDL_CaptureMouse(false);
    if (relativeMouseMode_)
    { /* existing code */ }
    // ...
    break;
```

### 10.2 — Synthesize button-up on focus loss

```cpp
case SDL_EVENT_WINDOW_FOCUS_LOST:
    SDL_CaptureMouse(false);
    // Synthesize mouse-up so guest doesn't get stuck
    {
        PlatformEvent upEvt{};
        upEvt.type = PlatformEvent::Type::MouseButtonUp;
        shell_->dispatchEvent(upEvt);
    }
    break;
```

### 10.3 — Cursor reappear on window leave

Add a `cursorHidden_` tracking bool to avoid redundant SDL calls:

In `imgui_backend.h` private:
```cpp
bool cursorHidden_ = false;
```

In `hideCursor()`:
```cpp
void ImGuiBackend::hideCursor()
{
    if (!cursorHidden_) { SDL_HideCursor(); cursorHidden_ = true; }
}
```

In `showCursor()`:
```cpp
void ImGuiBackend::showCursor()
{
    if (cursorHidden_) { SDL_ShowCursor(); cursorHidden_ = false; }
}
```

On mouse leave:
```cpp
case SDL_EVENT_WINDOW_MOUSE_LEAVE:
    showCursor();
    break;
```

### Fence

- [ ] Dragging a guest scrollbar outside the window: mouse-up is received
- [ ] Alt-Tab away while clicking: guest gets mouse-up
- [ ] Moving cursor off the window edge: host cursor reappears
- [ ] Build clean
- [ ] Commit: `"ui: phase 10 — mouse capture and cursor fixes"`

---

## Phase 11 — Command Key Passthrough + About Panel

### 11.1 — Suppress Cmd+Q quit

File: `src/platform/imgui_backend.cpp`

In the event loop, ignore `SDL_EVENT_QUIT`:

```cpp
case SDL_EVENT_QUIT:
    // Do not quit — only Power Off button terminates
    continue;
```

The user terminates solely via the overlay Power Off or `Ctrl+R` for
reboot.  `g_requestMacOff` triggers the actual shutdown path.

If the macOS app delegate sends `applicationShouldTerminate:`, respond
with `NSTerminateCancel` — but SDL3 handles this internally when
`SDL_EVENT_QUIT` is consumed.  Verify Cmd+Q no longer exits and
instead reaches the guest as ⌘Q.

If SDL3 still strips Cmd+Q before it reaches the key event, use
`SDL_SetHint(SDL_HINT_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK, "0")` and
explore `SDL_HINT_MAC_BACKGROUND_APP` or `SDL_SetEventFilter` to
intercept the quit before SDL swallows it.  (Test empirically.)

### 11.2 — About panel in macOS menu

File: `src/platform/imgui_backend.cpp` (or a new
`src/platform/macos_menu.mm`)

Override the About menu item to toggle overlay visibility and scroll
to the About section (or just make it a no-op since the overlay has
all info).

This is low-priority; if SDL3 doesn't expose menu callbacks, simply
leave the AppKit About panel with the default info.plist values
(app name, version, copyright).

### 11.3 — About content

Already addressed in Phase 7 (`drawAbout()`).  Ensure the URL opens
in a browser on click.  Verify `SDL_OpenURL()` works on macOS.

### Fence

- [ ] Cmd+Q is forwarded to guest (does not quit emulator)
- [ ] Only Power Off button terminates emulation
- [ ] About text shows "maxivmac", GPL v2, GitHub link
- [ ] Link opens browser
- [ ] Build clean
- [ ] Commit: `"ui: phase 11 — Command key passthrough, About panel"`

---

## Testing Strategy

Most UI features are visual/interactive and cannot be meaningfully
unit-tested.  The testing approach is:

### Automated (unit tests)

| Test                        | What it verifies                                |
|-----------------------------|-------------------------------------------------|
| `test_integer_snap`         | Snap algorithm: given (newW, newH, guestW, guestH) → correct (scale, snapW, snapH) |
| `test_stretched_viewport`   | Aspect-ratio viewport: given (winW, winH, guestW, guestH) → correct (viewW, viewH, offsetX, offsetY) |
| `test_speed_presets`        | `adjustSpeed(+1)` / `adjustSpeed(-1)` cycles correctly; clamped at bounds |
| `test_overlay_state_machine`| State transitions: Hidden→PeekPending→Peek→Hidden, Hidden→PeekPending→Sticky→Hidden, etc. |
| `test_bgra_to_rgba`         | Pixel swizzle produces correct byte order for known inputs |

Add these to `test/test_ui.cpp` (new file) and register in
`CMakeLists.txt`.

To make testable, **extract pure functions**:

```cpp
// In a header (e.g. src/platform/ui_math.h):
struct SnapResult { int scale; int width; int height; };
SnapResult ComputeIntegerSnap(int newW, int newH, int guestW, int guestH);

struct ViewportRect { float x; float y; float w; float h; };
ViewportRect ComputeStretchedViewport(float winW, float winH, int guestW, int guestH);
```

### Manual (visual verification)

Each phase's fence includes manual checks.  A checklist document
(`docs/MANUAL_TEST.md` already exists) should be extended with:

- [ ] Integer mode: drag window → snaps to discrete sizes
- [ ] Stretched mode: drag window → aspect-preserving viewport
- [ ] Fullscreen Integer: centered, dark grey borders
- [ ] Fullscreen Stretched: fills display, thin aspect bars only
- [ ] Ctrl tap: sticky overlay
- [ ] Ctrl hold: peek overlay (disappears on release)
- [ ] All shortcuts: Ctrl+F/M/S/→/←/0/P/I/R
- [ ] Screenshot: paste into Preview → correct image
- [ ] Insert Disk: dialog opens, file mounts
- [ ] Mouse: drag outside window, release, guest gets mouse-up
- [ ] Cmd+Q: forwarded to guest, emulator does NOT quit

### Non-regression (headless/selftest)

The existing `selftest.sh` / `test/verify.sh` scripts boot a Mac ROM
in headless mode and verify output.  These are not affected by UI
changes.  Run them as a sanity check to ensure no regressions in the
emulation core:

```bash
cmake --build --preset macos && ./bld/macos/tests && cd test && ./verify.sh
```

---

## New Files Summary

| File                                | Purpose                          |
|-------------------------------------|----------------------------------|
| `src/platform/ui_math.h`           | Pure snap/viewport computation   |
| `src/platform/clipboard_image.h`   | Clipboard image API              |
| `src/platform/clipboard_image.cpp` | SDL3 clipboard implementation    |
| `src/platform/stb_impl.cpp`        | stb_image_write implementation   |
| `libs/stb/stb_image_write.h`       | PNG encoder (if not present)     |
| `test/test_ui.cpp`                  | Unit tests for UI math/logic     |
