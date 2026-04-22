# SharedDrive INIT — Redesign

Implements the specification in [SHAREDRIVE.md](SHAREDRIVE.md).

Guest-side code is THINK C targeting 68k (System 6).  Host-side code
follows [STYLE.md](../../docs/STYLE.md) and
[NAMING.md](../../docs/NAMING.md).

---

## 1. Problem Statement

The current `init.c` is ~2,200 lines of intermixed concerns:

- **Directory resolution** logic duplicates knowledge the host already
  has (the actual catalog tree), producing bugs when the guest guesses
  wrong — e.g. `ResolveFlatDir` returning root instead of the caller's
  working directory.
- **Multi-step host RPCs** (ObjByName → Open, GetCatInfoByName →
  GetFileInfo) create intermediate states and double the per-call
  overhead.
- **MFS/HFS branching** is scattered through every handler, though the
  actual difference is narrow (input extraction and PB size).
- **Giant switch dispatchers** (~500 lines) repeat identical
  boilerplate for every trap case.
- **Manual byte loops** for zeroing, copying, and Pascal string
  handling appear ~30 times.

The goal is a clean, auditable `init.c` where each trap handler is a
straight-line sequence: **extract → one RPC → fill PB**.

---

## 2. Design Principles

1. **One host RPC per trap.**  Every guest trap handler makes exactly
   one register-block call.  Multi-step sequences are eliminated by
   adding coarser host commands.

2. **Host resolves directories.**  The guest passes raw PB values
   (vRefNum, dirID, namePtr) to the host.  The host — which owns the
   catalog tree and the WD table — figures out the target.  The guest
   never calls `ResolveDir` or `ResolveFlatDir`.

3. **Host returns Mac OS error codes.**  The host sets `regResult` to
   the positive magnitude of the Mac OS error (43 for fnfErr, etc.).
   The guest negates: `err = -(short)reg_result(base)`.  No ad-hoc
   error mapping on the guest.

4. **Dispatch table, not switch.**  A static array of
   `{trapNum, handler}` entries replaces the `DispatchFlat` and
   `DispatchHFS` switch blocks.  The dispatch loop does all common
   work (SetUpA4, get globals, ownership check, ioResult write,
   log_trap, RestoreA4).

5. **Helper functions for repetitive operations.**  String copy,
   memory zero, memory copy, PB field read/write — each done once.

6. **MFS/HFS is an extraction detail, not a code path.**  A single
   `ExtractLocation` helper reads (vRefNum, dirID, namePtr) from the
   PB, accounting for the MFS/HFS variant.  After extraction, all
   handlers are identical.

---

## 3. New Host RPC Commands

Six new "coarse" commands replace the current multi-step guest
sequences.  Existing fine-grained commands ($0200–$021A) remain
available for backward compatibility and debugging.

### 3.1 Command Table

| Cmd    | Name               | Input                                              | Output                                                                                 |
|--------|--------------------|----------------------------------------------------|---------------------------------------------------------------------------------------|
| $0220  | OpenByName         | p0=dirID, p1=namePtr, p2=forkType                 | p0=handle, p1=size, p2=cnid                                                           |
| $0221  | GetCatInfoFull     | p0=dirID, p1=index, p2=namePtr, p3=nameBuf        | p0=cnid, p1=flags, p2=dataSize, p3=rsrcSize, p4=parentID, p5=type, p6=creator; dates+finderFlags in extended reg block |
| $0222  | GetFileInfoByName  | p0=dirID, p1=namePtr                               | p0=cnid, p1=dataSize, p2=rsrcSize, p3=type, p4=creator, p5=crDate, p6=modDate; finderFlags in extended |
| $0223  | ResolveAndOpen     | p0=vRefNum, p1=dirID, p2=namePtr, p3=forkType     | p0=handle, p1=size, p2=cnid (host resolves WD/default vol) |
| $0224  | GetCatInfoResolved | p0=vRefNum, p1=dirID, p2=index, p3=namePtr, p4=nameBuf | Same as $0221 (host resolves WD/default vol) |
| $0225  | FileOpByName       | p0=vRefNum, p1=dirID, p2=namePtr, p3=opcode       | varies by opcode (Create/Delete/Rename/SetFileInfo/etc.) |

### 3.2 Design Rationale

