# Trap Definitions — Detailed Implementation Plan

Spec: [PLAN.md](PLAN.md)

Make `assets/traps.def` the authoritative, typed catalogue of every
System 6 trap.

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Audit — cross-reference TRAP_TABLE.md vs traps.def | ✅ done |
| 2 | Add missing struct types to types.def | ✅ done |
| 3 | Fill OS trap stubs (register conventions) | ✅ done |
| 4 | Fill Toolbox trap stubs — QuickDraw | ✅ done |
| 5 | Fill Toolbox trap stubs — Window & Menu managers | ✅ done |
| 6 | Fill Toolbox trap stubs — Control, Dialog, TextEdit | ✅ done |
| 7 | Fill Toolbox trap stubs — Event, Scrap, Sound, Misc | ✅ done |
| 8 | Fill Toolbox trap stubs — Color Toolbox & Palette | ✅ done |
| 9 | Add missing traps from TRAP_TABLE.md | ✅ no-op (0 missing) |
| 10 | Comment out post-System 6 traps | ✅ no-op (0 candidates) |
| 11 | Validate — build, test, spot-check | ✅ done |

Build gate: `cmake --build bld/macos`
Test gate:  `./bld/macos/tests`

---

## Current State

- **435 trap entries** total: 57 fully typed, 378 stubs (header only).
- Fully typed managers: Memory Manager (9), Resource Manager (22),
  File Manager (13), plus a handful of Event/Window/Misc traps.
- Stub-only: 88 OS traps, 289 Toolbox traps in the "additional"
  sections at the bottom of the file.
- Reference material:
  - `macdocs/ref/TRAP_TABLE.md` — 926-line Inside Macintosh trap table
    with trap words for every System 6 trap (alphabetical).
  - `macdocs/THINK_C_3/Mac #includes/` — 42 header files with pascal
    declarations and type definitions.
  - `macdocs/tech_doc/im202.html` — Inside Macintosh vol I–V (full
    HTML), containing register conventions and parameter descriptions.

---

## Phase 1 — Audit: Cross-Reference TRAP_TABLE.md vs traps.def

Produce a machine-readable gap list by comparing the two files.

### 1.1 — Extract trap words from TRAP_TABLE.md

Parse `macdocs/ref/TRAP_TABLE.md` to extract every `| Name | Trap Word |`
row.  Produce a sorted list of `(trapWord, name)` pairs.  Expected
count: ~500 entries.

### 1.2 — Extract trap words from traps.def

Parse `assets/traps.def` to extract every header line
`<trapWord> <name> <convention>`.  Note which have `in`/`out` lines
(typed) vs stubs.  Expected: 435 entries, 57 typed.

### 1.3 — Produce gap report

Compare the two lists:
- **Missing from traps.def** — traps in TRAP_TABLE.md but not in
  traps.def.  These need to be added (Phase 9).
- **Missing from TRAP_TABLE.md** — traps in traps.def but not in
  Inside Macintosh.  Candidates for comment-out (Phase 10).
- **Present but stub** — traps in both files that have no `in`/`out`
  lines.  These need parameters added (Phases 3–8).

Output as a markdown checklist in `assets/AUDIT.md` (temporary;
delete after Phase 11).  Group by manager/category.

### Fence

- [ ] `assets/AUDIT.md` exists with the gap report
- [ ] Every trap in TRAP_TABLE.md is accounted for
- [ ] Every trap in traps.def is accounted for
- [ ] Commit: `"traps: phase 1 — audit and gap list"`

---

## Phase 2 — Add Missing Struct Types to types.def

Add struct definitions required by trap parameters that do not exist
yet in `assets/types.def`.

### 2.1 — Identify needed structs

The following structs are needed for trap parameters but not currently
in types.def.  Verify each against THINK C 3 headers.

**EventRecord** (EventMgr.h):
```
struct EventRecord {
    0  sword    what
    2  long     message
    6  long     when
    10 Point    where
    14 word     modifiers
}
```

**ControlRecord** (ControlMgr.h) — partial, key fields only:
```
struct ControlRecord {
    0  Ptr      nextControl
    4  Ptr      contrlOwner
    8  Rect     contrlRect
    16 byte     contrlVis
    17 byte     contrlHilite
    18 sword    contrlValue
    20 sword    contrlMin
    22 sword    contrlMax
    24 Handle   contrlDefProc
    28 Handle   contrlData
    32 Handle   contrlAction
    36 slong    contrlRfCon
    40 Str255   contrlTitle
}
```

**DialogRecord** (DialogMgr.h):
```
struct DialogRecord {
    0   WindowRecord window
    156 Handle   items
    160 Handle   textH
    164 sword    editField
    166 sword    editOpen
    168 sword    aDefItem
}
```

**MenuInfo** (MenuMgr.h):
```
struct MenuInfo {
    0  sword    menuID
    2  sword    menuWidth
    4  sword    menuHeight
    6  Handle   menuProc
    10 long     enableFlags
    14 Str255   menuData
}
```

**TERecord** (TextEdit.h) — partial, key fields:
```
struct TERecord {
    0  Rect     destRect
    8  Rect     viewRect
    16 Rect     selRect
    24 sword    lineHeight
    26 sword    fontAscent
    28 Point    selPoint
    32 sword    selStart
    34 sword    selEnd
    36 sword    active
    38 ProcPtr  wordBreak
    42 ProcPtr  clikLoop
    46 Handle   hText
    50 sword    teLength
    52 sword    txFont
    54 sword    txFace
    56 sword    txMode
    58 sword    txSize
    60 Handle   inPort
    64 ProcPtr  highHook
    68 ProcPtr  caretHook
    72 sword    nLines
}
```

**PenState** (Quickdraw.h):
```
struct PenState {
    0  Point    pnLoc
    4  Point    pnSize
    8  sword    pnMode
    10 byte     pnPat[8]
}
```

**FontInfo** (FontMgr.h):
```
struct FontInfo {
    0  sword    ascent
    2  sword    descent
    4  sword    widMax
    6  sword    leading
}
```

**DateTimeRec** (OSUtil.h):
```
struct DateTimeRec {
    0  sword    year
    2  sword    month
    4  sword    day
    6  sword    hour
    8  sword    minute
    10 sword    second
    12 sword    dayOfWeek
}
```

**SysEnvRec** (OSUtil.h):
```
struct SysEnvRec {
    0  sword    environsVersion
    2  sword    machineType
    4  sword    systemVersion
    6  sword    processor
    8  Boolean  hasFPU
    9  Boolean  hasColorQD
    10 sword    keyBoardType
    12 sword    atDrvrVersNum
    14 sword    sysVRefNum
}
```

### 2.2 — Add structs to types.def

Add each struct to `assets/types.def` under appropriate section
headers.  Follow the existing format:
- Decimal byte offsets
- Primitive type or previously-defined struct name
- Use THINK C 3 header field names exactly

### 2.3 — Verify types load

Run `./bld/macos/tests` — the TypeRegistry loader must not emit any
"unknown type" warnings on stderr.

### Fence

- [ ] EventRecord, ControlRecord, DialogRecord, MenuInfo, TERecord,
      PenState, FontInfo, DateTimeRec, SysEnvRec added to types.def
- [ ] `./bld/macos/tests` passes
- [ ] Commit: `"traps: phase 2 — add missing struct types"`

---

## Phase 3 — Fill OS Trap Stubs (Register Conventions)

Add `in`/`out` lines to all 88 OS trap stubs.  OS traps use register
conventions: parameters in A0/D0, results in A0/D0.

### Reference

- Inside Macintosh vol II, chapter 4 (OS traps).
- `macdocs/tech_doc/im202.html` — search for trap name to find
  the register-level interface.
- General pattern: A0 = pointer/handle param, D0 = integer param
  or result.  For PB-based traps (File Manager style): A0 = ^ParamBlock,
  D0 = OSErr result.

### 3.1 — Memory Manager OS stubs

These are mostly single-param traps.  Examples:

```
A04C CompactMem os
  in  size:long.D0
  out size:long.D0

A01C FreeMem os
  out size:long.D0

A11A GetZone os
  out zone:Ptr.A0

A01B SetZone os
  in  zone:Ptr.A0

A063 MaxApplZone os

A036 MoreMasters os

A061 MaxBlock os
  out size:long.D0

A11D MaxMem os
  out size:long.D0  grow:long.A0

A02B EmptyHandle os
  in  h:Handle.A0

A04A HNoPurge os
  in  h:Handle.A0

A049 HPurge os
  in  h:Handle.A0

A066 NewEmptyHandle os
  out h:Handle.A0  err:OSErr.D0

A128 RecoverHandle os
  in  p:Ptr.A0
  out h:Handle.A0

A148 PtrZone os
  in  p:Ptr.A0
  out zone:Ptr.A0

A126 HandleZone os
  in  h:Handle.A0
  out zone:Ptr.A0

A027 ReallocHandle os
  in  h:Handle.A0  size:long.D0
  out err:OSErr.D0

A069 HGetState os
  in  h:Handle.A0
  out state:byte.D0

A06A HSetState os
  in  h:Handle.A0  state:byte.D0

A067 HSetRBit os
  in  h:Handle.A0

A068 HClrRBit os
  in  h:Handle.A0

A064 MoveHHi os
  in  h:Handle.A0

A04D PurgeMem os
  in  size:long.D0

A062 PurgeSpace os
  out total:long.D0  contig:long.A0

A065 StackSpace os
  out size:long.D0

A020 SetPtrSize os
  in  p:Ptr.A0  size:long.D0
  out err:OSErr.D0

A021 GetPtrSize os
  in  p:Ptr.A0
  out size:long.D0

A02C InitApplZone os

A02D SetApplLimit os
  in  limit:Ptr.A0

A057 SetAppBase os
  in  base:Ptr.A0

A04B SetGrowZone os
  in  proc:ProcPtr.A0

A019 InitZone os
```

### 3.2 — File Manager OS stubs

PB-based traps follow the pattern: `in pb:^ParamBlockType.A0`, `out err:OSErr.D0`.

```
A010 Allocate os
  in  pb:^IOParam.A0
  out err:OSErr.D0
  show-in  pb ioRefNum ioReqCount
  show-out pb ioResult ioActCount

A045 FlushFile os
  in  pb:^IOParam.A0
  out err:OSErr.D0
  show-in  pb ioRefNum
  show-out pb ioResult

A012 SetEOF os
  in  pb:^IOParam.A0
  out err:OSErr.D0
  show-in  pb ioRefNum ioMisc
  show-out pb ioResult

A018 GetFPos os
  in  pb:^IOParam.A0
  out err:OSErr.D0
  show-in  pb ioRefNum
  show-out pb ioResult ioPosOffset

A044 SetFPos os
  in  pb:^IOParam.A0
  out err:OSErr.D0
  show-in  pb ioRefNum ioPosMode ioPosOffset
  show-out pb ioResult ioPosOffset

A015 SetVol os
  in  pb:^VolumeParam.A0
  out err:OSErr.D0
  show-in  pb ioNamePtr ioVRefNum
  show-out pb ioResult

A00E UnmountVol os
  in  pb:^VolumeParam.A0
  out err:OSErr.D0
  show-in  pb ioNamePtr ioVRefNum
  show-out pb ioResult

A00F MountVol os
  in  pb:^VolumeParam.A0
  out err:OSErr.D0

A017 Eject os
  in  pb:^VolumeParam.A0
  out err:OSErr.D0
  show-in  pb ioNamePtr ioVRefNum
  show-out pb ioResult

A035 Offline os
  in  pb:^VolumeParam.A0
  out err:OSErr.D0
  show-in  pb ioNamePtr ioVRefNum
  show-out pb ioResult

A00B Rename os
  in  pb:^IOParam.A0
  out err:OSErr.D0
  show-in  pb ioNamePtr ioVRefNum ioMisc
  show-out pb ioResult

A041 SetFilLock os
  in  pb:^FileParam.A0
  out err:OSErr.D0
  show-in  pb ioNamePtr ioVRefNum
  show-out pb ioResult

A042 RstFilLock os
  in  pb:^FileParam.A0
  out err:OSErr.D0
  show-in  pb ioNamePtr ioVRefNum
  show-out pb ioResult

A043 SetFilType os
  in  pb:^FileParam.A0
  out err:OSErr.D0
  show-in  pb ioNamePtr ioVRefNum
  show-out pb ioResult
```

### 3.3 — Device Manager OS stubs

```
A004 Control os
  in  pb:^IOParam.A0
  out err:OSErr.D0
  show-in  pb ioRefNum
  show-out pb ioResult

A005 Status os
  in  pb:^IOParam.A0
  out err:OSErr.D0
  show-in  pb ioRefNum
  show-out pb ioResult

A006 KillIO os
  in  pb:^IOParam.A0
  out err:OSErr.D0
  show-in  pb ioRefNum
  show-out pb ioResult
```

### 3.4 — VBL / Interrupt OS stubs

```
A033 VInstall os
  in  vbl:Ptr.A0
  out err:OSErr.D0

A034 VRemove os
  in  vbl:Ptr.A0
  out err:OSErr.D0

A071 AttachVBL os
  in  vbl:Ptr.A0
  out err:OSErr.D0

A072 DoVBLTask os
  in  vbl:Ptr.A0

A082 DTInstall os
  in  dt:Ptr.A0
  out err:OSErr.D0
```

### 3.5 — OS Utility stubs

```
A039 ReadDateTime os
  out secs:long.D0 err:OSErr.D0

A03A SetDateTime os
  in  secs:long.D0
  out err:OSErr.D0

A038 WriteParam os
  out err:OSErr.D0

A03B Delay os
  in  ticks:long.A0
  out finalTicks:long.D0

A03C CmpString os
  in  str1:Ptr.A0  str2:Ptr.D0
  out result:sword.D0

A054 UprString os
  in  str:Ptr.A0  len:sword.D0

A050 RelString os
  in  str1:Ptr.A0  str2:Ptr.D0
  out result:sword.D0

A03F InitUtil os

A032 FlushEvents os
  in  mask:word.D0

A030 OSEventAvail os
  in  mask:word.D0  event:Ptr.A0
  out found:Boolean.D0

A031 GetOSEvent os
  in  mask:word.D0  event:Ptr.A0
  out found:Boolean.D0

A02F PostEvent os
  in  what:word.A0  msg:long.D0
  out err:OSErr.D0

A12F PPostEvent os
  in  what:word.A0  msg:long.D0
  out err:OSErr.D0  evtPtr:Ptr.A0

A090 SysEnvirons os
  in  version:sword.D0  env:^SysEnvRec.A0
  out err:OSErr.D0

A055 StripAddress os
  in  addr:Ptr.D0
  out addr:Ptr.D0

A05D SwapMMUMode os
  in  mode:byte.D0
  out oldMode:byte.D0

A047 SetTrapAddress os
  in  addr:Ptr.A0  trapNum:word.D0

A016 InitQueue os
  in  q:Ptr.A0
```

### 3.6 — Driver Install / ADB / Slot OS stubs

```
A03D DrvrInstall os
  in  dce:Ptr.A0  refNum:sword.D0
  out err:OSErr.D0

A03E DrvrRemove os
  in  refNum:sword.D0
  out err:OSErr.D0

A04F RDrvrInstall os
  in  dce:Ptr.A0  refNum:sword.D0
  out err:OSErr.D0

A077 CountADBs os
  out count:sword.D0

A078 GetIndADB os
  in  data:Ptr.A0  index:sword.D0
  out addr:sword.D0

A079 GetADBInfo os
  in  data:Ptr.A0  addr:sword.D0
  out err:OSErr.D0

A07A SetADBInfo os
  in  data:Ptr.A0  addr:sword.D0
  out err:OSErr.D0

A07B ADBReInit os

A07C ADBOp os
  in  data:Ptr.A0  cmd:sword.D0
  out err:OSErr.D0

A07D GetDefaultStartup os
  in  buf:Ptr.A0

A07E SetDefaultStartup os
  in  buf:Ptr.A0

A080 GetVideoDefault os
  in  buf:Ptr.A0

A081 SetVideoDefault os
  in  buf:Ptr.A0

A083 SetOSDefault os
  in  buf:Ptr.A0

A084 GetOSDefault os
  in  buf:Ptr.A0

A06E SlotManager os
  in  pb:Ptr.A0
  out err:OSErr.D0

A06F SlotVInstall os
  in  sq:Ptr.A0
  out err:OSErr.D0

A070 SlotVRemove os
  in  sq:Ptr.A0
  out err:OSErr.D0

A04E AddDrive os
  in  drvrRef:sword.D0  drvNum:sword.D1  queue:Ptr.A0
```

