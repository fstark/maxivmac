# Phase 3 — Always-Off Defines: Execution Plan

Self-contained step-by-step plan for removing all Phase 3 defines.
Each step ends with **build → test → commit → push**.

**Gate commands (run after every step):**

```sh
cmake --build --preset macos
cd test && ./verify.sh
```

**Commit template:** `Remove <DEFINE>: <summary>`

---

## Step 1 — `DisableLazyFlagAll` + derived cascade (3.12 + 3.13)

Remove the master debug switch and its three derived defines. This is a
single logical group that must be removed together.

### 1a. Remove `DisableLazyFlagAll` definition + conditional block

**File:** `src/cpu/m68k.cpp` lines 37–69

Delete the entire block that defines `DisableLazyFlagAll`, `ForceFlagsEval`,
`UseLazyZ`, and `UseLazyCC`:

```cpp
#ifndef DisableLazyFlagAll
#define DisableLazyFlagAll 0
#endif
    /* ... comment ... */

#ifndef ForceFlagsEval
#if DisableLazyFlagAll
#define ForceFlagsEval 1
#else
#define ForceFlagsEval 0
#endif
#endif

#ifndef UseLazyZ
#if DisableLazyFlagAll || ForceFlagsEval
#define UseLazyZ 0
#else
#define UseLazyZ 1
#endif
#endif

#ifndef UseLazyCC
#if DisableLazyFlagAll
#define UseLazyCC 0
#else
#define UseLazyCC 1
#endif
#endif
```

### 1b. Remove `#if ForceFlagsEval` sites

**File:** `src/cpu/m68k.cpp`

Three `#if ForceFlagsEval` blocks (lines ~3482, ~3873, ~3888). Since
`ForceFlagsEval` = 0, keep only the `#else` branch (or nothing if no `#else`).

- **Line ~3482:** `NeedDefaultLazyAllFlags` — `#if ForceFlagsEval` has a
  function body; `#else` has a `#define` macro. Keep the `#define`.
- **Line ~3873:** `NeedDefaultLazyXFlag` — same pattern. Keep the `#define`.
- **Line ~3888:** `HaveSetUpFlags` — `#if ForceFlagsEval` has a value;
  `#else` has a different value. Keep the `#else` value.

### 1c. Remove `#if UseLazyZ` sites (resolve to 1 = keep code)

**File:** `src/cpu/m68k.cpp`

Sites: lines ~119, ~155, ~3307, ~3473, ~3832, ~3861, ~3894, ~5688.
At each site, remove the `#if UseLazyZ` / `#endif` scaffolding and keep
the guarded code. If there's an `#else` branch, delete it.

### 1d. Remove `#if UseLazyCC` sites (resolve to 1 = keep code)

**File:** `src/cpu/m68k.cpp`

Sites: lines ~2322, ~2923, ~2949, ~3330, ~3906, ~3914.
Same treatment: remove guards, keep the code. Note line ~3914 uses
`#if ! UseLazyCC` — that block is dead code (since UseLazyCC = 1), delete it.

### Gate

```sh
cmake --build --preset macos
cd test && ./verify.sh
git add -A && git commit -m "Remove DisableLazyFlagAll + ForceFlagsEval/UseLazyZ/UseLazyCC cascade"
git push
```

---

## Step 2 — `HaveGlbReg` (3.10)

Remove the global-register optimization that is never used.

### 2a. Remove definition

**File:** `src/cpu/m68k.cpp` lines 136–137

Delete:
```cpp
#ifndef HaveGlbReg
#define HaveGlbReg 0
#endif
```

### 2b. Remove guarded code blocks

**File:** `src/cpu/m68k.cpp`

Five `#if HaveGlbReg` sites (lines ~8170, ~8204, ~8210, ~8216, ~8231).
Since `HaveGlbReg` = 0, delete all `#if HaveGlbReg` true-branches and keep
`#else` branches (which define `Em_Enter`/`Em_Exit` as no-ops and
`LocalMemAccessNtfy`/`LocalMMDV_Access` as direct aliases).