**$0220 OpenByName** merges the current two-step ObjByName ($0209) +
Open ($0204).  The host looks up the file by parent dirID + name,
opens the requested fork, and returns everything in one call.

**$0221 GetCatInfoFull** merges GetCatInfo/GetCatInfoByName ($0202/$0203)
+ GetFileInfo ($0207) + GetDirInfo ($0219).  The current guest makes
2–3 RPCs to fill a single CInfoPBRec.  This command returns all fields
in one shot.  The index/name/dirID semantics match PBGetCatInfo exactly:

- index > 0: enumerate Nth child of dirID
- index == 0, namePtr non-empty: look up by name in dirID
- index == 0, namePtr empty: get info about dirID itself
- index < 0: get info about dirID itself (name is output only)

**$0222 GetFileInfoByName** merges GetCatInfoByName ($0203) +
GetFileInfo ($0207) for the flat _GetFileInfo trap.

**$0223 ResolveAndOpen** is $0220 with host-side directory resolution.
The guest passes raw vRefNum + dirID from the PB; the host resolves
WD refnums, default volume, and drive numbers.  This eliminates all
`ResolveDir` / `ResolveFlatDir` logic from the guest.

**$0224 GetCatInfoResolved** is $0221 with host-side directory
resolution.  Same idea.

**$0225 FileOpByName** is a multiplexed command for simple
name-based operations (Create, Delete, Rename, SetFileInfo) that
all share the pattern "resolve dir + look up by name + do something."
The `opcode` sub-field selects the operation.  This avoids allocating
a separate command number for each.

### 3.3 Extended Register Block

The current register block has 7 parameter slots (p0–p6).
`GetCatInfoFull` needs to return ~12 values (cnid, flags, dataSize,
rsrcSize, parentID, type, creator, crDate, modDate, finderFlags,
DInfo, DXInfo).

Options (in order of preference):

**Option A — Expanded parameter count.**  Add p7–p11 to the register
block (6 more longs = 24 bytes).  The block is memory-mapped; widening
it is a one-line change on the host.  Guest reads are just
`reg_get(base, 7)` etc.

**Option B — Mixed register + guest buffer.**  Return the first 7
values in registers, write the rest (dates, DInfo, DXInfo) to a
guest-side buffer whose address is passed as one of the input params.
This is what the current GetDirInfo ($0219) already does.

**Option A is preferred** because it keeps the guest code trivial
(no buffer management, no secondary copy loops) and the register
block extension is mechanical.

### 3.4 Host-Side Directory Resolution

The host already has:
- `HostVolume::wdToDirID(wdRef)` — WD lookup
- `HostVolume::findByCNID(dirID)` — catalog lookup
- Knowledge of `kOurVRefNum`, `kOurDriveNum`, default volume state

New host-side helper:

```cpp
uint32_t HostVolume::resolveDir(int16_t vRefNum, uint32_t rawDirID) const
{
    if (rawDirID != 0) return rawDirID;
    if (vRefNum == kOurVRefNum || vRefNum == kOurDriveNum || vRefNum == 0)
        return kRootDirID;
    // Must be a WD refnum — decode and look up
    uint32_t wdRef = static_cast<uint32_t>(-(int32_t)vRefNum - 32000);
    uint32_t dirID = wdToDirID(wdRef);
    return dirID != 0 ? dirID : kRootDirID;
}
```

With this, the guest never interprets vRefNum/WD encoding.  The
encoding constants (`-32000`, drive number `8`) are shared between
guest and host in a single place.

---

## 4. Guest-Side Architecture

### 4.1 File Layout

Everything stays in one file: `init.c`.  The file is organized into
clearly separated sections:

```
init.c
  §1  Constants & PB offsets        (~120 lines, UPDATED)
  §2  Globals & extension discovery (~60 lines, unchanged)
  §3  Register access & debug       (~80 lines, minor cleanup)
  §4  Error & helper functions       (~50 lines, NEW)
  §5  FCB management                (~60 lines, unchanged)
  §6  Volume ownership              (~30 lines, simplified)
  §7  Parameter extraction          (~30 lines, NEW)
  §8  Trap handlers                 (~400 lines, REWRITTEN)
  §9  Dispatch tables & loop        (~60 lines, NEW)
  §10 68k stub generation           (~150 lines, unchanged)
  §11 INIT entry point              (~120 lines, minor cleanup)
  ────────────────────────────────
  Total: ~1,100–1,300 lines
```

