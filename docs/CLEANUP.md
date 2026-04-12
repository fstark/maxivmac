# Naming Cleanup Plan

Step-by-step plan to bring `src/` identifiers into compliance with
[NAMING.md](NAMING.md).  Each phase is a self-contained rename that
can be committed independently.

Excludes `src/cpu/` and `src/macsrc/` (exempt per NAMING.md).

---

## Build & Test Gate

Run after **every phase** before committing:

```sh
cmake --build bld/macos-headless 2>&1 | tail -3
cd test && ./verify.sh
```

Both must pass.  Commit format: `cleanup(naming): <phase description>`.

---

## Phase 1 — Bare Globals (missing `g_` prefix)

Rename non-static file-scope variables that lack the required `g_` prefix.

| Old Name | New Name | Files |
|---|---|---|
| `MasterEvtQLock` | `g_masterEvtQLock` | `machine.cpp`, `machine.h` |
| `PbufSize` | `g_pbufSize` | `osglu_common.cpp`, `osglu_common.h` |
| `EvtQIn` | `g_evtQIn` | `osglu_common.cpp`, `osglu_common.h` |
| `EvtQOut` | `g_evtQOut` | `osglu_common.cpp`, `osglu_common.h`, `platform.h` |
| `EvtQNeedRecover` | `g_evtQNeedRecover` | `osglu_common.cpp` |
| `theKeys` | `g_theKeys` | `osglu_common.cpp`, `osglu_common.h` |
| `e_p` | `g_entropyPool` | `osglu_common.cpp` |
| `EvtQA` | `g_evtQA` | `osglu_common.cpp`, `osglu_common.h` |
| `Drives` | `g_drives` | `disk_io.cpp`, `disk_io.h` |
| `DriveNames` | `g_driveNames` | `disk_io.cpp`, `disk_io.h` |
| `PbufDat` | `g_pbufDat` | `param_buffers.cpp`, `param_buffers.h`, `clipboard.cpp`, `disk_io.cpp`, `emulator_shell.cpp` |

**Test, then commit.**

---

## Phase 2 — `g_` Globals with PascalCase → camelCase

| Old Name | New Name | Files |
|---|---|---|
| `g_InstructionCount` | `g_instructionCount` | `machine.h`, `machine.cpp` |
| `g_LogStart` | `g_logStart` | `machine.h`, `machine.cpp`, `config_loader.cpp` |
| `g_LogEnd` | `g_logEnd` | `machine.h`, `machine.cpp`, `config_loader.cpp` |
| `g_SkipThrottle` | `g_skipThrottle` | `osglu_common.cpp`, `platform.h`, `emulator_shell.cpp` |

**Test, then commit.**

---

## Phase 3 — `My`-Prefix Functions and Macros

Drop the `My` prefix from functions; convert `My`-prefixed macros to
`UPPER_SNAKE_CASE`.

### Functions

| Old Name | New Name | Files |
|---|---|---|
| `MyMouseButtonSet` | `MouseButtonSet` | `osglu_common.h`, `osglu_common.cpp`, callers |
| `MyMousePositionSetDelta` | `MousePositionSetDelta` | `osglu_common.h`, `osglu_common.cpp`, callers |
| `MyMousePositionSet` | `MousePositionSet` | `osglu_common.h`, `osglu_common.cpp`, callers |
| `MyMoveBytesVM` | `MoveBytesVM` | `sony.cpp` (static, local rename) |

### Macros

