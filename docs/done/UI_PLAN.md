> **Note (2026-04):** Developer mode was removed.
> Phase 5 (Developer Mode + DockSpace) and Phase 6 (Tool Framework) are no longer applicable.

# UI_PLAN.md — ImGui UI Implementation Plan

Reference design: [docs/done/UI.md](done/UI.md)

---

## Phase 1: UI State Machine & Deferred Init

**Goal**: The app can launch into either Model Selector (no `--model`) or straight
into Windowed emulation (with `--model`). No UI changes yet — just the scaffolding.

### 1.1 Add UIState enum to ImGuiBackend

File: `src/platform/imgui_backend.h`

```cpp
enum class UIState { ModelSelector, Windowed, Fullscreen, Developer };
```

Add members:
```cpp
UIState uiState_ = UIState::ModelSelector;
bool    overlayVisible_ = false;
```

Add transition methods:
```cpp
void enterModelSelector();
void enterWindowed();
void enterFullscreen();
void enterDeveloper();
```

### 1.2 Split EmulatorShell init into two phases

Currently `EmulatorShell::init()` does everything: parse args, build config,
init hardware, load ROM, boot. The Model Selector needs the window up *before*
the machine is configured.

File: `src/platform/emulator_shell.h` / `.cpp`

**Phase A — platform init** (new `initPlatform()`):
- Parse args → `LaunchConfig`
- Init backend (`backend->init()`)
- Create window (placeholder size for selector, or model size if `--model`)
- Return `LaunchConfig` to caller

**Phase B — machine init** (new `initMachine(const LaunchConfig&)`):
- `BuildMachineConfig()` + `BuildEmulatorConfig()`
- ROM load, memory alloc, device init — everything currently after config parsing

The existing `init(argc, argv)` becomes a convenience wrapper calling both phases.

### 1.3 Add "no model" sentinel to LaunchConfig

File: `src/core/config_loader.h` / `.cpp`

Add a new model value or flag:
```cpp
// In LaunchConfig:
bool modelExplicit = false;  // true only if --model was passed
```

In `ParseCommandLine()`: set `modelExplicit = true` when `--model` is seen.

### 1.4 Branch runLoop() on UIState

File: `src/platform/imgui_backend.cpp`

Replace the single draw path with a switch:
```
while (!shouldQuit) {
    pollEvents();
    ImGui::NewFrame();

    switch (uiState_) {
    case UIState::ModelSelector:
        drawModelSelector();   // Phase 2
        break;
    case UIState::Windowed:
    case UIState::Fullscreen:
    case UIState::Developer:
        runEmulationTick();
        uploadFramebuffer();
        drawEmulationUI();     // calls overlay, dockspace, etc.
        break;
    }

    ImGui::Render();
    swapBuffers();
}
```

In `ModelSelector` state, emulation ticks are **not** run — the machine isn't
initialised yet.

### 1.5 Wire up imgui_main.cpp

File: `src/platform/imgui_main.cpp`

```cpp
int main(int argc, char** argv) {
    ProgramEarlyInit(argc, argv);
    const LaunchConfig& lc = GetLaunchConfig();

    ImGuiBackend backend;
    EmulatorShell shell(&backend);

    // Phase A: platform only
    shell.initPlatform(argc, argv);

    if (lc.modelExplicit) {
        // Phase B: full machine init, go straight to Windowed
        shell.initMachine(lc);
        backend.setUIState(UIState::Windowed);
    } else {
        // Stay in ModelSelector — machine not yet initialised
        backend.setUIState(UIState::ModelSelector);
    }

    backend.runLoop();
    shell.shutdown();
    ProgramCleanup();
}
```

### Compile gate

```
cmake --preset macos-imgui && cmake --build bld/macos-imgui
```
- `--model MacPlus` → window opens, emulation runs as before.
- No `--model` → window opens, blank (no crash). Model selector drawn in Phase 2.

---

## Phase 2: Model Selector Screen

**Goal**: user sees a grid of Mac models, picks one, configures it, mounts
disks, and boots.

### 2.1 Model metadata & ROM discovery

New file: `src/platform/imgui_model_selector.h` / `.cpp`

