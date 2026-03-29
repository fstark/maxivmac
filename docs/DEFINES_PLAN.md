# DEFINES Removal Plan — Phase 1: Code-Generation `#if` Guards

**Goal:** Remove preprocessor `#if`/`#ifdef`/`#ifndef` defines that conditionally
compile or exclude code blocks, replacing them with either always-compiled code
(when the value is constant) or runtime checks (when the value truly varies).

**Scope:** All `#define`s under `src/` (excluding `src/macsrc/`) that appear in
`#if`, `#ifdef`, or `#ifndef` guards and control which code is compiled in.

**Non-goals:** This plan does NOT touch:
- Constant/enum-like `#define`s (register offsets, buffer sizes, etc.)
- Macro functions (`get_ram_byte`, `POW_OF_2`, etc.)
- Include guards
- String constants
- Screen-mapper template parameters (`ScrnMapr_*`, `ScrnTrns_*`)

**Reference:** [DEFINES.md](DEFINES.md) for the full catalog.

---

## Summary

| Category | Defines | Total `#if` sites | Action | Status |
|----------|---------|-------------------|--------|--------|
| Always-1 emulation features | 9 | ~314 | Remove guard, keep the code | Open |
| Always-0 disabled features | 15 | ~83 | Remove guard AND the dead code | **Done** |
| Always-0 but keep for debug | 20 | ~155 | Keep (debug toggle) | Keep |
| CMake-configurable | 2 | ~71 | Keep or convert to runtime | Open |
| Platform-dependent | ~10 | ~22 | Keep (portability) | Keep |
| Derived/conditional | ~8 | ~20 | Remove after parent is resolved | Partial |

---

## Phase 1 — Always-On Constants (value = 1, never changes)

These defines are always `1`. The `#if 0` branches are dead code. Remove the
`#if`/`#else`/`#endif` scaffolding and keep only the "true" branch. Delete the
`#define` from its origin file.

### 1.1 `USE_68020` — always 1 (86 `#if` sites)

- **Defined:** `emulation_config.h:14`
- **Used in:** `m68k.cpp`, `m68k_tables.cpp`, `disasm.cpp`
- **Action:** Remove all `#if USE_68020` guards. Keep 68020 code, delete
  non-68020 fallback code. Runtime model selection already happens via
  `M68KITAB_setup()` dispatch table fixup.
- **Risk:** High volume — do in a dedicated pass. Run golden-file tests after.

### 1.2 `EM_FPU` — always 1 (12 `#if` sites)

- **Defined:** `emulation_config.h:15`
- **Used in:** `m68k.cpp`, `m68k_tables.cpp`, `fpu_68k.cpp`
- **Action:** Remove guards. FPU code is always compiled; runtime selection
  decides whether FPU instructions trap or execute.

### 1.3 `EM_MMU` — always 1 (10 `#if` sites)

- **Defined:** `emulation_config.h:16`
- **Used in:** `m68k.cpp`, `m68k_tables.cpp`
- **Action:** Same as EM_FPU.

### 1.4 `WANT_CYC_BY_PRI_OP` — always 1 (114 `#if` sites)

- **Defined:** `emulation_config.h:21`
- **Used in:** `m68k.cpp`, `machine.h` (for `RdAvgXtraCyc`/`WrAvgXtraCyc`)
- **Action:** Remove guards. Cycle-per-op accounting stays compiled-in.
  The `#else` branches are a simpler "no-cycle-tracking" path — delete them.
- **Risk:** Highest volume define. Do in its own pass.

### 1.5 `WANT_CLOSER_CYC` — always 1 (65 `#if` sites)

- **Defined:** `emulation_config.h:22`
- **Used in:** `m68k.cpp`
- **Action:** Remove guards. Closer-cycle code stays, fallback removed.

### 1.6 `WANT_DISASM` — always 1 (7 `#if` sites)

- **Defined:** `emulation_config.h:42`
- **Used in:** `machine.h`, `machine.cpp`, `m68k.cpp`
- **Action:** Remove guards. Disassembler always compiled in.

### 1.7 `INCLUDE_EXTN_PBUFS` — always 1 (8 `#if` sites)

