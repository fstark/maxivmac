# Plan: Remove Internal Screen Buffer UI

Remove ~4,000 lines of legacy screen-buffer overlay UI (text rendering, control mode,
localization) that duplicates the imgui overlay. Replace `MacMsg()` calls with
dbglog/stderr logging + SDL message boxes for fatal errors. Backfill a few missing
features into the existing imgui overlay (16x/32x speed, Stopped/Background/AutoSlow
toggles, About info).

## Feature Inventory & Disposition

| Old Feature | imgui Replacement | Action |
|---|---|---|
| Control Mode menu (Ctrl key) | imgui overlay (Ctrl key) | Already replaced — remove old |
| About (Ctrl+A) | None currently | **Add** "About" section to imgui Advanced tab |
| Open Disk (Ctrl+O) | "Insert Disk" button in Machine tab | Already replaced |
| Quit confirmation | Power Off button (no confirmation) | Replace with console warning log |
| Speed 1x–8x + Unlimited | Speed tab (1x/2x/4x/8x/Unlimited) | Already replaced |
| Speed 16x, 32x | Not in overlay | **Add** to Speed tab |
| Stopped toggle | Not in overlay | **Add** to Speed tab |
| Background toggle | Not in overlay | **Add** to Speed tab |
| AutoSlow toggle | Not in overlay | **Add** to Speed tab |
| Magnify toggle | Zoom in Display tab | Already replaced |
| Fullscreen toggle | Display tab | Already replaced |
| Reset (with confirmation) | Reboot button (no confirmation) | Keep as-is |
| Interrupt | Interrupt button | Already replaced |
| Ctrl Key toggle (see note below) | Not in overlay | Drop — solved by configurable overlay key (future) |
| Copy Options (build string) | Not in overlay | Drop — developer-only |
| Help screen | Self-evident imgui UI | Already replaced |
| MacMsg (non-fatal errors) | None | **Replace** with dbglog + stderr (toast candidates, see below) |
| MacMsg (fatal / OOM) | showMessageBox at shutdown | Already handled — remove MacMsg wrapper |
| WaitForRom / NoRom overlay | Model Selector | Already replaced — remove dead code |
| Abnormal ID warning | dbglog already logs it | Remove MacMsg call, keep dbglog |
| Unsupported disk warning | None | **Replace** with dbglog + stderr (toast candidate) |
| Text rendering (8×16 font, all depths) | Not needed | Remove entirely |
| 11 language translations | Not needed (imgui strings are English) | Remove entirely |

### Note: Ctrl Key toggle

The old "K" command in control mode toggled a synthetic Control key state on the
emulated Mac. This exists because the physical Ctrl key is intercepted by the
emulator overlay and never reaches the guest OS. Pressing Ctrl+K would "hold down"
the emulated Mac's Control key until toggled off.

This is a symptom of the overlay activation key being hardcoded to Ctrl. A better
solution is making the overlay activation key configurable (future work) — e.g.
allowing F12, Pause, or another key that doesn't conflict with the emulated keyboard.
Once that's done, the physical Ctrl key can pass through to the guest normally.

## Steps

### Phase 1: Backfill missing imgui overlay features

1. **Add 16x and 32x speed options** to `speeds[]` array in
   `imgui_overlay.cpp:drawSpeedTab()` — add entries `{"16x", 16}` and `{"32x", 32}`.
   Verify `g_speedValue` supports these values in the timing code.

2. **Add Stopped/Pause toggle** to Speed tab — checkbox or toggle button bound to
   `g_speedStopped`.

3. **Add Background toggle** to Speed tab — checkbox bound to `g_runInBackground`.

4. **Add AutoSlow toggle** to Speed tab — checkbox bound to `g_wantNotAutoSlow`
   (note: inverted sense).

5. **Add About section** to Advanced tab in imgui overlay — show product name,
   version, copyright. Use hardcoded English strings.

### Phase 2: Replace MacMsg() with logging

6. **Create a `ui_alert()` utility** (or reuse existing pattern) that:
   - Logs message via `dbglog_writeCStr()` + `fprintf(stderr, ...)`
   - For fatal errors, stores the message for `showMessageBox()` at shutdown (existing path)

