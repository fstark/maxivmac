# SharedDrive Logging Overhaul — Plan

**COMPLETED** — All 3 phases done. Commits: e0a9496..5fbef64

- Phase 1: Host infrastructure (`extfs_log.h/cpp`, `kExtFSLogTrap` dispatch)
- Phase 2: Guest migration (replaced all `dbg_log`/`dbg_hexdump` in DispatchFlat/DispatchHFS with `log_trap()`)
- Phase 3: Cleanup (hexdumps already removed in Phase 2, no-op)

---

## Original Goal

Replace the ad-hoc `dbg_log` calls in init.c with structured,
human-friendly trace logging.  Every trap we intercept should produce
a clear log line showing:

1. Which trap was called and what the caller asked for
2. Whether we handled it or passed it through to the ROM
3. What we changed in the param block (if anything)
4. The result (with a human-readable error name, not just a number)

## Design: do the heavy lifting on the host

The guest (THINK C, old C, 68k) is the wrong place to build pretty
strings.  Instead:

- **Guest:** sends a single structured RPC per trap invocation
  — just numbers, no string formatting.
- **Host (C++):** receives those numbers and pretty-prints
  everything, reading the param block directly from guest RAM.

This keeps the guest code simple and small, and gives us full C++
formatting power on the host.

## New host command: `ExtFSLogTrap` (`0x020F`)

```
p0 = trapWord  (e.g. $A000 for _Open, or $0009 for HFS GetCatInfo)
p1 = pb address in guest RAM
p2 = action:
       0 = PASS-THROUGH (not our volume, forwarding to ROM)
       1 = HANDLED (we processed it)
       2 = HANDLED+ERROR (we processed it but returned an error)
p3 = OSErr result (only meaningful when action=1 or 2)
p4 = flags:
       bit 0 = isHFS (trap word had bit 9 set / was _HFSDispatch)
       bit 1 = pb was modified (host should dump the output fields)
```

That's a single register-block write from the guest — five longs
plus a command word.  No format strings, no string copies.

## Guest-side changes (init.c)

### New helper macro

```c
static void log_trap(char *base, unsigned short trapWord,
    char *pb, short action, short err, short flags)
{
    reg_set(base, 0, (unsigned long)trapWord);
    reg_set(base, 1, (unsigned long)pb);
    reg_set(base, 2, (unsigned long)action);
    reg_set(base, 3, (unsigned long)(short)err);
    reg_set(base, 4, (unsigned long)flags);
    reg_command(base, 0x020F);
}

#define LOG_PASSTHRU  0
#define LOG_HANDLED   1
#define LOG_ERROR     2

#define LOG_F_HFS     0x0001
#define LOG_F_PBMOD   0x0002
```

### Usage pattern in dispatchers

Before (current code — many dbg_log calls per trap):
```c
case 0x00: /* _Open */
    dbg_log2(g->regBase, "SD _Open vr=%ld nm=%S", ...);
    err = DoOpen(pb, g->regBase, g->vcb, isHFS);
    dbg_log1(g->regBase, "SD _Open -> %ld", (long)err);
    *(short *)(pb + pb_ioResult) = err;
```

After (one call does everything):
```c
case 0x00: /* _Open */
    err = DoOpen(pb, g->regBase, g->vcb, isHFS);
    *(short *)(pb + pb_ioResult) = err;
    log_trap(g->regBase, trapWord, pb,
        err ? LOG_ERROR : LOG_HANDLED, err,
        (isHFS ? LOG_F_HFS : 0) | LOG_F_PBMOD);
```

For pass-through (not-our-volume):
```c
if (!IsOurVolume(vRefNum)) {
    log_trap(g->regBase, trapWord, pb, LOG_PASSTHRU, 0, 0);
    RestoreA4(); return 1;
}
```

### What gets removed

- All the per-trap `dbg_log`/`dbg_log1`/`dbg_log2` calls
- The `dbg_hexdump` calls in DispatchHFS
- The ad-hoc "SD pass-thru" logging

The `dbg_log` family and `dbg_hexdump` stay available for one-off
debugging, but the normal flow uses only `log_trap`.

## Host-side changes (extn_extfs.cpp)

### Trap name table

