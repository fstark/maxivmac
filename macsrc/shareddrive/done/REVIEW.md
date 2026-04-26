# SharedDrive INIT — Code Review Plan

The goal is to validate every routine in `init.c` against the canonical
Inside Macintosh File Manager specification, identify fragile
assumptions, and fix anything that deviates from the documented ABI.

This document describes **what** needs reviewing and **why**. A
separate detailed plan will follow once the review scope is agreed on.

---

## 1. ParamBlockRec Field Offsets (highest risk)

Every `*(type *)(pb + offset)` access is a hard-coded byte offset into
the caller's ParamBlockRec. One wrong offset silently corrupts unrelated
fields or reads garbage.

**What to verify:**

| Variant | Used by | Fields to check |
|---------|---------|-----------------|
| ioParam | TrapRead, TrapWrite, TrapGetEOF, TrapSetEOF, TrapGetFPos, TrapSetFPos, TrapAllocate | ioRefNum(24), ioMisc(28), ioBuffer(32), ioReqCount(36), ioActCount(40), ioPosMode(44), ioPosOffset(46) |
| fileParam | TrapGetFileInfo, TrapSetFileInfo, TrapCreate, TrapDelete, TrapRename | ioFDirIndex(28), ioFlAttrib(30), ioFlFndrInfo(32, **16 bytes**), ioFlNum(48), ioFlStBlk(52), ioFlLgLen(54), ioFlPyLen(58), ioFlRStBlk(62), ioFlRLgLen(64), ioFlRPyLen(68), ioFlCrDat(72), ioFlMdDat(76) |
| HFS fileParam extension | TrapGetFileInfo, TrapGetCatInfo | ioFlBkDat(80), ioFlXFndrInfo(84, 16 bytes), ioFlParID(100), ioFlClpSiz(104) |
| dirInfo (CInfoPBRec) | TrapGetCatInfo, TrapSetCatInfo | ioDrUsrWds(32, 16 bytes), ioDrDirID(48), ioDrNmFls(52), ioDrCrDat(72), ioDrMdDat(76), ioDrBkDat(80), ioDrFndrInfo(84), ioDrParID(100) |
| volumeParam | TrapGetVolInfo | Every field from ioVolIndex(28) through ioVFndrInfo(90) |
| WDParam | TrapOpenWD, TrapCloseWD, TrapGetWDInfo, TrapSetVol, TrapGetVol | ioWDIndex(26), ioWDProcID(28), ioWDVRefNum(32), ioWDDirID(48) |
| FCBPBRec | TrapGetFCBInfo | ioFCBIndx(28), ioFCBFlNm(32), through ioFCBParID(58) |

**Method:** For each handler, list every `pb + XX` access, then read
the corresponding Inside Macintosh IV chapter (im023, lines
25017–30929) to confirm offset, size, and signedness.

**Known risk areas:**
- `pb_ioFlFndrInfo` is 16 bytes (FInfo = fdType+fdCreator+fdFlags+fdLocation+fdFldr). The code writes sub-fields at +0, +4, +8, +10, +14 — need to confirm this matches FInfo layout.
- `pb + 31` written as `ioACUser` in TrapGetCatInfo — is offset 31 correct? (IM IV says ioACUser is at offset 31 for CInfoPBRec, but only in File Sharing contexts.)
- TrapGetVolInfo writes to `pb + 30, 34, 38, 40, 42, 44, 46, 48, 52, 56, 58, 62, 64, 66, 68, 70, 72, 76, 78, 82, 86, 90` — are all of these the correct volumeParam offsets?
- TrapCatMove reads `pb + 36` as `ioNewDirID`. Confirm this matches CMovePBRec.
- HFS GetVol writes `pb + 32` and `pb + 28` as WDParam fields — offsets need verification against the WDPBRec variant.

---

## 2. FCB (File Control Block) Layout & Management

The INIT maintains its own FCB entries inside the system FCB buffer.
Any mismatch with the ROM's FCB layout will crash applications that
read FCB fields (e.g., the Resource Manager reads fcbEOF and fcbPLen).

**What to verify:**

