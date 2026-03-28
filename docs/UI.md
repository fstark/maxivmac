# SDL Frontend Analysis

Analysis of `src/platform/sdl.cpp` for simplification and
eventual split into SDL-specific vs. general platform code.

Per [UI_PLAN.md](UI_PLAN.md), **Phase 0 removes all SDL 1.x and SDL 2.x
code paths**, leaving SDL3-only.  The analysis below reflects the
post-Phase-0 state.  Line counts are approximate and will shift as
Phase 0 is applied.

---

## Current structure

The file is one monolithic translation unit (~4 550 lines after Phase 0).
It uses SDL3 exclusively.

### Section map

| Lines | Section | SDL? | Notes |
|-------|---------|------|-------|
| 17–30 | Utilities (`MyMoveBytes`), includes | No | Trivial |
| 31–60 | Path helpers | Mixed | `CanGetAppPath` hardcoded to 1; uses `SDL_GetBasePath` |
| 62–89 | `ChildPath()` — build full path from dir+file | **No** | Pure C string manipulation |
| 91–164 | Debug log (`dbglog_open0/write0/close0`) | Mixed | `dbglog_ToSDL_Log` path calls `SDL_Log` |
| 166–172 | `#include` of common platform headers | — | |
| 174–190 | `NativeStrFromCStr()` — string translation | **No** | Uses intl_chars tables |
| 192–562 | **Disk I/O** (`InitDrives`, `vSonyTransfer`, `vSonyGetSize`, `vSonyEject`, `Sony_Insert*`, `MakeNewDisk`, `LoadInitialImages`) | **No** | Pure POSIX `fopen/fread/fwrite/fclose`. Only `Sony_Insert1a` touches SDL (drag-and-drop). |
| 564–647 | **ROM loading** (`LoadMacRom`, `LoadMacRomFrom`, `LoadMacRomFromPrefDir`, `LoadMacRomFromAppPar`) | Mostly no | `LoadMacRomFromPrefDir` uses `SDL_GetPrefPath` indirectly |
| 648–930 | **Screen map instantiations** — 27× `#include "screen_map.h"` with varying `ScrnMapr_*` defines | **No** | Generates BW and colour copy functions at every (srcDepth, dstDepth, scale) combo. Not SDL-specific at all. |
| 932–1302 | **`HaveChangedScreenBuff()`** — the big pixel-blit function | **Yes** | Core rendering: locks SDL texture, maps CLUT, copies pixels, presents. SDL3 path only. |
| 1305–1322 | `MyDrawChangesAndClear`, `DoneWithDrawingForTick` | Mixed | Calls into SDL rendering |
| 1324–1500 | **Mouse** — cursor hide/show, warp, position notify, `CheckMouseState` | **Yes** | `SDL_ShowCursor`, `SDL_WarpMouse*`, `SDL_GetMouseState` |
| 1500–1870 | **Keyboard** — `SDLScan2MacKeyCode` mapping table | **Yes** | ~210 lines (SDL1 table removed) |
| 1870–1900 | `DoKeyCode`, `DisableKeyRepeat`, `RestoreKeyRepeat`, `ReconnectKeyCodes3`, `DisconnectKeyCodes3` | Mixed | Small wrappers |
| 1900–2050 | **Time / date** — `TrueEmulatedTime`, `UpdateTrueEmulatedTime`, `CheckDateTime`, `StartUpTimeAdjust`, `InitLocationDat` | Mixed | Uses `SDL_GetTicks` / `SDL_Delay` |
| 2050–2500 | **Sound** — ring buffer, `my_audio_callback`, `MySound_Init/Start/Stop/UnInit`, `MySound_SecondNotify` | **Yes** | ~450 lines. Callback, `SDL_OpenAudio` / `SDL_OpenAudioDeviceStream` |
| 2530–2570 | **Dialogs** — `CheckSavedMacMsg` | Minimal | `SDL_ShowSimpleMessageBox` |
| 2570–3640 | **Clipboard** — `MacRoman2UniCode*`, `UniCode*2MacRoman`, `HTCEexport/HTCEimport` | Mixed | ~1 070 lines. Character conversion tables are pure data; clipboard calls use `SDL_SetClipboardText` / `SDL_GetClipboardText`. |
| 3640–3870 | **Event handling** — `HandleTheEvent` | **Yes** | SDL event dispatch (quit, key, mouse, wheel, drop, window focus/resize) |
| 3870–4500 | **Window creation/destruction** — `Screen_Init`, `CreateMainWindow`, `CloseMainWindow`, `ReCreateMainWindow`, grab/ungrab, fullscreen toggle | **Yes** | Single SDL3 `CreateMainWindow`. ~450 lines. |
| 4540–4700 | **Saved tasks** — `CheckForSavedTasks`, background/speed transitions, window recreate, cursor toggle, new-disk creation dispatch | Mixed | Orchestration loop — SDL only for cursor show/hide |
| 4700–4790 | **Command-line parsing** — `ScanCommandLine` | **No** | |
| 4790–4870 | **Main loop** — `WaitForNextTick`, `CheckForSystemEvents`, `WaitForTheNextEvent` | Mixed | `SDL_PollEvent` / `SDL_WaitEvent` / `SDL_Delay` |
| 4870–4930 | `ZapOSGLUVars`, `AllocMyMemory`, `UnallocMyMemory` | **No** | Pure allocation |
| 4930–4960 | `InitWhereAmI`, `UninitWhereAmI` | Yes | `SDL_GetBasePath`, `SDL_GetPrefPath` |
| 4960–5050 | `InitOSGLU`, `UnInitOSGLU` | Mixed | Init/teardown orchestration |
| 5050–5106 | `main()` | Mixed | Entry point |