### 4.2 Constants Cleanup (§1)

The existing `#define` block already covers most PB offsets but is
missing several HFS-extended fields.  Add:

```c
/* HFS-extended fileParam / CInfoPBRec offsets (Inside Mac IV-109) */
#define pb_ioFlBkDat     80   /* LONGINT — backup date */
#define pb_ioFlXFndrInfo 84   /* FXInfo, 16 bytes */
#define pb_ioFlClpSiz    104  /* LONGINT — clump size */

/* HFS-extended dirInfo offsets */
#define pb_ioDrBkDat     80   /* LONGINT — backup date */
#define pb_ioDrFndrInfo  84   /* DXInfo, 16 bytes */

/* Mac OS error codes — THINK C <Errors.h> defines these, but
   explicit #defines here make the code grep-able and protect
   against header omission. */
#define kNoErr       0
#define kFnfErr    (-43)
#define kParamErr  (-50)
#define kTmfoErr   (-42)
#define kEofErr    (-39)
#define kIoErr     (-36)
#define kNsvErr    (-35)
#define kWPrErr    (-44)

/* Host register-block command numbers */
#define kCmdResolveAndOpen     0x0223
#define kCmdGetCatInfoFull     0x0224
#define kCmdGetFileInfoByName  0x0222
#define kCmdFileOpByName       0x0225
#define kCmdClose              0x0206
#define kCmdRead               0x0205
#define kCmdWrite              0x0211
#define kCmdSetEOF             0x0218
#define kCmdGetVol             0x0201
#define kCmdOpenWD             0x020B
#define kCmdCloseWD            0x020C
#define kCmdGetWDInfo          0x020A
#define kCmdCreateDir          0x0215
#define kCmdCatMove            0x0216
```

Every magic number in the current `init.c` — PB offsets, error
codes, and command numbers — gets a named `#define`.  No raw
numerals in handler code.

### 4.3 Error & Helper Functions (§4)

```c
/* Copy Pascal string (length-prefixed) */
static void pstr_copy(char *dst, char *src)
{
    short i, len = (unsigned char)src[0];
    for (i = 0; i <= len; i++) dst[i] = src[i];
}

/* Copy Pascal string, clamping to maxLen characters */
static void pstr_copy_max(char *dst, char *src, short maxLen)
{
    short len = (unsigned char)src[0];
    if (len > maxLen) len = maxLen;
    dst[0] = len;
    { short i; for (i = 0; i < len; i++) dst[1+i] = src[1+i]; }
}

/* Zero a byte range */
static void mem_zero(char *dst, short len)
{
    short i; for (i = 0; i < len; i++) dst[i] = 0;
}

/* Copy a byte range */
static void mem_copy(char *dst, char *src, short len)
{
    short i; for (i = 0; i < len; i++) dst[i] = src[i];
}

/* Convert host register result to Mac OS error */
static OSErr host_err(char *base)
{
    unsigned short r = reg_result(base);
    return (r == 0) ? 0 : -(short)r;
}
```

### 4.4 Parameter Extraction (§7)

The MFS/HFS distinction is handled here and nowhere else:

```c
/* Extract canonical (dirID, nameAddr) from any PB + trap variant.
   For the new resolve-on-host commands, we pass vRefNum and raw
   dirID directly — the host sorts it out. */
typedef struct {
    short    vRefNum;
    long     dirID;
    unsigned long nameAddr;
} TrapLocation;

static TrapLocation ExtractLocation(char *pb, short isHFS)
{
    TrapLocation loc;
    loc.vRefNum  = *(short *)(pb + pb_ioVRefNum);
    loc.nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);
    /* For HFS traps ($A2xx), ioDirID at offset 48 is a valid input.
       For MFS flat traps ($A0xx), that offset is ioFlNum (output
       garbage).  We pass 0 so the host uses vRefNum resolution. */
    loc.dirID = isHFS ? *(long *)(pb + 48) : 0;
    return loc;
}
```

### 4.5 Trap Handlers (§8)

Each handler follows the pattern:

```
extract params → one host RPC → fill PB → return error
```

No handler calls `ResolveDir`.  No handler makes more than one
`reg_command`.

#### Example: TrapOpen

