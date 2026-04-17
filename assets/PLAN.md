# Low Memory Globals — Implementation Plan

Move the hardcoded `kLowMemGlobals[]` table to a data-driven
`assets/globals.def` file, with types from the type system
(`assets/types.def`).  Source of truth: THINK C 3 headers (System 6).

Globals or structs not in THINK C 3:
- If clearly System 7 → remove
- If uncertain → comment out with `#?`

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Add missing struct types to types.def | |
| 2 | Create assets/globals.def from THINK C 3 | |
| 3 | Write GlobalRegistry parser | |
| 4 | Wire GlobalRegistry into startup and debugger | |
| 5 | Update imgui\_lomem\_tool to compile | |

Build gate: `cmake --preset macos && cmake --build --preset macos`
Test gate:  `./bld/macos/tests`

---

## Phase 1 — Add missing struct types to types.def

### Goal

Add struct definitions required by low-memory globals that do not yet
exist in `assets/types.def`.  Seven structs are needed: four used
directly as inline global types, and three used as pointer targets
(`^Struct` / `^^Struct`).

### Sub-tasks

1. **Add QHdr** — Queue header (used inline by EventQueue, VBLQueue,
   DrvQHdr, VCBQHdr, FSQHdr).

   ```
   struct QHdr {
       0  sword    qFlags
       2  Ptr      qHead
       6  Ptr      qTail
   }
   ```

2. **Add SysParmType** — Parameter RAM image (used inline by SysParam).

   ```
   struct SysParmType {
       0  byte     valid
       1  byte     aTalkA
       2  byte     aTalkB
       3  byte     config
       4  sword    portA
       6  sword    portB
       8  slong    alarm
       12 sword    font
       14 sword    kbdPrint
       16 sword    volClik
       18 sword    misc
   }
   ```

3. **Add ScrapStuff** — Scrap info record (used inline by ScrapInfo).

   ```
   struct ScrapStuff {
       0  slong    scrapSize
       4  Handle   scrapHandle
       8  sword    scrapCount
       10 sword    scrapState
       12 Ptr      scrapName
   }
   ```

4. **Add Pattern** — QuickDraw 8-byte pattern (used inline by
   DragPattern, DeskPattern).

   ```
   struct Pattern {
       0  byte     pat[8]
   }
   ```

5. **Add Region** — QuickDraw region header (target of `^^Region`
   for GrayRgn).  Source: Quickdraw.h.

   ```
   struct Region {
       0  sword    rgnSize
       2  Rect     rgnBBox
   }
   ```

6. **Add Zone** — Heap zone header (target of `^Zone` for TheZone,
   SysZone, ApplZone).  Source: MemoryMgr.h.  All primitives.

   ```
   struct Zone {
       0  Ptr      bkLim
       4  Ptr      purgePtr
       8  Ptr      hFstFree
       12 slong    zcbFree
       16 ProcPtr  gzProc
       20 sword    moreMast
       22 sword    flags
       24 sword    cntRel
       26 sword    maxRel
       28 sword    cntNRel
       30 sword    maxNRel
       32 sword    cntEmpty
       34 sword    cntHandles
       36 slong    minCBFree
       40 ProcPtr  purgeProc
       44 Ptr      sparePtr
       48 Ptr      allocPtr
       52 sword    heapData
   }
   ```

7. **Add VCB** — Volume control block (target of `^VCB` for
   DefVCBPtr).  Source: FileMgr.h.  All primitives + byte array.

   ```
   struct VCB {
       0  Ptr      qLink
       4  sword    qType
       6  sword    vcbFlags
       8  sword    vcbSigWord
       10 slong    vcbCrDate
       14 slong    vcbLsBkUp
       18 sword    vcbAtrb
       20 sword    vcbNmFls
       22 sword    vcbDirSt
       24 sword    vcbBlLn
       26 sword    vcbNmBlks
       28 slong    vcbAlBlkSiz
       32 slong    vcbClpSiz
       36 sword    vcbAlBlSt
       38 slong    vcbNxtFNum
       42 sword    vcbFreeBks
       44 byte     vcbVN[28]
       72 sword    vcbDrvNum
       74 sword    vcbDRefNum
       76 sword    vcbFSID
       78 sword    vcbVRefNum
       80 Ptr      vcbMAdr
       84 Ptr      vcbBufAdr
       88 sword    vcbMLen
       90 sword    vcbDirIndex
       92 sword    vcbDirBlk
   }
   ```