After cleanup, the `#else` macros become unconditional definitions:
```cpp
#define Em_Enter
#define Em_Exit
#define LocalMemAccessNtfy MemAccessNtfy
#define LocalMMDV_Access MMDV_Access
```

### Gate

```sh
cmake --build --preset macos
cd test && ./verify.sh
git add -A && git commit -m "Remove HaveGlbReg: global-register optimization unused"
git push
```

---

## Step 3 — `FasterAlignedL` (3.11)

Remove the aligned long-word MATC optimization (measured as net-negative).

### 3a. Remove definition

**File:** `src/cpu/m68k.cpp` line 128

Delete:
```cpp
#define FasterAlignedL 0
    /*
        If most long memory access is long aligned, ...
    */
```

### 3b. Remove guarded code blocks

**File:** `src/cpu/m68k.cpp`

Eight `#if FasterAlignedL` sites (lines ~174, ~937, ~941, ~977, ~981,
~8441, ~8487, ~8738). Since `FasterAlignedL` = 0, delete all true-branches.
These include:
- `MATCrdL` / `MATCwrL` struct members (line ~174)
- Aligned-path in `get_long()` and `put_long()` (lines ~937, ~977)
- Forward declarations of `get_long_ext()` / `put_long_ext()` (lines ~941, ~981)
- MATC initialization entries (line ~8441)
- `get_long_ext()` / `put_long_ext()` implementations (lines ~8487, ~8738)

Where an `#else` branch exists, keep it.

### Gate

```sh
cmake --build --preset macos
cd test && ./verify.sh
git add -A && git commit -m "Remove FasterAlignedL: aligned-long MATC optimization (net-negative)"
git push
```

---

## Step 4 — `EXTRA_ABNORMAL_REPORTS` (3.1)

Remove the extra-verbose tier of abnormal reports.

### 4a. Remove definitions

**File:** `src/core/emulation_config.h` line 47 — delete `#define EXTRA_ABNORMAL_REPORTS 0`
**File:** `src/config/CNFUDPIC.h` line 44 — delete `#define EXTRA_ABNORMAL_REPORTS 0`

### 4b. Remove guarded code blocks

Nine `#if EXTRA_ABNORMAL_REPORTS` sites. No `#else` branches — delete
the entire `#if` … `#endif` block at each site:

| File | Line | Description |
|------|------|-------------|
| `src/devices/via_base.cpp` | ~199 | VIA shift-mode warning |
| `src/cpu/m68k.cpp` | ~1010 | Scale factor in 68020 extension word |
| `src/cpu/m68k.cpp` | ~6992 | Illegal opsize in CHK2/CMP2 |
| `src/cpu/m68k.cpp` | ~7043 | Illegal opsize in CAS |
| `src/cpu/m68k.cpp` | ~8539 | PC recalculation failure |
| `src/devices/sony.cpp` | ~1139 | Non-blockwise access in Sony_Prime |
| `src/devices/sony.cpp` | ~1319 | Unexpected OpCode in Sony_Control |
| `src/devices/sony.cpp` | ~1372 | Unexpected OpCode in Sony_Control |
| `src/core/machine.cpp` | ~1485 | IWM word access |

### Gate

```sh
cmake --build --preset macos
cd test && ./verify.sh
git add -A && git commit -m "Remove EXTRA_ABNORMAL_REPORTS: verbose diagnostics dead code"
git push
```

---

## Step 5 — `SONY_VERIFY_CHECKSUMS` (3.2)

**Action: Compile in unconditionally** (not remove). Remove the `#if` guard
but keep the code. Replace `ReportAbnormalID` calls with `dbglog_WriteNote`.

### 5a. Remove definitions

**File:** `src/core/emulation_config.h` line 35 — delete `#define SONY_VERIFY_CHECKSUMS 0`
**File:** `src/config/CNFUDPIC.h` line 32 — delete `#define SONY_VERIFY_CHECKSUMS 0`

### 5b. Remove guard, keep code, fix logging

