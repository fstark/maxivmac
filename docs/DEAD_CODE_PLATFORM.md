# Dead Code Audit — Platform Files

**Generated:** 2026-03-23  
**Scope:** 8 platform backend files in `src/platform/`

---

## Overview

| File | Total Lines | `#if 0` Dead Lines | % Dead | In Build? |
|------|------------|-------------------|--------|-----------|
| `win32.cpp` | 6,299 | 638 | 10% | **NO** |
| `sdl.cpp` | 5,261 | 1,656 | **31%** | **YES** (SDL backend) |
| `x11.cpp` | 5,754 | 172 | 2% | **NO** |
| `gtk.cpp` | 1,766 | 118 | 6% | **NO** |
| `nds.cpp` | 1,384 | 9 | 0% | **NO** |
| `dos.cpp` | 1,541 | 4 | 0% | **NO** |
| `carbon.cpp` | 5,669 | 298 | 5% | **NO** |
| `classic_mac.cpp` | 5,601 | 128 | 2% | **NO** |
| **TOTAL** | **33,275** | **3,023** | **9%** | |

The CMakeLists.txt only compiles two backends: **cocoa** (macOS via `cocoa.mm`) and **sdl** (cross-platform via `sdl.cpp`). The other 7 files are entirely dead — never compiled.

---

## Entire-File Dead Code Assessment

### Files That Are Themselves Dead (never compiled in maxivmac)

| File | Lines | Status | Reason |
|------|-------|--------|--------|
| `win32.cpp` | 6,299 | **DEAD_OBSOLETE** | Native Win32 backend; maxivmac uses SDL on Windows instead. Entire file unused. |
| `x11.cpp` | 5,754 | **DEAD_OBSOLETE** | Native X11 backend; maxivmac uses SDL on Linux instead. Entire file unused. |
| `gtk.cpp` | 1,766 | **DEAD_OBSOLETE** | GTK backend; maxivmac uses SDL instead. Entire file unused. |
| `nds.cpp` | 1,384 | **DEAD_OBSOLETE** | Nintendo DS backend. Dead platform — no NDS cross-compiler in build system. |
| `dos.cpp` | 1,541 | **DEAD_OBSOLETE** | MS-DOS backend. Dead platform — no DOS toolchain in build system. |
| `carbon.cpp` | 5,669 | **DEAD_OBSOLETE** | Mac OS X Carbon API backend. Carbon was deprecated in 10.8, removed in recent macOS. Replaced by cocoa.mm. |
| `classic_mac.cpp` | 5,601 | **DEAD_OBSOLETE** | Classic Mac OS (System 6/7/8/9) backend. Dead platform — no classic Mac compiler in build system. |

**Total dead-file lines: 27,014** (out of 33,275 — 81% of all platform code)

### Files That Are Actively Used

| File | Lines | Status |
|------|-------|--------|
| `sdl.cpp` | 5,261 | **ACTIVE** — SDL backend, used for all non-macOS builds and optionally macOS |

(Note: `cocoa.mm` is the other active backend but was not in the analysis scope.)

---

## Per-File `#if 0` Block Analysis

### 1. `win32.cpp` (6,299 lines — **ENTIRE FILE IS DEAD**)

| Lines | Size | Category | Description |
|-------|------|----------|-------------|
| L805-L811 | 7 | **KEYBOARD_LAYOUT** | Czech keyboard mapping alternative for Windows 7 |
| L1124-L1278 | 155 | **KEYBOARD_LAYOUT** | Latvian + many complex keyboard layouts ("too complicated, don't bother with") |
| L1279-L1584 | 306 | **KEYBOARD_LAYOUT** | Azeri Cyrillic, Belarusian, Bengali, Bosnian Cyrillic, Bulgarian, many CJK layouts — purely empty stubs and partial mappings |
| L2883-L2887 | 5 | **ALTERNATIVE_IMPL** | Old `InsertMenu` call replaced by `MENUITEMINFO` approach |
| L3108-L3110 | 3 | **PLATFORM_COMPAT** | `vMacScreenDepth` conditional — color mode init |
| L3365-L3374 | 10 | **ALTERNATIVE_IMPL** | Testing code for screen buffer PatBlt |
| L3375-L3495 | 121 | **PLATFORM_COMPAT** | `vMacScreenDepth` color mode drawing code |
| L4132-L4153 | 22 | **ALTERNATIVE_IMPL** | Alternative entropy source via `sysinfo()` |
| L5275-L5277 | 3 | **ALTERNATIVE_IMPL** | `GetShortPathName` call — short path variant |
| L5335-L5340 | 6 | **FUTURE_FEATURE** | Disabled `-l` command-line argument parsing |

**Summary:** 638 `#if 0` lines. The largest blocks (461 lines) are **KEYBOARD_LAYOUT** — disabled non-Latin keyboard mappings that were never fully implemented. The entire file is dead since win32.cpp is never compiled.

---

### 2. `sdl.cpp` (5,261 lines — **ACTIVE, heavily conditional**)

