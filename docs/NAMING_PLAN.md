# Parameter & Local Variable Naming Plan

Migrate all function parameters and local variables to **camelCase** as
specified in `NAMING.md`. Each phase groups related files so a single
build+test cycle validates the change.

---

## Phase 1 — `src/core/machine.h` + `src/core/machine.cpp` (debug logging)

Rename PascalCase parameters in the `dbglog_*` free functions.

| Function | Old parameter | New parameter |
|----------|--------------|---------------|
| `dbglog_WriteMemArrow` | `WriteMem` | `writeMem` |
| `dbglog_AddrAccess` | `Data` | `data` |
| `dbglog_AddrAccess` | `WriteMem` | `writeMem` |
| `dbglog_Access` | `Data` | `data` |
| `dbglog_Access` | `WriteMem` | `writeMem` |

**Files:** `src/core/machine.h`, `src/core/machine.cpp`

---

## Phase 2 — `src/core/machine.h` + `src/core/machine.cpp` (MMDV_Access)

Rename PascalCase parameters in `MMDV_Access`.

| Function | Old parameter | New parameter |
|----------|--------------|---------------|
| `MMDV_Access` | `Data` | `data` |
| `MMDV_Access` | `WriteMem` | `writeMem` |
| `MMDV_Access` | `ByteSize` | `byteSize` |

Also rename the local variable `origData` is already camelCase — no
change needed. But rename usage of `Data` throughout the function body.

**Files:** `src/core/machine.h`, `src/core/machine.cpp`

---

## Phase 3 — `src/core/machine.cpp` (PbufTransferVM + Extn locals)

Rename parameters and local variables in `PbufTransferVM` and the
extension dispatch block.

| Function / scope | Old name | New name |
|-----------------|----------|----------|
| `PbufTransferVM` (param) | `Buffera` | `buffera` |
| `PbufTransferVM` (param) | `IsWrite` | `isWrite` |
| `PbufTransferVM` (local) | `Buffer` | `buffer` |
| Extn dispatch (locals) | `Pbuf_No` | `pbufNo` |
| Extn dispatch (locals) | `Count` | `count` |
| Extn dispatch (locals) | `Buffera` | `buffera` |
| Extn dispatch (locals) | `IsWrite` | `isWrite` |
| Extn dispatch (locals) | `PbufCount` | `pbufCount` |

**Files:** `src/core/machine.cpp`

---

## Phase 4 — `src/devices/via_base.cpp`

Align implementation parameter names with the (already correct) header
declarations in `src/devices/via_base.h`.

| Function | Old parameter | New parameter |
|----------|--------------|---------------|
| `putORA` | `Selection` | `selection` |
| `putORA` | `Data` | `data` |
| `putORB` | `Selection` | `selection` |
| `putORB` | `Data` | `data` |
| `setDDR_A` | `Data` | `data` |
| `setDDR_B` | `Data` | `data` |
| `setInterruptFlag` | `VIA_Int` | `viaInt` |
| `clrInterruptFlag` | `VIA_Int` | `viaInt` |

**Files:** `src/devices/via_base.cpp`

---

## Phase 5 — `src/devices/sound.cpp`

Align implementation parameter name with the (already correct) header.

| Function | Old parameter | New parameter |
|----------|--------------|---------------|
| `subTick` | `SubTick` | `subTick` |

**Files:** `src/devices/sound.cpp`

---

## Phase 6 — `src/devices/sony.cpp` (macros + static functions)

Rename macro parameters and static function parameters/locals.

**Macros:**

| Macro | Old parameter | New parameter |
|-------|--------------|---------------|
| `vSonyIsLocked` | `Drive_No` | `driveNo` |
| `vSonyIsMounted` | `Drive_No` | `driveNo` |

**Static functions — parameters:**