- **Defined:** `emulation_config.h:26`
- **Used in:** `machine.h`, `machine.cpp`, `param_buffers.h`
- **Action:** Remove guards. Pbuf extension always compiled in.

### 1.8 `INCLUDE_EXTN_HOST_TEXT_CLIP_EXCHANGE` — always 1 (7 `#if` sites)

- **Defined:** `emulation_config.h:27`
- **Used in:** `machine.h`, `machine.cpp`
- **Action:** Remove guards. Clipboard extension always compiled in.

### 1.9 `SONY_SUPPORT_DC42` — always 1 (5 `#if` sites)

- **Defined:** `emulation_config.h:32`
- **Used in:** `sony.cpp`
- **Action:** Remove guards. DC42 always compiled in.

---

## Phase 2 — Always-On Constants (secondary, in other files)

### 2.1 `SONY_SUPPORT_TAGS` — always 1 (12 `#if` sites)

- **Defined:** `emulation_config.h:33`
- **Used in:** `sony.cpp`
- **Action:** Remove guards. Tag support always compiled in.

### 2.2 `SONY_WANT_CHECKSUMS_UPDATED` — always 1 (7 `#if` sites)

- **Defined:** `emulation_config.h:34`
- **Used in:** `sony.cpp`
- **Action:** Remove guards. Checksum update code always compiled in.

### 2.3 `USE_PCLIMIT` — always 1 (13 `#if` sites)

- **Defined:** `m68k.cpp:85`
- **Used in:** `m68k.cpp`
- **Action:** Remove guards. PC limit checking always active.

### 2.4 `UseLazyZ` — always 1 (9 `#if` sites)

- **Defined:** `m68k.cpp:58` (derived: 1 unless `DisableLazyFlagAll`)
- **Used in:** `m68k.cpp`
- **Action:** Remove guards once `DisableLazyFlagAll` is removed (Phase 3).

### 2.5 `UseLazyCC` — always 1 (7 `#if` sites)

- **Defined:** `m68k.cpp:66` (derived: 1 unless `DisableLazyFlagAll`)
- **Used in:** `m68k.cpp`
- **Action:** Same — remove after `DisableLazyFlagAll`.

### 2.6 `UseSonyPatch` — always 1 (4 `#if` sites)

- **Defined:** `rom.cpp:19`
- **Used in:** `rom.cpp`
- **Action:** Remove guards. Sony patch always applied.

### 2.7 `DisableRomCheck` — always 1 (2 `#if` sites)

- **Defined:** `rom.cpp:222`
- **Used in:** `rom.cpp`
- **Action:** Remove guards and the ROM-check code.

### 2.8 `DisableRamTest` — always 1 (2 `#if` sites)

- **Defined:** `rom.cpp:226`
- **Used in:** `rom.cpp`
- **Action:** Remove guards and the RAM-test code.

### 2.9 `RTCinitPRAM` — always 1 (2 `#if` sites)

- **Defined:** `rtc.cpp:57`
- **Used in:** `rtc.cpp`
- **Action:** Remove guards. PRAM init always happens.

### 2.10 `HAVE_WORKING_WARP` — always 1 (12 `#if` sites)

- **Defined:** `sdl.cpp:570`
- **Used in:** `sdl.cpp`
- **Action:** Remove guards. Warp always works on our SDL targets.

### 2.11 `USE_MOTION_EVENTS` — always 1 (2 `#if` sites)

- **Defined:** `sdl.cpp:745`
- **Used in:** `sdl.cpp`
- **Action:** Remove guards. Motion events always used.

### 2.12 `PbufHaveLock` — always 1 (1 `#if` site)

- **Defined:** `param_buffers.h:12`
- **Used in:** `param_buffers.h`
- **Action:** Remove guard. Lock support always compiled in.

### 2.13 `WantColorTransValid` — always 1 (4 `#if` sites)

- **Defined:** `osglu_common.h:98`
- **Used in:** `osglu_common.cpp`, `sdl.cpp`
- **Action:** Remove guards.

### 2.14 `ENABLE_FS_MOUSE_MOTION` — always 1 (18 `#if` sites)

