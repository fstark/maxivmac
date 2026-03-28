# Naming Migration Plan

Concrete steps to bring the `src/` codebase (excluding `src/macsrc/` and
`src/cpu/`) in line with [NAMING.md](NAMING.md).

Each phase is independent and can be merged separately. Within a phase,
tasks are ordered so earlier ones don't invalidate later ones. Every
task is a single, testable commit verified against the reference test
suite (`cd test && ./verify.sh`).

---

## Excluded Scope

| Directory | Reason |
|-----------|--------|
| `src/macsrc/` | Classic Mac source — not part of the emulator |
| `src/cpu/` | Deeply intertwined m68k/FPU code (~58 `myfp_` functions, 150+ PascalCase macros, macros wrapping function calls). High risk, low reward. Leave as-is. |

The `src/cpu/` types `si6r`, `ui6r`, `si6b`, `ui6b`, `myfpr`, `floatx80`,
`Bit32u`, `Bit64s` are all confined to CPU/FPU internals and do not
leak into the rest of the codebase — no action required.

---

## Phase 1 — Remove `mnvm_` Backward-Compat Aliases

**Goal:** Replace all `mnvm_` aliases with the `tMacErr::` scoped form.

**Why first:** These are trivial search-and-replace with zero semantic
risk, and removing them shrinks the public API surface of `platform.h`.

| # | Task | Files |
|---|------|-------|
| 1.1 | Replace every `mnvm_noErr` → `tMacErr::noErr`, `mnvm_miscErr` → `tMacErr::miscErr`, etc. across non-cpu `src/` | `core/machine.cpp`, `devices/video.cpp`, `devices/sony.cpp`, `platform/common/dbglog_platform.cpp`, `platform/common/clipboard.cpp`, `platform/common/osglu_common.cpp`, `platform/common/param_buffers.h`, `platform/common/disk_io.h`, `platform/sdl.cpp` |
| 1.2 | Delete the 19 `constexpr tMacErr mnvm_*` aliases in `platform/platform.h` | `platform/platform.h` |

**Verification:** `cd test && ./verify.sh` passes. `grep -r mnvm_ src/ --include='*.cpp' --include='*.h'` returns nothing outside `src/cpu/`.

---

## Phase 2 — Remove `My` Prefix from Functions

**Goal:** Drop the `My` / `my_` prefix from all non-cpu functions.

| # | Task | Rename | Files |
|---|------|--------|-------|
| 2.1 | Rename platform sound API | `MySound_Start` → `Sound_Start`, `MySound_Stop` → `Sound_Stop`, `MySound_Init` → `Sound_Init`, `MySound_UnInit` → `Sound_UnInit`, `MySound_SecondNotify` → `Sound_SecondNotify`, `MySound_EndWrite` → `Sound_EndWrite`, `MySound_AllocBuffer` → `Sound_AllocBuffer`, `MySound_FreeBuffer` → `Sound_FreeBuffer`, `MySound_BeginWrite` → `Sound_BeginWrite`, `MySound_WroteABlock` → `Sound_WroteABlock` | `platform/sdl_sound.h`, `platform/sdl_sound.cpp`, `platform/sdl.cpp`, `core/main.cpp` |
| 2.2 | Rename platform utility functions | `MyMoveBytes` → `MoveBytes`, `MyMoveMouse` → `MoveMouse`, `MyDrawChangesAndClear` → `DrawChangesAndClear`, `MyMouseConstrain` → `MouseConstrain` | `platform/sdl.cpp`, `platform/platform.h`, `platform/common/osglu_common.h` |
| 2.3 | Rename event queue symbols | `MyEvtQA` → `EvtQA`, `MyEvtQIn` → `EvtQIn`, `MyEvtQOut` → `EvtQOut`, `MyEvtQNeedRecover` → `EvtQNeedRecover`, `MyEvtQEl` → `EvtQEl`, `MyEvtQOutP()` → `EvtQOutP()`, `MyEvtQElPreviousIn()` → `EvtQElPreviousIn()`, `MyEvtQElAlloc()` → `EvtQElAlloc()`, `MasterMyEvtQLock` → `MasterEvtQLock` | `platform/platform.h`, `platform/common/osglu_common.h`, `platform/common/osglu_common.cpp`, `platform/sdl.cpp`, `core/machine.h` |
| 2.4 | Rename misc `My`-prefixed globals | `MyMouseButtonState` → `g_mouseButtonState`, `MyMousePosCurV` → `g_mousePosCurV`, `MyMousePosCurH` → `g_mousePosCurH` | `platform/common/osglu_common.h`, `platform/common/osglu_common.cpp`, `platform/sdl.cpp`, `platform/common/control_mode.cpp` |
| 2.5 | Rename `my_disk_icon_addr` | → `g_diskIconAddr` | `core/machine.h`, `core/machine.cpp`, `devices/sony.cpp` |

