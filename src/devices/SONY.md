# Sony Disk Driver — Architecture & Implementation

## Overview

The `SonyDevice` class emulates the Macintosh `.Sony` floppy disk driver.
Rather than emulating the actual Sony floppy drive hardware (IWM signals,
track stepping, GCR encoding), maxivmac **patches the ROM** to replace the
real `.Sony` DRVR with a thin 68 k stub that forwards every driver call to
the host via a memory-mapped I/O extension interface.  The host then
performs the actual I/O against disk-image files on the host file system.

### Key files

| File | Role |
|------|------|
| [sony.h](sony.h) | `SonyDevice` class declaration |
| [sony.cpp](sony.cpp) | Full host-side driver logic (~1 600 lines) |
| [rom.cpp](rom.cpp) | `Sony_Install()` — patches the 68 k stub + extension header into ROM |
| [../../extras/mydriver/disk/mydriver.a](../../extras/mydriver/disk/mydriver.a) | 68 k assembly source of the patched-in driver |
| [../../src/core/machine.h](../core/machine.h) | Extension IDs, `EXTN_DAT_*` layout, `kcom_callcheck` |
| [../../src/core/machine.cpp](../core/machine.cpp) | `extnAccess()` — MMIO dispatch that routes to `SonyDevice` |
| [../../src/platform/common/disk_io.h](../platform/common/disk_io.h) | Platform-side file I/O declarations |
| [../../src/platform/common/disk_io.cpp](../platform/common/disk_io.cpp) | `vSonyTransfer`, `vSonyEject`, `vSonyGetSize` — host `FILE *` ops |
| [../../src/cpu/m68k.cpp](../cpu/m68k.cpp) | `DiskInsertedPsuedoException()` — CPU-level pseudo-interrupt for mounts |

---

## 1. ROM Patching

At boot, `Sony_Install()` (rom.cpp) overwrites the real `.Sony` driver
in the ROM image with:

1. **The 68 k driver stub** (`sony_driver[]`, ~250 bytes of compiled
   machine code from `mydriver.a`).
2. **An 8-byte extension header** appended right after the stub:
   ```
   +0  word   kcom_callcheck  (0x5B17)  — magic sentinel
   +2  word   kExtnSony       (2)       — extension ID
   +4  long   extnBlockBase             — MMIO poke address
   ```
3. **The disk icon** (`my_disk_icon[]`) — used for Finder's drive icon.
4. **An optional screen-size hack** (screen resolution override).

The base address in ROM is model-specific:

| Model | ROM Offset |
|-------|-----------|
| Twig43 | `0x1836` |
| Twiggy | `0x16E4` |
| 128 K | `0x1690` |
| Plus/512 Ke | `0x17D30` |
| SE/Classic | `0x34680` |
| II/IIx | `0x2D72C` |

The driver name is `.Sony` for all models except Twiggy/Twig43, where it
is patched to `Disk`.

---

## 2. Guest-Side 68 k Driver (mydriver.a)

The 68 k stub in `extras/mydriver/disk/mydriver.a` implements a standard
Macintosh DRVR resource (device driver) with the five entry points
required by the Device Manager:

| Entry | Purpose | Extension command |
|-------|---------|-------------------|
| `DOpen` | Allocate SonyVars, init drive queue | `kCmndSonyOpenA/B/C` (three phases) |
| `DPrime` | Read/write block I/O | `kCmndSonyPrime` |
| `DControl` | Eject, format, verify, icon, drive info | `kCmndSonyControl` |
| `DStatus` | Drive status query | `kCmndSonyStatus` |
| `DClose` | Close (always refused) | `kCmndSonyClose` |

Additionally, `DUpdate` handles the disk-inserted callback (invoked by
`diskInsertedPseudoException`); it calls `kCmndSonyMount` and then
`_PostEvent` to notify the system.

### How the stub calls the host — the TailData trick

The `TailData` label at the very end of the assembly (before `ENDMAIN`)
marks the location where `Sony_Install()` appends the extension header
(magic + extension ID + poke address).  Every entry point uses the same
pattern:

```asm
; Build a mini stack frame: [extension header ptr] [command word]
LEA        TailData, A0
MOVE.L     (A0)+, -(A7)       ; push checkval + extension ID (first long of header)
MOVEA.L    (A0), A0            ; A0 = poke address (extnBlockBase)
MOVE.L     A7, (A0)           ; WRITE stack-frame address to MMIO port
                               ;   → this triggers extnAccess() on the host
ADDA.W     #6, A7             ; pop frame
MOVE.W     (A7)+, D0          ; pop result code
```