- **Defined:** `osglu_common.h:9`
- **Used in:** `osglu_common.h`, `osglu_common.cpp`, `sdl.cpp`
- **Action:** Remove guards.

### 2.15 `ENABLE_RECREATE_W` — always 1 (6 `#if` sites)

- **Defined:** `osglu_common.h:10`
- **Used in:** `osglu_common.cpp`, `sdl.cpp`
- **Action:** Remove guards.

### 2.16 `ENABLE_MOVE_MOUSE` — always 1 (1 `#if` site)

- **Defined:** `osglu_common.h:11`
- **Used in:** `sdl.cpp`
- **Action:** Remove guard.

### 2.17 `GRAB_KEYS_FULL_SCREEN` — always 1 (3 `#if` sites)

- **Defined:** `osglu_common.h:14` (with `#ifndef` guard, but nothing overrides it)
- **Used in:** `osglu_common.cpp`, `sdl.cpp`
- **Action:** Remove guards.

### 2.18 `UseControlKeys` — always 1 (11 `#if` sites)

- **Defined:** `platform_config.h:41`
- **Used in:** `control_mode.h`, `intl_chars.h`, `control_mode.cpp`
- **Action:** Remove guards. Control keys always enabled.

### 2.19 `WantEnblCtrlInt` — always 1 (7 `#if` sites)

- **Defined:** `platform_config.h:38`
- **Used in:** `control_mode.h`, `intl_chars.h`, `control_mode.cpp`
- **Action:** Remove guards.

### 2.20 `WantEnblCtrlRst` — always 1 (7 `#if` sites)

- **Defined:** `platform_config.h:39`
- **Used in:** `control_mode.h`, `intl_chars.h`, `control_mode.cpp`
- **Action:** Remove guards.

### 2.21 `WantEnblCtrlKtg` — always 1 (4 `#if` sites)

- **Defined:** `platform_config.h:40`
- **Used in:** `control_mode.h`, `intl_chars.h`
- **Action:** Remove guards.

### 2.22 `SaveDialogEnable` — always 1 (0 `#if` sites, used in expressions)

- **Defined:** `platform_config.h:9`
- **Used in:** only assignment; no `#if` guards.
- **Action:** Delete the define; inline `true` where used.

### 2.23 `EnableDragDrop` — always 1 (0 `#if` sites, used in expressions)

- **Defined:** `sdl_config.h:14`
- **Action:** Same — delete and inline.

---

## Phase 3 — Always-Off Constants (value = 0, dead code) — COMPLETE

All 15 Phase 3 defines have been removed across 14 commits. Details in
git history. Three defines were compiled in unconditionally
(`SONY_VERIFY_CHECKSUMS`, `WantAutoScrollBorder`, `UseLargeScreenHack`);
the remaining 12 were removed along with their dead code.

| # | Define(s) | Action | Commit |
|---|-----------|--------|--------|
| 1 | `DisableLazyFlagAll` + `ForceFlagsEval`, `UseLazyZ`, `UseLazyCC` | Remove | e1895b1 |
| 2 | `HaveGlbReg` | Remove | 8d905f5 |
| 3 | `FasterAlignedL` | Remove | 597c407 |
| 4 | `EXTRA_ABNORMAL_REPORTS` | Remove | 487cefe |
| 5 | `SONY_VERIFY_CHECKSUMS` | Compile in | 4d44b35 |
| 6 | `GRAB_KEYS_MAX_FULL_SCREEN` | Remove | 3cabcce |
| 7 | `EnableAltKeysMode` | Remove | 9ac5be1 |
| 8 | `NeedIntlChars` | Remove | 9f07ecb |
| 9 | `WantInitRunInBackground` | Remove | dee6b2f |
| 10 | `MyAppIsBundle` | Remove | 1f41a61 |
| 11 | `WantAutoScrollBorder` | Compile in | f6ad866 |
| 12 | `UseLargeScreenHack` | Compile in | 7a5c6c0 |
| 13 | `C_INCLUDE_UNUSED`, `cIncludeFPUUnused` | Remove | 405cf05 |
| 14 | `NeedCell2WinAsciiMap` | Remove | 076540b |

---

## Phase 4 — Debug-Only Toggles (KEEP)

