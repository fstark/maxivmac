# INIT_PLAN.md — Implementation Plan

Phased plan to create `macsrc/init/` from the existing `shareddrive/init.c`
and `clipsync/init.c`. Each phase produces a complete, compilable set of
files. No backend (C++ emulator) changes required.

---

## Phase 1 — Scaffold and `defs.h`

Create `macsrc/init/` directory with `defs.h` containing all shared
definitions extracted from both INITs.

**Contents of `defs.h`:**

1. Include guards / standard Toolbox headers (`<SetUpA4.h>`, `<Memory.h>`,
   `<OSUtils.h>`, `<Files.h>`, `<Devices.h>`, `<Events.h>`, `<Scrap.h>`,
   `<SegLoad.h>`, `<Shutdown.h>`)
2. Low-memory global addresses (`kSonyVarsPtr`, `kCheckVal`, `kVCBQHdr`,
   `kDrvQHdr`, `kFCBSPtr`, `kDefVCBPtr`, `kJGNEFilter`, `kCurApRefNum`,
   `kScrapCount`)
3. Volume/drive constants (`kBaseVRefNum`, `kBaseDriveNum`, `kMaxDrives`,
   `kOurVRefNum`, `kOurDriveNum`, `kOurDrvrRefNum`, `kAllocBlkSize`,
   `kTotalAllocBlks`, `kRootDirID`)
4. ParamBlock field offsets (all `pb_*` defines)
5. FCB field offsets (all `kFCB*` defines)
6. VCB field offset (`kVcbNxtCNID`)
7. HFS selectors and Mac OS error codes
8. Host command codes — drive range (`kCmdClose`, `kCmdRead`, `kCmdWrite`,
   `kCmdSetEOF`, `kCmdGetVol`, `kPB_*`, `kExtFSPollMount`,
   `kExtFSGetVolName`, `kExtFSGuestCmd`, `kNotOurs`, `kPassThrough`)
9. Host command codes — clip range (`kClipVersion 0x0100`,
   `kClipExport 0x0101`, `kClipImport 0x0102`, `kClipHasData 0x0103`,
   `kClipGetLen 0x0104`, `kClipSeqNo 0x0105`, `kClipKVSet 0x0106`,
   `kClipKVGet 0x0107`, `kClipDbgLog 0x0108`)
10. Permission constants (`kFsCurPerm`, etc.)
11. Trap-log flags (`LOG_PASSTHRU`, `LOG_HANDLED`, `LOG_ERROR`, `LOG_F_*`)
12. `MyDriverDat_R` struct (extension discovery)
13. Merged `Globals` typedef
14. Function prototypes (cross-file calls — see below)

**Prototypes in `defs.h`:**

```c
/* comm.c */
char       *find_reg_base(void);
void        reg_set(char *base, int n, unsigned long v);
unsigned long reg_get(char *base, int n);
void        reg_command(char *base, unsigned short cmd);
unsigned short reg_result(char *base);
Globals    *get_globals(void);
void        set_globals(char *base, Globals *g);
void        dbg_log6(char *base, char *fmt, unsigned long a,
                     unsigned long b, unsigned long c, unsigned long d,
                     unsigned long e, unsigned long f);
void        dbg_fatal6(char *base, char *fmt, unsigned long a,
                       unsigned long b, unsigned long c, unsigned long d,
                       unsigned long e, unsigned long f);
void        kv_set(char *regBase, unsigned long key, unsigned long val);
unsigned long kv_get(char *regBase, unsigned long key);

/* clip.c */
void        SyncClipboard(Globals *g);

/* drive.c */
short       DispatchFlat(char *pb, short trapWord);
short       DispatchHFS(char *pb, short selector);
void        InitTrapTables(void);
void        MountNewDrive(Globals *g, short slot, short vRefNum, short driveNum);

/* init.c */
void        FilterEntry(void);
```

**Deliverable:** `macsrc/init/defs.h` — compiles as a header (no code).