### Fence

- `types.def` parses without error (TypeRegistry load succeeds)
- Build passes
- Existing tests pass
- Commit: `"globals: add QHdr, SysParmType, ScrapStuff, Pattern, Region, Zone, VCB to types.def"`

---

## Phase 2 — Create assets/globals.def

### Goal

Create the data file containing all THINK C 3 low-memory globals,
organized by include-file section.

### File format

```
# globals.def — Macintosh low-memory global definitions
#
# Format: <hex_addr>  <type>  <name>  "<description>"
#
# Types: primitives (byte, sbyte, word, sword, long, slong, Ptr,
#        Handle, ProcPtr, OSType, Boolean, OSErr) or struct names
#        from types.def (QHdr, Pattern, Rect, ScrapStuff, etc.).
# Arrays: <type>[N] — N elements packed contiguously.
#         For PStr[N], N is the total buffer size incl. length byte.
# Commented-out entries: lines starting with #? are globals whose
#         System 6 status is uncertain.
# Sections: # ── SectionName ── comments group by THINK C 3 include file.
```

### Type mapping rules

| THINK C 3 type | globals.def type |
|----------------|-----------------|
| `int` | `sword` |
| `long` | `slong` |
| `char` | `sbyte` |
| `Byte` | `byte` |
| `Boolean` | `byte` |
| `THz` | `^Zone` |
| `GrafPtr`, `WindowPtr` | `^GrafPort` |
| `WindowPeek` | `^WindowRecord` |
| `VCB *` | `^VCB` |
| `RgnHandle` | `^^Region` |
| `Ptr` (raw address: screen base, ROM, RAM, hardware) | `Ptr` |
| `Handle` (target struct not in types.def: GDHandle, AuxWinHndl, etc.) | `Handle` |
| `HFSDefaults *`, `Ptr` (opaque pointer) | `Ptr` |
| `ProcPtr` | `ProcPtr` |
| `struct QHdr` | `QHdr` |
| `SysParmType` | `SysParmType` |
| `ScrapStuff` | `ScrapStuff` |
| `Pattern` | `Pattern` |
| `Rect` | `Rect` |
| `char[32]` (Str31 Pascal string buffer) | `Str31` |
| `char[N]` (other fixed-size byte buffers) | `byte[N]` |

Note: `^StructName` means pointer-to-struct (4 bytes, same as Ptr).
`^^StructName` means handle (pointer-to-pointer-to-struct, 4 bytes,
same as Handle).  This reuses the syntax already in `traps.def`.
Globals whose target struct is not yet in `types.def` (GDevice,
AuxWinRec, WidthTable, FamRec, etc.) remain bare `Handle`/`Ptr`
until those structs are added.

### Sections and globals

Complete listing by THINK C 3 include-file section.  Read each header
in `macdocs/THINK_C_3/Mac #includes/` and extract all globals defined
as `#define Name (*(type*)0xAddr)` patterns.

#### MacTypes (1 global)

| Addr | Type | Name |
|------|------|------|
| 0x28E | sword | ROM85 |

#### MemoryMgr (14 globals)

| Addr | Type | Name |
|------|------|------|
| 0x108 | Ptr | MemTop |
| 0x10C | Ptr | BufPtr |
| 0x114 | Ptr | HeapEnd |
| 0x118 | ^Zone | TheZone |
| 0x130 | Ptr | ApplLimit |
| 0x220 | sword | MemErr |
| 0x2A6 | ^Zone | SysZone |
| 0x2AA | ^Zone | ApplZone |
| 0x2AE | Ptr | ROMBase |
| 0x2B2 | Ptr | RAMBase |
| 0x31A | long | Lo3Bytes |
| 0x33C | ProcPtr | IAZNotify |
| 0x904 | Ptr | CurrentA5 |
| 0x908 | Ptr | CurStackBase |

#### OSUtil (3 globals)

| Addr | Type | Name |
|------|------|------|
| 0x15A | sword | SysVersion |
| 0x1F8 | SysParmType | SysParam |
| 0x20C | long | Time |

#### EventMgr (12 globals)

