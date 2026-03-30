# Platform Refactoring Plan

Reference: [PLATFORM_ARCH.md](PLATFORM_ARCH.md)

Build: `cmake --preset macos && cmake --build --preset macos`
Test: `cd test && ./verify.sh`

---

## Phase 1 — Extract screen_convert

Goal: Move all framebuffer conversion (CLUT/BW→ARGB8888) from sdl.cpp into
a standalone compilation unit. sdl.cpp continues to work but calls into
screen_convert instead of doing the conversion inline. No new interfaces,
no loop changes.

### 1.1 Create screen_convert.h

Create `src/platform/screen_convert.h` with:
- `extern uint8_t* ScalingBuff;` (the output buffer pointer, set by caller)
- `extern uint8_t* CLUT_final;` (the pre-computed CLUT table)
- `#define CLUT_FINAL_SZ (256 * 8 * 4)`
- Forward declarations for all 12 depth-copy functions:
  `UpdateBWDepth3Copy`, `UpdateBWDepth4Copy`, `UpdateBWDepth5Copy`,
  `UpdateColorSrc1Dst3Copy` through `UpdateColorSrc3Dst5Copy`
  (each takes `int16_t top, left, bottom, right`)
- `void BuildClutTable(...)` — builds the CLUT_final lookup from
  current color mode, depth, and CLUT_reds/greens/blues. Replaces the
  SDL_MapRGB calls with direct ARGB8888 packing.
- `void ConvertRect(uint16_t top, left, bottom, right)` — dispatches
  to the correct depth-copy based on current vMacScreenDepth, g_useColorMode,
  and bpp (always 4 for ARGB8888).
- `void ConvertRectSlow(uint8_t* dest, int pitch, uint16_t top, left, bottom, right)` — the
  per-pixel fallback path (currently the else branch in HaveChangedScreenBuff).

### 1.2 Create screen_convert.cpp

Create `src/platform/screen_convert.cpp`:
- `#include "platform/screen_convert.h"`
- Include required headers: `osglu_ui.h`, `osglu_common.h`, `platform.h`
- Define `uint8_t* ScalingBuff = nullptr;`
- Define `uint8_t* CLUT_final = nullptr;`  (remove `static` — now extern)
- All 12 `ScrnMapr` instantiations (moved from sdl.cpp):
  - `ScrnMapr_Dst` = `ScalingBuff` (via screen_map_inst.h, unchanged)
  - `ScrnMapr_Src` = `GetCurDrawBuff()` (via screen_map_inst.h, unchanged)
  - `ScrnMapr_Map` = `CLUT_final` (via screen_map_inst.h, unchanged)
- `BuildClutTable()`: replaces SDL_MapRGB with direct ARGB packing:
  `(0xFFu << 24) | (r << 16) | (g << 8) | b`
  Builds BWLUT or color CLUT, then populates CLUT_final exactly as
  the current code does but without any SDL dependency.
- `ConvertRect()`: the fast-path dispatcher — same switch logic as current
  code that selects `UpdateBWDepth3Copy` etc. based on depth and bpp.
  Since we always target ARGB8888 (bpp=4), this always calls the `Dst5`
  variants (DstDepth 5 = 32-bit = 4 bytes).
- `ConvertRectSlow()`: the per-pixel fallback. Writes into caller-provided
  buffer. Replaces SDL_MapRGB with the same ARGB packing.

### 1.3 Update sdl.cpp

- Remove the 12 `ScrnMapr` instantiation blocks (the `#define/#include` sequences).
- Remove `static uint8_t* ScalingBuff`, `static uint8_t* CLUT_final`, `CLUT_FINAL_SZ`.
- `#include "platform/screen_convert.h"`
- In `HaveChangedScreenBuff()`:
  - Remove CLUT_pixel/BWLUT_pixel local vars and the SDL_MapRGB calls that build them.
  - After `SDL_LockTexture`, set `ScalingBuff = (uint8_t*)pixels;`
  - Call `BuildClutTable()` then `ConvertRect(top, left, bottom, right)` for
    the fast path.
  - For the slow path, call `ConvertRectSlow((uint8_t*)pixels, pitch, ...)`.
  - Keep SDL_UnlockTexture, SDL_RenderTexture, SDL_RenderPresent as-is.
- In `AllocMyMemory()`: allocation of CLUT_final stays (or moves to screen_convert
  init function). Keep the `AllocBlock(&CLUT_final, ...)` call.
