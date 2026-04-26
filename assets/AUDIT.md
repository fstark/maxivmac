# Trap Audit — TRAP_TABLE.md vs traps.def

Cross-reference of Inside Macintosh trap table vs maxivmac trap definitions.

Sources:
- `macdocs/ref/TRAP_TABLE.md` — 926-line Inside Macintosh Appendix C extract
- `assets/traps.def` — maxivmac hierarchical tracer trap catalogue
- `assets/types.def` — struct type definitions used by trap parameters


## Current State

| Metric | Count |
|--------|-------|
| TRAP_TABLE.md unique trap words | 681 |
| traps.def total entries | 693 |
| Typed (has in/out params) | 653 |
| Parameterless (void/void) | 40 |
| Missing from traps.def | 0 |
| Post-System 6 candidates | 0 |
| Name mismatches | 0 |

### Coverage by section

| Section | Entries |
|---------|---------|
| Memory Manager | 9 |
| Resource Manager | 26 |
| File Manager | 14 |
| Event Manager | 2 |
| Misc (noreturn etc.) | 5 |
| OS Traps (additional) | 91 |
| Toolbox Traps (additional) | 546 |

### Struct types in types.def

Point, Rect, FInfo, FXInfo, DInfo, DXInfo, ParamBlockHeader, IOParam,
FileParam, VolumeParam, HFileInfo, DirInfo, CInfoPBRec, WDParam, FCBPBRec,
BitMap, GrafPort, WindowRecord, LaunchParam, QHdr, SysParmType, ScrapStuff,
Pattern, Region, Zone, VCB, EventRecord, ControlRecord, DialogRecord,
MenuInfo, TERecord, PenState, FontInfo, DateTimeRec, SysEnvRec.

---

## Dispatch Parent Traps (12)

These appear as section headers (not table rows) in TRAP_TABLE.md.
They are the 12 traps in traps.def that don't match an alphabetical
table entry.  All are valid System 6 dispatch multiplexers.

| Trap | Name | Convention | Manager |
|------|------|-----------|---------|
| `A260` | HFSDispatch | os | HFS extensions |
| `A06E` | SlotManager | os | Slot Manager |
| `A815` | SCSIDispatch | toolbox | SCSI Manager |
| `A82E` | Pack12 | toolbox | Color Picker |
| `A83D` | TEDispatch | toolbox | TextEdit extensions |
| `A895` | Shutdown | toolbox | Shutdown Manager |
| `A8B5` | ScriptUtil | toolbox | Script Manager |
| `A9E7` | Pack0 | toolbox | List Manager |
| `A9E9` | Pack2 | toolbox | Disk Initialization |
| `A9EA` | Pack3 | toolbox | Standard File |
| `A9ED` | Pack6 | toolbox | International Utilities |
| `A9EE` | Pack7 | toolbox | Binary-Decimal Conversion |

---

## Parameterless Traps (40)

These have a `void` marker line in traps.def — they take no arguments
and return nothing.  They are complete, not stubs.

### OS (7)

`A019` InitZone, `A02C` InitApplZone, `A036` MoreMasters,
`A03F` InitUtil, `A063` MaxApplZone, `A07B` ADBReInit,
`A07F` InternalWait

### Toolbox — Init / Cursor / Pen (13)

`A850` InitCursor, `A852` HideCursor, `A853` ShowCursor,
`A856` ObscureCursor, `AA1D` AllocCursor, `A896` HidePen,
`A897` ShowPen, `A89E` PenNormal, `A8FE` InitFonts,
`A912` InitWindows, `A930` InitMenus, `A995` InitResources,
`A996` RsrcZoneInit

### Toolbox — Drawing / State (6)

`A8CC` ClosePgon, `A8DA` OpenRgn, `A8F4` ClosePicture,
`A934` ClearMenuBar, `A937` DrawMenuBar, `A9B4` SystemTask

### Toolbox — Packages / Math (9)

`A82E` Pack12, `A9E6` InitAllPacks, `A9E7` Pack0, `A9E9` Pack2,
`A9EA` Pack3, `A9EB` FP68K, `A9EC` Elems68K, `A9ED` Pack6,
`A9EE` Pack7

### Toolbox — System (5)

`A895` Shutdown, `A8FD` PrGlue, `A9CC` TEInit,
`A9F4` ExitToShell, `AA90` InitPalettes
