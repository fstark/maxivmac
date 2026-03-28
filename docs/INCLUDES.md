# src/config/ — Configuration Header Analysis

The `src/config/` directory holds static configuration headers inherited from
the original minivmac build-system generator.  CMakeLists.txt adds
`src/config/` to the include path so these files are found via bare
`#include "CNFUDALL.h"` etc.

A stale copy of the old originals still lives in `cfg/` — those files are
**not** referenced by the build and can be deleted.

---

## File-by-file

### CNFUIALL.h — Compiler / platform configuration (user-independent, all code)

| | |
|---|---|
| **Generated?** | Originally yes (by the minivmac setup tool). Now hand-maintained. |
| **Included from** | `src/core/common.h` (line 14), `src/OSGCOMUI.h` (line 22), `src/platform/common/osglu_ui.h` (line 22) |
| **Defines** | `cIncludeUnused`, `UNUSED()` / `UnusedParam()` macros, endian/alignment probes (`BigEndianUnaligned`, `LittleEndianUnaligned`, `Have_ASR`, `HaveMySwapUi5r`), 64-bit integer typedefs (`si6r`, `ui6r`, etc.), `LIT64()` |
| **Future utility** | **Low-to-moderate.** The integer typedefs are used throughout the CPU emulator and FPU code. The endian/alignment flags could be replaced by `<bit>` (C++20) or compile-time checks. `cIncludeUnused` gates dead code — once that code is removed this define goes away. The `UNUSED()` macros duplicate standard `[[maybe_unused]]`. This file could eventually be folded into a single `src/core/types.h`. |

---

### CNFUIPIC.h — Platform-independent, user-independent configuration (per-variation)

| | |
|---|---|
| **Generated?** | Originally yes. Now hand-maintained. |
| **Included from** | `src/core/common.h` (line 16) |
| **Defines** | Nothing — the file is empty (just a pragma-once guard). |
| **Future utility** | **None.** Can be deleted together with the `#include` in `common.h`. |

---

### CNFUDALL.h — Model-independent device/display configuration (user-dependent, all code)

| | |
|---|---|
| **Generated?** | Originally yes. Now hand-maintained. |
| **Included from** | `src/core/common.h` (line 27), `src/platform/common/osglu_ud.h` (line 15) |
| **Defines** | `dbglog_HAVE`, `NumDrives` (6), `NumPbufs` (4). Comments note that screen dimensions, ROM size, clock speed, etc. are now runtime via `MachineConfig`. |
| **Future utility** | **Low.** Only three constants remain. `NumDrives` and `NumPbufs` could move into `MachineConfig` or a simple header. `dbglog_HAVE` could become a CMake compile definition. File is a candidate for elimination. |

---

### CNFUDOSG.h — OS-glue display/input configuration (user-dependent, OS-glue only)

| | |
|---|---|
| **Generated?** | Originally yes. Now hand-maintained. |
| **Included from** | `src/platform/common/osglu_ud.h` (line 10) |
| **Defines** | Key-code mappings (`MKC_formac_*` as inline constexpr uint8_t), control-mode toggles (`WantEnblCtrlInt`, `UseControlKeys`, …), `SaveDialogEnable`, `NeedIntlChars`, `kBldOpts` build-options string, version string. |
| **Future utility** | **Moderate.** Key mappings are actively used by `control_mode.cpp` and the keyboard driver. Many of the `#define` toggles (SaveDialog, AltKeys mode, etc.) are trivially always-on or always-off and could become constexpr bools or be inlined. `kBldOpts` and the version macro are useful. Could be refactored into a `src/platform/platform_config.h`. |

---

### CNFUDPIC.h — Per-model device/hardware configuration (user-dependent, platform-independent)

| | |
|---|---|
| **Generated?** | Originally yes. Now hand-maintained. |
| **Included from** | `src/core/common.h` (line 30) |
| **Defines** | CPU feature flags (`Use68020`, `EmFPU`, `EmMMU` — always 1 for unified binary), cycle-accuracy options (`WantCycByPriOp`, `WantCloserCyc`), extension flags (`IncludeExtnPbufs`, `IncludeExtnHostTextClipExchange`), Sony driver options (`Sony_SupportDC42`, `Sony_SupportTags`, …), parameter-RAM defaults (`SpeakerVol`, `MenuBlink`, …), wire variable accessor macros (`SoundDisable`, `VIA1_iA0`, …, `MemOverlay`, `ADB_Int`, …), change-notify callback aliases (`VIA1_iA4_ChangeNtfy`, …), `Mouse_Enabled()` declaration, `WantDisasm`. Includes `core/wire_ids.h`. |
| **Future utility** | **High — but should be split.** This is the largest and most load-bearing config header. It mixes three unrelated concerns and should be broken up as follows: |