- In `UnallocMyMemory()`: `free(CLUT_final)` stays.

### 1.4 Update CMakeLists.txt

Add `src/platform/screen_convert.cpp` to `MINIVMAC_SOURCES`.

### 1.5 Validate

```sh
cmake --preset macos && cmake --build --preset macos
cd test && ./verify.sh
```

### 1.6 Commit

```sh
git add -A && git commit -m "platform: extract screen_convert from sdl.cpp"
```

---

## Phase 2 — Create PlatformBackend interface + SdlBackend wrapper

Goal: Introduce the `PlatformBackend` abstract class and a `SdlBackend`
that wraps existing SDL calls. sdl.cpp delegates to SdlBackend for primitive
operations. The main loop is unchanged — this is a mechanical extraction.

### 2.1 Create platform_backend.h

Create `src/platform/platform_backend.h` with:
- `struct PlatformEvent` (Type enum, macKeyCode, x/y, wheelX/Y, filePath)
- `class PlatformBackend` abstract class with pure virtual methods:
  - Lifecycle: `init(EmulatorShell*)`, `shutdown()`, `runLoop()`
  - Window: `createWindow`, `destroyWindow`, `recreateWindow`, `getWindowSize`,
    `getWindowPosition`, `setFullscreen`
  - Cursor: `showCursor`, `hideCursor`, `warpCursor`, `setMouseGrab`
  - Audio: `audioInit`, `audioStart`, `audioStop`, `audioShutdown`
  - Keyboard: `disableKeyRepeat`, `restoreKeyRepeat`
  - Dialog: `showMessageBox`
  - Query: `getDisplayBounds`

  Note: `runLoop()` is declared but **not yet called** — sdl.cpp still owns
  the loop in this phase. It will be wired in Phase 3.

### 2.2 Create sdl_backend.h / sdl_backend.cpp

Create `src/platform/sdl_backend.h`:
- `class SdlBackend : public PlatformBackend`
- Private members for SDL objects: `SDL_Window*`, `SDL_Renderer*`,
  `SDL_Texture*`, `const SDL_PixelFormatDetails*`
- `MyWState` struct (moved from sdl.cpp)

Create `src/platform/sdl_backend.cpp`:
- Implement all PlatformBackend methods by wrapping existing SDL code:
  - `init()`: calls `SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO)` (from `Screen_Init`)
  - `shutdown()`: calls `SDL_QuitSubSystem`, `SDL_Quit`
  - `createWindow()`: contains the body of `CreateMainWindow()` — SDL_CreateWindow,
    SDL_CreateRenderer, SDL_SetRenderLogicalPresentation, SDL_CreateTexture,
    SDL_SetTextureScaleMode, SDL_GetPixelFormatDetails.
    Returns bool. Stores SDL objects in members.
  - `destroyWindow()`: body of `CloseMainWindow()`
  - `recreateWindow()`: orchestrates destroy + create with state save/restore
    (the `GetMyWState`/`SetMyWState` logic moves here)
  - `getWindowSize()`: `SDL_GetWindowSizeInPixels`
  - `getWindowPosition()`: `SDL_GetWindowPosition`
  - `setFullscreen()`: `SDL_SetWindowFullscreen`
  - `showCursor()` / `hideCursor()`: `SDL_ShowCursor` / `SDL_HideCursor`
  - `warpCursor()`: `SDL_WarpMouseInWindow`
  - `setMouseGrab()`: `SDL_SetWindowMouseGrab`
  - `audioInit()`: delegates to existing `Sound_Init()`
  - `audioStart()` / `audioStop()`: delegates to `Sound_Start()` / `Sound_Stop()`
  - `audioShutdown()`: delegates to `Sound_UnInit()`
  - `disableKeyRepeat()` / `restoreKeyRepeat()`: delegates to existing functions
  - `showMessageBox()`: `SDL_ShowSimpleMessageBox`
  - `getDisplayBounds()`: `SDL_GetDisplayBounds`
  - `runLoop()`: **stub** — empty body or assert. Will be filled in Phase 3.
- Expose getters for SDL objects that sdl.cpp still needs during this
  transitional phase: `renderer()`, `texture()`, `format()`, `window()`.

### 2.3 Update sdl.cpp

- `#include "platform/sdl_backend.h"`
- Create a file-scope `static SdlBackend* s_backend;` pointer
  (allocated in `InitOSGLU`, freed in `UnInitOSGLU`).