- **FCB field offsets and sizes:** kFCBFlNum(0,4), kFCBFlags(4,1), kFCBTypByt(5,1), kFCBEOF(8,4), kFCBPLen(12,4), kFCBCrPs(16,4), kFCBVPtr(20,4), kFCBClmpSize(30,4), kFCBHostHandle(34,4), kFCBExtRec(38,12), kFCBFType(50,4), kFCBCatPos(54,4), kFCBDirID(58,4), kFCBCName(62,32). Total = 94 bytes. Cross-reference with FSEqu.a from SuperMario ROM sources and IM IV-179.
- **AllocFCB:** Scans from offset 2 (skipping the 2-byte length word). Is kFCBLen=94 correct for all System versions (6.0.x and 7.x)?
- **FCB flags byte:** TrapOpen writes `0x01` (data fork). TrapOpenRF writes `0x03` (bit 1 = resource fork per IM). Confirm these match the fcbFlags bit definitions.
- **Repurposed field:** `kFCBBTCBPtr` (offset 34) is repurposed to store the host file handle. Is anything in the ROM or System reading this field? The B*-tree control block ptr is only relevant for files on real HFS volumes, so this should be safe, but confirm.
- **FreeFCB:** Just zeros `fcbFlNum`. Should it zero the entire 94-byte entry to avoid stale data leaking into subsequent AllocFCB?
- **Missing fields:** AllocFCB doesn't set kFCBClmpSize, kFCBExtRec, kFCBFType, kFCBCatPos. Are any of these read by the ROM or Toolbox (e.g., Resource Manager, Standard File)?

---

## 3. VCB (Volume Control Block) Initialization

The VCB is a 178-byte structure allocated at init time and enqueued
in the VCB queue. The Finder, Standard File, and various File Manager
calls read fields directly from the VCB.

**What to verify:**

- **Every VCB field** at offsets 0–177 against IM IV-155 (VCB record definition). Fields currently set: qLink(0), qType(4), vcbSigWord(8), vcbCrDate(10), vcbLsMod(14), vcbAtrb(18), vcbNmFls(20), vcbNmAlBlks(26), vcbAlBlkSiz(28), vcbClpSiz(32), vcbNxtCNID(38), vcbFreeBks(42), vcbVN(44), vcbDrvNum(72), vcbDRefNum(74), vcbFSID(76), vcbVRefNum(78).
- **Fields NOT set** (left at zero from NewPtrSysClear): vcbFlags(6), vcbVBMSt(22?), vcbMAdr(24?), vcbAllocPtr(varies). Are any of these read by the ROM or System and expected non-zero?
- **vcbSigWord = 0x4244:** This is `'BD'`. Real HFS volumes use `0x4244`. Confirm this is the correct HFS signature word.
- **vcbFSID = 0x5344:** This is `'SD'`. Is this the correct way to identify an external file system? Inside Macintosh says vcbFSID should be 0 for native volumes and non-zero for foreign FS. But does anything check for specific values?
- **vcbNxtCNID = 16:** Is this safe given the host assigns CNIDs? This field is supposed to be the "next unused catalog node ID" — does the host's CNID scheme ever conflict?

---

## 4. Drive Queue Element Format

The DQE is built manually (4 flag bytes + 16-byte DrvQEl) and
enqueued via AddDrive.

**What to verify against TN#36:**

- Flag bytes at offset 0: `0x00080000` = non-ejectable disk installed. TN#36 defines the flag byte layout. Confirm bit positions.
- `qType = 1` at offset 8: This selects the "use dQDrvSz2" size variant. Confirm.
- `dQFSID = 0` at offset 14: Native HFS driver. Correct for our use case?
- Drive size calculation: 32000 × 32768 / 512 = 2,048,000 sectors. The low/high word split at offsets 16/18 — confirm the byte order matches what the ROM expects (dQDrvSz at 16 = low word, dQDrvSz2 at 18 = high word?).

---

## 5. Trap Dispatch & 68k Stub Correctness

The dynamically generated 68k stubs are critical — any bug here is
a hard crash.

**What to verify:**

