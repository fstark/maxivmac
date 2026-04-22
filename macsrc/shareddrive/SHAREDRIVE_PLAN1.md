# SharedDrive — Phase 1 Implementation Plan

Design: [SHAREDRIVE_DESIGN.md](SHAREDRIVE_DESIGN.md)
Spec: [SHAREDRIVE.md](SHAREDRIVE.md)

Phase 1 adds new host-side infrastructure: wider register block,
`resolveDir` method, and all six new coarse RPC commands ($0220–$0225).
The existing INIT continues to work unchanged throughout — old commands
are untouched.

| Phase | Description                                    | Status |
|-------|------------------------------------------------|--------|
| 1.1   | Widen register block from 7 to 12 params       |        |
| 1.2   | Add guest-encoding constants to HostVolume      |        |
| 1.3   | Add `resolveDir` method + unit tests            |        |
| 1.4   | Add new command constants to extn_extfs.cpp     |        |
| 1.5   | Implement $0220 OpenByName                      |        |
| 1.6   | Implement $0222 GetFileInfoByName               |        |
| 1.7   | Implement $0221 GetCatInfoFull                  |        |
| 1.8   | Implement $0223 ResolveAndOpen                  |        |
| 1.9   | Implement $0224 GetCatInfoResolved              |        |
| 1.10  | Implement $0225 FileOpByName                    |        |
| 1.11  | Integration test with existing INIT             |        |

Build gate: `cmake --preset macos && cmake --build --preset macos`
Test gate:  `ctest --preset macos` (runs doctest binary)

---

## Phase 1.1 — Widen Register Block

Expand `s_regParam` from 7 to 12 slots in `machine.cpp` so new
commands can return up to 12 values.

### 1.1.1 — Modify `src/core/machine.cpp`

**Line ~554:** Change:
```cpp
static uint32_t s_regParam[7]; /* p0–p6 */
```
to:
```cpp
static uint32_t s_regParam[12]; /* p0–p11 */
```

**`regBlockAccess` function (~line 594):** Change the upper bound:
```cpp
if (regOff >= 2 && regOff < 16)
```
to:
```cpp
if (regOff >= 2 && regOff < 26)
```

This extends the memory-mapped window from 16 word-offsets to 26,
supporting 12 parameters (each parameter = 2 consecutive 16-bit
registers: high word + low word).

### Fence

- [ ] `s_regParam` has 12 elements
- [ ] `regBlockAccess` accepts regOff 2–25
- [ ] Full build clean
- [ ] Existing INIT still works (no behavioral change — old commands
      use p0–p6 only; unused slots default to 0)
- [ ] Commit: `"extfs: widen register block to 12 params"`

---

## Phase 1.2 — Guest-Encoding Constants

Add the guest volume identifiers to `HostVolume` so directory
resolution and command handlers can reference them by name.

### 1.2.1 — Modify `src/storage/host_volume.h`

Add to the public section of class `HostVolume`, near the existing
`kRootParentID` and `kRootDirID`:

```cpp
/* Guest-side volume identifiers.  The INIT uses these when
   building the VCB and drive queue entry.  They must match
   the #defines in macsrc/shareddrive/init.c. */
static constexpr int16_t  kGuestVRefNum  = -32000;
static constexpr int16_t  kGuestDriveNum = 8;
```

### Fence

- [ ] Constants compile and are accessible as
      `HostVolume::kGuestVRefNum` / `HostVolume::kGuestDriveNum`
- [ ] Full build clean
- [ ] Commit: `"extfs: add guest volume-encoding constants"`

---

## Phase 1.3 — `resolveDir` Method + Unit Tests

Add the host-side directory resolution method that replaces all
guest-side `ResolveDir` / `ResolveFlatDir` logic.

### 1.3.1 — Modify `src/storage/host_volume.h`

Add to the public section of class `HostVolume`:

```cpp
/* Resolve a guest (vRefNum, dirID) pair to a catalog dirID.
   If rawDirID is non-zero, returns it directly.
   Otherwise decodes vRefNum: our volume / drive → root,
   WD refnum → wdToDirID lookup, 0 → root.
   See SHAREDRIVE_DESIGN.md §3.4. */
uint32_t resolveDir(int16_t vRefNum, uint32_t rawDirID) const;
```

### 1.3.2 — Implement in `src/storage/host_volume.cpp`

```cpp
uint32_t HostVolume::resolveDir(int16_t vRefNum, uint32_t rawDirID) const
{
    if (rawDirID != 0)
        return rawDirID;
    if (vRefNum == kGuestVRefNum || vRefNum == kGuestDriveNum
        || vRefNum == 0)
        return kRootDirID;
    // Decode WD refnum: guest encodes as -(wdRef + 32000)
    auto wdRef = static_cast<uint32_t>(
        -(static_cast<int32_t>(vRefNum)) - 32000);
    uint32_t dirID = wdToDirID(wdRef);
    return dirID != 0 ? dirID : kRootDirID;
}
```

### 1.3.3 — Unit tests in `test/test_host_volume.cpp`

Add a new `TEST_CASE("resolveDir")`:

```cpp
TEST_CASE("resolveDir")
{
    auto tmp = makeTempDir();
    storage::HostVolume vol;
    vol.mount(tmp.path);
    std::filesystem::create_directory(tmp.path / "Sub");
    vol.mount(tmp.path);  // re-mount to pick up Sub

    // Explicit dirID always wins
    CHECK(vol.resolveDir(-32000, 17) == 17);
    CHECK(vol.resolveDir(0, 42) == 42);

    // Our vRefNum → root
    CHECK(vol.resolveDir(-32000, 0) == storage::HostVolume::kRootDirID);

    // Our drive number → root
    CHECK(vol.resolveDir(8, 0) == storage::HostVolume::kRootDirID);

    // Default volume (0) → root
    CHECK(vol.resolveDir(0, 0) == storage::HostVolume::kRootDirID);

    // WD refnum: open a WD for "Sub", then resolve it
    auto *sub = vol.findByName(storage::HostVolume::kRootDirID, "Sub");
    REQUIRE(sub != nullptr);
    uint32_t wd = vol.openWD(sub->cnid);
    int16_t encodedVRef = static_cast<int16_t>(-(static_cast<int32_t>(wd)) - 32000);
    CHECK(vol.resolveDir(encodedVRef, 0) == sub->cnid);

    // Unknown WD → root (graceful fallback)
    int16_t badVRef = static_cast<int16_t>(-32099);
    CHECK(vol.resolveDir(badVRef, 0) == storage::HostVolume::kRootDirID);

    vol.closeWD(wd);
}
```

### Fence

- [ ] `HostVolume::resolveDir` exists and compiles
- [ ] All doctest cases in `resolveDir` pass
- [ ] Existing tests still pass
- [ ] Full build clean
- [ ] Commit: `"extfs: add HostVolume::resolveDir with tests"`

---

## Phase 1.4 — New Command Constants

Add `constexpr` command codes for the six new RPC commands to
`extn_extfs.cpp`, alongside the existing ones.

### 1.4.1 — Modify `src/core/extn_extfs.cpp`

After the existing `static constexpr uint16_t kExtFSLogTrap = 0x20F;`
line, add:

```cpp
/* ── Coarse commands (Phase 1) ────────────────────── */
static constexpr uint16_t kExtFSOpenByName         = 0x220;
static constexpr uint16_t kExtFSGetCatInfoFull     = 0x221;
static constexpr uint16_t kExtFSGetFileInfoByName  = 0x222;
static constexpr uint16_t kExtFSResolveAndOpen     = 0x223;
static constexpr uint16_t kExtFSGetCatInfoResolved = 0x224;
static constexpr uint16_t kExtFSFileOpByName       = 0x225;
```

### Fence

- [ ] Constants compile
- [ ] Full build clean
- [ ] Commit: combined with next phase (1.5)

---

## Phase 1.5 — Implement $0220 OpenByName