**File:** `src/devices/sony.cpp` line ~337

Remove the `#if SONY_VERIFY_CHECKSUMS` and `#endif` lines. Keep the code
block between them. Replace the two `ReportAbnormalID` calls:

```cpp
// Before:
ReportAbnormalID(AbnormalID::kSONY_bad_dataChecksum, "bad dataChecksum");
// After:
dbglog_WriteNote("DC42 data checksum mismatch on disk image");

// Before:
ReportAbnormalID(AbnormalID::kSONY_bad_tagChecksum, "bad tagChecksum");
// After:
dbglog_WriteNote("DC42 tag checksum mismatch on disk image");
```

### Gate

```sh
cmake --build --preset macos
cd test && ./verify.sh
git add -A && git commit -m "Compile in SONY_VERIFY_CHECKSUMS: validate DC42 checksums at mount"
git push
```

---

## Step 6 — `GRAB_KEYS_MAX_FULL_SCREEN` (3.3)

Delete the define. No code to remove (0 `#if` sites in code).

### 6a. Remove definition

**File:** `src/platform/common/osglu_common.h` lines 17–19

Delete:
```cpp
#ifndef GRAB_KEYS_MAX_FULL_SCREEN
#define GRAB_KEYS_MAX_FULL_SCREEN 0
#endif
```

### Gate

```sh
cmake --build --preset macos
cd test && ./verify.sh
git add -A && git commit -m "Remove GRAB_KEYS_MAX_FULL_SCREEN: unused define with no code"
git push
```

---

## Step 7 — `EnableAltKeysMode` (3.4)

Remove the alternate keyboard-mode overlay feature.

### 7a. Remove definition

**File:** `src/platform/platform_config.h` line 10 — delete `#define EnableAltKeysMode 0`

### 7b. Remove guarded code blocks

Seven `#if EnableAltKeysMode` sites. Since value = 0, delete all true-branches
and keep `#else` branches where they exist:

| File | Line | Description |
|------|------|-------------|
| `src/platform/common/control_mode.h` | ~12 | `SpclModeAltKeyText` enum entry |
| `src/platform/common/control_mode.h` | ~40 | Keyboard alias definitions |
| `src/platform/common/control_mode.cpp` | ~284 | `#include "alt_keys.h"` |
| `src/platform/common/control_mode.cpp` | ~963 | Alt-key mode drawing — has `#else` (keep `#else` branch) |
| `src/platform/common/control_mode.cpp` | ~1069 | Alt-key mode detection in key checking |
| `src/platform/common/intl_chars.cpp` | ~616 | Alt-key insert text data blocks |
| `src/platform/common/intl_chars.h` | ~215 | Alt-key insert text cell enumerations |

### Gate

```sh
cmake --build --preset macos
cd test && ./verify.sh
git add -A && git commit -m "Remove EnableAltKeysMode: alt-keyboard overlay dead code"
git push
```

---

## Step 8 — `NeedIntlChars` (3.5)

Remove international character bitmap glyphs and escape-sequence parser.

### 8a. Remove definition

**File:** `src/platform/platform_config.h` line 45 — delete `#define NeedIntlChars 0`

### 8b. Remove guarded code blocks

Nine `#if NeedIntlChars` sites. No `#else` branches — delete entire blocks:

| File | Line | Description |
|------|------|-------------|
| `src/platform/common/intl_chars.h` | ~93 | International character cell enumeration |
| `src/platform/common/intl_chars.cpp` | ~270 | International character bitmap data |
| `src/platform/common/intl_chars.cpp` | ~725 | International character ASCII mapping |
| `src/platform/common/intl_chars.cpp` | ~918 | Character code lookup mapping |
| `src/platform/common/intl_chars.cpp` | ~1111 | Unicode mapping for intl chars |
| `src/platform/common/intl_chars.cpp` | ~1304 | Character name mapping |
| `src/platform/common/intl_chars.cpp` | ~1558 | Diacritic base character matching |
| `src/platform/common/intl_chars.cpp` | ~1633 | Special character case transforms (1) |
| `src/platform/common/intl_chars.cpp` | ~1645 | Special character case transforms (2) |