#### Proposed split

| Current content | New file | Rationale |
|---|---|---|
| Wire accessor macros, signal aliases, ChangeNtfy aliases (~60% of file) | `src/core/wire_macros.h` | Lives next to `wire_ids.h` and `wire_bus.cpp` — core emulation wiring |
| CPU flags, cycle options, Sony options, extension flags, `WantDisasm` | `src/core/emulation_config.h` | Compile-time emulation feature toggles |
| PRAM defaults (`SpeakerVol`, `MenuBlink`, etc.) | Inline into `rtc.cpp` or `MachineConfig` | Only used during PRAM initialization |
| `Mouse_Enabled()` declaration | Remove (already declared in `mouse.h`) | Duplicate |

---

### CNFUIOSG.h — OS-glue platform configuration (user-independent, OS-glue only)

| | |
|---|---|
| **Generated?** | Originally yes. Now hand-maintained. |
| **Included from** | `src/OSGCOMUI.h` (line 16), `src/platform/common/osglu_ui.h` (line 16) |
| **Defines** | SDL system includes (`<SDL.h>`, `<stdio.h>`, …), platform identity flags (`EnableDragDrop`, `MyAppIsBundle`, `WantOSGLUSDL`), application metadata strings (`kStrAppName`, `kAppVariationStr`, `kStrCopyrightYear`, `kMaintainerName`, `kStrHomePage`). |
| **Future utility** | **Moderate.** The SDL include and platform-identity bits are necessary as long as the SDL platform backend exists. The metadata strings are used in the about-box. Could be merged into a `src/platform/sdl_config.h` to make the SDL dependency explicit. |

---

### STRCONST.h — String constants via localization

| | |
|---|---|
| **Generated?** | Originally yes (selected a language-specific header). Now a trivial redirect. |
| **Included from** | `src/platform/common/osglu_ud.h` (line 22) |
| **Defines** | Nothing directly — just includes `lang/localization_keys.h` and `lang/localization.h`. |
| **Future utility** | **None as a separate file.** The two includes can be placed directly in `osglu_ud.h` (or wherever strings are needed). Candidate for deletion. |

---

### Info.plist — macOS application bundle metadata

| | |
|---|---|
| **Generated?** | No, hand-written. |
| **Included from** | **Nowhere.** The current CMakeLists.txt does not set `MACOSX_BUNDLE_INFO_PLIST` and does not produce a `.app` bundle during the CMake build. The pre-built `maxivmac.app/` bundle in the repo has its own copy. |
| **Defines** | Bundle identifier (`com.gryphel.maxivmac`), executable name, version 37.03, icon file, min macOS 10.15, high-res capable. |
| **Future utility** | **None.** The native macOS Cocoa backend has been removed with no plans to restore it. The SDL backend does not use a macOS bundle. Can be deleted. |

---

### English.lproj/dummy.txt — macOS bundle localisation placeholder

| | |
|---|---|
| **Generated?** | No. |
| **Included from** | **Nowhere.** Was a resource file for the Cocoa bundle build. |
| **Defines** | Contains the single word `dummy`. |
| **Future utility** | **None.** The native macOS Cocoa backend has been removed. Delete. |

---

## Summary

| File | Status | Action |
|------|--------|--------|
| `CNFUIALL.h` | Active, low content | Refactor into `core/types.h` |
| `CNFUIPIC.h` | Empty | Delete |
| `CNFUDALL.h` | Active, 3 defines | Merge into `MachineConfig` or a minimal header |
| `CNFUDOSG.h` | Active, key mappings + UI config | Refactor into `platform/platform_config.h` |
| `CNFUDPIC.h` | Active, large, load-bearing | Split into `core/wire_macros.h` + `core/emulation_config.h` |
| `CNFUIOSG.h` | Active, SDL config + metadata | Refactor into `platform/sdl_config.h` |
| `STRCONST.h` | Trivial redirect | Delete, inline the two includes |
| `Info.plist` | Unused by build | Delete |
| `English.lproj/` | Unused | Delete |

The `cfg/` directory at project root contains **stale copies** of the original
minivmac versions of these files.  It is not referenced by CMakeLists.txt and
can be removed entirely.