These defines are nominally `0` but are intentionally togglable by developers
for debugging. They should stay as `#if` guards. They are low-volume and
well-scoped to individual files.

| Define | Value | `#if` sites | File | Rationale |
|--------|-------|-------------|------|-----------|
| `SCC_dolog` | `dbglog_HAVE && 0` | 93 | `scc.cpp` | Debug toggle |
| `SCC_TrackMore` | `0` | 107 | `scc.cpp` | Deep debug toggle |
| `ASC_dolog` | `dbglog_HAVE && 0` | 30 | `asc.cpp` | Debug toggle |
| `VID_dolog` | `dbglog_HAVE && 0` | 27 | `video.cpp` | Debug toggle |
| `Sony_dolog` | `dbglog_HAVE && 0` | 21 | `sony.cpp` | Debug toggle |
| `IWM_dolog` | `dbglog_HAVE && 0` | 5 | `iwm.cpp` | Debug toggle |
| `m68k_logExceptions` | `dbglog_HAVE && 0` | 6 | `m68k.cpp` | Debug toggle |
| `dbglog_SoundStuff` | `0 && dbglog_HAVE` | 14 | `sdl_sound.cpp` | Debug toggle |
| `dbglog_SoundBuffStats` | `0 && dbglog_HAVE` | 4 | `sdl_sound.cpp` | Debug toggle |
| `DBGLOG_OSG_INIT` | `0 && dbglog_HAVE` | 4 | `sdl.cpp`, `tick_timer.cpp` | Debug toggle |
| `dbglog_TimeStuff` | `0 && dbglog_HAVE` | 2 | `tick_timer.cpp` | Debug toggle |
| `WantBreakPoint` | `0` | 4 | `m68k.cpp` | Debug toggle |
| `WantDumpTable` | `0` | 6 | `m68k.cpp` | Debug toggle |
| `WantDumpAJump` | `0` | 3 | `m68k.cpp` | Debug toggle |
| `DisasmIncludeCycles` | `0` | 4 | `disasm.cpp` | Debug toggle |
| `ReportAbnormalInterrupt` | `0` | 2 | `machine.cpp` | Debug toggle |
| `dbglog_ToStdErr` | `0` | 5 | `dbglog_platform.cpp` | Debug toggle |
| `dbglog_ToSDL_Log` | `0` | 2 | `dbglog_platform.cpp` | Debug toggle |
| `_VIA_Debug` | (undefined) | 17 | `ict_scheduler.cpp`, `keyboard.cpp`, `adb.cpp` | Debug toggle |
| `_RTC_Debug` | (undefined) | 7 | `rtc.cpp` | Debug toggle |

**Future:** Consider converting these to a runtime `--verbose=scc,asc,...`
flag instead of compile-time toggles, but that is out of scope for this plan.

---

## Phase 5 — CMake-Configurable Defines (KEEP or CONVERT)

### 5.1 `WantAbnormalReports` — CMake option (13 `#if` sites)

- **Defined:** CMake `MINIVMAC_ABNORMAL_REPORTS` → compile definition.
- **Used in:** `machine.h`, `machine.cpp`
- **Action:** **Keep for now.** Could become a runtime flag later. The `#if`
  guards compile out the entire abnormal-report infrastructure for release
  builds. Converting to runtime would add a branch per report site.

### 5.2 `EmLocalTalk` — CMake option (58 `#if` sites)

- **Defined:** CMake `MINIVMAC_LOCALTALK` → compile definition.
- **Used in:** `scc.cpp`, `scc.h`, `main.cpp`, `platform.h`,
  `osglu_common.cpp`
- **Action:** **Keep for now.** LocalTalk is a significant optional subsystem.
  The `#if EmLocalTalk` blocks include entire functions and data structures.
  Converting to runtime would bring in UDP networking code unconditionally.

### 5.3 `dbglog_HAVE` — CMake hardcoded to 1 (71 `#if` sites)

- **Defined:** CMake `target_compile_definitions`.
- **Used in:** everywhere (debug log infrastructure).
- **Action:** This is always 1 in the current build. The `#if dbglog_HAVE`
  guards were for old minivmac builds without logging.
  **Remove all guards and the `#define`.** The debug log subsystem is always
  present. The per-device `*_dolog` defines already gate actual output.
