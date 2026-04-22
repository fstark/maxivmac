# SharedDrive — Phase 2 Implementation Plan

Design: [SHAREDRIVE_DESIGN.md](SHAREDRIVE_DESIGN.md)
Spec: [SHAREDRIVE.md](SHAREDRIVE.md)
Prerequisite: [SHAREDRIVE_PLAN1.md](SHAREDRIVE_PLAN1.md) (all phases complete)

Phase 2 rewrites `init.c` to use the new coarse host commands,
dispatch tables, and helper functions.  The old INIT is replaced
entirely.  Old host commands ($0200–$021A) remain operational on
the host side for debugging.

| Phase | Description                                    | Status |
|-------|------------------------------------------------|--------|
| 2.1   | Add missing #defines (PB offsets, errors, cmds)|        |
| 2.2   | Add helper functions (pstr, mem, host_err)     |        |
| 2.3   | Add ExtractLocation + TrapLocation struct      |        |
| 2.4   | Rewrite FCB-based handlers (Close, Read, Write, EOF, FPos, Flush) |  |
| 2.5   | Rewrite Open/OpenRF using ResolveAndOpen       |        |
| 2.6   | Rewrite GetFileInfo/SetFileInfo using new RPCs |        |
| 2.7   | Rewrite GetCatInfo using GetCatInfoResolved    |        |
| 2.8   | Rewrite SetCatInfo using FileOpByName          |        |
| 2.9   | Rewrite Create/Delete/Rename using FileOpByName|        |
| 2.10  | Rewrite volume/WD handlers (GetVolInfo, GetVol, SetVol, etc.) |  |
| 2.11  | Rewrite GetVolParms, GetFCBInfo, DirCreate, CatMove |   |
| 2.12  | Replace dispatchers with table + loop          |        |
| 2.13  | Delete dead code (ResolveDir, old handlers)    |        |
| 2.14  | End-to-end test                                |        |

Build gate: THINK C project compiles to INIT resource (manual)
Test gate:  Boot emulator, Finder shows "Shared", open files from
            subdirectories, copy files, eject/remount

---

## Phase 2.1 — Add Missing #defines

Add PB offset constants, error codes, and host command numbers that
the current `init.c` lacks.

### 2.1.1 — Modify `macsrc/shareddrive/init.c`

**In the PB offsets section** (after `pb_ioFlParID`), add:

```c
/* HFS-extended fileParam / CInfoPBRec offsets (Inside Mac IV-109) */
#define pb_ioFlBkDat     80   /* LONGINT — backup date */
#define pb_ioFlXFndrInfo 84   /* FXInfo, 16 bytes */
#define pb_ioFlClpSiz    104  /* LONGINT — clump size */

/* HFS-extended dirInfo offsets */
#define pb_ioDrBkDat     80   /* LONGINT — backup date */
#define pb_ioDrFndrInfo  84   /* DXInfo, 16 bytes */
```

**After the existing constants**, add:

```c
/* Mac OS error codes */
#define kNoErr       0
#define kIoErr     (-36)
#define kEofErr    (-39)
#define kTmfoErr   (-42)
#define kFnfErr    (-43)
#define kWPrErr    (-44)
#define kParamErr  (-50)
#define kNsvErr    (-35)

/* Host register-block command numbers (coarse, Phase 1) */
#define kCmdResolveAndOpen     0x0223
#define kCmdGetCatInfoResolved 0x0224
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
#define kCmdGetDirInfo         0x0219
#define kCmdSetDirInfo         0x021A

/* FileOpByName sub-opcodes */
#define kFileOpCreate      0
#define kFileOpDelete      1
#define kFileOpRename      2
#define kFileOpSetFileInfo 3
#define kFileOpSetCatInfo  4
```

### Fence

- [ ] All new `#define`s compile (no conflicts with THINK C headers)
- [ ] No behavioral change yet
- [ ] Commit: `"init.c: add named constants for PB offsets, errors, commands"`

---

## Phase 2.2 — Add Helper Functions

Add `pstr_copy`, `pstr_copy_max`, `mem_zero`, `mem_copy`, and
`host_err` immediately after the debug/logging section (before FCB
management).

### 2.2.1 — Add to `macsrc/shareddrive/init.c`

Insert after the `dbg_hexdump` function and before `AllocFCB`:

```c
/* ---- Helpers ---- */

static void pstr_copy(char *dst, char *src)
{
    short i, len = (unsigned char)src[0];
    for (i = 0; i <= len; i++) dst[i] = src[i];
}

static void pstr_copy_max(char *dst, char *src, short maxLen)
{
    short len = (unsigned char)src[0];
    if (len > maxLen) len = maxLen;
    dst[0] = len;
    { short i; for (i = 0; i < len; i++) dst[1+i] = src[1+i]; }
}

static void mem_zero(char *dst, short len)
{
    short i; for (i = 0; i < len; i++) dst[i] = 0;
}

static void mem_copy(char *dst, char *src, short len)
{
    short i; for (i = 0; i < len; i++) dst[i] = src[i];
}

static OSErr host_err(char *base)
{
    unsigned short r = reg_result(base);
    return (r == 0) ? kNoErr : -(short)r;
}
```

### Fence

- [ ] Helpers compile
- [ ] No behavioral change
- [ ] Commit: combined with 2.3

---

## Phase 2.3 — Add ExtractLocation

Add the `TrapLocation` struct and `ExtractLocation` function after
the helpers, before the trap handlers.

### 2.3.1 — Add to `macsrc/shareddrive/init.c`

```c
/* ---- Parameter extraction ---- */

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
    loc.dirID = isHFS ? *(long *)(pb + 48) : 0;
    return loc;
}
```

### Fence

- [ ] Compiles
- [ ] Commit: `"init.c: add helpers and ExtractLocation"`

---

## Phase 2.4 — Rewrite FCB-Based Handlers

These handlers operate on an already-open file (keyed on ioRefNum).
They don't need directory resolution and mostly don't call the host
(except Read, Write, Close).  Rewrite them to use named constants
and helpers.

### 2.4.1 — Replace handlers in `init.c`

Replace `DoRead`, `DoWrite`, `DoClose`, `DoGetEOF`, `DoSetFPos`,
`DoGetFPos` with new versions named `TrapRead`, `TrapWrite`,
`TrapClose`, `TrapGetEOF`, `TrapSetEOF`, `TrapGetFPos`,
`TrapSetFPos`, `TrapFlushFile`, `TrapAllocate`.

Each follows the pattern: get FCB, operate, return error.

**TrapClose:**
```c
static OSErr TrapClose(char *pb, Globals *g, short isHFS)
{
    short refNum = *(short *)(pb + pb_ioRefNum);
    Ptr fcb = GetFCB(refNum);
    if (fcb == NULL) return kFnfErr;
    reg_set(g->regBase, 0, *(unsigned long *)(fcb + kFCBHostHandle));
    reg_command(g->regBase, kCmdClose);
    FreeFCB(refNum);
    return kNoErr;
}
```

**TrapRead:**
```c
static OSErr TrapRead(char *pb, Globals *g, short isHFS)
{
    short refNum = *(short *)(pb + pb_ioRefNum);
    unsigned long buffer = *(unsigned long *)(pb + pb_ioBuffer);
    long reqCount = *(long *)(pb + pb_ioReqCount);
    short posMode = *(short *)(pb + pb_ioPosMode);
    long posOffset = *(long *)(pb + pb_ioPosOffset);
    Ptr fcb;
    long mark, eof, handle;
    unsigned long actual;

    fcb = GetFCB(refNum);
    if (fcb == NULL) return kFnfErr;

    mark   = *(long *)(fcb + kFCBCrPs);
    eof    = *(long *)(fcb + kFCBEOF);
    handle = *(long *)(fcb + kFCBHostHandle);

    switch (posMode & 0x03) {
        case 1: mark = posOffset; break;
        case 2: mark = eof + posOffset; break;
        case 3: mark += posOffset; break;
    }
    if (mark < 0) mark = 0;
    if (mark > eof) mark = eof;

    if (reqCount <= 0) {
        *(long *)(pb + pb_ioActCount)  = 0;
        *(long *)(pb + pb_ioPosOffset) = mark;
        *(long *)(fcb + kFCBCrPs) = mark;
        return kNoErr;
    }
    if (mark + reqCount > eof)
        reqCount = eof - mark;

    reg_set(g->regBase, 0, handle);
    reg_set(g->regBase, 1, (unsigned long)mark);
    reg_set(g->regBase, 2, (unsigned long)reqCount);
    reg_set(g->regBase, 3, buffer);
    reg_command(g->regBase, kCmdRead);
    actual = reg_get(g->regBase, 0);

    mark += actual;
    *(long *)(fcb + kFCBCrPs) = mark;
    *(long *)(pb + pb_ioActCount) = actual;
    *(long *)(pb + pb_ioPosOffset) = mark;
    return (actual < (unsigned long)reqCount) ? kEofErr : kNoErr;
}
```