| Addr | Type | Name |
|------|------|------|
| 0x144 | sword | SysEvtMask |
| 0x14A | QHdr | EventQueue |
| 0x15C | sbyte | SEvtEnb |
| 0x16A | long | Ticks |
| 0x18E | sword | KeyThresh |
| 0x190 | sword | KeyRepThresh |
| 0x29A | ProcPtr | JGNEFilter |
| 0x29E | ProcPtr | Key1Trans |
| 0x2A2 | ProcPtr | Key2Trans |
| 0x2F0 | long | DoubleTime |
| 0x2F4 | long | CaretTime |
| 0x2F8 | sbyte | ScrDmpEnb |

#### Quickdraw (6 globals)

| Addr | Type | Name |
|------|------|------|
| 0x102 | sword | ScrVRes |
| 0x104 | sword | ScrHRes |
| 0x106 | sword | ScreenRow |
| 0x156 | long | RndSeed |
| 0x824 | Ptr | ScrnBase |
| 0x834 | Rect | CrsrPin |

#### WindowMgr (12 globals)

| Addr | Type | Name |
|------|------|------|
| 0x9D6 | ^WindowRecord | WindowList |
| 0x9DA | sword | SaveUpdate |
| 0x9DC | sword | PaintWhite |
| 0x9DE | ^GrafPort | WMgrPort |
| 0x9EE | ^^Region | GrayRgn |
| 0x9F6 | ProcPtr | DragHook |
| 0xA34 | Pattern | DragPattern |
| 0xA3C | Pattern | DeskPattern |
| 0xA64 | ^GrafPort | CurActivate |
| 0xA68 | ^GrafPort | CurDeactive |
| 0xA6C | ProcPtr | DeskHook |
| 0xA84 | ^GrafPort | GhostWindow |

#### MenuMgr (10 globals)

| Addr | Type | Name |
|------|------|------|
| 0xA0A | sword | TopMenuItem |
| 0xA0C | sword | AtMenuBottom |
| 0xA1C | Handle | MenuList |
| 0xA20 | sword | MBarEnable |
| 0xA24 | sword | MenuFlash |
| 0xA26 | sword | TheMenu |
| 0xA2C | ProcPtr | MBarHook |
| 0xA30 | ProcPtr | MenuHook |
| 0xB54 | long | MenuDisable |
| 0xBAA | sword | MBarHeight |

#### DialogMgr (1 global)

| Addr | Type | Name |
|------|------|------|
| 0xA8C | ProcPtr | ResumeProc |

#### TextEdit (2 globals)

| Addr | Type | Name |
|------|------|------|
| 0xAB0 | sword | TEScrpLength |
| 0xAB4 | Handle | TEScrpHandle |

#### FontMgr (5 globals)

| Addr | Type | Name |
|------|------|------|
| 0x984 | sword | ApFontID |
| 0xA63 | sbyte | FScaleDisable |
| 0xB2A | Handle | WidthTabHandle |
| 0xBF4 | sbyte | FractEnable |
| 0xBC2 | Handle | LastFOND |

#### FileMgr (7 globals)

| Addr | Type | Name |
|------|------|------|
| 0x210 | sword | BootDrive |
| 0x308 | QHdr | DrvQHdr |
| 0x338 | ProcPtr | EjectNotify |
| 0x34E | Ptr | FCBSPtr |
| 0x352 | ^VCB | DefVCBPtr |
| 0x356 | QHdr | VCBQHdr |
| 0x360 | QHdr | FSQHdr |

#### HFS (2 globals)

| Addr | Type | Name |
|------|------|------|
| 0x39E | Ptr | FmtDefaults |
| 0x3F6 | sword | FSFCBLen |

#### ResourceMgr (9 globals)

| Addr | Type | Name |
|------|------|------|
| 0xA50 | Handle | TopMapHndl |
| 0xA54 | Handle | SysMapHndl |
| 0xA58 | sword | SysMap |
| 0xA5A | sword | CurMap |
| 0xA5E | byte | ResLoad |
| 0xA60 | sword | ResErr |
| 0xAD8 | byte[20] | SysResName |
| 0xAF2 | ProcPtr | ResErrProc |
| 0xB9E | sword | RomMapInsert |

#### ScrapMgr (1 global)

| Addr | Type | Name |
|------|------|------|
| 0x960 | ScrapStuff | ScrapInfo |