The write to `extnBlockBase` is a **memory-mapped I/O** access.  It hits
the ATT (Address Translation Table) entry for the extension device, which
calls `extnAccess()`.

---

## 3. Host Extension Dispatch

### Legacy extension mechanism (`extnBlockBase + $00`)

The 68 k stub writes a 32-bit guest-RAM address to two consecutive MMIO
word registers:

| Word offset | Name | Purpose |
|-------------|------|---------|
| 0 | `kDSK_Params_Hi` | Upper 16 bits of parameter-block address |
| 1 | `kDSK_Params_Lo` | Lower 16 bits — **write triggers dispatch** |

On the second write, the host reconstructs the 32-bit pointer `p` and
reads the parameter block from guest RAM:

```
Parameter block (guest RAM, variable size):
  +$00  word  EXTN_DAT_CHECKVAL   — must be 0x5B17 (cleared after dispatch)
  +$02  word  EXTN_DAT_EXTENSION  — extension ID (kExtnDisk=1 or kExtnSony=2)
  +$04  word  EXTN_DAT_COMMND     — command code
  +$06  word  EXTN_DAT_RESULT     — result code (written by host)
  +$08  ...   EXTN_DAT_PARAMS     — command-specific parameters
```

Routing (in `extnAccess()`, machine.cpp):

| Extension ID | Value | Handler |
|-------------|-------|---------|
| `kExtnFindExtn` | 0 | `ExtnFind_Access()` |
| `kExtnDisk` | 1 | `SonyDevice::extnDiskAccess()` |
| `kExtnSony` | 2 | `SonyDevice::extnSonyAccess()` |
| `kExtnVideo` | 3 | `VideoDevice::extnVideoAccess()` |
| `kExtnParamBuffers` | 4 | `ExtnParamBuffers_Access()` |
| `kExtnHostTextClipExchange` | 5 | `ExtnHostTextClipExchange_Access()` |
| `kExtnExtFS` | 6 | (unused via legacy path) |

The Sony device uses **two** extension IDs:

- **`kExtnDisk` (1)** — Low-level disk commands (NDrives, raw Read/Write,
  Eject, GetSize, etc.).  Called by `extnDiskAccess()`.
- **`kExtnSony` (2)** — High-level Toolbox-level driver commands (Prime,
  Control, Status, Open, Close, Mount).  Called by `extnSonyAccess()`.

### Register-block mechanism (`extnBlockBase + $20`)

This is the **newer** mechanism, used by ClipSync and SharedDrive.  It is
**not** used by the Sony driver.  See §8 for how Sony could be migrated.

---

## 4. The Two Extension Interfaces in Detail

### 4.1 Low-Level Disk Extension (`kExtnDisk` → `extnDiskAccess`)

Provides raw block-level disk access plus drive management:

| Command | Code | Parameters (offsets from `EXTN_DAT_PARAMS`) | Action |
|---------|------|---------------------------------------------|--------|
| `kCmndVersion` | 0 | — | Returns version 2 |
| `kCmndDiskNDrives` | 1 | → `+8`: count | Return number of drives |
| `kCmndDiskRead` | 2 | `+16`: buffer, `+20`: drive, `+8`: start, `+12`: count | Block read |
| `kCmndDiskWrite` | 3 | (same layout) | Block write |
| `kCmndDiskEject` | 4 | `+20`: drive | Eject and close image |
| `kCmndDiskGetSize` | 5 | `+20`: drive → `+12`: size | Return image data size |
| `kCmndDiskGetCallBack` | 6 | → `+16`: address | Get mount callback |
| `kCmndDiskSetCallBack` | 7 | `+16`: address | Set mount callback |
| `kCmndDiskQuitOnEject` | 8 | — | Enable quit-after-eject |
| `kCmndDiskFeatures` | 9 | → `+8`: bitmask | Report supported features |
| `kCmndDiskNextPendingInsert` | 10 | → `+20`: drive | Get next unmounted drive |
| `kCmndDiskGetRawMode` | 11 | → `+16`: flag | Check raw-mode flag |
| `kCmndDiskSetRawMode` | 12 | `+16`: flag | Set raw-mode flag |
| `kCmndDiskNew` | 13 | `+8`: size, `+12`: name pbuf | Request new disk creation |
| `kCmndDiskGetNewWanted` | 14 | → `+16`: flag | Check if new disk pending |
| `kCmndDiskEjectDelete` | 15 | `+20`: drive | Eject and delete image |
| `kCmndDiskGetName` | 16 | `+8`: drive → `+12`: name pbuf | Get drive name |