---

## Phase 2 — `comm.c`

Extract from both INITs into `macsrc/init/comm.c`:

1. `#include "defs.h"`
2. `find_reg_base()` — verbatim from shareddrive (identical to clipsync)
3. `reg_set`, `reg_get`, `reg_command`, `reg_result` — verbatim
4. `get_globals` / `set_globals` — from shareddrive (uses $020E)
5. `kv_set` / `kv_get` — from clipsync (uses $0106/$0107)
6. `dbg_log6` — takes the command code as parameter instead of
   hardcoding it. Provide two wrappers via macros in `defs.h`:
   - `dbg_log_drive(base, fmt, ...)` → calls with $020D
   - `dbg_log_clip(base, fmt, ...)` → calls with $0108
7. `dbg_fatal6` — from shareddrive ($0214)
8. Utility functions: `pstr_copy`, `pstr_copy_max`, `mem_zero`,
   `mem_copy`, `pstr_equal`, `host_err`, `host_err_passthrough`

**Deliverable:** `macsrc/init/comm.c` — all low-level host communication.

---

## Phase 3 — `clip.c`

Extract from `clipsync/init.c` into `macsrc/init/clip.c`:

1. `#include "defs.h"`
2. `ExportMacToHost(Globals *g)` — adapted to use `g->regBase` directly
   (was using a local `regBase`). Uses `kClipExport` ($0101).
3. `ImportHostToMac(Globals *g)` — same adaptation. Uses `kClipGetLen`
   ($0104) + `kClipImport` ($0102).
4. `SyncClipboard(Globals *g)` — the periodic check, merged from
   clipsync's `SyncOnGNE()`:
   - Throttle via `g->lastClipTicks` (30 ticks)
   - Host→Mac check via `kClipSeqNo` + `kv_get`
   - Mac→Host check via `kScrapCount` + `kv_set`
   - Uses `dbg_log_clip` for logging

**Changes from original:**
- No `FilterGlobals` struct — uses unified `Globals`
- No `$0B00` pointer — `g` passed as argument
- `appId` still read from `kCurApRefNum` low-memory

**Deliverable:** `macsrc/init/clip.c`

---

## Phase 4 — `drive.c`

Extract from `shareddrive/init.c` into `macsrc/init/drive.c`:

1. `#include "defs.h"`
2. FCB management: `AllocFCB`, `FreeFCB`, `GetFCB`, `IsOurVCB`,
   `IsOurFCB`, `FindVCB`
3. All `Trap*` handler functions (TrapOpen, TrapClose, TrapRead,
   TrapWrite, TrapGetEOF, TrapSetEOF, TrapGetFPos, TrapSetFPos,
   TrapFlushFile, TrapAllocate, TrapOpenRF, TrapGetFileInfo,
   TrapSetFileInfo, TrapGetCatInfo, TrapSetCatInfo, TrapCreate,
   TrapDelete, TrapRename, TrapGetVolInfo, TrapGetVol, TrapSetVol,
   TrapUnmountVol, TrapEject, TrapFlushVol, TrapOpenWD, TrapCloseWD,
   TrapGetWDInfo, TrapGetVolParms, TrapGetFCBInfo, TrapDirCreate,
   TrapCatMove, TrapSetVInfo)
4. Dispatch tables: `sFlatTraps[]`, `sHFSTraps[]`, `AddEntry`,
   `InitTrapTables`
5. `DispatchFromTable`, `DispatchFlat`, `DispatchHFS`
6. `MountNewDrive(Globals *g, short slot, short vRefNum, short driveNum)`
7. `log_trap` helper
8. `dbg_hexdump` (debug utility, retained)

**Changes from original:**
- All functions receive `Globals *g` from dispatch (no change — already
  does this via `get_globals()` in `DispatchFlat`/`DispatchHFS`)
- Uses `dbg_log_drive` macro instead of raw `dbg_log6`

