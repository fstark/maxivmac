# SDL Frontend Simplification Plan

Step-by-step plan to simplify `src/platform/sdl.cpp` (5 106 lines → target
~2 500).  Each phase is independently committable and testable (build + verify).

Reference: [docs/UI.md](UI.md)

---

## Phase 0 — Go SDL3-only

Drop all SDL 1.x, SDL 2.x, and "SDL 0" (no-SDL) code paths.  This is
purely mechanical: resolve every `#if SDL_MAJOR_VERSION` to its SDL3 branch.

### Step 0a — CMakeLists.txt: require SDL3

Replace the "try SDL3, fall back to SDL2" logic with `find_package(SDL3 REQUIRED)`.
Remove `find_package(SDL2 ...)` entirely.

Files: `CMakeLists.txt`.

### Step 0b — sdl_config.h: use SDL3 include path

Change `#include <SDL.h>` to `#include <SDL3/SDL.h>`.  Remove SDL-version
detection `#ifndef SDL_MAJOR_VERSION` block in sdl.cpp (lines 31–41);
SDL3 always defines it.

Files: `src/platform/sdl_config.h`, `src/platform/sdl.cpp`.

### Step 0c — Remove SDL 1.x keyboard table

Delete `SDLKey2MacKeyCode()` (lines 1500–1658) and the SDL1 `DoKeyCode`
variant (lines 1809–1815).  Keep only `SDLScan2MacKeyCode()` + SDL3 `DoKeyCode`.

Files: `src/platform/sdl.cpp` (~160 lines removed).

### Step 0d — Remove SDL 1.x CreateMainWindow and SDL 0 stubs

Delete `#if 0 == SDL_MAJOR_VERSION` block (lines 4005–4042, window stubs)
and `#elif 1 == SDL_MAJOR_VERSION` block (lines 4043–4115, `SDL_SetVideoMode`
path).  Keep only the SDL 2+ `CreateMainWindow` (now SDL3-only).

Remove `SDL_Surface *my_surface`, `#define my_format (my_surface->format)`.

Files: `src/platform/sdl.cpp` (~110 lines removed).

### Step 0e — Flatten all `#if 0 != SDL_MAJOR_VERSION` blocks

These appear ~23 times and always evaluate to true.  Remove the `#if`/`#endif`
wrappers, keeping the body.  This includes:
- `HaveChangedScreenBuff` outer guard (line 935)
- Audio callback (line 2225)
- `Screen_Init` SDL_Init block (line 3872)
- `CheckForSystemEvents` / `WaitForTheNextEvent` (lines 4817, 4835)
- Timer code in `UpdateTrueEmulatedTime`, `IncrNextTime`, `InitNextTime`,
  `StartUpTimeAdjust`, `InitLocationDat`
- Cursor show/hide in `ForceShowCursor`, `CheckForSavedTasks`
- `MySound_Stop/Start/UnInit/Init`
- `SDL_Quit` in `UnInitOSGLU`

Remove `#define HaveWorkingTime 0` for SDL0 (line 1876–1878).
Remove `#if 0 == SDL_MAJOR_VERSION` / `#define HaveWorkingTime 0` (dead).
Hardcode `HaveWorkingTime` to 1 and remove its checks.

Files: `src/platform/sdl.cpp` (~50 lines of scaffolding removed).

### Step 0f — Resolve all `#if SDL_MAJOR_VERSION >= 3` to unconditionally true

These appear ~48 times.  For each:
- Keep the `>= 3` branch body.
- Delete the `#else` / `#elif SDL_MAJOR_VERSION <= 2` branch.
- Delete the `#if` / `#endif` wrapper.

This touches event names (`SDL_EVENT_QUIT` vs `SDL_QUIT`), render calls
(`SDL_RenderTexture` vs `SDL_RenderCopy`), audio API
(`SDL_OpenAudioDeviceStream` vs `SDL_OpenAudio`), window API
(`SDL_SetWindowMouseGrab` vs `SDL_SetWindowGrab`), pixel format API
(`SDL_GetPixelFormatDetails` vs `SDL_AllocFormat`), mouse API
(`SDL_GetMouseState` returning `float`), etc.

Also resolve the 7 `#if SDL_MAJOR_VERSION <= 2` (always false) and
8 `#if SDL_MAJOR_VERSION >= 2` (always true) guards.

Files: `src/platform/sdl.cpp` (~200 lines of branching removed).

### Step 0g — Remove `CanGetAppPath` conditionals

With SDL3, `SDL_GetBasePath()` always exists.  Hardcode `CanGetAppPath` to 1
and remove all `#if CanGetAppPath` guards.

Files: `src/platform/sdl.cpp` (~20 lines of scaffolding removed).

### Step 0h — Remove `WantOSGLUSDL` outer guard

The entire file is wrapped in `#ifdef WantOSGLUSDL ... #endif`.  Since
this is the only platform backend, remove the guard.

Files: `src/platform/sdl.cpp`, possibly `src/platform/sdl_config.h`.

**Verify:** Clean build + `test/verify.sh` pass.