### Fence

- [ ] All 88 OS trap stubs now have `in`/`out` lines
- [ ] `./bld/macos/tests` passes (traps.def loads without warnings)
- [ ] Commit: `"traps: phase 3 — OS trap parameters"`

---

## Phase 4 — Fill Toolbox Stubs: QuickDraw

~130 QuickDraw trap stubs.  Toolbox traps use stack conventions
(pascal calling convention): parameters pushed right-to-left, result
on stack.  No `.register` suffix in traps.def.

### Reference

- Inside Macintosh vol I, chapter 6 (QuickDraw).
- `macdocs/THINK_C_3/Mac #includes/Quickdraw.h`.
- Pascal prototype → traps.def translation:
  `PROCEDURE MoveTo(h, v: INTEGER)` → `in h:sword v:sword`
  `FUNCTION NewRgn: RgnHandle` → `out rgn:Handle`

### 4.1 — Port / Pen / Text primitives (~40 traps)

Add `in`/`out` lines for:

```
A86E InitGraf toolbox
  in  globalPtr:Ptr

A86D InitPort toolbox
  in  port:Ptr

A86F OpenPort toolbox
  in  port:Ptr

A87D CloseCPort toolbox
  in  port:Ptr

A873 SetPort toolbox
  in  port:Ptr

A874 GetPort toolbox
  out port:Ptr

A872 GrafDevice toolbox
  in  device:sword

A876 PortSize toolbox
  in  width:sword  height:sword

A877 MovePortTo toolbox
  in  leftGlobal:sword  topGlobal:sword

A878 SetOrigin toolbox
  in  h:sword  v:sword

A879 SetClip toolbox
  in  rgn:Handle

A87A GetClip toolbox
  in  rgn:Handle

A87B ClipRect toolbox
  in  r:Rect

A87C BackPat toolbox
  in  pat:Ptr

A896 HidePen toolbox

A897 ShowPen toolbox

A898 GetPenState toolbox
  in  penState:^PenState

A899 SetPenState toolbox
  in  penState:^PenState

A89A GetPen toolbox
  in  pt:Point

A89B PenSize toolbox
  in  width:sword  height:sword

A89C PenMode toolbox
  in  mode:sword

A89D PenPat toolbox
  in  pat:Ptr

A89E PenNormal toolbox

A88E SpaceExtra toolbox
  in  extra:long

A887 TextFont toolbox
  in  font:sword

A888 TextFace toolbox
  in  style:sword

A889 TextMode toolbox
  in  mode:sword

A88A TextSize toolbox
  in  size:sword

A88B GetFontInfo toolbox
  in  info:^FontInfo

A882 StdText toolbox
  in  count:sword  text:Ptr  numer:Point  denom:Point

A883 DrawChar toolbox
  in  ch:sword

A884 DrawString toolbox
  in  s:Str255

A885 DrawText toolbox
  in  text:Ptr  start:sword  count:sword

A886 TextWidth toolbox
  in  text:Ptr  start:sword  count:sword
  out width:sword

A88C StringWidth toolbox
  in  s:Str255
  out width:sword

A88D CharWidth toolbox
  in  ch:sword
  out width:sword
```

### 4.2 — Line / Move primitives (~5 traps)

```
A890 StdLine toolbox
  in  newPt:Point

A891 LineTo toolbox
  in  h:sword  v:sword

A892 Line toolbox
  in  dh:sword  dv:sword

A893 MoveTo toolbox
  in  h:sword  v:sword

A894 Move toolbox
  in  dh:sword  dv:sword
```

### 4.3 — Rect primitives (~14 traps)

```
A8A0 StdRect toolbox
  in  verb:byte  r:Rect

A8A1 FrameRect toolbox
  in  r:Rect

A8A2 PaintRect toolbox
  in  r:Rect

A8A3 EraseRect toolbox
  in  r:Rect

A8A4 InverRect toolbox
  in  r:Rect

A8A5 FillRect toolbox
  in  r:Rect  pat:Ptr

A8A6 EqualRect toolbox
  in  r1:Rect  r2:Rect
  out equal:Boolean

A8A7 SetRect toolbox
  in  r:Rect  left:sword  top:sword  right:sword  bottom:sword

A8A8 OffsetRect toolbox
  in  r:Rect  dh:sword  dv:sword

A8A9 InsetRect toolbox
  in  r:Rect  dh:sword  dv:sword

A8AA SectRect toolbox
  in  r1:Rect  r2:Rect  dst:Rect
  out result:Boolean

A8AB UnionRect toolbox
  in  r1:Rect  r2:Rect  dst:Rect

A8AC Pt2Rect toolbox
  in  pt1:Point  pt2:Point  dst:Rect

A8AD PtInRect toolbox
  in  pt:Point  r:Rect
  out inside:Boolean

A8AE EmptyRect toolbox
  in  r:Rect
  out empty:Boolean
```

### 4.4 — RoundRect / Oval / Arc / Poly primitives (~25 traps)

```
A8AF StdRRect toolbox
  in  verb:byte  r:Rect  ovalW:sword  ovalH:sword

A8B0 FrameRoundRect toolbox
  in  r:Rect  ovalW:sword  ovalH:sword

A8B1 PaintRoundRect toolbox
  in  r:Rect  ovalW:sword  ovalH:sword

A8B2 EraseRoundRect toolbox
  in  r:Rect  ovalW:sword  ovalH:sword

A8B3 InverRoundRect toolbox
  in  r:Rect  ovalW:sword  ovalH:sword

A8B4 FillRoundRect toolbox
  in  r:Rect  ovalW:sword  ovalH:sword  pat:Ptr

A8B6 StdOval toolbox
  in  verb:byte  r:Rect

A8B7 FrameOval toolbox
  in  r:Rect

A8B8 PaintOval toolbox
  in  r:Rect

A8B9 EraseOval toolbox
  in  r:Rect

A8BA InvertOval toolbox
  in  r:Rect

A8BB FillOval toolbox
  in  r:Rect  pat:Ptr

A8BD StdArc toolbox
  in  verb:byte  r:Rect  startAngle:sword  arcAngle:sword

A8BF PaintArc toolbox
  in  r:Rect  startAngle:sword  arcAngle:sword

A8C0 EraseArc toolbox
  in  r:Rect  startAngle:sword  arcAngle:sword

A8C1 InvertArc toolbox
  in  r:Rect  startAngle:sword  arcAngle:sword

A8C2 FillArc toolbox
  in  r:Rect  startAngle:sword  arcAngle:sword  pat:Ptr

A8C3 PtToAngle toolbox
  in  r:Rect  pt:Point
  out angle:sword

A8C5 StdPoly toolbox
  in  verb:byte  poly:Handle

A8C6 FramePoly toolbox
  in  poly:Handle

A8C7 PaintPoly toolbox
  in  poly:Handle

A8C8 ErasePoly toolbox
  in  poly:Handle

A8C9 InvertPoly toolbox
  in  poly:Handle

A8CA FillPoly toolbox
  in  poly:Handle  pat:Ptr

A8CB OpenPoly toolbox
  out poly:Handle

A8CC ClosePgon toolbox

A8CD KillPoly toolbox
  in  poly:Handle

A8CE OffsetPoly toolbox
  in  poly:Handle  dh:sword  dv:sword
```

### 4.5 — Region primitives (~20 traps)