**TrapWrite** — same structure, uses `kCmdWrite`, updates EOF if
mark grows past it, returns `kIoErr` on short write.

**TrapGetEOF, TrapGetFPos, TrapSetFPos** — same logic as current
`DoGetEOF`, `DoGetFPos`, `DoSetFPos` but with named error codes.

**TrapSetEOF** — now issues `kCmdSetEOF` to the host in addition
to updating the local FCB (fixes the missing host-notify bug from
the current code):
```c
static OSErr TrapSetEOF(char *pb, Globals *g, short isHFS)
{
    short refNum = *(short *)(pb + pb_ioRefNum);
    long newEOF = *(long *)(pb + pb_ioMisc);
    Ptr fcb = GetFCB(refNum);
    if (fcb == NULL) return kFnfErr;

    reg_set(g->regBase, 0, *(unsigned long *)(fcb + kFCBHostHandle));
    reg_set(g->regBase, 1, (unsigned long)newEOF);
    reg_command(g->regBase, kCmdSetEOF);

    *(long *)(fcb + kFCBEOF) = newEOF;
    *(long *)(fcb + kFCBPLen) = newEOF;
    return kNoErr;
}
```

**TrapFlushFile, TrapAllocate** — trivial no-ops returning `kNoErr`.

### Fence

- [ ] All 9 FCB-based handlers compile
- [ ] Old `DoRead`/`DoWrite`/`DoClose`/etc. still exist (not yet deleted)
- [ ] Commit: `"init.c: rewrite FCB-based trap handlers"`

---

## Phase 2.5 — Rewrite Open / OpenRF

Replace `DoOpen` and `DoOpenRF` with `TrapOpen` and `TrapOpenRF`
using `kCmdResolveAndOpen` ($0223).

### 2.5.1 — Add to `init.c`

Implement exactly as shown in SHAREDRIVE_DESIGN.md §4.5
(TrapOpen and TrapOpenRF examples).  Both use `ExtractLocation`,
one RPC, `AllocFCB`, `pstr_copy_max` for the FCB name.

The only difference: `TrapOpen` passes fork=0 + flags=0x01,
`TrapOpenRF` passes fork=1 + flags=0x03.

### Fence

- [ ] Both handlers compile
- [ ] Commit: `"init.c: rewrite Open/OpenRF with ResolveAndOpen"`

---

## Phase 2.6 — Rewrite GetFileInfo / SetFileInfo

Replace `DoGetFileInfo` and `DoSetFileInfo` with versions using
the new commands.

### 2.6.1 — TrapGetFileInfo

Uses `kCmdGetFileInfoByName` ($0222).  Implement exactly as shown
in SHAREDRIVE_DESIGN.md §4.5 (TrapGetFileInfo example).  Uses p7
and p8 from the extended register block.

### 2.6.2 — TrapSetFileInfo

Uses `kCmdFileOpByName` ($0225) with `kFileOpSetFileInfo`:

```c
static OSErr TrapSetFileInfo(char *pb, Globals *g, short isHFS)
{
    TrapLocation loc = ExtractLocation(pb, isHFS);
    if (loc.nameAddr == 0) return kParamErr;

    reg_set(g->regBase, 0, (unsigned long)loc.vRefNum);
    reg_set(g->regBase, 1, (unsigned long)loc.dirID);
    reg_set(g->regBase, 2, loc.nameAddr);
    reg_set(g->regBase, 3, kFileOpSetFileInfo);
    reg_set(g->regBase, 4, *(unsigned long *)(pb + pb_ioFlFndrInfo));
    reg_set(g->regBase, 5, *(unsigned long *)(pb + pb_ioFlFndrInfo + 4));
    reg_set(g->regBase, 6, (unsigned long)(unsigned short)
        *(short *)(pb + pb_ioFlFndrInfo + 8));
    reg_command(g->regBase, kCmdFileOpByName);
    return host_err(g->regBase);
}
```