#### PackageMgr (1 global)

| Addr | Type | Name |
|------|------|------|
| 0xAB8 | Handle[8] | AppPacks |

#### StdFilePkg (2 globals)

| Addr | Type | Name |
|------|------|------|
| 0x214 | sword | SFSaveDisk |
| 0x398 | long | CurDirStore |

#### SegmentLdr (6 globals)

| Addr | Type | Name |
|------|------|------|
| 0x2E0 | byte[16] | FinderName |
| 0x900 | sword | CurApRefNum |
| 0x910 | Str31 | CurApName |
| 0x934 | sword | CurJTOffset |
| 0x936 | sword | CurPageOption |
| 0xAEC | Handle | AppParmHandle |

#### PrintMgr (1 global)

| Addr | Type | Name |
|------|------|------|
| 0x944 | sword | PrintErr |

#### VRetraceMgr (1 global)

| Addr | Type | Name |
|------|------|------|
| 0x160 | QHdr | VBLQueue |

#### SoundDvr (1 global)

| Addr | Type | Name |
|------|------|------|
| 0x260 | sbyte | SdVolume |

#### DeviceMgr (9 globals)

| Addr | Type | Name |
|------|------|------|
| 0x11C | Ptr | UTableBase |
| 0x192 | ProcPtr[8] | Lvl1DT |
| 0x1B2 | ProcPtr[8] | Lvl2DT |
| 0x1D2 | sword | UnitNtryCnt |
| 0x1D4 | Ptr | VIA |
| 0x1D8 | Ptr | SCCRd |
| 0x1DC | Ptr | SCCWr |
| 0x1E0 | Ptr | IWM |
| 0x2BE | ProcPtr[4] | ExtStsDT |

#### Color (4 globals)

| Addr | Type | Name |
|------|------|------|
| 0x8A4 | Handle | MainDevice |
| 0x8A8 | Handle | DeviceList |
| 0x938 | sbyte | HiliteMode |
| 0xCC8 | Handle | TheGDevice |

#### ColorToolbox (3 globals)

| Addr | Type | Name |
|------|------|------|
| 0xCD0 | Handle | AuxWinHead |
| 0xCD4 | Handle | AuxCtlHead |
| 0xD50 | Handle | MenuCInfo |

**Total: 113 globals across 22 sections.**

### Globals from current table NOT in THINK C 3

The following exist in the current `kLowMemGlobals[]` but are absent
from THINK C 3 headers.  Disposition:

**Remove (System 7):**
- MMU32Bit (0x0CB2) — 32-bit addressing
- HiliteRGB (0x0DA0) — color highlight override
- JVBLTask (0x0D28) — VBL jump vector
- DTQueue (0x0D92), JDTInstall (0x0D9C) — Deferred Task queue/install
- TimeDBRA (0x0D00), TimeSCCDB (0x0D02), TimeSCSIDB (0x0DA6) — timing

**Comment out with `#?` (uncertain):**
- CPUFlag (0x012F), KbdLast (0x0218), KbdType (0x021E)
- SoundPtr (0x0262), SoundBase (0x0266), SoundLevel (0x027F),
  CurPitch (0x0280)
- PortBUse (0x0291), DSAlertTab (0x02BA), ABusVars (0x02D8)
- Scratch20 (0x01E4), Scratch8 (0x09FA), ToolScratch (0x09CE),
  ApplScratch (0x0A78)
- SPValid..SPMisc2 (0x1F8–0x20B — overlaps with SysParmType struct)
- BufTgFNum (0x02FC), BufTgFFlg (0x0300), BufTgFBkNum (0x0302),
  BufTgDate (0x0304)
- MinStack (0x031E), DefltStack (0x0322), GZRootHnd (0x0328)
- ToExtFS (0x03F2), DSAlertRect (0x03F8)
- JADBProc (0x06B8), QDColors (0x08B0)
- JournalFlag (0x08DE), WidthListHand (0x08E4), JournalRef (0x08E8),
  CrsrThresh (0x08EC)
- JFetch (0x08F4), JStash (0x08F8), JIODone (0x08FC)
- OldStructure (0x09E6), OldContent (0x09EA), SaveVisRgn (0x09F2)
- OneOne (0x0A02), MinusOne (0x0A06)
- TEDoText (0x0A70), TERecal (0x0A74)
- ANumber (0x0A98), ACount (0x0A9A), DABeeper (0x0A9C),
  DAStrings (0x0AA0)
