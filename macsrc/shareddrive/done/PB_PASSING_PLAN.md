# SharedDrive PB-Passing Migration — Implementation Plan

## Goal

Replace the register-stuffing RPC convention with PB-pointer-passing.
Each guest trap shim passes the PB address; the host reads/writes the
PB directly via `get_vm_byte`/`put_vm_byte`.  Guest code shrinks
dramatically; host code stays about the same size but becomes
self-documenting (offsets match Inside Macintosh).

## Principles

1. **One trap = one host command.**  No sub-opcodes, no
   `kFileOpByName` dispatcher.  Each patched trap maps 1:1 to a host
   command code.
2. **Guest passes PB pointer, not fields.**  `reg[0] = (uint32_t)pb`,
   then fire.  For commands that need a host file handle (Read, Write,
   Close, SetEOF) the guest also passes the handle in `reg[1]` since
   that comes from the FCB, not the PB.
3. **Host writes PB directly.**  Output fields are written to guest
   memory at their Inside Macintosh offsets.  The guest reads nothing
   back from registers (except for Open, which returns a host handle
   and CNID for FCB management).
4. **Legacy commands stay callable** during migration.  Old and new
   command codes coexist. Each phase can be tested independently.

## Command register convention (new)

All new commands:

| Register | Meaning |
|----------|---------|
| `reg[0]` | Guest PB address (always) |
| `reg[1]` | Host handle (only for handle-based I/O: Read/Write/Close/SetEOF) |
| `result` | Mac OS error code (0 = noErr, else negated positive) |

For Open/OpenRF only, additional outputs in registers:

| Register | Meaning |
|----------|---------|
| `reg[0]` | Host handle |
| `reg[1]` | Fork size (logical EOF) |
| `reg[2]` | CNID |

These are needed because the guest creates an FCB with this data.

## What does NOT change

These trap handlers are **purely guest-side** — they never make host
RPCs and need no migration:

- `TrapGetEOF` — reads FCB
- `TrapGetFPos` — reads FCB
- `TrapSetFPos` — reads/writes FCB
- `TrapFlushFile` — no-op
- `TrapAllocate` — no-op
- `TrapFlushVol` — no-op
- `TrapUnmountVol` — queue manipulation
- `TrapEject` — queue manipulation
- `TrapGetFCBInfo` — reads FCB
- `TrapGetVolParms` — fills fixed data
- `TrapSetVInfo` — no-op

These 11 handlers stay exactly as they are.

## What does NOT change (infrastructure)

- `DbgLog`, `Fatal`, `LogTrap`, `GuestVars` — these are debug/control
  commands, not File Manager traps. They stay as-is.
- `ExtFSVersion` — handshake command, stays as-is.
- The 68k stub generators (`MakeFlatStub`, `MakeHFSStub`) and
  dispatchers (`DispatchFlat`, `DispatchHFS`) are unchanged.
- FCB management (`AllocFCB`, `FreeFCB`, `GetFCB`, `IsOurFCB`)
  stays in the guest.
- `ExtractLocation` stays in the guest — but moves *into* the host.
  The host reads vRefNum/dirID from the PB and calls `resolveDir()`
  itself.

## Migration phases

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Host PB helper functions + new command codes | |
| 2 | Catalog browsing: GetCatInfo, GetFileInfo | |
| 3 | Open/Read/Write/Close/SetEOF (I/O group) | |
| 4 | Mutations: Create, Delete, Rename, DirCreate, CatMove | |
| 5 | Metadata: SetFileInfo, SetCatInfo | |
| 6 | Volume/WD: GetVolInfo, GetVol, SetVol, OpenWD, CloseWD, GetWDInfo | |
| 7 | Dead code removal | |

Build gate: `cmake --preset macos && cmake --build --preset macos`
Test gate:  `./bld/macos/tests`

---

## Phase 1 — Type-safe PB abstraction + new command codes

Add a type-safe `PBRef`/`PBField` abstraction for guest memory access,
new command codes, and standalone handler function stubs.
No behavior changes — the new infrastructure compiles but nothing
calls it yet.