### Gate

```sh
cmake --build --preset macos
cd test && ./verify.sh
git add -A && git commit -m "Remove NeedIntlChars: international character bitmap dead code"
git push
```

---

## Step 9 — `WantInitRunInBackground` (3.6)

Replace the define with a literal `false`.

### 9a. Remove definition

**File:** `src/platform/platform_config.h` line 37 — delete `#define WantInitRunInBackground 0`

### 9b. Replace expression usage

**File:** `src/platform/common/intl_chars.cpp` line ~1411

```cpp
// Before:
bool g_runInBackground = (WantInitRunInBackground != 0);
// After:
bool g_runInBackground = false;
```

### Gate

```sh
cmake --build --preset macos
cd test && ./verify.sh
git add -A && git commit -m "Remove WantInitRunInBackground: inline false default"
git push
```

---

## Step 10 — `MyAppIsBundle` (3.7)

Delete the define. No code references it.

### 10a. Remove definition

**File:** `src/platform/sdl_config.h` line 15 — delete `#define MyAppIsBundle 0`

### Gate

```sh
cmake --build --preset macos
cd test && ./verify.sh
git add -A && git commit -m "Remove MyAppIsBundle: unused vestige of Xcode build system"
git push
```

---

## Step 11 — `WantAutoScrollBorder` (3.8)

**Action: Compile in unconditionally.** Remove the `#if` guards but keep
the border-margin code.

### 11a. Remove definition

**File:** `src/platform/common/osglu_common.cpp` lines 637–639

Delete:
```cpp
#ifndef WantAutoScrollBorder
#define WantAutoScrollBorder 0
#endif
```

### 11b. Remove guards, keep code

**File:** `src/platform/common/osglu_common.cpp`

Four sites (lines ~650, ~663, ~686, ~699). At each site, remove the
`#if WantAutoScrollBorder` and `#endif` lines, keep the guarded code
(the `+ (g_viewHSize / 16)` and `- (g_viewHSize / 16)` expressions).

Example — line ~650, before:
```cpp
        Limit = g_viewHStart
#if WantAutoScrollBorder
            + (g_viewHSize / 16)
#endif
            ;
```

After:
```cpp
        Limit = g_viewHStart
            + (g_viewHSize / 16);
```

Apply the same pattern at all four sites.

### Gate

```sh
cmake --build --preset macos
cd test && ./verify.sh
git add -A && git commit -m "Compile in WantAutoScrollBorder: auto-scroll dead zone always active"
git push
```

---

## Step 12 — `UseLargeScreenHack` (3.9)

**Action: Compile in unconditionally.** Remove the `#if` guard but keep
the `ApplyScreenHack()` call so the screen hack executes when the emulated
screen differs from the default.

### 12a. Remove definition

**File:** `src/devices/rom.cpp` lines 21–22

Delete:
```cpp
#ifndef UseLargeScreenHack
#define UseLargeScreenHack 0
#endif
```

### 12b. Remove guard, keep code

**File:** `src/devices/rom.cpp` line ~213

Remove the `#if UseLargeScreenHack` and `#endif` lines, keep:
```cpp
    ApplyScreenHack(pto);
```

### Gate

```sh
cmake --build --preset macos
cd test && ./verify.sh
git add -A && git commit -m "Compile in UseLargeScreenHack: screen hack always applied when needed"
git push
```

---

## Step 13 — `C_INCLUDE_UNUSED` / `cIncludeFPUUnused` (3.14)

Remove unused SoftFloat FPU routines.

### 13a. Remove definitions

**File:** `src/core/types.h` line 15 — delete `#define C_INCLUDE_UNUSED 0`
**File:** `src/cpu/fpu_math.h` line 88 — delete `#define cIncludeFPUUnused C_INCLUDE_UNUSED`

### 13b. Remove guarded code blocks

**File:** `src/cpu/fpu_math.h`