7. **Replace each MacMsg call site:**
   - `emulator_shell.cpp:859` (OOM fatal) → log + return false
   - `emulator_shell.cpp:350` (quit warning) → log to stderr
   - `disk_io.cpp:173` (too many images) → log + stderr
   - `disk_io.cpp:206` (open failed) → log + stderr
   - `disk_io.cpp:234` (new disk creation failed) → log + stderr
   - `rom_loader.cpp:21,25` (ROM short/read error) → log + stderr, return error
   - `osglu_common.cpp:716` (abnormal ID) → keep dbglog, remove MacMsg call
   - `control_mode.cpp:381,386,391,397,1026` — all internal to control mode, deleted with it

### Phase 3: Remove internal screen buffer UI

8. **Delete `src/platform/common/control_mode.cpp`** (~1,050 lines)
9. **Delete `src/platform/common/control_mode.h`** (~60 lines)
10. **Delete `src/platform/common/intl_chars.cpp`** (~900 lines)
11. **Delete `src/platform/common/intl_chars.h`** (~130 lines)

12. **Remove screen buffer overlay compositing** from `osglu_common.cpp`:
    - Remove `GetCurDrawBuff()` overlay logic (`g_cntrlDisplayBuff` compositing)
    - Remove `MacMsg()` / `MacMsgDisplayOn()` / `MacMsgDisplayOff()` functions
    - Remove `SavedBriefMsg`, `SavedLongMsg`, `g_savedFatalMsg` if no longer needed
    - Remove `g_specialModes` and `SpecialModeTst/Set/Clr` macros
    - Remove `g_cntrlDisplayBuff` allocation and management

13. **Remove `WaitForRom()`** and `NoRomMsgDisplayOn()` — model selector handles ROM.

### Phase 4: Remove localization infrastructure

14. **Delete all 11 language files** in `src/lang/`:
    `lang_{english,french,german,italian,spanish,dutch,portuguese,polish,czech,serbian,catalan}.cpp`

15. **Delete localization infrastructure files:**
    `localization.cpp`, `localization.h`, `localization_keys.h`, `localization_impl.h`,
    `strings_english.h`

16. **Replace any remaining `Localize()` calls** outside deleted files with hardcoded
    English strings or log messages.

### Phase 5: Update build system and clean up

17. **Update `CMakeLists.txt`** — remove deleted files from `COMMON_SOURCES` and any
    lang source lists.

18. **Remove stale `#include` directives** referencing deleted headers.

19. **Remove global variables** that are no longer referenced: `g_specialModes`,
    `g_cntrlDisplayBuff`, `g_needWholeScreenDraw` (if only used by overlay).

20. **Clean up callers** — any code that checks `MacMsgDisplayed` or
    `SpecialModeTst(SpclModeControl)` in the main loop and key handling.

## Files

### Files to DELETE (~4,000 lines)

| File | Content |
|---|---|
| `src/platform/common/control_mode.cpp` | Overlay rendering, control mode state machine, key handling |
| `src/platform/common/control_mode.h` | Mode enums, prototypes |
| `src/platform/common/intl_chars.cpp` | 8×16 font bitmap data, character maps |
| `src/platform/common/intl_chars.h` | Cell enums, glyph declarations |
| `src/lang/lang_english.cpp` | English translations |
| `src/lang/lang_french.cpp` | French translations |
| `src/lang/lang_german.cpp` | German translations |
| `src/lang/lang_italian.cpp` | Italian translations |
| `src/lang/lang_spanish.cpp` | Spanish translations |
| `src/lang/lang_dutch.cpp` | Dutch translations |
| `src/lang/lang_portuguese.cpp` | Portuguese translations |
| `src/lang/lang_polish.cpp` | Polish translations |
| `src/lang/lang_czech.cpp` | Czech translations |
| `src/lang/lang_serbian.cpp` | Serbian translations |
| `src/lang/lang_catalan.cpp` | Catalan translations |
| `src/lang/localization.cpp` | Language dispatcher |
| `src/lang/localization.h` | Localization API |
| `src/lang/localization_keys.h` | String key constants |
| `src/lang/localization_impl.h` | LangPair structure |
| `src/lang/strings_english.h` | Master English string definitions |

### Files to MODIFY

