# SharedDrive INIT — Bug Analysis

## Symptoms reported

1. **Opening files with resources from the shared drive crashes** (even files with an empty resource fork)
2. **Copying the file to the main drive and opening it there works** — so the raw data/resource bytes are being served correctly
3. **TYPE/CREATOR info is lost** when files are copied to the shared drive

---

## Bug 1 (Critical): WD refnum encoding overflows `int16` — likely crash cause

The host allocates WD references starting at `0x8000` (32768) in `host_volume.h`:

```cpp
uint32_t nextWD_ = 0x8000;
```

The guest encodes these in `DoOpenWD` as:

```c
*(short *)(pb + pb_ioVRefNum) = (short)(-(long)wdRef - 32000);
```

With `wdRef = 32768`: `-(32768 + 32000) = -64768`, which **overflows `int16`** (range -32768 to 32767). Cast to `short`, it wraps to **+768**.

`IsOurVolume(768)` → **false**. Every subsequent File Manager call using this WD refnum silently falls through to the ROM, which can't find our virtual files.

**Why this causes the resource-fork crash:** When the Finder launches a file from the Shared volume, it passes a WD refnum to the application via AppParmHandle. The app calls `_OpenRF` with `vRefNum = 768`; `IsOurVolume` returns false; the call falls through to the ROM; the ROM fails; the app gets an unexpected error where it expects a valid resource file and crashes.

**Why basic copy/browse still works:** Those operations use the real `vRefNum = -32000` with explicit `dirID` via the `PBH*` HFS-aware traps (bit 9 set in the trap word), bypassing WDs entirely.

**Fix:** Change `nextWD_` to start at `1`. Also widen the `IsOurVolume` range check from 99 to 999 WDs.

---

## Bug 2 (Critical): `PBSetCatInfo` is a no-op — TYPE/CREATOR lost

In `DispatchHFS`, the `kSetCatInfo` handler:

```c
case kSetCatInfo:
    dbg_log(g->regBase, "SD _SetCatInfo -> noErr");
    *(short *)(pb + pb_ioResult) = 0;
    RestoreA4(); return 0;
```

Returns `noErr` without doing anything. On HFS volumes, the Finder uses `PBSetCatInfo` (HFSDispatch selector `$000A`) — not the flat `_SetFileInfo` — to set TYPE, CREATOR, flags, and dates after copying. Since this silently succeeds, file metadata is never persisted to the AppleDouble sidecar.

**Fix:** Extract TYPE/CREATOR from the CInfoPBRec, resolve name→CNID via ObjByName, and call `ExtFSSetFileInfo` (0x0213). Directories succeed silently. Full log_trap() coverage on all paths.

---

## Bug 3 (Medium): `AllocFCB` doesn't fully initialize the FCB

`AllocFCB` sets 7 fields but leaves these **containing stale values** from whatever file previously occupied this FCB slot:

| Field | Offset | Size | Risk |
|-------|--------|------|------|
| `fcbSBlk` | 6 | 2 | ROM may read first alloc block |
| `fcbClmpSize` | 30 | 4 | ROM may use for allocation math |
| `fcbExtRec` | 38 | 12 | ROM may use for extent-to-block mapping |
| `fcbFType` | 50 | 4 | ROM caches file type here |
| `fcbCatPos` | 54 | 4 | Catalog hint — garbage could confuse Close |

The ROM Resource Manager reads FCB fields directly from the buffer, not through traps. Stale extent records could cause ROM code to attempt disk I/O based on garbage block numbers.

**Fix:** Zero the entire 94-byte FCB before populating fields.

---

## Bug 4 (Medium): `_SetEOF` doesn't update the host

The `_SetEOF` handler (trap `$A012`) updates only the FCB's logical EOF — it never tells the host to truncate or extend the underlying file. The Resource Manager may call `_SetEOF` on a resource fork before writing. If the host file size and FCB EOF diverge, subsequent reads will return stale or wrong data.

**Fix:** Add an `ExtFSSetEOF` host command (0x218) that truncates/extends the data or resource fork.

---

## Other issues (not fixed yet)

### Bug 5 (Low): `get_globals()` makes a host RPC on every call

Every `DispatchFlat` and `DispatchHFS` invocation (and `IsOurFCB`/`IsOurVolume`) pays the cost of a full register-block RPC to retrieve a pointer that never changes after INIT time.

### Bug 6 (Low): `_GetFCBInfo` indexed scan returns `paramErr`

FCB index walks (`ioFCBIndx > 0`) return `paramErr(-50)` instead of passing through. The Resource Manager scans all open FCBs in some circumstances.

### Bug 7 (Low): `DoRead` returns `noErr` on zero-length read at EOF

Callers like the Resource Manager may need `eofErr(-39)` to detect end-of-file.

---

## Summary

| # | Severity | Symptom | Root cause | Status |
|---|----------|---------|------------|--------|
| 1 | **Critical** | Crash opening files with resources | WD refnum `int16` overflow | Fixed |
| 2 | **Critical** | TYPE/CREATOR lost | `PBSetCatInfo` is a no-op | Fixed |
| 3 | Medium | Potential crash/corruption | FCB not fully zeroed | Fixed |
| 4 | Medium | Resource fork data mismatch | `_SetEOF` not synced to host | Fixed |
| 5 | Low | Performance | `get_globals()` RPC on every trap | TODO |
| 6 | Low | Resource Manager scan failure | `_GetFCBInfo` indexed walk | TODO |
| 7 | Low | EOF detection | Zero-byte read returns `noErr` | TODO |