- **Register save/restore masks:** MOVEM.L at save uses $E0C0 (D0-D2/A0-A1 in predecrement order), restore uses $0307 (same registers in postincrement order). Verify these are inverses of each other per 68000 MOVEM encoding.
- **D1 convention:** The flat-file stub reads D1.W as the trap word. Confirm the ROM trap dispatcher puts the trap word in D1 for OS traps (IM V, im005 "Using Assembly Language"). Note: some sources say D1, others say it's available in D1 only for certain trap classes.
- **D0 convention:** The HFS stub reads D0.W as the selector. Confirm _HFSDispatch glue code puts the selector in D0.W before trap.
- **BNE.S displacement:** At byte 18, `$660A` branches forward 10 bytes to the pass-through path at byte 30. Count: 20 + 10 = 30. Correct.
- **Return value:** Handled case does `MOVE.W 16(A0),D0` to load ioResult. Confirm A0 still points to pb after MOVEM restore. (MOVEM.L restores A0 to its pre-stub-entry value, which was A0=pb. Correct.)
- **Pass-through:** Uses `MOVE.L #oldAddr, -(SP); RTS` pattern. This is a standard tail-jump idiom. But does the original trap handler expect registers in a specific state (D1 = trap word)?

---

## 6. Position Mode Handling (Read/Write/SetFPos)

The position mode logic (`posMode & 0x03`) is duplicated in TrapRead,
TrapWrite, and TrapSetFPos. This must match IM II-83:

| Mode | Value | Meaning |
|------|-------|---------|
| fsAtMark | 0 | From current position (ignore posOffset) |
| fsFromStart | 1 | Absolute from BOF |
| fsFromLEOF | 2 | Relative to EOF |
| fsFromMark | 3 | Relative to current mark |

**What to verify:**

- **Mode 0 (fsAtMark):** The switch has no `case 0`, so mark stays at its current value (correct — read at current mark). But posOffset is ignored, while the current code doesn't explicitly ignore it. Is this correct?
- **Negative mark clamping:** `if (mark < 0) mark = 0` — Is this the correct behavior? IM says negative positions should return posErr (-40).
- **Mark > EOF in Read:** Clamped to EOF, returns kEofErr. But IM says Read should return eofErr (-39) if the mark is already at or past EOF *before* reading.
- **Mark > EOF in SetFPos:** Clamped to EOF. IM says SetFPos past EOF should return eofErr and set the mark to EOF. But what about exactly-at-EOF? Current code allows mark == eof.
- **Write past EOF:** Mark is not clamped before write — write extends the file. After write, `if (mark > eof) eof = mark`. This correctly updates the FCB's EOF. But does the host side handle sparse writes (writing at offset > current EOF)?
- **posMode masking:** `posMode & 0x03` strips the sign bit and the "cache" bit (bit 4). But IM says bit 5 (newline mode) should also be handled for Read. The current code doesn't implement newline mode at all — is this a problem?

---

## 7. Error Code Correctness

Each trap should return specific error codes for specific failure
conditions. The current code uses a limited set: kNoErr(0), kIoErr(-36),
kEofErr(-39), kTmfoErr(-42), kFnfErr(-43), kWPrErr(-44), kParamErr(-50), kNsvErr(-35).

**What to verify by routine:**

| Handler | Currently returns | Missing per IM? |
|---------|-------------------|-----------------|
| TrapOpen | kParamErr, host_err, kTmfoErr, kNoErr | fnfErr if file not found (delegated to host?), opWrErr(-49) for locked file? |
| TrapClose | kFnfErr if bad refNum | Should be rfNumErr(-51), not fnfErr |
| TrapRead | kFnfErr if bad refNum, kEofErr on short read | posErr(-40) for invalid position? |
| TrapWrite | kFnfErr if bad refNum, kIoErr on short write | wrPermErr(-61) if opened read-only? |
| TrapGetFileInfo | kParamErr if no name | fnfErr (from host), need ioFlAttrib bit 0 for locked files |
| TrapGetCatInfo | host_err | dirNFErr(-120) for bad directory? |
| TrapGetFCBInfo | kParamErr if indexed, rfNumErr for bad refNum | Currently uses -51 as literal, should use named constant |
| TrapGetVolInfo | kPassThrough if not ours | nsvErr(-35) for no such volume? |

**Specific concerns:**
- TrapClose returns `kFnfErr` for a bad refNum — should be `rfNumErr` (-51).
- TrapRead returns `kFnfErr` for a bad refNum — same issue.
- TrapGetFCBInfo hard-codes `-51` instead of using a named constant.
- Host error translation: `host_err()` negates the result register. If the host returns Mac-style negative codes, the double-negation would produce positive numbers. Verify the contract.