- DSErrCode (0x0AF0), DlgFont (0x0AFA), WidthPtr (0x0B10)
- TmpResLoad (0x0B9F), IntlSpec (0x0BA0)
- SysFontFam (0x0BA6), SysFontSize (0x0BA8)
- SynListHandle (0x0D32)
- ROMFont0 (0x0980), ScrapSize (0x0960), ScrapHandle (0x0964),
  ScrapCount (0x0968), ScrapState (0x096A), ScrapName (0x096C)

Note: ScrapInfo (0x960) as ScrapStuff replaces the five individual scrap
fields (ScrapSize, ScrapHandle, ScrapCount, ScrapState, ScrapName).

### Address fix

Current table has VIA at 0x01DA.  THINK C 3 DeviceMgr.h has VIA at
0x1D4.  Use the THINK C 3 value.

### Fence

- `assets/globals.def` exists with 113+ entries across 22 sections
- No code changes yet (pure data file)
- Commit: `"globals: create globals.def from THINK C 3 headers"`

---

## Phase 3 — Write GlobalRegistry parser

### Goal

Create `src/lang/global_registry.h` and `src/lang/global_registry.cpp`
to parse `assets/globals.def` and expose the data through a C++ API.

### Data structures

```cpp
// src/lang/global_registry.h

#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class TypeRegistry;  // forward

struct GlobalDef {
    std::string name;          // e.g. "MemTop"
    uint32_t    addr;          // low-memory address
    uint16_t    size;          // computed from type (bytes)
    std::string typeName;      // type from types.def, e.g. "Ptr", "QHdr"
    uint16_t    count;         // array element count (1 for scalars)
    std::string section;       // include-file section, e.g. "MemoryMgr"
    std::string brief;         // one-line description
};

class GlobalRegistry {
public:
    // Parse globals.def.  Types validated against TypeRegistry.
    // Returns count of loaded globals.
    int load(const std::filesystem::path &path,
             const TypeRegistry &types);

    std::span<const GlobalDef> globals() const;
    int count() const;

    // Lookup helpers (binary search, sorted at load time).
    const GlobalDef *findByName(std::string_view name) const;
    const GlobalDef *findByAddr(uint32_t addr) const;

private:
    std::vector<GlobalDef> globals_;
    std::vector<int>       byName_;   // indices sorted by name
    std::vector<int>       byAddr_;   // indices sorted by addr
};
```

### Parser logic (`global_registry.cpp`)

Follow the existing pattern from `trap_defs.cpp` / `type_registry.cpp`:

1. Open file, read line by line with `std::getline()`
2. Skip blank lines and `#` comments (but NOT `#?` — those are
   commented-out globals, skip them for now too)
3. Track current section from `# ── SectionName ──` comment lines
   (parse the section name between `── ` delimiters)
4. Parse data lines: `<hex_addr> <type> <name> "<description>"`
   - Address: parse `0xNNN` hex literal
   - Type: handle prefixes first:
     - `^^Name` → handle-to-struct, size = 4, store typeName as `^^Name`
     - `^Name` → pointer-to-struct, size = 4, store typeName as `^Name`
     - Then split on `[` to get base type and optional array count
   - Name: single token
   - Description: everything inside double quotes
5. Resolve size: `^`/`^^` prefixed types are always 4 bytes.
   For others, call `TypeRegistry::sizeOf(typeName)` which already
   handles both primitives and struct types.  For arrays, multiply
   by count.

   Note: `TypeRegistry::sizeOf()` does not handle `^`/`^^` prefixes
   itself — the caller must strip them first.  This mirrors what
   `trap_defs.cpp` does (inline `^` check, then lookup the bare
   struct name).  A future refactor could add a
   `TypeRegistry::resolveSize()` that handles prefixes, arrays,
   and `^^` uniformly — but that's out of scope here.
6. Validate: for `^Name`/`^^Name`, strip prefix and verify the struct
   exists in TypeRegistry (warn if not, but still load).
7. After all lines are parsed, build `byName_` and `byAddr_` indices.

### Files to create