### 4.2 High-Level Sony Extension (`kExtnSony` → `extnSonyAccess`)

Implements the Toolbox-level `.Sony` device driver operations:

| Command | Code | Purpose |
|---------|------|---------|
| `kCmndVersion` | 0 | Return protocol version |
| `kCmndSonyPrime` | 1 | Read/write I/O (from Device Manager `_Read`/`_Write`) |
| `kCmndSonyControl` | 2 | Control calls (eject, format, verify, icon, drive info) |
| `kCmndSonyStatus` | 3 | Status calls (drive status) |
| `kCmndSonyClose` | 4 | Close driver (always returns `closErr`) |
| `kCmndSonyOpenA` | 5 | Open phase A — return required SonyVars size |
| `kCmndSonyOpenB` | 6 | Open phase B — initialize SonyVars, drive queue |
| `kCmndSonyOpenC` | 7 | Open phase C — set mount callback address |
| `kCmndSonyMount` | 8 | Mount a disk — set drive variables, return event message |

---

## 5. Data Flow — Complete Life Cycle

### 5.1 Boot Sequence

```
ROM loads → Sony patcher replaces .Sony DRVR in ROM image
  → System calls _Open on .Sony
    → DOpen (68k stub)
      → kCmndSonyOpenA → host returns required SonyVars size
      → guest calls _NewPtr,Sys,Clear to allocate SonyVars
      → kCmndSonyOpenB → host initializes:
          - SonyVars: checkval (0x841339E2), pokeaddr, NumDrives, extension ID
          - Drive variables: kDiskInPlace=0, kInstalled=1, drive number, refnum
          - Unit Table: copies DCE for driver reuse
          - Returns: first drive vars pointer, step, count, first drive#, driver ref
      → guest calls _AddDrive for each drive entry
      → guest installs VBL task (workaround for Winter Games)
      → guest installs Time Manager task (workaround for System 6.0.8 bug)
      → kCmndSonyOpenC → host stores mount callback (DUpdate address)
        → on Mac II family: callback |= 0x40000000 (32-bit clean marker)
```

### 5.2 Disk Insertion (Host → Guest)

```
User drops disk image file
  → Sony_Insert1() opens FILE*, calls DiskInsertNotify()
    → sets bit in g_sonyInsertedMask

SonyDevice::update() runs every 1/60s tick:
  → vSonyNextPendingInsert(): finds drive with InsertedMask & ~MountedMask
    → reads image header, detects format (DC42, raw HFS/MFS, partitioned)
    → configures s_imageDataOffset[], s_imageDataSize[], s_imageTagOffset[]
    → sets MountedMask bit
  → builds data word: drive index | (locked ? 0x00FF0000 : 0)
  → calls g_cpu.diskInsertedPseudoException(s_mountCallBack, data)
    → CPU: saves SR/PC, pushes data, jumps to DUpdate in 68k driver

DUpdate (68k stub):
  → calls kCmndSonyMount extension
    → Sony_Mount() (host): sets drive variables (format, write-protect, DiskInPlace)
    → returns eventMsg = driveNo + 1
  → calls _PostEvent(diskEvt, eventMsg)
    → Finder processes disk-inserted event, mounts volume
```

Rate limiting: `s_delayUntilNextInsert = 240` ticks (4 seconds) between
inserts, shortened to 4 ticks once `kDriveStatus` is called (indicating
the system has processed the previous mount).

### 5.3 Block Read (Guest → Host)

```
Application calls _Read (or File Manager reads)
  → Device Manager dispatches to .Sony DPrime
    → DPrime (68k stub): pushes ParamBlk + DCtlPtr, calls kCmndSonyPrime

Sony_Prime() (host):
  ← reads ParamBlk from guest RAM:
      driveNo  = ioVRefNum - 1
      IOTrap   = ioTrap (0xA002=read, 0xA003=write)
      sonyStart = dCtlPosition
      sonyCount = ioReqCount
      buffera   = ioBuffer
  → validates: drive exists, disk in place (0x02=clamped), block-aligned
  → calls Drive_Transfer(isWrite, buffera, driveNo, start, count, &actual)
    → adjusts for image header: offset += s_imageDataOffset[driveNo]
    → calls vSonyTransferVM():
      → get_real_address0(): translates guest address → host pointer
      → calls vSonyTransfer() (platform I/O):
        → fseek(g_drives[driveNo], offset, SEEK_SET)
        → fread(buffer, 1, count, g_drives[driveNo])
  → if tag buffer set: calls Sony_PrimeTags() for 12-byte/block metadata
  → updates dCtlPosition, ioActCount, ioResult, DskErr ($0142)
```

