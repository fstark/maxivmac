# SharedDrive INIT — Review Plan

Design: [REVIEW.md](REVIEW.md)

Each phase reads the relevant Inside Macintosh section, compares it
against the code in `init.c`, and produces a list of findings —
**no code changes**.

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | ParamBlockRec offsets — ioParam, fileParam, volumeParam variants | ✅ Done |
| 2 | ParamBlockRec offsets — CInfoPBRec, WDParam, FCBPBRec, CMovePBRec | ✅ Done |
| 3 | FCB layout, AllocFCB, FreeFCB, flags | ✅ Done |
| 4 | VCB initialization and field completeness | ✅ Done |
| 5 | Drive Queue Element format (TN#36) | ✅ Done |
| 6 | 68k stub machine code correctness | ✅ Done |
| 7 | Position mode & Read/Write/SetFPos semantics | ✅ Done |
| 8 | Error codes across all handlers | ✅ Done |
| 9 | Volume ownership (IsOurVolume, ExtractLocation) | ✅ Done |
| 10 | FInfo/FXInfo layout and GetVolInfo completeness | ✅ Done |
| 11 | WD semantics, Open/OpenRF flags, reentrancy, init ordering | ✅ Done |

All 11 phases completed 2026-04-24. Findings in [REVIEW_FINDINGS.md](REVIEW_FINDINGS.md).
Summary: 0 CRIT, 5 MAJOR, 14 MINOR, 7 STYLE.

Output: findings appended to `REVIEW_FINDINGS.md` after each phase.
No code changes. No commits to source files.

Build gate: N/A (read-only review)
Test gate: N/A

---

## Phase 1 — ParamBlockRec offsets: ioParam, fileParam, volumeParam

Read IM File Manager (im023, lines 25017–30929) to extract the
canonical ParamBlockRec field offsets for ioParam, fileParam, and
volumeParam variants. Compare every `pb + XX` access in the following
handlers against the spec.

### 1.1 — Read canonical ioParam layout from IM

Read `macdocs/tech_doc/im202.html` lines 25017–25500 (File Manager
intro and data structure definitions) to find the ioParam variant
field table. Record offset, size, and signedness of each field.

Cross-reference with the `#define pb_xxx` constants at the top of
`init.c` (lines 90–150).

### 1.2 — Audit ioParam handlers

For each of these handlers, list every `pb + offset` access and mark
✅ (matches spec) or ❌ (deviates):

- **TrapRead** (line ~555): ioRefNum, ioBuffer, ioReqCount, ioActCount, ioPosMode, ioPosOffset
- **TrapWrite** (line ~605): same set plus EOF update
- **TrapGetEOF** (line ~665): ioRefNum, ioMisc
- **TrapSetEOF** (line ~672): ioRefNum, ioMisc
- **TrapGetFPos** (line ~685): ioRefNum, ioPosOffset, ioReqCount, ioActCount
- **TrapSetFPos** (line ~695): ioRefNum, ioPosMode, ioPosOffset
- **TrapAllocate** (line ~710): (stub — just return noErr)

### 1.3 — Read canonical fileParam layout

Read IM im023 for the fileParam / HFileParam variant. Extract field
offsets for ioFDirIndex through ioFlMdDat, plus HFS extensions
(ioFlBkDat, ioFlXFndrInfo, ioFlParID, ioFlClpSiz).

### 1.4 — Audit fileParam handlers

- **TrapGetFileInfo** (line ~775): every pb field written
- **TrapSetFileInfo** (line ~825): every pb field read
- **TrapCreate** (line ~975): pb fields read (location-only)
- **TrapDelete** (line ~985): pb fields read
- **TrapRename** (line ~995): pb fields read, including ioMisc for new name

### 1.5 — Read canonical volumeParam / HVolumeParam layout

Read IM im023 for PBGetVInfo / PBHGetVInfo result record. Extract
every field offset from ioVolIndex through ioVFndrInfo.

### 1.6 — Audit TrapGetVolInfo

**TrapGetVolInfo** (line ~1015) writes fields using a mix of `pb_xxx`
constants and bare numeric offsets. Map every `pb + N` to the
canonical field name and confirm correctness. Pay special attention to:

- `pb + 42` (ioVBitMap)
- `pb + 44` (ioVAllocPtr)
- `pb + 56` (ioAlBlSt)
- `pb + 58` (ioVNxtCNID)
- `pb + 64..90` (HFS-only extension)

### 1.7 — Record findings

Append findings to `REVIEW_FINDINGS.md` in this format:

```markdown
## Phase 1 — ParamBlockRec offsets (ioParam, fileParam, volumeParam)

### <handler name>
| Field | Expected (IM) | Code uses | Status | Notes |
|-------|---------------|-----------|--------|-------|
| ioRefNum | offset 24, INTEGER | pb+24, short | ✅ | |
| ... | ... | ... | ... | ... |

### Issues found
1. **[SEV] description** — consequence
```

Severity tags: `[CRIT]` (crash/corruption), `[MAJOR]` (wrong behavior),
`[MINOR]` (edge case), `[STYLE]` (naming/consistency).

---

## Phase 2 — ParamBlockRec offsets: CInfoPBRec, WDParam, FCBPBRec, CMovePBRec

### 2.1 — Read canonical CInfoPBRec layout

Read IM im023 for PBGetCatInfo. The CInfoPBRec is a union of
hFileInfo and dirInfo variants sharing a common header. Extract
both variant field tables.

### 2.2 — Audit TrapGetCatInfo

**TrapGetCatInfo** (line ~845) has two code paths: directory (flags &
0x10) and file. For each path, list every `pb + offset` access.

Special attention:
- `pb + 31` written as ioACUser (byte)
- `pb + pb_ioDrNmFls` at offset 52 (INTEGER)
- `pb + pb_ioDrDirID` at offset 48 = same as pb_ioFlNum
- Dir path: DInfo at 32 (16-bytes), DXInfo at 84 (16 bytes)
- File path: all FInfo fields, then HFS extensions

### 2.3 — Audit TrapSetCatInfo

Check that reads from pb match the CInfoPBRec layout. Pay attention
to the directory case using `pb + pb_ioDrDirID` vs `pb + pb_ioDrUsrWds`.

### 2.4 — Read canonical WDParam (WDPBRec) layout

Read IM im023 for PBOpenWD, PBCloseWD, PBGetWDInfo. Extract the
WDPBRec field offsets.

### 2.5 — Audit WD handlers

- **TrapOpenWD** (line ~1200): reads ioWDDirID (48)
- **TrapCloseWD** (line ~1215): reads ioVRefNum (22)
- **TrapGetWDInfo** (line ~1225): reads/writes ioVRefNum, ioWDIndex, ioWDVRefNum, ioWDDirID, ioWDProcID, ioNamePtr
- **TrapGetVol** (line ~1105): HFS path writes ioWDVRefNum (32), ioWDProcID (28), ioWDDirID (48)
- **TrapSetVol** (line ~1130): reads ioWDDirID (48) in HFS path

### 2.6 — Read canonical FCBPBRec layout

Read TN#87 (`macdocs/tech_doc/tn405.html` lines 6273–6306) for the
corrected FCBPBRec offsets. Cross-reference with the `pb_ioFCBxxx`
defines in init.c.

### 2.7 — Audit TrapGetFCBInfo

Check every pb field written. Verify the FCBFlags packing into
ioFCBFlags (combining fcbFlags byte and fcbTypByt into a single
INTEGER at pb+36).

### 2.8 — Read canonical CMovePBRec layout

Read IM im023 for PBCatMove. Find the ioNewDirID field offset.

### 2.9 — Audit TrapCatMove

Verify `pb + 36` is correct for ioNewDirID. Check ioNamePtr and
ioDirID reads.

### 2.10 — Record findings

Append to `REVIEW_FINDINGS.md`.

---

## Phase 3 — FCB layout, AllocFCB, FreeFCB, flags

### 3.1 — Read canonical FCB layout

Read IM IV-179 (within im023) for the FCB record layout. Also read
`macdocs/ref/mac-rom/OS/` for FSEqu.a or equivalent equate file to
find the authoritative field offsets.

Search the Mac Plus ROM disassembly for FCB-related routines:
```
grep -n 'FCB\|fcbFlNum\|fcbEOF\|fcbPLen\|fcbVPtr' macdocs/ref/plus-rom-listing.asm.txt | head -40
```

### 3.2 — Verify kFCBLen = 94

Check whether System 6 and System 7 use the same FCB record size.
Search for FCBsLen or equivalent in the ROM sources.

### 3.3 — Audit AllocFCB

Verify:
- Scan start at offset 2 (past length word) is correct
- Fields set: FlNum, Flags, TypByt, EOF, PLen, CrPs, VPtr
- Fields NOT set but potentially read: ClmpSize(30), ExtRec(38), FType(50), CatPos(54), DirID(58), CName(62)
- Whether the Resource Manager reads any of the unset fields

### 3.4 — Audit FreeFCB

Check if zeroing only FlNum is sufficient, or if stale data in other
fields could leak.

### 3.5 — Verify fcbFlags semantics

Cross-reference:
- Bit 0 = fcbWriteMask (write access)
- Bit 1 = fcbRsrcBit (resource fork)
- Bit 4 = fcbSharedBit?

TrapOpen sets 0x01, TrapOpenRF sets 0x03. Verify these match.

### 3.6 — Verify kFCBBTCBPtr repurpose safety

Search the ROM disassembly for reads of FCB offset 34:
```
grep -n 'fcbBTCB\|34(A' macdocs/ref/plus-rom-listing.asm.txt | head -20
```

Check if any ROM code reads this field for non-B*tree purposes.

### 3.7 — Record findings

Append to `REVIEW_FINDINGS.md`.

---

## Phase 4 — VCB initialization and field completeness

### 4.1 — Read canonical VCB layout

Read IM im023 for the VCB record (should be near the data structures
section, within lines 25017–26000). Extract all field offsets from
0 to 177.

Also search the ROM disassembly for VCB-related routines to
understand which fields the ROM actually reads:
```
grep -n 'vcb\|VCB' macdocs/ref/plus-rom-listing.asm.txt | head -40
```

### 4.2 — Audit VCB initialization in main()

Map every field written in `main()` (line ~1790–1860) to the canonical
layout. Flag any fields left at zero that should be non-zero.

### 4.3 — Check vcbSigWord value

Verify 0x4244 (`'BD'`) is the correct HFS signature word.

### 4.4 — Check vcbFSID semantics

Read IM for vcbFSID meaning. Is 'SD' (0x5344) a valid choice? Does
Standard File or the Finder check vcbFSID?

### 4.5 — Check vcbNxtCNID interaction with host

The host assigns CNIDs starting from 16. The VCB says vcbNxtCNID=16.
Verify no conflict if the host has more than 0 files.

### 4.6 — Record findings

Append to `REVIEW_FINDINGS.md`.

---

## Phase 5 — Drive Queue Element (TN#36)

### 5.1 — Read TN#36

Read `macdocs/tech_doc/tn405.html` lines 2351–2498 (TN#36 — Drive
Queue Elements). Extract the DQE structure layout including the 4
flag bytes before qLink.

### 5.2 — Audit DQE construction

Verify:
- Flag byte layout at offsets 0–3 (0x00080000)
- qType at offset 8
- dQFSID at offset 14
- dQDrvSz / dQDrvSz2 at offsets 16/18 and byte order
- AddDrive parameter passing (dqe + 4)

### 5.3 — Record findings

Append to `REVIEW_FINDINGS.md`.

---

## Phase 6 — 68k stub machine code

### 6.1 — Read trap dispatch conventions

Read IM im005 (`macdocs/tech_doc/im202.html` lines 3255–3734) for:
- OS trap calling convention (A0 = pb, D0 = result)
- Which register holds the trap word after dispatch
- HFSDispatch convention (D0.W = selector)

### 6.2 — Verify MOVEM register masks

68000 MOVEM encoding:
- Predecrement (-(An)): bit 15 = D0, bit 14 = D1, ..., bit 7 = A0, bit 6 = A1, ...
- Postincrement ((An)+): bit 0 = D0, bit 1 = D1, ..., bit 8 = A0, bit 9 = A1, ...

For D0-D2/A0-A1:
- Predecrement: D0(15) + D1(14) + D2(13) + A0(7) + A1(6) = 0xE0C0 ✓?
- Postincrement: D0(0) + D1(1) + D2(2) + A0(8) + A1(9) = 0x0307 ✓?

Verify these are correct.

### 6.3 — Verify branch displacement

At stub offset 18, `BNE.S $0A` (opcode 0x660A):
- PC after BNE.S = 20 (18 + 2)
- Target = 20 + 10 = 30
- Byte 30 starts the pass-through MOVEM.L restore

Count the intervening bytes at 20–29:
- 20: MOVEM.L (SP)+  (4 bytes)
- 24: MOVE.W 16(A0),D0  (4 bytes)
- 28: RTS  (2 bytes)
Total = 10 bytes → target at 30. Verify.

### 6.4 — Verify the pass-through path

After MOVEM restore at byte 30, the old trap address is pushed and
RTS jumps there. Check whether the old handler expects:
- D1.W = original trap word (flat stubs)
- D0.W = original selector (HFS stub)
- Any other register state

The MOVEM restore brings D0-D2/A0-A1 back to pre-stub values, so
D1 (flat) and D0 (HFS) are restored. Verify this is correct.

### 6.5 — Verify return path for handled traps

The handled path does:
1. MOVEM.L (SP)+, D0-D2/A0-A1  (restore caller's registers)
2. MOVE.W 16(A0), D0  (D0 = pb->ioResult)
3. RTS

This returns D0 = ioResult to the trap caller. Confirm this matches
the OS trap return convention (D0.W = result code).

### 6.6 — Record findings

Append to `REVIEW_FINDINGS.md`.

---

## Phase 7 — Position mode & Read/Write/SetFPos semantics

### 7.1 — Read canonical Read/Write/SetFPos docs

Read IM im023 sections on PBRead, PBWrite, PBSetFPos, PBGetFPos.
Extract:
- posMode values (0 = fsAtMark, 1 = fsFromStart, 2 = fsFromLEOF, 3 = fsFromMark)
- Return value semantics on EOF, invalid position, zero-length request
- newline mode (bit 7 of ioPosMode for Read)
- What happens when mark > EOF before a read

### 7.2 — Audit TrapRead mark calculation

Compare the code's position calculation and clamping against the spec.
Specifically:
- Mode 0 (no case in switch): does falling through correctly use current mark?
- Negative mark → clamped to 0 vs spec says posErr(-40)?
- Mark ≥ EOF before read → eofErr per spec?
- reqCount ≤ 0 early return: correct?

### 7.3 — Audit TrapWrite mark calculation

Compare against spec:
- Mode 0 handling
- Mark not clamped to EOF (correct for write)
- FCB EOF/PLen update after extending write
- What if reqCount ≤ 0?

### 7.4 — Audit TrapSetFPos

Compare against spec:
- Mark clamped to EOF → should return eofErr?
- Exactly-at-EOF behavior
- posErr for negative result?

### 7.5 — Compare three implementations for consistency

The posMode switch is duplicated in TrapRead, TrapWrite, TrapSetFPos.
Check that all three handle modes 0–3 identically (or differ only
where the spec requires different behavior).

### 7.6 — Record findings

Append to `REVIEW_FINDINGS.md`.

---

## Phase 8 — Error codes across all handlers

### 8.1 — Read canonical error codes

Read IM Appendix A (`macdocs/tech_doc/im202.html` lines 58604–58920)
for the complete list of File Manager result codes. Key codes:

| Code | Name | Meaning |
|------|------|---------|
| 0 | noErr | |
| -33 | dirFulErr | directory full |
| -34 | dskFulErr | disk full |
| -35 | nsvErr | no such volume |
| -36 | ioErr | I/O error |
| -37 | bdNamErr | bad filename |
| -38 | fnOpnErr | file not open |
| -39 | eofErr | end of file |
| -40 | posErr | invalid position |
| -42 | tmfoErr | too many files open |
| -43 | fnfErr | file not found |
| -44 | wPrErr | write-protected |
| -45 | fLckdErr | file locked |
| -47 | fBsyErr | file busy |
| -48 | dupFNErr | duplicate filename |
| -49 | opWrErr | file already open for writing |
| -50 | paramErr | bad parameter |
| -51 | rfNumErr | bad reference number |
| -61 | wrPermErr | write permission denied |
| -120 | dirNFErr | directory not found |

### 8.2 — Audit each handler's error returns

For every handler in init.c, list each error return and compare
against the spec:

- What error should be returned for a bad refnum? (rfNumErr, not fnfErr)
- What error should be returned for mark past EOF? (eofErr)
- What error should Open return for "file not found"? (fnfErr — delegated to host)
- What error should Write return for read-only file? (wrPermErr — not implemented)

### 8.3 — Audit host_err() translation

The function does `-(short)r` where `r` is the host result register.
The host's `fmErrToReg()` returns positive values (43, 48, etc.).
So `-(short)43 = -43 = kFnfErr`. Verify this chain is correct and
no double-negation can occur.

### 8.4 — Record findings

Append to `REVIEW_FINDINGS.md`.

---

## Phase 9 — Volume ownership (IsOurVolume, ExtractLocation)

### 9.1 — Read IM on vRefNum semantics

Read IM im023 sections on volume reference numbers, working directory
reference numbers, and the relationship between them. Key questions:
- When is vRefNum a volume ref vs a WD ref?
- When is drive number used as vRefNum?
- What does vRefNum = 0 mean?

### 9.2 — Audit IsOurVolume

Walk through each condition:
1. `vRefNum == kOurVRefNum (-32000)` — direct volume match
2. `vRefNum == kOurDriveNum (8)` — drive number used as vol ref
3. `vRefNum < -32000 && > -32100` — our WD range
4. `vRefNum == 0` with DefVCBPtr check — default volume

For each, determine if there are false positives (claiming calls that
aren't ours) or false negatives (missing calls that are ours).

### 9.3 — Audit ExtractLocation

Trace through the three substitution cases for defaultWDRefNum.
For each, construct a concrete scenario where it activates and
check whether the behavior is correct per IM.

### 9.4 — Check the WD range limit

`> -32100` limits us to 99 WDs (refnums -32001 to -32099). If the
host allocates WD ref 100, the encoded vRefNum is -32100, which
IsOurVolume rejects. Is 99 enough? What breaks?

### 9.5 — Record findings

Append to `REVIEW_FINDINGS.md`.

---

## Phase 10 — FInfo/FXInfo layout, GetVolInfo completeness

### 10.1 — Read canonical FInfo and FXInfo layouts

Read IM for the FInfo record (fdType, fdCreator, fdFlags, fdLocation,
fdFldr) and FXInfo record (fdIconID, fdUnused, fdScript, fdXFlags,
fdComment, fdPutAway).

### 10.2 — Audit FInfo writes in TrapGetFileInfo and TrapGetCatInfo

Verify sub-field offsets within the 16-byte FInfo at pb+32:
- +0: fdType(4), +4: fdCreator(4), +8: fdFlags(2), +10: fdLocation(4), +14: fdFldr(2)

Check whether zeroing fdLocation and fdFldr causes Finder misbehavior
(icon stacking, wrong folder display).

### 10.3 — Audit FInfo reads in TrapSetFileInfo and TrapSetCatInfo

Verify that the host receives the correct fields. Check if fdLocation
and fdFldr are silently dropped.

### 10.4 — Audit TrapGetVolInfo bare offsets

Map every bare `pb + N` access (N = 30, 34, 38, 40, 42, 44, 46, 56,
58, 64, 66, 68, 70, 72, 76, 78, 82, 86, 90) to the canonical
HVolumeParam field name. Flag any mismatches.

### 10.5 — Check ioVDirCnt hard-coded to 1

Real volumes report actual directory count. Ours hard-codes 1. Could
any software (Finder, DiskTop, etc.) rely on this being accurate?

### 10.6 — Record findings

Append to `REVIEW_FINDINGS.md`.

---

## Phase 11 — WD semantics, Open/OpenRF flags, reentrancy, init ordering

### 11.1 — Audit WD operations against IM

Read IM im023 for PBOpenWD, PBCloseWD, PBGetWDInfo semantics:
- Should OpenWD record ioWDProcID?
- Should GetWDInfo support indexed enumeration (wdIndex > 0)?
- Should CloseWD validate the WD belongs to the caller?

### 11.2 — Audit Open/OpenRF permission handling

Read IM im023 for PBOpen / PBHOpen:
- Is ioPermssn (pb+27) checked?
- What fcbFlags bits should be set for read-only vs read-write?
- What error is returned when opening a file already open for writing?

### 11.3 — Assess reentrancy risk

Determine:
- Can VBL tasks or Deferred Tasks call File Manager traps?
  (Read IM im053, im017 for restrictions.)
- If yes, the static buffers (s_nameBuf, s_dirInfoBuf) and the
  register block are vulnerable.
- Assess practical risk: is this a theoretical concern or has it
  caused real bugs?

### 11.4 — Audit init ordering and error recovery

Walk through main() and check:
- If set_globals() is called before VCB allocation, is there a
  dangling-pointer risk on bail?
- If DQE allocation fails, does Standard File still work?
- If MakeFlatStub/MakeHFSStub fails, is the partial-patch state safe?
- Is the INIT resource correctly detached and locked?

### 11.5 — Check for missing trap coverage

List traps that are NOT patched but could be called for our volume:
- _MountVol, _Offline
- FileID traps (0x10–0x12)
- LockRng/UnlockRng
- Any others found in the trap table

### 11.6 — Record all findings

Append final section to `REVIEW_FINDINGS.md`.

---

## Output Format

After all 11 phases, `REVIEW_FINDINGS.md` should contain:

```markdown
# SharedDrive INIT — Review Findings

Generated: <date>
Source: macsrc/shareddrive/init.c (1913 lines)
Reference: Inside Macintosh (im023), TN#36, TN#87, TN#102

## Summary

| Severity | Count |
|----------|-------|
| CRIT | N |
| MAJOR | N |
| MINOR | N |
| STYLE | N |

## Phase 1 — ParamBlockRec offsets (ioParam, fileParam, volumeParam)
...findings...

## Phase 2 — ParamBlockRec offsets (CInfoPBRec, WDParam, FCBPBRec)
...findings...

(etc.)
```

Each finding has:
- **Severity tag**: `[CRIT]`, `[MAJOR]`, `[MINOR]`, `[STYLE]`
- **Location**: handler name and approximate line
- **Expected behavior**: what IM says
- **Actual behavior**: what init.c does
- **Consequence**: what goes wrong in practice
