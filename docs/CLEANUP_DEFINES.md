# Compile-Time #defines Cleanup — Completed

All planned steps have been executed and verified against the 6-model
golden test suite (Mac512Ke, MacPlus, MacSE, Classic, MacII, MacIIx).

## Architecture

**`MachineConfig`** describes the emulated hardware (what the Mac sees).
Fixed at startup.

**`EmulatorConfig`** (`src/core/emulator_config.h`) describes the
emulator's presentation (what the user sees). Mutable at runtime.
Fields: `fullscreen`, `magnify`, `windowScale`, `soundEnabled`, `speed`.

`BuildEmulatorConfig()` in `config_loader.cpp` constructs it from CLI
flags. `GetEmulatorConfig()` / `GetEmulatorConfigMut()` in `main.h`
provide access.

## What Was Removed

### CMake options removed

| Former CMake Variable | Former #define | Disposition |
|---|---|---|
| `MINIVMAC_FULLSCREEN_VAR` | `VarFullScreen` | Guards deleted, code unconditional |
| `MINIVMAC_FULLSCREEN_INIT` | `WantInitFullScreen` | → `EmulatorConfig::fullscreen` |
| `MINIVMAC_FULLSCREEN_MAY` | `MayFullScreen` | Guards deleted |
| `MINIVMAC_FULLSCREEN_MAY_NOT` | `MayNotFullScreen` | Guards deleted |
| `MINIVMAC_MAGNIFY_ENABLE` | `EnableMagnify` | Guards deleted |
| `MINIVMAC_MAGNIFY_INIT` | `WantInitMagnify` | → `EmulatorConfig::magnify` |
| `MINIVMAC_SOUND` | `MySoundEnabled` | Guards deleted; `--silent` flag added |
| `MINIVMAC_WINDOW_SCALE` | `MyWindowScale` | → `EmulatorConfig::windowScale`; `--scale` flag added |
| `MINIVMAC_SPEED` | `WantInitSpeedValue` | → `EmulatorConfig::speed`; `--speed` wired |
| `MINIVMAC_NUM_DRIVES` | `NumDrives` | → `constexpr int NumDrives = 6` in CNFUDALL.h |

### Hardcoded #defines removed from CNFUDALL.h.in

| Former #define | Disposition |
|---|---|
| `MySoundRecenterSilence` | Deleted (unused, always 0) |
| `kLn2SoundSampSz` | Inlined (always 4); `#if` branches for value 3 deleted |
| `NonDiskProtect` | Guards deleted, code unconditional |
| `IncludeSonyRawMode` | Guards deleted |
| `IncludeSonyGetName` | Guards deleted |
| `IncludeSonyNew` | Guards deleted |
| `IncludeSonyNameNew` | Guards deleted |
| `IncludePbufs` | Guards deleted |
| `EnableMouseMotion` | Guards deleted |
| `IncludeHostTextClipExchange` | Guards deleted |
| `EnableAutoSlow` | Guards deleted |
| `AutoLocation` | Guards deleted |
| `AutoTimeZone` | Guards deleted |
| `NumPbufs` | → `constexpr int NumPbufs = 4` in CNFUDALL.h |

## What Remains

### CNFUDALL.h.in / CNFUDALL.h

| #define | CMake Variable | Rationale |
|---|---|---|
| `dbglog_HAVE` | `MINIVMAC_DBGLOG` | Debug logging overhead; compile-time gate |
| `WantAbnormalReports` | `MINIVMAC_ABNORMAL_REPORTS` | Debug diagnostic |
| `EmLocalTalk` | `MINIVMAC_LOCALTALK` | Gates ~600 lines of networking code |

Plus `constexpr int NumDrives = 6` and `constexpr int NumPbufs = 4`.

### CNFUDOSG.h.in / CNFUDOSG.h

| #define | Notes |
|---|---|
| `SaveDialogEnable` | Always 1 |
| `EnableAltKeysMode` | Always 0 |
| `MKC_formac_*` | Key mappings — separate cleanup concern |
| `MKC_UnMappedKey` | Always MKC_Control |
| `WantInitRunInBackground` | Always 0 |
| `WantEnblCtrlInt/Rst/Ktg` | Control-mode toggles, all 1 |
| `UseControlKeys` | Always 1 |
| `NeedIntlChars` | Always 0 |
| `kBldOpts` | Build description string |

### CNFUIOSG.h.in

Backend selection and app identity strings. Genuine build-time choices.

### STRCONST.h.in

Language header includes. Separate concern.

## Future Work

- Key mappings (`MKC_formac_*`) should move to their own header.
- `WantInitRunInBackground` could become `EmulatorConfig::runInBackground`.
- `EmLocalTalk` could become runtime when networking gets a device abstraction.
- `dbglog_HAVE` / `WantAbnormalReports` could become runtime if debug
  logging overhead becomes negligible.

---

## Out of Scope

- Language (`MINIVMAC_LANGUAGE`) — separate plan.
- `MKC_formac_*` key mappings — separate concern, needs its own design.
- `WantCycByPriOp` — tracked in CLEANUP.md Step 4.
- `SCC_TrackMore` — tracked in CLEANUP.md Step 4.
- Moving globals into Machine — tracked in CLEANUP.md Step 3.
