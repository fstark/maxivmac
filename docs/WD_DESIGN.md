# WD_DESIGN — Host-Authoritative Working Directory Management

## Status

Design — not yet implemented.  Supersedes the incremental fixes in
`WD_PLAN.md`.

## Problem Statement

Working directory (WD) management is split between the guest INIT
(`macsrc/shareddrive/init.c`) and the host
(`src/storage/host_volume.cpp`, `src/core/extn_extfs.cpp`).  This
dual bookkeeping has caused **5 regressions**, all with the same
root pattern: the guest and host disagree about the current
directory.

Specific failure modes observed:

1. **TrapSetVol sends bare volume ref instead of WD refnum** — host
   resolves to root instead of the target folder.
2. **defaultDirID global gets stale** — single global can't track
   multiple active subdirectories.
3. **WD encoding arithmetic done twice** (guest encodes, host
   decodes) — any mismatch is silently wrong, never fails fast.
4. **TrapGetVol synthesizes ioWDDirID** by querying host, then
   returns to the app — extra round-trip that can disagree with what
   the host actually uses when resolving subsequent Open calls.
5. **sOurWDs[] tracking diverges from host wdTable_** — guest can
   lose track of WDs (e.g. table full), causing IsOurVolume() to
   miss WD refnums → trap dispatched to wrong filesystem.

The root cause is a single architectural flaw: **the guest maintains
WD state that the host also maintains, and the two are not kept in
sync**.

## Design Principle

**Single source of truth: the host owns all routing and WD state.**

The guest INIT becomes a thin dispatcher.  It does not decide
whether a trap is "ours" — it sends every File Manager trap to
the host.  The host checks volume ownership, resolves WDs, and
either handles the call or replies `kNotOurs` so the guest can
pass through to the ROM file system.

This eliminates dual bookkeeping entirely.  The guest does not
store, encode, decode, or track WD refnums.  It does not compare
vRefNums against ranges or VCB pointers.  Every routing and WD
decision is made in one place: the host.

## What the Guest Keeps

After this redesign, the guest Globals struct retains:

| Field         | Purpose                                      |
|---------------|----------------------------------------------|
| `regBase`     | Extension register block address             |
| `vcb[]`       | VCB pointers (for DefVCBPtr write-back only) |
| `dqe[]`       | Drive queue elements                         |
| `driveCount`  | Number of mounted drives                     |
| `savedA4`     | THINK C code resource A4                     |
| `ejected`     | Post-eject flag                              |
| `oldFilter`   | Previous jGNEFilter                          |
| `lastPollTick`| Tick at last poll                            |

`vcb[]` is retained only so the guest can write DefVCBPtr when the
host returns a slot index from `kPB_SetVol`.  It is never read for
routing decisions.

### Why the host reads DefVCBPtr