- **Note:** The device-specific debug defines (`SCC_dolog`, etc.) currently
  reference `dbglog_HAVE` in their expressions (`dbglog_HAVE && 0`). After
  removing `dbglog_HAVE`, rewrite those as plain `0`.

### 5.4 `LT_MayHaveEcho` — depends on `EmLocalTalk` (5 `#if` sites)

- **Used in:** `scc.cpp`, `platform.h`, `osglu_common.cpp`
- **Action:** Remove when `EmLocalTalk` is resolved. Currently scoped inside
  `#if EmLocalTalk` blocks, so removing it is safe only in that context.

---

## Phase 6 — Platform/Compiler-Dependent Defines (KEEP)

These vary by target platform or compiler. Keep them.

| Define | `#if` sites | Rationale |
|--------|-------------|-----------|
| `BIG_ENDIAN_UNALIGNED` | 7 | Platform-dependent byte order |
| `LITTLE_ENDIAN_UNALIGNED` | 12 | Platform-dependent byte order |
| `HAVE_ASR` | 2 | Compiler-dependent arithmetic shift |
| `HAVE_SWAP_UI5R` | 1 | Compiler-dependent byte swap |
| `FLOAT128` | 5 | Platform-dependent FPU width |
| `HaveUi5to6Mul` | 1 | Platform-dependent 64-bit math |
| `HaveUi6Div` | 1 | Platform-dependent 64-bit math |
| `BETTER_THAN_PENTIUM` | 3 | Platform-dependent optimisation |
| `_WIN32` | — | OS detection |
| `ln2mtb` | 6 | Memory-translation-block config |

**Future:** Some of these (`BIG_ENDIAN_UNALIGNED`, `HAVE_ASR`) could be
replaced with `constexpr` values auto-detected at compile time, but they
are low-priority.

---

## Phase 7 — Derived / Structural Defines (remove after parents)

These defines exist because of other defines. Once the parent is removed,
these become trivially constant and can be removed too.

| Define | Derives from | Sites | Action |
|--------|-------------|-------|--------|
| ~~`ForceFlagsEval`~~ | ~~`DisableLazyFlagAll`~~ | ~~5~~ | ~~Done (Phase 3, step 1)~~ |
| ~~`UseLazyZ`~~ | ~~`DisableLazyFlagAll`~~ | ~~9~~ | ~~Done (Phase 3, step 1)~~ |
| ~~`UseLazyCC`~~ | ~~`DisableLazyFlagAll`~~ | ~~7~~ | ~~Done (Phase 3, step 1)~~ |
| ~~`cIncludeFPUUnused`~~ | ~~`C_INCLUDE_UNUSED`~~ | ~~14~~ | ~~Done (Phase 3, step 13)~~ |
| `Sony_SupportOtherFormats` | `SONY_SUPPORT_DC42` | 0 | Remove after Phase 1.9 |
| `NeedDoMoreCommandsMsg` | `UseControlKeys` | 3 | Remove (→ 1) after Phase 2.18 |
| `NeedDoAboutMsg` | `UseControlKeys` | 3 | Remove (→ 1) after Phase 2.18 |
| `NeedRequestInsertDisk` | always 1 | 3 | Remove |
| `NeedRequestIthDisk` | always 1 | 3 | Remove |
| `NeedCell2MacAsciiMap` | defaults to 1 | 3 | Remove |
| `NeedCell2PlainAsciiMap` | defaults to 1 | 3 | Remove |
| `NeedCell2UnicodeMap` | defaults to 1 | 3 | Remove |

---

## Execution Order

Each phase should be done in a separate commit (or series of commits) with a
compile-and-test gate. The golden-file tests (`cd test && ./verify.sh`) must
pass after every phase.

### Completed