```cpp
static const char *flatTrapName(uint16_t trapNum) {
    switch (trapNum) {
        case 0x00: return "Open";
        case 0x01: return "Close";
        case 0x02: return "Read";
        case 0x03: return "Write";
        case 0x07: return "GetVolInfo";
        case 0x08: return "Create";
        case 0x09: return "Delete";
        case 0x0A: return "OpenRF";
        case 0x0B: return "Rename";
        case 0x0C: return "GetFileInfo";
        case 0x0D: return "SetFileInfo";
        case 0x0E: return "UnmountVol";
        case 0x10: return "Allocate";
        case 0x11: return "GetEOF";
        case 0x12: return "SetEOF";
        case 0x13: return "FlushVol";
        case 0x14: return "GetVol";
        case 0x15: return "SetVol";
        case 0x17: return "Eject";
        case 0x18: return "GetFPos";
        case 0x44: return "SetFPos";
        case 0x45: return "FlushFile";
        default:   return "???";
    }
}

static const char *hfsTrapName(uint16_t selector) {
    switch (selector) {
        case 0x01: return "OpenWD";
        case 0x02: return "CloseWD";
        case 0x05: return "CatMove";
        case 0x06: return "DirCreate";
        case 0x07: return "GetWDInfo";
        case 0x08: return "GetFCBInfo";
        case 0x09: return "GetCatInfo";
        case 0x0A: return "SetCatInfo";
        case 0x0B: return "SetVInfo";
        case 0x30: return "GetVolParms";
        default:   return "???";
    }
}
```

### Error name table

```cpp
static const char *osErrName(int16_t err) {
    switch (err) {
        case   0: return "noErr";
        case -33: return "dirFulErr (directory full)";
        case -34: return "dskFulErr (disk full)";
        case -35: return "nsvErr (no such volume)";
        case -36: return "ioErr (I/O error)";
        case -37: return "bdNamErr (bad name)";
        case -38: return "fnOpnErr (file not open)";
        case -39: return "eofErr (end of file)";
        case -40: return "posErr (position error)";
        case -42: return "tmfoErr (too many files open)";
        case -43: return "fnfErr (file not found)";
        case -44: return "wPrErr (volume is locked)";
        case -45: return "fLckdErr (file is locked)";
        case -46: return "vLckdErr (volume is software locked)";
        case -47: return "fBsyErr (file is busy)";
        case -48: return "dupFNErr (duplicate filename)";
        case -49: return "opWrErr (file already open for writing)";
        case -50: return "paramErr (bad parameter)";
        case -51: return "rfNumErr (bad refnum)";
        case -58: return "extFSErr (external FS error)";
        case -120: return "dirNFErr (directory not found)";
        default:  return NULL;  /* caller prints number */
    }
}
```

### Param block pretty-printer

The host reads the param block from guest RAM using `get_vm_byte` /
`get_vm_word` / `get_vm_long`.  Based on the trap, it knows which
variant of the param block to display.

For each trap, show the **relevant** fields only:

| Trap | Fields shown |
|------|-------------|
| Open, OpenRF | ioNamePtr→name, ioVRefNum, ioDirID (if HFS) |
| Close | ioRefNum |
| Read, Write | ioRefNum, ioReqCount, ioPosMode, ioPosOffset; after: ioActCount, ioPosOffset |
| GetFileInfo, SetFileInfo | ioNamePtr→name, ioVRefNum, ioDirID, ioFDirIndex; after: type/creator, sizes |
| GetCatInfo | ioVRefNum, ioDirID, ioFDirIndex, ioNamePtr→name; after: attrib, cnid, sizes, type/creator, parentID |
| GetVolInfo | ioVRefNum, ioVolIndex; after: vol name, vRefNum, free blocks |
| GetEOF, SetEOF | ioRefNum; after: ioMisc (EOF value) |
| Get/SetFPos | ioRefNum, ioPosMode, ioPosOffset |
| Create, Delete | ioNamePtr→name, ioVRefNum, ioDirID |
| OpenWD | ioVRefNum, ioWDDirID; after: ioVRefNum (WD refnum) |
| CloseWD | ioVRefNum |
| GetWDInfo | ioVRefNum, ioWDIndex; after: ioWDVRefNum, ioWDDirID |
| Allocate | ioRefNum, ioReqCount |
| FlushVol, FlushFile | ioVRefNum / ioRefNum |
| SetVol, GetVol | ioVRefNum, ioNamePtr→name |
| Eject, UnmountVol | ioVRefNum |