```
A8D1 StdRgn toolbox
  in  verb:byte  rgn:Handle

A8D2 FrameRgn toolbox
  in  rgn:Handle

A8D3 PaintRgn toolbox
  in  rgn:Handle

A8D4 EraseRgn toolbox
  in  rgn:Handle

A8D5 InverRgn toolbox
  in  rgn:Handle

A8D6 FillRgn toolbox
  in  rgn:Handle  pat:Ptr

A8D8 NewRgn toolbox
  out rgn:Handle

A8D9 DisposRgn toolbox
  in  rgn:Handle

A8DA OpenRgn toolbox

A8DB CloseRgn toolbox
  in  rgn:Handle

A8DC CopyRgn toolbox
  in  src:Handle  dst:Handle

A8DD SetEmptyRgn toolbox
  in  rgn:Handle

A8DE SetRecRgn toolbox
  in  rgn:Handle  left:sword  top:sword  right:sword  bottom:sword

A8DF RectRgn toolbox
  in  rgn:Handle  r:Rect

A8E0 OfsetRgn toolbox
  in  rgn:Handle  dh:sword  dv:sword

A8E1 InsetRgn toolbox
  in  rgn:Handle  dh:sword  dv:sword

A8E2 EmptyRgn toolbox
  in  rgn:Handle
  out empty:Boolean

A8E3 EqualRgn toolbox
  in  rgn1:Handle  rgn2:Handle
  out equal:Boolean

A8E4 SectRgn toolbox
  in  src1:Handle  src2:Handle  dst:Handle

A8E5 UnionRgn toolbox
  in  src1:Handle  src2:Handle  dst:Handle

A8E6 DiffRgn toolbox
  in  src1:Handle  src2:Handle  dst:Handle

A8E7 XorRgn toolbox
  in  src1:Handle  src2:Handle  dst:Handle

A8E8 PtInRgn toolbox
  in  pt:Point  rgn:Handle
  out inside:Boolean

A8E9 RectInRgn toolbox
  in  r:Rect  rgn:Handle
  out inside:Boolean
```

### 4.6 — Bit / Picture / Misc QD primitives (~20 traps)

```
A8EA SetStdProcs toolbox
  in  procs:Ptr

A8EB StdBits toolbox
  in  srcBits:Ptr  srcRect:Rect  dstRect:Rect  mode:sword  maskRgn:Handle

A8EC CopyBits toolbox
  in  srcBits:Ptr  dstBits:Ptr  srcRect:Rect  dstRect:Rect  mode:sword  maskRgn:Handle

A817 CopyMask toolbox
  in  srcBits:Ptr  maskBits:Ptr  dstBits:Ptr  srcRect:Rect  maskRect:Rect  dstRect:Rect

A8EF ScrollRect toolbox
  in  r:Rect  dh:sword  dv:sword  updateRgn:Handle

A8F3 OpenPicture toolbox
  in  picFrame:Rect
  out pic:Handle

A8F4 ClosePicture toolbox

A8F5 KillPicture toolbox
  in  pic:Handle

A8F6 DrawPicture toolbox
  in  pic:Handle  dst:Rect

A8F2 PicComment toolbox
  in  kind:sword  dataSize:sword  data:Handle

A8F8 ScalePt toolbox
  in  pt:Point  src:Rect  dst:Rect

A8F9 MapPt toolbox
  in  pt:Point  src:Rect  dst:Rect

A8FA MapRect toolbox
  in  r:Rect  src:Rect  dst:Rect

A8FB MapRgn toolbox
  in  rgn:Handle  src:Rect  dst:Rect

A8FC MapPoly toolbox
  in  poly:Handle  src:Rect  dst:Rect

A8CF PackBits toolbox
  in  srcPtr:Ptr  dstPtr:Ptr  srcBytes:sword

A8D0 UnpackBits toolbox
  in  srcPtr:Ptr  dstPtr:Ptr  dstBytes:sword

A87E AddPt toolbox
  in  src:Point  dst:Point

A87F SubPt toolbox
  in  src:Point  dst:Point

A880 SetPt toolbox
  in  pt:Point  h:sword  v:sword

A881 EqualPt toolbox
  in  pt1:Point  pt2:Point
  out equal:Boolean

A870 LocalToGlobal toolbox
  in  pt:Point

A871 GlobalToLocal toolbox
  in  pt:Point

A865 GetPixel toolbox
  in  h:sword  v:sword
  out pixel:Boolean

A862 ForeColor toolbox
  in  color:slong

A863 BackColor toolbox
  in  color:slong

A864 ColorBit toolbox
  in  whichBit:sword

A866 StuffHex toolbox
  in  dst:Ptr  s:Str255

A8BC SlopeFromAngle toolbox
  in  angle:sword
  out slope:long

A8C4 AngleFromSlope toolbox
  in  slope:long
  out angle:sword
```

### 4.7 — Font / Misc (InitFonts, etc.) (~5 traps)

```
A8FE InitFonts toolbox

A901 FMSwapFont toolbox
  in  inRec:Ptr
  out outRec:Ptr

A902 RealFont toolbox
  in  fontNum:sword  size:sword
  out real:Boolean

A903 SetFontLock toolbox
  in  lock:Boolean
```

### Fence

- [ ] All ~130 QuickDraw stubs now have `in`/`out` lines
- [ ] `./bld/macos/tests` passes
- [ ] Commit: `"traps: phase 4 — QuickDraw trap parameters"`

---

## Phase 5 — Fill Toolbox Stubs: Window Manager & Menu Manager

### 5.1 — Window Manager (~30 traps)

```
A912 InitWindows toolbox

A913 NewWindow toolbox
  in  storage:Ptr  boundsRect:Rect  title:Str255  visible:Boolean  procID:sword  behind:Ptr  goAway:Boolean  refCon:slong
  out window:Ptr

A9BD GetNewWindow toolbox
  in  templateID:sword  storage:Ptr  behind:Ptr
  out window:Ptr

A92D CloseWindow toolbox
  in  window:Ptr

A914 DisposWindow toolbox
  in  window:Ptr

A915 ShowWindow toolbox
  in  window:Ptr

A916 HideWindow toolbox
  in  window:Ptr

A91C HiliteWindow toolbox
  in  window:Ptr  hilite:Boolean

A91B MoveWindow toolbox
  in  window:Ptr  h:sword  v:sword  front:Boolean

A91D SizeWindow toolbox
  in  window:Ptr  w:sword  h:sword  update:Boolean

A91F SelectWindow toolbox
  in  window:Ptr

A920 BringToFront toolbox
  in  window:Ptr

A908 ShowHide toolbox
  in  window:Ptr  showFlag:Boolean

A924 FrontWindow toolbox
  out window:Ptr

A925 DragWindow toolbox
  in  window:Ptr  startPt:Point  bounds:Rect

A926 DragTheRgn toolbox
  in  rgn:Handle  startPt:Point  bounds:Rect  slop:Rect  axis:sword  proc:ProcPtr
  out result:long

A92B GrowWindow toolbox
  in  window:Ptr  startPt:Point  sizeRect:Rect
  out newSize:long

A92C FindWindow toolbox
  in  pt:Point
  out partCode:sword  window:Ptr

A91E TrackGoAway toolbox
  in  window:Ptr  pt:Point
  out goAway:Boolean

A83B TrackBox toolbox
  in  window:Ptr  pt:Point  partCode:sword
  out inside:Boolean

A83A ZoomWindow toolbox
  in  window:Ptr  partCode:sword  front:Boolean

A922 BeginUpdate toolbox
  in  window:Ptr

A923 EndUpdate toolbox
  in  window:Ptr

A927 InvalRgn toolbox
  in  rgn:Handle

A928 InvalRect toolbox
  in  r:Rect

A929 ValidRgn toolbox
  in  rgn:Handle

A92A ValidRect toolbox
  in  r:Rect

A917 GetWRefCon toolbox
  in  window:Ptr
  out refCon:slong

A918 SetWRefCon toolbox
  in  window:Ptr  refCon:slong

A919 GetWTitle toolbox
  in  window:Ptr  title:Str255

A91A SetWTitle toolbox
  in  window:Ptr  title:Str255

A92E SetWindowPic toolbox
  in  window:Ptr  pic:Handle

A92F GetWindowPic toolbox
  in  window:Ptr
  out pic:Handle

A904 DrawGrowIcon toolbox
  in  window:Ptr

A910 GetWMgrPort toolbox
  out port:Ptr

A911 CheckUpdate toolbox
  out event:Ptr  result:Boolean

A90A CalcVBehind toolbox
  in  window:Ptr  clobbered:Handle

A90B ClipAbove toolbox
  in  window:Ptr

A90C PaintOne toolbox
  in  window:Ptr  clobbered:Handle

A90D PaintBehind toolbox
  in  startWindow:Ptr  clobbered:Handle

A90E SaveOld toolbox
  in  window:Ptr

A90F DrawNew toolbox
  in  window:Ptr  update:Boolean

A905 DragGrayRgn toolbox
  in  rgn:Handle  startPt:Point  bounds:Rect  slop:Rect  axis:sword  proc:ProcPtr
  out result:long
```