- `src/lang/global_registry.h` — header as shown above
- `src/lang/global_registry.cpp` — parser implementation

### Files to modify

- `src/lang/CMakeLists.txt` (or parent) — add new source files to build

### Fence

- New files compile cleanly
- Build passes (no consumers yet, just the library)
- Commit: `"globals: add GlobalRegistry parser for globals.def"`

---

## Phase 4 — Wire GlobalRegistry into startup and debugger

### Goal

Load `globals.def` at startup, replace the hardcoded `kLowMemGlobals[]`
table, and update the debugger's symbol resolution.

### Sub-tasks

1. **Load at startup** — In `src/core/main.cpp`, after TypeRegistry
   load, add GlobalRegistry load:
   ```cpp
   g_globalRegistry.load(assetsDir / "globals.def", g_typeRegistry);
   ```
   Add `GlobalRegistry g_globalRegistry;` as a global or on the
   appropriate object.

2. **Update debugger/symbols.cpp** — Replace references to
   `kLowMemGlobals[]` / `kLowMemCount` with `GlobalRegistry` API:
   - `s_globalsByName` → use `g_globalRegistry.findByName()`
   - `s_globalsByAddr` → use `g_globalRegistry.findByAddr()`
   - Or populate the existing sorted indices from
     `g_globalRegistry.globals()`.

3. **Strip lomem\_globals.h** — Remove `LMType`, `LMCategory` enums,
   `LMGlobal` struct, `kLowMemGlobals[]`, `kLowMemCount`,
   `kLMCategoryLabels[]`.  Keep the snapshot/format helpers
   (`Lomem_SnapshotTake`, `lomem_snapshot_changed`) as they work
   on raw `g_ram[]` and are independent of the globals table.

4. **Strip lomem\_globals.cpp** — Remove the entire hardcoded table
   and category labels.  Keep `Lomem_SnapshotTake`,
   `lomem_snapshot_changed`, and the `rd8`/`rd16`/`rd32` helpers.
   Remove `lomem_format_value` and `lomem_type_label` (they depend
   on `LMType`; phase 5 will rewrite the display).

### Fence

- Build passes
- Debugger symbol resolution still works
- Tests pass
- Commit: `"globals: wire GlobalRegistry, remove hardcoded table"`

---

## Phase 5 — Update imgui\_lomem\_tool to compile

### Goal

Make `imgui_lomem_tool.cpp` compile against the new `GlobalRegistry`
API.  The UI can be simplified — a proper redesign will come later.

### Sub-tasks

1. **Include GlobalRegistry header** — Replace `lomem_globals.h`
   include (or keep it for snapshot helpers) and add
   `global_registry.h`.

2. **Iterate GlobalRegistry** — Replace `kLowMemGlobals[i]` /
   `kLowMemCount` with `g_globalRegistry.globals()`.

3. **Remove category filter** — The LMCategory enum no longer exists.
   Either replace with section-based filtering (combo box listing
   section names from GlobalRegistry) or remove filtering temporarily.
   Simplest: remove the category combo entirely.

4. **Simplified value display** — Write a local helper that formats
   values based on `GlobalDef::typeName`:
   - `"byte"` / `"sbyte"` → `$XX`
   - `"word"` / `"sword"` / `"OSErr"` → `$XXXX`
   - `"long"` / `"slong"` → `$XXXXXXXX`
   - `"Ptr"` / `"ProcPtr"` / `"^*"` → `$XXXXXXXX`
   - `"Handle"` / `"^^*"` → `$XXXXXXXX (H)`
   - `"OSType"` → `'XXXX'`
   - `"Rect"` → `(T,L)-(B,R)`
   - `"Pattern"` → `XX XX XX XX XX XX XX XX`
   - `"Str31"` / `"Str255"` → `"string"` (Pascal string decode)
   - Anything else (structs, arrays) → raw hex bytes

5. **Type column** — Show `GlobalDef::typeName` (possibly truncated)
   instead of the old `lomem_type_label()`.

6. **Table columns** — Keep Name, Addr, Type, Value, Brief.
   Replace `LMGlobal` field accesses with `GlobalDef` field accesses.

### Fence

- Build passes with no warnings related to lomem/globals
- Low Memory Globals panel opens and displays data
- Tests pass
- Commit: `"globals: update imgui lomem tool for GlobalRegistry"`