| Lines | Size | Category | Description |
|-------|------|----------|-------------|
| L813-L1237 | 425 | **PLATFORM_COMPAT** | `SDL_MAJOR_VERSION != 0` — entire `HaveChangedScreenBuff` for SDL1/2/3 (the `== 0` path = headless/no-SDL) |
| L1270-L1276 | 7 | **PLATFORM_COMPAT** | SDL cursor show/hide for non-zero SDL version |
| L1459-L1473 | 15 | **PLATFORM_COMPAT** | Mouse grab/warp for non-zero SDL version |
| L1853-L1857 | 5 | **PLATFORM_COMPAT** | `SDL_MAJOR_VERSION == 0` timing stub |
| L1859-L1871 | 13 | **PLATFORM_COMPAT** | `SDL_MAJOR_VERSION != 0` timer start |
| L1875-L1879 | 5 | **PLATFORM_COMPAT** | `SDL_MAJOR_VERSION != 0` frac time update |
| L1884-L1888 | 5 | **PLATFORM_COMPAT** | `SDL_MAJOR_VERSION != 0` int time update |
| L1901-L1942 | 42 | **PLATFORM_COMPAT** | `SDL_MAJOR_VERSION != 0` timer check logic |
| L1979-L1981 | 3 | **PLATFORM_COMPAT** | `SDL_MAJOR_VERSION != 0` timer init |
| L1991-L1996 | 6 | **PLATFORM_COMPAT** | `SDL_MAJOR_VERSION != 0` timer start |
| L2230-L2349 | 120 | **PLATFORM_COMPAT** | `SDL_MAJOR_VERSION != 0` audio callback, init, cleanup |
| L2389-L2391 | 3 | **PLATFORM_COMPAT** | `SDL_MAJOR_VERSION != 0` delay |
| L2396-L2404 | 9 | **PLATFORM_COMPAT** | `SDL_MAJOR_VERSION != 0` SDL audio device pause |
| L2420-L2428 | 9 | **PLATFORM_COMPAT** | `SDL_MAJOR_VERSION != 0` SDL audio device resume |
| L2435-L2441 | 7 | **PLATFORM_COMPAT** | `SDL_MAJOR_VERSION != 0` audio stream destroy |
| L2453-L2455 | 3 | **PLATFORM_COMPAT** | `SDL_MAJOR_VERSION != 0` audio spec |
| L2465-L2518 | 54 | **PLATFORM_COMPAT** | `SDL_MAJOR_VERSION != 0` audio open/init |
| L3668-L3910 | 243 | **PLATFORM_COMPAT** | `SDL_MAJOR_VERSION != 0` event handler (SDL1/2) |
| L3927-L3945 | 19 | **PLATFORM_COMPAT** | `SDL_MAJOR_VERSION != 0` check for quit event |
| L4066-L4699 | 634 | **PLATFORM_COMPAT** | `SDL_MAJOR_VERSION == 0` full main window/screen code (headless mode) |
| L4899-L4906 | 8 | **PLATFORM_COMPAT** | `SDL_MAJOR_VERSION != 0` cursor hide/show |
| L4972-L4978 | 7 | **PLATFORM_COMPAT** | `SDL_MAJOR_VERSION != 0` event pumping |
| L4990-L4997 | 8 | **PLATFORM_COMPAT** | `SDL_MAJOR_VERSION != 0` event flush |
| L5041-L5043 | 3 | **PLATFORM_COMPAT** | `SDL_MAJOR_VERSION != 0` SDL delay |
| L5242-L5244 | 3 | **PLATFORM_COMPAT** | `SDL_MAJOR_VERSION != 0` SDL quit |

**Summary:** 1,656 `#if 0` lines. **However, nearly all are `#if 0 != SDL_MAJOR_VERSION` or `#if 0 == SDL_MAJOR_VERSION` — these are NOT truly dead code.** They are compile-time conditionals: when SDL is present (`SDL_MAJOR_VERSION >= 2`), the `!= 0` paths compile normally. The `== 0` paths (634 lines) provide a headless/no-SDL fallback. This is working SDL version-dispatch code, not disabled code.

**Truly dead `#if 0` in sdl.cpp: ~0 lines** — all blocks use the `0 != MACRO` pattern which is a comparison, not disabling.

---

### 3. `x11.cpp` (5,754 lines — **ENTIRE FILE IS DEAD**)

| Lines | Size | Category | Description |
|-------|------|----------|-------------|
| L915-L918 | 4 | **DEAD_OBSOLETE** | Disabled `#include <pwd.h>` / `<unistd.h>` |
| L924-L926 | 3 | **ALTERNATIVE_IMPL** | `struct passwd *user` variable |
| L932-L950 | 19 | **ALTERNATIVE_IMPL** | `getpwuid()` fallback for `$HOME` — disabled per man page advice |
| L1474-L1487 | 14 | **PLATFORM_COMPAT** | `vMacScreenDepth` color mode drawing block |
| L1524-L1546 | 23 | **PLATFORM_COMPAT** | `vMacScreenDepth` color mode magnified drawing |
| L3084-L3087 | 4 | **FUTURE_FEATURE** | `AutoTimeZone` — `tm_gmtoff` portability concern |
| L3784-L3805 | 22 | **ALTERNATIVE_IMPL** | Alternative entropy via `sysinfo()` |
| L3995-L3997 | 3 | **DEAD_OBSOLETE** | Debug event logging (`MyDbgEvents`) |
| L4274-L4278 | 5 | **DEAD_OBSOLETE** | Debug fprintf for bitmap_bit_order |
| L4294-L4315 | 22 | **DEAD_OBSOLETE** | Debug fprintf for DefaultDepth, caps |
| L4348-L4350 | 3 | **PLATFORM_COMPAT** | `vMacScreenDepth` color mode works flag |
| L4640-L4642 | 3 | **ALTERNATIVE_IMPL** | Disabled `XSync` call |
| L4644-L4664 | 21 | **ALTERNATIVE_IMPL** | Disabled RandR extension check |
| L5045-L5057 | 13 | **ALTERNATIVE_IMPL** | Disabled `XMoveWindow` (broken in Red Hat 6) |
| L5140-L5146 | 7 | **FUTURE_FEATURE** | Disabled `ToggleWantFullScreen` path |
| L5287-L5292 | 6 | **FUTURE_FEATURE** | Disabled `-l` argument parsing for `SpeedValue` |