Fourteen `#if cIncludeFPUUnused` sites. Since value = 0, delete entire
true-branch blocks at each site:

| Line | Description |
|------|-------------|
| ~897 | `ne128()` helper function |
| ~1844 | `floatx80_to_int32_round_to_zero()` |
| ~2489 | `floatx80_eq()` |
| ~2521 | FPU comparison function |
| ~2556 | FPU comparison function |
| ~2591 | FPU comparison function |
| ~2620 | FPU comparison function |
| ~2658 | FPU comparison function |
| ~3771 | FPU conversion function |
| ~3795 | FPU conversion function |
| ~3929 | FPU arithmetic function |
| ~3968 | FPU arithmetic function |
| ~4037 | FPU arithmetic function |
| ~4229 | FPU arithmetic function |

### Gate

```sh
cmake --build --preset macos
cd test && ./verify.sh
git add -A && git commit -m "Remove C_INCLUDE_UNUSED/cIncludeFPUUnused: delete unreachable FPU routines"
git push
```

---

## Step 14 — `NeedCell2WinAsciiMap` (3.15)

Remove the Windows-1252 character mapping table.

### 14a. Remove definition

**File:** `src/platform/common/intl_chars.h` lines 259–260

Delete:
```cpp
#ifndef NeedCell2WinAsciiMap
#define NeedCell2WinAsciiMap 0
#endif
```

### 14b. Remove guarded code blocks

Two `#if NeedCell2WinAsciiMap` sites:

| File | Line | Description |
|------|------|-------------|
| `src/platform/common/intl_chars.h` | ~263 | `extern` declaration of `Cell2WinAsciiMap[]` |
| `src/platform/common/intl_chars.cpp` | ~832 | Definition of `Cell2WinAsciiMap[]` table |

Delete both `#if` … `#endif` blocks entirely (including the table data).

### Gate

```sh
cmake --build --preset macos
cd test && ./verify.sh
git add -A && git commit -m "Remove NeedCell2WinAsciiMap: Windows-1252 mapping table dead code"
git push
```

---

## Summary

| Step | Define(s) | Action | Files touched |
|------|-----------|--------|---------------|
| 1 | `DisableLazyFlagAll`, `ForceFlagsEval`, `UseLazyZ`, `UseLazyCC` | Remove | `m68k.cpp` |
| 2 | `HaveGlbReg` | Remove | `m68k.cpp` |
| 3 | `FasterAlignedL` | Remove | `m68k.cpp` |
| 4 | `EXTRA_ABNORMAL_REPORTS` | Remove | `emulation_config.h`, `CNFUDPIC.h`, `m68k.cpp`, `sony.cpp`, `via_base.cpp`, `machine.cpp` |
| 5 | `SONY_VERIFY_CHECKSUMS` | Compile in | `emulation_config.h`, `CNFUDPIC.h`, `sony.cpp` |
| 6 | `GRAB_KEYS_MAX_FULL_SCREEN` | Remove | `osglu_common.h` |
| 7 | `EnableAltKeysMode` | Remove | `platform_config.h`, `control_mode.h`, `control_mode.cpp`, `intl_chars.cpp`, `intl_chars.h` |
| 8 | `NeedIntlChars` | Remove | `platform_config.h`, `intl_chars.h`, `intl_chars.cpp` |
| 9 | `WantInitRunInBackground` | Remove | `platform_config.h`, `intl_chars.cpp` |
| 10 | `MyAppIsBundle` | Remove | `sdl_config.h` |
| 11 | `WantAutoScrollBorder` | Compile in | `osglu_common.cpp` |
| 12 | `UseLargeScreenHack` | Compile in | `rom.cpp` |
| 13 | `C_INCLUDE_UNUSED`, `cIncludeFPUUnused` | Remove | `types.h`, `fpu_math.h` |
| 14 | `NeedCell2WinAsciiMap` | Remove | `intl_chars.h`, `intl_chars.cpp` |

**Total: 15 defines removed across 14 steps, 14 commits.**