### Fence

- [ ] Both handlers compile
- [ ] Commit: `"init.c: rewrite GetFileInfo/SetFileInfo"`

---

## Phase 2.7 — Rewrite GetCatInfo

Replace `DoGetCatInfo` with `TrapGetCatInfo` using
`kCmdGetCatInfoResolved` ($0224).  This is the most complex handler
because of the 4 index/name modes and the file-vs-directory output
variants.

### 2.7.1 — TrapGetCatInfo

```c
static OSErr TrapGetCatInfo(char *pb, Globals *g, short isHFS)
{
    short vRefNum = *(short *)(pb + pb_ioVRefNum);
    long dirID = *(long *)(pb + pb_ioDirID);
    short index = *(short *)(pb + pb_ioFDirIndex);
    unsigned long nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);

    reg_set(g->regBase, 0, (unsigned long)(short)vRefNum);
    reg_set(g->regBase, 1, (unsigned long)dirID);
    reg_set(g->regBase, 2, (unsigned long)(long)index);
    reg_set(g->regBase, 3, nameAddr);
    reg_set(g->regBase, 4, (unsigned long)s_nameBuf);
    reg_command(g->regBase, kCmdGetCatInfoResolved);
    if (reg_result(g->regBase) != 0)
        return host_err(g->regBase);

    {
        unsigned long cnid    = reg_get(g->regBase, 0);
        unsigned long flags   = reg_get(g->regBase, 1);
        unsigned long dataSz  = reg_get(g->regBase, 2);
        unsigned long rsrcSz  = reg_get(g->regBase, 3);
        unsigned long parentID= reg_get(g->regBase, 4);
        unsigned long type    = reg_get(g->regBase, 5);
        unsigned long creator = reg_get(g->regBase, 6);
        unsigned long crDate  = reg_get(g->regBase, 7);
        unsigned long modDate = reg_get(g->regBase, 8);
        unsigned long fFlags  = reg_get(g->regBase, 9);

        /* Copy name to caller's buffer */
        if (nameAddr != 0)
            pstr_copy((char *)nameAddr, s_nameBuf);

        if (flags & 0x10) {
            /* Directory */
            *(unsigned char *)(pb + pb_ioFlAttrib) = 0x10;
            *(unsigned char *)(pb + 31) = 0;  /* ioACUser */

            /* Fetch DInfo + DXInfo from host */
            reg_set(g->regBase, 0, cnid);
            reg_set(g->regBase, 1, (unsigned long)s_dirInfoBuf);
            reg_command(g->regBase, kCmdGetDirInfo);
            if (reg_result(g->regBase) == 0) {
                mem_copy(pb + pb_ioDrUsrWds, s_dirInfoBuf, 16);
                mem_copy(pb + pb_ioDrFndrInfo, s_dirInfoBuf + 16, 16);
            } else {
                mem_zero(pb + pb_ioDrUsrWds, 16);
                mem_zero(pb + pb_ioDrFndrInfo, 16);
            }

            *(short *)(pb + pb_ioDrNmFls) = (short)dataSz;
            *(long  *)(pb + pb_ioDrDirID) = cnid;
            *(long  *)(pb + pb_ioDrParID) = parentID;
            *(long  *)(pb + pb_ioDrCrDat) = crDate;
            *(long  *)(pb + pb_ioDrMdDat) = modDate;
            *(long  *)(pb + pb_ioDrBkDat) = 0;
        } else {
            /* File */
            *(unsigned char *)(pb + pb_ioFlAttrib) = 0;
            *(unsigned long *)(pb + pb_ioFlFndrInfo)     = type;
            *(unsigned long *)(pb + pb_ioFlFndrInfo + 4) = creator;
            *(short *)(pb + pb_ioFlFndrInfo + 8)         = (short)fFlags;
            *(long  *)(pb + pb_ioFlFndrInfo + 10)        = 0;
            *(short *)(pb + pb_ioFlFndrInfo + 14)        = 0;
            *(long  *)(pb + pb_ioFlNum)    = cnid;
            *(short *)(pb + pb_ioFlStBlk)  = 0;
            *(long  *)(pb + pb_ioFlLgLen)  = dataSz;
            *(long  *)(pb + pb_ioFlPyLen)  = dataSz;
            *(short *)(pb + pb_ioFlRStBlk) = 0;
            *(long  *)(pb + pb_ioFlRLgLen) = rsrcSz;
            *(long  *)(pb + pb_ioFlRPyLen) = rsrcSz;
            *(long  *)(pb + pb_ioFlCrDat)  = crDate;
            *(long  *)(pb + pb_ioFlMdDat)  = modDate;
            *(long  *)(pb + pb_ioFlBkDat)  = 0;
            mem_zero(pb + pb_ioFlXFndrInfo, 16);
            *(long  *)(pb + pb_ioFlParID)  = parentID;
            *(long  *)(pb + pb_ioFlClpSiz) = 0;
        }
    }
    return kNoErr;
}
```