### 1.1 — PBRef / PBField in extn_extfs.cpp

The type is baked into the field definition.  A width mismatch is a
compile error — there is no way to accidentally `get_vm_long` a
`int16_t` field:

```cpp
/* ── Type-safe PB access ─────────────────────────── */

template <typename T>
struct PBField { uint32_t offset; };

namespace detail {
template <typename T> T   pbRead(uint32_t addr);
template <typename T> void pbWrite(uint32_t addr, T v);

template <> inline uint8_t pbRead<uint8_t>(uint32_t a) { return get_vm_byte(a); }
template <> inline int8_t  pbRead<int8_t>(uint32_t a)  { return static_cast<int8_t>(get_vm_byte(a)); }
template <> inline uint16_t pbRead<uint16_t>(uint32_t a) {
    return uint16_t(get_vm_byte(a)) << 8 | get_vm_byte(a + 1);
}
template <> inline int16_t pbRead<int16_t>(uint32_t a)  { return static_cast<int16_t>(pbRead<uint16_t>(a)); }
template <> inline uint32_t pbRead<uint32_t>(uint32_t a) {
    return uint32_t(get_vm_byte(a)) << 24 | uint32_t(get_vm_byte(a+1)) << 16
         | uint32_t(get_vm_byte(a+2)) << 8 | get_vm_byte(a+3);
}
template <> inline int32_t pbRead<int32_t>(uint32_t a) { return static_cast<int32_t>(pbRead<uint32_t>(a)); }

template <> inline void pbWrite<uint8_t>(uint32_t a, uint8_t v) { put_vm_byte(a, v); }
template <> inline void pbWrite<int8_t>(uint32_t a, int8_t v) { put_vm_byte(a, static_cast<uint8_t>(v)); }
template <> inline void pbWrite<uint16_t>(uint32_t a, uint16_t v) {
    put_vm_byte(a, (v >> 8) & 0xFF); put_vm_byte(a + 1, v & 0xFF);
}
template <> inline void pbWrite<int16_t>(uint32_t a, int16_t v) { pbWrite<uint16_t>(a, static_cast<uint16_t>(v)); }
template <> inline void pbWrite<uint32_t>(uint32_t a, uint32_t v) {
    put_vm_byte(a, (v>>24)&0xFF); put_vm_byte(a+1, (v>>16)&0xFF);
    put_vm_byte(a+2, (v>>8)&0xFF); put_vm_byte(a+3, v&0xFF);
}
template <> inline void pbWrite<int32_t>(uint32_t a, int32_t v) { pbWrite<uint32_t>(a, static_cast<uint32_t>(v)); }
} // namespace detail

template <typename T>
struct PBProxy {
    uint32_t addr;
    operator T() const { return detail::pbRead<T>(addr); }
    PBProxy &operator=(T v) { detail::pbWrite<T>(addr, v); return *this; }
};

struct PBRef {
    uint32_t addr;
    template <typename T>
    PBProxy<T> operator[](PBField<T> f) const { return {addr + f.offset}; }
};
```

Usage:

```cpp
PBRef pb{regParam[0]};
int16_t vRef  = pb[ioVRefNum];    // int16_t — enforced at compile time
uint32_t cnid = pb[ioFlNum];      // uint32_t
pb[ioFlParID] = e->parentDirID;   // direct write — no put_vm_long dance
pb[ioFlAttrib] = uint8_t(0x10);   // byte write
```

Misuse is a compile error: `uint32_t x = pb[ioVRefNum]` narrows
`int16_t` → `uint32_t` and the compiler warns.  Trying to pass
`ioVRefNum` (which is `PBField<int16_t>`) where `PBField<uint32_t>`
is expected won't compile.

### 1.2 — PB field definitions

Typed field constants matching Inside Macintosh IV.  These replace
raw offset constants — there are no `kPB_*` integer constants:

```cpp
/* PB fields — Inside Macintosh IV (type encodes width) */

/* Shared header */
constexpr PBField<int16_t>  ioResult    {16};
constexpr PBField<uint32_t> ioNamePtr   {18};
constexpr PBField<int16_t>  ioVRefNum   {22};
constexpr PBField<int16_t>  ioRefNum    {24};
constexpr PBField<uint8_t>  ioPermssn   {27};
constexpr PBField<uint32_t> ioMisc      {28};

/* ioParam variant */
constexpr PBField<uint32_t> ioBuffer    {32};
constexpr PBField<uint32_t> ioReqCount  {36};
constexpr PBField<uint32_t> ioActCount  {40};
constexpr PBField<int16_t>  ioPosMode   {44};
constexpr PBField<int32_t>  ioPosOffset {46};

/* fileParam / CInfoPBRec hFileInfo variant */
constexpr PBField<int16_t>  ioFDirIndex {28};
constexpr PBField<uint8_t>  ioFlAttrib  {30};
/* ioFlFndrInfo at 32 is 16 raw bytes — use raw copy, not PBField */
constexpr uint32_t kOff_ioFlFndrInfo = 32;
constexpr PBField<uint32_t> ioFlNum     {48};
constexpr PBField<int16_t>  ioFlStBlk   {52};
constexpr PBField<uint32_t> ioFlLgLen   {54};
constexpr PBField<uint32_t> ioFlPyLen   {58};
constexpr PBField<int16_t>  ioFlRStBlk  {62};
constexpr PBField<uint32_t> ioFlRLgLen  {64};
constexpr PBField<uint32_t> ioFlRPyLen  {68};
constexpr PBField<uint32_t> ioFlCrDat   {72};
constexpr PBField<uint32_t> ioFlMdDat   {76};
constexpr PBField<uint32_t> ioFlBkDat   {80};
/* ioFlXFndrInfo at 84 is 16 raw bytes */
constexpr uint32_t kOff_ioFlXFndrInfo = 84;
constexpr PBField<uint32_t> ioFlParID   {100};
constexpr PBField<uint32_t> ioFlClpSiz  {104};

/* CInfoPBRec dirInfo variant (overlapping offsets, different types) */
/* ioDrUsrWds at 32 is 16 raw bytes (DInfo) */
constexpr uint32_t kOff_ioDrUsrWds = 32;
constexpr PBField<uint32_t> ioDrDirID   {48};
constexpr PBField<int16_t>  ioDrNmFls   {52};
constexpr PBField<uint32_t> ioDrCrDat   {72};
constexpr PBField<uint32_t> ioDrMdDat   {76};
constexpr PBField<uint32_t> ioDrBkDat   {80};
/* ioDrFndrInfo at 84 is 16 raw bytes (DXInfo) */
constexpr uint32_t kOff_ioDrFndrInfo = 84;
constexpr PBField<uint32_t> ioDrParID   {100};

/* WDParam variant */
constexpr PBField<int16_t>  ioWDIndex   {26};
constexpr PBField<uint32_t> ioWDProcID  {28};
constexpr PBField<int16_t>  ioWDVRefNum {32};
constexpr PBField<uint32_t> ioWDDirID   {48};

/* volumeParam variant */
constexpr PBField<int16_t>  ioVolIndex  {28};
constexpr PBField<int16_t>  ioVNmAlBlks {46};
constexpr PBField<uint32_t> ioVAlBlkSiz {48};
constexpr PBField<uint32_t> ioVClpSiz   {52};
constexpr PBField<int16_t>  ioVFrBlk    {62};

/* CatMove */
constexpr PBField<uint32_t> ioNewDirID  {36};
```

Note: multi-byte blobs (FInfo, DInfo, FXInfo, DXInfo) use raw
`uint32_t` offset constants (`kOff_*`) and byte-by-byte copy.
Only scalar fields get `PBField<T>`.

### 1.3 — New command codes

