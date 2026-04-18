# Plan: Audit & Complete show-in / show-out Fields in traps.def

## Goal

Ensure every File Manager trap (and HFS subtrap) in `traps.def` shows all
fields that are useful for debugging, cross-referenced against the Inside
Macintosh parameter block definitions.

## Principles

- **show-in**: Every input field that disambiguates *what* the call is doing.
  Include name, volume, directory index, dir ID — anything that changes
  the call's behavior.
- **show-out**: Every output field a debugger would want to see — result
  code, returned name, attributes, finder info, sizes, dates, parent ID.
- **Omit**: Internal/allocator fields (ioBuffer, ioVBitMap, ioAllocPtr,
  ioFlStBlk, ioFlPyLen, ioFlRStBlk, ioFlRPyLen) and fillers.
- **Consistency**: If GetFileInfo shows `ioFDirIndex` and `ioFlLgLen`,
  PBGetCatInfo (its HFS superset) must show at least as much.

## Phase 1 — Classic File Manager Traps

### 1.1 GetFileInfo (A00C) — FileParam

**Current:**
```
show-in  pb ioNamePtr ioVRefNum ioFDirIndex
show-out pb ioResult ioNamePtr ioFlFndrInfo ioFlNum ioFlLgLen ioFlRLgLen
```
**Change:** Add `ioFlAttrib` and `ioFlCrDat` `ioFlMdDat` to show-out.
```
show-in  pb ioNamePtr ioVRefNum ioFDirIndex
show-out pb ioResult ioNamePtr ioFlAttrib ioFlFndrInfo ioFlNum ioFlLgLen ioFlRLgLen ioFlCrDat ioFlMdDat
```
**Rationale:** ioFlAttrib bit 0 = locked, bit 4 = directory (impossible here
but useful to confirm). Dates are always useful for debugging Finder issues.

---

### 1.2 Create (A008) — FileParam

**Current:**
```
show-in  pb ioNamePtr ioVRefNum
show-out pb ioResult
```
**Change:** No change needed. Create takes minimal inputs; the directory
context comes from ioVRefNum (which may be a WD refnum encoding the dir).

---

### 1.3 Delete (A009) — FileParam

**Current:**
```
show-in  pb ioNamePtr ioVRefNum
show-out pb ioResult
```
**Change:** No change needed. Same reasoning as Create.

---

### 1.4 SetFileInfo (A00D) — FileParam

**Current:**
```
show-in  pb ioNamePtr ioVRefNum ioFlFndrInfo
show-out pb ioResult
```
**Change:** No change needed. Shows the finder info being set, which is the
primary payload.

---

### 1.5 MountVol (A00F) — VolumeParam

**Current:** No show-in/show-out.
**Change:** Add show-out.
```
show-out pb ioResult ioVRefNum
```
**Rationale:** Knowing whether mount succeeded and what VRefNum was assigned
is essential.

---

### 1.6 Open (A000), OpenRF (A00A) — IOParam

**Current:**
```
show-in  pb ioNamePtr ioVRefNum
show-out pb ioResult ioRefNum
```
**Change:** Add `ioPermssn` to show-in.
```
show-in  pb ioNamePtr ioVRefNum ioPermssn
show-out pb ioResult ioRefNum
```
**Rationale:** Permission mode (read/write/shared) is critical when debugging
file-open failures or sharing violations.

---

## Phase 2 — HFS Dispatch Subtraps

### 2.1 PBGetCatInfo (0x09) — CInfoPBRec

**Current:**
```
show-in  pb ioNamePtr ioDirID
show-out pb ioResult ioFlAttrib ioFlFndrInfo
```
**Change:** Add `ioVRefNum` and `ioFDirIndex` to show-in; add `ioNamePtr`,
`ioFlLgLen`, `ioFlRLgLen`, `ioFlParID`, `ioFlCrDat`, `ioFlMdDat` to show-out.
```
show-in  pb ioNamePtr ioVRefNum ioFDirIndex ioDirID
show-out pb ioResult ioNamePtr ioFlAttrib ioFlFndrInfo ioFlLgLen ioFlRLgLen ioFlCrDat ioFlMdDat ioFlParID
```
**Rationale:** ioFDirIndex completely changes lookup behavior (positive =
iterate, 0 = by name, negative = directory by ID). ioVRefNum identifies the
volume/WD. Output should match GetFileInfo at minimum, plus ioFlParID for
path reconstruction.

