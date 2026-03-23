# Phase 3 — File Rename & Directory Structure (Detailed Plan)

Make the source tree navigable: rename cryptic 8.3 filenames to human-readable names, organize into logical subdirectories, convert code-as-headers into proper compilation units, and rename `.c` → `.cpp` / `.m` → `.mm`.

---

## Context: What We're Starting With

After Phase 2 the codebase compiles as C++17 with standard types and no custom macros. But the source tree still looks like 1990s Mac development:

```
src/
  ADBEMDEV.c   ASCEMDEV.c   COMOSGLU.h   CONTROLM.h   ...
  GLOBGLUE.c   IWMEMDEV.c   M68KITAB.c   MINEM68K.c   ...
  OSGLUCCO.m   OSGLUSDL.c   PROGMAIN.c   VIAEMDEV.c   ...
  (86 files, flat, all-caps, cryptic abbreviations)
cfg/
  CNFUIALL.h  CNFUDALL.h  CNFUDPIC.h  ...  (7 config headers + Info.plist)
```

### Files by Category

**Compilation units (22):** `GLOBGLUE.c`, `M68KITAB.c`, `MINEM68K.c`, `DISAM68K.c`, `VIAEMDEV.c`, `VIA2EMDV.c`, `IWMEMDEV.c`, `SCCEMDEV.c`, `RTCEMDEV.c`, `ROMEMDEV.c`, `SCSIEMDV.c`, `SONYEMDV.c`, `SCRNEMDV.c`, `VIDEMDEV.c`, `MOUSEMDV.c`, `ADBEMDEV.c`, `ASCEMDEV.c`, `SNDEMDEV.c`, `KBRDEMDV.c`, `PMUEMDEV.c`, `PROGMAIN.c`, `OSGLUCCO.m` (+ `OSGLUSDL.c` for SDL builds)

**True declaration headers (21):** One `.h` per compilation unit above (e.g., `GLOBGLUE.h`, `MINEM68K.h`, etc.) plus `OSGLUAAA.h` (platform abstraction contract)

**Code-as-headers — shared platform (5):** `COMOSGLU.h` (1,390 lines), `CONTROLM.h` (1,420 lines), `PBUFSTDC.h`, `INTLCHAR.h`, `DATE2SEC.h` — `#include`'d into each backend

**Code-as-headers — C "templates" (2):** `SCRNMAPR.h` (169 lines, included 2–12× per backend with different `#define` params), `SCRNTRNS.h` (162 lines, similar pattern)

**Code-as-headers — device internals (5):** `FPCPEMDV.h` + `FPMATHEM.h` (included by `MINEM68K.c` only), `ADBSHARE.h` (by `ADBEMDEV.c`), `SCRNHACK.h` + `HPMCHACK.h` (by `ROMEMDEV.c`)

**Code-as-headers — include chains (4):** `PICOMMON.h`, `OSGCOMUI.h`, `OSGCOMUD.h`, `DFCNFCMP.h` — "precompiled header" chains

**Other headers (4):** `ENDIANAC.h` (endian macros), `ALTKEYSM.h` + `ACTVCODE.h` (included by `CONTROLM.h`), `LOCALTLK.h` (optional LocalTalk)

**Language strings (11):** `STRCNENG.h`, `STRCNFRE.h`, `STRCNGER.h`, etc. — one active per build

**Orphaned / dead code (4):** `SGLUDDSP.h`, `SGLUALSA.h`, `LTOVRBPF.h`, `LTOVRUDP.h` — no `#include` references

**Resources (10):** `ICONAPPO.icns`, `ICONDSKO.icns`, `ICONROMO.icns` (macOS), `ICONAPPW.ico`, `ICONDSKW.ico`, `ICONROMW.ico` (Windows), `ICONAPPM.r`, `ICONDSKM.r`, `ICONROMM.r`, `main.r` (Classic Mac)

---

## Target Directory Structure

```
src/
  core/
    machine.cpp          ← GLOBGLUE.c
    machine.h            ← GLOBGLUE.h
    main.cpp             ← PROGMAIN.c
    main.h               ← PROGMAIN.h
    endian.h             ← ENDIANAC.h
    defaults.h           ← DFCNFCMP.h
    common.h             ← PICOMMON.h (becomes a thin forwarding header)
  cpu/
    m68k.cpp             ← MINEM68K.c
    m68k.h               ← MINEM68K.h
    m68k_tables.cpp      ← M68KITAB.c
    m68k_tables.h        ← M68KITAB.h
    disasm.cpp           ← DISAM68K.c
    disasm.h             ← DISAM68K.h
    fpu_emdev.h          ← FPCPEMDV.h  (stays as included-header — used as inline code)
    fpu_math.h           ← FPMATHEM.h  (stays as included-header — used as inline code)
  devices/
    via.cpp              ← VIAEMDEV.c
    via.h                ← VIAEMDEV.h
    via2.cpp             ← VIA2EMDV.c
    via2.h               ← VIA2EMDV.h
    iwm.cpp              ← IWMEMDEV.c
    iwm.h                ← IWMEMDEV.h
    scc.cpp              ← SCCEMDEV.c
    scc.h                ← SCCEMDEV.h
    rtc.cpp              ← RTCEMDEV.c
    rtc.h                ← RTCEMDEV.h
    rom.cpp              ← ROMEMDEV.c
    rom.h                ← ROMEMDEV.h
    scsi.cpp             ← SCSIEMDV.c
    scsi.h               ← SCSIEMDV.h
    sony.cpp             ← SONYEMDV.c
    sony.h               ← SONYEMDV.h
    screen.cpp           ← SCRNEMDV.c
    screen.h             ← SCRNEMDV.h
    video.cpp            ← VIDEMDEV.c
    video.h              ← VIDEMDEV.h
    mouse.cpp            ← MOUSEMDV.c
    mouse.h              ← MOUSEMDV.h
    adb.cpp              ← ADBEMDEV.c
    adb.h                ← ADBEMDEV.h
    adb_shared.h         ← ADBSHARE.h  (stays as included-header — only in adb.cpp)
    asc.cpp              ← ASCEMDEV.c
    asc.h                ← ASCEMDEV.h
    sound.cpp            ← SNDEMDEV.c
    sound.h              ← SNDEMDEV.h
    keyboard.cpp         ← KBRDEMDV.c
    keyboard.h           ← KBRDEMDV.h
    pmu.cpp              ← PMUEMDEV.c
    pmu.h                ← PMUEMDEV.h
    screen_hack.h        ← SCRNHACK.h  (stays as included-header — only in rom.cpp)
    hpmac_hack.h         ← HPMCHACK.h  (stays as included-header — only in rom.cpp)
  platform/
    platform.h           ← OSGLUAAA.h
    cocoa.mm             ← OSGLUCCO.m
    sdl.cpp              ← OSGLUSDL.c
    localtalk.h          ← LOCALTLK.h
    common/
      osglu_common.cpp   ← COMOSGLU.h  (converted to proper compilation unit)
      osglu_common.h     ← (new — extracted declarations from COMOSGLU.h)
      control_mode.cpp   ← CONTROLM.h  (converted to proper compilation unit)
      control_mode.h     ← (new — extracted declarations from CONTROLM.h)
      alt_keys.h         ← ALTKEYSM.h
      actv_code.h        ← ACTVCODE.h
      param_buffers.cpp  ← PBUFSTDC.h  (converted to proper compilation unit)
      param_buffers.h    ← (new — extracted declarations)
      intl_chars.cpp     ← INTLCHAR.h  (converted to proper compilation unit)
      intl_chars.h       ← (new — extracted declarations)
      date_to_sec.h      ← DATE2SEC.h  (stays as included-header — small, conditionally used)
      screen_map.h       ← SCRNMAPR.h  (stays as C-template header — multi-included)
      screen_translate.h ← SCRNTRNS.h  (stays as C-template header — multi-included)
      osglu_ui.h         ← OSGCOMUI.h  (thin include-chain header)
      osglu_ud.h         ← OSGCOMUD.h  (thin include-chain header)
  config/
    CNFUIALL.h           ← cfg/CNFUIALL.h (moved, name kept — used in include chains)
    CNFUIPIC.h           ← cfg/CNFUIPIC.h
    CNFUIOSG.h           ← cfg/CNFUIOSG.h  (CMake-generated stays in build tree)
    CNFUDALL.h           ← cfg/CNFUDALL.h
    CNFUDOSG.h           ← cfg/CNFUDOSG.h
    CNFUDPIC.h           ← cfg/CNFUDPIC.h
    STRCONST.h           ← cfg/STRCONST.h
    Info.plist           ← cfg/Info.plist
    English.lproj/       ← cfg/English.lproj/
  lang/
    strings_english.h    ← STRCNENG.h
    strings_french.h     ← STRCNFRE.h
    strings_german.h     ← STRCNGER.h
    strings_italian.h    ← STRCNITA.h
    strings_spanish.h    ← STRCNSPA.h
    strings_dutch.h      ← STRCNDUT.h
    strings_portuguese.h ← STRCNPTB.h
    strings_polish.h     ← STRCNPOL.h
    strings_czech.h      ← STRCNCZE.h
    strings_serbian.h    ← STRCNSRL.h
    strings_catalan.h    ← STRCNCAT.h
  resources/
    ICONAPPO.icns        (macOS icons)
    ICONDSKO.icns
    ICONROMO.icns
    ICONAPPW.ico         (Windows icons)
    ICONDSKW.ico
    ICONROMW.ico
    ICONAPPM.r           (Classic Mac resources — kept for reference)
    ICONDSKM.r
    ICONROMM.r
    main.r
```