```cpp
/* PB-based commands — one per Mac trap */
static constexpr uint16_t kPB_GetCatInfo    = 0x230;
static constexpr uint16_t kPB_GetFileInfo   = 0x231;
static constexpr uint16_t kPB_Open          = 0x232;
static constexpr uint16_t kPB_OpenRF        = 0x233;
static constexpr uint16_t kPB_Create        = 0x238;
static constexpr uint16_t kPB_Delete        = 0x239;
static constexpr uint16_t kPB_Rename        = 0x23A;
static constexpr uint16_t kPB_SetFileInfo   = 0x23B;
static constexpr uint16_t kPB_SetCatInfo    = 0x23C;
static constexpr uint16_t kPB_DirCreate     = 0x23D;
static constexpr uint16_t kPB_CatMove       = 0x23E;
static constexpr uint16_t kPB_GetVolInfo    = 0x23F;
static constexpr uint16_t kPB_GetVol        = 0x240;
static constexpr uint16_t kPB_SetVol        = 0x241;
static constexpr uint16_t kPB_OpenWD        = 0x242;
static constexpr uint16_t kPB_CloseWD       = 0x243;
static constexpr uint16_t kPB_GetWDInfo     = 0x244;
```

No codes for Read/Write/Close/SetEOF — they stay register-based.

### 1.4 — Host-side resolveDir helper

Factor out `ExtractLocation`-equivalent logic for the host:

```cpp
static uint32_t pbResolveDir(PBRef pb)
{
    int16_t vRefNum = pb[ioVRefNum];
    uint32_t dirID  = pb[ioDrDirID];
    return s_volume.resolveDir(vRefNum, dirID);
}
```

### Fence

- [ ] `PBRef`/`PBField` template, field definitions, and command codes
      compile
- [ ] No behavioral change — all existing tests pass
- [ ] Commit: `"shareddrive: phase 1 — type-safe PB abstraction and command codes"`

---

## Phase 2 — Catalog browsing: GetCatInfo, GetFileInfo

Migrate the two biggest PB-heavy read traps.  These fill many output
fields in the PB from catalog data.

**Manual test:** Boot, open the Shared volume, open a subfolder,
open a file with Get Info.

### 2.1 — Host: PbGetCatInfo standalone function

```cpp
static uint16_t PbGetCatInfo(PBRef pb)
{
    uint32_t dirID = pbResolveDir(pb);
    int16_t index  = pb[ioFDirIndex];
    uint32_t nameAddr = pb[ioNamePtr];
    // ... resolve entry, fill PB fields, return error code
}
```

```
Input:  reg[0] = PB address
Reads from PB: ioVRefNum, ioDrDirID, ioFDirIndex, ioNamePtr
```

Logic (host reads PB, calls resolveDir, dispatches to volume methods,
writes results directly to PB using `PBRef`):

- `index > 0` → indexed enumeration: `nthChild(dirID, index)`.
  Write `ioFlAttrib`, all FInfo/DInfo fields, sizes, dates, parentID,
  name to PB.
- `index == 0 && name` → by-name lookup via `findByPath()`.
- `index == 0 && !name` → info about dirID itself via `findByCNID()`.
- `index < 0` → info about dirID itself.
- For directories: write DInfo/DXInfo (32 bytes) to PB at
  `kOff_ioDrUsrWds` and `kOff_ioDrFndrInfo` via byte copy.
  Call `getDirInfo()` to fetch the raw Finder directory info.
- For files: write FInfo (16 bytes) at `kOff_ioFlFndrInfo` via byte
  copy for the blob, plus typed writes for scalars:
  `pb[ioFlNum] = e->cnid`, `pb[ioFlLgLen] = e->dataForkSize`, etc.
- Write name to `ioNamePtr` buffer using `writePascalString()`.
- Root/volume synthesis stays as-is (hardcoded "Shared" responses).

### 2.2 — Host: PbGetFileInfo standalone function

```cpp
static uint16_t PbGetFileInfo(PBRef pb)
{
    uint32_t dirID = pbResolveDir(pb);
    uint32_t nameAddr = pb[ioNamePtr];
    // ... resolve by name, fill PB, return error code
}
```

Like GetCatInfo but only for the `_GetFileInfo` trap (files only,
no directory variant, no indexed enumeration).  Resolves by name,
fills FInfo, `ioFlNum`, sizes, dates, `ioFlParID`.