- Replace direct SDL calls with backend calls:
  - `Screen_Init()` body → `s_backend->init(nullptr)` (no shell yet)
  - `CreateMainWindow()` body → `s_backend->createWindow(...)`
  - `CloseMainWindow()` body → `s_backend->destroyWindow()`
  - `ForceShowCursor()` → `s_backend->showCursor()`
  - `SDL_HideCursor()`/`SDL_ShowCursor()` in `CheckForSavedTasks` → `s_backend->hideCursor()`/`showCursor()`
  - `SDL_WarpMouseInWindow` in `MoveMouse` → `s_backend->warpCursor()`
  - `SDL_SetWindowMouseGrab` in `GrabTheMachine`/`UngrabMachine` → `s_backend->setMouseGrab()`
  - `Sound_Init/Start/Stop/UnInit` → `s_backend->audioInit/Start/Stop/Shutdown()`
  - `SDL_ShowSimpleMessageBox` in `CheckSavedMacMsg` → `s_backend->showMessageBox()`
  - `SDL_GetDisplayBounds` in `ToggleWantFullScreen` → `s_backend->getDisplayBounds()`
  - `DisableKeyRepeat/RestoreKeyRepeat` → `s_backend->disableKeyRepeat/restoreKeyRepeat()`
- `HaveChangedScreenBuff()` still uses SDL texture via backend getters:
  `s_backend->texture()`, `s_backend->renderer()`, etc.
  This will be cleaned up in Phase 3.
- `ReCreateMainWindow()` → calls `s_backend->recreateWindow(...)` instead of
  manual GetMyWState/SetMyWState dance.
- Remove `static SDL_Window* my_main_wind`, `my_renderer`, `my_texture`, `my_format`
  — these now live in SdlBackend.
- Remove `Screen_Init()`, `CreateMainWindow()`, `CloseMainWindow()`,
  `GetMyWState()`, `SetMyWState()`, `ZapMyWState()`, `MyWState` struct — moved to SdlBackend.

### 2.4 Update CMakeLists.txt

Add `src/platform/sdl_backend.cpp` to `MINIVMAC_SOURCES`.

### 2.5 Validate

```sh
cmake --preset macos && cmake --build --preset macos
cd test && ./verify.sh
```

### 2.6 Commit

```sh
git add -A && git commit -m "platform: introduce PlatformBackend + SdlBackend wrapper"
```

---

## Phase 3 — Create EmulatorShell, invert the loop

Goal: Move all platform-independent logic from sdl.cpp into EmulatorShell.
Invert the control flow so the backend drives the frame loop.
Delete MainEventLoop(). sdl.cpp becomes app_main.cpp (~30 lines).

This is the largest phase. The steps below are ordered to keep the build
working at each sub-step where possible, but the validate/commit gate
is at the end of the full phase.

### 3.1 Export core functions