---

## Strategy

### Guiding Principles

1. **One logical change per commit** — each commit does exactly one kind of operation (move files, rename files, update includes, convert a code-as-header). Build + boot verified after each.
2. **Use `git mv`** to preserve history tracking. Git detects renames via content similarity, so even `git mv` + content changes in the same commit work if the content change is small (<50%).
3. **Create directory structure first** — empty dirs, then move files into them.
4. **Update `#include` paths in the same commit as the move** — otherwise the build breaks.
5. **Convert code-as-headers last** — this is the riskiest step and benefits from already having the final directory structure in place.

### What Stays as Include-Headers (Not Converted to .cpp)

Some `.h` files are intentionally designed for `#include`-based expansion and **should not** be converted to separate compilation units:

| File | Reason |
|------|--------|
| `SCRNMAPR.h` → `screen_map.h` | C "template" — included 2–12× per backend with different `#define` params to generate different functions. Would need C++ templates to replace. |
| `SCRNTRNS.h` → `screen_translate.h` | Same pattern — multi-included with different params. |
| `FPCPEMDV.h` → `fpu_emdev.h` | Included at line 8216 of `MINEM68K.c` deep inside the CPU emulator. Tightly coupled, uses local macros/state. |
| `FPMATHEM.h` → `fpu_math.h` | Same — helper for FPU emulation, included right before `FPCPEMDV.h`. |
| `ADBSHARE.h` → `adb_shared.h` | Included only by `ADBEMDEV.c`, contains shared ADB state — small, single consumer. |
| `SCRNHACK.h` → `screen_hack.h` | Included only by `ROMEMDEV.c` — small ROM screen patch code. |
| `HPMCHACK.h` → `hpmac_hack.h` | Included only by `ROMEMDEV.c` — small hack. |
| `DATE2SEC.h` → `date_to_sec.h` | Small utility, conditionally used by some backends. |
| `ALTKEYSM.h` → `alt_keys.h` | Included only by `CONTROLM.h` — part of control mode internals. |
| `ACTVCODE.h` → `actv_code.h` | Included only by `CONTROLM.h`. |

### What Gets Converted to Proper Compilation Units

These code-as-headers contain substantial implementation code that is `#include`'d into every backend, causing code duplication and making the build fragile:

| File | Lines | Included By | Conversion Strategy |
|------|-------|-------------|-------------------|
| `COMOSGLU.h` | 1,390 | All 9 OSGLU backends | Extract declarations → `osglu_common.h`, definitions → `osglu_common.cpp`. Link as separate TU. |
| `CONTROLM.h` | 1,420 | All 9 OSGLU backends | Extract declarations → `control_mode.h`, definitions → `control_mode.cpp`. Keeps `#include` of `ALTKEYSM.h` and `ACTVCODE.h` internally. |
| `PBUFSTDC.h` | ~200 | 5 backends | Extract → `param_buffers.h` + `param_buffers.cpp`. |
| `INTLCHAR.h` | ~300 | All 9 backends | Extract → `intl_chars.h` + `intl_chars.cpp`. Pure data tables. |

### Orphaned Files

These 4 files have zero `#include` references anywhere in the codebase and will be moved to a `src/unused/` directory (not deleted — they may be useful for future Linux ALSA/OSS sound or LocalTalk features):

- `SGLUDDSP.h` — OSS `/dev/dsp` sound
- `SGLUALSA.h` — ALSA sound
- `LTOVRBPF.h` — LocalTalk over BPF
- `LTOVRUDP.h` — LocalTalk over UDP

---

## Steps

### Step 3.1 — Create Directory Skeleton