**Summary:** 172 `#if 0` lines. Mixture of obsolete debug code, alternative implementations tried and abandoned, and portability workarounds. Entire file is dead — never compiled.

---

### 4. `gtk.cpp` (1,766 lines — **ENTIRE FILE IS DEAD**)

| Lines | Size | Category | Description |
|-------|------|----------|-------------|
| L447-L457 | 11 | **ALTERNATIVE_IMPL** | Alternative mouse coordinate extraction |
| L492-L494 | 3 | **DEAD_OBSOLETE** | Debug fprintf for keyboard events |
| L802-L807 | 6 | **ALTERNATIVE_IMPL** | Alternative GDate-based time implementation |
| L812-L815 | 4 | **FUTURE_FEATURE** | `AutoTimeZone` via `tm_gmtoff` |
| L942-L944 | 3 | **DEAD_OBSOLETE** | Debug fprintf for Expose events |
| L1043-L1049 | 7 | **ALTERNATIVE_IMPL** | Cmd+Opt reconnect handling |
| L1078-L1080 | 3 | **DEAD_OBSOLETE** | Disabled DisableKeyRepeat |
| L1085-L1087 | 3 | **DEAD_OBSOLETE** | Disabled RestoreKeyRepeat |
| L1222-L1231 | 10 | **FUTURE_FEATURE** | Disabled `--display` argument parsing |
| L1240-L1244 | 5 | **FUTURE_FEATURE** | Disabled `-l` argument parsing |
| L1275-L1277 | 3 | **DEAD_OBSOLETE** | Disabled `XFlush` call |
| L1330-L1337 | 8 | **ALTERNATIVE_IMPL** | Alternative GTK main loop iteration |
| L1369-L1373 | 5 | **DEAD_OBSOLETE** | Debug fprintf for key_press_event |
| L1374-L1389 | 16 | **ALTERNATIVE_IMPL** | Alternative key event handling |
| L1399-L1403 | 5 | **DEAD_OBSOLETE** | Debug fprintf for key_release_event |
| L1507-L1515 | 9 | **ALTERNATIVE_IMPL** | Alternative widget creation |
| L1608-L1610 | 3 | **DEAD_OBSOLETE** | Unused button variable |
| L1721-L1734 | 14 | **FUTURE_FEATURE** | Commented-out quit button in UI |

**Summary:** 118 `#if 0` lines. Mostly debug printfs, abandoned alternatives, and a few future features. Entire file is dead — never compiled.

---

### 5. `nds.cpp` (1,384 lines — **ENTIRE FILE IS DEAD**)

| Lines | Size | Category | Description |
|-------|------|----------|-------------|
| L515-L519 | 5 | **FUTURE_FEATURE** | `EnableFSMouseMotion` — full-screen mouse motion |
| L763-L766 | 4 | **FUTURE_FEATURE** | `AutoTimeZone` via `tm_gmtoff` |

**Summary:** 9 `#if 0` lines. Very clean file code-wise, but the **entire 1,384-line file is dead** — targets Nintendo DS, which is not a supported platform for maxivmac.

---

### 6. `dos.cpp` (1,541 lines — **ENTIRE FILE IS DEAD**)

| Lines | Size | Category | Description |
|-------|------|----------|-------------|
| L1125-L1128 | 4 | **FUTURE_FEATURE** | `AutoTimeZone` via `tm_gmtoff` |

**Summary:** 4 `#if 0` lines. Very clean, but the **entire 1,541-line file is dead** — targets MS-DOS (DJGPP toolchain), which is not a supported platform for maxivmac.

---

### 7. `carbon.cpp` (5,669 lines — **ENTIRE FILE IS DEAD**)