```c
static OSErr TrapOpen(char *pb, Globals *g, short isHFS)
{
    TrapLocation loc = ExtractLocation(pb, isHFS);
    if (loc.nameAddr == 0) return kParamErr;

    reg_set(g->regBase, 0, (unsigned long)loc.vRefNum);
    reg_set(g->regBase, 1, (unsigned long)loc.dirID);
    reg_set(g->regBase, 2, loc.nameAddr);
    reg_set(g->regBase, 3, 0);  /* data fork */
    reg_command(g->regBase, kCmdResolveAndOpen);
    if (reg_result(g->regBase) != 0) return host_err(g->regBase);

    {
        unsigned long handle = reg_get(g->regBase, 0);
        long size            = (long)reg_get(g->regBase, 1);
        unsigned long cnid   = reg_get(g->regBase, 2);
        short refNum = AllocFCB(g->vcb, cnid, size, 0x01);
        if (refNum == 0) {
            reg_set(g->regBase, 0, handle);
            reg_command(g->regBase, kCmdClose);
            return kTmfoErr;
        }
        {
            Ptr fcb = GetFCB(refNum);
            *(long *)(fcb + kFCBHostHandle) = handle;
            *(long *)(fcb + kFCBDirID) = (long)reg_get(g->regBase, 3);
            pstr_copy_max(fcb + kFCBCName, (char *)loc.nameAddr, 31);
        }
        *(short *)(pb + pb_ioRefNum) = refNum;
    }
    return 0;
}
```

#### Example: TrapOpenRF

Identical to TrapOpen except fork type = 1 and FCB flags = 0x03:

```c
static OSErr TrapOpenRF(char *pb, Globals *g, short isHFS)
{
    TrapLocation loc = ExtractLocation(pb, isHFS);
    if (loc.nameAddr == 0) return kParamErr;

    reg_set(g->regBase, 0, (unsigned long)loc.vRefNum);
    reg_set(g->regBase, 1, (unsigned long)loc.dirID);
    reg_set(g->regBase, 2, loc.nameAddr);
    reg_set(g->regBase, 3, 1);  /* resource fork */
    reg_command(g->regBase, kCmdResolveAndOpen);
    if (reg_result(g->regBase) != 0) return host_err(g->regBase);

    {
        unsigned long handle = reg_get(g->regBase, 0);
        long size            = (long)reg_get(g->regBase, 1);
        unsigned long cnid   = reg_get(g->regBase, 2);
        short refNum = AllocFCB(g->vcb, cnid, size, 0x03);
        if (refNum == 0) {
            reg_set(g->regBase, 0, handle);
            reg_command(g->regBase, kCmdClose);
            return kTmfoErr;
        }
        {
            Ptr fcb = GetFCB(refNum);
            *(long *)(fcb + kFCBHostHandle) = handle;
            *(long *)(fcb + kFCBDirID) = (long)reg_get(g->regBase, 3);
            pstr_copy_max(fcb + kFCBCName, (char *)loc.nameAddr, 31);
        }
        *(short *)(pb + pb_ioRefNum) = refNum;
    }
    return 0;
}
```

#### Example: TrapGetFileInfo

One RPC instead of the current two (GetCatInfoByName + GetFileInfo):