Merges ObjByName ($0209) + Open ($0204) into one command.

### 1.5.1 — Add case in `ExtnExtFSDispatch`

Insert before the `default:` case in the switch:

```cpp
case kExtFSOpenByName:
{
    uint32_t dirID = regParam[0];
    std::string name = readPascalString(regParam[1]);
    auto forkType = (regParam[2] == 1) ? storage::ForkType::Resource
                                       : storage::ForkType::Data;

    dbg_printf("[ExtFS] OpenByName dir=%u name=\"%s\" fork=%u\n",
               dirID, name.c_str(), regParam[2]);

    auto *e = s_volume.findByName(dirID, name);
    if (!e) { regResult = fmErrToReg(storage::FMErr::kFnfErr); break; }

    uint32_t size = 0;
    storage::FMErr err;
    uint32_t handle = s_volume.openFork(e->cnid, forkType, size, err);
    if (handle == 0) { regResult = fmErrToReg(err); break; }

    regParam[0] = handle;
    regParam[1] = size;
    regParam[2] = e->cnid;
    regResult = 0;
}
break;
```

### Fence

- [ ] Build clean
- [ ] Old INIT still works (doesn't call $0220 yet)
- [ ] Commit: `"extfs: add OpenByName ($0220) and GetFileInfoByName ($0222)"`
      (combined with 1.4 and 1.6)

---

## Phase 1.6 — Implement $0222 GetFileInfoByName

Merges GetCatInfoByName ($0203) + GetFileInfo ($0207) into one
command.  Uses the wider register block (p0–p8).

### 1.6.1 — Add case in `ExtnExtFSDispatch`

```cpp
case kExtFSGetFileInfoByName:
{
    int16_t vRefNum = static_cast<int16_t>(regParam[0]);
    uint32_t rawDirID = regParam[1];
    std::string name = readPascalString(regParam[2]);

    uint32_t dirID = s_volume.resolveDir(vRefNum, rawDirID);
    dbg_printf("[ExtFS] GetFileInfoByName dir=%u name=\"%s\"\n",
               dirID, name.c_str());

    auto *e = s_volume.findByName(dirID, name);
    if (!e)
    {
        regResult = fmErrToReg(storage::FMErr::kFnfErr);
        break;
    }

    regParam[0] = e->cnid;
    regParam[1] = e->dataForkSize;
    regParam[2] = e->rsrcForkSize;
    regParam[3] = e->type;
    regParam[4] = e->creator;
    regParam[5] = e->crDate;
    regParam[6] = e->modDate;
    regParam[7] = e->finderFlags;
    regParam[8] = e->parentDirID;
    regResult = 0;
}
break;
```

### Fence

- [ ] Build clean, uses p7 and p8 (verifies wider register block)
- [ ] Commit: combined with 1.4 + 1.5 as
      `"extfs: add OpenByName ($0220) and GetFileInfoByName ($0222)"`

---

## Phase 1.7 — Implement $0221 GetCatInfoFull

The most complex new command — combines GetCatInfo + GetFileInfo +
GetDirInfo into one RPC.  Returns up to 12 register values.

### 1.7.1 — Add case in `ExtnExtFSDispatch`

```cpp
case kExtFSGetCatInfoFull:
{
    uint32_t dirID = regParam[0];
    int32_t index = static_cast<int32_t>(regParam[1]);
    uint32_t nameAddr = regParam[2];
    uint32_t nameBuf = regParam[3];

    dbg_printf("[ExtFS] GetCatInfoFull dir=%u idx=%d\n", dirID, index);

    const storage::CatalogEntry *e = nullptr;

    if (index > 0)
    {
        /* Indexed enumeration */
        if (dirID == kRootParentID)
        {
            if (index == 1)
            {
                /* Return root volume itself */
                regParam[0] = kRootDirID;
                regParam[1] = 0x10; /* isDir */
                regParam[2] = static_cast<uint32_t>(
                    s_volume.childCount(kRootDirID));
                regParam[3] = 0; /* rsrcSize=0 for dirs */
                regParam[4] = kRootParentID;
                regParam[5] = 0; regParam[6] = 0; /* type/creator */
                {
                    uint32_t now = static_cast<uint32_t>(
                        std::time(nullptr)) + appledouble::kMacEpochOffset;
                    regParam[7] = now; regParam[8] = now;
                }
                regParam[9] = 0; /* finderFlags */
                if (nameBuf) writePascalString(nameBuf, "Shared");
                regResult = 0;
                break;
            }
            regResult = fmErrToReg(storage::FMErr::kFnfErr);
            break;
        }
        e = s_volume.nthChild(dirID, index);
    }
    else if (index == 0 && nameAddr != 0)
    {
        /* By-name lookup */
        std::string name = readPascalString(nameAddr);
        if (!name.empty())
        {
            e = s_volume.findByName(dirID, name);
        }
        else
        {
            /* Empty name = info about dirID itself */
            e = s_volume.findByCNID(dirID);
            if (!e && (dirID == kRootDirID || dirID == kRootParentID))
            {
                /* Synthesize root */
                goto synthesize_root;
            }
        }
    }
    else
    {
        /* index <= 0: info about dirID itself */
        if (dirID == kRootDirID || dirID == kRootParentID)
        {
            goto synthesize_root;
        }
        e = s_volume.findByCNID(dirID);
    }

    if (!e)
    {
        regResult = fmErrToReg(storage::FMErr::kFnfErr);
        break;
    }

    regParam[0] = e->cnid;
    regParam[1] = e->isDirectory ? 0x10u : 0u;
    regParam[2] = e->isDirectory
        ? static_cast<uint32_t>(s_volume.childCount(e->cnid))
        : e->dataForkSize;
    regParam[3] = e->isDirectory ? 0u : e->rsrcForkSize;
    regParam[4] = e->parentDirID;
    regParam[5] = e->type;
    regParam[6] = e->creator;
    regParam[7] = e->crDate;
    regParam[8] = e->modDate;
    regParam[9] = e->finderFlags;
    if (nameBuf) writePascalString(nameBuf, e->macName);
    regResult = 0;
    break;

synthesize_root:
    {
        uint32_t now = static_cast<uint32_t>(
            std::time(nullptr)) + appledouble::kMacEpochOffset;
        regParam[0] = kRootDirID;
        regParam[1] = 0x10;
        regParam[2] = static_cast<uint32_t>(
            s_volume.childCount(kRootDirID));
        regParam[3] = 0;
        regParam[4] = kRootParentID;
        regParam[5] = 0; regParam[6] = 0;
        regParam[7] = now; regParam[8] = now;
        regParam[9] = 0;
        if (nameBuf) writePascalString(nameBuf, "Shared");
        regResult = 0;
    }
    break;
}
break;
```

### Fence

- [ ] Build clean
- [ ] Handles all 4 GetCatInfo modes (indexed, by-name, self-lookup,
      root synthesis)
- [ ] Commit: `"extfs: add GetCatInfoFull ($0221)"`

---

## Phase 1.8 — Implement $0223 ResolveAndOpen

Like $0220 OpenByName but with host-side directory resolution.
The guest passes raw vRefNum + dirID; the host calls `resolveDir`.

### 1.8.1 — Add case in `ExtnExtFSDispatch`

```cpp
case kExtFSResolveAndOpen:
{
    int16_t vRefNum = static_cast<int16_t>(regParam[0]);
    uint32_t rawDirID = regParam[1];
    std::string name = readPascalString(regParam[2]);
    auto forkType = (regParam[3] == 1) ? storage::ForkType::Resource
                                       : storage::ForkType::Data;

    uint32_t dirID = s_volume.resolveDir(vRefNum, rawDirID);
    dbg_printf("[ExtFS] ResolveAndOpen vref=%d dir=%u→%u name=\"%s\"\n",
               vRefNum, rawDirID, dirID, name.c_str());

    auto *e = s_volume.findByName(dirID, name);
    if (!e)
    {
        regResult = fmErrToReg(storage::FMErr::kFnfErr);
        break;
    }

    uint32_t size = 0;
    storage::FMErr err;
    uint32_t handle = s_volume.openFork(e->cnid, forkType, size, err);
    if (handle == 0) { regResult = fmErrToReg(err); break; }

    regParam[0] = handle;
    regParam[1] = size;
    regParam[2] = e->cnid;
    regParam[3] = dirID; /* resolved dirID for guest FCB */
    regResult = 0;
}
break;
```

### Fence

- [ ] Build clean
- [ ] Commit: `"extfs: add ResolveAndOpen ($0223)"`

---

## Phase 1.9 — Implement $0224 GetCatInfoResolved

Like $0221 GetCatInfoFull but with host-side directory resolution.

### 1.9.1 — Add case in `ExtnExtFSDispatch`

```cpp
case kExtFSGetCatInfoResolved:
{
    int16_t vRefNum = static_cast<int16_t>(regParam[0]);
    uint32_t rawDirID = regParam[1];
    int32_t index = static_cast<int32_t>(regParam[2]);
    uint32_t nameAddr = regParam[3];
    uint32_t nameBuf = regParam[4];

    uint32_t dirID = s_volume.resolveDir(vRefNum, rawDirID);
    dbg_printf("[ExtFS] GetCatInfoResolved vref=%d dir=%u→%u idx=%d\n",
               vRefNum, rawDirID, dirID, index);

    /* Delegate to GetCatInfoFull logic with resolved dirID.
       Re-pack into the same regParam layout that $0221 expects. */
    regParam[0] = dirID;
    regParam[1] = static_cast<uint32_t>(index);
    regParam[2] = nameAddr;
    regParam[3] = nameBuf;
    /* Fall through to the $0221 handler by calling dispatch
       recursively.  Alternative: extract $0221 body into a
       helper function and call it from both cases. */
    ExtnExtFSDispatch(kExtFSGetCatInfoFull, regParam, regResult);
}
break;
```

**Note:** The recursive dispatch is clean because $0221 is pure
(no side effects other than regParam/regResult writes).  An
alternative is to extract the $0221 body into a static helper
`doCatInfoFull(...)` called from both cases — choose whichever
is cleaner during implementation.

### Fence

- [ ] Build clean
- [ ] Commit: `"extfs: add GetCatInfoResolved ($0224)"`

---

## Phase 1.10 — Implement $0225 FileOpByName

Multiplexed command for Create, Delete, Rename, SetFileInfo,
SetCatInfo operations.

### 1.10.1 — Define sub-opcodes

Add to the constants section:

```cpp
static constexpr uint32_t kFileOpCreate      = 0;
static constexpr uint32_t kFileOpDelete      = 1;
static constexpr uint32_t kFileOpRename      = 2;
static constexpr uint32_t kFileOpSetFileInfo = 3;
static constexpr uint32_t kFileOpSetCatInfo  = 4;
```

### 1.10.2 — Add case in `ExtnExtFSDispatch`

```cpp
case kExtFSFileOpByName:
{
    int16_t vRefNum = static_cast<int16_t>(regParam[0]);
    uint32_t rawDirID = regParam[1];
    uint32_t nameAddr = regParam[2];
    uint32_t opcode = regParam[3];

    uint32_t dirID = s_volume.resolveDir(vRefNum, rawDirID);
    std::string name = readPascalString(nameAddr);

    dbg_printf("[ExtFS] FileOpByName op=%u dir=%u name=\"%s\"\n",
               opcode, dirID, name.c_str());

    switch (opcode)
    {
        case kFileOpCreate:
        {
            storage::FMErr err;
            uint32_t cnid = s_volume.createFile(dirID, name, err);
            if (cnid == 0) { regResult = fmErrToReg(err); break; }
            regParam[0] = cnid;
            regResult = 0;
        }
        break;

        case kFileOpDelete:
        {
            auto err = s_volume.remove(dirID, name);
            regResult = fmErrToReg(err);
        }
        break;

        case kFileOpRename:
        {
            std::string newName = readPascalString(regParam[4]);
            auto err = s_volume.rename(dirID, name, newName);
            regResult = fmErrToReg(err);
        }
        break;

        case kFileOpSetFileInfo:
        {
            auto *e = s_volume.findByName(dirID, name);
            if (!e)
            {
                regResult = fmErrToReg(storage::FMErr::kFnfErr);
                break;
            }
            uint32_t type = regParam[4];
            uint32_t creator = regParam[5];
            uint16_t flags = static_cast<uint16_t>(regParam[6]);
            auto err = s_volume.setFileInfo(e->cnid, type, creator, flags);
            regResult = fmErrToReg(err);
        }
        break;

        case kFileOpSetCatInfo:
        {
            /* Files: set Finder info.  Dirs: set DInfo+DXInfo. */
            auto *e = s_volume.findByName(dirID, name);
            if (!e)
            {
                regResult = fmErrToReg(storage::FMErr::kFnfErr);
                break;
            }
            if (e->isDirectory)
            {
                uint32_t guestBuf = regParam[4];
                uint8_t buf[32];
                for (int i = 0; i < 32; i++)
                    buf[i] = get_vm_byte(guestBuf + i);
                auto err = s_volume.setDirInfo(e->cnid, buf);
                regResult = fmErrToReg(err);
            }
            else
            {
                uint32_t type = regParam[4];
                uint32_t creator = regParam[5];
                uint16_t flags = static_cast<uint16_t>(regParam[6]);
                auto err = s_volume.setFileInfo(
                    e->cnid, type, creator, flags);
                regResult = fmErrToReg(err);
            }
        }
        break;

        default:
            regResult = fmErrToReg(storage::FMErr::kParamErr);
            break;
    }
}
break;
```

### Fence

- [ ] Build clean
- [ ] All 5 sub-opcodes implemented
- [ ] Commit: `"extfs: add FileOpByName ($0225) multiplexed command"`

---

## Phase 1.11 — Integration Test

Verify the existing INIT still works with the widened register block,
and that the new commands are callable (even though no INIT uses them
yet).

### 1.11.1 — Unit tests in `test/test_host_volume.cpp`

Add tests for the new commands by calling the HostVolume methods
they wrap (the dispatch wiring is tested implicitly by the existing
integration tests):

```cpp
TEST_CASE("resolveDir with WD open close cycle")
{
    auto tmp = makeTempDir();
    storage::HostVolume vol;
    std::filesystem::create_directory(tmp.path / "A");
    std::filesystem::create_directory(tmp.path / "A" / "B");
    writeFile(tmp.path / "A" / "B" / "hello.txt", "hello");
    vol.mount(tmp.path);

    auto *a = vol.findByName(storage::HostVolume::kRootDirID, "A");
    REQUIRE(a != nullptr);
    auto *b = vol.findByName(a->cnid, "B");
    REQUIRE(b != nullptr);

    uint32_t wd = vol.openWD(b->cnid);
    int16_t encoded = static_cast<int16_t>(
        -(static_cast<int32_t>(wd)) - 32000);

    // resolveDir with the encoded WD should find B
    CHECK(vol.resolveDir(encoded, 0) == b->cnid);

    // findByName in that resolved dir should find the file
    auto resolved = vol.resolveDir(encoded, 0);
    auto *f = vol.findByName(resolved, "hello.txt");
    CHECK(f != nullptr);
    CHECK(f->dataForkSize > 0);

    vol.closeWD(wd);
}
```

### 1.11.2 — Manual smoke test

Boot emulator with existing INIT + a `shared/` directory with
subdirectories.  Verify:
- Volume mounts normally
- Files open from subdirectories
- No regressions from the wider register block

### Fence

- [ ] All unit tests pass
- [ ] Manual smoke test passes
- [ ] Commit: `"extfs: phase 1 integration tests"`
