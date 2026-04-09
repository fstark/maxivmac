# UI Overlay Plan — Status

## Completed

Removed ~4,000 lines of legacy screen-buffer overlay UI (text rendering, control
mode, 11-language localization) and replaced with stderr logging. Backfilled missing
features into the imgui overlay. See commit `0b356be`.

**What was removed:**
- `control_mode.cpp/h` — text overlay, control mode state machine, key handling
- `intl_chars.cpp/h` — 8×16 font bitmaps, character maps, string substitution
- `src/lang/` — all 11 language files + localization infrastructure (16 files)
- `MacMsg()` overlay calls → replaced with `fprintf(stderr, ...)`
- Screen buffer compositing (`GetCurDrawBuff`, `g_cntrlDisplayBuff`)
- `WaitForRom()` — model selector handles ROM selection
- `--lang` CLI option

**What was added:**
- `keyboard_map.cpp/h` — surviving keyboard functions and globals
- Speed tab: 16x, 32x options; Stopped, Run in Background, AutoSlow toggles
- Advanced tab: About section (product name, copyright, license)

**Dropped features (not worth replacing):**
- Ctrl Key toggle — symptom of hardcoded overlay key; real fix is configurable key
- Copy Options — developer-only; build system documents this
- Confirmation dialogs (quit/reset/interrupt) — imgui panel reduces accidental activation

## Remaining Work

### imgui toast notification system

Some errors that were previously shown to the user via `MacMsg()` now only appear on
stderr. A lightweight imgui toast system would restore user-visible feedback.

#### Toast use cases

| Use Case | Source | Severity | Notes |
|---|---|---|---|
| **Disk image open failed** | `disk_io.cpp` — file not found, permissions | Warning | Most common user-facing error |
| **Too many disk images** | `disk_io.cpp` — all 6 slots full | Warning | User tried to insert 7th disk |
| **New disk creation failed** | `disk_io.cpp` — fopen write error | Warning | Disk create feature |
| **ROM file too short** | `rom_loader.cpp` — truncated ROM | Error | Rare, but confusing if silent |
| **ROM read error** | `rom_loader.cpp` — I/O error on ROM file | Error | Rare |
| **Unsupported disk image** | `sony.cpp` — bad format | Warning | User dragged wrong file type |
| **Abnormal trap** | `osglu_common.cpp` — emulation anomaly | Info | Developer-oriented, low priority |
| **Quit with disks mounted** | `emulator_shell.cpp` — unclean shutdown | Warning | Could also be a confirmation prompt |

#### Toast design notes

- Auto-dismiss after ~5 seconds, with manual dismiss
- Stack multiple toasts vertically (bottom-right or top-right corner)
- Color-coded by severity (info/warning/error)
- Should not block emulation or steal focus
- Implementation: small self-contained imgui widget, no dependency on the old overlay

### Configurable overlay activation key

The overlay is currently hardcoded to Ctrl (SDL_SCANCODE_LCTRL / SDL_SCANCODE_RCTRL).
This conflicts with the emulated Mac's Control key. Future work should allow
configuring the activation key (e.g. F12, Pause, right-Ctrl only) so the physical
Ctrl can pass through to the guest OS. This eliminates the need for the old "Ctrl Key
toggle" workaround entirely.