### 2.3 — Guest: TrapGetCatInfo / TrapGetFileInfo become stubs

```c
static OSErr TrapGetCatInfo(char *pb, Globals *g, short isHFS)
{
    reg_set(g->regBase, 0, (unsigned long)pb);
    reg_command(g->regBase, kPB_GetCatInfo);
    return host_err(g->regBase);
}

static OSErr TrapGetFileInfo(char *pb, Globals *g, short isHFS)
{
    reg_set(g->regBase, 0, (unsigned long)pb);
    reg_command(g->regBase, kPB_GetFileInfo);
    return host_err(g->regBase);
}
```

### 2.4 — Wire into dispatch switch

The switch body for new commands is a one-liner that delegates to
the standalone function:

```cpp
case kPB_GetCatInfo:
    regResult = PbGetCatInfo(PBRef{regParam[0]});
    break;
case kPB_GetFileInfo:
    regResult = PbGetFileInfo(PBRef{regParam[0]});
    break;
```

### Fence

- [ ] Host reads PB and writes all catalog fields directly via `PBRef`
- [ ] Guest GetCatInfo/GetFileInfo are 3-line stubs
- [ ] Build passes `cmake --build --preset macos`
- [ ] Unit tests pass: `./bld/macos/tests`
- [ ] **Manual test:** boot Mac, open Shared volume, browse folders,
      Get Info on files — all data correct
- [ ] Commit: `"shareddrive: phase 2 — PB-passing for GetCatInfo + GetFileInfo"`

---

## Phase 3 — Open/OpenRF

Migrate the Open traps.  Read/Write/Close/SetEOF stay register-based
(the guest does mark/posMode computation with FCB state).

**Manual test:** Open a file (TeachText, ResEdit), read its contents,
edit and save, verify on host filesystem.

### 3.1 — Host: PbOpen / PbOpenRF standalone functions

```cpp
static uint16_t PbOpen(PBRef pb, uint32_t regParam[])
{
    uint32_t dirID = pbResolveDir(pb);
    uint32_t nameAddr = pb[ioNamePtr];
    uint8_t perm = pb[ioPermssn];
    // resolve, open data fork
    // regParam[0] = handle, regParam[1] = size, regParam[2] = cnid
    return 0;
}

static uint16_t PbOpenRF(PBRef pb, uint32_t regParam[])
{
    // identical but opens resource fork
}
```

```
Input:  reg[0] = PB address
Reads from PB: ioVRefNum, ioDrDirID, ioNamePtr, ioPermssn
Output: reg[0] = host handle, reg[1] = fork size, reg[2] = CNID
```

Returns handle/size/cnid in registers because the guest needs them
for FCB creation — the only exception to "host writes PB directly."

### 3.2 — Guest: TrapOpen / TrapOpenRF shrink

The `ExtractLocation` call and its 5 register-sets disappear.
Guest still does:
- `reg_set(reg[0], pb)` + fire command
- Read back handle/size/cnid from registers
- Create FCB, write `ioRefNum` to PB

### 3.3 — Read/Write/Close/SetEOF stay unchanged

These remain on `kCmdRead`/`kCmdWrite`/`kCmdClose`/`kCmdSetEOF`
with register-based passing.  The guest does mark/posMode logic
that must stay guest-side.

### 3.4 — Wire into dispatch switch

```cpp
case kPB_Open:
    regResult = PbOpen(PBRef{regParam[0]}, regParam);
    break;
case kPB_OpenRF:
    regResult = PbOpenRF(PBRef{regParam[0]}, regParam);
    break;
```

### Fence

- [ ] Host Open/OpenRF read PB directly via `PBRef`
- [ ] Guest Open/OpenRF shrink (no ExtractLocation, no register
      stuffing for name/vref/dirID)
- [ ] Read/Write/Close/SetEOF unchanged
- [ ] Build passes
- [ ] Unit tests pass
- [ ] **Manual test:** open files data+resource forks, read, edit, save
- [ ] Commit: `"shareddrive: phase 3 — PB-passing for Open/OpenRF"`

---