Define a featured-model list (4–6 curated models shown prominently):
```cpp
struct ModelEntry {
    MacModel    model;
    const char* displayName;   // "Macintosh Plus", "Macintosh II", etc.
    const char* description;   // one-liner: "Compact 68000, 4 MB"
    bool        featured;      // shown in main grid vs "More Models..."
    bool        romAvailable;  // checked at startup via ResolveRomPath()
    GLuint      iconTexture;   // loaded from roms/MacPlus.png or placeholder
};
```

Featured models (tentative): **Mac 128K, Mac Plus, Mac SE, Mac II** — covering
the main eras. Remaining 8 behind a "More Models..." button or tab.

At startup, scan `roms/` directory:
- For each model: call `ResolveRomPath("", model, romDir)` — non-empty = available.
- Load `roms/<ModelName>.png` if it exists → GL texture. Otherwise generate a
  colored placeholder rectangle.
- Optionally load `roms/<ModelName>.json` for model-specific metadata
  (valid RAM sizes, description override). If absent, use compiled-in defaults.

### 2.2 Model grid UI

**Main view** — clean, centered, no menu bar:
- App title + version at top
- 2×2 or 2×3 grid of featured models, each a large clickable card:
  icon (or placeholder), display name, one-line description
- Cards for models without a ROM are visually dimmed and not clickable
- Below the grid: "More Models..." button → replaces grid with full 12-model
  list (same card style, smaller)

No collapsible sections. No tree views. Flat, icon-driven.

### 2.3 Config panel

Clicking a model card replaces the grid with a configuration view:
- Model name + icon at top
- **Tabs** (not collapsible sections): "Machine" | "Disks"

**Machine tab**:
- RAM size: dropdown, populated from `roms/<Model>.json` `ramOptions` array or
  compiled-in defaults (e.g. Plus: [1, 2.5, 4] MB)
- Screen depth: 1-bit / 8-bit radio (only for models that support both)
- ROM path: read-only text showing resolved path, with "Browse..." override button
- Speed: dropdown (1×, 2×, 4×, 8×, Unlimited)

**Disks tab**:
- List of up to 6 disk slots (matching `NumDrives`)
- Each slot: filename display + "Browse..." button + "×" remove button
- Drag/drop from Finder onto the list adds a disk
- Empty slots shown as dashed outlines with "Drop disk image here" hint
- (Future: "Bundled Disks" section — placeholder text for now)

**Buttons** at bottom:
- **Boot** (primary, prominent) — calls `shell.initMachine(builtConfig)`,
  transitions to `UIState::Windowed`
- **Back** — returns to model grid

### 2.4 Optional: roms/*.json model metadata

Example `roms/MacPlus.json`:
```json
{
    "displayName": "Macintosh Plus",
    "description": "Compact 68000, up to 4 MB RAM, 512×342 B&W",
    "ramOptions": [1, 2.5, 4],
    "defaultRam": 4
}
```

If the file doesn't exist, the selector uses compiled-in fallbacks.
This allows users/distributors to customise the selector without recompiling.

### 2.5 Optional: roms/*.png model icons

If `roms/MacPlus.png` exists, load it as a GL texture for the card icon.
If not, draw a styled coloured rectangle with the model name initial.

PNG loading: use `stb_image.h` (single-header, already common in ImGui projects).

### Compile gate

- Launch without `--model` → model grid appears with 4–6 featured models
- Models with ROMs in `roms/` are clickable; others dimmed
- Click Mac Plus → config panel, adjust RAM, add a disk, click Boot → emulation starts
- `--model MacPlus` still boots directly (no selector shown)

---

## Phase 3: Control Overlay

**Goal**: Ctrl key shows a clean ImGui overlay replacing the legacy
`control_mode.cpp` framebuffer painting. Works in Windowed and Fullscreen states.

### 3.1 Create overlay module

New file: `src/platform/imgui_overlay.h` / `.cpp`

```cpp
void DrawControlOverlay(UIState state, EmulatorShell* shell);
```

### 3.2 Ctrl key interception

File: `src/platform/imgui_backend.cpp`

In the event loop, detect SDL_SCANCODE_LCTRL / RCTRL down/up:
- Down → `overlayVisible_ = true`, pause forwarding Ctrl to emulator
- Up → `overlayVisible_ = false`

