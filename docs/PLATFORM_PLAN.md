# Frontend Implementation Plan

Goal: Add a headless backend (for CI golden-file testing without SDL) and
an ImGui backend (for a rich debug/development UI). Both build on the
architecture established by the platform refactoring (app_main + EmulatorShell
+ PlatformBackend).

Each phase produces a working build. Validate with `cmake --build` and
`test/verify.sh` at every gate.

---

## Phase 1 — Decouple SDL from common sources

Before adding non-SDL backends, two files that are always compiled have
SDL dependencies that block headless builds:

- `sdl_keyboard.cpp` — includes `<SDL3/SDL.h>`, linked into all builds
- `sdl_sound.cpp` — includes `<SDL3/SDL.h>`, linked into all builds

The headless backend has no SDL. We need these files excluded from headless
builds, or their interfaces abstracted so the shell doesn't depend on SDL
symbols at link time.

### 1.1 Split sdl_sound into interface + SDL implementation

Create `src/platform/sound_interface.h`:
```cpp
void Sound_Start();
void Sound_Stop();
bool Sound_Init();
void Sound_UnInit();
void Sound_SecondNotify();
bool Sound_AllocBuffer();
void Sound_FreeBuffer();
```

`sdl_sound.h` retains the SDL-specific includes and the SDL implementation.
Create `src/platform/null_sound.cpp` that provides stubs (all no-ops,
`Sound_Init` returns true) for headless builds.

### 1.2 Guard sdl_keyboard for headless

`sdl_keyboard.h` exposes `SDLScan2MacKeyCode(SDL_Scancode)` which uses
`SDL_Scancode` — an SDL type. The headless backend doesn't call it.

Approach: don't compile `sdl_keyboard.cpp` in headless builds. The
`PlatformBackend::translateSdlEvent()` that calls it lives in
`sdl_backend.cpp` (not compiled for headless either). The shell never
calls `SDLScan2MacKeyCode` directly.

