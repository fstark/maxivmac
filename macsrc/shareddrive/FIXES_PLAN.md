# SharedDrive INIT — Fixes Plan

**Status: COMPLETE** — 24 April 2026

Findings: [REVIEW_FINDINGS.md](REVIEW_FINDINGS.md)
Source: [init.c](init.c)

All 7 phases completed successfully.  Commit range: `bd3f718..bb838aa`

| Phase | Description                                      | Status    |
|-------|--------------------------------------------------|-----------|
| 1     | Add missing error constants                      | Completed |
| 2     | Fix rfNumErr for bad reference numbers           | Completed |
| 3     | Fix position-mode error returns (Read/SetFPos)   | Completed |
| 4     | Fix negative ioReqCount handling                 | Completed |
| 5     | Open permission & access-path conflict checks    | Completed |
| 6     | VCB field accuracy                               | Completed |
| 7     | WD process tracking                              | Completed |

Build gate: `cmake --preset macos && cmake --build --preset macos`
Test gate: Boot emulator with SharedDrive, exercise file ops from
Finder and THINK C IDE.

---

## Deferred / Won't Fix

These findings are intentionally not addressed.  Each has a rationale.

| Finding | Severity | Rationale |
|---------|----------|-----------|
| FreeFCB leaves stale data (P3 #1) | MINOR | All critical fields overwritten by AllocFCB. No regression risk. |
| vcbFilCnt/vcbDirCnt in VCB zero (P4 #2) | MINOR | Most tools use PBHGetVInfo which returns correct values. Direct VCB access is rare. |
| fdLocation zeroed (P10 #1) | MINOR | Cosmetic — Finder auto-arranges at (0,0). Acceptable for virtual volume. |
| fdLocation/fdFldr not persisted on Set (P10 #2) | MINOR | Would require host-side protocol extension. Out of scope for bug-fix pass. |
| Newline mode not implemented (P7 #5) | MINOR | Rare in File Manager usage. Significant effort for low payoff. |
| WD limit of 99 (P9 #1) | MINOR | Sufficient for practical usage. Deep hierarchies are unrealistic. |
| DQE failure degrades silently (P11 #6) | MINOR | Fallback is acceptable — volume still accessible by name. |
| Partial trap patching (P11 #7) | MINOR | Already logged. Acceptable degradation. |
| Reentrancy of static buffers (P11 #9) | MINOR | Not a real issue — emulator serializes all 68k execution. |
| Missing _Offline trap (P11 #10) | MINOR | Low risk. System rarely offlines virtual volumes. |
| CloseWD does not validate caller (P11 #5) | MINOR | Harmless in single-user emulator. |
| All STYLE findings | STYLE | No behavioral impact. |

---

## Phase 1 — Add missing error constants

Add the error codes needed by subsequent phases.  Pure additions,
no behavior change.

### 1.1 — New `#define` constants

Add after the existing error constants block (after `kNsvErr`):

```c
#define kPosErr    (-40)
#define kOpWrErr   (-49)
#define kRfNumErr  (-51)
#define kWrPermErr (-61)
```

`kEofErr (-39)` already exists.  `kParamErr (-50)` already exists.

### Fence

- [ ] Four new `#define`s added to init.c
- [ ] No other code changed
- [ ] Commit: `"shareddrive: add missing Mac OS error constants"`

---

## Phase 2 — Fix rfNumErr for bad reference numbers

**Fixes:** Phase 8, finding #1 (MINOR) — seven handlers return
fnfErr (-43) instead of rfNumErr (-51) for invalid reference numbers.

### 2.1 — Replace kFnfErr with kRfNumErr in refnum checks

In each of these seven handlers, change the `GetFCB() == NULL` return
from `kFnfErr` to `kRfNumErr`:

| Handler | Approximate line |
|---------|-----------------|
| TrapClose | `if (fcb == NULL) return kFnfErr;` |
| TrapRead | `if (fcb == NULL) return kFnfErr;` |
| TrapWrite | `if (fcb == NULL) return kFnfErr;` |
| TrapGetEOF | `if (fcb == NULL) return kFnfErr;` |
| TrapSetEOF | `if (fcb == NULL) return kFnfErr;` |
| TrapGetFPos | `if (fcb == NULL) return kFnfErr;` |
| TrapSetFPos | `if (fcb == NULL) return kFnfErr;` |

Each line: `return kFnfErr;` → `return kRfNumErr;`

TrapGetFCBInfo already uses -51 directly.  No change needed there.

### Fence

- [ ] All seven handlers return `kRfNumErr` for NULL FCB
- [ ] TrapGetFCBInfo unchanged (already correct)
- [ ] Commit: `"shareddrive: return rfNumErr for invalid refnums"`

---

## Phase 3 — Fix position-mode error returns

**Fixes:**
- Phase 7, finding #1 (MAJOR) — TrapRead does not return eofErr
- Phase 7, finding #2 (MAJOR) — TrapRead does not return posErr
- Phase 7, finding #3 (MAJOR) — TrapSetFPos does not return eofErr/posErr
- Phase 7, finding #4 (MINOR) — TrapWrite does not return posErr

The position-mode switch is shared across TrapRead, TrapWrite, and
TrapSetFPos.  All three need the same structural fix: compute the
mark, then check for out-of-range *before* clamping, and return the
appropriate error after performing the operation.

### 3.1 — TrapRead: return posErr and eofErr

Replace the clamping logic:

```c
/* BEFORE */
if (mark < 0) mark = 0;
if (mark > eof) mark = eof;

/* AFTER */
if (mark < 0) {
    *(long *)(pb + pb_ioActCount)  = 0;
    *(long *)(pb + pb_ioPosOffset) = 0;
    *(long *)(fcb + kFCBCrPs)     = 0;
    return kPosErr;
}
```

Then, after the host read completes, return eofErr when the read
was truncated by EOF.  The existing comparison
`(actual < reqCount) ? kEofErr : kNoErr` is almost right, but it
must also trigger when the *original* request (before truncation)
extended past EOF.

Change the eofErr logic:

```c
/* BEFORE */
if (mark + reqCount > eof)
    reqCount = eof - mark;
/* ...host read... */
return (actual < (unsigned long)reqCount) ? kEofErr : kNoErr;

/* AFTER — track whether we truncated */
{
    short hitEof = 0;
    if (mark + reqCount > eof) {
        reqCount = eof - mark;
        hitEof = 1;
    }
    /* ...host read... */
    if (actual < (unsigned long)reqCount)
        return kEofErr;       /* host short read */
    if (hitEof)
        return kEofErr;       /* original request exceeded EOF */
    return kNoErr;
}
```

Edge case: `mark == eof && reqCount > 0` → truncated reqCount = 0,
hitEof = 1.  Host returns actual = 0.  Result: eofErr.  Correct.

### 3.2 — TrapSetFPos: return posErr and eofErr

Replace:

```c
/* BEFORE */
if (mark < 0) mark = 0;
if (mark > eof) mark = eof;
*(long *)(fcb + kFCBCrPs) = mark;
*(long *)(pb + pb_ioPosOffset) = mark;
return kNoErr;

/* AFTER */
if (mark < 0) {
    *(long *)(fcb + kFCBCrPs)     = 0;
    *(long *)(pb + pb_ioPosOffset) = 0;
    return kPosErr;
}
if (mark > eof) {
    *(long *)(fcb + kFCBCrPs)     = eof;
    *(long *)(pb + pb_ioPosOffset) = eof;
    return kEofErr;
}
*(long *)(fcb + kFCBCrPs) = mark;
*(long *)(pb + pb_ioPosOffset) = mark;
return kNoErr;
```

Per IM: "If you try to set the mark past the logical end-of-file,
PBSetFPos moves the mark to the end-of-file and returns eofErr."
And: "posErr — Attempt to position before start of file."  The mark
is clamped to the boundary and the error is returned.

### 3.3 — TrapWrite: return posErr for negative mark

Replace:

```c
/* BEFORE */
if (mark < 0) mark = 0;

/* AFTER */
if (mark < 0) {
    *(long *)(pb + pb_ioActCount)  = 0;
    *(long *)(pb + pb_ioPosOffset) = 0;
    return kPosErr;
}
```

TrapWrite does NOT clamp mark to eof (writes can extend) — that
remains unchanged.

### Fence

- [ ] TrapRead returns `kPosErr` for negative mark
- [ ] TrapRead returns `kEofErr` when original request extends past EOF
- [ ] TrapSetFPos returns `kPosErr` for negative mark
- [ ] TrapSetFPos returns `kEofErr` when mark set past EOF
- [ ] TrapWrite returns `kPosErr` for negative mark
- [ ] Manual test: open a text file, read to EOF → app detects eofErr
- [ ] Commit: `"shareddrive: return posErr/eofErr per Inside Macintosh"`

---

## Phase 4 — Fix negative ioReqCount handling

**Fixes:** Phase 8, finding #2 (MINOR) — negative ioReqCount returns
noErr instead of paramErr.

### 4.1 — TrapRead: check for negative reqCount

Replace:

```c
/* BEFORE */
if (reqCount <= 0) {
    *(long *)(pb + pb_ioActCount)  = 0;
    *(long *)(pb + pb_ioPosOffset) = mark;
    *(long *)(fcb + kFCBCrPs)     = mark;
    return kNoErr;
}

/* AFTER */
if (reqCount < 0) {
    *(long *)(pb + pb_ioActCount)  = 0;
    *(long *)(pb + pb_ioPosOffset) = mark;
    *(long *)(fcb + kFCBCrPs)     = mark;
    return kParamErr;
}
if (reqCount == 0) {
    *(long *)(pb + pb_ioActCount)  = 0;
    *(long *)(pb + pb_ioPosOffset) = mark;
    *(long *)(fcb + kFCBCrPs)     = mark;
    return kNoErr;
}
```

### 4.2 — TrapWrite: same check

Apply the identical split to TrapWrite's `reqCount <= 0` guard.

### Fence

- [ ] TrapRead returns `kParamErr` for negative ioReqCount
- [ ] TrapWrite returns `kParamErr` for negative ioReqCount
- [ ] Zero ioReqCount still returns `kNoErr` (unchanged)
- [ ] Commit: `"shareddrive: return paramErr for negative ioReqCount"`

---

## Phase 5 — Open permission & access-path conflict checks

**Fixes:**
- Phase 11, finding #1 (MAJOR) — no ioPermssn check in TrapOpen/TrapOpenRF
- Phase 11, finding #2 (MAJOR) — no opWrErr for conflicting access paths

This is the most complex phase.  It requires:
1. Reading ioPermssn from the parameter block
2. Mapping permission to appropriate FCB flags
3. Scanning existing FCBs for conflicts before allocating

### 5.1 — Add ioPermssn offset constant

```c
#define pb_ioPermssn  27  /* SignedByte */
```

IM IV: ioPermssn is at offset 27 in ioParam, 1 byte (SignedByte).

Permission values:

```c
#define kFsCurPerm   0   /* whatever is allowed */
#define kFsRdPerm    1   /* read only */
#define kFsWrPerm    2   /* write only */
#define kFsRdWrPerm  3   /* exclusive read/write */
```

### 5.2 — Add conflict-check helper

Add a helper function that scans all FCBs for an existing open of the
same file (by CNID) and determines whether the requested permission
conflicts:

```c
/*
 * CheckOpenConflict — scan FCBs for conflicting open of same file.
 *
 * Returns kNoErr if no conflict, kOpWrErr if the new open would
 * conflict with an existing access path.
 *
 * Rules (IM IV):
 * - If any existing path has write permission, a new write or
 *   exclusive open returns opWrErr.
 * - If new open requests exclusive (fsRdWrPerm), any existing path
 *   is a conflict.
 */
static OSErr CheckOpenConflict(unsigned long cnid,
    unsigned char requestedPerm)
{
    Ptr fcbBuf = *(Ptr *)kFCBSPtr;
    short fcbLen, i;
    if (fcbBuf == NULL) return kNoErr;
    fcbLen = *(short *)fcbBuf;
    for (i = 2; i < fcbLen; i += kFCBLen) {
        Ptr fcb = fcbBuf + i;
        if (*(unsigned long *)(fcb + kFCBFlNum) != cnid)
            continue;
        /* An access path already exists for this file. */
        {
            unsigned char existingFlags = *(unsigned char *)(fcb + kFCBFlags);
            short existingWrite = existingFlags & 0x01;

            /* New exclusive open conflicts with any existing path */
            if (requestedPerm == 3)
                return kOpWrErr;

            /* New write open conflicts with existing write path */
            if ((requestedPerm == 0 || requestedPerm == 2) &&
                existingWrite)
                return kOpWrErr;
        }
    }
    return kNoErr;
}
```

### 5.3 — Modify TrapOpen

After resolving the file (successful kCmdResolveAndOpen) and before
AllocFCB, read ioPermssn and check for conflicts:

```c
{
    unsigned long handle = reg_get(g->regBase, 0);
    long size            = (long)reg_get(g->regBase, 1);
    unsigned long cnid   = reg_get(g->regBase, 2);
    unsigned char perm   = *(unsigned char *)(pb + pb_ioPermssn);
    unsigned char flags;
    OSErr conflict;

    /* Map permission to FCB flags */
    if (perm == 1)          /* fsRdPerm — read only */
        flags = 0x00;
    else                    /* fsCurPerm, fsWrPerm, fsRdWrPerm */
        flags = 0x01;       /* fcbWriteMask */

    /* Check for conflicting access paths */
    conflict = CheckOpenConflict(cnid, perm);
    if (conflict != kNoErr) {
        reg_set(g->regBase, 0, handle);
        reg_command(g->regBase, kCmdClose);
        return conflict;
    }

    {
        short refNum = AllocFCB(g->vcb, cnid, size, flags);
        /* ... rest unchanged ... */
    }
}
```

### 5.4 — Modify TrapOpenRF

Same pattern as TrapOpen, but the resource-fork bit (0x02) is OR'd
into flags:

```c
    if (perm == 1)
        flags = 0x02;       /* resource fork, read only */
    else
        flags = 0x03;       /* resource fork + write */
```

### 5.5 — TrapWrite: check write permission

Add a write-permission check at the top of TrapWrite, after the FCB
lookup:

```c
if ((*(unsigned char *)(fcb + kFCBFlags) & 0x01) == 0)
    return kWrPermErr;
```

### Fence

- [ ] `pb_ioPermssn` and permission constants defined
- [ ] `CheckOpenConflict()` function added
- [ ] TrapOpen reads ioPermssn and maps to FCB flags
- [ ] TrapOpenRF same
- [ ] TrapWrite returns `kWrPermErr` for read-only FCBs
- [ ] Multiple opens with fsRdPerm succeed (no conflict)
- [ ] Second open with fsRdWrPerm when file already open → opWrErr
- [ ] Manual test: open file read-only from THINK C, verify no write
- [ ] Commit: `"shareddrive: enforce ioPermssn and detect open conflicts"`

---

## Phase 6 — VCB field accuracy

**Fixes:**
- Phase 4, finding #1 (MINOR) — vcbNxtCNID never updated
- Phase 10, finding #3 (MINOR) — ioVDirCnt hardcoded to 1

### 6.1 — Maintain vcbNxtCNID

In TrapOpen and TrapOpenRF, after successful open, update the VCB's
vcbNxtCNID if the file's CNID is >= the current value:

```c
{
    long nextCNID = *(long *)(g->vcb + kVcbNxtCNID);
    if ((long)cnid >= nextCNID)
        *(long *)(g->vcb + kVcbNxtCNID) = (long)(cnid + 1);
}
```

Add the VCB offset constant:

```c
#define kVcbNxtCNID  38
```

Also update vcbNxtCNID in TrapCreate (after file creation succeeds,
the host assigns a new CNID).  TrapCreate doesn't currently return the
new CNID, so update via a simple `nextCNID++` on the VCB — imperfect
but better than stale.

### 6.2 — Fix ioVDirCnt in TrapGetVolInfo

TrapGetVolInfo currently writes `1` for ioVDirCnt.  The host already
provides directory count information (or it can be hardcoded to match
vcbNmFls as a ratio).  The simplest correct fix: query the host for
the directory count, or use a Globals field populated at mount time.

If the host doesn't provide a separate dir count, change to a
reasonable estimate:

```c
/* TrapGetVolInfo: replace hardcoded 1 */
/* BEFORE: *(long *)(pb + 86) = 1; */
*(long *)(pb + 86) = g->volDirCount;
```

Add `volDirCount` to the Globals struct (computed at init from the
host's directory enumeration).  If the host RPC doesn't supply this,
keep the hardcoded value but use the actual count from the host's
initial volume info (available via kCmdGetVol).

**Fallback:** If adding volDirCount to Globals is disproportionate
effort, accept the hardcoded 1 and move this to "deferred."

### Fence

- [ ] vcbNxtCNID updated on file open and create
- [ ] `kVcbNxtCNID` constant defined
- [ ] ioVDirCnt improved (or explicitly deferred with rationale)
- [ ] Commit: `"shareddrive: keep vcbNxtCNID current"`

---

## Phase 7 — WD process tracking

**Fixes:**
- Phase 11, finding #3 (MINOR) — TrapOpenWD does not record ioWDProcID
- Phase 11, finding #4 (MINOR) — TrapGetWDInfo does not support
  indexed enumeration

### 7.1 — Store ioWDProcID in working directory table

The WD table is host-side — WD open/close/query go through RPC
commands kCmdOpenWD, kCmdCloseWD, kCmdGetWDInfo.  Storing the
process ID requires passing it to the host.

Read ioWDProcID from the parameter block in TrapOpenWD:

```c
long procID = *(long *)(pb + pb_ioWDProcID);
reg_set(g->regBase, 2, (unsigned long)procID);
```

The host side must be modified to store and return this value.  If
the host RPC already has a spare register slot for the OpenWD command,
use it.  Otherwise, add one.

### 7.2 — Support indexed enumeration in TrapGetWDInfo

Currently returns kNsvErr for `ioWDIndex != 0`.  The host-side
kCmdGetWDInfo handler needs to support an index parameter and return
successive WD entries.

If indexed enumeration requires significant host-side changes, defer
this sub-task and document the limitation.

### Fence

- [ ] TrapOpenWD passes ioWDProcID to host
- [ ] Host stores procID per WD (or sub-task deferred)
- [ ] TrapGetWDInfo indexed enumeration (or sub-task deferred)
- [ ] Commit: `"shareddrive: pass ioWDProcID to host on OpenWD"`