### 5.4 Control Calls

| csCode | Name | Action |
|--------|------|--------|
| 1 | KillIO | Returns `miscErr` |
| 5 | VerifyDisk | No-op, returns `noErr` |
| 6 | FormatDisk | No-op, returns `noErr` |
| 7 | EjectDisk | Clears drive vars, calls `Drive_Eject()` → `vSonyEject()` (fclose) |
| 8 | SetTagBuffer | Stores guest-RAM tag buffer address |
| 9 | TrackCacheControl | No-op on 512Ke+; `controlErr` on 128K |
| 21 | DriveIcon | Returns `g_diskIconAddr` for large (non-floppy) disks |
| 23 | DriveInfo | Returns drive type+position (SE+ only) |

### 5.5 Eject (Guest → Host)

```
Finder / application calls _Eject
  → DeviceManager calls DControl with csCode=7
    → kCmndSonyControl extension
      → Sony_Control(): clears DiskInPlace, WriteProt, resets QRefNum
        → Drive_Eject():
          → clears MountedMask bit
          → Drive_UpdateChecksums(): recalculates DC42 checksums if applicable
          → vSonyEject(): fclose(g_drives[driveNo]), free(name)
          → if s_quitOnEject && no disks left: g_forceMacOff = true
```

---

## 6. Disk Image Format Detection

On insertion, `vSonyNextPendingInsert()` reads the first 32 KB of the
image and tries three format detectors in order:

1. **Disk Copy 4.2** — signature `0x0100` at offset 82 (`kDC42offset_private`).
   Validates data/tag sizes, checksums.  Sets `dataOffset = 84`,
   `dataSize` and optional `tagOffset`.

2. **Raw HFS or MFS** — checks for `$4244` (HFS) or `$D2D7` (MFS) volume
   signature at offset `$400` (boot block + 1 KB).  Validates VBM start,
   allocation block size, clump size, and volume name length.

3. **Partitioned HFS** — scans first 64 blocks for Apple Partition Map
   entries (`$504D` magic).  Locates the `Apple_HFS` partition and sets
   `dataOffset`/`dataSize` to its extent.

If no format is detected: `WarnMsgUnsupportedDisk()` and returns `miscErr`.

When `g_sonyRawMode` is true, format detection is skipped entirely —
the whole file is treated as raw block data.

---

## 7. State Management

### Per-Drive Host State

| Variable | Type | Scope | Purpose |
|----------|------|-------|---------|
| `g_drives[i]` | `FILE *` | disk_io.cpp | Open file handle |
| `g_driveNames[i]` | `char *` | disk_io.cpp | Host file path |
| `g_sonyInsertedMask` | `uint32_t` | platform.h | Bit per inserted drive |
| `g_sonyWritableMask` | `uint32_t` | platform.h | Bit per writable drive |
| `s_sonyMountedMask` | `uint32_t` | sony.cpp | Bit per OS-mounted drive |
| `s_imageDataOffset[i]` | `uint32_t` | sony.cpp | Byte offset to data in image file |
| `s_imageDataSize[i]` | `uint32_t` | sony.cpp | Data region size in bytes |
| `s_imageTagOffset[i]` | `uint32_t` | sony.cpp | Tag region offset (0 = none) |

### Per-Drive Guest State (SonyVars, in guest RAM)

Located at `get_vm_long(SonyVarsPtr)` + `FirstDriveVarsOffset` +
`EachDriveVarsSize × i`:

| Offset | Size | Name | Purpose |
|--------|------|------|---------|
| 0 | 2 | kTrack | Current track (unused by emulation) |
| 2 | 1 | kWriteProt | `$FF` = locked, `$00` = writable |
| 3 | 1 | kDiskInPlace | `$00`=none, `$01`=inserted, `$02`=clamped |
| 4 | 1 | kInstalled | `$01` = drive present |
| 5 | 1 | kSides | `$00` = single, `$FF` = double |
| 6 | 4 | kQLink | Next drive queue link |
| 10 | 2 | kQType | `0` = size saved, `1` = very large |
| 12 | 2 | kQDriveNo | Drive number (1-based) |
| 14 | 2 | kQRefNum | Driver refnum (`$FFFB` = .Sony) |
| 16 | 2 | kQFSID | File system ID (0 = MacOS) |
| 18 | 2 | kQDrvSz | Low word of block count (for large disks) |
| 20 | 2 | kQDrvSz2 | High word of block count |

