# WD_PLAN — Root Working Directory Remediation

## Problem

The SharedDrive INIT uses `kOurVRefNum` (-32000) as a raw volume
reference number, but unlike real HFS volumes, -32000 is **not** a
Working Directory refnum and does not resolve through the host WD
table.  When applications (e.g. THINK C) obtain -32000 via
`GetWDInfo.ioWDVRefNum` and later pass it to `Open(vRefNum=-32000,
dirID=0)`, the host's `resolveDir()` maps `dirID=0` to the root
directory instead of the caller's intended subdirectory.

On real HFS, `GetWDInfo.ioWDVRefNum` returns a value like -1, which
**is** a WD refnum (the system allocates a root WD at boot for the
startup disk).  Apps passing -1 + dirID=0 resolve correctly through
the WD table.

Current workarounds (committed in b86be1a) use a `defaultDirID`
global and special-case fallbacks.  These are fragile: a single
global cannot track multiple active subdirectories simultaneously.

## Root Cause

We never allocate a WD for the root directory.  Apps can end up
holding the raw `-32000`, which resolves to root regardless of
context.

## Fix

Allocate a permanent root WD at mount time so that `-32000` (raw
volume ref) never leaks to applications.  Every vRefNum an app
receives will be a WD refnum that resolves through the host table.

## Files

- `macsrc/shareddrive/init.c` — guest INIT (all changes here)
- `src/storage/host_volume.cpp` — host WD table (no changes needed)
- `src/core/extn_extfs.cpp` — host dispatch (no changes needed)

## Phases

### Phase 1 — Allocate root WD at mount time

**Globals change:**

Replace `defaultDirID` (long) with `rootWDRefNum` (short).

```c
typedef struct {
    char    *regBase;
    Ptr      vcb;
    Ptr      dqe;
    long     volFileCount;
    long     volTotalBytes;
    long     savedA4;
    short    rootWDRefNum;     /* permanent WD for root dir, created at init */
    short    defaultWDRefNum;  /* WD refnum from last SetVol */
    short    ejected;
} Globals;
```

**Init code** (in `main()`, after host version check):

```c
/* Create permanent root WD — mirrors real HFS boot-volume root WD */
reg_set(regBase, 0, (unsigned long)kOurVRefNum);
reg_set(regBase, 1, (unsigned long)kRootDirID);
reg_command(regBase, kCmdOpenWD);
{
    unsigned long wdRef = reg_get(regBase, 0);
    g->rootWDRefNum = (short)(-(long)wdRef - 32000);
}
g->defaultWDRefNum = g->rootWDRefNum;
```

### Phase 2 — GetWDInfo returns rootWDRefNum

`TrapGetWDInfo` currently returns `kOurVRefNum` as `ioWDVRefNum`.
Change to return `g->rootWDRefNum`:

```c
*(short *)(pb + pb_ioWDVRefNum) = g->rootWDRefNum;
```

This is the critical fix.  Apps that call `GetWDInfo(myWD)` and
store `ioWDVRefNum` for later Open calls will now hold a WD refnum
that resolves through the host table to the root directory.

### Phase 3 — Simplify TrapSetVol

Remove the WD→dirID resolution logic.  `TrapSetVol` only needs to
track `defaultWDRefNum`:

```c
static OSErr TrapSetVol(char *pb, Globals *g, short isHFS)
{
    short vRefNum = *(short *)(pb + pb_ioVRefNum);
    unsigned long nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);

    if (!IsOurVolume(vRefNum)) {
        if (nameAddr != 0) {
            unsigned char *p = (unsigned char *)nameAddr;
            if (p[0]==6 && p[1]=='S' && p[2]=='h' && p[3]=='a'
                && p[4]=='r' && p[5]=='e' && p[6]=='d') {
                *(Ptr *)kDefVCBPtr = g->vcb;
                g->defaultWDRefNum = g->rootWDRefNum;
                return kNoErr;
            }
        }
        return 1; /* pass-through */
    }

    /* Store the caller's WD refnum (or rootWDRefNum for raw vol ref) */
    if (vRefNum < kOurVRefNum && vRefNum > -32100)
        g->defaultWDRefNum = vRefNum;
    else
        g->defaultWDRefNum = g->rootWDRefNum;

    *(Ptr *)kDefVCBPtr = g->vcb;
    return kNoErr;
}
```

No WD→dirID resolution.  No `defaultDirID` read/write.

### Phase 4 — Simplify ExtractLocation

Remove the `defaultDirID` fallback.  The only special case
remaining is `vRefNum==0` when our VCB is default — substitute
`defaultWDRefNum`:

```c
static TrapLocation ExtractLocation(char *pb, short isHFS, Globals *g)
{
    TrapLocation loc;
    loc.vRefNum  = *(short *)(pb + pb_ioVRefNum);
    loc.nameAddr = *(unsigned long *)(pb + pb_ioNamePtr);
    if (isHFS) {
        loc.dirID = *(long *)(pb + 48);
    } else {
        loc.dirID = 0;
    }
    return loc;
}
```

No `defaultDirID` fallback.  The host `resolveDir()` handles WD
refnums directly — when `dirID==0` and `vRefNum` is a WD refnum,
it decodes through the WD table automatically.

### Phase 5 — Simplify TrapGetVol

```c
static OSErr TrapGetVol(char *pb, Globals *g, short isHFS)
{
    unsigned long nmAddr = *(unsigned long *)(pb + pb_ioNamePtr);
    if (nmAddr != 0) {
        unsigned char *p = (unsigned char *)nmAddr;
        p[0]=6; p[1]='S'; p[2]='h'; p[3]='a';
        p[4]='r'; p[5]='e'; p[6]='d';
    }
    *(short *)(pb + pb_ioVRefNum) = g->defaultWDRefNum;
    if (isHFS) {
        *(short *)(pb + 32) = g->rootWDRefNum;  /* ioWDVRefNum */
        *(long  *)(pb + 28) = 0;                /* ioWDProcID */
        /* ioWDDirID: resolve defaultWDRefNum through host WD table */
        /* This could be left as 0 — most apps use the WD refnum directly */
    }
    return kNoErr;
}
```

### Phase 6 — Simplify TrapGetVolInfo

Return `defaultWDRefNum` in `ioVRefNum` (already done).  No other
changes needed — this was already fixed.

### Phase 7 — Remove defaultDirID

Remove `defaultDirID` from Globals.  Remove all reads/writes of
it.  grep for `defaultDirID` to confirm zero remaining references.

### Phase 8 — Clean up IsOurVolume

`IsOurVolume` still checks `kOurVRefNum`.  This must remain because
internal code (TrapGetCatInfo, host commands) still uses -32000 for
the volume identity.  The constant stays — it's just never exposed
to applications.

## Invariant

After this change: **every vRefNum returned to applications in any
PB output field is either a WD refnum (-32001..) or 0.  The raw
volume ref -32000 is internal-only.**

## Build/Test

Guest INIT only — recompile with THINK C inside the emulator.
No host-side changes needed.

Test:
1. Boot, verify SharedDrive mounts in Finder
2. Open THINK C project in subdirectory of shared drive
3. Compile — source files must open successfully
4. Open a second project in a different subdirectory
5. Switch between projects — both must compile
