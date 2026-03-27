# Cleanup Plan: Compile-Time #defines → Runtime Config

This plan eliminates the `MINIVMAC_*` CMake options and their generated
`#define`s, replacing them with either hardcoded values (for pointless
options) or runtime configuration (for useful preferences).

The key architectural insight: **`MachineConfig` describes the emulated
hardware** (what the Mac sees). A new **`EmulatorConfig` describes the
emulator's presentation** (what the user sees). This separation matters
because:

- `MachineConfig` is fixed at startup — changing RAM or CPU mid-run
  makes no sense.
- `EmulatorConfig` is mutable at runtime — toggling fullscreen, changing
  speed, or muting sound should be immediate, no restart needed.
- A future UI panel maps cleanly: machine settings = startup dialog,
  emulator settings = preferences / hotkeys.

---

## Current State

### CMake options (CMakeLists.txt)

| CMake Variable | Template | Generated #define | Default |
|---|---|---|---|
| `MINIVMAC_SOUND` | CNFUDALL.h.in | `MySoundEnabled` | 1 |
| `MINIVMAC_NUM_DRIVES` | CNFUDALL.h.in | `NumDrives` | 6 |
| `MINIVMAC_ABNORMAL_REPORTS` | CNFUDALL.h.in | `WantAbnormalReports` | 0 |
| `MINIVMAC_LOCALTALK` | CNFUDALL.h.in | `EmLocalTalk` | 0 |
| `MINIVMAC_DBGLOG` | CNFUDALL.h.in | `dbglog_HAVE` | 1 |
| `MINIVMAC_MAGNIFY_ENABLE` | CNFUDOSG.h.in | `EnableMagnify` | 1 |
| `MINIVMAC_MAGNIFY_INIT` | CNFUDOSG.h.in | `WantInitMagnify` | 1 |
| `MINIVMAC_WINDOW_SCALE` | CNFUDOSG.h.in | `MyWindowScale` | 2 |
| `MINIVMAC_FULLSCREEN_VAR` | CNFUDOSG.h.in | `VarFullScreen` | 1 |
| `MINIVMAC_FULLSCREEN_INIT` | CNFUDOSG.h.in | `WantInitFullScreen` | 0 |
| `MINIVMAC_FULLSCREEN_MAY` | CNFUDOSG.h.in | `MayFullScreen` | 1 |
| `MINIVMAC_FULLSCREEN_MAY_NOT` | CNFUDOSG.h.in | `MayNotFullScreen` | 1 |
| `MINIVMAC_SPEED` | CNFUDOSG.h.in | `WantInitSpeedValue` | 4 |

### Hardcoded #defines in CNFUDALL.h.in (no CMake variable)

These are already constants with no CMake knob. They will be addressed
in the final cleanup pass (step 5 below).

| #define | Value | Notes |
|---|---|---|
| `MySoundRecenterSilence` | 0 | Sound implementation detail |
| `kLn2SoundSampSz` | 4 | Sound sample size (16-bit) |
| `NonDiskProtect` | 1 | Always want disk protection |
| `IncludeSonyRawMode` | 1 | Always want raw disk mode |
| `IncludeSonyGetName` | 1 | Always want disk name support |
| `IncludeSonyNew` | 1 | Always want new-disk support |
| `IncludeSonyNameNew` | 1 | Always want named new-disk |
| `IncludePbufs` | 1 | Always want param buffers |
| `NumPbufs` | 4 | Param buffer count |
| `EnableMouseMotion` | 1 | Always want mouse |
| `IncludeHostTextClipExchange` | 1 | Always want clipboard |
| `EnableAutoSlow` | 1 | Always want auto-slow |
| `AutoLocation` | 1 | Always set Mac location |
| `AutoTimeZone` | 1 | Always set Mac timezone |

### Hardcoded #defines in CNFUDOSG.h.in (no CMake variable)

