# Platform Layer Architecture Proposal

## Problem Statement

`sdl.cpp` (950+ lines) is a monolithic file that mixes:

1. **SDL-specific I/O** — window creation, texture rendering, SDL event dispatch
2. **Emulator-level UI logic** — fullscreen toggling, magnification, cursor grab/ungrab, background/foreground transitions, speed control, "saved tasks" polling
3. **Framebuffer conversion** — depth-mapped pixel blitting (BW and color, multiple src/dst depths)
4. **Application lifecycle** — `main()`, command-line parsing, memory allocation, init/teardown sequencing
5. **Disk insertion orchestration** — path resolution, ROM-or-disk routing, numbered-disk probing

Meanwhile, SDL-specific code for sound (`sdl_sound.cpp`) and keyboard (`sdl_keyboard.cpp`) are properly separated into their own files. The inconsistency is backwards: the *most complex* SDL code (rendering, windowing, events) is jammed into sdl.cpp alongside pure logic that has nothing to do with SDL.

### Consequences

- **Can't swap backends.** Replacing SDL with Dear ImGui or a headless backend requires rewriting emulator logic that shouldn't change.
- **Can't test UI logic.** Fullscreen toggle, grab/ungrab state machine, speed ramping — all untestable because they're welded to SDL calls.
- **Can't run headless.** A scripting/CI mode needs the tick loop and disk insertion but not any windowing code.

## Current Dependency Flow

```
main() → ProgramEarlyInit() → InitOSGLU() → ProgramMain()
                                                  ↓
                                            MainEventLoop()       [core/main.cpp]
                                                  ↓
                                          WaitForNextTick()       [sdl.cpp — platform]
                                          ├── CheckForSystemEvents()  [SDL]
                                          ├── CheckForSavedTasks()    [mixed!]
                                          └── SDL_Delay / timing      [SDL]
```

There are two nested run loops:
- **core/main.cpp `MainEventLoop()`**: `WaitForNextTick → RunEmulatedTicksToTrueTime → DoEmulateExtraTime`. This calls `WaitForNextTick()` which blocks until a host tick is due.
- **sdl.cpp `WaitForNextTick()`**: polls SDL events, runs the saved-tasks state machine, delays or blocks waiting for timing.

Additionally, `WaitForRom()` in control_mode.cpp has its own blocking loop calling `WaitForNextTick()` while waiting for a ROM to be drag-dropped.

`CheckForSavedTasks()` is the worst offender — a ~100-line polling function that interleaves pure emulator logic (disk insertion, speed state, message display) with SDL calls (window recreation, cursor grab, cursor hide).

## Why ImGui Requires Backend-Driven Frame Loop

Dear ImGui uses an **immediate-mode** rendering model. Every frame, the application must:
1. Call `ImGui::NewFrame()`
2. Issue all draw commands (menus, debug windows, emulator viewport)
3. Call `ImGui::Render()`
4. Present the result

This creates three incompatibilities with the current architecture:

### 1. Frame cadence
SDL renders only when the emulated screen has changed (dirty-rect push). ImGui must render **every host frame** — menus, hover states, and debug windows update continuously even when the emulated screen is static.

### 2. Loop ownership
The core's `MainEventLoop` drives everything: it calls `WaitForNextTick()` which blocks inside SDL. ImGui needs to own the outer frame loop and call emulation as a non-blocking subroutine: "run however many ticks are due, then let me render my frame."

### 3. Event routing
ImGui consumes raw SDL events (`ImGui_ImplSDL3_ProcessEvent`) *before* translating them to emulator actions. If the mouse is over an ImGui window, ImGui swallows the click; otherwise the event goes to the emulator. The proposed abstract `pollEvents()` can't work if it hides the raw events from ImGui.

### Conclusion
**The backend must own the main loop.** The emulator shell and core become passive — the backend calls into them, not the other way around. This is the only model that works for SDL, ImGui, and headless alike.

## Proposed Architecture

Three layers, each replaceable independently. **The backend drives the frame loop:**

