# Plan: ImGui UI Redesign

## TL;DR
Replace the minimal ImGui menu bar + floating debug windows with a full UI: model selector on launch, 3-state UI (Windowed/Fullscreen/Developer), ImGui control overlay replacing legacy `control_mode`, and a DockSpace-based developer mode with a pluggable tool framework.

## Application Flow

### State Machine
```
[Launch] → no model → [ModelSelector] → boot → [Windowed]
[Launch] → model given → [Windowed]
[Windowed] ↔ [Fullscreen]  (ctrl overlay or hotkey)
[Windowed] → [Developer]    (ctrl overlay option)
[Developer] → [Windowed]    (close dev mode)
```
Three UI states + a pre-boot model selector. Developer is always windowed. Ctrl overlay works in all 3 running states.

### Model Selector (pre-boot)
- Displayed when app launches without `--model` argument
- Grid of all 12 MacModel entries (Twig43 through IIx), each with icon + display name
- Models without a matching ROM in `roms/` are greyed out (not clickable)
- Clicking a model opens a **Config Panel** replacing the grid:
  - RAM size (dropdown, model-valid options)
  - Screen size (width × height, constrained per model family)
  - Screen depth (1bpp/8bpp, model-dependent)
  - ROM path override (file picker, pre-filled with default)
  - Speed preset
- **Disk Tab** (tab or collapsible section in config panel):
  - List of disks to mount at boot (up to NumDrives=6)
  - Drag/drop from Finder onto the list
  - Browse button per slot
  - Future: bundled ROM disks section
- **Boot button**: builds LaunchConfig/MachineConfig, calls existing init path
- **Back button**: returns to model grid

### Windowed State
- Window sized to emulator screen + menu bar (current behavior, slightly refined)
- Menu bar: File (Quit), Machine (Fullscreen toggle), View (Zoom 1×/2×/3×), Debug (toggle dev tools — hidden unless in Developer mode)
- Emulator viewport rendered as GL texture (current `drawEmulatorViewport()`)
- Ctrl key → shows overlay

### Fullscreen State
- Emulator fills the display, no menu bar, no window chrome
- Background is black; emu viewport centered (preserving aspect ratio)
- Ctrl key → overlay appears centered on screen
- Escape → toggles back to Windowed

### Developer State
- Window resizes to large (e.g. 1400×900 or display-proportional)
- ImGui DockSpace fills the window (via `ImGui::DockSpaceOverViewport`)
- Default layout: emulator viewport docked center, tool panels in side/bottom docks
- Menu bar gains Debug menu with tool toggles (Registers, Disassembly, Memory, VIA, future tools)
- Ctrl overlay still works on top of the emu viewport
- Exiting developer mode: window shrinks back, tools hidden, DockSpace disabled

### Control Overlay (Ctrl key)
Replaces legacy `control_mode.cpp` framebuffer painting with an ImGui overlay.
- Semi-transparent background covering emu viewport
- Appears when Ctrl is pressed; disappears on release (same as current behavior)
- Action buttons/items:
  - **Insert Disk** (opens file dialog or drag/drop zone)
  - **Eject Disk** (per-drive, only if disk inserted)
  - **Interrupt** (NMI)
  - **Reboot** (with confirmation if disks inserted)
  - **Power Off** (soft shutdown, model-dependent)
  - **Screenshot** → clipboard
  - **Zoom**: 1×, 2×, 3× radio buttons
  - **Speed**: 1×, 2×, 4×, 8×, unlimited
  - **Fullscreen** toggle
  - **Developer Mode** toggle (enters/exits Developer state)
- Layout: centered panel, grouped sections (Machine, Display, Advanced)

### Developer Tool Framework
A lightweight registry so new tools can be added with minimal boilerplate.
- **ToolPanel interface**: Each tool implements `name()`, `draw()`, `isVisible()`/`setVisible()`
- **ToolRegistry**: vector of ToolPanel pointers; `DrawAllTools()` iterates and calls `draw()` on visible ones; menu bar auto-generates toggle items
- Existing debug windows (Registers, Disassembly, Memory, VIA) refactored into ToolPanel implementations
- Future tools (breakpoints, snapshots, trap explorer, serial log, advanced settings) just register with the same interface

## Steps