### 5.2 — Menu Manager (~30 traps)

```
A933 AppendMenu toolbox
  in  menu:Handle  data:Str255

A94D AddResMenu toolbox
  in  menu:Handle  resType:OSType

A934 ClearMenuBar toolbox

A936 DeleteMenu toolbox
  in  menuID:sword

A937 DrawMenuBar toolbox

A938 HiliteMenu toolbox
  in  menuID:sword

A939 EnableItem toolbox
  in  menu:Handle  item:sword

A93A DisableItem toolbox
  in  menu:Handle  item:sword

A93B GetMenuBar toolbox
  out menuBar:Handle

A93C SetMenuBar toolbox
  in  menuBar:Handle

A93D MenuSelect toolbox
  in  startPt:Point
  out result:long

A93E MenuKey toolbox
  in  ch:sword
  out result:long

A93F GetItmIcon toolbox
  in  menu:Handle  item:sword
  out icon:byte

A940 SetItmIcon toolbox
  in  menu:Handle  item:sword  icon:byte

A941 GetItmStyle toolbox
  in  menu:Handle  item:sword
  out style:sword

A942 SetItmStyle toolbox
  in  menu:Handle  item:sword  style:sword

A943 GetItmMark toolbox
  in  menu:Handle  item:sword
  out mark:sword

A944 SetItmMark toolbox
  in  menu:Handle  item:sword  mark:sword

A945 CheckItem toolbox
  in  menu:Handle  item:sword  checked:Boolean

A946 GetItem toolbox
  in  menu:Handle  item:sword  name:Str255

A947 SetItem toolbox
  in  menu:Handle  item:sword  name:Str255

A948 CalcMenuSize toolbox
  in  menu:Handle

A949 GetMHandle toolbox
  in  menuID:sword
  out menu:Handle

A94A SetMFlash toolbox
  in  count:sword

A94B PlotIcon toolbox
  in  r:Rect  icon:Handle

A94C FlashMenuBar toolbox
  in  menuID:sword

A84E GetItemCmd toolbox
  in  menu:Handle  item:sword
  out cmd:sword

A84F SetItemCmd toolbox
  in  menu:Handle  item:sword  cmd:sword

A80B PopUpMenuSelect toolbox
  in  menu:Handle  top:sword  left:sword  popUpItem:sword
  out result:long

A809 GetCVariant toolbox
  in  window:Ptr
  out variant:sword

A9BF GetRMenu toolbox
  in  menuID:sword
  out menu:Handle

A9C0 GetNewMBar toolbox
  in  menuBarID:sword
  out menuBar:Handle
```

### Fence

- [ ] All Window Manager and Menu Manager stubs typed
- [ ] `./bld/macos/tests` passes
- [ ] Commit: `"traps: phase 5 — Window & Menu trap parameters"`

---

## Phase 6 — Fill Toolbox Stubs: Control, Dialog, TextEdit

### 6.1 — Control Manager (~26 traps)

```
A954 NewControl toolbox
  in  window:Ptr  bounds:Rect  title:Str255  visible:Boolean  value:sword  min:sword  max:sword  procID:sword  refCon:slong
  out ctrl:Ptr

A9BE GetNewControl toolbox
  in  ctrlID:sword  window:Ptr
  out ctrl:Ptr

A955 DisposControl toolbox
  in  ctrl:Ptr

A956 KillControls toolbox
  in  window:Ptr

A957 ShowControl toolbox
  in  ctrl:Ptr

A958 HideControl toolbox
  in  ctrl:Ptr

A959 MoveControl toolbox
  in  ctrl:Ptr  h:sword  v:sword

A95C SizeControl toolbox
  in  ctrl:Ptr  w:sword  h:sword

A95D HiliteControl toolbox
  in  ctrl:Ptr  hilite:sword

A95E GetCTitle toolbox
  in  ctrl:Ptr  title:Str255

A95F SetCTitle toolbox
  in  ctrl:Ptr  title:Str255

A960 GetCtlValue toolbox
  in  ctrl:Ptr
  out value:sword

A961 GetMinCtl toolbox
  in  ctrl:Ptr
  out min:sword

A962 GetMaxCtl toolbox
  in  ctrl:Ptr
  out max:sword

A963 SetCtlValue toolbox
  in  ctrl:Ptr  value:sword

A964 SetMinCtl toolbox
  in  ctrl:Ptr  min:sword

A965 SetMaxCtl toolbox
  in  ctrl:Ptr  max:sword

A95A GetCRefCon toolbox
  in  ctrl:Ptr
  out refCon:slong

A95B SetCRefCon toolbox
  in  ctrl:Ptr  refCon:slong

A96A GetCtlAction toolbox
  in  ctrl:Ptr
  out proc:ProcPtr

A96B SetCtlAction toolbox
  in  ctrl:Ptr  proc:ProcPtr

A966 TestControl toolbox
  in  ctrl:Ptr  pt:Point
  out partCode:sword

A968 TrackControl toolbox
  in  ctrl:Ptr  pt:Point  proc:ProcPtr
  out partCode:sword

A96C FindControl toolbox
  in  pt:Point  window:Ptr
  out partCode:sword  ctrl:Ptr

A969 DrawControls toolbox
  in  window:Ptr

A96D Draw1Control toolbox
  in  ctrl:Ptr

A953 UpdtControl toolbox
  in  window:Ptr  updateRgn:Handle

A967 DragControl toolbox
  in  ctrl:Ptr  startPt:Point  bounds:Rect  slop:Rect  axis:sword
```

### 6.2 — Dialog Manager (~24 traps)

```
A97B InitDialogs toolbox
  in  resumeProc:ProcPtr

A97D NewDialog toolbox
  in  storage:Ptr  bounds:Rect  title:Str255  visible:Boolean  procID:sword  behind:Ptr  goAway:Boolean  refCon:slong  items:Handle
  out dialog:Ptr

A97C GetNewDialog toolbox
  in  dialogID:sword  storage:Ptr  behind:Ptr
  out dialog:Ptr

A982 CloseDialog toolbox
  in  dialog:Ptr

A983 DisposDialog toolbox
  in  dialog:Ptr

A981 DrawDialog toolbox
  in  dialog:Ptr

A991 ModalDialog toolbox
  in  filterProc:ProcPtr
  out itemHit:sword

A980 DialogSelect toolbox
  in  event:Ptr
  out dialog:Ptr  itemHit:sword  result:Boolean

A97F IsDialogEvent toolbox
  in  event:Ptr
  out result:Boolean

A985 Alert toolbox
  in  alertID:sword  filterProc:ProcPtr
  out itemHit:sword

A986 StopAlert toolbox
  in  alertID:sword  filterProc:ProcPtr
  out itemHit:sword

A987 NoteAlert toolbox
  in  alertID:sword  filterProc:ProcPtr
  out itemHit:sword

A988 CautionAlert toolbox
  in  alertID:sword  filterProc:ProcPtr
  out itemHit:sword

A989 CouldAlert toolbox
  in  alertID:sword

A98A FreeAlert toolbox
  in  alertID:sword

A979 CouldDialog toolbox
  in  dialogID:sword

A98B ParamText toolbox
  in  param0:Str255  param1:Str255  param2:Str255  param3:Str255

A98C ErrorSound toolbox
  in  proc:ProcPtr

A98D GetDItem toolbox
  in  dialog:Ptr  item:sword
  out itemType:sword  itemHandle:Handle  itemBox:Rect

A98E SetDItem toolbox
  in  dialog:Ptr  item:sword  itemType:sword  itemHandle:Handle  itemBox:Rect

A98F SetIText toolbox
  in  item:Handle  text:Str255

A990 GetIText toolbox
  in  item:Handle  text:Str255

A97E SelIText toolbox
  in  dialog:Ptr  item:sword  selStart:sword  selEnd:sword

A978 UpdtDialog toolbox
  in  dialog:Ptr  updateRgn:Handle

A984 FindDItem toolbox
  in  dialog:Ptr  pt:Point
  out item:sword
```