```
┌──────────────────────────────────────────────────────┐
│                     main()                            │  app_main.cpp
│    ProgramEarlyInit → create backend → backend.run()  │
└──────────────────────┬───────────────────────────────┘
                       │ owns
┌──────────────────────▼───────────────────────────────┐
│     SdlBackend / ImGuiBackend / HeadlessBackend       │
│                                                       │
│  OWNS THE FRAME LOOP. Each iteration:                │
│    1. Poll platform events                            │
│       - ImGui: feed raw events to ImGui IO first      │
│    2. Translate remaining events → shell abstract API  │
│    3. shell.processSavedTasks()                        │
│    4. while (shell.tickIsDue()) shell.runOneTick()     │
│    5. Render:                                         │
│       - SDL: blit emulator texture, present            │
│       - ImGui: NewFrame → menus → Image(emuTex)       │
│                → debug windows → Render → Present      │
│       - Headless: no-op / write to file                │
│                                                       │
│  Platform primitives:                                 │
│    - window create/destroy/resize/fullscreen           │
│    - cursor show/hide/warp/grab                        │
│    - audio init/start/stop                             │
│    - message box                                       │
│    - keyboard repeat                                   │
└──────────────────────┬───────────────────────────────┘
                       │ calls into
┌──────────────────────▼───────────────────────────────┐
│              EmulatorShell                             │  emulator_shell.cpp/.h
│                                                       │
│  Platform-independent emulator lifecycle:             │
│  - init / teardown sequencing & memory allocation     │
│  - saved-tasks state machine (requests backend ops    │
│    through the PlatformBackend interface)              │
│  - abstract event dispatch → emulator key/mouse/disk  │
│  - disk insertion orchestration                        │
│  - background/foreground transition logic              │
│  - speed control state machine                         │
│  - fullscreen/magnify intent & window re-creation      │
│  - cursor grab state machine                           │
│  - mouse coordinate transforms                        │
│  - framebuffer conversion (CLUT→ARGB8888)              │
│  - timing queries (tickIsDue, getDelay)                │
│                                                       │
│  Exposes to backend:                                  │
│    bool tickIsDue()                                    │
│    void runOneTick()                                   │
│    bool isFramebufferDirty()                           │
│    const uint8_t* getFramebuffer()                     │
│    void processSavedTasks()                            │
│    void dispatchEvent(const PlatformEvent&)             │
│    uint32_t getDelay()  // ms to next tick             │
│    bool shouldQuit()                                   │
└──────────────────────┬───────────────────────────────┘
                       │ calls into
┌──────────────────────▼───────────────────────────────┐
│              Emulation Core                            │  core/main.cpp
│                                                       │
│  ProgramEarlyInit() — parse CLI, build Machine         │
│  InitEmulation() — wire ICT, build address space       │
│  RunEmulatedTicksToTrueTime()  — run N ticks           │
│  DoEmulateExtraTime()  — speed >1x sub-ticks           │
│  ProgramCleanup()                                      │
│                                                       │
│  No longer owns MainEventLoop.                         │
│  Emulation step is a callable function, not a loop.    │
└──────────────────────────────────────────────────────┘
```

## The Key Change: Decomposing MainEventLoop / WaitForNextTick

### Current (core drives, platform blocks)

```
core/main.cpp MainEventLoop():
  loop:
    WaitForNextTick()          ← blocks inside sdl.cpp
      CheckForSystemEvents()   ← SDL_PollEvent
      CheckForSavedTasks()     ← mixed SDL + emulator logic
      SDL_Delay / SDL_WaitEvent
    RunEmulatedTicksToTrueTime()
    DoEmulateExtraTime()
```

### Proposed (backend drives, core is passive)

```
Backend::runLoop():
  loop:
    events = pollPlatformEvents()        ← backend-specific
    shell.dispatchEvents(events)
    shell.processSavedTasks()            ← pure logic, calls backend primitives
    while (shell.tickIsDue()):
      shell.runOneTick()                 ← calls core emulation step
    backend renders frame                ← backend-specific
    delay if needed (shell.getDelay())
```

The core's `RunEmulatedTicksToTrueTime()` and `DoEmulateExtraTime()` become non-static functions callable from the shell. `MainEventLoop()` is deleted. `WaitForNextTick()` is decomposed: its timing logic moves to `EmulatorShell::tickIsDue()` and `EmulatorShell::getDelay()`; its event-polling and state-machine duties are handled by the backend calling `shell.dispatchEvents()` and `shell.processSavedTasks()`.

### WaitForRom — drop this feature

`WaitForRom()` in control_mode.cpp currently has a blocking loop that displays a "drag ROM here" message and waits. **This should be deleted entirely.** It's a UX dead-end — completely undiscoverable (users don't guess they can drag a ROM onto the window), and the need for it means ROM resolution already failed.

If ROM loading fails, the emulator should exit with a clear error message pointing the user to `--rom` or the ROM search paths. A future welcome screen (model selection, memory, screen size) is the right place to handle ROM selection if needed. Until then, fail fast.

## The PlatformBackend Interface