### Phase 1: UI State Machine & Skeleton
1. Add `enum class UIState { ModelSelector, Windowed, Fullscreen, Developer }` to `ImGuiBackend`
2. Refactor `runLoop()` to branch on UIState — each state has its own draw path
3. Add state transition methods: `enterWindowed()`, `enterFullscreen()`, `enterDeveloper()`, `enterModelSelector()`
4. When LaunchConfig has no model override (need a "no model specified" sentinel), start in ModelSelector state; otherwise start in Windowed

**Verification**: app launches into model selector when no `--model`, and directly into Windowed with `--model MacPlus`

### Phase 2: Model Selector Screen
5. Create `imgui_model_selector.h/.cpp` with `DrawModelSelector()` returning a boot action
6. Build model metadata table: display name, icon texture (placeholder colored rectangles initially), ROM filename, presence check
7. Grid layout: `ImGui::BeginTable` with image+label per cell; greyed-out styling for missing ROMs
8. Click handler: store selected model, switch to config sub-view
9. Config sub-view: RAM dropdown (values from MachineConfig defaults + valid overrides), screen size, depth, ROM path input
10. Disk list sub-view (tab or accordion): slots for up to 6 disks, file-drop integration, browse button
11. Boot button: populate LaunchConfig from UI selections, call through existing `ProgramMain`/`shell.init()` path, transition to Windowed state

**Verification**: can select MacPlus, adjust RAM, mount a disk image, and boot into emulation

### Phase 3: Control Overlay (replaces legacy control_mode)
12. Create `imgui_overlay.h/.cpp` with `DrawControlOverlay(UIState currentState)`
13. Overlay renders as an ImGui window with `ImGuiWindowFlags_NoDecoration | NoMove | NoBackground` + semi-transparent draw list rect
14. Wire Ctrl key detection in `imgui_backend.cpp` event loop: set `overlayVisible_` flag (replaces `Keyboard_UpdateControlKey`)
15. Implement action buttons calling existing globals/functions: `g_wantMacReset`, `g_forceMacOff`, `g_requestInsertDisk`, `g_wantMagnify`, `g_speedValue`, `ToggleWantFullScreen()`, and new developer-mode toggle
16. Zoom/speed controls as radio buttons or sliders mapping to EmulatorConfig
17. Screenshot action: read back GL framebuffer or use `shell_->getFramebuffer()`, copy to system clipboard via SDL
18. Remove/bypass legacy `control_mode.cpp` rendering paths for imgui backend (keep for SDL backend until removal)

**Verification**: Ctrl key shows overlay in all 3 states; actions (reset, disk insert, speed change, fullscreen toggle) work correctly

### Phase 4: Developer Mode + DockSpace
19. In `enterDeveloper()`: resize window larger, enable `ImGui::DockSpaceOverViewport` in the draw path
20. Create `imgui_developer.h/.cpp` for DockSpace setup and default layout
21. Emu viewport becomes a dockable ImGui window (remove `NoResize` flag, give it a dock ID)
22. Set up initial dock layout programmatically on first enter (emu center, tools right/bottom)
23. In `enterWindowed()` from Developer: restore window size, disable DockSpace, re-apply fixed viewport

**Verification**: entering developer mode rearranges to docked layout; exiting restores compact window

### Phase 5: Tool Framework + Migrate Debug Windows
24. Create `imgui_tool.h` — `ToolPanel` base class with `virtual const char* name()`, `virtual void draw()`, `bool visible`
25. Create `imgui_tool_registry.h/.cpp` — `RegisterTool()`, `DrawAllTools()`, `DrawToolMenu()` (auto populates Debug menu)
26. Refactor `DrawRegisterWindow` → `RegistersTool : ToolPanel`
27. Refactor `DrawDisassemblyWindow` → `DisassemblyTool : ToolPanel`
28. Refactor `DrawMemoryWindow` → `MemoryTool : ToolPanel`
29. Refactor `DrawVIAWindow` → `VIATool : ToolPanel`
30. Register all tools in backend init. `DrawDebugWindows()` becomes `DrawAllTools()`.
31. Menu bar Debug menu uses `DrawToolMenu()` to auto-list all registered tools

**Verification**: all 4 existing debug windows still work, now dockable; adding a stub 5th tool (e.g. "About Emulator") works by just registering it

### Phase 6: Fullscreen Polish
32. In Fullscreen state: hide menu bar, letterbox emu viewport, black background
33. Ctrl overlay positions centered on screen (not window-relative)
34. Escape key exits fullscreen (intercepted before forwarding to emulator)
35. Handle display bounds correctly for multi-monitor setups