**Verification:** `cd test && ./verify.sh` passes. `grep -rn '\bMy[A-Z]\|my_' src/ --include='*.cpp' --include='*.h' | grep -v src/cpu/` returns nothing.

---

## Phase 3 — Rename `t`-Prefixed Type Aliases

**Goal:** Replace `t`-prefixed application types with PascalCase.

| # | Task | Rename | Files |
|---|------|--------|-------|
| 3.1 | Rename `tPbuf` | → `PbufIndex` | `platform/platform.h` + all users |
| 3.2 | Rename `tDrive` | → `DriveIndex` | `core/emulation_config.h`, `platform/platform.h` + all users |
| 3.3 | Rename `trSoundSamp`, `tbSoundSamp`, `tpSoundSamp` | → `RawSoundSample`, `BufferedSoundSample`, `SoundSamplePtr` | `platform/platform.h`, `platform/sdl_sound.cpp` |
| 3.4 | Rename `trSoundTemp` | → `SoundTemp` | `platform/sdl_sound.cpp` |
| 3.5 | Rename `NotAPbuf` macro | → `NOT_A_PBUF` | `platform/platform.h` + all users |

**Note:** `tMacErr` is intentionally preserved — it mirrors a Mac OS
type and is scoped via `enum class`. Renaming it would hurt readability
for anyone familiar with classic Mac programming.

**Verification:** `cd test && ./verify.sh` passes. `grep -rn '\bt[A-Z][a-z]' src/ --include='*.cpp' --include='*.h' | grep -v src/cpu/ | grep -v tMacErr` returns nothing.

---

## Phase 4 — File-Scope Statics → `s_camelCase`

**Goal:** Rename PascalCase file-scope statics to `s_camelCase`.

Split by file to keep diffs reviewable.