```cpp
// src/platform/platform_backend.h

// Abstract event from the host platform.
// Backend translates platform-native events (SDL, Win32, etc.) into these.
struct PlatformEvent {
    enum Type {
        Quit, KeyDown, KeyUp,
        MouseMove, MouseDown, MouseUp,
        MouseWheel, FileDrop,
        FocusGained, FocusLost,
        MouseEnter, MouseLeave,
        WindowResized,
    };
    Type type;
    uint8_t macKeyCode;    // for KeyDown/KeyUp: already translated to MKC_*
                           // (backend does the scancode→MKC translation)
    float x, y;            // for MouseMove/MouseDown/MouseUp (logical coords)
    float wheelX, wheelY;  // for MouseWheel
    std::string filePath;  // for FileDrop
};

class EmulatorShell;  // forward

class PlatformBackend {
public:
    virtual ~PlatformBackend() = default;

    // --- Lifecycle ---
    virtual bool init(EmulatorShell* shell) = 0;
    virtual void shutdown() = 0;

    // Backend owns the main loop. Returns when the emulator quits.
    // Each frame: poll events, call shell, render, repeat.
    virtual void runLoop() = 0;

    // --- Window management (called BY the shell, not by the backend loop) ---
    virtual bool createWindow(int w, int h, const char* title) = 0;
    virtual void destroyWindow() = 0;
    virtual bool recreateWindow(int w, int h, bool fullscreen) = 0;
    virtual void getWindowSize(int* w, int* h) = 0;
    virtual void getWindowPosition(int* x, int* y) = 0;
    virtual void setFullscreen(bool fs) = 0;

    // --- Cursor (called BY the shell) ---
    virtual void showCursor() = 0;
    virtual void hideCursor() = 0;
    virtual void warpCursor(int x, int y) = 0;
    virtual void setMouseGrab(bool grab) = 0;

    // --- Audio (called BY the shell) ---
    virtual bool audioInit() = 0;
    virtual void audioStart() = 0;
    virtual void audioStop() = 0;
    virtual void audioShutdown() = 0;

    // --- Keyboard repeat (called BY the shell) ---
    virtual void disableKeyRepeat() = 0;
    virtual void restoreKeyRepeat() = 0;

    // --- Dialogs (called BY the shell) ---
    virtual void showMessageBox(const char* title, const char* msg) = 0;

    // --- Query (called BY the shell for ToggleWantFullScreen) ---
    virtual bool getDisplayBounds(int displayIndex, int* w, int* h) = 0;
};
```

### Design decisions

1. **No `presentFrame()`, no `pollEvents()`, no `delay()` in the interface.** The backend handles all of this internally in `runLoop()`. It reads the shell's framebuffer via `shell->getFramebuffer()`, it calls `shell->dispatchEvent()` for each translated event, and it handles its own timing. This means ImGui can draw whatever it wants around the emulator viewport without the shell knowing or caring.

2. **`macKeyCode` in PlatformEvent, not raw scancodes.** The scancode→MKC translation table is backend-specific (SDL scancodes vs. Win32 virtual keys vs. whatever ImGui uses). The backend translates to MKC before handing to the shell. The existing `SDLScan2MacKeyCode()` table stays in the SDL backend.

3. **The shell calls backend primitives only for side-effects** (create window, grab cursor, start audio). It never asks the backend for frame data or timing. Flow is: backend drives → shell reacts → shell may call backend primitives → backend observes shell state for rendering.

4. **Sound buffer interface stays as-is.** The ring-buffer protocol (`Sound_BeginWrite` / `Sound_EndWrite`) is already decoupled from the backend loop — the core writes samples during emulation ticks, and the audio callback reads them asynchronously. Both SDL and ImGui+SDL use SDL_Audio under the hood, so the audio callback model works for both. For a truly non-SDL backend (e.g., PortAudio), only the callback plumbing changes, not the ring buffer.

## EmulatorShell Responsibilities