When `overlayVisible_`, the overlay is drawn on top of the emulator viewport.
Mouse/keyboard events go to ImGui (the overlay), not to the emulator.

Bypass the legacy `Keyboard_UpdateControlKey` / `DoControlModeStuff` path
entirely in the ImGui backend.

### 3.3 Overlay layout

Design: a **centered, semi-transparent panel** over the emulator viewport.

The panel is icon-driven with small, pleasantly grouped icon buttons.
Layout uses a **tabbed** approach for organisation (not collapsible sections):

**Tab: Machine**
| Icon | Action | Notes |
|------|--------|-------|
| 💾   | Insert Disk | opens native file dialog |
| ⏏️   | Eject Disk 1..N | one per inserted disk, greyed if empty |
| ⚡   | Interrupt (NMI) | |
| 🔄   | Reboot | confirmation if disks mounted |
| ⏻    | Power Off | model-dependent (soft power) |
| 📷   | Screenshot → clipboard | |

**Tab: Display**
| Icon | Action | Notes |
|------|--------|-------|
| 1×/2×/3× | Zoom level | radio group |
| 🖥️   | Fullscreen / Windowed | toggle |

**Tab: Speed**
| Icon | Action | Notes |
|------|--------|-------|
| 1×/2×/4×/8×/∞ | Emulation speed | radio group |

**Tab: Advanced** (or bottom row, always visible)
| Icon | Action | Notes |
|------|--------|-------|
| 🔧   | Developer Mode | enters/exits Developer state |

Tabs keep the overlay compact. Each tab is a single row of icons with tooltips.
If the overlay is small enough, tabs could be icon-group rows instead
(all visible, separated by thin dividers) — decide during implementation.

### 3.4 Action wiring

Each button calls existing globals/shell methods:
- Insert disk → `shell_->insertDiskOrRom(path, false)` (after file dialog)
- Eject → existing `g_requestEjectDisk` or equivalent
- Reboot → `g_wantMacReset = true`
- Power off → `g_forceMacOff = true`
- Screenshot → read `shell_->getFramebuffer()`, encode to PNG, copy to
  system clipboard via SDL3 clipboard API
- Zoom → update `g_magnify` / `EmulatorConfig::scale`
- Speed → `g_speedValue = N`
- Fullscreen → `shell_->toggleWantFullScreen()` → triggers state transition
- Developer → `enterDeveloper()`

### 3.5 Disable legacy control_mode for ImGui backend

The overlay replaces the framebuffer-painting control mode. In `imgui_backend.cpp`,
ensure `DoControlModeStuff()` is never called (or returns immediately).
Legacy code remains for SDL backend until SDL removal.

### Compile gate

- Ctrl key → overlay appears over emulator
- Release Ctrl → overlay disappears
- All action buttons functional
- Legacy control mode no longer renders in ImGui backend

---

## Phase 4: Fullscreen State

**Goal**: Fullscreen fills the screen with the emulator, no chrome. Ctrl overlay
works. No Escape key (it belongs to the emulated Mac).

### 4.1 Enter/exit fullscreen

`enterFullscreen()`:
- `SDL_SetWindowFullscreen(window_, true)`
- `uiState_ = UIState::Fullscreen`
- Hide cursor when over viewport

`enterWindowed()` (from fullscreen):
- `SDL_SetWindowFullscreen(window_, false)`
- Restore previous window size
- `uiState_ = UIState::Windowed`

### 4.2 Fullscreen rendering