| Lines | Size | Category | Description |
|-------|------|----------|-------------|
| L383-L405 | 23 | **ALTERNATIVE_IMPL** | `CGDisplayMoveCursorToPoint` dynamic lookup — replaced by different approach |
| L1408-L1437 | 30 | **PLATFORM_COMPAT** | `vMacScreenDepth` color mode OpenGL drawing |
| L1525-L1534 | 10 | **ALTERNATIVE_IMPL** | Old `glBitmap` screen draw approach |
| L1538-L1547 | 10 | **PLATFORM_COMPAT** | `vMacScreenDepth` color drawing variant |
| L1558-L1572 | 15 | **ALTERNATIVE_IMPL** | "Quick and dirty check" of drawing bounds via `glDrawPixels` in red |
| L1838-L1843 | 6 | **DEAD_OBSOLETE** | `MyShowCursorProc` per-display — replaced by single-display call |
| L1847-L1850 | 4 | **DEAD_OBSOLETE** | Deprecated `ShowCursor()` call path |
| L1857-L1862 | 6 | **DEAD_OBSOLETE** | `MyHideCursorProc` per-display |
| L1866-L1869 | 4 | **DEAD_OBSOLETE** | Deprecated `HideCursor()` call path |
| L1947-L1958 | 12 | **ALTERNATIVE_IMPL** | Alternative cursor positioning via `CGDisplayMoveCursorToPoint` |
| L2023-L2046 | 24 | **ALTERNATIVE_IMPL** | Alternative `InitMousePosition` implementation |
| L2411-L2422 | 12 | **PLATFORM_COMPAT** | Full-screen grab handling logic |
| L2676-L2678 | 3 | **ALTERNATIVE_IMPL** | `GrabbedPort` variable for screen grab experiments |
| L2680-L2702 | 23 | **ALTERNATIVE_IMPL** | `AdjustMainScreenGrab` using `CGCaptureAllDisplays` |
| L2704-L2736 | 33 | **ALTERNATIVE_IMPL** | `CGReleaseAllDisplays` dynamic lookup |
| L2875-L2877 | 3 | **DEAD_OBSOLETE** | Disabled call to `AdjustMainScreenGrab()` |
| L3345-L3360 | 16 | **ALTERNATIVE_IMPL** | Nav Services `kNavCBUserAction` handler |
| L4032-L4035 | 4 | **ALTERNATIVE_IMPL** | Mouse event handling ("CheckMouseState will take care of it") |
| L4328-L4330 | 3 | **PLATFORM_COMPAT** | `vMacScreenDepth` color mode works flag |
| L4729-L4746 | 18 | **ALTERNATIVE_IMPL** | Cursor visibility check via `CGCursorIsVisible` |
| L5046-L5049 | 4 | **ALTERNATIVE_IMPL** | Duplicate mouse event handling (same as L4032) |
| L5228-L5259 | 32 | **ALTERNATIVE_IMPL** | Display reconfiguration callback |
| L5414-L5416 | 3 | **PLATFORM_COMPAT** | `vMacScreenDepth` pixel size multiplier |

**Summary:** 298 `#if 0` lines. Heavy on alternative implementations (Carbon was iteratively developed with many API approach tryouts). Entire file is dead — Carbon API was deprecated in macOS 10.8 (2012) and is functionally removed. Replaced by `cocoa.mm`.

---

### 8. `classic_mac.cpp` (5,601 lines — **ENTIRE FILE IS DEAD**)

| Lines | Size | Category | Description |
|-------|------|----------|-------------|
| L471-L473 | 3 | **ALTERNATIVE_IMPL** | Alternative `FSClose` return |
| L1127-L1208 | 82 | **ALTERNATIVE_IMPL** | "Experimental code in progress" — direct pixel buffer manipulation for full-screen mode |
| L2068-L2079 | 12 | **DEAD_OBSOLETE** | Debug fprintf for sound sample data |
| L2295-L2297 | 3 | **DEAD_OBSOLETE** | Disabled `AdjustMainScreenGrab()` call |
| L2450-L2460 | 11 | **PLATFORM_COMPAT** | Control Strip hiding (System 7.5 feature) |
| L2492-L2501 | 10 | **PLATFORM_COMPAT** | Control Strip showing |
| L5190-L5193 | 4 | **PLATFORM_COMPAT** | Disabled `EventAvail` check ("causes crashes on some machines") |
| L5209-L5211 | 3 | **DEAD_OBSOLETE** | Unused closing brace |

**Summary:** 128 `#if 0` lines. Includes an 82-line experimental pixel-blitting block. The entire file is dead — targets Classic Mac OS (System 6-9), which hasn't been a viable platform since ~2002.

---

## Category Totals (across `#if 0` blocks only)

| Category | Blocks | ~Lines |
|----------|--------|--------|
| **KEYBOARD_LAYOUT** | 3 | ~461 |
| **PLATFORM_COMPAT** | ~25 | ~960 |
| **ALTERNATIVE_IMPL** | ~30 | ~520 |
| **DEAD_OBSOLETE** | ~18 | ~85 |
| **FUTURE_FEATURE** | ~10 | ~50 |

Note: sdl.cpp's `#if 0 != SDL_MAJOR_VERSION` blocks (~1,656 lines) are **not dead code** — they are compile-time version dispatch and compile normally when SDL is present.

---

## Recommendations

### High Priority — Remove entire dead files (~27,000 lines)

These 7 files are never compiled and serve no purpose in the modern maxivmac project:

| File | Lines | Justification |
|------|-------|---------------|
| `classic_mac.cpp` | 5,601 | Classic Mac OS — dead since 2002 |
| `nds.cpp` | 1,384 | Nintendo DS — novelty port, no build support |
| `dos.cpp` | 1,541 | MS-DOS — dead platform, no build support |
| `carbon.cpp` | 5,669 | Carbon API — deprecated 2012, removed from macOS |
| `win32.cpp` | 6,299 | Native Win32 — replaced by SDL backend |
| `x11.cpp` | 5,754 | Native X11 — replaced by SDL backend |
| `gtk.cpp` | 1,766 | GTK — replaced by SDL backend |

