# maxivmac Guest INIT — Unified Design

## Name

**maxivmac INIT** (resource type `INIT`, ID 128)

Previously two separate INITs:
- `SharedDrive INIT` (ID 315) — File Manager trap patching for host-shared volumes
- `ClipSync INIT` (ID 314) — jGNEFilter-based clipboard synchronisation

## Overview

A single INIT resource that provides all host-integration services:
shared drive mounting, clipboard sync, and guest-command polling.
One jGNEFilter handles all periodic tasks (clipboard sync + mount polling + guest commands).

The backend register interface is **unchanged** — the two command ranges
(`$1xx` for clipboard, `$2xx` for shared drive / infrastructure) remain
as-is so the emulator C++ side needs no modification.

## Directory

`macsrc/init/`

## Source Files

| File | Responsibility |
|------|---------------|
| `comm.c` | Extension discovery (`find_reg_base`), register access helpers (`reg_set`/`reg_get`/`reg_command`/`reg_result`), debug logging (`dbg_log`), fatal shutdown, globals pointer get/set via host KV |
| `clip.c` | Clipboard sync logic: `ExportMacToHost`, `ImportHostToMac`, `SyncClipboard` (the per-tick check) |
| `drive.c` | SharedDrive trap handlers, FCB management, VCB/DQE allocation, dispatch tables, `MountNewDrive` |
| `init.c` | INIT entry (`main`), jGNEFilter entry (`FilterEntry`), trap stub generation & installation, boot-time initialisation sequence |

## Shared Header

A single `defs.h` with:
- All `#define` constants (low-memory addresses, PB offsets, FCB offsets, error codes, command codes)
- `Globals` struct definition (merged from both INITs)
- Function prototypes for cross-file calls

## Merged Globals

```c
typedef struct {
    /* comm */
    char   *regBase;        /* extension register base pointer */
    long    savedA4;        /* THINK C A4 for code resource relocation */

    /* drive */
    Ptr     vcb[kMaxDrives];
    Ptr     dqe[kMaxDrives];
    short   driveCount;
    long    volFileCount;
    long    volTotalBytes;
    short   ejected;

    /* clip */
    long    lastClipTicks;  /* throttle clipboard checks */

    /* filter */
    long    oldFilter;      /* previous jGNEFilter to chain */
    long    lastPollTick;   /* throttle mount polling */
} Globals;
```

Globals are stored in a system-heap `NewPtrSys` block. The pointer is
registered with the host via `ExtFSGuestVars` ($020E) so it survives
across application context switches without needing A5 or low-memory
scratch slots (the clipsync `$0B00` trick is retired).

## jGNEFilter (unified)

One `FilterEntry` in `init.c`:

1. Save registers, `SetUpA4()`
2. Retrieve `Globals *g` via `get_globals()`
3. **Clipboard sync** — call `SyncClipboard(g)` (throttled to every 30 ticks)
4. **Mount polling** — call `PollMounts(g)` (throttled to every 60 ticks)
5. **Guest commands** — call `PollGuestCmd(g)` (same cadence as mount polling)
6. `RestoreA4()`, restore registers
7. Chain to `g->oldFilter`

## Backend Command Ranges (unchanged)

### Clipboard ($1xx) — handled by `extn_clip.cpp`

| Code | Name | Direction |
|------|------|-----------|
| $100 | ClipVersion | → p0=version |
| $101 | ClipExport | p0=buf, p1=len |
| $102 | ClipImport | p0=buf, p1=cap → p1=actual |
| $103 | ClipHasData | → p0=0/1 |
| $104 | ClipGetLen | → p0=len |
| $105 | ClipSeqNo | → p0=seqno |
| $106 | ClipKVSet | p0=key, p1=val |
| $107 | ClipKVGet | p0=key → p0=val |
| $108 | ClipDbgLog | p0=fmt, p1–p6=args |

### Shared Drive ($2xx) — handled by `extn_extfs.cpp`

| Code | Name | Direction |
|------|------|-----------|
| $200 | ExtFSVersion | → p0=version |
| $201 | ExtFSGetVol | → p0=files, p1=bytes, p2=dirs |
| $205 | ExtFSRead | p0=handle, p1=off, p2=cnt, p3=buf |
| $206 | ExtFSClose | p0=handle |
| $20A | ExtFSGetWDInfo | p0=wdRef → p0=proc, p1=dir |
| $20B | ExtFSOpenWD | p0=vRef, p1=dir, p2=proc → p0=wdRef |
| $20D | ExtFSDbgLog | p0=fmt, p1–p6=args |
| $20E | ExtFSGuestVars | p0=ptr, p1=0(get)/1(set) |
| $20F | ExtFSLogTrap | p0=trap, p1=pb, p2=action, p3=err, p4=flags |
| $211 | ExtFSWrite | p0=handle, p1=off, p2=cnt, p3=buf |
| $214 | ExtFSFatal | p0=fmt, p1–p6=args |
| $218 | ExtFSSetEOF | p0=handle, p1=newSize |
| $219 | ExtFSPollMount | → p0=slot (or $FFFFFFFF), p1=vRef, p2=drv |
| $21A | ExtFSGetVolName | p0=slot, p1=nameBuf |
| $220 | ExtFSGuestCmd | p0=pathBuf → result=cmd |
| $230–$247 | PB_* | p0=PB addr, p1=isHFS |

## Debug Logging

Both subsystems log through the host but use different command codes
($108 for clip, $20D for drive). The unified `dbg_log` in `comm.c`
takes one extra parameter — the subsystem prefix — and routes to the
appropriate backend command:

```c
void dbg_log(Globals *g, short subsys, char *fmt, ...);
/* subsys: 0=drive (uses $20D), 1=clip (uses $108) */
```

This keeps backend compatibility. A future cleanup can unify to a single
log command.

## Trap Patching (in `init.c`)

Unchanged from current shareddrive. Dynamic 68k stubs in system heap,
patching `_HFSDispatch` and 21 flat-file traps. Clipboard does not
need trap patches (it uses the Scrap Manager via normal toolbox calls).

## Boot Sequence (`main` in `init.c`)

```
1. RememberA0 / SetUpA4
2. find_reg_base() — bail if NULL
3. Check ExtFSVersion ($200) — bail if 0 (no shared dir)
4. Check ClipVersion ($100) — note if < 2 (clip disabled, drive-only mode)
5. Allocate Globals in system heap
6. set_globals() via $20E
7. DetachResource / HLock the INIT handle (ID 128)
8. Drain PollMount queue → MountNewDrive for each
9. InitTrapTables + install trap patches (if driveCount > 0)
10. Install jGNEFilter (always — needed for clip even without drives)
11. Log "maxivmac INIT: done"
```

If the host has no shared directory AND clipboard version < 2, the INIT
bails early (nothing to do).

## THINK C Project Layout

Single code resource project (`maxivmac.π`):
- Resource type: `INIT`, ID: 128
- Segment: one segment (all four .c files linked together)
- `defs.h` included by all .c files

## Migration Notes

- The old `SharedDrive INIT` (ID 315) and `ClipSync INIT` (ID 314)
  are retired. Only the unified INIT (ID 128) is installed.
- The `$0B00` low-memory scratch trick from clipsync is removed;
  globals live in host-backed storage via `$020E`.
- jGNEFilter chaining: only one filter installed (instead of two
  separate ones that previously chained through each other depending
  on install order).
- Guest command polling (launch/quit/shutdown) moves from drive-only
  filter into the unified filter — works even without a shared drive.