### 6.3 — TextEdit (~27 traps)

```
A9CC TEInit toolbox

A9D2 TENew toolbox
  in  destRect:Rect  viewRect:Rect
  out te:Handle

A9CD TEDispose toolbox
  in  te:Handle

A9D1 TESetSelect toolbox
  in  selStart:sword  selEnd:sword  te:Handle

A9D4 TEClick toolbox
  in  pt:Point  extend:Boolean  te:Handle

A9D0 TECalText toolbox
  in  te:Handle

A9D3 TEUpdate toolbox
  in  r:Rect  te:Handle

A9D8 TEActivate toolbox
  in  te:Handle

A9D9 TEDeactivate toolbox
  in  te:Handle

A9DA TEIdle toolbox
  in  te:Handle

A9DC TEKey toolbox
  in  ch:sword  te:Handle

A9D5 TECopy toolbox
  in  te:Handle

A9D6 TECut toolbox
  in  te:Handle

A9DB TEPaste toolbox
  in  te:Handle

A9D7 TEDelete toolbox
  in  te:Handle

A9DE TEInsert toolbox
  in  text:Ptr  length:long  te:Handle

A9CF TESetText toolbox
  in  text:Ptr  length:long  te:Handle

A9CB TEGetText toolbox
  in  te:Handle
  out text:Handle

A9DF TESetJust toolbox
  in  just:sword  te:Handle

A9DD TEScroll toolbox
  in  dh:sword  dv:sword  te:Handle

A9CE TextBox toolbox
  in  text:Ptr  length:long  box:Rect  just:sword

A812 TEPinScroll toolbox
  in  dh:sword  dv:sword  te:Handle

A813 TEAutoView toolbox
  in  auto:Boolean  te:Handle

A811 TESelView toolbox
  in  te:Handle

A83C TEGetOffset toolbox
  in  pt:Point  te:Handle
  out offset:sword

A83D TEDispatch toolbox
  in  selector:sword

A83E TEStyleNew toolbox
  in  destRect:Rect  viewRect:Rect
  out te:Handle
```

### Fence

- [ ] All Control, Dialog, TextEdit stubs typed
- [ ] `./bld/macos/tests` passes
- [ ] Commit: `"traps: phase 6 — Control, Dialog, TextEdit parameters"`

---

## Phase 7 — Fill Toolbox Stubs: Event, Scrap, Sound, Misc

### 7.1 — Event Manager (~6 traps)

```
A970 GetNextEvent toolbox
  in  mask:word  event:Ptr
  out found:Boolean

A971 EventAvail toolbox
  in  mask:word  event:Ptr
  out found:Boolean

A973 StillDown toolbox
  out down:Boolean

A974 Button toolbox
  out down:Boolean

A975 TickCount toolbox
  out ticks:long

A976 GetKeys toolbox
  in  keys:Ptr

A977 WaitMouseUp toolbox
  out released:Boolean
```

### 7.2 — Scrap Manager (~5 traps)

```
A9F9 InfoScrap toolbox
  out scrap:Ptr

A9FC ZeroScrap toolbox
  out err:long

A9FE PutScrap toolbox
  in  length:long  type:OSType  src:Ptr
  out err:long

A9FD GetScrap toolbox
  in  dst:Handle  type:OSType
  out length:long

A9FB LodeScrap toolbox
  out err:long

A9FA UnlodeScrap toolbox
  out err:long
```

### 7.3 — Sound Manager (~7 traps)

```
A807 SndNewChannel toolbox
  in  chan:Ptr  synth:sword  init:long  proc:ProcPtr
  out err:OSErr

A801 SndDisposeChannel toolbox
  in  chan:Ptr  quiet:Boolean
  out err:OSErr

A803 SndDoCommand toolbox
  in  chan:Ptr  cmd:Ptr  noWait:Boolean
  out err:OSErr

A804 SndDoImmediate toolbox
  in  chan:Ptr  cmd:Ptr
  out err:OSErr

A805 SndPlay toolbox
  in  chan:Ptr  snd:Handle  async:Boolean
  out err:OSErr

A806 SndControl toolbox
  in  id:sword  cmd:Ptr
  out err:OSErr

A802 SndAddModifier toolbox
  in  chan:Ptr  modifier:ProcPtr  id:sword  init:long
  out err:OSErr
```

### 7.4 — Desk Manager / System (~10 traps)

```
A9B7 CloseDeskAcc toolbox
  in  refNum:sword

A9B6 OpenDeskAcc toolbox
  in  name:Str255
  out refNum:sword

A9B2 SystemEvent toolbox
  in  event:Ptr
  out handled:Boolean

A9B3 SystemClick toolbox
  in  event:Ptr  window:Ptr

A9B4 SystemTask toolbox

A9B5 SystemMenu toolbox
  in  menuResult:long

A9C2 SysEdit toolbox
  in  editCmd:sword
  out result:Boolean

A9C8 SysBeep toolbox
  in  duration:sword
```

### 7.5 — Resource Manager extras (~7 traps)

```
A9AA ChangedResource toolbox
  in  rsrc:Handle

A9AD RmveResource toolbox
  in  rsrc:Handle

A9AF ResError toolbox
  out err:OSErr

A9B0 WriteResource toolbox
  in  rsrc:Handle

A80D Count1Resources toolbox
  in  resType:OSType
  out count:sword

A81C Count1Types toolbox
  out count:sword

A80E Get1IxResource toolbox
  in  resType:OSType  index:sword
  out rsrc:Handle

A80F Get1IxType toolbox
  in  index:sword  resType:OSType

A820 Get1NamedResource toolbox
  in  resType:OSType  name:Str255
  out rsrc:Handle

A821 MaxSizeRsrc toolbox
  in  rsrc:Handle
  out size:long

A80C RGetResource toolbox
  in  resType:OSType  resID:sword
  out rsrc:Handle

A9C5 RsrcMapEntry toolbox
  in  rsrc:Handle
  out offset:long

A810 Unique1ID toolbox
  in  resType:OSType
  out id:sword

A9C1 UniqueID toolbox
  in  resType:OSType
  out id:sword
```

### 7.6 — Utility / Pack / Misc (~30 traps)