**If historical preservation is desired**, these could be moved to a `src/platform/legacy/` directory or tagged in git and deleted from HEAD.

### Medium Priority — Clean sdl.cpp

The `sdl.cpp` `#if 0 == SDL_MAJOR_VERSION` block (L4066-L4699, 634 lines) provides a headless/no-SDL fallback. If headless mode is not needed, this could be removed. All `#if 0 != SDL_MAJOR_VERSION` blocks are live code when SDL is present and should be left alone.

### Low Priority — If keeping legacy files for reference

If any legacy platform files are kept, the `#if 0` blocks within them could be cleaned up, but this is moot if the files themselves are removed.

---
---

# Dead Code Audit — Common Platform, Lang, Unused, Resources & Config

**Generated:** 2026-03-23  
**Scope:**
- `src/platform/common/` (15 files)
- `src/lang/` (11 files)
- `src/unused/` (4 files)
- `src/resources/` (10 files)
- Config: `cfg/CNFUIOSG.h`, `src/config/CNFUIALL.h`, `src/config/CNFUIPIC.h`

---

## Key Config Values Affecting Dead Code

These defines in `src/config/CNFUDOSG.h` and `src/config/CNFUDALL.h` cause large sections to be always-false:

| Define | Value | Effect |
|--------|-------|--------|
| `UseActvCode` | **0** | Activation code system (commercial licensing) — all ~500 lines dead |
| `EnableDemoMsg` | **0** | Demo/trial watermark overlay — all ~120 lines dead |
| `EmLocalTalk` | **0** | LocalTalk networking emulation — all ~60 lines dead in common/ |
| `cIncludeUnused` | **0** | Unused code inclusion master switch — off |
| `UseActvFile` | **0** | Derived: `UseActvCode && 0` — always 0 (activation file save/load) |

---

## src/platform/common/ — Per-File Analysis

### 1. `control_mode.cpp` (1,392 lines) — **4 `#if 0` blocks, multiple always-false conditionals**

#### Explicit `#if 0` blocks:

| Lines | Size | Condition | Description | Category |
|-------|------|-----------|-------------|----------|
| L496 | 2 | `#if 0 && (UseActvCode \|\| EnableDemoMsg)` | `kCntrlMsgRegStrCopied` enum value for "registration string copied" message | **SAFELY_REMOVABLE** |
| L597–L607 | 11 | `#if 0` | `CopyRegistrationStr()` — function to copy registration/variation string to clipboard | **SAFELY_REMOVABLE** |
| L680–L685 | 6 | `#if 0 && (UseActvCode \|\| EnableDemoMsg)` | Key handler `MKC_P` → `CopyRegistrationStr()` in control mode | **SAFELY_REMOVABLE** |
| L956–L966 | 11 | `#if 0` | `kCntrlMsgRegStrCopied` display handler — "Registration String copied." / "Variation name copied." | **SAFELY_REMOVABLE** |

#### Always-false conditionals (due to config values):

| Lines | Condition | Description | Category |
|-------|-----------|-------------|----------|
| L1023–L1050 | `#if EnableDemoMsg` (=0) | `DrawDemoMode()` — bouncing "Demo" watermark overlay; `DemoModeSecondNotify()` | **SAFELY_REMOVABLE** |
| L1052 | `#if UseActvCode` (=0) | `#include "platform/common/actv_code.h"` — pulls in entire 374-line activation system | **SAFELY_REMOVABLE** |
| L1069 | `#if UseActvCode` (=0) | `DrawActvCodeMode()` dispatcher case | **SAFELY_REMOVABLE** |
| L1079–L1082 | `#if EnableDemoMsg` (=0) | `DrawDemoMode()` dispatcher case | **SAFELY_REMOVABLE** |
| L1231–L1243 | `#if EnableAltKeysMode \|\| EnableDemoMsg` | Special mode mask including demo mode bit | **SAFELY_REMOVABLE** (demo part) |
| L1257–L1261 | `#if UseActvCode` (=0) | `SpclModeActvCode` key handler dispatch | **SAFELY_REMOVABLE** |

**Total dead in control_mode.cpp: ~30 lines `#if 0`, ~70 lines always-false config = ~100 lines**

---

### 2. `actv_code.h` (374 lines) — **ENTIRE FILE IS DEAD**

Only included via `#if UseActvCode` at control_mode.cpp L1052. Since `UseActvCode=0`, this file is never compiled.

| Lines | Description | Category |
|-------|-------------|----------|
| L1–374 | Commercial activation code system: RSA-like key verification (`KeyFun0/1/2`, `CheckActvCode`), digit-entry UI (`DoActvCodeModeKey`), file save/load (`ActvCodeFileSave/Load`), display (`DrawCellsActvCodeModeBody`, `DrawActvCodeMode`) | **SAFELY_REMOVABLE** |

Additionally, `UseActvFile` is defined in `intl_chars.h` L257 as `#define UseActvFile 0` (derived from `UseActvCode && 0`), making all `#if UseActvFile` blocks within actv_code.h doubly dead.

**This was a commercial licensing feature from the original Mini vMac. It has no value in an open-source modernization.**

---

