# HFS Trap Variants — Implementation Plan

Design: [HFS_TRAPS.md](HFS_TRAPS.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Widen OS trap mask to preserve bit 9 | ✅ |
| 2 | Add HIOParam struct to types.def | ✅ |
| 3 | Add HFS variant entries to traps.def | ✅ |
| 4 | Update NewPtrClear comment in traps.def | ✅ |
| 5 | Extend debugger smoke tests | ✅ |

Build gate: `cmake --preset macos && cmake --build --preset macos`
Test gate:  `cd test && ./debugger_smoke.sh`

---

## Phase 1 — Widen OS Trap Mask to Preserve Bit 9

The mask change is the foundation: without it, $A200 and $A000 map to
the same key so distinct HFS entries cannot exist.  This is a one-line
change with no new files.

### 1.1 — Modify `maskTrapWord()` in `src/cpu/trap_defs.cpp`

Change the OS branch mask from `0x01FF` to `0x03FF`.

File: `src/cpu/trap_defs.cpp`, line 182 (the `else` branch).

Before:
```cpp
	else
		return 0xA000 | (tw & 0x01FF); /* OS: keep bits 0-8 */
```

After:
```cpp
	else
		return 0xA000 | (tw & 0x03FF); /* OS: keep bits 0-9 */
```

No other call sites change — `find()`, `findSubtrap()`, the load
loop, `TrapTracer::enter()`, and the filter commands all call through
`maskTrapWord()` already.

### Fence

- [ ] `maskTrapWord(0xA214)` returns `0xA214` (not `0xA014`)
- [ ] `maskTrapWord(0xA014)` still returns `0xA014`
- [ ] `maskTrapWord(0xA31E)` returns `0xA31E` (NewPtrClear becomes distinct key)
- [ ] `maskTrapWord(0xA11E)` still returns `0xA11E` (NewPtr flat)
- [ ] Toolbox masking unchanged: `maskTrapWord(0xA9A0)` still returns `0xA9A0`
- [ ] Full build clean
- [ ] Existing smoke tests pass (no HFS entries yet, so no filter tests break)
- [ ] Commit: `"traps: preserve bit 9 in OS trap mask for HFS variants"`

---

## Phase 2 — Add HIOParam Struct to types.def

HOpen, HOpenRF, and HRename use IO-style fields (`ioRefNum`,
`ioPermssn`, `ioMisc`) plus the HFS `ioDirID` at offset 48.
Neither `IOParam` nor `HFileInfo` has the right combination.

### 2.1 — Add `HIOParam` to `assets/types.def`

Insert after the existing `IOParam` struct (after line 85 in
`assets/types.def`).

```
struct HIOParam {
    0  ParamBlockHeader header
    24 sword    ioRefNum
    26 sbyte    ioVersNum
    27 sbyte    ioPermssn
    28 Ptr      ioMisc
    32 Ptr      ioBuffer
    36 long     ioReqCount
    40 long     ioActCount
    44 sword    ioPosMode
    46 slong    ioPosOffset
    48 long     ioDirID
}
```

This is `IOParam` + `ioDirID` at offset 48, matching
`HParamBlockRec` from Inside Macintosh.  The `ioDirID` overlaps the
tail of `ioPosOffset` — this matches the real layout; the tracer only
reads fields named in show-in/show-out, so the overlap is harmless.

### Fence

- [ ] `assets/types.def` contains `struct HIOParam` with `ioDirID`
      at offset 48
- [ ] Full build clean (types.def is parsed at runtime, so this only
      needs to be syntactically correct)
- [ ] Existing smoke tests still pass
- [ ] Commit: `"traps: add HIOParam struct for HFS I/O variants"`

---

## Phase 3 — Add HFS Variant Entries to traps.def

Nine new entries, one per HFS variant from Inside Macintosh.
Each gets its own trap word (bit 9 set), name, struct type, and
show-in/show-out fields.

### 3.1 — Add HFS entries to `assets/traps.def`

Insert a new section after the existing flat File Manager entries
(after the `A015 SetVol` block, before the `A260 HFSDispatch` block).
Use a section comment to delineate.

```
# ── HFS File Manager variants (bit 9 set) ──────────────
# These are the H-prefixed variants documented in Inside Macintosh.
# They use HParamBlockRec (larger than ParamBlockRec) and include
# ioDirID for hierarchical path resolution.

A200 HOpen os
  in  pb:^HIOParam.A0
  out err:OSErr.D0
  show-in  pb ioNamePtr ioVRefNum ioDirID ioPermssn
  show-out pb ioResult ioRefNum

A208 HCreate os
  in  pb:^HFileInfo.A0
  out err:OSErr.D0
  show-in  pb ioNamePtr ioVRefNum ioDirID
  show-out pb ioResult

A209 HDelete os
  in  pb:^HFileInfo.A0
  out err:OSErr.D0
  show-in  pb ioNamePtr ioVRefNum ioDirID
  show-out pb ioResult

A20A HOpenRF os
  in  pb:^HIOParam.A0
  out err:OSErr.D0
  show-in  pb ioNamePtr ioVRefNum ioDirID ioPermssn
  show-out pb ioResult ioRefNum

A20B HRename os
  in  pb:^HIOParam.A0
  out err:OSErr.D0
  show-in  pb ioNamePtr ioVRefNum ioDirID ioMisc
  show-out pb ioResult

A20C HGetFileInfo os
  in  pb:^HFileInfo.A0
  out err:OSErr.D0
  show-in  pb ioNamePtr ioVRefNum ioFDirIndex ioDirID
  show-out pb ioResult ioNamePtr ioFlAttrib ioFlFndrInfo ioFlNum ioFlLgLen ioFlRLgLen ioFlCrDat ioFlMdDat ioDirID ioFlParID

A20D HSetFileInfo os
  in  pb:^HFileInfo.A0
  out err:OSErr.D0
  show-in  pb ioNamePtr ioVRefNum ioDirID ioFlFndrInfo
  show-out pb ioResult

A214 HGetVol os
  in  pb:^WDParam.A0
  out err:OSErr.D0
  show-out pb ioResult ioNamePtr ioVRefNum ioWDProcID ioWDVRefNum ioWDDirID

A215 HSetVol os
  in  pb:^WDParam.A0
  out err:OSErr.D0
  show-in  pb ioNamePtr ioVRefNum ioWDDirID
  show-out pb ioResult
```

### Fence

- [ ] `assets/traps.def` contains 9 new entries: HOpen through HSetVol
- [ ] Each entry uses the correct struct type (HIOParam, HFileInfo,
      or WDParam) as specified in the design
- [ ] Full build clean
- [ ] Existing smoke tests still pass
- [ ] Commit: `"traps: add HFS File Manager variant definitions"`

---

## Phase 4 — Update NewPtrClear Comment in traps.def

The existing comment at line 49 says OS trap flag bits 9 and 10 are
masked off.  After Phase 1, only bit 10 is masked off.  Update the
comment to reflect the new masking and explain why NewPtrClear ($A31E)
now has a distinct key but no separate entry.

### 4.1 — Update comment in `assets/traps.def`

Replace lines 49–51:

Before:
```
# NewPtrClear ($A31E) = NewPtr ($A11E) + CLEAR bit (bit 9).
# OS trap flag bits (CLEAR=9, SYS=10) are masked off during lookup,
# so $A31E automatically maps to the NewPtr definition above.
```

After:
```
# NewPtrClear ($A31E) = NewPtr ($A11E) + CLEAR bit (bit 9).
# OS trap masking now preserves bit 9, so $A31E and $A11E are
# distinct keys.  No separate entry is defined for NewPtrClear
# since Inside Mac does not give it a distinct name.
# Bit 10 (SYS heap) is still masked off.
```

### Fence

- [ ] Comment updated at line 49 of `assets/traps.def`
- [ ] Full build clean
- [ ] Existing smoke tests still pass
- [ ] Commit: `"traps: update NewPtrClear comment for new bit-9 masking"`

---

## Phase 5 — Extend Debugger Smoke Tests

Verify that HFS variants are distinct from flat variants in the
filter system, and that both can be independently enabled/disabled.

### 5.1 — Add test cases to `test/debugger_smoke.sh`

Add the following tests after the existing "trace traps" tests
(after the "info trace on" test, around line 128):

**Test: `+HOpen` resolves independently of `+Open`**
```bash
check "trace traps HOpen distinct" "trace traps +HOpen
info trace
quit" "+HOpen"
```

This verifies that `+HOpen` is recognized as a valid trap name
(it would fail if the entry didn't load or if masking collapsed
it to Open).

**Test: `+Open` does not include HOpen**
```bash
check "trace traps Open not HOpen" "trace traps +Open
info trace
quit" "+Open"
```

Verify the filter shows `+Open` (not `+HOpen`), confirming they
are separate.

**Test: `+HGetVol` resolves**
```bash
check "trace traps HGetVol" "trace traps +HGetVol
info trace
quit" "+HGetVol"
```

Verifies an HFS entry using WDParam struct type loads correctly.

### Fence

- [ ] Three new test cases added to `test/debugger_smoke.sh`
- [ ] All smoke tests pass (both existing and new)
- [ ] Full build clean
- [ ] Commit: `"test: smoke tests for HFS trap variant filters"`

---

## Summary

5 phases, progressing from the foundational mask change through data
definitions to testing.  Each phase is independently committable
and leaves the build green.

| Phase | Files modified | Risk |
|-------|---------------|------|
| 1 | `src/cpu/trap_defs.cpp` | Low — one-line mask change; all call sites go through `maskTrapWord()` |
| 2 | `assets/types.def` | Minimal — new struct, no existing code affected |
| 3 | `assets/traps.def` | Low — additive; new entries don't interfere with existing ones |
| 4 | `assets/traps.def` | Trivial — comment-only change |
| 5 | `test/debugger_smoke.sh` | Minimal — additive tests |