| #define | Value | Notes |
|---|---|---|
| `SaveDialogEnable` | 1 | Always want save dialogs |
| `EnableAltKeysMode` | 0 | Alternative key mapping off |
| `MKC_formac_*` | various | Key mappings — separate concern |
| `MKC_UnMappedKey` | MKC_Control | Unmapped key target |
| `WantInitRunInBackground` | 0 | Background running default |
| `WantInitNotAutoSlow` | 0 | Auto-slow enabled by default |
| `WantEnblCtrlInt` | 1 | Control-mode interrupt |
| `WantEnblCtrlRst` | 1 | Control-mode reset |
| `WantEnblCtrlKtg` | 1 | Control-mode key toggle |
| `UseControlKeys` | 1 | Control key mode |
| `NeedIntlChars` | 0 | International chars |
| `kBldOpts` | string | Build description — keep? |

---

## New Architecture

### EmulatorConfig struct

```cpp
// src/core/emulator_config.h

struct EmulatorConfig {
    // Display
    bool     fullscreen   = false;   // start fullscreen
    bool     magnify      = true;    // enable pixel scaling
    uint8_t  windowScale  = 2;       // 1x, 2x, 3x, ...

    // Audio
    bool     soundEnabled = true;    // audio output

    // Speed
    int      speed        = 4;       // 0=all-out, 1=1x, 2=2x, 3=4x, 4=8x, 5=16x

    // Background behavior
    bool     runInBackground = false;
    bool     autoSlow        = true;
};
```

### LaunchConfig changes

`LaunchConfig` gains fields that map to `EmulatorConfig`:

```cpp
// Already exists:
int  speed;
bool fullscreen;

// Add:
bool silent      = false;   // --silent
int  windowScale = 0;       // --scale=N (0 = use default)
```

### BuildEmulatorConfig

New function alongside `BuildMachineConfig`:

```cpp
EmulatorConfig BuildEmulatorConfig(const LaunchConfig& launch);
```

### Access pattern

The platform layer (`sdl.cpp`) currently reads `#define`s at startup into
mutable globals (`UseFullScreen`, `UseMagnify`, `SpeedValue`). Those
globals become reads/writes on `EmulatorConfig`, which the platform holds
by reference. The emulator core (`Machine`, devices) never touches
`EmulatorConfig` — it only knows `MachineConfig`.

---

## Execution Plan

Each step is one or more commits. Every commit runs the 6-model
verification (Mac512Ke, MacPlus, MacSE, Classic, MacII, MacIIx).

### Step 1: Create EmulatorConfig (infrastructure)

- [ ] Create `src/core/emulator_config.h` with the struct above.
- [ ] Add `EmulatorConfig BuildEmulatorConfig(const LaunchConfig&)` to
      `config_loader.h/cpp`.
- [ ] Thread `EmulatorConfig` into the platform init path alongside
      `MachineConfig` (just plumbing — not used yet).
- [ ] Verify: no behavior change, all golden files pass.

### Step 2: Tier 1 — remove pointless gate defines

These options exist only to disable features at compile time. Nobody
builds an emulator that can't go fullscreen. Remove the `#if` guards,
keep the code unconditionally.

**2a: Fullscreen gates**

- [ ] Remove `VarFullScreen` — delete all `#if VarFullScreen` / `#endif`,
      keep the code inside unconditionally.
- [ ] Remove `MayFullScreen` — same treatment. All fullscreen viewport
      code becomes unconditional.
- [ ] Remove `MayNotFullScreen` — same.
- [ ] Remove `WantInitFullScreen` — replace the one use
      (`static bool UseFullScreen = (WantInitFullScreen != 0)`) with
      reading `EmulatorConfig::fullscreen`.
- [ ] Remove from CNFUDOSG.h.in and CMakeLists.txt: `MINIVMAC_FULLSCREEN_VAR`,
      `MINIVMAC_FULLSCREEN_INIT`, `MINIVMAC_FULLSCREEN_MAY`,
      `MINIVMAC_FULLSCREEN_MAY_NOT`.