---

## 8. Volume Ownership Logic (IsOurVolume / ExtractLocation)

This is the gatekeeper for every trap. If it misidentifies a call as
ours or not-ours, either we corrupt another volume or we miss calls
meant for us.

**What to verify:**

- **IsOurVolume accepts vRefNum = kOurDriveNum (8).** Is drive number
  ever used as a vRefNum in normal Mac apps? Under MFS yes, under HFS
  it shouldn't be, but System 6 compat code might.
- **WD range check:** `vRefNum < -32000 && vRefNum > -32100` limits us to 100 working directories. Is this documented/sufficient? What happens if app 101 opens a WD?
- **vRefNum == 0 check:** Reads DefVCBPtr. This means after `_SetVol` to our volume, every vRefNum=0 call goes to us. Is this correct behavior for the "default volume" semantics?
- **ExtractLocation's defaultWDRefNum substitution:** Three different cases redirect to defaultWDRefNum:
  1. `vRefNum == 0` → substitute defaultWDRefNum
  2. `dirID == 0 && vRefNum == kOurVRefNum` and defaultWDRefNum differs from rootWD → substitute
  3. `dirID == 0 && vRefNum == rootWDRefNum` and defaultWDRefNum differs → substitute
  
  Case 2 and 3 look like hacks to work around apps that use the raw volume ref or root WD. Are there edge cases where dirID==0 is legitimate and means "root directory"?

---

## 9. FInfo / Finder Info Layout

FInfo is written at `pb + pb_ioFlFndrInfo` (offset 32) as:
```
+0:  fdType     (4 bytes)
+4:  fdCreator  (4 bytes)
+8:  fdFlags    (2 bytes)
+10: fdLocation (4 bytes — Point, written as 0)
+14: fdFldr     (2 bytes — written as 0)
```

**What to verify:**
- Confirm this matches the FInfo record layout in IM IV-103. The total should be 16 bytes.
- The code zeros fdLocation and fdFldr. Is this always correct, or should the host supply them? (Finder uses fdLocation to position icons.)
- TrapSetFileInfo sends only type, creator, flags to the host. It does NOT send fdLocation or fdFldr. Are these silently lost?
- FXInfo (16 bytes at pb+84 in HFS-extended calls) is always zeroed. Are any apps or INITs reading this? (FXInfo contains fdIconID, fdComment, fdPutAway.)

---

## 10. GetVolInfo Completeness

TrapGetVolInfo fills many fields. Some are written with bare offsets
(not `pb_xxx` constants), making them hard to audit.

**What to verify:**

- `pb + 42` = ioVBitMap? (Not clear from the define list.)
- `pb + 44` = ioVAllocPtr? (Not defined.)
- `pb + 56` = ioAlBlSt?
- `pb + 58` = ioVNxtCNID — written as `fileCount + 16`. This should be the next unused CNID, but it's being faked from file count. Will any software rely on this being accurate?
- `pb + 64` = ioVSigWord (HFS only). Written as `0x4244`. This duplicates vcbSigWord.
- `pb + 70` = ioVFSID. Written as `0x5344` ("SD"). Some code checks this to detect foreign file systems.
- `pb + 82` = ioVFilCnt. Same as ioVNmFls?
- `pb + 86` = ioVDirCnt. Hard-coded to 1. Should be actual directory count.
- `pb + 90` = ioVFndrInfo. 32 bytes zeroed. Finder stores boot-app info here. Zeroing is likely fine but should confirm.

---

## 11. Working Directory Semantics

WD refnum encoding: `guest WD refnum = -(hostWDRef + 32000)`

**What to verify:**

- TrapOpenWD: Reads `pb_ioWDDirID` (offset 48). Ignores `ioWDProcID` (offset 28). IM says the procID should be associated with the WD for later retrieval. Our implementation drops it.
- TrapGetWDInfo: Only handles `wdIndex == 0` (direct lookup). Indexed WD enumeration (`wdIndex > 0`) returns kNsvErr. Is any software iterating WDs?
- TrapCloseWD: Unconditionally closes. Should it check if the WD is ours? What if someone calls CloseWD on somebody else's WD ref that happens to decode into our range?
- WD refnum space: The encoding `-(wdRef + 32000)` for wdRef=1 gives -32001. For wdRef=100 gives -32100. The IsOurVolume check limits this to `> -32100`. But what if the host allocates more than 99 WDs?