**Note:** This handler makes TWO host RPCs for directories
(GetCatInfoResolved + GetDirInfo) because DInfo/DXInfo is 32 bytes
that doesn't fit in the register block.  This is acceptable —
DirInfo is rare and the second RPC is cheap.  Future optimization:
fold DInfo into a wider register block or guest buffer.

### Fence

- [ ] Handler compiles and handles all 4 modes
- [ ] Commit: `"init.c: rewrite GetCatInfo with GetCatInfoResolved"`

---

## Phase 2.8 — Rewrite SetCatInfo

Uses `kCmdFileOpByName` with `kFileOpSetCatInfo`.

### 2.8.1 — TrapSetCatInfo

```c
static OSErr TrapSetCatInfo(char *pb, Globals *g, short isHFS)
{
    unsigned long nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);
    short vRefNum = *(short *)(pb + pb_ioVRefNum);
    long dirID = *(long *)(pb + pb_ioDirID);

    if (*(unsigned char *)(pb + pb_ioFlAttrib) & 0x10) {
        /* Directory: persist DInfo + DXInfo directly */
        unsigned long cnid = *(unsigned long *)(pb + pb_ioDrDirID);
        if (cnid != 0) {
            mem_copy(s_dirInfoBuf, pb + pb_ioDrUsrWds, 16);
            mem_copy(s_dirInfoBuf + 16, pb + pb_ioDrFndrInfo, 16);
            reg_set(g->regBase, 0, cnid);
            reg_set(g->regBase, 1, (unsigned long)s_dirInfoBuf);
            reg_command(g->regBase, kCmdSetDirInfo);
        }
        return kNoErr;
    }

    /* File: use FileOpByName with SetCatInfo opcode */
    if (nameAddr == 0) return kParamErr;

    reg_set(g->regBase, 0, (unsigned long)(short)vRefNum);
    reg_set(g->regBase, 1, (unsigned long)dirID);
    reg_set(g->regBase, 2, nameAddr);
    reg_set(g->regBase, 3, kFileOpSetCatInfo);
    reg_set(g->regBase, 4, *(unsigned long *)(pb + pb_ioFlFndrInfo));
    reg_set(g->regBase, 5, *(unsigned long *)(pb + pb_ioFlFndrInfo + 4));
    reg_set(g->regBase, 6, (unsigned long)(unsigned short)
        *(short *)(pb + pb_ioFlFndrInfo + 8));
    reg_command(g->regBase, kCmdFileOpByName);
    return host_err(g->regBase);
}
```

### Fence

- [ ] Handler compiles
- [ ] Commit: `"init.c: rewrite SetCatInfo"`

---

## Phase 2.9 — Rewrite Create / Delete / Rename

All three use `kCmdFileOpByName` with different opcodes.

### 2.9.1 — TrapCreate, TrapDelete, TrapRename