| File | Changes |
|---|---|
| `src/platform/imgui_overlay.cpp` | Add 16x/32x speeds, Stopped/Background/AutoSlow toggles, About section |
| `src/platform/common/osglu_common.cpp` | Remove MacMsg, overlay compositing, special modes |
| `src/platform/emulator_shell.cpp` | Replace MacMsg calls with logging |
| `src/core/disk_io.cpp` | Replace MacMsg calls with logging |
| `src/platform/common/rom_loader.cpp` | Replace MacMsg calls with logging, remove WaitForRom |
| `src/core/machine.cpp` | Remove WarnMsgAbnormalID MacMsg call (keep dbglog) |
| `CMakeLists.txt` | Remove deleted files from source lists |

## Verification

1. **Build** — `cmake --build bld/macos` compiles cleanly, no missing symbols
2. **Golden tests** — `test/verify.sh` passes for all ROM models
3. **Manual test** — Ctrl key opens imgui overlay with full speed options and
   toggles; errors appear on stderr; About info visible
4. **Headless build** — `bld/macos-headless` still compiles
5. **Grep sanity** — `grep -r "MacMsg\|control_mode\|intl_chars\|Localize\|SpclMode" src/`
   returns zero hits

## Decisions

- **Ctrl Key toggle** — Dropped. Symptom of hardcoded overlay key; real fix is
  configurable overlay activation key (future work).
- **Copy Options** — Dropped. Developer tool; build system documents this.
- **Confirmation dialogs** (quit/reset/interrupt) — Dropped. imgui buttons are in
  a deliberate control panel, reducing accidental activation. Quit warning logged
  to stderr.
- **Localization** — Entire system removed. Re-adding i18n later would use
  imgui-native approaches, not screen buffer text.
- **Error display to user** — stderr + dbglog for now. imgui toast system is a
  follow-up (see below).

## Future: imgui toast notification system

After removing the screen buffer overlay, some errors that were previously shown to
the user via `MacMsg()` will only appear on stderr. A lightweight imgui toast system
would restore user-visible feedback for these cases.

### Toast use cases

These are situations where the user should see a brief, non-modal notification in
the imgui UI rather than (or in addition to) a stderr log:

| Use Case | Current MacMsg Call | Severity | Notes |
|---|---|---|---|
| **Disk image open failed** | `disk_io.cpp:206` — file not found, permissions | Warning | Most common user-facing error |
| **Too many disk images** | `disk_io.cpp:173` — all 6 slots full | Warning | User tried to insert 7th disk |
| **New disk creation failed** | `disk_io.cpp:234` — fopen write error | Warning | Disk create feature |
| **ROM file too short** | `rom_loader.cpp:21` — truncated ROM | Error | Rare, but confusing if silent |
| **ROM read error** | `rom_loader.cpp:25` — I/O error on ROM file | Error | Rare |
| **Unsupported disk image** | `control_mode.cpp:1026` — bad format | Warning | User dragged wrong file type |
| **Abnormal trap** | `osglu_common.cpp:716` — emulation anomaly | Info | Developer-oriented, low priority |
| **Quit with disks mounted** | `emulator_shell.cpp:350` — unclean shutdown | Warning | Could also be a confirmation prompt |

### Toast design notes

- Auto-dismiss after ~5 seconds, with manual dismiss
- Stack multiple toasts vertically (bottom-right or top-right corner)
- Color-coded by severity (info/warning/error)
- Should not block emulation or steal focus
- Implementation: small self-contained imgui widget, no dependency on the old overlay

### Configurable overlay activation key

The overlay is currently hardcoded to Ctrl (SDL_SCANCODE_LCTRL / SDL_SCANCODE_RCTRL).
This conflicts with the emulated Mac's Control key. Future work should allow
configuring the activation key (e.g. F12, Pause, right-Ctrl only) so the physical
Ctrl can pass through to the guest OS. This eliminates the need for the "Ctrl Key
toggle" workaround entirely.

## Further Considerations

1. **Headless build compatibility** — `fprintf(stderr, ...)` is safe everywhere;
   verify `dbglog_*` functions are available in headless builds.
2. **`g_needWholeScreenDraw`** — Check if this flag is used for anything besides
   overlay compositing. If also used for window resize/redraw, keep it.