- No menu bar (don't call `drawMenuBar()`)
- Black background (`glClearColor(0, 0, 0, 1)`)
- Emulator viewport scaled to fill screen while preserving aspect ratio
  (letterboxed if display aspect ≠ emu aspect)
- Use integer scaling if possible (e.g. 3× for a 512×342 on 1920×1080),
  fall back to best-fit with `GL_LINEAR` filtering

### 4.3 Overlay in fullscreen

Same overlay as windowed, but positioned centered on the full display
rather than relative to a window. Semi-transparent background covers the
full screen.

### 4.4 No Escape key binding

Escape is NOT intercepted by the backend — it is forwarded to the emulated Mac.
The only way out of fullscreen is through the control overlay
(Ctrl → Fullscreen/Windowed toggle).

### Compile gate

- Toggle fullscreen from overlay → emu fills screen, black letterbox
- Ctrl overlay works in fullscreen
- Escape key reaches emulated Mac
- Overlay "Windowed" button returns to windowed state

---

## Phase 5: Developer Mode + DockSpace

**Goal**: A large windowed mode with ImGui DockSpace. Emulator viewport is one
docked panel among many. Menu bar with Debug menu.

### 5.1 Enter/exit developer

`enterDeveloper()`:
- Save current window size/position
- Resize window to large (e.g. 80% of display or 1400×900 minimum)
- Enable `ImGui::DockSpaceOverViewport()` in draw path
- Show menu bar with Debug menu
- `uiState_ = UIState::Developer`

`enterWindowed()` (from developer):
- Restore saved window size/position
- Disable DockSpace
- Hide debug tool panels
- `uiState_ = UIState::Windowed`

### 5.2 DockSpace layout

File: new `src/platform/imgui_developer.h` / `.cpp`

```cpp
void SetupDeveloperDockSpace();  // called once on first enter
void DrawDeveloperUI();          // called each frame in Developer state
```

Default layout (programmatic, on first enter):
```
┌─────────────────────────────────────────────┐
│  Menu Bar                                   │
├────────────┬──────────────┬─────────────────┤
│            │              │                 │
│  Tool      │  Macintosh   │  Tool           │
│  Panel     │  Viewport    │  Panel          │
│  (left)    │  (center)    │  (right)        │
│            │              │                 │
├────────────┴──────────────┴─────────────────┤
│  Tool Panel (bottom)                        │
└─────────────────────────────────────────────┘
```

The user can freely rearrange by dragging dock tabs. ImGui persists the layout
in `imgui.ini`.

### 5.3 Emulator viewport as dockable window

In Developer mode, `drawEmulatorViewport()` uses a normal ImGui window
(no `NoResize`) so it participates in the DockSpace. The texture display
scales to fit the window with aspect ratio preserved.

### 5.4 Menu bar in Developer mode

Only shown in Developer state:
```
File | Machine | Debug | Tools
```

- **File**: Quit
- **Machine**: Windowed (exit dev mode), Fullscreen
- **Debug**: auto-populated tool toggles (Phase 6)
- **Tools**: (future — advanced settings, trap explorer, etc.)

### 5.5 Ctrl overlay in Developer mode

TBD — the overlay may feel redundant when the menu bar + tool panels are visible.
Implement it but consider making it optional (or only showing the
Machine-action subset). Decide after testing.

### Compile gate

- Enter Developer from overlay → window expands, DockSpace layout
- Emu viewport dockable, tools in panels
- Drag panels around → docking works
- Exit developer → returns to compact windowed mode
- `imgui.ini` persists developer layout

---

## Phase 6: Tool Framework & Debug Window Migration

**Goal**: Existing debug windows become pluggable ToolPanel instances. New tools
can be added by implementing a simple interface and registering.

### 6.1 ToolPanel base class

New file: `src/platform/imgui_tool.h`

```cpp
class ToolPanel {
public:
    virtual ~ToolPanel() = default;
    virtual const char* name() const = 0;   // menu label & window title
    virtual const char* icon() const { return nullptr; } // optional icon
    virtual void draw() = 0;                // ImGui drawing
    bool visible = false;
};
```

### 6.2 ToolRegistry

New file: `src/platform/imgui_tool_registry.h` / `.cpp`

```cpp
class ToolRegistry {
public:
    void registerTool(std::unique_ptr<ToolPanel> tool);
    void drawAllVisible();           // iterates, calls draw() on visible
    void drawToolMenu();             // Debug menu: toggle items for each tool
    ToolPanel* findByName(const char* name);
private:
    std::vector<std::unique_ptr<ToolPanel>> tools_;
};
```

### 6.3 Migrate existing debug windows

Current: 4 free functions + 4 `extern bool` toggles in `imgui_debug_windows.cpp`.

Migrate each to a ToolPanel subclass:
- `RegistersTool` — wraps `DrawRegisterWindow()`
- `DisassemblyTool` — wraps `DrawDisassemblyWindow()`
- `MemoryTool` — wraps `DrawMemoryWindow()`
- `VIATool` — wraps `DrawVIAWindow()`

Each reads relevant emulator state the same way the current free functions do.
Registration happens in `ImGuiBackend::init()`:
```cpp
toolRegistry_.registerTool(std::make_unique<RegistersTool>());
toolRegistry_.registerTool(std::make_unique<DisassemblyTool>());
// etc.
```

### 6.4 Auto-generated Debug menu

`toolRegistry_.drawToolMenu()` produces:
```
Debug
  ☑ Registers
  ☐ Disassembly
  ☑ Memory
  ☐ VIA State
```

No hardcoded menu items — adding a new tool is: implement ToolPanel, register it.

### 6.5 Future tools (not in this plan, but the framework supports)

- CPU Control (step, continue, pause)
- Breakpoints
- Snapshots (save/restore state)
- Trap Explorer (A-trap table viewer)
- Serial Log viewer
- Advanced Settings (run-in-background, sound toggle, etc.)

Each is a new `FooTool : ToolPanel` + one `registerTool()` call.

### Compile gate

- All 4 debug windows work as before in Developer mode
- Debug menu auto-populated from registry
- Add a stub "About" tool with 3 lines of code → appears in menu, draws a window
- No regressions in non-developer modes (tools hidden)
- Golden-file self-tests pass (`./selftest.sh`)

---

## Phase Summary

| Phase | Depends on | Key deliverable |
|-------|-----------|-----------------|
| 1     | —         | UIState enum, deferred init, runLoop branching |
| 2     | 1         | Model selector grid + config + disk picker |
| 3     | 1         | ImGui control overlay (replaces legacy) |
| 4     | 3         | Fullscreen with overlay, no Escape capture |
| 5     | 1         | Developer mode with DockSpace |
| 6     | 5         | ToolPanel framework, debug window migration |

Phases 2 and 3 can proceed in parallel after Phase 1.
Phases 4 and 5 can proceed in parallel after their dependencies.

---

## New Files

| File | Purpose |
|------|---------|
| `src/platform/imgui_model_selector.h/.cpp` | Model grid, config panel, disk picker |
| `src/platform/imgui_overlay.h/.cpp` | Ctrl-key control overlay |
| `src/platform/imgui_developer.h/.cpp` | DockSpace setup, developer layout |
| `src/platform/imgui_tool.h` | ToolPanel base class |
| `src/platform/imgui_tool_registry.h/.cpp` | Tool registration + auto-menu |

## Modified Files

| File | Changes |
|------|---------|
| `src/platform/imgui_backend.h` | UIState enum, overlay flag, transition methods, ToolRegistry member |
| `src/platform/imgui_backend.cpp` | runLoop branching, Ctrl interception, state transitions, DockSpace |
| `src/platform/imgui_main.cpp` | Two-phase init, ModelSelector vs direct boot |
| `src/platform/emulator_shell.h/.cpp` | Split `init()` → `initPlatform()` + `initMachine()` |
| `src/core/config_loader.h/.cpp` | `modelExplicit` flag in LaunchConfig |
| `src/platform/imgui_debug_windows.h/.cpp` | Refactor to ToolPanel subclasses |
| `CMakeLists.txt` | Add new source files to imgui target |

## Risks & Open Questions

1. **Deferred init is the biggest risk.** Splitting `EmulatorShell::init()` touches
   globals that assume they're set before the window exists. Careful audit of init
   order needed. Mitigated by keeping `initPlatform()` minimal.

2. **Ctrl key vs macOS Command key.** Ctrl is fine on macOS (apps use Cmd). On
   other platforms, may want a configurable trigger key. Not blocking — polish later.

3. **Overlay in Developer mode.** May feel redundant with the menu bar present.
   Implement it, test it, decide whether to keep. The overlay code is the same
   regardless.

4. **stb_image dependency for PNG icons.** If we don't want the dependency, defer
   icons and use coloured placeholder rectangles. Icons are pure polish.

5. **Integer scaling in fullscreen.** For small screens (e.g. 512×342 on 1080p),
   integer 2× = 1024×684 with black bars; 3× doesn't fit. Best-fit with bilinear
   may look better. Need to test and decide.