Check: `emulator_shell.cpp` must have no `#include "sdl_keyboard.h"`.
(Currently confirmed: it doesn't.)

### 1.3 Restructure CMakeLists.txt for multi-backend

```cmake
# ── Common sources (no SDL dependency) ──
set(COMMON_SOURCES
    src/core/...
    src/devices/...
    src/cpu/...
    src/platform/common/...
    src/platform/screen_convert.cpp
    src/platform/emulator_shell.cpp
    src/lang/...
)

# ── SDL-common sources (shared by sdl + imgui backends) ──
set(SDL_COMMON_SOURCES
    src/platform/sdl_sound.cpp
    src/platform/sdl_keyboard.cpp
)

# ── Backend selection ──
set(MAXIVMAC_BACKEND "sdl" CACHE STRING "Backend: sdl, imgui, headless")

if(MAXIVMAC_BACKEND STREQUAL "sdl")
    set(BACKEND_SOURCES
        src/platform/sdl_backend.cpp
        src/platform/app_main.cpp
    )
    set(NEED_SDL ON)
elseif(MAXIVMAC_BACKEND STREQUAL "imgui")
    set(BACKEND_SOURCES
        src/platform/imgui_backend.cpp
        src/platform/imgui_main.cpp
    )
    set(NEED_SDL ON)        # ImGui backend uses SDL3 for windowing
    set(NEED_IMGUI ON)
elseif(MAXIVMAC_BACKEND STREQUAL "headless")
    set(BACKEND_SOURCES
        src/platform/headless_backend.cpp
        src/platform/headless_main.cpp
        src/platform/null_sound.cpp
    )
    set(NEED_SDL OFF)
endif()

set(MINIVMAC_SOURCES ${COMMON_SOURCES} ${BACKEND_SOURCES})
if(NEED_SDL)
    list(APPEND MINIVMAC_SOURCES ${SDL_COMMON_SOURCES})
endif()

add_executable(maxivmac ${MINIVMAC_SOURCES})

if(NEED_SDL)
    find_package(SDL3 REQUIRED)
    target_link_libraries(maxivmac PRIVATE SDL3::SDL3)
endif()
if(NEED_IMGUI)
    # See Phase 4 for ImGui integration details
endif()
```

### 1.4 Add CMake presets for each backend

In `CMakePresets.json`, add presets:
- `macos` — existing, uses sdl backend (default)
- `macos-headless` — sets `MAXIVMAC_BACKEND=headless`
- `macos-imgui` — sets `MAXIVMAC_BACKEND=imgui`

### 1.5 Validate

```sh
# SDL build still works:
cmake --preset macos && cmake --build --preset macos
cd test && ./verify.sh

# Headless builds (won't pass verify yet — no headless backend):
cmake --preset macos-headless && cmake --build --preset macos-headless
```

### 1.6 Commit

```sh
git add -A && git commit -m "build: restructure CMake for multi-backend (sdl, imgui, headless)"
```

---

## Phase 2 — HeadlessBackend

Create a minimal headless backend that runs the emulator without any
windowing, audio, or input. Uses `g_SkipThrottle` (already set by
`--verify`) to run at max speed.

### 2.1 Create headless_backend.h/.cpp

`src/platform/headless_backend.h`:
```cpp
#include "platform/platform_backend.h"

class HeadlessBackend : public PlatformBackend {
public:
    bool init(EmulatorShell* shell) override;
    void shutdown() override;
    void runLoop() override;

    // Window — no-ops returning success
    bool createWindow(const char*, int, int, bool) override { return true; }
    void destroyWindow() override {}
    bool recreateWindow(const char*, int, int, bool) override { return true; }
    void getWindowSize(int* w, int* h) override { *w = 0; *h = 0; }
    void getWindowPosition(int* x, int* y) override { *x = 0; *y = 0; }
    void setWindowPosition(int, int) override {}
    void setFullscreen(bool) override {}
    void clearScreen() override {}

    // Cursor — no-ops
    void showCursor() override {}
    void hideCursor() override {}
    bool warpCursor(int, int) override { return true; }
    void setMouseGrab(bool) override {}

    // Audio — no-ops
    bool audioInit() override { return true; }
    void audioStart() override {}
    void audioStop() override {}
    void audioShutdown() override {}

    // Keyboard — no-ops
    void disableKeyRepeat() override {}
    void restoreKeyRepeat() override {}

    // Dialog — stderr
    void showMessageBox(const char* t, const char* m) override;

    // Query
    bool getDisplayBounds(PlatformDisplayBounds*) override { return false; }

private:
    EmulatorShell* shell_ = nullptr;
};
```

`src/platform/headless_backend.cpp` — `runLoop()`:
```cpp
void HeadlessBackend::runLoop() {
    while (!shell_->shouldQuit()) {
        shell_->processSavedTasks();
        if (shell_->shouldQuit()) break;
        while (shell_->tickIsDue() && !shell_->shouldQuit())
            shell_->runOneTick();
    }
}
```

### 2.2 Create headless_main.cpp

`src/platform/headless_main.cpp`:
```cpp
#include "platform/headless_backend.h"
#include "platform/emulator_shell.h"
#include "core/main.h"

int main(int argc, char** argv) {
    ProgramEarlyInit(argc, argv);
    const LaunchConfig& lc = GetLaunchConfig();
    if (lc.help) return 0;

    HeadlessBackend backend;
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

### 2.3 Create null_sound.cpp

`src/platform/null_sound.cpp` — stub implementations of the sound
interface for headless builds (no SDL dependency):
```cpp
#include "platform/sdl_sound.h"  // or sound_interface.h
void Sound_Start() {}
void Sound_Stop() {}
bool Sound_Init() { return true; }
void Sound_UnInit() {}
void Sound_SecondNotify() {}
bool Sound_AllocBuffer() { return true; }
void Sound_FreeBuffer() {}
```

### 2.4 Validate

```sh
# Headless build:
cmake --preset macos-headless && cmake --build --preset macos-headless

# Headless can run verify (uses --verify which sets g_SkipThrottle):
cd test && EMU=../bld/macos-headless/maxivmac ./verify.sh

# SDL build still passes:
cmake --preset macos && cmake --build --preset macos
cd test && ./verify.sh
```

The headless verify.sh may need a small tweak to accept an `EMU` override,
or we create a separate test script. Either way, golden-file output must
match the SDL build exactly — the emulation is deterministic and both
backends call the same shell/core code.

### 2.5 Commit

```sh
git add -A && git commit -m "platform: add HeadlessBackend for CI testing"
```

---

## Phase 3 — Delete dead sdl.cpp

The old monolithic `sdl.cpp` is no longer compiled. Remove it.

```sh
rm src/platform/sdl.cpp
git add -A && git commit -m "platform: delete dead sdl.cpp"
```

---

## Phase 4 — Vendor Dear ImGui

Dear ImGui is designed to be vendored (dropped into the source tree).
There is no system package. We vendor it as a git subtree or a copy
under `libs/imgui/`.

### 4.1 Fetch ImGui + SDL3+OpenGL3 backends

```sh
mkdir -p libs/imgui
# Download imgui release (v1.91.x or latest)
# Required files:
#   imgui.h, imgui.cpp, imgui_draw.cpp, imgui_tables.cpp, imgui_widgets.cpp
#   imgui_demo.cpp        (optional, useful for development)
#   backends/imgui_impl_sdl3.h, backends/imgui_impl_sdl3.cpp
#   backends/imgui_impl_opengl3.h, backends/imgui_impl_opengl3.cpp
```

File layout:
```
libs/imgui/
    imgui.h
    imgui.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    imgui_demo.cpp
    imgui_internal.h
    imconfig.h
    imstb_rectpack.h
    imstb_textedit.h
    imstb_truetype.h
    backends/
        imgui_impl_sdl3.h
        imgui_impl_sdl3.cpp
        imgui_impl_opengl3.h
        imgui_impl_opengl3.cpp
        imgui_impl_opengl3_loader.h
```

### 4.2 Add ImGui to CMake

```cmake
if(NEED_IMGUI)
    set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/libs/imgui")
    set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_sources(maxivmac PRIVATE ${IMGUI_SOURCES})
    target_include_directories(maxivmac PRIVATE
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )
    # OpenGL
    find_package(OpenGL REQUIRED)
    target_link_libraries(maxivmac PRIVATE OpenGL::GL)
endif()
```

### 4.3 Validate

```sh
cmake --preset macos-imgui && cmake --build --preset macos-imgui
# Just verify it compiles. No backend yet.
```

### 4.4 Commit

```sh
git add -A && git commit -m "vendor: add Dear ImGui v1.91.x with SDL3+OpenGL3 backends"
```

---

## Phase 5 — ImGuiBackend: minimal working backend

Create an ImGui backend that renders the emulator viewport in an ImGui
window. No debug windows yet — just the emulated screen and a menu bar.

### 5.1 Create imgui_backend.h

`src/platform/imgui_backend.h`:
```cpp
#include "platform/platform_backend.h"
#include <SDL3/SDL.h>
#include <imgui.h>

typedef unsigned int GLuint;

class ImGuiBackend : public PlatformBackend {
public:
    bool init(EmulatorShell* shell) override;
    void shutdown() override;
    void runLoop() override;

    bool createWindow(const char* title,
        int width, int height, bool fullscreen) override;
    void destroyWindow() override;
    bool recreateWindow(const char* title,
        int width, int height, bool fullscreen) override;
    void getWindowSize(int* w, int* h) override;
    void getWindowPosition(int* x, int* y) override;
    void setWindowPosition(int x, int y) override;
    void setFullscreen(bool fullscreen) override;
    void clearScreen() override;

    void showCursor() override;
    void hideCursor() override;
    bool warpCursor(int x, int y) override;
    void setMouseGrab(bool grab) override;

    bool audioInit() override;
    void audioStart() override;
    void audioStop() override;
    void audioShutdown() override;

    void disableKeyRepeat() override;
    void restoreKeyRepeat() override;

    void showMessageBox(const char* title, const char* message) override;
    bool getDisplayBounds(PlatformDisplayBounds* bounds) override;

private:
    EmulatorShell* shell_ = nullptr;
    SDL_Window* window_ = nullptr;
    SDL_GLContext glContext_ = nullptr;
    GLuint emuTextureId_ = 0;

    PlatformEvent translateSdlEvent(SDL_Event& event);
    bool imGuiConsumedEvent(const SDL_Event& event) const;
    void uploadFramebuffer();
    void drawMenuBar();
    void drawEmulatorViewport();
};
```

### 5.2 Create imgui_backend.cpp

Key implementation details:

**init():**
- `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)`
- `SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE)`
- `SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3)` / minor 2
- Store shell pointer, call `InitKeyCodes()`

**createWindow():**
- `SDL_CreateWindow(title, w*2, h*2, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE)`
  (2x to give ImGui room for UI around the viewport)
- `SDL_GL_CreateContext(window_)`
- `SDL_GL_MakeCurrent(window_, glContext_)`
- `SDL_GL_SetSwapInterval(1)` (vsync)
- `ImGui::CreateContext()`
- `ImGui_ImplSDL3_InitForOpenGL(window_, glContext_)`
- `ImGui_ImplOpenGL3_Init("#version 150")`
- `glGenTextures(1, &emuTextureId_)`
- Configure texture: `GL_NEAREST` filtering, `GL_CLAMP_TO_EDGE`
- Allocate initial texture storage: `glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, ...)`

**runLoop():**
```cpp
void ImGuiBackend::runLoop() {
    while (!shell_->shouldQuit()) {
        // 1. Poll events — ImGui first
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (!imGuiConsumedEvent(event)) {
                PlatformEvent pe = translateSdlEvent(event);
                if (pe.type != PlatformEvent::Type::None)
                    shell_->dispatchEvent(pe);
            }
        }

        // 2. State machine
        shell_->processSavedTasks();
        if (shell_->shouldQuit()) break;

        // 3. Run emulation ticks
        while (shell_->tickIsDue() && !shell_->shouldQuit())
            shell_->runOneTick();

        // 4. Upload framebuffer to GL texture if dirty
        uploadFramebuffer();

        // 5. ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        drawMenuBar();
        drawEmulatorViewport();

        ImGui::Render();

        int displayW, displayH;
        SDL_GetWindowSizeInPixels(window_, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window_);
    }
}
```

**imGuiConsumedEvent():**
```cpp
bool ImGuiBackend::imGuiConsumedEvent(const SDL_Event& event) const {
    ImGuiIO& io = ImGui::GetIO();
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            return io.WantCaptureKeyboard;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_WHEEL:
            return io.WantCaptureMouse;
        default:
            return false;
    }
}
```

**translateSdlEvent():**
Identical to `SdlBackend::translateSdlEvent()`. Both use `SDLScan2MacKeyCode()`
and `SDL_ConvertEventToRenderCoordinates()`.

To avoid duplicating this: either make it a free function in a shared file
(e.g., `sdl_event_translate.h/.cpp`), or have ImGuiBackend include and call
the same code. The simplest approach is a shared header with an inline
implementation, since both backends already include `sdl_keyboard.h`.

**uploadFramebuffer():**
```cpp
void ImGuiBackend::uploadFramebuffer() {
    if (!shell_->isFramebufferDirty()) return;
    glBindTexture(GL_TEXTURE_2D, emuTextureId_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
        vMacScreenWidth, vMacScreenHeight,
        GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
        shell_->getFramebuffer());
    shell_->clearDirtyFlag();
}
```

Note on pixel format: the shell's framebuffer is ARGB8888 (host-endian).
On little-endian (x86/ARM), this is `0xAARRGGBB` stored as `BB GG RR AA`
in memory, which is `GL_BGRA` + `GL_UNSIGNED_INT_8_8_8_8_REV`. If this
doesn't look right, fall back to `GL_RGBA` + `GL_UNSIGNED_BYTE` with a
swizzle, or adjust `screen_convert.cpp` to output `GL_RGBA` order.
Test empirically.

**drawMenuBar():**
```cpp
void ImGuiBackend::drawMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Insert Disk...", "Ctrl+O"))
                { /* file dialog or drop target */ }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Ctrl+Q"))
                { /* set g_forceMacOff */ }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Machine")) {
            if (ImGui::MenuItem("Speed: 1x", nullptr, !g_speedStopped))
                { /* toggle */ }
            if (ImGui::MenuItem("Fullscreen", "Ctrl+F"))
                shell_->toggleWantFullScreen();
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}
```

**drawEmulatorViewport():**
```cpp
void ImGuiBackend::drawEmulatorViewport() {
    ImGui::SetNextWindowSize(
        ImVec2(vMacScreenWidth + 16, vMacScreenHeight + 36),
        ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Macintosh")) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        // Maintain aspect ratio
        float scale = std::min(
            avail.x / vMacScreenWidth,
            avail.y / vMacScreenHeight);
        ImVec2 size(vMacScreenWidth * scale, vMacScreenHeight * scale);
        ImGui::Image((ImTextureID)(intptr_t)emuTextureId_, size);
    }
    ImGui::End();
}
```

### 5.3 Create imgui_main.cpp

`src/platform/imgui_main.cpp`:
```cpp
#include "platform/imgui_backend.h"
#include "platform/emulator_shell.h"
#include "core/main.h"