| # | File | Renames |
|---|------|---------|
| 4.1 | `platform/sdl.cpp` | `UseFullScreen` → `s_useFullScreen`, `UseMagnify` → `s_useMagnify`, `CurSpeedStopped` → `s_curSpeedStopped`, `WindowScale` → `s_windowScale`, `HaveCursorHidden` → `s_haveCursorHidden`, `WantCursorHidden` → `s_wantCursorHidden`, `CaughtMouse` → `s_caughtMouse`, `GrabMachine` → `s_grabMachine`, `CurWinIndx` → `s_curWinIndx`, `gBackgroundFlag` → `s_backgroundFlag`, `gTrueBackgroundFlag` → `s_trueBackgroundFlag` |
| 4.2 | `platform/sdl_sound.cpp` | `TheSoundBuffer` → `s_soundBuffer`, `MaxFilledSoundBuffs` → `s_maxFilledSoundBuffs`, `TheWriteOffset` → `s_writeOffset`, `HaveSoundOut` → `s_haveSoundOut` |
| 4.3 | `core/main.cpp` | `SubTickCounter` → `s_subTickCounter`, `ticksSinceSecond` → `s_ticksSinceSecond`, `ExtraSubTicksToDo` → `s_extraSubTicksToDo`, `CurEmulatedTime` → `s_curEmulatedTime` |
| 4.4 | `core/machine.cpp` | `GotOneAbnormal` → `s_gotOneAbnormal`, `ParamAddrHi` → `s_paramAddrHi`, `LastATTel` → `s_lastATTel`, `CurIPL` → `s_curIPL` |
| 4.5 | `devices/sony.cpp` | `vSonyMountedMask` → `s_sonyMountedMask`, `DelayUntilNextInsert` → `s_delayUntilNextInsert`, `MountCallBack` → `s_mountCallBack`, `QuitOnEject` → `s_quitOnEject`, `TheTagBuffer` → `s_tagBuffer` |
| 4.6 | `devices/adb_shared.h` | `ADB_TalkDatBuf` → `s_adbTalkDatBuf`, `NotSoRandAddr` → `s_notSoRandAddr`, `MouseADBAddress` → `s_mouseADBAddress`, `SavedCurMouseButton` → `s_savedCurMouseButton`, `MouseADBDeltaH` → `s_mouseADBDeltaH`, `MouseADBDeltaV` → `s_mouseADBDeltaV`, `KeyboardADBAddress` → `s_keyboardADBAddress` |
| 4.7 | `devices/scc.cpp` | `CTSpacketPending` → `s_ctsPacketPending`, `CTSpacketRxDA` → `s_ctsPacketRxDA`, `CTSpacketRxSA` → `s_ctsPacketRxSA`, `IsFindingNode` → `s_isFindingNode`, `LTAddrSrchMd` → `s_ltAddrSrchMd`, `SCC` → `s_scc` |
| 4.8 | `devices/rtc.cpp` | `RTC` → `s_rtc`, `LastRealDate` → `s_lastRealDate` |
| 4.9 | `devices/video.cpp` | `UseGrayTones` → `s_useGrayTones` |
| 4.10 | `devices/asc.cpp` | `ASC_Playing` → `s_ascPlaying` |
| 4.11 | `platform/common/tick_timer.cpp` | `LastTime` → `s_lastTime`, `NextIntTime` → `s_nextIntTime`, `NextFracTime` → `s_nextFracTime` |
| 4.12 | `platform/common/osglu_common.cpp` | `NextDrawRow` → `s_nextDrawRow` |
| 4.13 | `platform/common/alt_keys.h` | `AltKeysLockText` → `s_altKeysLockText`, `AltKeysTrueCmnd` → `s_altKeysTrueCmnd`, `AltKeysTrueOption` → `s_altKeysTrueOption`, `AltKeysTrueShift` → `s_altKeysTrueShift`, `AltKeysModOn` → `s_altKeysModOn`, `AltKeysTextOn` → `s_altKeysTextOn` |
| 4.14 | `lang/localization.cpp` | `g_currentLang` → `s_currentLang`, `g_initialized` → `s_initialized` (these are file-scope, not true globals) |

**Verification:** `cd test && ./verify.sh` passes. `grep -rn 'static.*[A-Z][a-z].*=' src/ --include='*.cpp' --include='*.h' | grep -v src/cpu/ | grep -v 'static constexpr\|static const\|static.*s_'` returns only known exceptions.

---

## Phase 5 — Bare Globals → `g_` Prefix

**Goal:** Add `g_` prefix to all non-prefixed global variables.

Split into subsets by semantic grouping.