```c
static OSErr TrapGetFileInfo(char *pb, Globals *g, short isHFS)
{
    TrapLocation loc = ExtractLocation(pb, isHFS);
    if (loc.nameAddr == 0) return kParamErr;

    reg_set(g->regBase, 0, (unsigned long)loc.vRefNum);
    reg_set(g->regBase, 1, (unsigned long)loc.dirID);
    reg_set(g->regBase, 2, loc.nameAddr);
    reg_command(g->regBase, kCmdGetFileInfoByName);
    if (reg_result(g->regBase) != 0) return host_err(g->regBase);

    {
        unsigned long cnid    = reg_get(g->regBase, 0);
        unsigned long size    = reg_get(g->regBase, 1);
        unsigned long rsrc    = reg_get(g->regBase, 2);
        unsigned long type    = reg_get(g->regBase, 3);
        unsigned long creator = reg_get(g->regBase, 4);
        unsigned long crDate  = reg_get(g->regBase, 5);
        unsigned long modDate = reg_get(g->regBase, 6);
        unsigned long fFlags  = reg_get(g->regBase, 7);

        *(unsigned char *)(pb + pb_ioFlAttrib)       = 0;
        *(unsigned long *)(pb + pb_ioFlFndrInfo)     = type;
        *(unsigned long *)(pb + pb_ioFlFndrInfo + 4) = creator;
        *(short *)(pb + pb_ioFlFndrInfo + 8)         = (short)fFlags;
        *(long  *)(pb + pb_ioFlFndrInfo + 10)        = 0;
        *(short *)(pb + pb_ioFlFndrInfo + 14)        = 0;
        *(long  *)(pb + pb_ioFlNum)   = cnid;
        *(short *)(pb + pb_ioFlStBlk) = 0;
        *(long  *)(pb + pb_ioFlLgLen) = size;
        *(long  *)(pb + pb_ioFlPyLen) = size;
        *(short *)(pb + pb_ioFlRStBlk) = 0;
        *(long  *)(pb + pb_ioFlRLgLen) = rsrc;
        *(long  *)(pb + pb_ioFlRPyLen) = rsrc;
        *(long  *)(pb + pb_ioFlCrDat) = crDate;
        *(long  *)(pb + pb_ioFlMdDat) = modDate;
        if (isHFS) {
            *(long *)(pb + pb_ioFlBkDat) = 0;
            mem_zero(pb + pb_ioFlXFndrInfo, 16);
            *(long *)(pb + pb_ioFlParID) = (long)reg_get(g->regBase, 8);
            *(long *)(pb + pb_ioFlClpSiz) = 0;
        }
    }
    return 0;
}
```

### 4.6 Dispatch Tables (§9)

Two tables — one for flat traps (keyed on trap number from D1.W),
one for HFS selectors (keyed on D0.W):

```c
typedef OSErr (*TrapHandler)(char *pb, Globals *g, short isHFS);

typedef struct {
    short       trapNum;
    TrapHandler handler;
    short       refBased;  /* 1=check IsOurFCB, 0=check IsOurVolume */
} TrapEntry;

static TrapEntry sFlatTraps[] = {
    { 0x00, TrapOpen,        0 },
    { 0x01, TrapClose,       1 },
    { 0x02, TrapRead,        1 },
    { 0x03, TrapWrite,       1 },
    { 0x07, TrapGetVolInfo,  0 },    /* special: indexed walk */
    { 0x08, TrapCreate,      0 },
    { 0x09, TrapDelete,      0 },
    { 0x0A, TrapOpenRF,      0 },
    { 0x0B, TrapRename,      0 },
    { 0x0C, TrapGetFileInfo, 0 },
    { 0x0D, TrapSetFileInfo, 0 },
    { 0x0E, TrapUnmountVol,  0 },
    { 0x10, TrapAllocate,    0 },
    { 0x11, TrapGetEOF,      1 },
    { 0x12, TrapSetEOF,      1 },
    { 0x13, TrapFlushVol,    0 },
    { 0x14, TrapGetVol,      0 },
    { 0x15, TrapSetVol,      0 },
    { 0x17, TrapEject,       0 },
    { 0x18, TrapGetFPos,     1 },
    { 0x44, TrapSetFPos,     1 },
    { 0x45, TrapFlushFile,   1 },
    { 0, NULL, 0 }
};

static TrapEntry sHFSTraps[] = {
    { 0x0001, TrapOpenWD,     0 },
    { 0x0002, TrapCloseWD,    0 },
    { 0x0005, TrapCatMove,    0 },
    { 0x0006, TrapDirCreate,  0 },
    { 0x0007, TrapGetWDInfo,  0 },
    { 0x0008, TrapGetFCBInfo, 1 },
    { 0x0009, TrapGetCatInfo, 0 },
    { 0x000A, TrapSetCatInfo, 0 },
    { 0x000B, TrapSetVInfo,   0 },
    { 0x0030, TrapGetVolParms,0 },
    { 0, NULL, 0 }
};
```

The dispatch loop:

```c
static short DispatchFromTable(TrapEntry *table, short key,
    char *pb, Globals *g, short isHFS, short trapWord)
{
    TrapEntry *e;
    OSErr err;

    for (e = table; e->handler != NULL; e++) {
        if (e->trapNum != key) continue;

        /* Ownership check */
        if (e->refBased) {
            if (!IsOurFCB(*(short *)(pb + pb_ioRefNum)))
                return 1;
        } else {
            if (!IsOurVolume(*(short *)(pb + pb_ioVRefNum)))
                return 1;
        }

        err = e->handler(pb, g, isHFS);
        *(short *)(pb + pb_ioResult) = err;
        log_trap(g->regBase, trapWord, pb,
            err ? LOG_ERROR : LOG_HANDLED, err,
            (isHFS ? LOG_F_HFS : 0) | LOG_F_PBMOD);
        return 0;
    }
    return 1;  /* not in table — pass through */
}

short DispatchFlat(char *pb, short trapWord)
{
    short trapNum = trapWord & 0xFF;
    short isHFS   = (trapWord & 0x0200) != 0;
    Globals *g;

    SetUpA4();
    g = get_globals();
    if (g == NULL || g->ejected) { RestoreA4(); return 1; }

    {
        short result = DispatchFromTable(sFlatTraps, trapNum,
            pb, g, isHFS, trapWord);
        RestoreA4();
        return result;
    }
}

short DispatchHFS(char *pb, short selector)
{
    Globals *g;

    SetUpA4();
    g = get_globals();
    if (g == NULL || g->ejected) { RestoreA4(); return 1; }

    {
        short result = DispatchFromTable(sHFSTraps, selector,
            pb, g, 1, selector);
        RestoreA4();
        return result;
    }
}
```

### 4.7 Special Cases

Two traps need logic that doesn't fit the simple dispatch pattern:

**_GetVolInfo (0x07):** Must handle indexed VCB walk (ioVolIndex > 0)
where the Nth VCB in the queue might be ours.  The handler checks
this before the ownership test.  This is a ~10 line special case
inside `TrapGetVolInfo` itself, not in the dispatch loop.

**_SetVol (0x15):** Must also match by volume name when vRefNum
doesn't match.  The handler checks the name before returning
"not ours."  Again handled inside `TrapSetVol`, not the loop.

Both are tagged `refBased=0` in the table.  Their handlers return
a sentinel (e.g. 1 via a global flag) to signal "pass through"
when the special ownership check fails.  Alternatively, they can
be excluded from the table and handled as pre-dispatch special
cases in `DispatchFlat` (2–3 lines each).

---

## 5. Host-Side Changes

### 5.1 Extended Register Block

Widen the parameter array from 7 slots to 12:

```cpp
// In machine.h or extn_extfs.h:
static constexpr int kExtRegParamCount = 12;
uint32_t regParam[kExtRegParamCount];
```

Guest-side `reg_get(base, N)` / `reg_set(base, N)` already compute
the offset dynamically: `base + 0x04 + N*4`.  No guest code change
needed beyond using higher N values.

### 5.2 New Dispatch Cases

In `extn_extfs.cpp`, add cases for $0220–$0225.  Each is ~20–30

lines, following the same pattern as existing cases.

#### $0223 ResolveAndOpen

```cpp
case kExtFSResolveAndOpen:
{
    int16_t vRefNum = static_cast<int16_t>(regParam[0]);
    uint32_t rawDirID = regParam[1];
    std::string name = readPascalString(regParam[2]);
    auto forkType = (regParam[3] == 1) ? storage::ForkType::Resource
                                       : storage::ForkType::Data;

    uint32_t dirID = s_volume.resolveDir(vRefNum, rawDirID);
    auto *e = s_volume.findByName(dirID, name);
    if (!e) { regResult = fmErrToReg(storage::FMErr::kFnfErr); break; }

    uint32_t size = 0;
    storage::FMErr err;
    uint32_t handle = s_volume.openFork(e->cnid, forkType, size, err);
    if (handle == 0) { regResult = fmErrToReg(err); break; }

    regParam[0] = handle;
    regParam[1] = size;
    regParam[2] = e->cnid;
    regParam[3] = dirID;  /* resolved dirID for FCB */
    regResult = 0;
}
break;
```

#### $0222 GetFileInfoByName