**Estimated removal:** ~550 lines, leaving ~4 550 lines.

---

## Phase 1 — Commit to UseSDLscaling=1

Let SDL3's renderer handle integer scaling instead of software-doubling
every pixel in `screen_map.h` instantiations.

### Step 1a — Set `UseSDLscaling` to 1

Change `#define UseSDLscaling 0` to `#define UseSDLscaling 1`.

Files: `src/platform/sdl.cpp`.

### Step 1b — Delete dead `!UseSDLscaling` blocks

Remove the 12 scaled screen_map instantiations (lines 731–763, 844–927)
and all `#if ! UseSDLscaling` branches inside `HaveChangedScreenBuff`
(5 blocks at lines 1012, 1052, 1094, 1114, 1138, 1155, 1200).

Remove `MaxScale` (was 2, now 1), `WindowScale` multiplication in
`CreateMainWindow` texture size, and software-scaling logic in pixel blit.

Also remove `ScaledCopy` function references from the dispatch tables.

Remove `#define MaxScale` and simplify `CLUT_finalsz` accordingly.

Files: `src/platform/sdl.cpp` (~250 lines removed).

### Step 1c — Remove `UseSDLscaling` define entirely

Now that it's always 1, remove the define and the single remaining
`#if UseSDLscaling` guard (line 4258 in `CreateMainWindow` texture size).

Files: `src/platform/sdl.cpp`.

**Verify:** Clean build + `test/verify.sh` pass.

**Estimated removal:** ~280 lines, leaving ~4 270 lines.

---

## Phase 2 — Extract MacRoman ↔ Unicode tables

The four character-conversion functions total ~750 lines of hand-written
`switch` statements.  They have zero SDL dependency.

### Step 2a — Create `src/platform/common/mac_roman.h` / `.cpp`

Move these functions:
- `MacRoman2UniCodeSize()` (~130 lines)
- `MacRoman2UniCodeData()` (~130 lines)
- `UniCodePoint2MacRoman()` (~130 lines)
- `UniCodeStr2MacRoman()` (~80 lines)
- `UniCodeStrLength()` (~50 lines)
- `NativeStrFromCStr()` (~15 lines)

Declare them in `mac_roman.h`.  Implement in `mac_roman.cpp`.

Files: new `src/platform/common/mac_roman.h`, new `mac_roman.cpp`,
`src/platform/sdl.cpp`, `CMakeLists.txt`.

### Step 2b — Update sdl.cpp to include mac_roman.h

Replace the moved function bodies with `#include "platform/common/mac_roman.h"`.

Files: `src/platform/sdl.cpp`.

**Verify:** Clean build + `test/verify.sh` pass.

**Estimated removal from sdl.cpp:** ~750 lines, leaving ~3 520 lines.

---

## Phase 3 — Extract disk I/O

Pure POSIX file operations with no SDL dependency.

### Step 3a — Create `src/platform/common/disk_io.h` / `.cpp`

Move these functions:
- `InitDrives()`, `UnInitDrives()`
- `vSonyTransfer()`, `vSonyGetSize()`
- `vSonyEject0()`, `vSonyEject()`, `vSonyEjectDelete()`
- `vSonyGetName()`
- `Sony_Insert0()`, `Sony_Insert1()`
- `WriteZero()`, `MakeNewDisk0()`, `MakeNewDisk()`
- `Sony_Insert1a()`, `Sony_Insert2()`, `Sony_InsertIth()`
- `LoadInitialImages()`
- Static arrays: `Drives[]`, `DriveNames[]`

Files: new `disk_io.h`, new `disk_io.cpp`, `sdl.cpp`, `CMakeLists.txt`.

**Verify:** Clean build + `test/verify.sh` pass.

**Estimated removal from sdl.cpp:** ~370 lines, leaving ~3 150 lines.

---

## Phase 4 — Extract ROM loader

### Step 4a — Create `src/platform/common/rom_loader.h` / `.cpp`

Move:
- `LoadMacRomFrom()`
- `LoadMacRomFromPrefDir()`
- `LoadMacRomFromAppPar()`
- `LoadMacRom()`
- `rom_path` static variable

`LoadMacRomFromPrefDir` uses `pref_dir` (from `SDL_GetPrefPath`), so it
takes a `const char*` parameter instead of accessing the static directly.
Same for `LoadMacRomFromAppPar` with `d_arg`/`app_parent`.

Files: new `rom_loader.h`, new `rom_loader.cpp`, `sdl.cpp`, `CMakeLists.txt`.

**Verify:** Clean build + `test/verify.sh` pass.

**Estimated removal from sdl.cpp:** ~90 lines, leaving ~3 060 lines.

---

## Phase 5 — Extract debug log and path utilities

### Step 5a — Create `src/platform/common/dbglog.cpp`

Move `dbglog_open0()`, `dbglog_write0()`, `dbglog_close0()` and the
`dbglog_File` static.  After Phase 0, the `dbglog_ToSDL_Log` path uses
`SDL_Log()` — that one line is the only SDL dependency, acceptable to
keep or pass as a callback.