SonyVars header (model-dependent offset):

| Offset | Size | Purpose |
|--------|------|---------|
| 16 | 4 | `kcom_checkval` (`0x841339E2`) — integrity marker |
| 20 | 4 | `pokeaddr` — `extnBlockBase` for extension calls |
| 24 | 2 | `NumDrives` |
| 26 | 2 | `kExtnDisk` — extension ID for low-level calls |
| 28 | ... | `NullTask` (Time Manager task, post-128K only) |

### Global State

| Variable | Purpose |
|----------|---------|
| `s_mountCallBack` | Guest address of `DUpdate` routine (set in OpenC) |
| `s_delayUntilNextInsert` | Rate limiter for mount events |
| `s_quitOnEject` | Exit emulator when last disk is ejected |
| `s_tagBuffer` | Guest-RAM address for tag buffer (12 bytes/block metadata) |

---

## 8. Future Directions — Unified Host ↔ Guest Communication

### Current state of affairs

The codebase has **two** host ↔ guest communication mechanisms:

| | Legacy Extension (PB) | Register Block |
|---|---|---|
| **MMIO address** | `extnBlockBase + $00` | `extnBlockBase + $20` |
| **Trigger** | Two 16-bit writes (hi then lo) | Single 16-bit write to command register |
| **Parameters** | Variable-size block in guest RAM | Fixed 7 × 32-bit registers in host |
| **Result** | Written back into guest-RAM PB | Readable from host result register |
| **Requires** | Guest builds PB, writes checkval + extension ID | Guest writes params, writes command word |
| **Users** | Sony, Video, ParamBuffers, legacy HTCE | ClipSync, SharedDrive (ExtFS) |

The register-block mechanism is cleaner, faster, and more extensible.
The legacy mechanism requires the guest to maintain a parameter block in
RAM, write a magic sentinel, and use a two-step MMIO trigger.

### Legacy mechanisms slated for removal

- **ClipIn/ClipOut** desk accessories — replaced by the ClipSync INIT
- **ImportFL/ExportFL** applications — replaced by SharedDrive
- **ExtnHostTextClipExchange** (kExtnHTCE) — replaced by register-block
  clipboard commands (`$100`–`$108`)
- **ExtnParamBuffers** — used only by the above legacy tools

### Goal: migrate Sony to the register-block mechanism

The Sony driver should be migrated from the legacy parameter-block
protocol to the same register-block RPC used by ClipSync and SharedDrive.
This would:

1. **Unify all host ↔ guest communication** under a single mechanism.
2. **Simplify the 68 k stub** — no TailData trick, no checkval sentinel,
   no two-step MMIO trigger.  Just `MOVE.L param, $F0C024; MOVE.W #cmd, $F0C020`.
3. **Remove the legacy dispatch path** entirely (the `kDSK_Params_Hi` /
   `kDSK_Params_Lo` two-write mechanism), since it would have no users.
4. **Allow the legacy extensions to be dropped** — `kExtnFindExtn`,
   `kExtnParamBuffers`, `kExtnHostTextClipExchange` become dead code.

### Proposed register-block command range

Following the existing convention:
- `$100`–`$1FF`: Clipboard
- `$200`–`$2FF`: SharedDrive/ExtFS
- **`$300`–`$3FF`: Disk / Sony** (new)

### Proposed command mapping