**2b: Magnify gate**

- [ ] Remove `EnableMagnify` — delete all `#if EnableMagnify` / `#endif`,
      keep magnification code unconditional.
- [ ] Remove `WantInitMagnify` — replace use with `EmulatorConfig::magnify`.
- [ ] Remove from CNFUDOSG.h.in and CMakeLists.txt: `MINIVMAC_MAGNIFY_ENABLE`,
      `MINIVMAC_MAGNIFY_INIT`.

**2c: CNFUDALL.h.in hardcoded constants**

These already have no CMake variable — they're just `#define`s that
should be plain constants or deleted:

- [ ] `NonDiskProtect`, `IncludeSonyRawMode`, `IncludeSonyGetName`,
      `IncludeSonyNew`, `IncludeSonyNameNew` — verify they are only
      tested as `#if X` where X=1, then delete the defines, remove the
      `#if`/`#endif` wrapping, keep the code.
- [ ] `IncludePbufs`, `NumPbufs` — same. `NumPbufs` becomes a constexpr.
- [ ] `EnableMouseMotion` — same.
- [ ] `IncludeHostTextClipExchange` — same.
- [ ] `AutoLocation`, `AutoTimeZone` — same.
- [ ] `EnableAutoSlow` — same. (The auto-slow *behavior* later becomes
      an `EmulatorConfig` toggle, but the code stays compiled in.)

### Step 3: Tier 2 — move useful options to EmulatorConfig

**3a: `MySoundEnabled` → `EmulatorConfig::soundEnabled`**

This is the exemplar for the pattern.

- [ ] Add `--silent` flag to `LaunchConfig` / CLI parser.
- [ ] Wire `EmulatorConfig::soundEnabled` from `LaunchConfig::silent`.
- [ ] In `sound.cpp`: remove `#if MySoundEnabled`, wrap the audio
      *output* path (not the device emulation) with a runtime check.
      The Sound device always runs its logic (the Mac OS expects the
      VIA sound interrupt); only the final PCM output to SDL is gated.
- [ ] In `asc.cpp`: same — the ASC registers are always emulated (Mac
      software reads/writes them), but the audio buffer fill is gated
      by `soundEnabled`.
- [ ] Remove `MySoundEnabled` from CNFUDALL.h.in.
- [ ] Remove `MINIVMAC_SOUND` from CMakeLists.txt.
- [ ] `MySoundRecenterSilence` (always 0) — just delete.
- [ ] `kLn2SoundSampSz` (always 4) — convert to constexpr in sound.h.

**3b: `MyWindowScale` → `EmulatorConfig::windowScale`**

- [ ] Add `--scale=N` flag to `LaunchConfig` / CLI parser.
- [ ] Wire `EmulatorConfig::windowScale` from launch config.
- [ ] In `sdl.cpp`: replace `MyWindowScale` reads with
      `emulatorConfig.windowScale`. The many `#define ScrnMapr_Scale
      MyWindowScale` blocks become `ScrnMapr_Scale` reading from config.
- [ ] Remove `MyWindowScale` from CNFUDOSG.h.in.
- [ ] Remove `MINIVMAC_WINDOW_SCALE` from CMakeLists.txt.

**3c: `WantInitSpeedValue` → `EmulatorConfig::speed`**

- [ ] Already have `--speed=N` in CLI. Wire it to `EmulatorConfig::speed`.
- [ ] Replace `SpeedValue = WantInitSpeedValue` in `osglu_common.cpp`
      with `SpeedValue = emulatorConfig.speed`.
- [ ] Remove `WantInitSpeedValue` from CNFUDOSG.h.in.
- [ ] Remove `MINIVMAC_SPEED` from CMakeLists.txt.

**3d: `NumDrives` → constexpr**

Not really an emulator option — 6 drives is universally correct. Just
make it a constexpr and remove the CMake knob.

- [ ] Replace `#define NumDrives @MINIVMAC_NUM_DRIVES@` with
      `constexpr int NumDrives = 6;` in an appropriate header.