### 3. `osglu_common.cpp` (~1,275 lines) — **1 `#if 0` block**

| Lines | Size | Condition | Description | Category |
|-------|------|-----------|-------------|----------|
| L1238–L1240 | 3 | `#if 0` | `LT_NodeHint = 1;` — forces node hint to 1 for testing LocalTalk collision handling. Inside `#if EmLocalTalk` (=0) so doubly dead. | **DEBUG_LOGGING** |

The surrounding `#if EmLocalTalk` block (L1228–L1272) containing `LT_PickStampNodeHint()` and `EntropyPoolAddPtr()` is also dead since `EmLocalTalk=0`.

---

### 4. `osglu_common.h` (~200 lines) — **Always-false conditionals**

| Lines | Condition | Description | Category |
|-------|-----------|-------------|----------|
| L142–L145 | `#if EmLocalTalk` (=0) | `extern` declarations for `e_p[2]`, `LT_MyStamp` | **NOT_YET_ENABLED** |
| L184–L191 | `#if dbglog_HAVE` | Debug logging function declarations and macros (`dbglog_ReserveAlloc`, `dbglog_close`, `dbglog_open`, `dbglog_write`) | **DEBUG_LOGGING** |
| L195–L198 | `#if EmLocalTalk` (=0) | `EntropyPoolAddPtr()`, `LT_PickStampNodeHint()` declarations | **NOT_YET_ENABLED** |

**Note:** `dbglog_HAVE` is controlled by build configuration and IS active in debug builds, so those blocks are NOT dead — they are **DEBUG_LOGGING** (conditionally compiled).

---

### 5. `screen_translate.h` (162 lines) — **1 `#if 0` block**

| Lines | Size | Condition | Description | Category |
|-------|------|-----------|-------------|----------|
| L109–L116 | 8 | `#if 0` | Alternative bit-shifting for 16-bit (5-5-5) to 32-bit ARGB pixel conversion. Different byte ordering from the active code above it. | **ALTERNATIVE_IMPL** |

This is an alternative pixel format mapping that was tried and abandoned. The active code at L100–L108 handles the same conversion.

---

### 6. `screen_map.h` (169 lines) — **NO dead code**

The `#if 0 == (ScrnMapr_MapElSz & 3)` at L63 is **NOT dead code** — it's a compile-time conditional that evaluates based on the template parameter `ScrnMapr_MapElSz`. It selects `uint32_t`, `uint16_t`, or `uint8_t` for the transfer type. This is a working template pattern.

---

### 7. `intl_chars.h` (358 lines) — **Always-false conditionals**

| Lines | Condition | Description | Category |
|-------|-----------|-------------|----------|
| L238–L247 | `#if EnableDemoMsg` (=0) | `kCellDemo0`–`kCellDemo7` enum values for demo watermark cells | **SAFELY_REMOVABLE** |
| L256–L258 | `#if UseActvCode && 0` | `#define UseActvFile 1` / else `#define UseActvFile 0` — activation file feature, always 0 | **SAFELY_REMOVABLE** |

---

### 8. `intl_chars.cpp` (1,978 lines) — **Always-false conditionals**

| Lines | Condition | Description | Category |
|-------|-----------|-------------|----------|
| L648–L674 | `#if EnableDemoMsg` (=0) | Pixel data for `kCellDemo0`–`kCellDemo7` — 8×16 bitmap glyphs forming the "Demo" watermark border/text | **SAFELY_REMOVABLE** |

---

### 9. `control_mode.h` (110 lines) — **Conditional debug code**

| Lines | Condition | Description | Category |
|-------|-----------|-------------|----------|
| L93–L95 | `#if dbglog_HAVE` | `MacMsgDebugAlert(char *s)` declaration | **DEBUG_LOGGING** |

This is live when debug logging is enabled. Not dead.

---

### 10. Remaining files in `src/platform/common/` — **NO dead code**

| File | Lines | Status |
|------|-------|--------|
| `osglu_ud.h` | — | Clean — no `#if 0`, no always-false conditions |
| `osglu_ui.h` | — | Clean |
| `alt_keys.h` | — | Clean |
| `date_to_sec.h` | — | Clean |
| `param_buffers.cpp` | — | Clean |
| `param_buffers.h` | — | Clean |

---

## src/platform/common/ Summary

| Category | Files | ~Lines |
|----------|-------|--------|
| **SAFELY_REMOVABLE** | control_mode.cpp, actv_code.h, intl_chars.h, intl_chars.cpp | ~510 (incl. 374-line actv_code.h) |
| **ALTERNATIVE_IMPL** | screen_translate.h | ~8 |
| **DEBUG_LOGGING** | osglu_common.cpp, osglu_common.h, control_mode.h | ~15 (live in debug builds) |
| **NOT_YET_ENABLED** | osglu_common.h, osglu_common.cpp (EmLocalTalk blocks) | ~30 |

---

## src/lang/ — All 11 Files

| File | Status |
|------|--------|
| `strings_english.h` | Clean — no dead code |
| `strings_french.h` | Clean |
| `strings_german.h` | Clean |
| `strings_italian.h` | Clean |
| `strings_spanish.h` | Clean |
| `strings_portuguese.h` | Clean |
| `strings_dutch.h` | Clean |
| `strings_catalan.h` | Clean |
| `strings_czech.h` | Clean |
| `strings_polish.h` | Clean |
| `strings_serbian.h` | Clean |