```cpp
// src/platform/emulator_shell.h

class EmulatorShell {
public:
    EmulatorShell(PlatformBackend* backend);

    // --- Lifecycle (called from main, before backend.runLoop) ---
    bool init(int argc, char** argv);
    void shutdown();

    // --- Called by backend each frame ---
    void dispatchEvent(const PlatformEvent& evt);
    void processSavedTasks();   // state machine: background, speed, grab, etc.

    // --- Timing (called by backend to decide when to tick) ---
    bool tickIsDue();           // true if an emulation tick should run now
    void runOneTick();          // advance emulation by one 60Hz tick + extras
    uint32_t getDelayMs();      // ms the backend should sleep (0 = don't sleep)
    bool shouldQuit();          // true when g_forceMacOff

    // --- Framebuffer (read by backend for rendering) ---
    bool isFramebufferDirty();
    const uint8_t* getFramebuffer();  // ARGB8888, vMacScreenWidth × vMacScreenHeight
    int getScreenWidth();
    int getScreenHeight();
    void clearDirtyFlag();

    // --- Disk orchestration ---
    bool insertDiskOrRom(const char* path, bool silent);

private:
    PlatformBackend* backend_;

    // ---- State (currently sdl.cpp statics) ----
    bool useFullScreen_ = false;
    bool useMagnify_ = false;
    int windowScale_ = 1;
    bool backgroundFlag_ = false;
    bool trueBackgroundFlag_ = false;
    bool curSpeedStopped_ = true;
    bool grabMachine_ = false;
    bool haveCursorHidden_ = false;
    bool wantCursorHidden_ = false;
    bool caughtMouse_ = false;

    // Fullscreen window position tracking
    int curWinIndx_ = 0;
    bool havePositionWins_[2] = {};
    int winPositionsX_[2] = {};
    int winPositionsY_[2] = {};
    int winMagStates_[2] = {};

    // Offsets for fullscreen centering
    int hOffset_ = 0;
    int vOffset_ = 0;

    // ---- Framebuffer ----
    uint8_t* argbBuffer_ = nullptr;   // ARGB8888 output buffer
    uint8_t* clutFinal_ = nullptr;    // pre-computed CLUT expansion table
    bool framebufferDirty_ = false;

    // ---- Internal methods ----
    void convertFramebuffer(uint16_t top, uint16_t left,
                            uint16_t bottom, uint16_t right);
    void drawChangesAndClear();
    void mousePositionNotify(int x, int y);
    bool moveMouse(int16_t h, int16_t v);
    void grabMachine();
    void ungrabMachine();
    void mouseConstrain();
    void toggleWantFullScreen();
    void leaveBackground();
    void enterBackground();
    void leaveSpeedStopped();
    void enterSpeedStopped();
    bool loadInitialImages();
};
```

## How Each Backend Uses the Shell

### SdlBackend::runLoop()

```cpp
void SdlBackend::runLoop() {
    while (!shell_->shouldQuit()) {
        // 1. Poll SDL events → translate → feed to shell
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            PlatformEvent pe = translateSdlEvent(event);
            if (pe.type != PlatformEvent::Type(-1))
                shell_->dispatchEvent(pe);
        }

        // 2. Run shell state machine (background, grab, disk, etc.)
        shell_->processSavedTasks();
        if (shell_->shouldQuit()) break;

        // 3. If speed-stopped, sleep and loop (no emulation)
        if (!shell_->tickIsDue()) {
            uint32_t delay = shell_->getDelayMs();
            if (delay > 0) SDL_Delay(delay);
            continue;
        }

        // 4. Run emulation ticks
        while (shell_->tickIsDue()) {
            shell_->runOneTick();
        }

        // 5. Present emulator framebuffer
        if (shell_->isFramebufferDirty()) {
            SDL_LockTexture(texture_, nullptr, &pixels, &pitch);
            memcpy(pixels, shell_->getFramebuffer(), ...);
            SDL_UnlockTexture(texture_);
            SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);
            SDL_RenderPresent(renderer_);
            shell_->clearDirtyFlag();
        }
    }
}
```

### ImGuiBackend::runLoop()

```cpp
void ImGuiBackend::runLoop() {
    while (!shell_->shouldQuit()) {
        // 1. Poll events — ImGui gets them FIRST
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);

            // If ImGui didn't consume the event, forward to shell
            if (!ImGui::GetIO().WantCaptureKeyboard ||
                event.type != SDL_EVENT_KEY_DOWN) {
                PlatformEvent pe = translateSdlEvent(event);
                if (pe.type != PlatformEvent::Type(-1))
                    shell_->dispatchEvent(pe);
            }
        }

        // 2. State machine
        shell_->processSavedTasks();
        if (shell_->shouldQuit()) break;

        // 3. Run emulation ticks (non-blocking — run however many are due)
        while (shell_->tickIsDue()) {
            shell_->runOneTick();
        }

        // 4. Upload emulator framebuffer to GPU texture if dirty
        if (shell_->isFramebufferDirty()) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                shell_->getScreenWidth(), shell_->getScreenHeight(),
                GL_BGRA, GL_UNSIGNED_BYTE, shell_->getFramebuffer());
            shell_->clearDirtyFlag();
        }

        // 5. Full ImGui frame — EVERY frame, regardless of emulator updates
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        drawMenuBar();                      // File, Edit, Machine, View menus
        drawEmulatorViewport(emuTextureId_); // ImGui::Image() of emulated screen
        drawDebugWindows();                  // memory viewer, disassembler, etc.

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window_);
    }
}
```

### HeadlessBackend::runLoop()

```cpp
void HeadlessBackend::runLoop() {
    while (!shell_->shouldQuit()) {
        shell_->processSavedTasks();

        // No event polling — scripted input would go through
        // shell_->dispatchEvent() from an external command interface

        // Advance one tick per iteration (no wall-clock gating)
        if (shell_->tickIsDue()) {
            shell_->runOneTick();
        }
        // No rendering, no delay
    }
}
```

## Impact on core/main.cpp

The core's `MainEventLoop()` currently owns the outer loop:

```cpp
static void MainEventLoop() {
    for (;;) {
        WaitForNextTick();         // blocks — platform-side
        if (g_forceMacOff) return;
        RunEmulatedTicksToTrueTime();
        DoEmulateExtraTime();
    }
}
```

**This function is deleted.** Its body is decomposed:

| Current code | New location | Called by |
|---|---|---|
| `WaitForNextTick()` timing logic | `EmulatorShell::tickIsDue()` + `getDelayMs()` | backend loop |
| `WaitForNextTick()` event polling | backend's own `SDL_PollEvent` / ImGui poll | backend loop |
| `WaitForNextTick()` → `CheckForSavedTasks()` | `EmulatorShell::processSavedTasks()` | backend loop |
| `RunEmulatedTicksToTrueTime()` | stays in core, called by `EmulatorShell::runOneTick()` | backend loop via shell |
| `DoEmulateExtraTime()` | stays in core, called by `EmulatorShell::runOneTick()` | backend loop via shell |
| `DoneWithDrawingForTick()` | called by shell inside `runOneTick()` | — |

`RunEmulatedTicksToTrueTime()` and `DoEmulateExtraTime()` remain in core/main.cpp but become externally callable (non-static, declared in main.h):

```cpp
// core/main.h — new exports
extern void RunEmulatedTicksToTrueTime();
extern void DoEmulateExtraTime();
extern bool InitEmulation();
```

`ProgramMain()` becomes a thin wrapper, or is removed entirely:
```cpp
// before: void ProgramMain() { if (InitEmulation()) MainEventLoop(); }
// after:  shell calls InitEmulation() during init, then backend drives ticks
```

## What Goes Where — Detailed Breakdown

### Functions that move to EmulatorShell (platform-independent logic)

| Current location in sdl.cpp | New home | Notes |
|---|---|---|
| `CheckForSavedTasks()` | `EmulatorShell::processSavedTasks()` | Biggest win. Pure logic with backend calls for side-effects |
| `WaitForNextTick()` timing/gating | `EmulatorShell::tickIsDue()` + `getDelayMs()` | Uses `UpdateTrueEmulatedTime()`, `CheckDateTime()` from tick_timer |
| `HaveChangedScreenBuff()` — CLUT build + depth dispatch | `EmulatorShell::convertFramebuffer()` | Writes to `argbBuffer_` instead of SDL texture |
| `DrawChangesAndClear()`, `DoneWithDrawingForTick()` | `EmulatorShell` internal methods | Sets `framebufferDirty_` flag instead of calling SDL render |
| All `ScrnMapr` instantiations (`UpdateBWDepth3Copy` etc.) | `screen_convert.cpp` | `ScrnMapr_Dst` = `argbBuffer_`, not SDL texture pixels |
| `HandleTheEvent()` | Split: translation in backend, dispatch in `EmulatorShell::dispatchEvent()` | Shell receives abstract `PlatformEvent`, maps to `Keyboard_updateKeyMap2`, `MyMousePositionSet`, `Sony_Insert1a` |
| `MousePositionNotify()` | `EmulatorShell::mousePositionNotify()` | Pure coordinate math + calls to `MyMousePositionSet` / `MyMousePositionSetDelta` |
| `MoveMouse()` | `EmulatorShell::moveMouse()` | Coordinate transform, then `backend_->warpCursor()` |
| `GrabTheMachine()` / `UngrabMachine()` | `EmulatorShell` methods | `backend_->setMouseGrab()` |
| `MouseConstrain()` | `EmulatorShell::mouseConstrain()` | Pure math + `moveMouse()` |
| `LeaveBackground()` / `EnterBackground()` | `EmulatorShell` methods | Calls `backend_->restoreKeyRepeat()` etc. |
| `LeaveSpeedStopped()` / `EnterSpeedStopped()` | `EmulatorShell` methods | Calls `backend_->audioStart()` / `audioStop()` |
| `ToggleWantFullScreen()` | `EmulatorShell::toggleWantFullScreen()` | Pure logic + `backend_->getDisplayBounds()` for magnify decision |
| `ReCreateMainWindow()` state save/restore | `EmulatorShell` | Calls `backend_->recreateWindow()` |
| `AllocMyMemory()` / `UnallocMyMemory()` | `EmulatorShell::init()` / `shutdown()` | Memory allocation is not SDL |
| `InitOSGLU()` / `UnInitOSGLU()` | `EmulatorShell::init()` / `shutdown()` | Sequencing |
| `ZapOSGLUVars()`, `ZapWinStateVars()`, `ZapMyWState()` | `EmulatorShell` constructor | Init state |
| `LoadInitialImages()`, `Sony_Insert1a/2/InsertIth` | `EmulatorShell` methods | Disk orchestration |
| `ScanCommandLine()` | **Deleted** | Already superseded by `ProgramEarlyInit()` / `ParseCommandLine()` in core. The sdl.cpp version only handles `--rom` which is already in `LaunchConfig` |
| `NativeStrFromCStr()` | stays in shell or common/ | String utility |
| `CheckSavedMacMsg()` | `EmulatorShell::shutdown()` | Calls `backend_->showMessageBox()` |
| `main()` | `app_main.cpp` | Thin: `ProgramEarlyInit → create backend → shell.init → backend.runLoop → shell.shutdown` |
| `ExtraTimeNotOver()` | `EmulatorShell::tickIsDue()` helper | Timing logic |
| `CheckForSystemEvents()` | **Deleted** — backend polls directly | |
| `WaitForTheNextEvent()` | **Deleted** — backend handles blocking wait in its loop | |
| `CheckMouseState()` | `EmulatorShell` | Called from `runOneTick` path, uses `backend_->getMousePosition()` or the last-known position from events |