## Phase 4 — Mutations: Create, Delete, Rename, DirCreate, CatMove

Migrate file/directory creation, deletion, renaming, and moving.

**Manual test:** New Folder, Duplicate a file, rename it, drag to trash,
move a file between folders.

### 4.1 — Host: standalone functions

Each is a standalone function taking `PBRef`:

```cpp
static uint16_t PbCreate(PBRef pb)
{
    uint32_t dirID = pbResolveDir(pb);
    std::string name = readPascalString(pb[ioNamePtr]);
    // s_volume.createFile(dirID, name, err)
    return fmErrToReg(err);
}

static uint16_t PbDelete(PBRef pb)    { /* same pattern */ }
static uint16_t PbRename(PBRef pb)    { /* reads ioMisc for new name */ }
static uint16_t PbDirCreate(PBRef pb) { /* writes ioDrDirID = new CNID */ }
static uint16_t PbCatMove(PBRef pb)   { /* reads ioNewDirID */ }
```

PB fields read/written:

| Function | Reads | Writes |
|----------|-------|--------|
| PbCreate | ioVRefNum, ioDrDirID, ioNamePtr | — |
| PbDelete | ioVRefNum, ioDrDirID, ioNamePtr | — |
| PbRename | ioVRefNum, ioDrDirID, ioNamePtr, ioMisc | — |
| PbDirCreate | ioVRefNum, ioDrDirID, ioNamePtr | ioDrDirID (new CNID) |
| PbCatMove | ioVRefNum, ioDrDirID, ioNamePtr, ioNewDirID | — |

### 4.2 — Guest: all five become 3-line stubs

```c
static OSErr TrapCreate(char *pb, Globals *g, short isHFS)
{
    reg_set(g->regBase, 0, (unsigned long)pb);
    reg_command(g->regBase, kPB_Create);
    return host_err(g->regBase);
}
```

TrapCreate still bumps `vcbNxtCNID` after a successful call.
TrapDirCreate reads back `ioDirID` from PB (host wrote it).

### 4.3 — Wire into dispatch switch

```cpp
case kPB_Create:    regResult = PbCreate(PBRef{regParam[0]});    break;
case kPB_Delete:    regResult = PbDelete(PBRef{regParam[0]});    break;
case kPB_Rename:    regResult = PbRename(PBRef{regParam[0]});    break;
case kPB_DirCreate: regResult = PbDirCreate(PBRef{regParam[0]}); break;
case kPB_CatMove:   regResult = PbCatMove(PBRef{regParam[0]});   break;
```

### Fence

- [ ] Five standalone host functions, five guest stubs
- [ ] Build + unit tests pass
- [ ] **Manual test:** New Folder, Duplicate, rename, trash, move
- [ ] Commit: `"shareddrive: phase 4 — PB-passing for Create/Delete/Rename/DirCreate/CatMove"`

---

## Phase 5 — Metadata: SetFileInfo, SetCatInfo

Migrate metadata-setting traps.  These write Finder info (type,
creator, flags, icon position) for files and directories.

**Manual test:** Get Info on a file (change type/creator if possible),
move icons in a folder window, reboot and verify positions persist.

### 5.1 — Host: PbSetFileInfo standalone function

```cpp
static uint16_t PbSetFileInfo(PBRef pb)
{
    uint32_t dirID = pbResolveDir(pb);
    std::string name = readPascalString(pb[ioNamePtr]);
    // Read FInfo 16 bytes from pb.addr + kOff_ioFlFndrInfo
    // Decompose type/creator/flags/location/folder
    // s_volume.setFileInfo(...)
}
```

Reads: ioVRefNum, ioDrDirID, ioNamePtr, FInfo blob at offset 32.

### 5.2 — Host: PbSetCatInfo standalone function

```cpp
static uint16_t PbSetCatInfo(PBRef pb)
{
    uint32_t dirID = pbResolveDir(pb);
    std::string name = readPascalString(pb[ioNamePtr]);
    // Read FInfo/DInfo at offset 32 (16 bytes)
    // Read FXInfo/DXInfo at offset 84 (16 bytes)
    // Determine file vs dir from catalog, dispatch accordingly
}
```