| Function | Old parameter | New parameter |
|----------|--------------|---------------|
| `vSonyNextPendingInsert0` | `Drive_No` | `driveNo` |
| `CheckReadableDrive` | `Drive_No` | `driveNo` |
| `vSonyTransferVM` | `IsWrite` | `isWrite` |
| `vSonyTransferVM` | `Buffera` | `buffera` |
| `vSonyTransferVM` | `Drive_No` | `driveNo` |
| `vSonyTransferVM` | `Sony_Start` | `sonyStart` |
| `vSonyTransferVM` | `Sony_Count` | `sonyCount` |
| `vSonyTransferVM` | `Sony_ActCount` | `sonyActCount` |
| `DC42BlockChecksum` | `Drive_No` | `driveNo` |
| `DC42BlockChecksum` | `Sony_Start` | `sonyStart` |
| `DC42BlockChecksum` | `Sony_Count` | `sonyCount` |
| `Drive_UpdateChecksums` | `Drive_No` | `driveNo` |
| `vSonyNextPendingInsert` | `Drive_No` | `driveNo` |
| `Drive_Transfer` | `IsWrite` | `isWrite` |
| `Drive_Transfer` | `Buffera` | `buffera` |
| `Drive_Transfer` | `Drive_No` | `driveNo` |
| `Drive_Transfer` | `Sony_Start` | `sonyStart` |
| `Drive_Transfer` | `Sony_Count` | `sonyCount` |
| `Drive_Transfer` | `Sony_ActCount` | `sonyActCount` |
| `Drive_Eject` | `Drive_No` | `driveNo` |
| `Drive_EjectDelete` | `Drive_No` | `driveNo` |
| `DriveVarsLocation` | `Drive_No` | `driveNo` |
| `Sony_PrimeTags` | `Drive_No` | `driveNo` |
| `Sony_PrimeTags` | `Sony_Start` | `sonyStart` |
| `Sony_PrimeTags` | `Sony_Count` | `sonyCount` |
| `Sony_PrimeTags` | `IsWrite` | `isWrite` |

**Local variables** (rename along with the parameters they shadow):

| Local | Old name | New name |
|-------|----------|----------|
| `vSonyTransferVM` | `Buffer` | `buffer` |
| `Drive_UpdateChecksums` | `DataOffset` | `dataOffset` |
| `Drive_UpdateChecksums` | `DataSize` | `dataSize` |
| `Sony_PrimeTags` | `TagOffset` | `tagOffset` |
| `Sony_Prime` (various) | `Drive_No` | `driveNo` |
| `Sony_Prime` | `IsWrite` | `isWrite` |
| `Sony_Prime` | `Buffera` | `buffera` |
| `Sony_Control` | `Drive_No` | `driveNo` |
| `Sony_Status` | `Drive_No` | `driveNo` |
| `Sony_Status` | `Src` | `src` |

**Files:** `src/devices/sony.cpp`

---

## Phase 7 — `src/platform/platform.h` (declarations)

Rename parameters in extern declarations. Must stay in sync with the
implementation files changed in Phases 8–9.

| Function | Old parameter | New parameter |
|----------|--------------|---------------|
| `AllocBlock` | `FillOnes` | `fillOnes` |
| `CheckPbuf` | `Pbuf_No` | `pbufNo` |
| `PbufGetSize` | `Pbuf_No` | `pbufNo` |
| `PbufGetSize` | `*Count` | `*count` |
| `PbufTransfer` | `Buffer` | `buffer` |
| `PbufTransfer` | `IsWrite` | `isWrite` |
| `vSonyIsInserted` (macro) | `Drive_No` | `driveNo` |
| `vSonyTransfer` | `IsWrite` | `isWrite` |
| `vSonyTransfer` | `Buffer` | `buffer` |
| `vSonyTransfer` | `Drive_No` | `driveNo` |
| `vSonyTransfer` | `Sony_Start` | `sonyStart` |
| `vSonyTransfer` | `Sony_Count` | `sonyCount` |
| `vSonyEject` | `Drive_No` | `driveNo` |
| `vSonyGetSize` | `Drive_No` | `driveNo` |
| `vSonyGetSize` | `*Sony_Count` | `*sonyCount` |
| `DiskRevokeWritable` | `Drive_No` | `driveNo` |
| `vSonyEjectDelete` | `Drive_No` | `driveNo` |
| `vSonyGetName` | `Drive_No` | `driveNo` |