### Example output

```
SharedDrive │ _Open(PBHOpen) vRefNum=-32000 name="README.txt" dirID=2
            │   → HANDLED: noErr, refNum=2
SharedDrive │ _Read  refNum=2 reqCount=512 posMode=0 posOffset=0
            │   → HANDLED: noErr, actCount=512 posOffset=512
SharedDrive │ _GetFileInfo vRefNum=0 name="Finder"
            │   → PASS-THROUGH (not our volume, vRefNum=0)
SharedDrive │ _GetCatInfo(HFS) vRefNum=-32000 dirID=2 index=1
            │   → HANDLED: noErr
            │     name="README.txt" cnid=16 type='TEXT' creator='ttxt'
            │     dataEOF=1234 rsrcEOF=0 parentID=2
SharedDrive │ _OpenRF vRefNum=-32000 name="MyApp" dirID=2
            │   → ERROR: -43 fnfErr (file not found)
SharedDrive │ _SetVol vRefNum=-32001 name="Shared"
            │   → HANDLED: noErr (DefVCBPtr set)
SharedDrive │ HFS:GetFCBInfo refNum=660 index=0
            │   → HANDLED: noErr, fileNum=16 name="README.txt" parID=2
```

### FourCC display

Type and creator codes are shown as quoted ASCII when all bytes are
printable, otherwise as hex: `type='TEXT'` vs `type=$3F3F3F3F`.

## Implementation phases

### Phase 1: Host infrastructure (no guest changes yet)

New files: `src/core/extfs_log.h` and `src/core/extfs_log.cpp`

All logging logic lives in these files — the trap name tables, error
name tables, param block pretty-printer, and FourCC formatter.
`extn_extfs.cpp` just calls one function from `extfs_log.h`.

1. Create `src/core/extfs_log.h` with:
   - `void extfsLogTrap(uint16_t trapWord, uint32_t pbAddr, uint16_t action, int16_t err, uint16_t flags);`
2. Create `src/core/extfs_log.cpp` with:
   - Trap name + error name tables
   - `readGuestWord(addr)` / `readGuestLong(addr)` helpers
   - `formatParamBlock(trapWord, pbAddr, flags)` — reads guest RAM,
     returns a string describing the input fields
   - `formatResultFields(trapWord, pbAddr, flags)` — reads guest RAM,
     returns a string describing the output fields
   - `extfsLogTrap()` — ties it all together, calls `guestConsoleAppend`
3. Add `extfs_log.cpp` to CMakeLists.txt
4. Wire up `kExtFSLogTrap` in `extn_extfs.cpp` — one-liner that
   calls `extfsLogTrap()`

### Phase 2: Guest migration

1. Add `log_trap()` function and constants to init.c
2. Replace all logging in `DispatchFlat`:
   - Pass-through path: one `log_trap(..., LOG_PASSTHRU, ...)`
   - Each handled trap case: one `log_trap(..., LOG_HANDLED/LOG_ERROR, ...)`
3. Replace all logging in `DispatchHFS` likewise
4. Remove the now-unused per-trap `dbg_log*` calls
5. Keep `dbg_log` / `dbg_hexdump` available for temporary debugging

### Phase 3: Cleanup

1. Remove the `dbg_hexdump` calls in the GetCatInfo handler
   (the host-side pretty-printer replaces them)
2. Verify log output by booting and doing a copy operation
3. Trim any leftover dead log code

## Risks

- **Log volume:** during boot, the Finder calls GetCatInfo and
  GetVolInfo hundreds of times.  The host logging goes to stderr
  which is fine, but the in-memory console buffer (deque) may grow.
  Consider not appending pass-through calls to the console buffer
  (only print to stderr).

- **Performance:** one RPC per trap invocation is acceptable —
  it's already what `dbg_log` does, and we're replacing multiple
  `dbg_log` calls per trap with a single `log_trap`.

- **Guest RAM reads from host:** we already do this for format
  strings and Pascal strings, so no new concerns.  The param block
  is always at a valid guest address (A0 from the trap dispatcher).