**No `#if 0`, no always-false conditions, no dead code.** These are pure string constant files selected at build time by language configuration. All 11 are valid and any one may be compiled depending on locale settings.

---

## src/unused/ — 4 Files (All UNUSED_MODULE)

These files reside in `src/unused/` and are **never compiled** by the build system. They are not `#include`d anywhere in active code.

### 1. `LTOVRBPF.h` (383 lines) — LocalTalk Over Berkeley Packet Filter

**Purpose:** Implements LocalTalk networking via macOS/BSD BPF (raw Ethernet frames). Filters for EtherType 0x809B (AppleTalk). Uses BPF device `/dev/bpfN` for packet capture/injection.

| Lines | Size | Condition | Description | Category |
|-------|------|-----------|-------------|----------|
| L173–L175 | 3 | `#if 0` | Unused `addrlen` variable in routing socket parser | **SAFELY_REMOVABLE** |
| L321–L323 | 3 | `#if 0` | `dbglog_writeln("SCC founds packets from BPF")` — disabled debug log | **DEBUG_LOGGING** |
| L338–L341 | 4 | `#if 0` | `dbglog_writeln("SCC finished set of packets from BPF")` — disabled debug log | **DEBUG_LOGGING** |
| L381–L383 | 3 | `#if ! LT_MayHaveEcho` | Echo prevention error — `#error` if echo not handled | Compile-time guard |

**Future value: MEDIUM-HIGH.** BPF is the correct approach for real LocalTalk emulation on macOS. If LocalTalk support is ever enabled (`EmLocalTalk=1`), this file would be needed for the macOS backend. Would need updating for modern macOS security (SIP restrictions on `/dev/bpf`).

**Category: UNUSED_MODULE**

---

### 2. `LTOVRUDP.h` (457 lines) — LocalTalk Over UDP

**Purpose:** Implements LocalTalk networking via UDP multicast (group `239.192.76.84`, port 1954). Cross-platform: supports both POSIX sockets and Winsock. Embeds process ID in packets for loopback detection.

| Lines | Condition | Description | Category |
|-------|-----------|-------------|----------|
| L22–L24 | `#if ! LT_MayHaveEcho` | Conditional `#include <ifaddrs.h>` for IP-based echo detection | Conditional |
| L141–L143 | `#if ! use_winsock` | POSIX errno clearing | Conditional |
| L312–L370 | `#if ! LT_MayHaveEcho` | `ipInPacketIsMine()` — iterates network interfaces to detect self-sent packets | Conditional |
| L371–L387 | `#if ! LT_MayHaveEcho` | `packetIsOneISent()` — combined PID + IP loopback check | Conditional |
| L390–L428 | `#if ! use_winsock` / `#if use_winsock` | Platform-specific socket error handling | Conditional |
| L431–L443 | `#if ! LT_MayHaveEcho` | Echo filtering in `LT_ReceivePacket()` | Conditional |

No `#if 0` blocks. The `#if` conditions are all runtime-config-based (not always-false).

**Future value: HIGH.** UDP multicast is the most portable way to implement networked LocalTalk between multiple emulator instances. This would enable AppleTalk networking between maxivmac instances on any platform. Well-designed with both Winsock and POSIX support.

**Category: UNUSED_MODULE**

---

### 3. `SGLUALSA.h` (1,619 lines) — Sound GLUe for ALSA

**Purpose:** Full ALSA sound output implementation for Linux. Supports both static linking (`#include "alsa/asoundlib.h"`) and dynamic loading via `dlopen("libasound.so.2")`. The dynamic path is the active code path within the file.

| Lines | Size | Condition | Description | Category |
|-------|------|-----------|-------------|----------|
| L27–L235 | 209 | `#if 0` | Static ALSA linking path — `#define My_xxx snd_xxx` mappings. The `#else` at L236 provides dynamic `dlopen` loading (the preferred approach). | **ALTERNATIVE_IMPL** |
| L1485–L1499 | 15 | `#if 0` | Disabled warnings for buffer_size / period_size mismatches during hw_params setup | **DEBUG_LOGGING** |
| L1526–L1534 | 9 | `#if 0` | Disabled `snd_pcm_sw_params_set_avail_min()` call — commented as potentially needed for older ALSA versions | **ALTERNATIVE_IMPL** |

**Future value: HIGH.** This is the standard Linux sound backend. If maxivmac targets Linux (which it does via SDL), having a native ALSA backend as an alternative to SDL audio could be valuable. The dynamic-loading approach (lines 236–1619) is well-implemented and avoids hard ALSA dependency.

**Category: UNUSED_MODULE**

---

### 4. `SGLUDDSP.h` (229 lines) — Sound GLUe for /dev/dsp (OSS)

**Purpose:** Sound output via the legacy OSS (Open Sound System) `/dev/dsp` interface. Uses `ioctl()` with `SNDCTL_DSP_*` commands. Simple, synchronous write-based audio.

No `#if 0` blocks within the file. Clean implementation.

**Future value: LOW.** OSS is deprecated on modern Linux (replaced by ALSA, then PulseAudio/PipeWire). FreeBSD still supports OSS natively, so this has marginal value for BSD platforms. For Linux, SGLUALSA.h is the correct choice.