**Verification**: fullscreen displays correctly on primary monitor; Ctrl overlay and Escape work

## Relevant Files

- [imgui_backend.h](src/platform/imgui_backend.h) — Add UIState enum, state transitions, overlay flag; refactor `runLoop()`, `drawMenuBar()`
- [imgui_backend.cpp](src/platform/imgui_backend.cpp) — Main loop branching, Ctrl key interception, DockSpace integration
- [imgui_debug_windows.h](src/platform/imgui_debug_windows.h) — Refactor to ToolPanel interface
- [imgui_debug_windows.cpp](src/platform/imgui_debug_windows.cpp) — Migrate 4 windows to ToolPanel subclasses
- [imgui_main.cpp](src/platform/imgui_main.cpp) — Handle "no model" launch path
- [config_loader.h](src/core/config_loader.h) — Add sentinel for "no model specified"; `ModelToString()` and `DefaultRomFileName()` used by selector
- [machine_config.h](src/core/machine_config.h) — `MacModel` enum, `MachineConfig` struct (read-only reference for valid config ranges)
- [control_mode.h](src/platform/common/control_mode.h) — Legacy overlay; bypass for imgui backend
- [control_mode.cpp](src/platform/common/control_mode.cpp) — Legacy overlay actions (reference for action list + globals)
- [emulator_shell.h](src/platform/emulator_shell.h) — `toggleWantFullScreen()`, `insertDiskOrRom()`, `getFramebuffer()` used by overlay
- [emulation_config.h](src/core/emulator_config.h) — `EmulatorConfig` speed/magnify values referenced by overlay controls
- **New files**:
  - `src/platform/imgui_model_selector.h/.cpp` — Model picker + config UI
  - `src/platform/imgui_overlay.h/.cpp` — Control overlay
  - `src/platform/imgui_developer.h/.cpp` — DockSpace layout
  - `src/platform/imgui_tool.h` — ToolPanel interface
  - `src/platform/imgui_tool_registry.h/.cpp` — Tool registration and drawing

## Verification
1. Build with `cmake --preset macos-imgui && cmake --build bld/macos-imgui`
2. Launch without `--model` → model selector grid appears
3. Select a model with ROM present → config panel shows; boot → emulation starts
4. Ctrl key in Windowed → overlay appears with all action buttons; release → disappears
5. Toggle fullscreen from overlay → emu fills screen; Escape → returns to windowed
6. Enter Developer mode from overlay → window expands, DockSpace layout with emu + tool panels
7. Drag tool panels around → docking works; close a tool from Debug menu → tool disappears
8. All 4 existing debug tools (Registers, Disassembly, Memory, VIA) work correctly in Developer mode
9. Golden-file self-tests still pass (`./selftest.sh`) — no emulation regression

## Decisions
- **3 UI states** (Windowed, Fullscreen, Developer). Developer is always windowed — no fullscreen+dev combo.
- **Show all 12 models** in selector, greyed out if ROM missing.
- **Replace legacy control_mode** with ImGui overlay for the imgui backend. Legacy code stays for SDL backend until SDL removal.
- **ImGui DockSpace** for developer mode — user can freely rearrange tool panels.
- **ToolPanel interface** — lightweight base class; tools self-register; menu auto-populates.
- **Ctrl key** remains the overlay trigger (matching legacy behavior). Could become configurable later.
- **Icons**: start with colored placeholder rectangles; real icons are a polish task.
- **Video recording** (mp4): out of scope for this plan; noted as future work.
- **Bundled ROM disks**: disk tab has a placeholder section; actual bundling is separate work.

## Further Considerations
1. **Deferred init**: Currently `shell.init()` boots the machine immediately. The model selector needs a two-phase init: platform init first (window, GL, ImGui), then machine init after user picks a model. This requires splitting `EmulatorShell::init()` — probably the largest structural change.
2. **Model-valid RAM options**: Each model has a fixed set of valid RAM sizes (e.g. Plus: 1MB/2.5MB/4MB, Mac II: 1-8MB). We should derive these from MachineConfig rather than hardcoding, but the current factory doesn't expose valid ranges — may need a small helper.
3. **Ctrl key conflict**: On macOS, Ctrl is rarely used by Mac apps (Command is primary), so it's safe. On other platforms the overlay trigger might need to be configurable (e.g. F12 or a special combo). This is a polish concern, not blocking.