### Functions that stay in SdlBackend (SDL-specific)

| Current location | New home | Notes |
|---|---|---|
| `Screen_Init()` — `SDL_Init()` | `SdlBackend::init()` | SDL lifecycle |
| `CreateMainWindow()` — `SDL_CreateWindow`, renderer, texture, logical presentation, format | `SdlBackend::createWindow()` | SDL windowing |
| `CloseMainWindow()` | `SdlBackend::destroyWindow()` | SDL cleanup |
| `GetMyWState()` / `SetMyWState()` / `MyWState` struct | `SdlBackend` internals for `recreateWindow()` | SDL window state save/restore during mode switch |
| SDL event poll + translate to `PlatformEvent` | `SdlBackend::runLoop()` internal | `SDLScan2MacKeyCode()` lives here |
| `SDL_Delay` / `SDL_WaitEvent` | `SdlBackend::runLoop()` internal | Timing |
| Cursor show/hide/warp/grab | `SdlBackend` methods | SDL cursor API |
| Message box | `SdlBackend::showMessageBox()` | `SDL_ShowSimpleMessageBox` |
| `sdl_sound.cpp` — audio stream, ring buffer callback | `SdlBackend` audio methods (or stays as companion file) | SDL audio. Both SDL-only and ImGui+SDL use SDL_Audio |
| `sdl_keyboard.cpp` — `SDLScan2MacKeyCode()` | stays separate, used by `SdlBackend` and `ImGuiBackend` | Scancode→MKC translation table. ImGui+SDL uses the same SDL scancodes |

### Functions that stay in core/ (unchanged)

| Function | File | Notes |
|---|---|---|
| `ProgramEarlyInit()` | core/main.cpp | CLI parsing, Machine creation |
| `InitEmulation()` | core/main.cpp | ICT wiring, device init |
| `RunEmulatedTicksToTrueTime()` | core/main.cpp | Made non-static, called by shell |
| `DoEmulateExtraTime()` | core/main.cpp | Made non-static, called by shell |
| `DoEmulateOneTick()` | core/main.cpp | Internal to core |
| `SixtiethSecondNotify()` / `SixtiethEndNotify()` | core/main.cpp | Internal to core |
| `ProgramCleanup()` | core/main.cpp | Machine teardown |

### Functions that stay in platform/common/ (unchanged)

| Function | File | Notes |
|---|---|---|
| `Screen_OutputFrame()` | osglu_common.cpp | Dirty-rect tracking, calls screen compare |
| `ScreenClearChanges()`, `ScreenChangedAll()` | osglu_common.cpp | Dirty-rect bookkeeping |
| `AutoScrollScreen()` | osglu_common.cpp | Scroll math |
| `Keyboard_UpdateKeyMap()`, `MyMouseButtonSet()`, etc. | osglu_common.cpp | Event queue writers |
| `InitKeyCodes()`, `DisconnectKeyCodes()` | osglu_common.cpp | Key state management |
| `EvtQTryRecoverFromFull()` | osglu_common.cpp | Event queue recovery |
| `MacMsg()`, `MacMsgOverride()`, `MacMsgDisplayOn/Off()` | control_mode.cpp | Message display state |
| `WaitForRom()` | control_mode.cpp | **Deleted.** Fail fast with error message if ROM not found |
| `DoControlModeKey()` | control_mode.cpp | Ctrl+key handling (sets globals like `g_wantMagnify`) |
| `ToggleWantFullScreen()` | currently sdl.cpp, **moves to shell** | Called by control_mode.cpp via function pointer or direct call |
| All disk I/O, ROM loading, tick timer, param buffers, path utils | various common/ files | Already platform-independent |

### WaitForRom — special case

`WaitForRom()` currently blocks in a loop calling `WaitForNextTick()`. It's called from `InitOSGLU()` before `ProgramMain()`. In the new architecture:

**Option A (recommended):** Make it a shell state. `EmulatorShell::init()` sets `waitingForRom_ = true` if no ROM loaded. The backend loop keeps running — SDL renders the "Insert ROM" control-mode screen, ImGui renders it inside its viewport. When a ROM file is dropped, `dispatchEvent(FileDrop)` loads it and clears the flag. Then `init()` continues.

**Option B (expedient):** Shell exposes a `pumpUntilRomLoaded()` that repeatedly calls `backend->runOneFrame()` (a new method). Simpler to implement but adds a method to the interface just for this one case.

### ToggleWantFullScreen — special case

This function is currently in sdl.cpp and called from `control_mode.cpp` via a direct call. It queries `SDL_GetDisplayBounds` to decide magnification. In the new architecture it moves to the shell and calls `backend_->getDisplayBounds()`. control_mode.cpp continues to call it directly — it's already declared in platform.h as `extern void ToggleWantFullScreen()`, and the shell implementation provides it.

## New Files

| File | Purpose |
|---|---|
| `src/platform/platform_backend.h` | `PlatformBackend` interface + `PlatformEvent` struct |
| `src/platform/emulator_shell.h` | `EmulatorShell` class declaration |
| `src/platform/emulator_shell.cpp` | Shell implementation: state machine, event dispatch, timing, disk orchestration |
| `src/platform/screen_convert.h` | `convertFramebuffer()` declaration + `ScrnMapr` instantiations |
| `src/platform/screen_convert.cpp` | CLUT build + depth-specific copy dispatch. `ScrnMapr_Dst` = ARGB buffer |
| `src/platform/sdl_backend.h` | `SdlBackend` class declaration |
| `src/platform/sdl_backend.cpp` | SDL implementation of `PlatformBackend` + `runLoop()` |
| `src/platform/app_main.cpp` | `main()` — creates backend + shell, wires them, calls `backend->runLoop()` |
| `src/platform/headless_backend.h/.cpp` | (future) Headless backend |
| `src/platform/imgui_backend.h/.cpp` | (future) ImGui backend |

## Framebuffer Conversion Detail