```c
static OSErr TrapCreate(char *pb, Globals *g, short isHFS)
{
    TrapLocation loc = ExtractLocation(pb, isHFS);
    if (loc.nameAddr == 0) return kParamErr;
    reg_set(g->regBase, 0, (unsigned long)loc.vRefNum);
    reg_set(g->regBase, 1, (unsigned long)loc.dirID);
    reg_set(g->regBase, 2, loc.nameAddr);
    reg_set(g->regBase, 3, kFileOpCreate);
    reg_command(g->regBase, kCmdFileOpByName);
    return host_err(g->regBase);
}

static OSErr TrapDelete(char *pb, Globals *g, short isHFS)
{
    TrapLocation loc = ExtractLocation(pb, isHFS);
    if (loc.nameAddr == 0) return kParamErr;
    reg_set(g->regBase, 0, (unsigned long)loc.vRefNum);
    reg_set(g->regBase, 1, (unsigned long)loc.dirID);
    reg_set(g->regBase, 2, loc.nameAddr);
    reg_set(g->regBase, 3, kFileOpDelete);
    reg_command(g->regBase, kCmdFileOpByName);
    return host_err(g->regBase);
}

static OSErr TrapRename(char *pb, Globals *g, short isHFS)
{
    TrapLocation loc = ExtractLocation(pb, isHFS);
    unsigned long newNameAddr = *(unsigned long *)(pb + pb_ioMisc);
    if (loc.nameAddr == 0 || newNameAddr == 0) return kParamErr;
    reg_set(g->regBase, 0, (unsigned long)loc.vRefNum);
    reg_set(g->regBase, 1, (unsigned long)loc.dirID);
    reg_set(g->regBase, 2, loc.nameAddr);
    reg_set(g->regBase, 3, kFileOpRename);
    reg_set(g->regBase, 4, newNameAddr);
    reg_command(g->regBase, kCmdFileOpByName);
    return host_err(g->regBase);
}
```

### Fence

- [ ] All three compile
- [ ] Commit: `"init.c: rewrite Create/Delete/Rename"`

---

## Phase 2.10 — Rewrite Volume / WD Handlers

These are mostly local (no new host commands) or use existing
commands.

### 2.10.1 — TrapGetVolInfo

Still calls `kCmdGetVol` ($0201) for file count / byte totals.
The `isHFS` flag controls whether to write HFS-extended fields at
offsets 64–121.  Uses `mem_zero` for the 32-byte ioVFndrInfo.
Uses named PB offset constants.

Port the existing `DoGetVolInfo` logic with:
- Named constants throughout
- `mem_zero` for the Finder info block
- Named `pb_ioFlBkDat` etc. instead of raw offsets

### 2.10.2 — TrapGetVol, TrapSetVol

Same logic as current, with named constants.  `TrapSetVol` must
still handle the name-match special case for "Shared".

### 2.10.3 — TrapUnmountVol, TrapEject

Same logic (dequeue DQE and VCB, set ejected flag).

### 2.10.4 — TrapFlushVol

No-op returning `kNoErr`.

### 2.10.5 — TrapOpenWD, TrapCloseWD, TrapGetWDInfo

Same logic as current `DoOpenWD`/`DoCloseWD`/`DoGetWDInfo` with
named constants.  Uses `pstr_copy` for volume name.

### Fence

- [ ] All 9 handlers compile
- [ ] Commit: `"init.c: rewrite volume and WD handlers"`

---

## Phase 2.11 — Rewrite Remaining HFS Handlers

### 2.11.1 — TrapGetVolParms

Same as current `DoGetVolParms` with named constants.

### 2.11.2 — TrapGetFCBInfo

Same as current logic (reads FCB fields, fills FCBPBRec) with
named constants and `pstr_copy_max` for the filename.

### 2.11.3 — TrapDirCreate

Uses `kCmdCreateDir` ($0215) directly:

```c
static OSErr TrapDirCreate(char *pb, Globals *g, short isHFS)
{
    long dirID = *(long *)(pb + pb_ioDirID);
    unsigned long nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);
    if (nameAddr == 0) return kParamErr;

    short vRefNum = *(short *)(pb + pb_ioVRefNum);
    reg_set(g->regBase, 0, (unsigned long)vRefNum);
    reg_set(g->regBase, 1, (unsigned long)dirID);
    reg_set(g->regBase, 2, nameAddr);
    reg_set(g->regBase, 3, kFileOpCreate);  /* reuse Create opcode?
        Actually for dirs we still use kCmdCreateDir directly */
    /* Better: use the direct command */
    reg_set(g->regBase, 0, (unsigned long)dirID);
    reg_set(g->regBase, 1, nameAddr);
    reg_command(g->regBase, kCmdCreateDir);
    if (reg_result(g->regBase) != 0)
        return host_err(g->regBase);
    *(long *)(pb + pb_ioDirID) = (long)reg_get(g->regBase, 0);
    return kNoErr;
}
```