### Summary by category

| Category | Approx lines | SDL-specific? |
|----------|-------------|---------------|
| Disk I/O | 370 | No |
| ROM loading | 85 | No |
| Screen map instantiations | 280 | No |
| Pixel blit (`HaveChangedScreenBuff`) | 370 | Yes |
| Mouse | 180 | Yes |
| Keyboard mapping | 210 | Yes |
| Time / tick | 150 | Thin SDL wrapper |
| Sound | 450 | Yes |
| Clipboard (char tables + SDL calls) | 1 070 | Mostly no (tables are pure data) |
| Window management | 450 | Yes |
| Event dispatch | 230 | Yes |
| Dialogs / messages | 40 | Minimal |
| Main loop / orchestration | 280 | Mixed |
| Utilities / init / teardown | 200 | No |
| **Total** | **~4 550** | |

---

## Proposed split

### New files (extracted from `sdl.cpp`)

| New file | Content | Lines est. |
|----------|---------|-----------|
| `platform/common/disk_io.cpp` | `InitDrives`, `UnInitDrives`, `vSonyTransfer`, `vSonyGetSize`, `vSonyEject*`, `Sony_Insert0/1/2`, `Sony_InsertIth`, `LoadInitialImages`, `WriteZero`, `MakeNewDisk*` | ~350 |
| `platform/common/rom_loader.cpp` | `LoadMacRomFrom`, `LoadMacRomFromPrefDir`, `LoadMacRomFromAppPar`, `LoadMacRom` | ~90 |
| `platform/common/mac_roman.cpp` | `MacRoman2UniCodeSize`, `MacRoman2UniCodeData`, `UniCodePoint2MacRoman`, `UniCodeStr2MacRoman`, `UniCodeStrLength`, `NativeStrFromCStr` | ~750 |
| `platform/common/clipboard.cpp` | `HTCEexport`, `HTCEimport` (thin wrappers over mac_roman + SDL clipboard) | ~30 |
| `platform/common/dbglog.cpp` | `dbglog_open0`, `dbglog_write0`, `dbglog_close0` | ~65 |
| `platform/common/tick_timer.cpp` | `TrueEmulatedTime`, time-step logic, `IncrNextTime`, `InitNextTime`, `UpdateTrueEmulatedTime`, `CheckDateTime`, `StartUpTimeAdjust`, `InitLocationDat` | ~130 |
| `platform/common/path_utils.cpp` | `ChildPath` | ~30 |

### What stays in `sdl.cpp`