| # | Group | Renames | Files |
|---|-------|---------|-------|
| 5.1 | Memory pointers | `RAM` → `g_ram`, `ROM` → `g_rom`, `VidMem` → `g_vidMem`, `VidROM` → `g_vidROM` | `core/machine.h`, `platform/platform.h`, `platform/common/osglu_common.h` + all users |
| 5.2 | Screen state | `screencomparebuff` → `g_screenCompareBuff`, `ScreenChangedTop/Left/Bottom/Right` → `g_screenChangedTop/Left/Bottom/Right`, `ScreenChangedQuiet*` → `g_screenChangedQuiet*`, `ViewHSize/VSize/HStart/VStart` → `g_viewHSize/VSize/HStart/VStart` | `platform/common/osglu_common.h` + all users |
| 5.3 | Mouse state | `SavedMouseH/V` → `g_savedMouseH/V`, `HaveMouseMotion` → `g_haveMouseMotion`, `CurMouseV/H` → `g_curMouseV/H` | `platform/common/osglu_common.h`, `platform/platform.h` + all users |
| 5.4 | Sony / disk state | `vSonyWritableMask` → `g_sonyWritableMask`, `vSonyInsertedMask` → `g_sonyInsertedMask`, `vSonyRawMode` → `g_sonyRawMode`, `vSonyNewDiskWanted` → `g_sonyNewDiskWanted`, `vSonyNewDiskSize` → `g_sonyNewDiskSize`, `vSonyNewDiskName` → `g_sonyNewDiskName` | `platform/platform.h` + all users |
| 5.5 | Time / clock | `OnTrueTime` → `g_onTrueTime`, `CurMacDateInSeconds` → `g_curMacDateInSeconds`, `CurMacLatitude` → `g_curMacLatitude`, `CurMacLongitude` → `g_curMacLongitude`, `CurMacDelta` → `g_curMacDelta`, `TrueEmulatedTime` → `g_trueEmulatedTime`, `NewMacDateInSeconds` → `g_newMacDateInSeconds` | `platform/platform.h`, `platform/common/tick_timer.h` + all users |
| 5.6 | Video / color | `UseColorMode` → `g_useColorMode`, `ColorModeWorks` → `g_colorModeWorks`, `ColorMappingChanged` → `g_colorMappingChanged`, `ColorTransValid` → `g_colorTransValid` | `platform/platform.h`, `platform/common/osglu_common.h` + all users |
| 5.7 | Control flags | `ROM_loaded` → `g_romLoaded`, `RequestMacOff` → `g_requestMacOff`, `ForceMacOff` → `g_forceMacOff`, `WantMacInterrupt` → `g_wantMacInterrupt`, `WantMacReset` → `g_wantMacReset`, `EmVideoDisable` → `g_emVideoDisable`, `EmLagTime` → `g_emLagTime`, `SpeedValue` → `g_speedValue`, `WantNotAutoSlow` → `g_wantNotAutoSlow`, `InterruptButton` → `g_interruptButton` | `platform/platform.h`, `platform/common/osglu_common.h`, `core/machine.h` + all users |
| 5.8 | UI / control mode | `SpecialModes` → `g_specialModes`, `NeedWholeScreenDraw` → `g_needWholeScreenDraw`, `CntrlDisplayBuff` → `g_cntrlDisplayBuff`, `SpeedStopped` → `g_speedStopped`, `RunInBackground` → `g_runInBackground`, `WantFullScreen` → `g_wantFullScreen`, `WantMagnify` → `g_wantMagnify`, `RequestInsertDisk` → `g_requestInsertDisk`, `RequestIthDisk` → `g_requestIthDisk`, `ControlKeyPressed` → `g_controlKeyPressed` | `platform/common/control_mode.h`, `platform/common/intl_chars.h` + all users |
| 5.9 | Networking / LocalTalk | `LT_NodeHint` → `g_ltNodeHint`, `CertainlyNotMyPacket` → `g_certainlyNotMyPacket`, `LT_TxBuffer` → `g_ltTxBuffer`, `LT_TxBuffSz` → `g_ltTxBuffSz`, `LT_RxBuffer` → `g_ltRxBuffer`, `LT_RxBuffSz` → `g_ltRxBuffSz`, `LT_MyStamp` → `g_ltMyStamp` | `platform/platform.h`, `platform/common/osglu_common.h` + all users |
| 5.10 | Misc | `Wires` → `g_wires`, `PbufAllocatedMask` → `g_pbufAllocatedMask`, `SavedIDMsg` → `g_savedIDMsg`, `SavedFatalMsg` → `g_savedFatalMsg`, `QuietTime` → `g_quietTime`, `QuietSubTicks` → `g_quietSubTicks` | `core/machine.h`, `platform/platform.h`, `platform/common/osglu_common.h` + all users |

**Verification:** `cd test && ./verify.sh` passes. `grep -rn '^extern ' src/ --include='*.h' | grep -v 'g_\|src/cpu/'` returns only `ROM` (if kept) or nothing.

---

## Phase 6 — PascalCase `#define` Macros → `UPPER_SNAKE_CASE`

**Goal:** Rename all PascalCase `#define` macros to `UPPER_SNAKE_CASE`.

Split by file. The `kXxx` named constants that are `#define` (not
`constexpr`) are included — they are macros and should follow macro
naming. Constants that are better expressed as `constexpr` should be
converted first (see notes).