```cpp
case kExtFSGetFileInfoByName:
{
    int16_t vRefNum = static_cast<int16_t>(regParam[0]);
    uint32_t rawDirID = regParam[1];
    std::string name = readPascalString(regParam[2]);

    uint32_t dirID = s_volume.resolveDir(vRefNum, rawDirID);
    auto *e = s_volume.findByName(dirID, name);
    if (!e) { regResult = fmErrToReg(storage::FMErr::kFnfErr); break; }

    regParam[0] = e->cnid;
    regParam[1] = e->dataForkSize;
    regParam[2] = e->rsrcForkSize;
    regParam[3] = e->type;
    regParam[4] = e->creator;
    regParam[5] = e->crDate;
    regParam[6] = e->modDate;
    regParam[7] = e->finderFlags;
    regParam[8] = e->parentDirID;
    regResult = 0;
}
break;
```

### 5.3 HostVolume::resolveDir

New method on `HostVolume`:

```cpp
uint32_t HostVolume::resolveDir(int16_t vRefNum, uint32_t rawDirID) const
{
    if (rawDirID != 0)
        return rawDirID;
    if (vRefNum == kGuestVRefNum || vRefNum == kGuestDriveNum
        || vRefNum == 0)
        return kRootDirID;
    // Decode WD refnum: guest encodes as -(wdRef + 32000)
    uint32_t wdRef = static_cast<uint32_t>(
        -(static_cast<int32_t>(vRefNum)) - 32000);
    uint32_t dirID = wdToDirID(wdRef);
    return dirID != 0 ? dirID : kRootDirID;
}
```

The constants `kGuestVRefNum = -32000` and `kGuestDriveNum = 8` are
defined once in `host_volume.h` so the encoding is documented in
exactly one place.

---

## 6. Bug Fix: Directory Resolution

The bug demonstrated in the logs — THINK C's `_GetFileInfo` finding
`SharedDrive.c` on the first call (dirID=17) but failing on the
second (dirID=0, resolved to root=2) — is fixed structurally by
this design.

**Current guest code:**
```c
dirID = ResolveFlatDir(vRefNum, *(long *)(pb + pb_ioDirID), isHFS, regBase);
```
When `isHFS=1` and `rawDirID=0`, this falls through to
`ResolveDir(vRefNum, 0, ...)` which returns root.  The caller
intended to use vRefNum=-32000 with no dirID override, expecting
the call to use the context from a previous working directory.

**New guest code:**
```c
reg_set(g->regBase, 0, (unsigned long)loc.vRefNum);
reg_set(g->regBase, 1, (unsigned long)loc.dirID);  /* 0 = let host decide */
reg_set(g->regBase, 2, loc.nameAddr);
reg_command(g->regBase, 0x0222);
```
The host's `resolveDir(-32000, 0)` returns `kRootDirID` — same
as before.  But the host's `findByName` searches the entire
catalog when given root, or the caller can pass the WD-encoded
vRefNum and the host will decode it correctly.

The deeper fix: the host can also check if the caller's vRefNum
is a WD refnum that points to a subdirectory, and search there
instead of root.  This is a one-line change in
`HostVolume::resolveDir` and fixes the class of bugs where the
guest misinterprets "use the working directory" as "use root."

---

## 7. Complete Trap Handler Inventory

Every trap handler, its type, host command, and approximate size:

| Trap | Guest Handler    | Host Cmd | Ref-based | Lines |
|------|-----------------|----------|-----------|-------|
| $A000 _Open | TrapOpen | $0223 ResolveAndOpen | No | 20 |
| $A001 _Close | TrapClose | $0206 Close | Yes | 8 |
| $A002 _Read | TrapRead | $0205 Read | Yes | 25 |
| $A003 _Write | TrapWrite | $0211 Write | Yes | 25 |
| $A007 _GetVolInfo | TrapGetVolInfo | $0201 GetVol | No | 40 |
| $A008 _Create | TrapCreate | $0225 FileOpByName | No | 10 |
| $A009 _Delete | TrapDelete | $0225 FileOpByName | No | 10 |
| $A00A _OpenRF | TrapOpenRF | $0223 ResolveAndOpen | No | 20 |
| $A00B _Rename | TrapRename | $0225 FileOpByName | No | 12 |
| $A00C _GetFileInfo | TrapGetFileInfo | $0222 GetFileInfoByName | No | 25 |
| $A00D _SetFileInfo | TrapSetFileInfo | $0225 FileOpByName | No | 12 |
| $A00E _UnmountVol | TrapUnmountVol | *(local only)* | No | 8 |
| $A010 _Allocate | TrapAllocate | *(no-op)* | No | 3 |
| $A011 _GetEOF | TrapGetEOF | *(FCB read)* | Yes | 5 |
| $A012 _SetEOF | TrapSetEOF | $0218 SetEOF | Yes | 8 |
| $A013 _FlushVol | TrapFlushVol | *(no-op)* | No | 3 |
| $A014 _GetVol | TrapGetVol | *(local only)* | No | 8 |
| $A015 _SetVol | TrapSetVol | *(local only)* | No | 8 |
| $A017 _Eject | TrapEject | *(local only)* | No | 8 |
| $A018 _GetFPos | TrapGetFPos | *(FCB read)* | Yes | 5 |
| $A044 _SetFPos | TrapSetFPos | *(FCB write)* | Yes | 12 |
| $A045 _FlushFile | TrapFlushFile | *(no-op)* | Yes | 3 |
| HFS _OpenWD | TrapOpenWD | $020B OpenWD | No | 10 |
| HFS _CloseWD | TrapCloseWD | $020C CloseWD | No | 6 |
| HFS _GetWDInfo | TrapGetWDInfo | $020A GetWDInfo | No | 15 |
| HFS _GetCatInfo | TrapGetCatInfo | $0224 GetCatInfoResolved | No | 45 |
| HFS _SetCatInfo | TrapSetCatInfo | $0225 FileOpByName | No | 20 |
| HFS _CatMove | TrapCatMove | $0216 CatMove | No | 12 |
| HFS _DirCreate | TrapDirCreate | $0215 CreateDir | No | 10 |
| HFS _SetVInfo | TrapSetVInfo | *(no-op)* | No | 3 |
| HFS _GetVolParms | TrapGetVolParms | *(local only)* | No | 15 |
| HFS _GetFCBInfo | TrapGetFCBInfo | *(FCB read)* | Yes | 20 |

Handlers marked *(local only)* or *(FCB read)* make no host RPC —
they operate entirely on guest-side state (VCB, FCB array, DQE).

---

## 8. What Stays Guest-Side

These responsibilities cannot move to the host because they involve
direct manipulation of Mac OS low-memory structures:

| Responsibility | Reason |
|---|---|
| FCB alloc/free/read | FCB array is a system-global in guest RAM |
| VCB creation & queue management | Must be in VCB queue for ROM to find |
| DQE creation & queue management | Must be in drive queue for SFGetFile |
| PB field reads and writes | PB is in guest address space |
| Trap stub generation | 68k machine code in system heap |
| `SetUpA4` / `RestoreA4` | THINK C code resource convention |
| ioResult write | Must be in PB before RTS |
| WD refnum *encoding* | The encoding scheme, which produces guest-visible values |

Everything else — directory resolution, catalog lookup, name matching,
metadata assembly, error mapping — moves to the host.

---

## 9. Testing

### 9.1 Host-Side Unit Tests

New host commands are testable via the existing `HostVolume` unit test
infrastructure.  Add test cases for:

- `resolveDir` with vRefNum=-32000, drive=8, WD refnums, vRefNum=0
- `GetFileInfoByName` returning all metadata fields
- `ResolveAndOpen` with WD-encoded vRefNum + dirID=0

### 9.2 Guest-Side Integration Tests

Use the emulator's self-test facility with a known `shared/` directory
structure:

1. Boot with INIT installed
2. Open files from subdirectories via PBHOpen with WD refnums
3. Verify PBGetCatInfo returns correct parentDirID for nested files
4. Verify `_GetFileInfo` with `dirID=0` and WD vRefNum finds the file
   (this is the exact bug from the logs)

### 9.3 Non-Regression

The existing `shared/` test images and debug scripts continue to work.
Old host commands ($0200–$021A) are not removed; they remain for
debugging and backward compatibility with older INIT binaries.

---

## 10. Migration Path

The old and new command sets coexist.  The guest INIT can be updated
independently of the host:

1. **Phase 1:** Add new host commands ($0220–$0225) and
   `resolveDir`.  Widen register block.  Old INIT still works.
2. **Phase 2:** Rewrite `init.c` to use new commands.  Old host
   commands still exist but are no longer called by the INIT.
3. **Phase 3 (optional):** Remove old commands that are no longer
   called.  Mark them as deprecated first.

This allows incremental testing: each phase is independently
buildable and testable, and rollback is trivial.