---

## 12. Open / OpenRF Fork Flag Semantics

TrapOpen passes fork=0 (data), TrapOpenRF passes fork=1 (resource).

**What to verify:**

- **FCB flags for data fork:** `0x01`. IM says bit 0 of fcbFlags = file is open for writing (fcbWriteMask). But our TrapOpen always sets this, even for read-only opens. Should we check the ioPermssn field (pb+27)?
- **FCB flags for resource fork:** `0x03`. Bit 0 = write, bit 1 = resource fork (fcbRsrcBit). This is correct if writing. But again, always set even for read.
- **Permission checking:** The `ioPermssn` field (fsCurPerm=0, fsRdPerm=1, fsWrPerm=2, fsRdWrPerm=3) is completely ignored. On a real volume, opening with fsRdPerm should not allow writes. This could cause data corruption if an app assumes read-only semantics.
- **Deny modes:** HFS supports shared access with deny modes (ioPermssn bits 4-5). Completely unimplemented. Probably OK for a single-user emulator, but should at least track open modes.

---

## 13. Reentrancy & Global State

The INIT uses static globals:
- `s_nameBuf[64]` — single static name buffer
- `s_dirInfoBuf[32]` — single static DInfo transfer buffer
- `s_hexLine[80]` — hex dump buffer

**Concerns:**

- Classic Mac OS is cooperatively multitasked, so trap-level calls should not nest *within the same task*. But can a VBL task or Deferred Task call File Manager? If so, the static buffers could be corrupted mid-call.
- The host calls (reg_set/reg_command) are effectively a critical section — if a trap handler is interrupted between reg_set and reg_command, the register block is corrupted. Is this possible? (Answer: yes, if a VBL task or Time Manager completion routine does File Manager calls.)
- `get_globals()` calls `find_reg_base()` every time, which reads SonyVarsPtr. This is correct but wasteful. The regBase could be cached in the Globals struct (it is: `g->regBase`). But `get_globals()` itself can't use the cached version because it needs regBase to *find* globals.

---

## 14. Initialization Ordering & Error Recovery

The INIT's `main()` function does many steps with minimal error recovery.

**What to verify:**

- If VCB allocation fails, it bails out. But globals are already registered with `set_globals()`. Is there a dangling pointer?
- If DQE allocation fails, the volume still gets the VCB in the queue. Will Standard File find a volume with no drive?
- If any trap stub allocation fails (`MakeFlatStub/MakeHFSStub`), a log message is emitted but installation continues. This means some traps are patched and others aren't — an inconsistent state.
- The INIT detaches and locks its own code resource. But if the resource isn't found (wrong ID?), the code remains purgeable. A heap compaction later would crash.
- `InitTrapTables()` fills static arrays. If called before `SetUpA4()` establishes A4 for globals, the static arrays would be in the wrong data segment. Verify A4 is set up correctly.

---

## 15. Missing Trap Support

Some File Manager calls are patched but trivially stubbed. Others
are not patched at all.

**Patched but no-op:**
- `TrapFlushFile` — returns kNoErr without flushing. Probably fine (no OS-level caching), but should confirm the host side flushes on close.
- `TrapAllocate` — returns kNoErr without allocating. The host filesystem doesn't need pre-allocation, but apps may rely on PLen reflecting the allocation.
- `TrapFlushVol` — same.
- `TrapSetVInfo` — completely ignored. Some installers use this.

