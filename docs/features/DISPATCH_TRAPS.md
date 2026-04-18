# First-Class Dispatch Trap Subtraps

Promote dispatch-trap subtraps (e.g. `PBGetCatInfo` inside
`HFSDispatch`) to first-class citizens in the tracing, breakpoint,
and counting systems.

## Problem

Twelve Mac OS traps are dispatchers that multiplex dozens of
sub-functions through a selector register:

| Trap | Word | Selector | Convention | Subtraps |
|------|------|----------|------------|----------|
| HFSDispatch | A260 | word in D0 | OS | 11 |
| SCSIDispatch | A815 | sword on stack | Toolbox | 14 |
| SlotManager | A06E | SpBlock pointer → spSlot | OS | ~30 |
| TEDispatch | A83D | sword on stack | Toolbox | 10 |
| Shutdown | A895 | sword on stack | Toolbox | 4 |
| ScriptUtil | A8B5 | long on stack | Toolbox | ~17 |
| Pack0 | A9E7 | sword on stack | Toolbox | (List Manager) |
| Pack2 | A9E9 | sword on stack | Toolbox | (Disk Init) |
| Pack3 | A9EA | sword on stack | Toolbox | (Standard File) |
| Pack6 | A9ED | sword on stack | Toolbox | (Intl Utilities) |
| Pack7 | A9EE | sword on stack | Toolbox | (Binary-Decimal) |
| Pack12 | A82E | sword on stack | Toolbox | (Color Picker) |

Today these are opaque: the tracer logs `HFSDispatch(selector:9)`,
breakpoints can only target the parent trap, and counters aggregate
all sub-calls under one bucket. The selector value is never resolved
to a human-readable name.

## Goal

Subtraps behave like real traps in every subsystem:

1. **Tracing** — `trace traps on` shows `PBGetCatInfo(pb:$00ABCDEF)`
   with full typed parameters and struct-field dumps, not
   `HFSDispatch(selector:9, pb:$...)`.
2. **Breakpoints** — `break PBGetCatInfo` works. `break HFSDispatch`
   still breaks on every HFS sub-call.
3. **Counting** — each subtrap has its own counter. Watchlist can
   include individual subtraps.
4. **Name resolution** — `info traps GetCat*` lists `PBGetCatInfo`
   alongside regular traps. Tab completion works.
5. **Filtering** — `trace traps PBGetCatInfo PBOpenWD` traces only
   those two HFS calls, not all of HFSDispatch.

The fact that a subtrap is dispatched through a parent trap is an
implementation detail invisible to the user.

## Performance

A perf hit is acceptable. Reading the selector register on every
dispatch-trap entry is cheap (one `m68k_getRegs()` call that the
tracer already makes). The runtime cost is zero when tracing/
breakpoints/counting are inactive.

## Subtraps to Define

### HFSDispatch (A260) — priority

| Selector | Name | PB Type |
|----------|------|---------|
| 0x01 | PBOpenWD | WDParam |
| 0x02 | PBCloseWD | WDParam |
| 0x05 | PBCatMove | CInfoPBRec |
| 0x06 | PBDirCreate | HFileInfo |
| 0x07 | PBGetWDInfo | WDParam |
| 0x08 | PBGetFCBInfo | FCBPBRec |
| 0x09 | PBGetCatInfo | CInfoPBRec |
| 0x0A | PBSetCatInfo | CInfoPBRec |
| 0x0B | PBSetVInfo | VolumeParam |
| 0x10 | PBCreateFileIDRef | HFileInfo |
| 0x11 | PBDeleteFileIDRef | HFileInfo |
| 0x30 | PBGetVolParms | HFileInfo |

### SCSIDispatch (A815)

| Selector | Name |
|----------|------|
| 0 | SCSIReset |
| 1 | SCSIGet |
| 2 | SCSISelect |
| 3 | SCSICmd |
| 4 | SCSIComplete |
| 5 | SCSIRead |
| 6 | SCSIWrite |
| 7 | SCSIInstall |
| 8 | SCSIRBlind |
| 9 | SCSIWBlind |
| 10 | SCSIStat |
| 11 | SCSISelAtn |
| 12 | SCSIMsgIn |
| 13 | SCSIMsgOut |

### TEDispatch (A83D)

| Selector | Name |
|----------|------|
| 0 | TEStylePaste |
| 1 | TESetStyle |
| 2 | TEReplaceStyle |
| 3 | TEGetStyle |
| 4 | GetStyleHandle |
| 5 | SetStyleHandle |
| 6 | GetStyleScrap |
| 7 | TEStyleInsert |
| 8 | TEGetPoint |
| 9 | TEGetHeight |

### Shutdown (A895)

| Selector | Name |
|----------|------|
| 1 | ShutDwnPower |
| 2 | ShutDwnStart |
| 3 | ShutDwnInstall |
| 4 | ShutDwnRemove |

### SlotManager (A06E)

| Selector | Name |
|----------|------|
| 0 | sReadByte |
| 1 | sReadWord |
| 2 | sReadLong |
| 3 | sGetcString |
| 5 | sGetBlock |
| 6 | sFindStruct |
| 7 | sReadStruct |
| 16 | sReadInfo |
| 17 | sReadPRAMRec |
| 18 | sPutPRAMRec |
| 19 | sReadFHeader |
| 20 | sNextRsrc |
| 21 | sNextTypesRsrc |
| 22 | sRsrcInfo |
| 23 | sDisposePtr |
| 24 | sCkCardStatus |
| 25 | sReadDrvrName |
| 27 | sFindDevBase |
| 32 | InitSDeclMgr |
| 33 | sPrimaryInit |
| 34 | sCardChanged |
| 35 | sExec |
| 36 | sOffsetData |
| 37 | InitPRAMRecs |
| 38 | sReadPBSize |
| 40 | sCalcStep |
| 41 | InitsRsrcTable |
| 42 | sSearchSRT |
| 43 | sUpdateSRT |
| 44 | sCalcsPointer |
| 45 | sGetDriver |
| 46 | sPtrToSlot |
| 47 | sFindsInfoRecPtr |
| 48 | sFindsRsrcPtr |
| 49 | sdeleteSRTRec |

### ScriptUtil (A8B5)

| Selector | Name |
|----------|------|
| 0 | smFontScript |
| 2 | smIntlScript |
| 4 | smKybdScript |
| 6 | smFont2Script |
| 8 | smGetEnvirons |
| 10 | smSetEnvirons |
| 12 | smGetScript |
| 14 | smSetScript |
| 16 | smCharByte |
| 18 | smCharType |
| 20 | smPixel2Char |
| 22 | smChar2Pixel |
| 24 | smTranslit |
| 26 | smFindWord |
| 28 | smHiliteText |
| 30 | smDrawJust |
| 32 | smMeasureJust |

### Packages (Pack0/2/3/6/7/12)

Deferred. These can be added incrementally once the mechanism
is proven with HFSDispatch and SCSI.

## Design

See [DISPATCH_TRAPS_DESIGN.md](DISPATCH_TRAPS_DESIGN.md).