int main(int argc, char** argv) {
    ProgramEarlyInit(argc, argv);
    const LaunchConfig& lc = GetLaunchConfig();
    if (lc.help) return 0;

    ImGuiBackend backend;
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

### 5.4 Extract shared SDL event translation

Create `src/platform/sdl_event_translate.h` with the `translateSdlEvent()`
function used by both `SdlBackend` and `ImGuiBackend`. Move the implementation
from `sdl_backend.cpp` and have both backends call it.

This avoids code duplication and ensures consistent event handling.

### 5.5 Audio: reuse sdl_sound

The ImGui backend uses SDL3 for audio — same as the SDL backend. The
existing `sdl_sound.cpp` works unchanged. `audioInit()` calls `Sound_Init()`,
etc. The PlatformBackend audio methods in ImGuiBackend delegate identically
to SdlBackend.

### 5.6 Validate

```sh
cmake --preset macos-imgui && cmake --build --preset macos-imgui
./bld/macos-imgui/maxivmac --model=MacPlus --rom=roms/MacPlus.ROM extras/disks/608.hfs
# Visual check: emulator screen appears in ImGui window, keyboard/mouse work
```

### 5.7 Commit

```sh
git add -A && git commit -m "platform: add ImGuiBackend with emulator viewport"
```

---

## Phase 6 — ImGui debug windows

Add debug/development windows. These are purely additive — each is an
independent ImGui window drawn in `drawDebugWindows()`.

### 6.1 Memory viewer

Display RAM contents as a hex grid with ASCII sidebar. Uses ImGui's
`ImGui::InputText` for an address field and `ImGui::Text` for the hex dump.

Reads from `g_ram[]` (via `machine.h`) — no new core API needed.

### 6.2 Register viewer

Show CPU registers (D0-D7, A0-A7, PC, SR) updated every tick.
Requires exposing current register state from the CPU. Check if
`m68k.cpp` already has an accessor or if we need to add a read-only
`M68K::getRegisters()` method.

### 6.3 Disassembler view

Show disassembled instructions around PC. Uses the existing
`src/cpu/disasm.cpp` disassembler.

### 6.4 Device state panels

Show VIA, SCC, IWM register states. Each device's internal state is
already accessible through the Machine's device pointers.

### 6.5 Commit

```sh
git add -A && git commit -m "imgui: add debug windows (memory, registers, disassembly)"
```

---

## Phase 7 — Polish and cleanup

### 7.1 File dialogs

Add native file dialog support for disk insertion. Options:
- **tinyfd** (tiny file dialogs) — single-header, works on macOS/Linux/Windows
- **NFD** (Native File Dialog) — small C library
- ImGui file browser widget (pure ImGui, no native dialog)

### 7.2 Keyboard/mouse focus handling

Ensure that when the emulator viewport has focus:
- Keyboard events go to the emulated Mac
- Mouse clicks inside the viewport are translated to Mac coordinates
- Mouse grab (for games/drawing) still works via the backend

When an ImGui window (menu, debug panel) has focus:
- ImGui handles the events
- The emulator continues running but doesn't receive input

### 7.3 Configuration persistence

Save/restore ImGui layout (`imgui.ini`) and emulator preferences.

### 7.4 Commit

```sh
git add -A && git commit -m "imgui: file dialogs, focus handling, layout persistence"
```

---

## Summary of new files

| Phase | File | Purpose |
|---|---|---|
| 1 | `src/platform/null_sound.cpp` | Sound stubs for headless |
| 2 | `src/platform/headless_backend.h/.cpp` | HeadlessBackend |
| 2 | `src/platform/headless_main.cpp` | Headless entry point |
| 3 | _(delete)_ `src/platform/sdl.cpp` | Remove dead code |
| 4 | `libs/imgui/...` | Vendored Dear ImGui |
| 5 | `src/platform/imgui_backend.h/.cpp` | ImGuiBackend |
| 5 | `src/platform/imgui_main.cpp` | ImGui entry point |
| 5 | `src/platform/sdl_event_translate.h` | Shared SDL→PlatformEvent |

## CMake presets

| Preset | Backend | SDL | ImGui | Output binary |
|---|---|---|---|---|
| `macos` | sdl | yes | no | `bld/macos/maxivmac` |
| `macos-headless` | headless | no | no | `bld/macos-headless/maxivmac` |
| `macos-imgui` | imgui | yes | yes | `bld/macos-imgui/maxivmac` |

## Risks and open questions

1. **GL pixel format** — ARGB8888 from screen_convert may need byte-order
   adjustment for OpenGL. Test on first ImGui render; fix in screen_convert
   if needed (output RGBA instead of ARGB, or use GL format flags).

2. **Mouse coordinate mapping in ImGui viewport** — The emulator viewport
   is an ImGui::Image. Mouse events need to be translated from ImGui
   viewport coordinates to emulator screen coordinates. ImGui provides
   `ImGui::GetItemRectMin()` after `Image()` for this.

3. **Fullscreen in ImGui** — Full-screen mode means the ImGui window goes
   fullscreen, not just the viewport. The menu bar and debug windows
   remain accessible. This is different from SDL fullscreen where the
   emulator fills the screen. Need to decide on UX.

4. **Headless sound linkage** — If any common/ file references
   `Sound_BeginWrite`/`Sound_EndWrite` at link time, the headless build
   may fail. These are only called from device/core code during
   emulation ticks. Verify with a test link.

5. **ImGui version** — Target v1.91.x (latest stable as of 2026). The
   SDL3 backend (`imgui_impl_sdl3.cpp`) was added in v1.91.0.