In `src/core/main.cpp`:
- Make `RunEmulatedTicksToTrueTime()` non-static (remove `static`).
- Make `DoEmulateExtraTime()` non-static.
- Make `InitEmulation()` non-static.
- Add `DoneWithDrawingForTick()` declaration if not already extern
  (it's currently in sdl.cpp — it will move to the shell).
- Delete `MainEventLoop()`.
- Simplify `ProgramMain()` to just call `InitEmulation()` and return
  success/failure (the tick loop is now driven by the backend):
  ```cpp
  bool ProgramMain() { return InitEmulation(); }
  ```

In `src/core/main.h`:
- Add: `extern bool InitEmulation();`
- Add: `extern void RunEmulatedTicksToTrueTime();`
- Add: `extern void DoEmulateExtraTime();`
- Change `ProgramMain()` return type to `bool`.

### 3.2 Create emulator_shell.h

Create `src/platform/emulator_shell.h`:
- `class EmulatorShell` per PLATFORM_ARCH.md design.
- Public API:
  - `EmulatorShell(PlatformBackend* backend)`
  - `bool init(int argc, char** argv)` — runs the full init sequence
    (AllocMyMemory, InitWhereAmI, ScanCommandLine, LoadMacRom,
    LoadInitialImages, InitLocationDat, backend→createWindow,
    backend→audioInit, ProgramMain/InitEmulation)
  - `void shutdown()` — full teardown (UnInitOSGLU equivalent)
  - `void dispatchEvent(const PlatformEvent& evt)` — translates to emulator
    actions (key, mouse, disk drop, focus, window events)
  - `void processSavedTasks()` — state machine
  - `bool tickIsDue()` — timing check (replaces ExtraTimeNotOver logic)
  - `void runOneTick()` — calls RunEmulatedTicksToTrueTime + DoEmulateExtraTime
    + DoneWithDrawingForTick
  - `uint32_t getDelayMs()` — how long to sleep
  - `bool shouldQuit()` — returns g_forceMacOff
  - `bool isSpeedStopped()` — for backend to decide blocking wait vs poll
  - `bool isFramebufferDirty()`
  - `const uint8_t* getFramebuffer()`
  - `int getScreenWidth()` / `getScreenHeight()`
  - `void clearDirtyFlag()`
  - `bool insertDiskOrRom(const char* path, bool silent)` — Sony_Insert1a
- Private members: all state from sdl.cpp static vars (useFullScreen_, useMagnify_,
  windowScale_, backgroundFlag_, trueBackgroundFlag_, curSpeedStopped_,
  grabMachine_, haveCursorHidden_, wantCursorHidden_, caughtMouse_,
  hOffset_, vOffset_, window position tracking arrays, argbBuffer_).

### 3.3 Create emulator_shell.cpp

Create `src/platform/emulator_shell.cpp` — move code from sdl.cpp:

**Init/shutdown** (from InitOSGLU / UnInitOSGLU):
- `init()`: AllocMyMemory, InitWhereAmI, ScanCommandLine (or delete since
  ProgramEarlyInit already parsed CLI), LoadMacRom, LoadInitialImages,
  InitLocationDat, backend_→init(this), backend_→audioInit(),
  backend_→createWindow(...), ProgramMain() (now just InitEmulation).
  Remove WaitForRom() call — if ROM load fails, return false.
- `shutdown()`: the body of UnInitOSGLU, calling backend_ methods instead of
  raw SDL.

**Event dispatch** (from HandleTheEvent):
- `dispatchEvent()`: switch on PlatformEvent::Type — delegates to
  mousePositionNotify, MyMouseButtonSet, Keyboard_updateKeyMap2,
  Sony_Insert1a for FileDrop, trueBackgroundFlag_ for Focus, etc.
  The scancode→MKC translation is already done by the backend.

**State machine** (from CheckForSavedTasks):
- `processSavedTasks()`: same logic, but SDL calls replaced with
  backend_ calls (showCursor/hideCursor, recreateWindow, setMouseGrab).
  New disk creation code moves here. The cursor hide/show at the end
  uses backend_→showCursor()/hideCursor().

**Timing** (from WaitForNextTick):
- `tickIsDue()`: calls `UpdateTrueEmulatedTime()`, returns whether
  `g_trueEmulatedTime != g_onTrueTime`. For `g_SkipThrottle` mode
  (record/verify), always returns true after advancing `g_onTrueTime`.
- `runOneTick()`: calls `RunEmulatedTicksToTrueTime()` +
  `DoEmulateExtraTime()` + `drawChangesAndClear()`. Updates
  `g_onTrueTime` and calls `CheckDateTime()`/`Sound_SecondNotify()`
  as needed.
- `getDelayMs()`: returns `GetTimerDelay()`.

**Framebuffer** (from HaveChangedScreenBuff / DrawChangesAndClear):
- `drawChangesAndClear()`: if dirty rect exists, call `convertFramebuffer()`
  then `ScreenClearChanges()`. Sets `framebufferDirty_ = true`.
- `convertFramebuffer()`: set `ScalingBuff = argbBuffer_`, call
  `BuildClutTable()`, call `ConvertRect()` (from screen_convert).
  For fullscreen, apply view clipping. No SDL calls.

**Mouse** (from MousePositionNotify, MoveMouse, MouseConstrain, CheckMouseState):
- All move to shell methods. `moveMouse()` calls `backend_→warpCursor()`.

**Grab/ungrab** (from GrabTheMachine, UngrabMachine):
- Move to shell. Call `backend_→setMouseGrab()`.

**Background/speed** (from LeaveBackground, EnterBackground, LeaveSpeedStopped, EnterSpeedStopped):
- Move to shell. Call `backend_→restoreKeyRepeat()`, `backend_→audioStart()` etc.

**Fullscreen toggle** (from ToggleWantFullScreen):
- Move to shell. Calls `backend_→getDisplayBounds()`.
- Provide the `extern "C"` or namespace-qualified wrapper that control_mode.cpp
  calls (keep the `ToggleWantFullScreen()` free function, have it delegate to
  a global shell pointer).

**Disk orchestration** (from Sony_Insert1a, Sony_Insert2, Sony_InsertIth, LoadInitialImages):
- Move to shell.

**Remaining utilities**:
- `NativeStrFromCStr()` — move to shell or common.
- `MoveBytes()` — move to common (it's a memcpy wrapper).
- `CheckSavedMacMsg()` — inline into shutdown(), calls backend_→showMessageBox().
- `app_parent`, `pref_dir`, `d_arg`, `n_arg` — become shell members.

### 3.4 Delete WaitForRom

In `src/platform/common/control_mode.cpp`:
- Delete the `WaitForRom()` function body. Replace with a stub that returns
  `g_romLoaded` (or delete entirely if no callers remain after init changes).
- The shell's `init()` checks `g_romLoaded` after `LoadMacRom()` and returns
  false with an error message if it failed.

### 3.5 Wire SdlBackend::runLoop()

In `src/platform/sdl_backend.cpp`, implement `runLoop()`:
```
while (!shell_->shouldQuit()):
  poll SDL events → translate to PlatformEvent → shell_->dispatchEvent()
  shell_->processSavedTasks()
  if shouldQuit: break
  if isSpeedStopped:
    // render current frame, then block
    present if dirty
    SDL_WaitEvent → translate → dispatchEvent
    continue
  if !tickIsDue:
    SDL_Delay(shell_->getDelayMs())
    continue
  while tickIsDue:
    shell_->runOneTick()
  present if framebufferDirty:
    SDL_LockTexture, memcpy from getFramebuffer(), SDL_UnlockTexture
    SDL_RenderTexture, SDL_RenderPresent
    shell_->clearDirtyFlag()
```

Event translation helper `translateSdlEvent()`:
- Uses `SDLScan2MacKeyCode()` for key events (include sdl_keyboard.h)
- Calls `SDL_ConvertEventToRenderCoordinates` for mouse coordinate mapping
- Maps SDL event types to PlatformEvent::Type

Also handle the `CheckMouseState()` equivalent: if not background and not
caught mouse, read mouse position via `SDL_GetMouseState` and feed as a
MouseMove event (once per frame after ticks).

### 3.6 Replace sdl.cpp with app_main.cpp

Rename `src/platform/sdl.cpp` → `src/platform/app_main.cpp`.
Replace its contents with ~30 lines:

```cpp
#include "platform/sdl_backend.h"
#include "platform/emulator_shell.h"
#include "core/main.h"

int main(int argc, char** argv) {
    ProgramEarlyInit(argc, argv);
    const LaunchConfig& lc = GetLaunchConfig();
    if (lc.help) return 0;

    SdlBackend backend;
    EmulatorShell shell(&backend);

    if (!shell.init(argc, argv)) {
        shell.shutdown();
        ProgramCleanup();
        return 1;
    }

    backend.runLoop();

    shell.shutdown();
    ProgramCleanup();
    return 0;
}
```

### 3.7 Update CMakeLists.txt

- Replace `src/platform/sdl.cpp` with `src/platform/app_main.cpp`
- Add `src/platform/emulator_shell.cpp`
- Keep `src/platform/sdl_backend.cpp`, `src/platform/screen_convert.cpp`

### 3.8 Provide global ToggleWantFullScreen wrapper

Since control_mode.cpp calls `ToggleWantFullScreen()` as a free function
(declared `extern` in platform.h), provide a thin wrapper in
emulator_shell.cpp:

```cpp
static EmulatorShell* g_shell = nullptr;  // set during init

void ToggleWantFullScreen() {
    if (g_shell) g_shell->toggleWantFullScreen();
}
```

Similarly, `DoneWithDrawingForTick()` needs a free-function wrapper
since core/main.cpp (RunEmulatedTicksToTrueTime) calls it:

```cpp
void DoneWithDrawingForTick() {
    if (g_shell) g_shell->drawChangesAndClear();
}
```

And `ExtraTimeNotOver()`:
```cpp
bool ExtraTimeNotOver() {
    return g_shell ? g_shell->tickIsDue() : false;
}
```

### 3.9 Validate

```sh
cmake --preset macos && cmake --build --preset macos
cd test && ./verify.sh
```

### 3.10 Commit

```sh
git add -A && git commit -m "platform: create EmulatorShell, invert loop, delete sdl.cpp"
```