| Code | Name | Replaces | Parameters |
|------|------|----------|------------|
| `$300` | `DiskVersion` | `kCmndVersion` | → p0 = version |
| `$301` | `DiskNDrives` | `kCmndDiskNDrives` | → p0 = count |
| `$302` | `DiskRead` | `kCmndDiskRead` | p0=buffer, p1=drive, p2=start, p3=count → p3=actual |
| `$303` | `DiskWrite` | `kCmndDiskWrite` | (same) |
| `$304` | `DiskEject` | `kCmndDiskEject` | p0=drive |
| `$305` | `DiskGetSize` | `kCmndDiskGetSize` | p0=drive → p1=size |
| `$306` | `DiskGetCallback` | `kCmndDiskGetCallBack` | → p0=address |
| `$307` | `DiskSetCallback` | `kCmndDiskSetCallBack` | p0=address |
| `$308` | `DiskQuitOnEject` | `kCmndDiskQuitOnEject` | — |
| `$309` | `DiskFeatures` | `kCmndDiskFeatures` | → p0=bitmask |
| `$30A` | `DiskNextPending` | `kCmndDiskNextPendingInsert` | → p0=drive |
| `$30B` | `DiskNew` | `kCmndDiskNew` | p0=size, p1=name_pbuf |
| `$30C` | `DiskGetName` | `kCmndDiskGetName` | p0=drive → p1=name_pbuf |
| `$310` | `SonyPrime` | `kCmndSonyPrime` | p0=ParamBlk, p1=DeviceCtl |
| `$311` | `SonyControl` | `kCmndSonyControl` | p0=ParamBlk, p1=DeviceCtl |
| `$312` | `SonyStatus` | `kCmndSonyStatus` | p0=ParamBlk, p1=DeviceCtl |
| `$313` | `SonyClose` | `kCmndSonyClose` | p0=ParamBlk |
| `$314` | `SonyOpenA` | `kCmndSonyOpenA` | → p0=required_size |
| `$315` | `SonyOpenB` | `kCmndSonyOpenB` | p0=SonyVars, p1=size, ... |
| `$316` | `SonyOpenC` | `kCmndSonyOpenC` | p0=callback_addr |
| `$317` | `SonyMount` | `kCmndSonyMount` | p0=data → p1=eventMsg |

### Migration strategy

#### Phase 1 — Host-side dual support

Add a new `regDiskDispatch()` handler in machine.cpp, registered in the
`$300`–`$3FF` range alongside the existing Clip and ExtFS dispatchers.
The handler calls the same `SonyDevice` methods, just unpacking params
from `s_regParam[]` instead of guest-RAM.

The legacy path remains for backward compatibility.

#### Phase 2 — New 68 k driver stub

Write a new `mydriver.a` that uses the register-block calling convention:

```asm
; Example: DPrime via register block
DPrime:
    MOVEA.L  SonyVars, A2
    MOVEA.L  20(A2), A2          ; A2 = regBlockBase (pokeaddr + $20)
    MOVE.L   A0, 4(A2)           ; p0 = ParamBlk ptr
    MOVE.L   A1, 8(A2)           ; p1 = DCtlPtr
    MOVE.W   #$0310, (A2)        ; command = SonyPrime → triggers dispatch
    MOVE.W   2(A2), D0           ; D0 = result
    ...
```

This eliminates the TailData trick and the two-step MMIO poke.

#### Phase 3 — Remove legacy dispatch

Once the new driver is validated:
1. Remove the `kDSK_Params_Hi` / `kDSK_Params_Lo` two-write path from
   `extnAccess()`.
2. Remove `kExtnFindExtn`, `kExtnParamBuffers`, `kExtnHostTextClipExchange`.
3. Remove the `kcom_callcheck` sentinel mechanism entirely.
4. Remove the ClipIn/ClipOut desk accessories and ImportFL/ExportFL apps.
5. The 8-byte extension header in ROM is no longer needed — the driver
   just needs to know `extnBlockBase + $20` (can be a compile-time
   constant or read from a fixed low-memory location).

#### Phase 4 — Further simplification

With all extensions on the register block, consider whether the
high-level Sony commands (`$310`–`$317`) should remain, or whether the
68 k stub should be made even thinner — for example, the Prime/Control/
Status logic could be moved entirely into the 68 k stub (reading
ParamBlk fields and calling low-level `$302`/`$303` DiskRead/DiskWrite
directly), removing the need for the host to reach into guest-RAM
parameter blocks at all.  This would make the driver self-contained on
the guest side and reduce host↔guest coupling.

### Constraints

- The **SonyVars** structure and **drive queue** layout are dictated by
  the Mac OS and cannot change.  The guest must still maintain these in
  RAM for the File Manager, Finder, and applications to function.
- The **mount callback** mechanism (pseudo-exception to `DUpdate`) is
  fundamental to how disk insertion works and must be preserved.
- The register block is limited to 12 × 32-bit parameters.  The current
  Sony commands all fit comfortably within this limit.
- Some commands (Prime, Control, Status) still need to read/write the
  Toolbox ParamBlockRec in guest RAM.  The register block provides the
  PB address; the host reads it via `get_vm_long()`.  This is the same
  pattern SharedDrive uses for its PB-based commands (`$230`–`$245`).