**Deliverable:** `macsrc/init/drive.c`

---

## Phase 5 — `init.c` (entry + filter + stubs)

The main file, extracted from shareddrive's boot code + filter,
merged with clipsync's filter logic:

1. `#include "defs.h"`
2. `MakeFlatStub(long dispatchAddr, long oldAddr)` — verbatim
3. `MakeHFSStub(long dispatchAddr, long oldAddr)` — verbatim
4. `InstallFlatPatch(unsigned short trapWord, char *regBase)` — verbatim
5. `InstallHFSPatch(char *regBase)` — verbatim
6. `PollMounts(Globals *g)` — extracted from shareddrive's filter body:
   call `kExtFSPollMount`, if slot != $FFFFFFFF call `MountNewDrive` +
   `PostEvent(diskEvt, driveNum)`
7. `PollGuestCmd(Globals *g)` — extracted from shareddrive's filter body:
   launch/ExitToShell/ShutDwnPower logic
8. `FilterEntry(void)` — unified jGNEFilter:
   ```
   save regs, SetUpA4
   g = get_globals()
   if g != NULL:
     SyncClipboard(g)             -- clip.c (internally throttled)
     if TickCount - g->lastPollTick >= 60:
       g->lastPollTick = TickCount
       PollMounts(g)
       PollGuestCmd(g)
   RestoreA4, restore regs
   chain to g->oldFilter
   ```
9. `main(void)` — boot sequence:
   ```
   RememberA0 / SetUpA4
   find_reg_base → bail if NULL
   check ExtFSVersion ($200) → set driveAvail flag
   check ClipVersion ($100) → set clipAvail flag
   bail if neither available
   allocate Globals (NewPtrSysClear)
   set_globals via $020E
   DetachResource('INIT', 128) / HLock / HNoPurge
   if driveAvail:
     drain PollMount queue
     if driveCount > 0:
       InitTrapTables
       InstallHFSPatch
       install 21 flat patches
   install jGNEFilter (always)
   log done
   RestoreA4
   ```

**Deliverable:** `macsrc/init/init.c`

---

## Phase 6 — Cleanup & Documentation

1. Add a short `macsrc/init/README.md`:
   - Build instructions (THINK C 5, code resource, INIT ID 128)
   - Notes on installing into system file
2. Update `macsrc/INIT.md` status section to mark plan as executed
3. Verify no stray references to old INIT IDs (314, 315) in new code

**Deliverable:** `macsrc/init/README.md`, updated `macsrc/INIT.md`

---

## File Dependency Graph

```
defs.h ──────────────────────────────────┐
  │                                      │
  ├── comm.c  (no deps on other .c)      │
  │     ↑                                │
  ├── clip.c  (calls comm.c functions)   │
  │     ↑                                │
  ├── drive.c (calls comm.c functions)   │
  │     ↑                                │
  └── init.c  (calls comm/clip/drive)    │
              (provides FilterEntry,     │
               main)                     │
```

All four .c files include `defs.h` and are linked into one code resource.
No circular dependencies between .c files — only `init.c` calls into the
other three.

---

## Verification

Since we can't compile on a vintage Mac in this workflow, verification is:
- Manual review of each file for syntactic correctness (THINK C subset of
  ANSI C, no prototypes in function bodies, 16-bit int)
- Confirm command codes match the backend constants in `extn_clip.cpp` and
  `extn_extfs.cpp`
- Confirm all functions referenced cross-file have prototypes in `defs.h`
- Confirm no `$0B00` usage survives
- Confirm INIT resource ID is 128 throughout
- **Byte-for-byte function comparison**: every function body carried over
  from `shareddrive/init.c` or `clipsync/init.c` must be diff'd against
  the original to confirm no accidental rewriting, reordering, or
  "interpretation" by the LLM. Only explicitly documented changes (e.g.
  replacing `regBase` locals with `g->regBase`, removing `$0B00` access)
  are permitted to differ. Any other delta is a bug.