**Files:** `src/platform/platform.h`

---

## Phase 8 — `src/platform/common/osglu_common.h` + `src/platform/common/osglu_common.cpp`

Rename parameters in common platform glue functions.

| Function | Old parameter | New parameter |
|----------|--------------|---------------|
| `PbufNewNotify` | `Pbuf_No` | `pbufNo` |
| `PbufDisposeNotify` | `Pbuf_No` | `pbufNo` |
| `CheckPbuf` | `Pbuf_No` | `pbufNo` |
| `PbufGetSize` | `Pbuf_No` | `pbufNo` |
| `PbufGetSize` | `*Count` | `*count` |
| `FirstFreeDisk` | `*Drive_No` | `*driveNo` |
| `DiskRevokeWritable` | `Drive_No` | `driveNo` |
| `DiskInsertNotify` | `Drive_No` | `driveNo` |
| `DiskEjectedNotify` | `Drive_No` | `driveNo` |
| `AllocBlock` | `FillOnes` | `fillOnes` |
| `DisconnectKeyCodes` | `KeepMask` | `keepMask` |

**Files:** `src/platform/common/osglu_common.h`, `src/platform/common/osglu_common.cpp`

---

## Phase 9 — `src/platform/common/param_buffers.h` + `src/platform/common/param_buffers.cpp`

Rename parameters in the parameter-buffer transfer function.

| Function | Old parameter | New parameter |
|----------|--------------|---------------|
| `PbufTransfer` | `Buffer` | `buffer` |
| `PbufTransfer` | `IsWrite` | `isWrite` |

**Files:** `src/platform/common/param_buffers.h`, `src/platform/common/param_buffers.cpp`

---

## Phase 10 — `src/platform/common/disk_io.h` + `src/platform/common/disk_io.cpp`

Rename parameters in disk I/O functions.

| Function | Old parameter | New parameter |
|----------|--------------|---------------|
| `vSonyTransfer` | `IsWrite` | `isWrite` |
| `vSonyTransfer` | `Buffer` | `buffer` |
| `vSonyTransfer` | `Drive_No` | `driveNo` |
| `vSonyTransfer` | `Sony_Start` | `sonyStart` |
| `vSonyTransfer` | `Sony_Count` | `sonyCount` |
| `vSonyTransfer` | `*Sony_ActCount` | `*sonyActCount` |
| `vSonyGetSize` | `Drive_No` | `driveNo` |
| `vSonyGetSize` | `*Sony_Count` | `*sonyCount` |
| `vSonyEject0` | `Drive_No` | `driveNo` |
| `vSonyEject` | `Drive_No` | `driveNo` |
| `vSonyEjectDelete` | `Drive_No` | `driveNo` |
| `vSonyGetName` | `Drive_No` | `driveNo` |

Also rename local variables using the old style inside these functions
(e.g. `NewSony_Count` → `newSonyCount`).

**Files:** `src/platform/common/disk_io.h`, `src/platform/common/disk_io.cpp`

---

## Execution Notes

- **Build gate:** After each phase, run `cmake --build` and confirm no
  errors.
- **Test gate:** After each phase, run `selftest.sh` to verify golden
  file regression tests still pass.
- **Commit:** One commit per phase with message format:
  `naming: rename parameters to camelCase in <file(s)>`
- **Phases 7–10** must be applied together in a single build cycle
  because `platform.h` declares functions implemented across multiple
  files. Apply Phase 7 first, then 8–10, then build.