```
A9E0 Munger toolbox
  in  h:Handle  offset:long  ptr1:Ptr  len1:long  ptr2:Ptr  len2:long
  out result:long

A9E1 HandToHand toolbox
  in  h:Handle
  out err:OSErr

A9E2 PtrToXHand toolbox
  in  src:Ptr  dst:Handle  size:long
  out err:OSErr

A9E3 PtrToHand toolbox
  in  src:Ptr  size:long
  out h:Handle  err:OSErr

A9E4 HandAndHand toolbox
  in  src:Handle  dst:Handle
  out err:OSErr

A9EF PtrAndHand toolbox
  in  src:Ptr  dst:Handle  size:long
  out err:OSErr

A9F0 LoadSeg toolbox
  in  segNum:sword

A9F1 UnloadSeg toolbox
  in  routine:Ptr

A9F3 Chain toolbox
  in  fileName:Str255

A9F5 GetAppParms toolbox
  in  name:Str255  refNum:sword  appHandle:Handle

A9C3 KeyTrans toolbox
  in  transData:Ptr  keycode:sword
  out result:long

A9C6 Secs2Date toolbox
  in  secs:long  d:Ptr

A9C7 Date2Secs toolbox
  in  d:Ptr
  out secs:long

A895 Shutdown toolbox

A9B8 GetPattern toolbox
  in  patID:sword
  out pat:Handle

A9B9 GetCursor toolbox
  in  cursorID:sword
  out cursor:Handle

A9BA GetString toolbox
  in  stringID:sword
  out s:Handle

A9BB GetIcon toolbox
  in  iconID:sword
  out icon:Handle

A9BC GetPicture toolbox
  in  picID:sword
  out pic:Handle

A906 NewString toolbox
  in  s:Str255
  out h:Handle

A907 SetString toolbox
  in  h:Handle  s:Str255

A868 FixMul toolbox
  in  a:long  b:long
  out result:long

A869 FixRatio toolbox
  in  numer:sword  denom:sword
  out result:long

A86A HiWord toolbox
  in  x:long
  out hi:sword

A86B LoWord toolbox
  in  x:long
  out lo:sword

A86C FixRound toolbox
  in  x:long
  out result:sword

A84D FixDiv toolbox
  in  a:long  b:long
  out result:long

A840 Fix2Long toolbox
  in  x:long
  out result:long

A841 Fix2Frac toolbox
  in  x:long
  out result:long

A842 Frac2Fix toolbox
  in  x:long
  out result:long

A843 Fix2X toolbox
  in  x:long

A844 X2Fix toolbox
  out result:long

A845 Frac2X toolbox
  in  x:long

A846 X2Frac toolbox
  out result:long

A847 FracCos toolbox
  in  x:long
  out result:long

A848 FracSin toolbox
  in  x:long
  out result:long

A849 FracSqrt toolbox
  in  x:long
  out result:long

A84A FracMul toolbox
  in  a:long  b:long
  out result:long

A84B FracDiv toolbox
  in  a:long  b:long
  out result:long

A818 FixAtan2 toolbox
  in  x:long  y:long
  out result:long

A835 FontMetrics toolbox
  in  info:^FontInfo

A837 MeasureText toolbox
  in  count:sword  text:Ptr  charLocs:Ptr

A838 CalcMask toolbox
  in  src:Ptr  dst:Ptr  srcRow:sword  dstRow:sword  height:sword  words:sword

A839 SeedFill toolbox
  in  src:Ptr  dst:Ptr  srcRow:sword  dstRow:sword  height:sword  words:sword  seedH:sword  seedV:sword

A8B5 ScriptUtil toolbox
  in  selector:long

A9E5 InitPack toolbox
  in  packID:sword

A9E6 InitAllPacks toolbox

A9E7 Pack0 toolbox
A82E Pack12 toolbox
A9E9 Pack2 toolbox
A9EA Pack3 toolbox
A9ED Pack6 toolbox
A9EE Pack7 toolbox
A9EB FP68K toolbox
A9EC Elems68K toolbox

A80A GetWVariant toolbox
  in  window:Ptr
  out variant:sword

A815 SCSIDispatch toolbox
  in  selector:sword
```

### Fence

- [ ] All Event, Scrap, Sound, Misc stubs typed
- [ ] `./bld/macos/tests` passes
- [ ] Commit: `"traps: phase 7 — Event, Scrap, Sound, Misc parameters"`

---

## Phase 8 — Fill Toolbox Stubs: Color Toolbox & Palette Manager

These are System 6 Color QuickDraw traps (AA00–AA9F range).
Many are simple object creation/disposal or attribute get/set.

### 8.1 — Color QuickDraw core (~30 traps)

```
AA00 OpenCport toolbox
  in  port:Ptr

AA01 InitCport toolbox
  in  port:Ptr

AA03 NewPixMap toolbox
  out pixMap:Handle

AA04 DisposPixMap toolbox
  in  pixMap:Handle

AA05 CopyPixMap toolbox
  in  src:Handle  dst:Handle

AA06 SetCPortPix toolbox
  in  pixMap:Handle

AA07 NewPixPat toolbox
  out pixPat:Handle

AA08 DisposPixPat toolbox
  in  pixPat:Handle

AA09 CopyPixPat toolbox
  in  src:Handle  dst:Handle

AA0A PenPixPat toolbox
  in  pixPat:Handle

AA0B BackPixPat toolbox
  in  pixPat:Handle

AA0C GetPixPat toolbox
  in  patID:sword
  out pixPat:Handle

AA0D MakeRGBPat toolbox
  in  pixPat:Handle  color:Ptr

AA0E FillCRect toolbox
  in  r:Rect  pixPat:Handle

AA0F FillCOval toolbox
  in  r:Rect  pixPat:Handle

AA10 FillCRoundRect toolbox
  in  r:Rect  ovalW:sword  ovalH:sword  pixPat:Handle

AA11 FillCArc toolbox
  in  r:Rect  startAngle:sword  arcAngle:sword  pixPat:Handle

AA12 FillCRgn toolbox
  in  rgn:Handle  pixPat:Handle

AA13 FillCPoly toolbox
  in  poly:Handle  pixPat:Handle

AA14 RGBForeColor toolbox
  in  color:Ptr

AA15 RGBBackColor toolbox
  in  color:Ptr

AA16 SetCPixel toolbox
  in  h:sword  v:sword  color:Ptr

AA17 GetCPixel toolbox
  in  h:sword  v:sword  color:Ptr

AA19 GetForeColor toolbox
  in  color:Ptr

AA1A GetBackColor toolbox
  in  color:Ptr

AA21 OpColor toolbox
  in  color:Ptr

AA22 HiliteColor toolbox
  in  color:Ptr

AA23 CharExtra toolbox
  in  extra:long

AA40 QDError toolbox
  out err:sword

AA4E SetStdCProcs toolbox
  in  procs:Ptr

AA4F CalcCMask toolbox
  in  src:Ptr  dst:Ptr  srcRow:sword  dstRow:sword  height:sword  words:sword  color:Ptr  proc:ProcPtr  flags:long

AA50 SeedCFill toolbox
  in  src:Ptr  dst:Ptr  srcRow:sword  dstRow:sword  height:sword  words:sword  seedH:sword  seedV:sword  proc:ProcPtr  flags:long
```

### 8.2 — Color Table / Cursor / Icon (~10 traps)

```
AA18 GetCTable toolbox
  in  ctID:sword
  out ct:Handle

AA24 DisposCTable toolbox
  in  ct:Handle

AA1B GetCCursor toolbox
  in  cursorID:sword
  out cursor:Handle

AA26 DisposCCursor toolbox
  in  cursor:Handle

AA1C SetCCursor toolbox
  in  cursor:Handle

AA1D AllocCursor toolbox

AA1E GetCIcon toolbox
  in  iconID:sword
  out icon:Handle

AA25 DisposCIcon toolbox
  in  icon:Handle

AA1F PlotCIcon toolbox
  in  r:Rect  icon:Handle
```

### 8.3 — GDevice (~10 traps)

```
AA28 GetCTSeed toolbox
  out seed:long

AA29 GetDeviceList toolbox
  out device:Handle

AA2A GetMainDevice toolbox
  out device:Handle

AA2B GetNextDevice toolbox
  in  device:Handle
  out next:Handle

AA27 GetMaxDevice toolbox
  in  r:Rect
  out device:Handle

AA2C TestDeviceAttribute toolbox
  in  device:Handle  attr:sword
  out result:Boolean

AA2D SetDeviceAttribute toolbox
  in  device:Handle  attr:sword  value:Boolean

AA2E InitGDevice toolbox
  in  qdRefNum:sword  mode:long  device:Handle

AA30 DisposGDevice toolbox
  in  device:Handle

AA31 SetGDevice toolbox
  in  device:Handle

AA32 GetGDevice toolbox
  out device:Handle
```

### 8.4 — Palette Manager (~15 traps)