**Not patched (potential problems if called for our volume):**
- `_MountVol` ($A00F) — Not patched. If PostEvent had been used (it wasn't), this would need handling.
- `_ResolveFileIDRef` / `_CreateFileIDRef` / `_DeleteFileIDRef` — HFS selectors 0x10-0x12 defined but not in the dispatch table. These are used by the Alias Manager. Will cause kParamErr if called.
- `_GetVolParms` — IS handled, but `vMAttrib = 0` means we report no capabilities. Should we set `bHasDesktopMgr` or `bHasCatSearch`?
- `_LockRng` / `_UnlockRng` — Not patched. File sharing apps may need these.

---

## 16. Specific Code Smells & Suspicious Patterns

These are things that look wrong or fragile on inspection.

1. **TrapGetCatInfo writes `pb + 31` as ioACUser** — Is this byte-aligned correctly? The CInfoPBRec has ioACUser as a SignedByte at offset 31 only in the hFileInfo variant. For dirInfo, this is still in the ioFlAttrib range. Confirm the structure union.

2. **TrapGetVolInfo indexed walk** uses `*(Ptr *)(kVCBQHdr + 2)` for qHead — confirms VCBQHdr is a QHdr (qFlags=2, qHead=4 normally?). Actually, QHdr is { short qFlags; QElemPtr qHead; QElemPtr qTail; } so qHead is at offset 2 on 68k (short=2 bytes). Verify.

3. **host_err() negation:** `return (r == 0) ? kNoErr : -(short)r;` — This means the host returns positive error codes that get negated. Verify the host side returns positive values (it does: `fmErrToReg` returns unsigned 43, 48, etc.). So `-43` becomes `kFnfErr`. Correct.

4. **IsOurVolume accepts drive number 8.** What if another driver uses drive 8? The INIT registers drive 8 via AddDrive, so there shouldn't be a conflict. But if AddDrive fails silently...

5. **TrapGetVolInfo returns `g->defaultWDRefNum` in ioVRefNum** — Real GetVolInfo should return the volume's true refnum, not a WD. Standard File and apps use this value for subsequent calls. If they store the WD refnum and it gets closed, those stored values become dangling.

6. **`g->ejected` flag** — After TrapEject/TrapUnmountVol, the flag prevents further dispatch. But does it clean up open FCBs? Open file handles on the host side? WDs?

---

## Review Approach

For each section above:

1. **Extract the canonical specification** from Inside Macintosh (im023
   lines 25017-30929), Technical Notes, and the SuperMario ROM sources.
2. **Map every code path** in the handler to spec requirements.
3. **Flag deviations** with severity:
   - **Critical:** Will crash or corrupt data in normal use
   - **Major:** Causes incorrect behavior for common apps
   - **Minor:** Edge case or cosmetic (app would need unusual call patterns)
4. **Propose fix** inline or defer to implementation plan.

### Reference materials

| Source | Location | Used for |
|--------|----------|----------|
| Inside Macintosh: Files | `macdocs/tech_doc/im202.html` lines 25017–30929 | ParamBlockRec layouts, trap semantics, error codes |
| Inside Macintosh IV: File Mgr data structures | Same, plus Appendix A (error codes at 58604) | VCB, FCB, ParamBlockRec field offsets |
| TN#36 — Drive Queue Elements | `macdocs/tech_doc/tn405.html` lines 2351–2498 | DQE flag bytes, size fields |
| TN#87 — Error in FCBPBRec | `macdocs/tech_doc/tn405.html` lines 6273–6306 | FCBPBRec corrected offsets |
| TN#102 — HFS/MFS Compatibility | tn405.html (find exact lines) | HFS selector dispatch |
| FSEqu.a (SuperMario ROM) | `macdocs/ref/mac-rom/OS/` | FCB field equates, VCB equates |
| Mac Plus ROM disassembly | `macdocs/ref/plus-rom-listing.asm.txt` | Actual trap implementations |
| IM "Using Assembly Language" | im005, lines 3255–3734 | Trap dispatch register conventions |

### Estimated review sections

| # | Section | Risk | Effort |
|---|---------|------|--------|
| 1 | ParamBlockRec offsets | High | Large — every handler |
| 2 | FCB layout & management | High | Medium |
| 3 | VCB initialization | Medium | Small |
| 4 | DQE format | Low | Small |
| 5 | 68k stubs | Critical | Small (verify machine code) |
| 6 | Position mode handling | Medium | Small |
| 7 | Error codes | Medium | Medium |
| 8 | Volume ownership | Medium | Medium |
| 9 | FInfo layout | Medium | Small |
| 10 | GetVolInfo fields | Medium | Medium |
| 11 | WD semantics | Medium | Small |
| 12 | Open/OpenRF flags | Medium | Small |
| 13 | Reentrancy | Low | Small (assess only) |
| 14 | Init ordering | Low | Small |
| 15 | Missing traps | Low | Assessment only |
| 16 | Code smells | Varies | As encountered |