| Old Name | New Name | Files |
|---|---|---|
| `MyEvtQLg2Sz` | `EVT_Q_LG2_SZ` | `osglu_common.h` |
| `MyEvtQSz` | `EVT_Q_SZ` | `osglu_common.h`, `osglu_common.cpp` |
| `MyEvtQIMask` | `EVT_Q_IMASK` | `osglu_common.h`, `osglu_common.cpp` |
| `MyPathSep` | `PATH_SEP` | `path_utils.h`, `path_utils.cpp` |
| `MyInvTimeDivPow` | `INV_TIME_DIV_POW` | `tick_timer.cpp` |
| `MyInvTimeDiv` | `INV_TIME_DIV` | `tick_timer.cpp` |
| `MyInvTimeDivMask` | `INV_TIME_DIV_MASK` | `tick_timer.cpp` |
| `MyInvTimeStep` | `INV_TIME_STEP` | `tick_timer.cpp` |

**Test, then commit.**

---

## Phase 4 — File-Scope Statics (missing `s_` prefix)

### 4a — Device statics

| Old Name | New Name | File |
|---|---|---|
| `SoundReg801`–`SoundReg805` | `s_soundReg801`–`s_soundReg805` | `asc.cpp` |
| `SoundReg_Volume` | `s_soundRegVolume` | `asc.cpp` |
| `ASC_SampBuff` | `s_ascSampBuff` | `asc.cpp` |
| `ASC_ChanA` | `s_ascChanA` | `asc.cpp` |
| `ASC_FIFO_Out` | `s_ascFifoOut` | `asc.cpp` |
| `ASC_FIFO_InA` | `s_ascFifoInA` | `asc.cpp` |
| `ASC_FIFO_InB` | `s_ascFifoInB` | `asc.cpp` |
| `MyCTSBuffer` | `s_ctsBuffer` | `scc.cpp` |
| `my_node_address` | `s_nodeAddress` | `scc.cpp` |
| `rx_data_offset` | `s_rxDataOffset` | `scc.cpp` |
| `ImageDataOffset` | `s_imageDataOffset` | `sony.cpp` |
| `ImageDataSize` | `s_imageDataSize` | `sony.cpp` |
| `ImageTagOffset` | `s_imageTagOffset` | `sony.cpp` |
| `SCSI` (array) | `s_scsi` | `scsi.cpp` |
| `IWM` (struct instance) | `s_iwm` | `iwm.cpp` |
| `ADB_SzDatBuf` | `s_adbSzDatBuf` | `adb_shared.h` |
| `ADB_DatBuf` | `s_adbDatBuf` | `adb_shared.h` |
| `ADB_CurCmd` | `s_adbCurCmd` | `adb_shared.h` |
| `ADB_ListenDatBuf` | `s_adbListenDatBuf` | `adb.cpp` |

**Test, then commit.**

### 4b — Core / platform statics

| Old Name | New Name | File |
|---|---|---|
| `ATTListA` | `s_attListA` | `machine.cpp` |
| `cur_audio` | `s_curAudio` | `sdl_sound.cpp` |
| `dbglog_File` | `s_dbglogFile` | `dbglog_platform.cpp` |
| `dbglog_bufpos` | `s_dbglogBufpos` | `osglu_common.cpp` |
| `dbglog_bufp` | `s_dbglogBufp` | `osglu_common.cpp` |

**Test, then commit.**

---

## Phase 5 — camelCase Free Functions → PascalCase

| Old Name | New Name | Files |
|---|---|---|
| `customreset()` | `CustomReset()` | `machine.cpp`, `machine.h` |
| `extnReset()` | `ExtnReset()` | `machine.cpp`, `machine.h`, `main.cpp` |
| `memoryReset()` | `MemoryReset()` | `machine.cpp`, `machine.h`, `main.cpp` |
| `extnClipDispatch()` | `ExtnClipDispatch()` | `extn_clip.cpp`, `extn_clip.h`, `machine.cpp` |
| `extnDbgConsoleClear()` | `ExtnDbgConsoleClear()` | `extn_clip.cpp`, `extn_clip.h` |
| `lomem_snapshot_take()` | `Lomem_SnapshotTake()` | `lomem_globals.cpp`, `lomem_globals.h`, callers |
| `hostClipSetText()` | `HostClipSetText()` | `clipboard.cpp`, `clipboard.h`, callers |