`HaveChangedScreenBuff()` in sdl.cpp (~150 lines) currently:
1. Locks SDL texture → gets pixel pointer
2. Builds CLUT lookup table (BWLUT or color CLUT → SDL pixel values)
3. Sets `ScalingBuff = pixels` (the texture's raw buffer)
4. Dispatches to depth-specific copy functions which write into `ScalingBuff`
5. Unlocks texture
6. Calls `SDL_RenderTexture` + `SDL_RenderPresent`

The coupling: `SDL_MapRGB()` is called to build the lookup table, and the copiers write directly into the SDL texture.

### Proposed split

Since we always output ARGB8888 (which is what the SDL texture format already is), and `SDL_MapRGB` for ARGB8888 is trivially `(0xFF << 24) | (r << 16) | (g << 8) | b`, the CLUT build doesn't need SDL at all.

```
screen_convert.cpp:
  buildClutTable(argbFormat)      — pure math, writes CLUT_final
  ScrnMapr instantiations         — write into shell's argbBuffer_
    ScrnMapr_Dst = shell->argbBuffer_
    ScrnMapr_Src = GetCurDrawBuff()
    ScrnMapr_Map = CLUT_final

EmulatorShell::convertFramebuffer():
  1. Build CLUT table (calls screen_convert)
  2. Dispatch to appropriate depth copier (calls screen_convert)
  3. Set framebufferDirty_ = true

Backend (during render):
  if (shell->isFramebufferDirty()):
    SDL: lock texture, memcpy from argbBuffer_, unlock, render
    ImGui: glTexSubImage2D from argbBuffer_
    Headless: optionally write to file
```

The slow path (per-pixel fallback for unusual bpp/pitch) also moves to screen_convert and writes to `argbBuffer_`.

## Sound Architecture

The sound subsystem is already well-separated and works identically for SDL and ImGui+SDL:

```
Core emulation (during tick):
  → Sound device writes samples via Sound_BeginWrite / Sound_EndWrite
  → These write into the ring buffer (s_soundBuffer)

SDL audio thread (async callback):
  → sdl3_audio_callback reads from the ring buffer
  → Feeds SDL_AudioStream
```

This model doesn't change. Both `SdlBackend` and `ImGuiBackend` use SDL audio under the hood. The backend's audio methods (`audioInit`, `audioStart`, `audioStop`, `audioShutdown`) are thin wrappers around the existing `Sound_Init()`, `Sound_Start()`, `Sound_Stop()`, `Sound_UnInit()`.

For a future non-SDL audio backend, only the callback plumbing changes. The ring buffer protocol stays the same. `Sound_BeginWrite` / `Sound_EndWrite` remain in platform.h as the core-facing API.

## Global Variables — Transition Strategy

The shell/core boundary currently communicates via globals in `platform.h`, `osglu_common.h`, and `intl_chars.h`. Examples:

- **Core → Shell**: `g_screenChangedTop/Left/Bottom/Right` (dirty rect), `g_forceMacOff`, `g_sonyNewDiskWanted`, `g_needWholeScreenDraw`, `g_requestIthDisk`
- **Shell → Core**: `g_curMouseH/V`, `theKeys[]`, `g_mouseButtonState`, `g_onTrueTime`, `g_curMacDateInSeconds`
- **Shared state**: `g_wantMagnify`, `g_wantFullScreen`, `g_speedStopped`, `g_runInBackground` (set by control_mode, read by shell)

**These globals stay for now.** The shell reads/writes them as the current code does. Replacing them with a proper interface is a separate, larger refactor that would touch core/ and every device. The immediate goal is to make the platform layer swappable, not to eliminate all globals.

## Migration Strategy

This can be done incrementally. Each phase produces a working build.

### Phase 1: Extract `screen_convert.cpp`
- Move CLUT building + all `ScrnMapr` instantiations from sdl.cpp
- Shell-owned `argbBuffer_` replaces SDL texture lock as the write target
- `HaveChangedScreenBuff()` in sdl.cpp calls screen_convert, then uploads to texture
- **No loop changes. No new interfaces. Just extraction.**
- Verify: selftest passes, visual output identical

### Phase 2: Create `PlatformBackend` interface + `SdlBackend`
- Create `platform_backend.h` with the interface
- Create `sdl_backend.cpp` wrapping existing SDL calls (constructor takes no shell — thin forwarding for now)
- sdl.cpp starts calling `SdlBackend` methods instead of raw SDL
- `CreateMainWindow` → `backend->createWindow()`, etc.
- **`WaitForNextTick` still exists. Loop unchanged.**
- Verify: selftest passes

### Phase 3: Create `EmulatorShell`, invert the loop
- Create `emulator_shell.cpp` — move all state machine, event dispatch, disk orchestration from sdl.cpp
- Make `RunEmulatedTicksToTrueTime()` and `DoEmulateExtraTime()` non-static in core/main.cpp
- Delete `MainEventLoop()` — the backend now drives via `shell.tickIsDue()` / `shell.runOneTick()`
- `SdlBackend` gets `runLoop()` which calls shell methods
- Delete `WaitForRom()` — if ROM loading fails, exit with a clear error
- sdl.cpp becomes `app_main.cpp` with just `main()` (~30 lines)
- Verify: selftest passes, interactive use works

### Phase 4: Add `ImGuiBackend`
- Create `imgui_backend.cpp` with its own `runLoop()`
- Shares `sdl_keyboard.cpp` scancode table and SDL audio
- Build system flag to choose backend
- Verify: emulator runs in ImGui viewport

### Phase 5: (Optional) Remove `SdlBackend`
- Once ImGui is the primary UI, the SDL-only backend can be removed
- `SdlBackend::runLoop()` deletion, `sdl_backend.cpp` deletion
- ImGuiBackend becomes the sole backend

### Phase 6: (Future) Add `HeadlessBackend`
- No window, no audio, tick-based timing
- Enables scripted/CI emulation, automated testing

## Naming Rationale

- **EmulatorShell** (not "Frontend", not "UI"): it's the shell around the emulation core that handles the host-side lifecycle. It's not a UI — it orchestrates the UI. "Shell" is accurate for the headless case too.
- **PlatformBackend** (not "Driver", not "Port"): it's the backend that a specific platform provides. Maps naturally to `SdlBackend`, `ImGuiBackend`, `HeadlessBackend`.
- **screen_convert** (not "screen_blit", not "pixel_convert"): it converts the emulated screen buffer to host-displayable ARGB pixels.
- **app_main.cpp** (not "entry.cpp", not "main.cpp"): avoids confusion with `core/main.cpp`.

## What Doesn't Change

- **`src/core/`** — Mostly untouched. `MainEventLoop()` is deleted and `RunEmulatedTicksToTrueTime` / `DoEmulateExtraTime` become non-static, but all emulation logic stays in place.
- **`src/platform/common/`** — Stays as-is. `disk_io.cpp`, `rom_loader.cpp`, `tick_timer.cpp`, `osglu_common.cpp`, `control_mode.cpp`, `param_buffers.cpp`, `path_utils.cpp` etc. are already platform-independent. (Exception: `WaitForRom()` in control_mode.cpp is deleted — see above.)
- **`sdl_keyboard.cpp`** — Already separate. Used by both SdlBackend and ImGuiBackend.
- **`sdl_sound.cpp`** — Already separate. Used by all SDL-based backends.
- **The global variable interface (`platform.h`)** — Stays for now. The shell populates these globals; the core reads them. Eliminating globals is a separate, future concern.