| Step | Phase | Defines removed | Status |
|------|-------|----------------|--------|
| 1 | 3.12+3.13 | `DisableLazyFlagAll`, `ForceFlagsEval`, `UseLazyZ`, `UseLazyCC` | **Done** |
| 2 | 3.10 | `HaveGlbReg` | **Done** |
| 3 | 3.11 | `FasterAlignedL` | **Done** |
| 4 | 3.1 | `EXTRA_ABNORMAL_REPORTS` | **Done** |
| 5 | 3.2 | `SONY_VERIFY_CHECKSUMS` (compiled in) | **Done** |
| 6 | 3.3 | `GRAB_KEYS_MAX_FULL_SCREEN` | **Done** |
| 7 | 3.4 | `EnableAltKeysMode` | **Done** |
| 8 | 3.5 | `NeedIntlChars` | **Done** |
| 9 | 3.6 | `WantInitRunInBackground` | **Done** |
| 10 | 3.7 | `MyAppIsBundle` | **Done** |
| 11 | 3.8 | `WantAutoScrollBorder` (compiled in) | **Done** |
| 12 | 3.9 | `UseLargeScreenHack` (compiled in) | **Done** |
| 13 | 3.14 | `C_INCLUDE_UNUSED`, `cIncludeFPUUnused` | **Done** |
| 14 | 3.15 | `NeedCell2WinAsciiMap` | **Done** |

### Remaining

| Step | Phase | Defines to remove | Est. `#if` sites | Risk |
|------|-------|-------------------|-----------------|------|
| 15 | 2.3 | `USE_PCLIMIT` | 13 | Low |
| 16 | 1.6 | `WANT_DISASM` | 7 | Low |
| 17 | 1.7+1.8 | `INCLUDE_EXTN_PBUFS`, `INCLUDE_EXTN_HOST_TEXT_CLIP_EXCHANGE` | 15 | Low |
| 18 | 1.9+2.1–2.2 | `SONY_SUPPORT_DC42`, `SONY_SUPPORT_TAGS`, `SONY_WANT_CHECKSUMS_UPDATED` | 24 | Low |
| 19 | 1.2+1.3 | `EM_FPU`, `EM_MMU` | 22 | Low |
| 20 | 2.6–2.9 | `UseSonyPatch`, `DisableRomCheck`, `DisableRamTest`, `RTCinitPRAM` | 10 | Low |
| 21 | 2.10+2.11+2.12 | `HAVE_WORKING_WARP`, `USE_MOTION_EVENTS`, `PbufHaveLock` | 15 | Low |
| 22 | 2.13–2.17 | `WantColorTransValid`, `ENABLE_FS_MOUSE_MOTION`, `ENABLE_RECREATE_W`, `ENABLE_MOVE_MOUSE`, `GRAB_KEYS_FULL_SCREEN` | 28 | Low |
| 23 | 2.18–2.21 | `UseControlKeys`, `WantEnblCtrlInt/Rst/Ktg` | 29 | Low |
| 24 | 2.22+2.23 | `SaveDialogEnable`, `EnableDragDrop` | ~4 | Low |
| 25 | 5.3 | `dbglog_HAVE` | 71 | Medium |
| 26 | 1.1 | `USE_68020` | 86 | High |
| 27 | 1.4 | `WANT_CYC_BY_PRI_OP` | 114 | High |
| 28 | 1.5 | `WANT_CLOSER_CYC` | 65 | High |
| 29 | 7 (rest) | Remaining derived/structural | ~20 | Low |

**Phase 3 total: 15 defines removed, ~83 `#if` sites cleaned, 14 commits.**
**Remaining: ~38 defines, ~547 `#if` sites.**

---

## Kept Defines (not removed by this plan)

| Category | Count | Reason |
|----------|-------|--------|
| Debug toggles (Phase 4) | 20 | Intentional developer knobs |
| CMake options (`WantAbnormalReports`, `EmLocalTalk`) | 2 | Genuine build variation |
| Platform/compiler (`BIG_ENDIAN_UNALIGNED`, etc.) | ~10 | Portability |
| `CurAltHappyMac` | 1 | Fun Easter-egg build option |
| `dbglog_buflnsz` | 1 | Optional buffered logging tuning |
| Screen-mapper template params | ~12 | Template instantiation mechanism |

---

## Validation

After each step:
1. `cmake --build build` — must compile clean (no warnings from removed code).
2. `cd test && ./verify.sh` — golden-file regression tests must pass.
3. `grep -rn '#if.*REMOVED_DEFINE' src/` — verify no stale references.