### 2.11.4 — TrapCatMove

Uses `kCmdCatMove` ($0216):

```c
static OSErr TrapCatMove(char *pb, Globals *g, short isHFS)
{
    long srcDirID = *(long *)(pb + pb_ioDirID);
    long dstDirID = *(long *)(pb + 36);  /* ioNewDirID */
    unsigned long nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);
    if (nameAddr == 0) return kParamErr;

    reg_set(g->regBase, 0, (unsigned long)srcDirID);
    reg_set(g->regBase, 1, nameAddr);
    reg_set(g->regBase, 2, (unsigned long)dstDirID);
    reg_command(g->regBase, kCmdCatMove);
    return host_err(g->regBase);
}
```

### 2.11.5 — TrapSetVInfo

No-op returning `kNoErr`.

### Fence

- [ ] All handlers compile
- [ ] Commit: `"init.c: rewrite GetVolParms, GetFCBInfo, DirCreate, CatMove"`

---

## Phase 2.12 — Replace Dispatchers with Table + Loop

Delete the old `DispatchFlat` and `DispatchHFS` functions.  Replace
with `sFlatTraps[]`, `sHFSTraps[]`, `DispatchFromTable()`, and
new slim `DispatchFlat` / `DispatchHFS` entry points.

### 2.12.1 — Add dispatch infrastructure

Add the `TrapHandler` typedef, `TrapEntry` struct, both tables,
and `DispatchFromTable` as specified in SHAREDRIVE_DESIGN.md §4.6.

### 2.12.2 — Handle special cases

`TrapGetVolInfo` and `TrapSetVol` need pre-dispatch ownership
logic.  Two approaches:

**Option A (preferred):** The `refBased=0` ownership check in
`DispatchFromTable` uses `IsOurVolume`.  `TrapGetVolInfo` and
`TrapSetVol` handle their special cases internally — if the volume
isn't theirs, they return a sentinel error (e.g. `1`) that the
dispatch loop interprets as "pass through."  Add a `kPassThrough`
constant for this.

**Option B:** Handle these two traps before the table lookup in
`DispatchFlat`, as 2–3 line special cases.

### 2.12.3 — Delete old dispatchers

Remove the old `DispatchFlat` and `DispatchHFS` switch blocks
(~500 lines).

### Fence

- [ ] Both new dispatchers compile
- [ ] Old switch blocks deleted
- [ ] Commit: `"init.c: replace dispatch switches with table + loop"`

---

## Phase 2.13 — Delete Dead Code

Remove functions that are no longer called:

- `ResolveDir`
- `ResolveFlatDir`
- `DoGetCatInfo`
- `DoCreate`, `DoDelete`
- `DoOpen`, `DoOpenRF`
- `DoRead`, `DoWrite`, `DoClose`
- `DoGetFileInfo`, `DoSetFileInfo`
- `DoGetEOF`, `DoGetFPos`, `DoSetFPos`
- `DoGetVolInfo`, `DoGetVolParms`
- `DoOpenWD`, `DoCloseWD`, `DoGetWDInfo`

### Fence

- [ ] No dead code remains
- [ ] Build clean
- [ ] Commit: `"init.c: remove old handlers and ResolveDir"`

---

## Phase 2.14 — End-to-End Test

### 2.14.1 — Test with real guest OS

Boot emulator with:
1. A `shared/` directory containing subdirectories and files
2. The new INIT installed

Verify:
- [ ] Volume "Shared" appears on desktop
- [ ] Finder shows correct file names, sizes, type/creator icons
- [ ] Open files from root and subdirectories
- [ ] Open files from THINK C (the specific bug scenario:
      PBOpenWD → PBGetWDInfo → PBGetCatInfo → PBGetFileInfo →
      PBOpen for a file in a subdirectory)
- [ ] Copy files from Shared to a disk image
- [ ] Resource fork files open correctly
- [ ] Eject and remount works
- [ ] _Create, _Delete, _Rename on writable volumes
- [ ] No regressions in Finder navigation

### Fence

- [ ] All manual tests pass
- [ ] Commit: `"init.c: phase 2 complete — clean INIT rewrite"`