**Test, then commit.**

---

## Phase 6 — `_Ty` Suffix Structs → PascalCase

| Old Name | New Name | File |
|---|---|---|
| `Channel_Ty` | `ChannelState` | `scc.cpp` |
| `SCC_Ty` | `SCCState` | `scc.cpp` |
| `IWM_Ty` | `IWMState` | `iwm.cpp` |
| `RTC_Ty` | `RTCState` | `rtc.cpp` |
| `Mode_Ty` | `enum class IWMMode` | `iwm.cpp` |
| `SoundR` | `SoundState` | `sdl_sound.cpp` |
| `ASC_ChanR` | `ASCChannel` | `asc.cpp` |

Also rename the `VIA_Ty` member in `via_base.h` → `VIAState` if scope
is localized.

**Test, then commit.**

---

## Phase 7 — Type Aliases

| Old Name | New Name | Files |
|---|---|---|
| `iCountt` | `InstructionCount` | `machine.h`, `ict_scheduler.h`, callers |
| `ATTep` | `ATTEntryPtr` | `machine.h`, `machine.cpp` |
| `SoundTemp` | `SoundSample` | `sdl_sound.cpp` (local) |

**Test, then commit.**

---

## Phase 8 — PascalCase Macros → `UPPER_SNAKE_CASE`

Split into sub-phases to keep diffs reviewable.

### 8a — Wire macros (`wire_macros.h`)

| Old | New |
|---|---|
| `SoundDisable` | `SOUND_DISABLE` |
| `SoundVolb0`–`SoundVolb2` | `SOUND_VOL_B0`–`SOUND_VOL_B2` |
| `MemOverlay` | `MEM_OVERLAY` |
| `Addr32` | `ADDR32` |
| `Vid_VBLinterrupt` | `VID_VBL_INTERRUPT` |
| `Vid_VBLintunenbl` | `VID_VBL_INT_UNENBL` |

**Test, then commit.**

### 8b — Core macros (`machine.h`, `machine.cpp`, `main.cpp`)

| Old | New |
|---|---|
| `ReportAbnormalID` | `REPORT_ABNORMAL_ID` |
| `ReportAbnormalInterrupt` | `REPORT_ABNORMAL_INTERRUPT` |
| `ExtnDat_checkval` … `ExtnDat_params` | `EXTN_DAT_CHECKVAL` … `EXTN_DAT_PARAMS` |
| `RdAvgXtraCyc` | `RD_AVG_XTRA_CYC` |
| `WrAvgXtraCyc` | `WR_AVG_XTRA_CYC` |
| `CyclesScaledPerTick` | `CYCLES_SCALED_PER_TICK` |
| `CyclesScaledPerSubTick` | `CYCLES_SCALED_PER_SUB_TICK` |
| `AddToATTListWithMTB` | `ADD_TO_ATT_LIST_WITH_MTB` |

**Test, then commit.**

### 8c — Platform macros

| Old | New | File(s) |
|---|---|---|
| `PbufIsAllocated` | `PBUF_IS_ALLOCATED` | `osglu_common.h` |
| `PbufUnlock` | `PBUF_UNLOCK` | `param_buffers.h` |
| `QuietEnds()` | `QUIET_ENDS()` | `platform.h` |
| `ScrnTrns_Scale` | `SCRN_TRNS_SCALE` | `screen_translate.h` |
| `ScrnTrns_DstZLo` | `SCRN_TRNS_DST_Z_LO` | `screen_translate.h` |
| `Keyboard_UpdateKeyMap1` | `KEYBOARD_UPDATE_KEY_MAP_1` | `keyboard_map.h` |
| `DisconnectKeyCodes1` | `DISCONNECT_KEY_CODES_1` | `keyboard_map.h` |
| `vMacScreen*` family | `VMAC_SCREEN_*` | `platform.h` |