| Section | Reason |
|---------|--------|
| Window creation / destruction | Deeply SDL-specific (SDL_CreateWindow, renderer, texture) |
| `HaveChangedScreenBuff` + screen map instantiations | Texture locking, SDL pixel format, `SDL_RenderCopy` |
| Mouse cursor / warp | `SDL_ShowCursor`, `SDL_WarpMouseInWindow` |
| Keyboard tables + event dispatch | SDL scancodes, `SDL_PollEvent` |
| Sound (audio callback + init) | `SDL_OpenAudio`, audio stream |
| `main()`, `InitOSGLU`, `UnInitOSGLU`, event loop | SDL init/quit, entry point |

Estimated `sdl.cpp` after split: **~2 700 → ~2 000 lines**.

---

## Simplification opportunities

> **Note:** SDL 1.x and 2.x removal is handled by Phase 0 of
> [UI_PLAN.md](UI_PLAN.md) and is reflected in the line counts above.

### 1. MacRoman ↔ UTF-8 tables → lookup arrays

The 256-entry hand-written `switch` statements for MacRoman ↔ Unicode
(~750 lines total) can be replaced with two static arrays:
```cpp
static const uint16_t kMacRomanToUnicode[128] = { 0x00C4, 0x00C5, ... };
static const struct { uint16_t unicode; uint8_t macRoman; }
    kUnicodeToMacRoman[] = { ... };
```
This shrinks ~750 lines to ~40 lines plus compact data.

### 2. Screen map instantiation explosion

27 `#include "screen_map.h"` invocations (280 lines of `#define` boilerplate)
generate copy functions for every combination of:
- src depth: 0(BW), 1, 2, 3
- dst depth: 3, 4, 5
- scale: 1, 2

With `UseSDLscaling=1` (let SDL handle scaling), the scaled variants
disappear entirely, halving this code.  If we commit to `UseSDLscaling=1`
(modern GPUs handle scaling trivially), 14 instantiations can be removed.

### 3. `HaveChangedScreenBuff` refactoring

This 370-line function has two paths:
- **Fast path**: when pitch matches expectations — uses pre-built CLUT lookup
  tables and screen_map copy functions.
- **Slow path**: pixel-by-pixel with `switch(bpp)` for every pixel.

The slow path exists for edge cases (24-bpp, unexpected pitch).
It could be simplified or removed for modern displays that always give
32-bpp ARGB.

---

## Dependency graph

```
main()
 └─ InitOSGLU()
     ├─ AllocMyMemory()           [general]
     ├─ InitWhereAmI()            [SDL]
     ├─ dbglog_open()             [general + SDL log variant]
     ├─ ScanCommandLine()         [general]
     ├─ LoadMacRom()              [general]
     ├─ LoadInitialImages()       [general]
     ├─ InitLocationDat()         [general + SDL_GetTicks]
     ├─ Screen_Init()             [SDL]
     ├─ MySound_Init()            [SDL]
     ├─ CreateMainWindow()        [SDL]
     └─ WaitForRom()              [general]

 └─ ProgramMain()
     └─ WaitForNextTick() loop
         ├─ CheckForSystemEvents()   [SDL: SDL_PollEvent → HandleTheEvent]
         ├─ CheckForSavedTasks()     [mixed]
         ├─ DoneWithDrawingForTick() [SDL texture present]
         ├─ MySound_SecondNotify()   [general + SDL correction]
         └─ CheckMouseState()        [SDL]

 └─ UnInitOSGLU()
     ├─ MySound_UnInit()          [SDL]
     ├─ UnInitDrives()            [general]
     ├─ CloseMainWindow()         [SDL]
     └─ SDL_Quit()                [SDL]
```

---

## Recommended execution order

See [UI_PLAN.md](UI_PLAN.md) for the full execution plan.

1. **Phase 0 — Go SDL3-only** (drop SDL 1.x + 2.x, ~550 lines)
2. **Phase 1 — UseSDLscaling=1** (remove software-scaled screen_map, ~280 lines)
3. **Phases 2–6 — Extract non-SDL code** (mac_roman, disk_io, rom_loader, dbglog, tick_timer)
4. **Phases 7–9 — Simplify and split remaining SDL code**

Each phase is independently committable and testable against golden files.