- [ ] Remove `MINIVMAC_NUM_DRIVES` from CMakeLists.txt.

### Step 4: Tier 3 — keep as compile-time (for now)

These stay as `#define`s with CMake variables because they have real
compile-time implications:

| Define | Rationale |
|---|---|
| `dbglog_HAVE` | Debug logging overhead; keep build-time gate |
| `WantAbnormalReports` | Same — debug diagnostic |
| `EmLocalTalk` | Gates ~600 lines of networking + potential deps |

These can be revisited later when the codebase is cleaner. LocalTalk
in particular may eventually become a runtime toggle if it gets a proper
device abstraction.

### Step 5: Final cleanup of config headers

After steps 1–4, the `.h.in` templates will be significantly slimmer.
This step cleans up the residue:

- [ ] CNFUDALL.h.in: only `dbglog_HAVE`, `WantAbnormalReports`,
      `EmLocalTalk` remain as CMake-substituted values. The rest are
      either deleted or converted to constexpr in source headers.
      Consider whether this file is still needed at all.
- [ ] CNFUDOSG.h.in: key mappings (`MKC_formac_*`), control mode toggles
      (`WantEnblCtrlInt/Rst/Ktg`, `UseControlKeys`), and the `kBldOpts`
      string remain. These are a separate cleanup concern (key mappings
      should probably be in their own header, not generated).
- [ ] CNFUIOSG.h.in: backend selection and app identity strings. Keep
      as-is — these are genuine build-time choices.
- [ ] STRCONST.h.in: language header include. Separate plan (noted in
      CMakeLists.txt).
- [ ] Remove emptied CMake variables from CMakeLists.txt.

---

## Pattern for Each Define Removal

Repeatable procedure for every define:

```
1. grep -rn DEFINE_NAME src/ cmake/
2. Classify each use:
   - #if DEFINE_NAME ... #endif  →  remove guards, keep body
   - value reference (= DEFINE_NAME)  →  replace with config read or constexpr
3. Edit source files
4. Remove from .h.in template
5. Remove CMake variable (if any) from CMakeLists.txt
6. Build + verify (6 models)
7. Commit
```

---

## Files Modified Per Step

| Step | Files touched |
|---|---|
| 1 | `emulator_config.h` (new), `config_loader.h/cpp`, `main.cpp`, `sdl.cpp` |
| 2a | `sdl.cpp`, `intl_chars.cpp`, `CNFUDOSG.h.in`, `CMakeLists.txt` |
| 2b | `sdl.cpp`, `intl_chars.cpp`, `CNFUDOSG.h.in`, `CMakeLists.txt` |
| 2c | `sdl.cpp`, `osglu_common.cpp`, `control_mode.cpp`, `param_buffers.cpp`, `sony.cpp`, `CNFUDALL.h.in` |
| 3a | `sound.h`, `sound.cpp`, `asc.cpp`, `config_loader.cpp`, `CNFUDALL.h.in`, `CMakeLists.txt` |
| 3b | `sdl.cpp`, `config_loader.cpp`, `CNFUDOSG.h.in`, `CMakeLists.txt` |
| 3c | `osglu_common.cpp`, `config_loader.cpp`, `CNFUDOSG.h.in`, `CMakeLists.txt` |
| 3d | `sony.cpp`, `machine.cpp`, `CNFUDALL.h.in`, `CMakeLists.txt` |
| 5 | `CNFUDALL.h.in`, `CNFUDOSG.h.in`, `CMakeLists.txt` |

---

## Out of Scope

- Language (`MINIVMAC_LANGUAGE`) — separate plan.
- `MKC_formac_*` key mappings — separate concern, needs its own design.
- `WantCycByPriOp` — tracked in CLEANUP.md Step 4.
- `SCC_TrackMore` — tracked in CLEANUP.md Step 4.
- Moving globals into Machine — tracked in CLEANUP.md Step 3.