**Test, then commit.**

### 8d — Device macros

| Old | New | File |
|---|---|---|
| `ChecksumBlockSize` | `CHECKSUM_BLOCK_SIZE` | `sony.cpp` |
| `SizeCheckSumsToUpdate` | `SIZE_CHECKSUMS_TO_UPDATE` | `sony.cpp` |
| `MinTicksBetweenInsert` | `MIN_TICKS_BETWEEN_INSERT` | `sony.cpp` |
| `SonyVarsPtr` | `SONY_VARS_PTR` | `sony.cpp` |
| `FirstDriveVarsOffset` | `FIRST_DRIVE_VARS_OFFSET` | `sony.cpp` |
| `EachDriveVarsSize` | `EACH_DRIVE_VARS_SIZE` | `sony.cpp` |
| `MinSonVarsSize` | `MIN_SON_VARS_SIZE` | `sony.cpp` |
| `Sony_dolog` | `SONY_DO_LOG` | `sony.cpp` |
| `CntrlParam_csCode` | `CNTRL_PARAM_CS_CODE` | `video.cpp` |
| `CntrlParam_csParam` | `CNTRL_PARAM_CS_PARAM` | `video.cpp` |
| `VidBaseAddr` | `VID_BASE_ADDR` | `video.cpp` |
| `SetContains` | `SET_CONTAINS` | `via_base.cpp` |
| `Group1Base`, `Group2Base` | `GROUP1_BASE`, `GROUP2_BASE` | `rtc.cpp` |
| `TrackSpeed` | `TRACK_SPEED` | `rtc.cpp` |
| `AlarmOn` | `ALARM_ON` | `rtc.cpp` |
| `DiskCacheSz` | `DISK_CACHE_SZ` | `rtc.cpp` |
| `StartUpDisk` | `STARTUP_DISK` | `rtc.cpp` |
| `DiskCacheOn` | `DISK_CACHE_ON` | `rtc.cpp` |
| `MouseScalingOn` | `MOUSE_SCALING_ON` | `rtc.cpp` |
| `SpeakerVol` | `SPEAKER_VOL` | `rtc.cpp` |
| `DesiredMinFilledSoundBuffs` | `DESIRED_MIN_FILLED_SOUND_BUFFS` | `sdl_sound.cpp` |

**Test, then commit.**

---

## Phase 9 — Unscoped Enums → `enum class`

| Enum | File | Notes |
|---|---|---|
| `Mode_Ty { On, Off }` | `iwm.cpp` | → `enum class IWMMode` (done in Phase 6) |
| `LMType` | `lomem_globals.h` | → `enum class LMType` — update value references |
| `LMCategory` | `lomem_globals.h` | → `enum class LMCategory` — update value references |

`WireID` can remain unscoped — the `Wire_` prefix acts as a namespace
and values are used as array indices.

**Test, then commit.**

---

## Phase 10 — PascalCase Parameters and Locals

| Old Name | New Name | File |
|---|---|---|
| `NewMousePosh`, `NewMousePosv` | `newMousePosH`, `newMousePosV` | `emulator_shell.cpp` |
| `NewWindowX`, `NewWindowY` | `newWindowX`, `newWindowY` | `emulator_shell.cpp` |
| `NewWindowWidth`, `NewWindowHeight` | `newWindowWidth`, `newWindowHeight` | `emulator_shell.cpp` |
| `Pbuf_No` | `pbufNo` | `machine.cpp` |
| `WriteMem` (param) | `writeMem` | `machine.cpp` |

**Test, then commit.**

---

## Final Verification

After all phases:

```sh
cmake --build bld/macos-headless 2>&1 | tail -3
cd test && ./verify.sh
```

Confirm all models pass.  Optionally run with asan preset to check
for no new issues:

```sh
cmake --build --preset macos-asan
EMU=./bld/macos-asan/maxivmac ./test/verify.sh
```