Create the target directory structure with empty placeholder files (git doesn't track empty directories):

```
src/core/
src/cpu/
src/devices/
src/platform/
src/platform/common/
src/config/
src/lang/
src/resources/
src/unused/
```

**Changes:**
- Create each directory with a `.gitkeep` if needed (or rely on the files we move in subsequent steps).
- No source changes, no build changes.

**Verification:** `cmake --build --preset macos-cocoa` — still works (no files moved yet).

**Commit:** `Create directory skeleton for Phase 3 restructuring`

---

### Step 3.2 — Move Config Files: `cfg/` → `src/config/`

Move the static config headers and associated files from `cfg/` into `src/config/`.

**Moves:**
```
git mv cfg/CNFUIALL.h   src/config/CNFUIALL.h
git mv cfg/CNFUIPIC.h   src/config/CNFUIPIC.h
git mv cfg/CNFUIOSG.h   src/config/CNFUIOSG.h
git mv cfg/CNFUDALL.h   src/config/CNFUDALL.h
git mv cfg/CNFUDOSG.h   src/config/CNFUDOSG.h
git mv cfg/CNFUDPIC.h   src/config/CNFUDPIC.h
git mv cfg/STRCONST.h   src/config/STRCONST.h
git mv cfg/Info.plist    src/config/Info.plist
git mv cfg/English.lproj src/config/English.lproj
```

**CMakeLists.txt changes:**
- Update include path: `"${CMAKE_SOURCE_DIR}/cfg"` → `"${CMAKE_SOURCE_DIR}/src/config"`
- Update `CNFUIALL.h` / `CNFUIPIC.h` copy sources
- Update `Info.plist` path for `MACOSX_BUNDLE_INFO_PLIST`
- Update `English.lproj/dummy.txt` resource path

**No `#include` changes needed** — the config headers are found via include paths, not relative paths. The CMake-generated copies in `bld/cfg_generated/` are unaffected.

**Verification:**
```bash
rm -rf bld/macos-cocoa && cmake --preset macos-cocoa && cmake --build --preset macos-cocoa
```
Boot System 7 — works.

**Commit:** `Move cfg/ → src/config/`

---

### Step 3.3 — Move Language String Headers to `src/lang/`

**Moves:**
```
git mv src/STRCNENG.h  src/lang/strings_english.h
git mv src/STRCNFRE.h  src/lang/strings_french.h
git mv src/STRCNGER.h  src/lang/strings_german.h
git mv src/STRCNITA.h  src/lang/strings_italian.h
git mv src/STRCNSPA.h  src/lang/strings_spanish.h
git mv src/STRCNDUT.h  src/lang/strings_dutch.h
git mv src/STRCNPTB.h  src/lang/strings_portuguese.h
git mv src/STRCNPOL.h  src/lang/strings_polish.h
git mv src/STRCNCZE.h  src/lang/strings_czech.h
git mv src/STRCNSRL.h  src/lang/strings_serbian.h
git mv src/STRCNCAT.h  src/lang/strings_catalan.h
```

**Include path changes:**
- These are included by `STRCONST.h` (in `cfg_generated/`) via a `#include "STRCNENG.h"` directive. After the rename, `STRCONST.h` must reference the new names.
- Update `cmake/STRCONST.h.in` template: change `#include "STRCN@lang@.h"` to `#include "lang/strings_@lang@.h"` (and update the CMake language map from `STRCNENG.h` → `lang/strings_english.h`, etc.).
- Also update `src/config/STRCONST.h` (the static fallback): change `#include "STRCNENG.h"` → `#include "lang/strings_english.h"`.

**CMakeLists.txt changes:**
- Update the `_lang_map_*` variables to use the new filenames.
- Add `"${CMAKE_SOURCE_DIR}/src"` as an include path (it's already there — `src/lang/` is found via `src/` + `lang/strings_english.h` relative path).

**Verification:** Clean build + boot.

**Commit:** `Move and rename language string headers to src/lang/`

---

### Step 3.4 — Move Resource Files to `src/resources/`

**Moves:**
```
git mv src/ICONAPPO.icns  src/resources/ICONAPPO.icns
git mv src/ICONDSKO.icns  src/resources/ICONDSKO.icns
git mv src/ICONROMO.icns  src/resources/ICONROMO.icns
git mv src/ICONAPPW.ico   src/resources/ICONAPPW.ico
git mv src/ICONDSKW.ico   src/resources/ICONDSKW.ico
git mv src/ICONROMW.ico   src/resources/ICONROMW.ico
git mv src/ICONAPPM.r     src/resources/ICONAPPM.r
git mv src/ICONDSKM.r     src/resources/ICONDSKM.r
git mv src/ICONROMM.r     src/resources/ICONROMM.r
git mv src/main.r         src/resources/main.r
```

**CMakeLists.txt changes:**
- Update the icon resource path: `src/ICONAPPO.icns` → `src/resources/ICONAPPO.icns`

**Verification:** Build + boot. App icon still shows in Dock.

**Commit:** `Move resource files to src/resources/`

---

### Step 3.5 — Move Orphaned Files to `src/unused/`

**Moves:**
```
git mv src/SGLUDDSP.h   src/unused/SGLUDDSP.h
git mv src/SGLUALSA.h   src/unused/SGLUALSA.h
git mv src/LTOVRBPF.h   src/unused/LTOVRBPF.h
git mv src/LTOVRUDP.h   src/unused/LTOVRUDP.h
```

These files have zero `#include` references. No build changes needed.

**Verification:** Build — trivially passes (no references to update).

**Commit:** `Move orphaned/unused headers to src/unused/`

---

### Step 3.6 — Move and Rename Core Files

Move the emulation core files to `src/core/`.

**Moves + renames:**
```
git mv src/GLOBGLUE.c   src/core/machine.cpp
git mv src/GLOBGLUE.h   src/core/machine.h
git mv src/PROGMAIN.c   src/core/main.cpp
git mv src/PROGMAIN.h   src/core/main.h
git mv src/ENDIANAC.h   src/core/endian.h
git mv src/DFCNFCMP.h   src/core/defaults.h
git mv src/PICOMMON.h   src/core/common.h
```

**`#include` updates (same commit):**

Every `.c` file includes `"PICOMMON.h"` → change to `"core/common.h"`. Every OSGLU backend includes headers from the chain. Update all references:

| Old Include | New Include | Files Affected |
|-------------|-------------|----------------|
| `"PICOMMON.h"` | `"core/common.h"` | All 21 `.c` compilation units |
| `"GLOBGLUE.h"` | `"core/machine.h"` | `PICOMMON.h` (now `core/common.h`), `OSGLUCCO.m`, `OSGLUSDL.c` |
| `"PROGMAIN.h"` | `"core/main.h"` | `OSGLUCCO.m`, `OSGLUSDL.c` |
| `"ENDIANAC.h"` | `"core/endian.h"` | `PICOMMON.h` → `core/common.h`, `OSGCOMUI.h` |
| `"DFCNFCMP.h"` | `"core/defaults.h"` | `PICOMMON.h` → `core/common.h`, `OSGCOMUI.h` |

Also update internal includes within `core/common.h` itself (the old `PICOMMON.h`):
```cpp
#include "CNFUIALL.h"      // unchanged — found via include path
#include "CNFUIPIC.h"      // unchanged
#include "core/defaults.h" // was DFCNFCMP.h
#include "core/endian.h"   // was ENDIANAC.h
#include "CNFUDALL.h"      // unchanged
#include "platform/platform.h"  // NOT YET — OSGLUAAA.h hasn't moved yet
#include "CNFUDPIC.h"      // unchanged
#include "core/machine.h"  // was GLOBGLUE.h
```

**Important:** `OSGLUAAA.h` hasn't moved yet (that's Step 3.8). In this step, update `core/common.h` to include `"OSGLUAAA.h"` (still found via `src/` include path). It gets updated to `"platform/platform.h"` in Step 3.8.

**CMakeLists.txt changes:**
- Update source file list: `src/GLOBGLUE.c` → `src/core/machine.cpp`, `src/PROGMAIN.c` → `src/core/main.cpp`
- Remove `LANGUAGE CXX` override for these files (they're already `.cpp`)

**Verification:** Clean build + boot.

**Commit:** `Move and rename core files to src/core/`

---

### Step 3.7 — Move and Rename CPU Files

**Moves + renames:**
```
git mv src/MINEM68K.c   src/cpu/m68k.cpp
git mv src/MINEM68K.h   src/cpu/m68k.h
git mv src/M68KITAB.c   src/cpu/m68k_tables.cpp
git mv src/M68KITAB.h   src/cpu/m68k_tables.h
git mv src/DISAM68K.c   src/cpu/disasm.cpp
git mv src/DISAM68K.h   src/cpu/disasm.h
git mv src/FPCPEMDV.h   src/cpu/fpu_emdev.h
git mv src/FPMATHEM.h   src/cpu/fpu_math.h
```

**`#include` updates:**

| Old Include | New Include | Files Affected |
|-------------|-------------|----------------|
| `"MINEM68K.h"` | `"cpu/m68k.h"` | `MINEM68K.c` → `cpu/m68k.cpp` (self), `core/main.cpp`, `core/machine.cpp` |
| `"M68KITAB.h"` | `"cpu/m68k_tables.h"` | `cpu/m68k.cpp` |
| `"DISAM68K.h"` | `"cpu/disasm.h"` | `cpu/m68k.cpp` |
| `"FPCPEMDV.h"` | `"cpu/fpu_emdev.h"` | `cpu/m68k.cpp` (line ~8216) |
| `"FPMATHEM.h"` | `"cpu/fpu_math.h"` | `cpu/m68k.cpp` (line ~8215) |

**CMakeLists.txt changes:**
- Update source list: `src/MINEM68K.c` → `src/cpu/m68k.cpp`, `src/M68KITAB.c` → `src/cpu/m68k_tables.cpp`, `src/DISAM68K.c` → `src/cpu/disasm.cpp`

**Verification:** Clean build + boot.

**Commit:** `Move and rename CPU files to src/cpu/`

---

### Step 3.8 — Move and Rename Device Files

**Moves + renames (16 devices × 2 files = 32 file moves):**

```
git mv src/VIAEMDEV.c   src/devices/via.cpp
git mv src/VIAEMDEV.h   src/devices/via.h
git mv src/VIA2EMDV.c   src/devices/via2.cpp
git mv src/VIA2EMDV.h   src/devices/via2.h
git mv src/IWMEMDEV.c   src/devices/iwm.cpp
git mv src/IWMEMDEV.h   src/devices/iwm.h
git mv src/SCCEMDEV.c   src/devices/scc.cpp
git mv src/SCCEMDEV.h   src/devices/scc.h
git mv src/RTCEMDEV.c   src/devices/rtc.cpp
git mv src/RTCEMDEV.h   src/devices/rtc.h
git mv src/ROMEMDEV.c   src/devices/rom.cpp
git mv src/ROMEMDEV.h   src/devices/rom.h
git mv src/SCSIEMDV.c   src/devices/scsi.cpp
git mv src/SCSIEMDV.h   src/devices/scsi.h
git mv src/SONYEMDV.c   src/devices/sony.cpp
git mv src/SONYEMDV.h   src/devices/sony.h
git mv src/SCRNEMDV.c   src/devices/screen.cpp
git mv src/SCRNEMDV.h   src/devices/screen.h
git mv src/VIDEMDEV.c   src/devices/video.cpp
git mv src/VIDEMDEV.h   src/devices/video.h
git mv src/MOUSEMDV.c   src/devices/mouse.cpp
git mv src/MOUSEMDV.h   src/devices/mouse.h
git mv src/ADBEMDEV.c   src/devices/adb.cpp
git mv src/ADBEMDEV.h   src/devices/adb.h
git mv src/ASCEMDEV.c   src/devices/asc.cpp
git mv src/ASCEMDEV.h   src/devices/asc.h
git mv src/SNDEMDEV.c   src/devices/sound.cpp
git mv src/SNDEMDEV.h   src/devices/sound.h
git mv src/KBRDEMDV.c   src/devices/keyboard.cpp
git mv src/KBRDEMDV.h   src/devices/keyboard.h
git mv src/PMUEMDEV.c   src/devices/pmu.cpp
git mv src/PMUEMDEV.h   src/devices/pmu.h
```

Also move device-internal included headers:
```
git mv src/ADBSHARE.h   src/devices/adb_shared.h
git mv src/SCRNHACK.h   src/devices/screen_hack.h
git mv src/HPMCHACK.h   src/devices/hpmac_hack.h
```

**`#include` updates:**

Each device `.c` file includes `"PICOMMON.h"` (already updated to `"core/common.h"` in Step 3.6) and its own header. Update the self-include and cross-includes:

| Old Include | New Include | Files Affected |
|-------------|-------------|----------------|
| `"VIAEMDEV.h"` | `"devices/via.h"` | `devices/via.cpp`, `core/main.cpp`, `core/machine.cpp` |
| `"VIA2EMDV.h"` | `"devices/via2.h"` | `devices/via2.cpp`, `core/main.cpp`, `core/machine.cpp` |
| `"IWMEMDEV.h"` | `"devices/iwm.h"` | etc. |
| `"SCCEMDEV.h"` | `"devices/scc.h"` | etc. |
| `"RTCEMDEV.h"` | `"devices/rtc.h"` | etc. |
| `"ROMEMDEV.h"` | `"devices/rom.h"` | etc. |
| `"SCSIEMDV.h"` | `"devices/scsi.h"` | etc. |
| `"SONYEMDV.h"` | `"devices/sony.h"` | etc. |
| `"SCRNEMDV.h"` | `"devices/screen.h"` | etc. |
| `"VIDEMDEV.h"` | `"devices/video.h"` | etc. |
| `"MOUSEMDV.h"` | `"devices/mouse.h"` | etc. |
| `"ADBEMDEV.h"` | `"devices/adb.h"` | etc. |
| `"ASCEMDEV.h"` | `"devices/asc.h"` | etc. |
| `"SNDEMDEV.h"` | `"devices/sound.h"` | etc. |
| `"KBRDEMDV.h"` | `"devices/keyboard.h"` | etc. |
| `"PMUEMDEV.h"` | `"devices/pmu.h"` | etc. |
| `"ADBSHARE.h"` | `"devices/adb_shared.h"` | `devices/adb.cpp` |
| `"SCRNHACK.h"` | `"devices/screen_hack.h"` | `devices/rom.cpp` |
| `"HPMCHACK.h"` | `"devices/hpmac_hack.h"` | `devices/rom.cpp` |

**Where device headers are included:**
- Each device `.cpp` includes its own `.h` (self-include — straightfoward)
- `core/main.cpp` (was `PROGMAIN.c`) includes ALL device headers for `InitEmulation()`/`ResetEmulation()`
- `core/machine.cpp` (was `GLOBGLUE.c`) includes several device headers for `MMDV_Access()` dispatch
- Some devices cross-reference each other (e.g., VIA includes references to SCC, IWM, etc. — but these are via `core/machine.h` externs, not direct includes)

**CMakeLists.txt changes:**
- Replace all 16 `src/XXXEMDEV.c` entries with `src/devices/*.cpp` equivalents

**Verification:** Clean build + boot.

**Commit:** `Move and rename device files to src/devices/`

---

### Step 3.9 — Move and Rename Platform Files

**Moves + renames:**

```
git mv src/OSGLUAAA.h   src/platform/platform.h
git mv src/OSGLUCCO.m   src/platform/cocoa.mm
git mv src/OSGLUSDL.c   src/platform/sdl.cpp
git mv src/LOCALTLK.h   src/platform/localtalk.h

git mv src/OSGCOMUI.h   src/platform/common/osglu_ui.h
git mv src/OSGCOMUD.h   src/platform/common/osglu_ud.h
git mv src/COMOSGLU.h   src/platform/common/osglu_common_impl.h
git mv src/CONTROLM.h   src/platform/common/control_mode_impl.h
git mv src/PBUFSTDC.h   src/platform/common/param_buffers_impl.h
git mv src/INTLCHAR.h   src/platform/common/intl_chars_impl.h
git mv src/DATE2SEC.h   src/platform/common/date_to_sec.h
git mv src/SCRNMAPR.h   src/platform/common/screen_map.h
git mv src/SCRNTRNS.h   src/platform/common/screen_translate.h
git mv src/ALTKEYSM.h   src/platform/common/alt_keys.h
git mv src/ACTVCODE.h   src/platform/common/actv_code.h
```

**Note:** In this step, `COMOSGLU.h`, `CONTROLM.h`, `PBUFSTDC.h`, and `INTLCHAR.h` are renamed with an `_impl.h` suffix to clearly signal they are still code-as-headers (implementation included via `#include`). They are **not yet converted** to separate compilation units — that happens in Steps 3.11–3.14.

**`#include` updates:**

| Old Include | New Include | Files Affected |
|-------------|-------------|----------------|
| `"OSGLUAAA.h"` | `"platform/platform.h"` | `core/common.h`, `platform/common/osglu_ud.h` |
| `"OSGCOMUI.h"` | `"platform/common/osglu_ui.h"` | `platform/cocoa.mm`, `platform/sdl.cpp` |
| `"OSGCOMUD.h"` | `"platform/common/osglu_ud.h"` | `platform/cocoa.mm`, `platform/sdl.cpp` |
| `"COMOSGLU.h"` | `"platform/common/osglu_common_impl.h"` | `platform/cocoa.mm`, `platform/sdl.cpp` |
| `"CONTROLM.h"` | `"platform/common/control_mode_impl.h"` | `platform/cocoa.mm`, `platform/sdl.cpp` |
| `"PBUFSTDC.h"` | `"platform/common/param_buffers_impl.h"` | `platform/cocoa.mm`, `platform/sdl.cpp` |
| `"INTLCHAR.h"` | `"platform/common/intl_chars_impl.h"` | `platform/cocoa.mm`, `platform/sdl.cpp` |
| `"DATE2SEC.h"` | `"platform/common/date_to_sec.h"` | (not used in active backends) |
| `"SCRNMAPR.h"` | `"platform/common/screen_map.h"` | `platform/cocoa.mm`, `platform/sdl.cpp` |
| `"SCRNTRNS.h"` | `"platform/common/screen_translate.h"` | `platform/cocoa.mm` |
| `"ALTKEYSM.h"` | `"platform/common/alt_keys.h"` | `platform/common/control_mode_impl.h` |
| `"ACTVCODE.h"` | `"platform/common/actv_code.h"` | `platform/common/control_mode_impl.h` |
| `"LOCALTLK.h"` | `"platform/localtalk.h"` | `platform/cocoa.mm` |
| `"PROGMAIN.h"` in backends | `"core/main.h"` | `platform/cocoa.mm`, `platform/sdl.cpp` (if not already done in 3.6) |

**CMakeLists.txt changes:**
- Update backend source paths: `src/OSGLUCCO.m` → `src/platform/cocoa.mm`, `src/OSGLUSDL.c` → `src/platform/sdl.cpp`
- `.mm` files no longer need the `LANGUAGE OBJCXX` override (they're natively ObjC++)
- `.cpp` files no longer need `LANGUAGE CXX` override

Also move the 7 non-active platform backends (they're not in the build, but move them for cleanliness):
```
git mv src/OSGLUOSX.c   src/platform/carbon.cpp
git mv src/OSGLUXWN.c   src/platform/x11.cpp
git mv src/OSGLUGTK.c   src/platform/gtk.cpp
git mv src/OSGLUWIN.c   src/platform/win32.cpp
git mv src/OSGLUDOS.c   src/platform/dos.cpp
git mv src/OSGLUNDS.c   src/platform/nds.cpp
git mv src/OSGLUMAC.c   src/platform/classic_mac.cpp
```

These don't compile, so no `#include` fixup is needed for them now. Their internal includes can be updated in a future cleanup pass if/when they're re-enabled.

**Verification:** Clean build + boot.

**Commit:** `Move and rename platform files to src/platform/`

---

### Step 3.10 — Update CMake to Use New Paths; Remove `LANGUAGE` Overrides

At this point all files are in their final locations. Clean up the CMakeLists.txt:

**Changes:**
1. Replace the source list with the new paths, organized by subdirectory:
   ```cmake
   set(MINIVMAC_SOURCES
       # Core
       src/core/machine.cpp
       src/core/main.cpp
       # CPU
       src/cpu/m68k.cpp
       src/cpu/m68k_tables.cpp
       src/cpu/disasm.cpp
       # Devices
       src/devices/via.cpp
       src/devices/via2.cpp
       src/devices/iwm.cpp
       src/devices/scc.cpp
       src/devices/rtc.cpp
       src/devices/rom.cpp
       src/devices/scsi.cpp
       src/devices/sony.cpp
       src/devices/screen.cpp
       src/devices/video.cpp
       src/devices/mouse.cpp
       src/devices/adb.cpp
       src/devices/asc.cpp
       src/devices/sound.cpp
       src/devices/keyboard.cpp
       src/devices/pmu.cpp
   )
   ```
2. Backend adds `src/platform/cocoa.mm` or `src/platform/sdl.cpp`
3. Remove the `foreach` loop that sets `LANGUAGE CXX` on `.c` files — all sources are now `.cpp` or `.mm`
4. Update include directories:
   ```cmake
   target_include_directories(minivmac PRIVATE
       "${MINIVMAC_GENERATED_CFG_DIR}"
       "${CMAKE_SOURCE_DIR}/src/config"
       "${CMAKE_SOURCE_DIR}/src"
   )
   ```
5. Update resource paths for app bundle

**Verification:** Clean build from scratch + boot.

**Commit:** `Update CMakeLists.txt for new directory structure; remove LANGUAGE overrides`

---

### Step 3.11 — Convert `COMOSGLU.h` → `osglu_common.cpp` + `osglu_common.h`

This is the first and most impactful code-as-header conversion. `COMOSGLU.h` (1,390 lines) contains global variable definitions and function implementations that are `#include`'d into each backend file.

**Strategy:**

1. **Extract declarations** into `src/platform/common/osglu_common.h`:
   - All `extern` variable declarations (ROM, vSonyWritableMask, CurMacDateInSeconds, CLUT arrays, timing variables, etc.)
   - All function prototypes
   - Keep the `#define` / `#if` guards that control feature availability (EnableFSMouseMotion, etc.)

2. **Move definitions** into `src/platform/common/osglu_common.cpp`:
   - All variable definitions (with initializers)
   - All function bodies
   - This file includes `"platform/common/osglu_ui.h"`, `"platform/common/osglu_ud.h"`, and `"platform/common/osglu_common.h"`

3. **In backends** (`platform/cocoa.mm`, `platform/sdl.cpp`):
   - Replace `#include "platform/common/osglu_common_impl.h"` with `#include "platform/common/osglu_common.h"`

4. **Add `osglu_common.cpp` to CMakeLists.txt** as a compiled source

**Caution — tricky patterns:**
- `COMOSGLU.h` defines global variables like `uint8_t * ROM = nullptr;` — these become `extern uint8_t * ROM;` in the header and `uint8_t * ROM = nullptr;` in the `.cpp`.
- Variables inside `#if` blocks (e.g., `#if IncludeSonyNew`) must keep their guards in both header and source.
- Functions that reference backend-specific symbols: scan for any symbols provided by the backend that `COMOSGLU.h` calls. These need forward declarations or an interface header. **Key concern:** `COMOSGLU.h` should only depend on `OSGLUAAA.h` contracts — verify this is the case. If it calls backend-specific functions, those need to become part of the platform interface.
- The `_impl.h` file can be deleted after this step (or kept as a redirect for non-active backends).

**Verification:** Clean build + boot. Check that ROM loads, disk operations work, timing is correct.

**Commit:** `Convert COMOSGLU.h from code-as-header to osglu_common.cpp compilation unit`

---

### Step 3.12 — Convert `CONTROLM.h` → `control_mode.cpp` + `control_mode.h`

`CONTROLM.h` (1,420 lines) implements the in-emulator control mode UI (the overlay menu when you press Ctrl+Enter). It also `#include`s `ALTKEYSM.h` and `ACTVCODE.h`.

**Strategy:**

1. **Extract declarations** → `src/platform/common/control_mode.h`:
   - The `SpclMode` enum
   - Function prototypes for `DoControlModeStuff()`, `MacMsgOverride()`, etc.
   - Export any `SpecialModes` / `NeedWholeScreenDraw` as `extern`

2. **Move definitions** → `src/platform/common/control_mode.cpp`:
   - All function bodies
   - Variable definitions
   - Keep the internal `#include "platform/common/alt_keys.h"` and `#include "platform/common/actv_code.h"`

3. **In backends:** Replace `#include "platform/common/control_mode_impl.h"` with `#include "platform/common/control_mode.h"`

4. **Add to CMakeLists.txt** as a compiled source

**Caution:**
- `CONTROLM.h` references screen-drawing functions that may be backend-specific. Audit carefully and ensure they go through the platform interface.
- The `ALTKEYSM.h` and `ACTVCODE.h` data tables stay as included headers inside `control_mode.cpp` — they're internal implementation details.

**Verification:** Clean build + boot. Test the control mode (Ctrl+click / Ctrl+Enter) — menus appear, ROM-not-found message displays correctly.

**Commit:** `Convert CONTROLM.h from code-as-header to control_mode.cpp compilation unit`

---

### Step 3.13 — Convert `PBUFSTDC.h` → `param_buffers.cpp` + `param_buffers.h`

`PBUFSTDC.h` (~200 lines) implements parameter buffer operations using stdio. Smaller and straightforward.

**Strategy:** Same as above — declarations to header, definitions to `.cpp`, update backends, add to CMake.

**Verification:** Clean build + boot.

**Commit:** `Convert PBUFSTDC.h from code-as-header to param_buffers.cpp compilation unit`

---

### Step 3.14 — Convert `INTLCHAR.h` → `intl_chars.cpp` + `intl_chars.h`

`INTLCHAR.h` (~300 lines) contains international character translation tables. Pure data — the simplest conversion.

**Strategy:** Same as above — data arrays move to `.cpp`, `extern` declarations in header.

**Verification:** Clean build + boot.

**Commit:** `Convert INTLCHAR.h from code-as-header to intl_chars.cpp compilation unit`

---

### Step 3.15 — Clean Up `_impl.h` Files and Verify Final State

After Steps 3.11–3.14, the `_impl.h` files are no longer included by the active backends. Clean them up:

1. **Delete** `osglu_common_impl.h`, `control_mode_impl.h`, `param_buffers_impl.h`, `intl_chars_impl.h` — they've been replaced by proper `.h`/`.cpp` pairs.
2. **Verify no remaining `#include` references** to the deleted files (only the inactive backends would reference them — those can be updated to include the new headers, or left broken since they don't compile anyway).
3. **Update inactive backends** (`src/platform/carbon.cpp`, etc.) to replace the old `#include` directives with comments noting the new locations. This is optional but aids future re-enablement.

**Verification:**
```bash
# Full clean build from scratch
rm -rf bld/macos-cocoa
cmake --preset macos-cocoa && cmake --build --preset macos-cocoa

# Check no old filenames remain in active include directives
grep -rn 'PICOMMON\|GLOBGLUE\|PROGMAIN\|ENDIANAC\|DFCNFCMP\|OSGLUAAA' \
  src/core/ src/cpu/ src/devices/ src/platform/cocoa.mm src/platform/sdl.cpp
# Should return zero matches

# Verify all old src/*.c and src/*.h files have been moved
ls src/*.c src/*.h src/*.m 2>/dev/null
# Should return "No such file" — all files now in subdirectories
```

Boot System 7 — verify full functionality:
- Boot to desktop
- Mouse and keyboard work
- Sound plays (startup chime)
- Floppy disk eject/insert works
- Control mode overlay works (Ctrl+click)
- Magnification toggle works

**Commit:** `Phase 3 cleanup: remove _impl.h shims, verify final directory structure`

---

### Step 3.16 — Update Documentation

1. Update `docs/BUILDING.md` to reference the new file locations.
2. Update `docs/INSIGHTS.md` if it references old filenames.
3. Add a brief "Source Tree" section to `README.md` showing the new layout.

**Verification:** Documentation reads correctly, all file references are valid.

**Commit:** `Update documentation for new directory structure`

---

## File Mapping Reference

Complete mapping from old names to new names:

### Core
| Old | New |
|-----|-----|
| `src/GLOBGLUE.c` | `src/core/machine.cpp` |
| `src/GLOBGLUE.h` | `src/core/machine.h` |
| `src/PROGMAIN.c` | `src/core/main.cpp` |
| `src/PROGMAIN.h` | `src/core/main.h` |
| `src/ENDIANAC.h` | `src/core/endian.h` |
| `src/DFCNFCMP.h` | `src/core/defaults.h` |
| `src/PICOMMON.h` | `src/core/common.h` |

### CPU
| Old | New |
|-----|-----|
| `src/MINEM68K.c` | `src/cpu/m68k.cpp` |
| `src/MINEM68K.h` | `src/cpu/m68k.h` |
| `src/M68KITAB.c` | `src/cpu/m68k_tables.cpp` |
| `src/M68KITAB.h` | `src/cpu/m68k_tables.h` |
| `src/DISAM68K.c` | `src/cpu/disasm.cpp` |
| `src/DISAM68K.h` | `src/cpu/disasm.h` |
| `src/FPCPEMDV.h` | `src/cpu/fpu_emdev.h` |
| `src/FPMATHEM.h` | `src/cpu/fpu_math.h` |

### Devices
| Old | New |
|-----|-----|
| `src/VIAEMDEV.c` / `.h` | `src/devices/via.cpp` / `.h` |
| `src/VIA2EMDV.c` / `.h` | `src/devices/via2.cpp` / `.h` |
| `src/IWMEMDEV.c` / `.h` | `src/devices/iwm.cpp` / `.h` |
| `src/SCCEMDEV.c` / `.h` | `src/devices/scc.cpp` / `.h` |
| `src/RTCEMDEV.c` / `.h` | `src/devices/rtc.cpp` / `.h` |
| `src/ROMEMDEV.c` / `.h` | `src/devices/rom.cpp` / `.h` |
| `src/SCSIEMDV.c` / `.h` | `src/devices/scsi.cpp` / `.h` |
| `src/SONYEMDV.c` / `.h` | `src/devices/sony.cpp` / `.h` |
| `src/SCRNEMDV.c` / `.h` | `src/devices/screen.cpp` / `.h` |
| `src/VIDEMDEV.c` / `.h` | `src/devices/video.cpp` / `.h` |
| `src/MOUSEMDV.c` / `.h` | `src/devices/mouse.cpp` / `.h` |
| `src/ADBEMDEV.c` / `.h` | `src/devices/adb.cpp` / `.h` |
| `src/ASCEMDEV.c` / `.h` | `src/devices/asc.cpp` / `.h` |
| `src/SNDEMDEV.c` / `.h` | `src/devices/sound.cpp` / `.h` |
| `src/KBRDEMDV.c` / `.h` | `src/devices/keyboard.cpp` / `.h` |
| `src/PMUEMDEV.c` / `.h` | `src/devices/pmu.cpp` / `.h` |
| `src/ADBSHARE.h` | `src/devices/adb_shared.h` |
| `src/SCRNHACK.h` | `src/devices/screen_hack.h` |
| `src/HPMCHACK.h` | `src/devices/hpmac_hack.h` |

### Platform
| Old | New |
|-----|-----|
| `src/OSGLUAAA.h` | `src/platform/platform.h` |
| `src/OSGLUCCO.m` | `src/platform/cocoa.mm` |
| `src/OSGLUSDL.c` | `src/platform/sdl.cpp` |
| `src/LOCALTLK.h` | `src/platform/localtalk.h` |
| `src/OSGCOMUI.h` | `src/platform/common/osglu_ui.h` |
| `src/OSGCOMUD.h` | `src/platform/common/osglu_ud.h` |
| `src/COMOSGLU.h` | `src/platform/common/osglu_common.cpp` + `.h` |
| `src/CONTROLM.h` | `src/platform/common/control_mode.cpp` + `.h` |
| `src/PBUFSTDC.h` | `src/platform/common/param_buffers.cpp` + `.h` |
| `src/INTLCHAR.h` | `src/platform/common/intl_chars.cpp` + `.h` |
| `src/DATE2SEC.h` | `src/platform/common/date_to_sec.h` |
| `src/SCRNMAPR.h` | `src/platform/common/screen_map.h` |
| `src/SCRNTRNS.h` | `src/platform/common/screen_translate.h` |
| `src/ALTKEYSM.h` | `src/platform/common/alt_keys.h` |
| `src/ACTVCODE.h` | `src/platform/common/actv_code.h` |

### Config
| Old | New |
|-----|-----|
| `cfg/CNFUIALL.h` | `src/config/CNFUIALL.h` |
| `cfg/CNFUIPIC.h` | `src/config/CNFUIPIC.h` |
| `cfg/CNFUIOSG.h` | `src/config/CNFUIOSG.h` |
| `cfg/CNFUDALL.h` | `src/config/CNFUDALL.h` |
| `cfg/CNFUDOSG.h` | `src/config/CNFUDOSG.h` |
| `cfg/CNFUDPIC.h` | `src/config/CNFUDPIC.h` |
| `cfg/STRCONST.h` | `src/config/STRCONST.h` |
| `cfg/Info.plist` | `src/config/Info.plist` |

### Language Strings
| Old | New |
|-----|-----|
| `src/STRCNENG.h` | `src/lang/strings_english.h` |
| `src/STRCNFRE.h` | `src/lang/strings_french.h` |
| `src/STRCNGER.h` | `src/lang/strings_german.h` |
| `src/STRCNITA.h` | `src/lang/strings_italian.h` |
| `src/STRCNSPA.h` | `src/lang/strings_spanish.h` |
| `src/STRCNDUT.h` | `src/lang/strings_dutch.h` |
| `src/STRCNPTB.h` | `src/lang/strings_portuguese.h` |
| `src/STRCNPOL.h` | `src/lang/strings_polish.h` |
| `src/STRCNCZE.h` | `src/lang/strings_czech.h` |
| `src/STRCNSRL.h` | `src/lang/strings_serbian.h` |
| `src/STRCNCAT.h` | `src/lang/strings_catalan.h` |

### Inactive Platform Backends
| Old | New |
|-----|-----|
| `src/OSGLUOSX.c` | `src/platform/carbon.cpp` |
| `src/OSGLUXWN.c` | `src/platform/x11.cpp` |
| `src/OSGLUGTK.c` | `src/platform/gtk.cpp` |
| `src/OSGLUWIN.c` | `src/platform/win32.cpp` |
| `src/OSGLUDOS.c` | `src/platform/dos.cpp` |
| `src/OSGLUNDS.c` | `src/platform/nds.cpp` |
| `src/OSGLUMAC.c` | `src/platform/classic_mac.cpp` |

### Unused/Orphaned
| Old | New |
|-----|-----|
| `src/SGLUDDSP.h` | `src/unused/SGLUDDSP.h` |
| `src/SGLUALSA.h` | `src/unused/SGLUALSA.h` |
| `src/LTOVRBPF.h` | `src/unused/LTOVRBPF.h` |
| `src/LTOVRUDP.h` | `src/unused/LTOVRUDP.h` |

---

## Commit Sequence

| # | Commit Message | Steps | Risk |
|---|---------------|-------|------|
| 1 | `Create directory skeleton for Phase 3 restructuring` | 3.1 | None |
| 2 | `Move cfg/ → src/config/` | 3.2 | Low — include paths only |
| 3 | `Move and rename language string headers to src/lang/` | 3.3 | Low — 1 template update |
| 4 | `Move resource files to src/resources/` | 3.4 | Low — 1 CMake path |
| 5 | `Move orphaned/unused headers to src/unused/` | 3.5 | None |
| 6 | `Move and rename core files to src/core/` | 3.6 | Medium — all files include common.h |
| 7 | `Move and rename CPU files to src/cpu/` | 3.7 | Low |
| 8 | `Move and rename device files to src/devices/` | 3.8 | Medium — many files, many includes |
| 9 | `Move and rename platform files to src/platform/` | 3.9 | Medium — complex include chains |
| 10 | `Update CMakeLists.txt for new directory structure` | 3.10 | Low |
| 11 | `Convert COMOSGLU.h from code-as-header to osglu_common.cpp` | 3.11 | **High** — largest refactor |
| 12 | `Convert CONTROLM.h from code-as-header to control_mode.cpp` | 3.12 | **High** — UI interaction code |
| 13 | `Convert PBUFSTDC.h from code-as-header to param_buffers.cpp` | 3.13 | Low — small file |
| 14 | `Convert INTLCHAR.h from code-as-header to intl_chars.cpp` | 3.14 | Low — pure data |
| 15 | `Phase 3 cleanup: remove _impl.h shims, verify final structure` | 3.15 | Low |
| 16 | `Update documentation for new directory structure` | 3.16 | None |

---

## Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|-----------|
| **`COMOSGLU.h` conversion breaks globals** — Variables like `ROM`, `vSonyWritableMask` are defined in the code-as-header and used by both the backend and the core | Linker errors (duplicate symbols or undefined symbols) | Carefully audit which symbols are used cross-TU. Use `extern` in headers, single definition in `osglu_common.cpp`. Compile incrementally. |
| **`CONTROLM.h` references backend-local functions** — The control mode may call backend-specific drawing or event functions | Compile errors after extraction | Audit call graph before extracting. Any backend-specific calls must go through function pointers or a virtual interface defined in `platform.h`. |
| **`SCRNMAPR.h` multi-include template breaks** if include path changes | Compile errors — functions not generated | The `#include "platform/common/screen_map.h"` path just needs to be correct. The template mechanism (`#define` + `#include` + `#undef` at end of file) is path-independent. Verify each generated function exists in the compiled output. |
| **Header include chain ordering** — `PICOMMON.h` and `OSGCOMUI.h`/`OSGCOMUD.h` establish a precise include order. Renaming could break the order. | Compile errors — missing types, undefined macros | Keep the include chains intact. Only change the `#include` paths, not the order. Verify after each step. |
| **Git history fragmentation** — `git mv` + content changes in same commit may confuse `git log --follow` | `git log --follow` fails to trace history | Do `git mv` and `#include` updates in the same commit (git handles this well if content change is <50%). Avoid other content changes in the same commit. |
| **`LANGUAGE CXX` removal causes ObjC++ issues** — `.mm` files might need special flags | Compile errors in Cocoa backend | `.mm` is natively ObjC++ — CMake handles it. Verify with a clean build. |
| **Inactive backends have broken includes** | No immediate impact (they don't compile) | Acceptable. Document in commit message. Fix when/if re-enabled. |

---

## Execution Dependencies

- **Phase 2 must be complete** — standard types, `#pragma once` guards, C++17 compilation.
- **Each commit is independently buildable** — the build must pass after every commit.
- **Steps 3.1–3.10 are safe renames** — pure mechanical changes, low risk.
- **Steps 3.11–3.14 are behavioral changes** — converting code-as-headers to separate compilation units changes linking behavior. These must be done carefully with thorough testing.
- **Step 3.10 can be merged into steps 3.6–3.9** if preferred (update CMake in each step rather than as a separate final step). The plan keeps it separate for clarity, but in practice it's better to update CMake paths as files are moved to keep the build green.

---

## Estimated Effort

| Steps | Description | Effort |
|-------|-------------|--------|
| 3.1–3.5 | Directory creation + simple moves (config, lang, resources, orphans) | ~1 hour |
| 3.6–3.9 | Core/CPU/devices/platform renames + include fixup | ~3 hours |
| 3.10 | CMake cleanup | ~30 minutes |
| 3.11–3.14 | Code-as-header → compilation unit conversions | ~4–6 hours |
| 3.15–3.16 | Cleanup + docs | ~1 hour |
| **Total** | | **~10–12 hours** |

---

## Post-Phase 3 State

After this phase:

```
src/
  core/       — machine.cpp, main.cpp, machine.h, main.h, common.h, endian.h, defaults.h
  cpu/        — m68k.cpp, m68k_tables.cpp, disasm.cpp + headers, fpu headers
  devices/    — 16 device .cpp/.h pairs + internal headers
  platform/   — cocoa.mm, sdl.cpp, platform.h, localtalk.h
    common/   — osglu_common.cpp/.h, control_mode.cpp/.h, param_buffers.cpp/.h,
                intl_chars.cpp/.h, screen_map.h, screen_translate.h, date_to_sec.h,
                osglu_ui.h, osglu_ud.h, alt_keys.h, actv_code.h
  config/     — 7 config headers + Info.plist + English.lproj/
  lang/       — 11 language string headers
  resources/  — icons and Classic Mac resources
  unused/     — 4 orphaned headers
```

- **86 files** organized into **8 directories** instead of 1 flat directory
- All filenames are **human-readable lowercase** (except config headers which retain their names for CMake template compatibility)
- **4 code-as-headers converted** to proper compilation units (COMOSGLU, CONTROLM, PBUFSTDC, INTLCHAR)
- **6 code-as-headers retained** as included templates (SCRNMAPR, SCRNTRNS, FPCPEMDV, FPMATHEM, ADBSHARE, DATE2SEC, SCRNHACK, HPMCHACK, ALTKEYSM, ACTVCODE)
- All `.c` → `.cpp`, `.m` → `.mm` — no more `LANGUAGE CXX` overrides in CMake
- The build produces an identical binary; the emulator boots System 7 with all features working