**Category: UNUSED_MODULE**

---

## src/unused/ Summary

| File | Lines | Internal Dead | Future Value | Category |
|------|-------|---------------|--------------|----------|
| `LTOVRBPF.h` | 383 | 10 lines (debug logs + unused var) | **MEDIUM-HIGH** (macOS LocalTalk) | UNUSED_MODULE |
| `LTOVRUDP.h` | 457 | 0 lines | **HIGH** (cross-platform LocalTalk) | UNUSED_MODULE |
| `SGLUALSA.h` | 1,619 | 233 lines (static linking alt + debug) | **HIGH** (Linux native sound) | UNUSED_MODULE |
| `SGLUDDSP.h` | 229 | 0 lines | **LOW** (OSS deprecated) | UNUSED_MODULE |
| **Total** | **2,688** | **243** | | |

---

## src/resources/ — 10 Files

| File | Type | Status |
|------|------|--------|
| `ICONAPPO.icns` | macOS app icon | Binary resource — no code |
| `ICONAPPW.ico` | Windows app icon | Binary resource — no code |
| `ICONAPPM.r` | Classic Mac resource fork | Resource definition — no dead code |
| `ICONDSKO.icns` | macOS disk icon | Binary resource |
| `ICONDSKW.ico` | Windows disk icon | Binary resource |
| `ICONDSKM.r` | Classic Mac disk icon resource | Resource definition |
| `ICONROMO.icns` | macOS ROM icon | Binary resource |
| `ICONROMW.ico` | Windows ROM icon | Binary resource |
| `ICONROMM.r` | Classic Mac ROM icon resource | Resource definition |
| `main.r` | Classic Mac main resource | Resource definition |

**No dead code.** These are all binary/resource files with no preprocessor conditionals.

---

## Config Files

### 1. `cfg/CNFUIOSG.h` — macOS Cocoa Platform Config

Contains: Cocoa/CoreAudio/OpenGL includes, `EnableDragDrop=1`, `MyAppIsBundle=1`, app metadata strings.

**No dead code.** All defines are actively used by the cocoa.mm backend.

### 2. `src/config/CNFUIALL.h` — Cross-Platform Config

| Line | Define | Value | Effect |
|------|--------|-------|--------|
| L12 | `SmallGlobals` | 0 | Normal (non-compact) globals — not dead, just default path |
| L13 | `cIncludeUnused` | **0** | Master switch: unused code not included. Guards unknown blocks. |
| L19 | `BigEndianUnaligned` | 0 | No big-endian unaligned access optimization |
| L23 | `LittleEndianUnaligned` | 0 | No little-endian unaligned access optimization |
| L27 | `Have_ASR` | 0 | No arithmetic shift right intrinsic |
| L31 | `HaveMySwapUi5r` | 0 | No custom byte-swap for 5-byte values |

**No `#if 0` blocks.** The zero-valued defines cause fallback/safe code paths to be chosen elsewhere — this is normal platform configuration, not dead code. `cIncludeUnused=0` is notable as it suppresses optional code blocks throughout the codebase.

### 3. `src/config/CNFUIPIC.h` — Platform-Specific Config

Contains only a comment header — **empty config file, no dead code.**

---

## Grand Summary — All Scoped Files

| Scope | Files | Total Lines | Dead/Conditional Lines | Category Breakdown |
|-------|-------|-------------|----------------------|-------------------|
| `src/platform/common/` | 15 | ~5,500 | ~510 (incl actv_code.h) | 374 SAFELY_REMOVABLE (actv_code.h), ~100 SAFELY_REMOVABLE (control_mode), 8 ALTERNATIVE_IMPL, ~30 NOT_YET_ENABLED |
| `src/lang/` | 11 | ~3,500 | **0** | Clean |
| `src/unused/` | 4 | 2,688 | **2,688** (entire dir unused) + 243 internal | All UNUSED_MODULE |
| `src/resources/` | 10 | N/A (binary) | **0** | No code |
| Config files | 3 | ~80 | **0** | Configuration, not dead code |

### Top Removal Candidates

| Priority | Target | Lines Saved | Risk |
|----------|--------|-------------|------|
| **1** | `actv_code.h` — delete entirely | 374 | **None** — commercial licensing feature, UseActvCode=0 hardcoded |
| **2** | All `#if 0` blocks in `control_mode.cpp` (4 blocks) | 30 | **None** — registration string copy, never enabled |
| **3** | `EnableDemoMsg` blocks in control_mode.cpp + intl_chars.h + intl_chars.cpp | ~70 | **None** — demo watermark, EnableDemoMsg=0 hardcoded |
| **4** | `SGLUDDSP.h` — delete from unused/ | 229 | **Low** — OSS is deprecated; ALSA supersedes it |
| **5** | `#if 0` static-linking block in `SGLUALSA.h` (L27–L235) | 209 | **None** — dynamic loading is always preferred |
| **Keep** | `LTOVRUDP.h`, `LTOVRBPF.h`, `SGLUALSA.h` (minus dead blocks) | — | These have genuine future value for LocalTalk and Linux audio |
| **Keep** | `EmLocalTalk` blocks in osglu_common.h/cpp | — | NOT_YET_ENABLED — needed if LocalTalk is activated |