vRefNum=0 means "use the default volume."  The system's ground
truth for the default volume is the low-memory global `DefVCBPtr`
(address 0x0352).  When another filesystem (the ROM's HFS handler)
processes a `_SetVol`, it writes DefVCBPtr — our host never hears
about it.  Any cached `defaultSlot_` would go stale.

The solution: the host reads DefVCBPtr directly from guest RAM
(4 bytes at 0x0352), then reads `vcbVRefNum` at offset 78 of the
VCB it points to.  If that vRefNum matches one of our slots, the
call is ours.  If not, it's `kNotOurs`.

```cpp
// In DriveManager or volumeFromPB:
bool isDefaultOurs(int &outSlot) const
{
    uint32_t vcbPtr = get_vm_long(0x0352);  // DefVCBPtr
    if (vcbPtr == 0) return false;
    int16_t vRef = static_cast<int16_t>(get_vm_word(vcbPtr + 78));
    outSlot = slotFromVRefNum(vRef);
    return outSlot >= 0;
}
```

This is the same mechanism the host already uses for reading PB
fields and Pascal strings — it's not a new kind of coupling.
DefVCBPtr is authoritative guest state, like ioVRefNum in a
parameter block.

**Removed:**

| Field              | Why                                       |
|--------------------|-------------------------------------------|
| `rootWDRefNum`     | Host tracks root WD internally            |
| `defaultWDRefNum`  | Host tracks default WD                    |
| `sOurWDs[]`        | Host tracks allocated WDs                 |
| `sOurWDCount`      | Host tracks allocated WDs                 |

## What the Guest Keeps Doing

The guest INIT still:

- Intercepts File Manager traps via the dispatch table.
- Manages the FCB table (open file tracking).
- Manages VCB queue entries, drive queue entries.
- Writes DefVCBPtr when host says "this is ours".

The guest INIT stops:

- Deciding "is this our volume?" — host decides.
- Encoding/decoding WD refnums (`-(wdRef + 32000)` arithmetic).
- Tracking which WD refnums it allocated.
- Storing any "current directory" state.
- Synthesizing WD-related PB output fields.
- Comparing vRefNums against ranges or VCB names.

## Guest Dispatch Model

Every File Manager trap follows this pattern:

```c
static OSErr TrapWhatever(char *pb, Globals *g, short isHFS)
{
    reg_set(g->regBase, 0, (unsigned long)pb);
    reg_set(g->regBase, 1, (unsigned long)isHFS);
    reg_command(g->regBase, kPB_Whatever);

    short result = reg_result(g->regBase);
    if (result == kNotOurs) return 1;  /* pass through to ROM */
    return result;
}
```

The host returns `kNotOurs` (a dedicated sentinel, e.g. 0xFFFF)
when the vRefNum/name doesn't match any of our volumes.  The guest
treats this as "pass through" and jumps to the original trap
handler.

Traps that need post-processing (e.g. TrapOpen allocates an FCB)
still do that work after the host call succeeds.  But the routing
decision is always the host's.

**Performance note:** Every File Manager trap now does a
register-block round-trip, even for real-HFS-disk calls.  The
reg_command mechanism is a memory-mapped write + read (no I/O, no
syscall).  The host returns kNotOurs within a few comparisons.
This adds ~microseconds to each trap, which is acceptable for the
simplicity gain.

## New Host Commands

### `kPB_SetVol` (0x0246) — Atomic SetVol

Replaces the two-call sequence (kCmdOpenWD + kPB_SetDefaultVRefNum).

**Guest sends:**
```
reg[0] = pointer to parameter block (in guest RAM)
reg[1] = isHFS flag
```

**Host reads from PB:**
- `ioVRefNum` (offset 22) — the vRefNum the app passed
- `ioNamePtr` (offset 18) — volume name (if any)
- `ioWDDirID` (offset 48, HFS only) — target directory ID

**Host does:**
1. Resolve volume from vRefNum or name.
2. If `ioWDDirID != 0 && ioWDDirID != kRootDirID`:
   Open a new WD for that dirID, set it as default.
3. Else if vRefNum is a WD refnum:
   Set that WD as default.
4. Else:
   Set the root WD as default.

**Host returns:**
- `reg[0]` = slot index (so guest can set DefVCBPtr).
- `regResult` = 0 on success, `kNotOurs` if volume not ours,
  error code otherwise.

**Guest does with result:**
- If `kNotOurs`: return 1 (pass through to ROM).
- Sets `DefVCBPtr = g->vcb[reg[0]]`.
- That's it.

### `kPB_GetVol` (0x0247) — Atomic GetVol

Replaces the guest logic that synthesizes GetVol/GetVolInfo output.

**Guest sends:**
```
reg[0] = pointer to parameter block (in guest RAM)
reg[1] = isHFS flag
```

**Host fills PB fields directly in guest RAM:**
- `ioVRefNum` (22) = current default WD refnum (guest-encoded)
- `ioNamePtr` target = volume name (Pascal string)
- If HFS:
  - `ioWDVRefNum` (32) = root WD refnum (guest-encoded)
  - `ioWDProcID` (28) = 0
  - `ioWDDirID` (48) = dirID of current default WD

**Host returns:**
- `regResult` = 0, or `kNotOurs` if the default volume isn't ours.

**Guest does:**
- If `kNotOurs`: return 1 (pass through to ROM).
- Otherwise: nothing extra.  All PB fields are already filled.

### Retained Commands (unchanged)

| Command            | Code   | Notes                         |
|--------------------|--------|-------------------------------|
| `kPB_OpenWD`       | 0x0242 | Apps call _OpenWD directly    |
| `kPB_CloseWD`      | 0x0243 | Apps call _CloseWD directly   |
| `kPB_GetWDInfo`    | 0x0244 | Apps call _GetWDInfo directly |

These are pure pass-through: guest sends the PB, host does the
work, guest returns the result.  No guest-side WD state involved.

### Removed Commands

| Command                | Code   | Why removed                  |
|------------------------|--------|------------------------------|
| `kCmdOpenWD`           | 0x020B | Absorbed into kPB_SetVol     |
| `kCmdGetWDInfo`        | 0x020A | Only used by guest GetVol    |
| `kPB_SetDefaultVRefNum`| 0x0245 | Absorbed into kPB_SetVol     |

(The host can keep handling these for backward compatibility during
transition, but the guest INIT will stop calling them.)

## Multi-Volume WD Scoping

### Current Problem

Host WD operations (`PbOpenWD`, `PbCloseWD`, `PbGetWDInfo`,
`RegOpenWD`, `RegCloseWD`) are hardcoded to `s_drives.volume(0)`.
If two shared folders are mounted, WDs only work on the first.

### Design

Keep a **global WD→(slot, dirID) map** in `DriveManager` instead
of per-volume maps.  This eliminates the need to route WD
operations by slot:

```cpp
// In DriveManager:
struct WDEntry {
    int      slot;
    uint32_t dirID;
    uint32_t procID;
};
std::unordered_map<uint32_t, WDEntry> wdTable_;
uint32_t nextWD_ = 1;
```

When `PbOpenWD` is called, `DriveManager` determines the volume
from `volumeFromPB()` (which already handles all vRefNum forms),
records the slot, and returns a globally unique WD ref.

When `resolveDir` or `PbGetWDInfo` is called with a WD refnum,
`DriveManager` looks it up directly — no slot routing needed.

**Recommendation:** Move WD table to `DriveManager`.

### Host State After Redesign

```
DriveManager
├── wdTable_: map<uint32_t, WDEntry>   // all WDs across all volumes
├── nextWD_: uint32_t                   // monotonic WD allocator
├── defaultWD_: uint32_t                // current default WD ref
│                                       // (replaces per-volume defaultVRefNum_)
└── slots_[8]
    └── HostVolume                      // catalog, open forks (no WD state)
```

Note: no `defaultSlot_` — the host reads `DefVCBPtr` from guest
RAM when it needs the current default volume.  This avoids a
cached-default-goes-stale problem (see "Why the host reads
DefVCBPtr" above).

## Trap-by-Trap Changes

### TrapSetVol (guest)

**Before:** 80 lines of WD resolution, encoding, host calls, and
`defaultWDRefNum` bookkeeping.

**After:**
```c
static OSErr TrapSetVol(char *pb, Globals *g, short isHFS)
{
    reg_set(g->regBase, 0, (unsigned long)pb);
    reg_set(g->regBase, 1, (unsigned long)isHFS);
    reg_command(g->regBase, kPB_SetVol);

    short result = reg_result(g->regBase);
    if (result == kNotOurs) return 1;
    if (result != 0) return result;

    /* Host told us which slot — set DefVCBPtr */
    {
        short slot = (short)reg_get(g->regBase, 0);
        *(Ptr *)kDefVCBPtr = g->vcb[slot];
    }
    return kNoErr;
}
```

No IsOurVolume check.  No WD arithmetic.  No tracking.  No globals
written.

### TrapGetVol (guest)

**Before:** Reads defaultWDRefNum, synthesizes PB fields, queries
host for ioWDDirID.

**After:**
```c
static OSErr TrapGetVol(char *pb, Globals *g, short isHFS)
{
    reg_set(g->regBase, 0, (unsigned long)pb);
    reg_set(g->regBase, 1, (unsigned long)isHFS);
    reg_command(g->regBase, kPB_GetVol);

    short result = reg_result(g->regBase);
    if (result == kNotOurs) return 1;
    return result;
}
```

### TrapGetVolInfo (guest)

**Before:** Returns defaultWDRefNum in ioVRefNum and synthesizes
ioWDDirID with a host round-trip.

**After:** Host fills all PB fields.  The host already has
access to all the data: volume metadata (file count, sizes) via
HostVolume, WD fields via DriveManager, volume name via the
VolumeName table.  The host reads VCB metadata from guest RAM
if needed (same mechanism as DefVCBPtr).

### TrapOpenWD / TrapCloseWD / TrapGetWDInfo (guest)

**Already pure pass-through.**  The only change is removing the
`TrackWD()` / `UntrackWD()` calls since the guest no longer tracks
WDs.  The host returns `kNotOurs` if the vRefNum doesn't match.

### TrapOpen / TrapOpenRF / TrapGetCatInfo / etc. (guest)

**Same pattern** — send PB to host, host returns `kNotOurs` or
result.  Guest-side routing logic (`IsOurVolume()` checks) is
removed.  Traps that do post-processing (FCB allocation for Open)
still do that after the host returns success.

### IsOurVolume / IsOurVCB / FindVCB (guest)

**Deleted.**  The host is the sole authority on volume ownership.

## Host-Side `resolveDir()` Redesign

Current `resolveDir()` has special-case logic for
`defaultVRefNum_` that has been a source of bugs.

**New implementation** (on DriveManager, not HostVolume):

```cpp
uint32_t DriveManager::resolveDir(int16_t vRefNum,
                                  uint32_t rawDirID,
                                  int &outSlot) const
{
    // Explicit dirID always wins.
    if (rawDirID != 0)
    {
        outSlot = slotFromVRefNum(vRefNum);
        if (outSlot < 0)
        {
            // vRefNum=0 with explicit dirID: check DefVCBPtr.
            if (vRefNum == 0 && isDefaultOurs(outSlot))
                return rawDirID;
            return 0;  // can't determine slot
        }
        return rawDirID;
    }

    // vRefNum 0 → check DefVCBPtr to see if default is ours.
    if (vRefNum == 0)
    {
        int defSlot = -1;
        if (!isDefaultOurs(defSlot))
        {
            outSlot = -1;
            return 0;  // not ours → caller returns kNotOurs
        }
        outSlot = defSlot;
        return wdToDirID(defaultWD_);
    }

    // Direct volume ref or drive number → root dir.
    int slot = slotFromVRefNum(vRefNum);
    if (slot >= 0)
    {
        outSlot = slot;
        return kRootDirID;
    }

    // WD refnum → look up.
    auto wdRef = decodeGuestWDRef(vRefNum);
    auto it = wdTable_.find(wdRef);
    if (it == wdTable_.end())
    {
        // Unknown WD: fail immediately, do not fall back.
        outSlot = -1;
        return 0;  // caller checks for 0 and returns error
    }
    outSlot = it->second.slot;
    return it->second.dirID;
}
```

Key difference from current code: **unknown WD returns 0 (error),
not kRootDirID**.  This is the fail-fast principle — a bad WD
refnum must not silently resolve to root.

## Fail-Fast Rules

1. **Unknown WD refnum → error, not root.**
   `resolveDir()` returns 0.  Callers return `kRfNumErr` or
   `kNsvErr` to the application.

2. **kPB_SetVol with invalid dirID → error, not fallback.**
   If the requested dirID doesn't exist in the catalog, return
   `kDirNFErr`.  Do not fall back to root.

3. **vRefNum not ours → `kNotOurs`, not silent pass-through.**
   The host explicitly returns the `kNotOurs` sentinel.  The
   guest explicitly passes through.  No guessing.

4. **No `else return kRootDirID` fallbacks.**
   Every branch in resolveDir either returns a known value or
   returns 0 (error).  The "else root" pattern is banned.

## Files Changed

### Guest — `macsrc/shareddrive/init.c`

- Remove `rootWDRefNum`, `defaultWDRefNum` from Globals.
- Remove `sOurWDs[]`, `sOurWDCount`, `TrackWD()`, `UntrackWD()`.
- Remove `IsOurVolume()`, `IsOurVCB()`, `FindVCB()`.
- Rewrite all trap handlers to host-first dispatch pattern.
- Remove root WD creation from `main()`.
- Remove `kCmdOpenWD`, `kCmdGetWDInfo` command constants.

### Host — `src/core/extn_extfs.cpp`

- Add `kPB_SetVol` handler (reads PB, checks ownership, calls
  DriveManager).  Returns `kNotOurs` if volume not ours.
- Add `kPB_GetVol` handler (fills PB fields in guest RAM).
  Returns `kNotOurs` if default volume not ours.
- Add `kNotOurs` return path to all existing PB handlers
  (`PbOpen`, `PbOpenRF`, `PbGetCatInfo`, `PbCreate`, `PbDelete`,
  `PbRename`, `PbOpenWD`, `PbCloseWD`, `PbGetWDInfo`, etc.).
  Currently these assume the guest already validated ownership;
  now the host must do it via `volumeFromPB()` and return
  `kNotOurs` when the vRefNum doesn't match.
- Update `PbOpenWD` to use DriveManager WD table
  (not `s_drives.volume(0)`).
- Update `PbCloseWD` likewise.
- Update `PbGetWDInfo` likewise.
- Replace `volumeFromPB()` WD logic to use DriveManager.
- Replace `s_defaultSlot` with DefVCBPtr reads via
  `DriveManager::isDefaultOurs()`.
- Remove `kPB_SetDefaultVRefNum` handler (or keep as no-op for
  transition).
- Remove `RegOpenWD` / `RegCloseWD` / `RegGetWDInfo` register
  handlers (unused after guest changes).

### Host — `src/storage/host_volume.h` / `host_volume.cpp`

- Remove `wdTable_`, `nextWD_`, `defaultVRefNum_` from
  `HostVolume`.
- Remove `openWD()`, `closeWD()`, `wdToDirID()`, `wdToProcID()`,
  `setDefaultVRefNum()`, `resolveDir()` from HostVolume.

### Host — `src/storage/drive_manager.h` / `drive_manager.cpp`

- Add `WDEntry { slot, dirID, procID }` struct.
- Add `wdTable_`, `nextWD_`, `defaultWD_`.
- Add `openWD()`, `closeWD()`, `wdToDirID()`, `resolveDir()`.
- Add `isDefaultOurs()` — reads DefVCBPtr from guest RAM.
- Add `setDefaultWD()`, `defaultWDAsGuestVRefNum()`.

## WD Refnum Encoding

Unchanged from current scheme:
```
guestVRefNum = -(wdRef + kBaseVRefNum)
wdRef = -(guestVRefNum) - kBaseVRefNum
```

where `kBaseVRefNum = 32000`.

`wdRef` is a monotonically increasing uint32 allocated by
`DriveManager::nextWD_`.

The encoding is done in exactly one place: `extn_extfs.cpp`
handlers that write guest PB fields.  The decoding is done in
exactly one place: `DriveManager::resolveDir()` and
`DriveManager::isOurWD()`.

The guest INIT never encodes or decodes WD refnums.

## Invariants

1. The guest INIT stores zero WD or routing state.
2. Every WD refnum returned to a Mac application was produced by
   the host in the same trap call.
3. `resolveDir()` never returns `kRootDirID` as a fallback for an
   error condition.  Errors return 0.
4. The WD table lives in DriveManager, not per-volume.
5. Volume ownership is decided in exactly one place: the host.
   The guest never compares vRefNums against ranges, VCB pointers,
   or WD tables.

## Risks and Mitigations

**Risk:** Every File Manager trap now does a host round-trip, even
for real-HFS-disk calls.

**Mitigation:** The `reg_command` mechanism is a memory-mapped
write + read.  The host's ownership check (`volumeFromPB`) is a
few comparisons returning `kNotOurs` immediately.  Measured
overhead per non-matching trap should be sub-microsecond in
emulated time.  If profiling shows this is a problem, we can add
a guest-side fast-reject for vRefNum ranges that can never be
ours (e.g. small negative numbers from real HFS WDs), but only
if measured — not preemptively.

**Risk:** Removing the guest root WD creation means the host must
create root WDs at mount time.

**Mitigation:** `DriveManager::mount()` calls `openWD(kRootDirID)`
automatically and stores the ref.  The root WD ref for each slot
is available via `DriveManager::rootWD(int slot)`.

**Risk:** Backward compatibility if old guest INIT is paired with
new host (or vice versa).

**Mitigation:** old codepath detection.  If the host receives the
old `kPB_SetDefaultVRefNum` command, it handles it as before.  If
the host receives the new `kPB_SetVol`, it uses the new path.  The
guest INIT version is already reported via `kExtFSGuestVars`.

## Test Plan

1. **Boot, mount single shared folder** — Finder shows volume.
2. **THINK C project in subfolder** — Open project, compile.  All
   source files must open without error.
3. **SetVol to subfolder, then Open with vRefNum=0** — file found
   in correct directory.
4. **Two shared folders mounted** — SetVol to subfolder on second
   volume, Open with vRefNum=0 finds file on correct volume.
5. **CloseWD then use stale refnum** — must get error, not root.
6. **GetVol after SetVol** — returned ioVRefNum+ioWDDirID match
   the SetVol target.
7. **Regression test: OpenRF in subfolder** — the original bug
   that started this investigation.
8. **Non-regression selftest.sh** — all existing tests pass.