Reads: ioVRefNum, ioDrDirID, ioNamePtr, blob at offsets 32 and 84.

### 5.3 — Guest: both become 3-line stubs

No more register-stuffing for 6 FInfo fields.  No more `s_dirInfoBuf`
static buffer copy.

### 5.4 — Wire into dispatch switch

```cpp
case kPB_SetFileInfo: regResult = PbSetFileInfo(PBRef{regParam[0]}); break;
case kPB_SetCatInfo:  regResult = PbSetCatInfo(PBRef{regParam[0]});  break;
```

### Fence

- [ ] Guest SetFileInfo/SetCatInfo are stubs
- [ ] `s_dirInfoBuf` static buffer can be removed (if no other users)
- [ ] Build + unit tests pass
- [ ] **Manual test:** icon positions, Get Info metadata, reboot check
- [ ] Commit: `"shareddrive: phase 5 — PB-passing for SetFileInfo/SetCatInfo"`

---

## Phase 6 — Volume/WD: GetVolInfo, GetVol, SetVol, OpenWD, CloseWD, GetWDInfo

Migrate volume information and working directory traps.

**Manual test:** Volume mounts, Finder shows correct sizes, desktop
file works, opening files from subdirectories works (WD resolution).

### 6.1 — Host: PbGetVolInfo standalone function

```cpp
static uint16_t PbGetVolInfo(PBRef pb)
{
    // Read ioVolIndex, fill ~30 PB fields
}
```

Note: TrapGetVolInfo currently checks VCB queue for indexed walk.
That VCB walk stays in the guest.  If the guest determines the call
is for our volume, it fires the host command.  The host fills PB
fields but the guest still writes some VCB-derived fields (vcbCrDate
etc.) because they come from the guest VCB, not the host.

**Compromise:** For GetVolInfo, keep a hybrid approach.  The guest
checks ownership and fills VCB-derived fields.  The host fills the
dynamic fields (file counts, free blocks).  Or — move all of it to
the host, which would need access to VCB creation date (passed once
at init time or in an extra register).

### 6.2 — Host: PbGetVol / PbSetVol standalone functions

GetVol fills volume name and vRefNum.  SetVol sets the default volume.

SetVol has complex ownership-checking logic including name matching.
The guest ownership check stays; the host handles WD creation for
the HSetVol case.

### 6.3 — Host: PbOpenWD / PbCloseWD / PbGetWDInfo standalone functions

```cpp
static uint16_t PbOpenWD(PBRef pb) {
    uint32_t dirID  = pb[ioWDDirID];
    uint32_t procID = pb[ioWDProcID];
    uint32_t wdRef  = s_volume.openWD(dirID, procID);
    pb[ioVRefNum] = int16_t(-(int32_t(wdRef) + 32000));
    return 0;
}

static uint16_t PbCloseWD(PBRef pb) { /* reads ioVRefNum */ }
static uint16_t PbGetWDInfo(PBRef pb) {
    // reads ioVRefNum, ioWDIndex
    // writes ioWDProcID, ioWDVRefNum, ioWDDirID, name
}
```

### 6.4 — Guest: stubs

### 6.5 — Wire into dispatch switch

```cpp
case kPB_GetVolInfo: regResult = PbGetVolInfo(PBRef{regParam[0]}); break;
case kPB_GetVol:     regResult = PbGetVol(PBRef{regParam[0]});     break;
case kPB_SetVol:     regResult = PbSetVol(PBRef{regParam[0]});     break;
case kPB_OpenWD:     regResult = PbOpenWD(PBRef{regParam[0]});     break;
case kPB_CloseWD:    regResult = PbCloseWD(PBRef{regParam[0]});    break;
case kPB_GetWDInfo:  regResult = PbGetWDInfo(PBRef{regParam[0]});  break;
```

### Fence

- [ ] Volume and WD traps migrated to standalone functions
- [ ] Build + unit tests pass
- [ ] **Manual test:** full boot cycle, volume appears, SFGetFile
      dialog shows volume, working directory navigation works