Files: new `dbglog.cpp`, `sdl.cpp`, `CMakeLists.txt`.

### Step 5b — Create `src/platform/common/path_utils.h` / `.cpp`

Move `ChildPath()`.  Pure C string utility.

Files: new `path_utils.h`, new `path_utils.cpp`, `sdl.cpp`, `CMakeLists.txt`.

**Verify:** Clean build + `test/verify.sh` pass.

**Estimated removal from sdl.cpp:** ~90 lines, leaving ~2 970 lines.

---

## Phase 6 — Extract time / tick management

### Step 6a — Create `src/platform/common/tick_timer.h` / `.cpp`

Move:
- `TrueEmulatedTime`, `OnTrueTime`, `NewMacDateInSeconds`
- Time-step constants (`MyInvTimeDiv`, `MyInvTimeStep`, …)
- `LastTime`, `NextIntTime`, `NextFracTime`
- `IncrNextTime()`, `InitNextTime()`
- `UpdateTrueEmulatedTime()`, `CheckDateTime()`
- `StartUpTimeAdjust()`, `InitLocationDat()`

These use `SDL_GetTicks()` — a single-function dependency.  Either accept
it or pass a `uint32_t (*getTicks)()` function pointer at init.

Files: new `tick_timer.h`, new `tick_timer.cpp`, `sdl.cpp`, `CMakeLists.txt`.

**Verify:** Clean build + `test/verify.sh` pass.

**Estimated removal from sdl.cpp:** ~130 lines, leaving ~2 840 lines.

---

## Phase 7 — Simplify screen_map instantiation boilerplate

After Phase 1 (UseSDLscaling=1), 13 screen_map instantiations remain
(BW + colour at depths 1,2,3 × dest depths 3,4,5, minus the scaled copies).
Each requires 5–6 `#define` lines + 1 `#include`.

### Step 7a — Create a helper macro

```cpp
#define INSTANTIATE_SCREEN_MAP(Name, SrcD, DstD) \
    ...
```

Replace the 13 repetitions with 13 one-line macro calls.

Files: `src/platform/sdl.cpp` (~80 lines removed, replaced by ~15 lines).

**Verify:** Clean build + `test/verify.sh` pass.

**Estimated removal from sdl.cpp:** ~65 lines, leaving ~2 775 lines.

---

## Phase 8 — Extract clipboard

### Step 8a — Create `src/platform/common/clipboard.cpp`

Move `HTCEexport()` and `HTCEimport()`.  These use `mac_roman.h` functions
(extracted in Phase 2) plus `SDL_SetClipboardText` / `SDL_GetClipboardText`.
The two SDL calls are the only dependency — accept it or callback-ify.

Files: new `clipboard.cpp`, `sdl.cpp`, `CMakeLists.txt`.

**Verify:** Clean build + `test/verify.sh` pass.

**Estimated removal from sdl.cpp:** ~50 lines, leaving ~2 725 lines.

---

## Phase 9 — Further simplifications in remaining SDL code

These are optional cleanups after the previous phases.

### Step 9a — Extract sound into `src/platform/sdl_sound.cpp`

Move the entire sound section (~300 lines): ring buffer management,
`my_audio_callback`, `sdl3_audio_callback`, `MySound_Init/Start/Stop/UnInit`,
`MySound_SecondNotify`, and associated constants/statics.

### Step 9b — Extract keyboard into `src/platform/sdl_keyboard.cpp`

Move `SDLScan2MacKeyCode()` (~150 lines), `DoKeyCode()`, key repeat stubs.

### Step 9c — Extract event handling into `src/platform/sdl_events.cpp`

Move `HandleTheEvent()` (~230 lines).  Depends on keyboard + mouse +
window functions.

### Step 9d — Simplify `HaveChangedScreenBuff`

After UseSDLscaling=1, the fast path always applies for 8/16/32-bpp.
Remove the 24-bpp slow path (modern displays never use it).  Simplify
the CLUT loop.

---

## Summary

| Phase | Description | Lines removed | sdl.cpp after |
|-------|-------------|--------------|---------------|
| 0 | SDL3-only | ~550 | ~4 550 |
| 1 | UseSDLscaling=1 | ~280 | ~4 270 |
| 2 | Extract MacRoman tables | ~750 | ~3 520 |
| 3 | Extract disk I/O | ~370 | ~3 150 |
| 4 | Extract ROM loader | ~90 | ~3 060 |
| 5 | Extract dbglog + path utils | ~90 | ~2 970 |
| 6 | Extract tick timer | ~130 | ~2 840 |
| 7 | Simplify screen_map boilerplate | ~65 | ~2 775 |
| 8 | Extract clipboard | ~50 | ~2 725 |
| 9 | Split remaining SDL sections | ~680 | ~2 050 |

After Phase 8, `sdl.cpp` is ~2 700 lines of purely SDL3 code: window
management, rendering, mouse/cursor, keyboard, sound, events, and the
main loop.  Phase 9 can further split it into focused ~300-line files.