```
AA90 InitPalettes toolbox

AA91 NewPalette toolbox
  in  entries:sword  ctab:Handle  srcUsage:sword  tolerance:sword
  out palette:Handle

AA92 GetNewPalette toolbox
  in  paletteID:sword
  out palette:Handle

AA94 ActivatePalette toolbox
  in  window:Ptr

AA95 SetPalette toolbox
  in  window:Ptr  palette:Handle  update:Boolean

AA96 GetPalette toolbox
  in  window:Ptr
  out palette:Handle

AA97 PmForeColor toolbox
  in  entry:sword

AA98 PmBackColor toolbox
  in  entry:sword

AA99 AnimateEntry toolbox
  in  window:Ptr  entry:sword  color:Ptr

AA9A AnimatePalette toolbox
  in  window:Ptr  ctab:Handle  srcIndex:sword  dstEntry:sword  dstLength:sword

AA9B GetEntryColor toolbox
  in  palette:Handle  entry:sword  color:Ptr

AA9C SetEntryColor toolbox
  in  palette:Handle  entry:sword  color:Ptr

AA9D GetEntryUsage toolbox
  in  palette:Handle  entry:sword
  out usage:sword

AA9E SetEntryUsage toolbox
  in  palette:Handle  entry:sword  usage:sword  tolerance:sword

AA9F CTab2Palette toolbox
  in  ctab:Handle  palette:Handle  srcUsage:sword  tolerance:sword

AAA0 Palette2CTab toolbox
  in  palette:Handle  ctab:Handle
```

### 8.5 — Color Window / Dialog / Menu (~15 traps)

```
AA45 NewCWindow toolbox
  in  storage:Ptr  boundsRect:Rect  title:Str255  visible:Boolean  procID:sword  behind:Ptr  goAway:Boolean  refCon:slong
  out window:Ptr

AA46 GetNewCWindow toolbox
  in  windowID:sword  storage:Ptr  behind:Ptr
  out window:Ptr

AA4B NewCDialog toolbox
  in  storage:Ptr  boundsRect:Rect  title:Str255  visible:Boolean  procID:sword  behind:Ptr  goAway:Boolean  refCon:slong  items:Handle
  out dialog:Ptr

AA48 GetCWMgrPort toolbox
  out port:Ptr

AA41 SetWinColor toolbox
  in  window:Ptr  table:Handle

AA42 GetAuxWin toolbox
  in  window:Ptr
  out table:Handle  result:Boolean

AA43 SetCtlColor toolbox
  in  ctrl:Ptr  table:Handle

AA44 GetAuxCtl toolbox
  in  ctrl:Ptr
  out table:Handle  result:Boolean

AA47 SetDeskCPat toolbox
  in  pixPat:Handle

AA60 DelMCEntries toolbox
  in  menuID:sword  item:sword

AA61 GetMCInfo toolbox
  out table:Handle

AA62 SetMCInfo toolbox
  in  table:Handle

AA63 DispMCInfo toolbox
  in  table:Handle

AA64 GetMCEntry toolbox
  in  menuID:sword  item:sword
  out entry:Ptr

AA65 SetMCEntries toolbox
  in  count:sword  entries:Ptr

AA66 MenuChoice toolbox
  out result:long

AA33 Color2Index toolbox
  in  color:Ptr
  out index:long

AA34 Index2Color toolbox
  in  index:long  color:Ptr

AA35 InvertColor toolbox
  in  color:Ptr

AA36 RealColor toolbox
  in  color:Ptr
  out result:Boolean

AA37 GetSubTable toolbox
  in  ctab:Handle  index:sword  target:Handle

AA39 MakeITable toolbox
  in  ctab:Handle  itab:Handle  res:sword

AA3A AddSearch toolbox
  in  proc:ProcPtr

AA3B AddComp toolbox
  in  proc:ProcPtr

AA3C SetClientID toolbox
  in  id:sword

AA3D ProtectEntry toolbox
  in  index:sword  protect:Boolean

AA3E ReserveEntry toolbox
  in  index:sword  reserve:Boolean

AA3F SetEntries toolbox
  in  start:sword  count:sword  table:Ptr

AA49 SaveEntries toolbox
  in  ctab:Handle  selection:Handle
  out result:Handle

AA4A RestoreEntries toolbox
  in  saved:Handle  ctab:Handle  selection:Handle

AA4C DelSearch toolbox
  in  proc:ProcPtr

AA4D DelComp toolbox
  in  proc:ProcPtr
```

### Fence

- [ ] All Color Toolbox and Palette stubs typed
- [ ] `./bld/macos/tests` passes
- [ ] Commit: `"traps: phase 8 — Color Toolbox & Palette parameters"`

---

## Phase 9 — Add Missing Traps from TRAP_TABLE.md

Using the audit from Phase 1, add trap entries that appear in
`macdocs/ref/TRAP_TABLE.md` but are absent from `traps.def`.

### 9.1 — Identify missing traps

Consult `assets/AUDIT.md` for the "Missing from traps.def" list.
Known candidates (from preliminary survey — TRAP_TABLE.md has ~500
entries, traps.def has 435):

- **BitAnd** (A858), **BitOr** (A85B), **BitXor** (A859),
  **BitNot** (A85A), **BitShift** (A85C), **BitTst** (A85D),
  **BitSet** (A85E), **BitClr** (A85F) — Toolbox utility traps
- **CalcVis** (A909) — Window Manager
- **ClosePort** (A87D) — QuickDraw
- **FrameArc** (A8BE) — QuickDraw (missing from traps.def)
- Any others identified in Phase 1

### 9.2 — Add each missing trap

For each missing trap, add a full entry with convention and
`in`/`out` parameters.  Place in the correct section of traps.def
(sorted by trap word within its section).

### 9.3 — Verify count

After adding, the total trap count in traps.def should match or
exceed the count in TRAP_TABLE.md (minus any System 7+ traps held
back for Phase 10).

### Fence

- [ ] All traps from TRAP_TABLE.md accounted for
- [ ] `./bld/macos/tests` passes
- [ ] Commit: `"traps: phase 9 — add missing traps"`

---

## Phase 10 — Comment Out Post-System 6 Traps

### 10.1 — Identify System 7+ traps

Traps present in traps.def but NOT in TRAP_TABLE.md (which is
System 6 era) and NOT in THINK C 3 headers are candidates.

Use the audit from Phase 1.  Common System 7 additions:
- Traps added by Script Manager extensions
- Traps added by MultiFinder/Process Manager (System 7)
- Color Toolbox traps that postdate CQD 1.0

### 10.2 — Comment out with `#?` prefix

For each System 7+ trap, prefix the header line with `#?`:
```
#? AXXX TrapName toolbox
```

This follows the same convention used in `globals.def` for
uncertain/post-System 6 entries.

### Fence

- [ ] Post-System 6 traps identified and commented out
- [ ] `./bld/macos/tests` passes
- [ ] Commit: `"traps: phase 10 — comment out System 7+ traps"`

---

## Phase 11 — Validate

### 11.1 — Build and test

```
cmake --build bld/macos && ./bld/macos/tests
```

### 11.2 — Load-time validation

Run the emulator and check stderr for any "unknown type" or
"bad param" warnings from traps.def loading.  Zero warnings expected.

### 11.3 — Spot-check against Inside Macintosh

Verify a sample of 10 traps from different managers against the
corresponding Inside Macintosh documentation:

1. **NewHandle** (A122) — Memory Manager OS trap, D0 in/out
2. **GetResource** (A9A0) — Resource Manager Toolbox
3. **Open** (A000) — File Manager OS trap with ^IOParam
4. **NewWindow** (A913) — Window Manager Toolbox
5. **ModalDialog** (A991) — Dialog Manager Toolbox
6. **TENew** (A9D2) — TextEdit Toolbox
7. **CopyBits** (A8EC) — QuickDraw Toolbox
8. **MenuSelect** (A93D) — Menu Manager Toolbox
9. **SndPlay** (A805) — Sound Manager Toolbox
10. **RGBForeColor** (AA14) — Color QuickDraw Toolbox

For each, verify:
- Correct convention (os/toolbox)
- Correct parameter names and types
- Correct register assignments (OS traps)
- Correct parameter order (Toolbox traps)

### 11.4 — Clean up

Delete `assets/AUDIT.md` (temporary file from Phase 1).

### Fence

- [ ] Build clean
- [ ] All tests pass
- [ ] Zero load-time warnings
- [ ] 10-trap spot-check passes
- [ ] AUDIT.md deleted
- [ ] Commit: `"traps: phase 11 — validate and clean up"`