- [ ] Commit: `"shareddrive: phase 6 — PB-passing for volume and WD traps"`

---

## Phase 7 — Dead code removal

Remove all legacy register-based host commands that are no longer
called by any guest code.

### 7.1 — Remove old command codes and handlers

Delete from `extn_extfs.cpp`:
- `kExtFSGetCatInfo`, `kExtFSGetCatInfoName` handlers
- `kExtFSGetCatInfoFull`, `kExtFSGetCatInfoResolved` handlers
- `kExtFSGetFileInfoByName` handler
- `kExtFSOpen`, `kExtFSOpenByName`, `kExtFSResolveAndOpen` handlers
- `kExtFSCreateFile`, `kExtFSDeleteFile` handlers
- `kExtFSSetFileInfo` handler
- `kExtFSCreateDir`, `kExtFSCatMove`, `kExtFSRename` handlers
- `kExtFSGetDirInfo`, `kExtFSSetDirInfo` handlers
- `kExtFSFileOpByName` handler and `kFileOp*` sub-opcodes
- `doCatInfoFull` helper function
- `kExtFSReadDir`, `kExtFSObjByName`, `kExtFSGetFileInfo` handlers

Keep:
- `kExtFSRead`, `kExtFSWrite`, `kExtFSClose`, `kExtFSSetEOF`
  (still used for handle-based I/O)
- `kExtFSVersion`, `kExtFSGetVol`, `kExtFSDbgLog`, `kExtFSLogTrap`,
  `kExtFSGuestVars`, `kExtFSFatal` (infrastructure)
- `kExtFSOpenWD`, `kExtFSCloseWD`, `kExtFSGetWDInfo`
  (keep or already migrated by phase 6)

### 7.2 — Remove guest-side dead code

- Remove `s_nameBuf` if no longer used (GetCatInfo was its only user)
- Remove `s_dirInfoBuf` if no longer used
- Remove `ExtractLocation` if all callers migrated
- Remove old `kCmd*` constants from guest

### 7.3 — Update command code comments

Clean up the command table comment at top of init.c.

### Fence

- [ ] No dead command handlers remain
- [ ] Build clean, tests pass
- [ ] **Manual test:** full regression — boot, browse, open, edit,
      save, New Folder, rename, trash, Get Info, icon positions, reboot
- [ ] Commit: `"shareddrive: phase 7 — remove legacy register-based commands"`

---

## Risk notes

- **Byte order:** The PB is in 68k memory (big-endian).  `PBField<T>`
  + `PBProxy` handle all byte-swapping centrally in the `detail::`
  templates.  The risk surface shrinks to getting those 6 template
  specializations right in Phase 1 — in later phases it's impossible
  to get byte order wrong.
- **Field width mismatches:** The old approach let you silently do
  `get_vm_long()` on a 2-byte field.  `PBField<T>` makes this a
  compile error.  This is the main safety win.
- **Signed fields:** `ioVRefNum` is `PBField<int16_t>`,
  `ioPosOffset` is `PBField<int32_t>`.  The type is in the field
  definition — no chance of forgetting to cast.
- **Multi-byte blobs:** FInfo, DInfo, FXInfo, DXInfo are 16-byte
  blobs that don't map to a single scalar type.  These use raw
  `uint32_t` offset constants (`kOff_ioFlFndrInfo` etc.) with
  byte-by-byte copy loops.  They bypass `PBRef` intentionally.
- **GetVolInfo complexity:** This trap has the most PB fields (~30)
  and mixed ownership (some fields from VCB, some from host).
  Consider a hybrid approach as noted in Phase 6.
- **Read/Write stay register-based:** The guest does mark/posMode
  computation that depends on FCB state.  Moving this to the host
  would require exposing FCB data to the host, which is a larger
  refactoring not justified here.
- **Standalone functions:** Every new PB handler is a standalone
  `static uint16_t PbXxx(PBRef pb)` function.  The dispatch switch
  is a pure routing table with one-line cases.  This makes each
  handler independently readable, testable, and auditable.