| # | File | Scope | Notes |
|---|------|-------|-------|
| 6.1 | `config/CNFUDPIC.h` | `Use68020` → `USE_68020`, `EmFPU` → `EM_FPU`, `EmMMU` → `EM_MMU`, `WantCycByPriOp` → `WANT_CYC_BY_PRI_OP`, `WantCloserCyc` → `WANT_CLOSER_CYC`, `IncludeExtnPbufs` → `INCLUDE_EXTN_PBUFS`, `IncludeExtnHostTextClipExchange` → `INCLUDE_EXTN_HOST_TEXT_CLIP_EXCHANGE`, `Sony_SupportDC42` → `SONY_SUPPORT_DC42`, `Sony_SupportTags` → `SONY_SUPPORT_TAGS`, `Sony_WantChecksumsUpdated` → `SONY_WANT_CHECKSUMS_UPDATED`, `Sony_VerifyChecksums` → `SONY_VERIFY_CHECKSUMS`, `WantDisasm` → `WANT_DISASM`, `ExtraAbnormalReports` → `EXTRA_ABNORMAL_REPORTS` | High-traffic — touches many files via `#if`. Run `grep -rn` per macro to map all usage sites before renaming. |
| 6.2 | `core/types.h` | `cIncludeUnused` → `C_INCLUDE_UNUSED`, `BigEndianUnaligned` → `BIG_ENDIAN_UNALIGNED`, `LittleEndianUnaligned` → `LITTLE_ENDIAN_UNALIGNED`, `Have_ASR` → `HAVE_ASR`, `HaveMySwapUi5r` → `HAVE_SWAP_UI5R`, `LIT64` → `LIT64` (already uppercase) | |
| 6.3 | `platform/common/osglu_common.h` | `EnableFSMouseMotion` → `ENABLE_FS_MOUSE_MOTION`, `EnableRecreateW` → `ENABLE_RECREATE_W`, `EnableMoveMouse` → `ENABLE_MOVE_MOUSE`, `GrabKeysFullScreen` → `GRAB_KEYS_FULL_SCREEN`, `GrabKeysMaxFullScreen` → `GRAB_KEYS_MAX_FULL_SCREEN`, `PowOf2` → `POW_OF_2`, `Pow2Mask` → `POW2_MASK`, `ModPow2` → `MOD_POW2`, `FloorDivPow2` → `FLOOR_DIV_POW2`, `FloorPow2Mult` → `FLOOR_POW2_MULT`, `CeilPow2Mult` → `CEIL_POW2_MULT`, `UnusedParam` → fold into `UNUSED` | |
| 6.4 | `devices/via_base.cpp` | `BitMask` → `BIT_MASK`, `TestBit` → `TEST_BIT`, `CyclesPerViaTime` → `CYCLES_PER_VIA_TIME`, `CyclesScaledPerViaTime` → `CYCLES_SCALED_PER_VIA_TIME` | Local-only macros. |
| 6.5 | `platform/sdl.cpp` | `HaveWorkingWarp` → `HAVE_WORKING_WARP`, `UseMotionEvents` → `USE_MOTION_EVENTS`, `CLUT_finalsz` → `CLUT_FINAL_SZ`, `dbglog_OSGInit` → `DBGLOG_OSG_INIT` | |
| 6.6 | `platform/sdl_sound.cpp` | `AudioStepVal` → `AUDIO_STEP_VAL`, `ConvertTempSoundSampleFromNative` → `CONVERT_TEMP_SOUND_SAMPLE_FROM_NATIVE`, `ConvertTempSoundSampleToNative` → `CONVERT_TEMP_SOUND_SAMPLE_TO_NATIVE`, `kCenterTempSound` → `K_CENTER_TEMP_SOUND` | |
| 6.7 | `devices/keyboard.cpp` | `MaxKeyboardWait` → `MAX_KEYBOARD_WAIT` | |

**Note on `kXxx` `#define` constants:** Many `kXxx` macros in
`core/machine.cpp`, `devices/sony.cpp`, `devices/via_base.cpp`, and
`devices/video.cpp` are integer constants that should ideally become
`constexpr` rather than macros. When converting:

1. If the macro is a simple literal → convert to `constexpr` and keep
   the `kXxx` name (matches `constexpr` constant convention).
2. If the macro wraps a function call (e.g., `kRAM_ln2Spc` wraps
   `addrmap_kRAM_ln2Spc()`) → convert to an inline function and rename
   to PascalCase (e.g., `GetRAMLn2Spc()`), or keep as a macro with
   `UPPER_SNAKE_CASE`.