---

### 2.2 PBSetCatInfo (0x0A) — CInfoPBRec

**Current:**
```
show-in  pb ioNamePtr ioDirID
show-out pb ioResult
```
**Change:** Add `ioVRefNum` and `ioFlFndrInfo` to show-in.
```
show-in  pb ioNamePtr ioVRefNum ioDirID ioFlFndrInfo
show-out pb ioResult
```
**Rationale:** Shows what's being set and on which volume.

---

### 2.3 PBCatMove (0x05) — CInfoPBRec (actually CMovePBRec)

**Current:**
```
show-in  pb ioNamePtr ioDirID
show-out pb ioResult
```
**Change:** Add `ioVRefNum` to show-in.  
*(Note: the real CMovePBRec also has ioNewName at offset 28 and ioNewDirID
at offset 36, but CInfoPBRec doesn't define those fields. Adding them would
require a CMovePBRec type in types.def — flagged as a follow-up.)*
```
show-in  pb ioNamePtr ioVRefNum ioDirID
show-out pb ioResult
```

---

### 2.4 PBDirCreate (0x06) — HFileInfo

**Current:**
```
show-in  pb ioNamePtr ioDirID
show-out pb ioResult ioDirID
```
**Change:** Add `ioVRefNum` to show-in.
```
show-in  pb ioNamePtr ioVRefNum ioDirID
show-out pb ioResult ioDirID
```

---

### 2.5 PBOpenWD (0x01) — WDParam

**Current:**
```
(no show-in)
show-out pb ioResult
```
**Change:** Add show-in and expand show-out.
```
show-in  pb ioNamePtr ioVRefNum ioWDProcID ioWDDirID
show-out pb ioResult ioVRefNum
```
**Rationale:** All four input fields determine what WD is opened. The
returned ioVRefNum is the WD refnum to be used in subsequent calls.

---

### 2.6 PBCloseWD (0x02) — WDParam

**Current:**
```
(no show-in)
show-out pb ioResult
```
**Change:** Add show-in.
```
show-in  pb ioVRefNum
show-out pb ioResult
```
**Rationale:** Need to know which WD is being closed.

---

### 2.7 PBGetWDInfo (0x07) — WDParam

**Current:**
```
(no show-in)
show-out pb ioResult ioWDProcID ioWDVRefNum
```
**Change:** Add show-in and `ioWDDirID` to show-out.
```
show-in  pb ioVRefNum ioWDIndex
show-out pb ioResult ioWDProcID ioWDVRefNum ioWDDirID
```
**Rationale:** ioVRefNum and ioWDIndex are the lookup keys. ioWDDirID in
output tells which directory the WD points to.

---

### 2.8 PBGetFCBInfo (0x08) — FCBPBRec

**Current:**
```
(no show-in)
show-out pb ioResult
```
**Change:** Add show-in and useful output fields.
```
show-in  pb ioRefNum ioFCBIndx
show-out pb ioResult ioNamePtr ioFCBFlNm ioFCBFlags ioFCBEOF ioFCBVRefNum ioFCBParID
```
**Rationale:** The two lookup modes are by refnum (ioFCBIndx=0) or by index
(ioFCBIndx>0). The output fields are the most useful: file number, flags
(read/write/resource fork), logical EOF, volume, and parent directory.

---

### 2.9 PBGetVolParms (0x30) — HFileInfo (mistyped?)

**Current:**
```
(no show-in)
show-out pb ioResult
```
**Change:** No change. The actual data is returned in a buffer (ioBuffer),
not in the parameter block fields. The current definition is adequate.

---

### 2.10 PBSetVInfo (0x0B) — VolumeParam

**Current:**
```
(no show-in)
show-out pb ioResult
```
**Change:** Add show-in.
```
show-in  pb ioNamePtr ioVRefNum
show-out pb ioResult
```

---

## Phase 3 — Follow-up (not in this change)

- Add `CMovePBRec` to `types.def` with `ioNewName` (offset 28) and
  `ioNewDirID` (offset 36) so PBCatMove can show the destination.
- Consider adding `ioPermssn` field to IOParam if not already present
  (it is — at offset 27).
- Consider HFS-extended versions of Open/Create/Delete/Rename if those
  are traced separately from the classic traps.

## Execution

Each change is a single-line edit in `traps.def`. Apply all changes,
then build and run the self-test to confirm no parse errors.