| # | File | Scope |
|---|------|-------|
| 6.8 | `core/machine.cpp` | Convert simple `kXxx` `#define`s to `constexpr` (≈ 40 constants for address map, command IDs, extension IDs). Leave function-wrapping macros as-is until Phase 7. |
| 6.9 | `devices/via_base.cpp` | Convert `kIntCA2`..`kIntT1`, `kORB`..`kORA` to `constexpr` with same `kXxx` names. |
| 6.10 | `devices/sony.cpp` | Convert `kDC42offset_*`, `kCmndDisk*`, `kFeatureCmndDisk_*`, `kParamDisk*`, `kTrack`..`kDriveInfo` to `constexpr`. |
| 6.11 | `devices/video.cpp` | Convert `kCmndVideo*`, `VidBaseAddr` to `constexpr`. |
| 6.12 | `devices/screen.cpp` | Convert `kMain_Offset`, `kAlternate_Offset` to `constexpr`. |
| 6.13 | `devices/sound.cpp` | Convert `kSnd_Main_Offset`, `kSnd_Alt_Offset` to `constexpr`. |
| 6.14 | `devices/scsi.cpp` | Convert `kSCSI_Size` to `constexpr`. |
| 6.15 | `devices/hpmac_hack.h` | Convert `kAHM_*` defines to a proper `enum` or `constexpr` values. |

---

## Phase 7 — Address-Map Macros in `machine.cpp`

**Goal:** Replace function-wrapping `#define` macros with proper inline
functions or `Machine` methods.

The macros `kRAM_ln2Spc`, `kVidMem_Base`, `kSCSI_Block_Base`, etc. in
`core/machine.cpp` wrap `static` functions that query `g_machine->config()`.
These should become either:

- Named free functions: `GetRAMLn2Spc()`, `GetVidMemBase()`, etc.
- Or `Machine` member functions.

| # | Task |
|---|------|
| 7.1 | Replace each `#define kXxx addrmap_kXxx()` pair with a single inline function. Remove the `addrmap_` prefix and the macro. |
| 7.2 | Update all call sites (all within `machine.cpp`). |

---

## Phase 8 — `Module_Function` Free Functions

**Goal:** Where a `Module_Function` free function has a natural class
home, move it there as a camelCase method.

| # | Function | Target |
|---|----------|--------|
| 8.1 | `Extn_Reset()`, `Extn_Access()` | → `Machine::extnReset()`, `Machine::extnAccess()` (or a future `ExtnDevice`) |
| 8.2 | `Memory_Reset()` | → `Machine::memoryReset()` |
| 8.3 | `ICT_Zap()`, `ICT_add()` | Already backed by `ICTScheduler` — make these methods if not already |
| 8.4 | `Keyboard_UpdateKeyMap2()`, `Keyboard_RemapMac()` | → `KeyboardDevice` methods (or platform bridge class) |
| 8.5 | `Sony_Insert1a()`, `Sony_Insert2()`, `Sony_InsertIth()` | Platform-layer disk helpers — consider a `DiskManager` class or namespace |
| 8.6 | `Screen_Init()` | → platform init sequence (namespace or class method) |
| 8.7 | `Drive_Transfer()`, `Drive_Eject()`, etc. in `sony.cpp` | → `SonyDevice` methods |

**Note:** Some `Module_Function` names are fine as free functions when
there is no natural class (e.g., `NativeStrFromCStr`). Don't force
everything into a class.

---

## Execution Notes

1. **One commit per numbered task.** Keeps blame clean and makes
   bisection possible.

2. **Run `cd test && ./verify.sh` after every commit.** The reference test suite
   is the ground truth. A naming change that breaks it is wrong.

3. **Use editor rename / `sed` for mechanical renames.** Most of these
   tasks are pure search-and-replace. Use `grep -rn` to find all sites
   before starting, and `grep -rn` again afterwards to confirm zero
   residual references.

4. **Beware stringified names.** Some identifiers appear in debug
   strings or `name()` methods. A text search will catch these.

5. **Phase order matters loosely.** Phases 1–3 are cleanest first.
   Phase 4–6 are medium effort. Phase 7–9 involve structural changes.
   But any phase can be done independently.

6. **Update `NAMING.md` legacy table** after each phase completes to
   remove entries that no longer apply.
