# Type System — Specification

A data-driven type registry for describing classic Mac OS memory
structures.  Definitions live in a `.def` text file, loaded at
startup, and used by any subsystem that needs to interpret guest
memory: the debugger `x` command, the trap tracer, ExtFS logging,
and future overlay inspectors.

## Problem

Every subsystem that reads guest structures today hard-codes offsets,
sizes, and field names in C++.  `extfs_log.cpp` has 22 `kPB_*`
constants and two 200-line `switch` functions that hand-format each
param block variant.  The debugger `x` command can only dump raw
hex.  Adding a new structure or fixing a field name requires editing
C++ and recompiling.

The Macintosh Toolbox has hundreds of structures (param blocks,
GrafPort, WindowRecord, DialogRecord, TEHandle, etc.) and many are
unions where the same offset means different things depending on
context.  A single authoritative definition file, derived from the
Apple headers, would eliminate duplication and mistakes.

## Goals

1. Define Mac structures in a plain-text `.def` file — no recompile
   to add or fix a definition.
2. Support nested structs, unions, arrays, and all Mac scalar types.
3. Provide a C++ API: given a type name and a guest address, produce
   a list of `(name, value)` pairs or a formatted multi-line dump.
4. Integrate with the debugger (`x/t` format), trap tracer, and
   ExtFS logger — each consumer picks which fields to show.
5. Zero coupling to emulator internals beyond `get_vm_byte/word/long`.

## Non-coupling Rule

The type registry depends only on:

- `get_vm_byte(uint32_t) → uint8_t`
- `get_vm_word(uint32_t) → uint16_t`
- `get_vm_long(uint32_t) → uint32_t`

It must not include headers from `cpu/`, `core/`, `storage/`, or
`debugger/`.  Consumers call into the registry, never the reverse.

## Definition File

`assets/types.def` — loaded once at startup.

### Primitives (built-in, not defined in the file)

| Name       | Size | Display        |
|------------|------|----------------|
| `byte`     | 1    | `$XX`          |
| `sbyte`    | 1    | signed decimal |
| `Boolean`  | 1    | `true`/`false` |
| `word`     | 2    | `$XXXX`        |
| `sword`    | 2    | signed decimal |
| `long`     | 4    | `$XXXXXXXX`    |
| `slong`    | 4    | signed decimal |
| `Ptr`      | 4    | `$XXXXXXXX`    |
| `Handle`   | 4    | `$XXXXXXXX`    |
| `ProcPtr`  | 4    | `$XXXXXXXX`    |
| `OSType`   | 4    | `'ABCD'`       |
| `OSErr`    | 2    | decimal + name |
| `Fixed`    | 4    | fixed-point    |
| `Fract`    | 4    | fraction       |
| `Str255`   | 256  | Pascal string  |
| `Str63`    | 64   | Pascal string  |
| `Str31`    | 32   | Pascal string  |

Pointer-sized fields (`Ptr`, `Handle`, `ProcPtr`, `RgnHandle`, etc.)
are all 4 bytes.  The pointed-to type is for documentation and future
dereferencing — it does not affect the field's own size or display.

### Syntax

```
# Comment — ignored

struct <name> {
    <offset> <type> <fieldName>    # optional trailing comment
    ...
}

union <name> {
    <variant-tag> <type-name>      # each arm is a previously-defined struct
    ...
}
```

**Offsets** are decimal byte offsets from the start of the struct.
They must be monotonically non-decreasing.  Gaps are allowed (fillers
are implicit).  The struct's total size is inferred from the last
field.

**Type** is a primitive name, a previously-defined struct name, or
an array `<type>[N]`.

**Variant tags** for unions are arbitrary identifiers used by
consumers to select which arm to display (e.g., `file` vs `dir`).

### Example

```
# ── Finder Info ──────────────────────────

struct FInfo {
    0  OSType   fdType
    4  OSType   fdCreator
    8  word     fdFlags
    10 Point    fdLocation
    14 sword    fdFldr
}

struct FXInfo {
    0  sword    fdIconID
    2  sword    fdUnused[3]
    8  sbyte    fdScript
    9  sbyte    fdXFlags
    10 sword    fdComment
    12 long     fdPutAway
}

struct DInfo {
    0  Rect     frRect
    8  word     frFlags
    10 Point    frLocation
    14 sword    frView
}

struct DXInfo {
    0  Point    frScroll
    4  long     frOpenChain
    8  sbyte    frScript
    9  sbyte    frXFlags
    10 sword    frComment
    12 long     frPutAway
}

# ── Small composites ────────────────────

struct Point {
    0  sword    v
    2  sword    h
}

struct Rect {
    0  sword    top
    2  sword    left
    4  sword    bottom
    6  sword    right
}

# ── ParamBlockHeader ────────────────────
# Shared prefix for all File Manager param blocks.

struct ParamBlockHeader {
    0  Ptr      qLink
    4  sword    qType
    6  word     ioTrap
    8  Ptr      ioCmdAddr
    12 ProcPtr  ioCompletion
    16 OSErr    ioResult
    18 Ptr      ioNamePtr        # StringPtr
    22 sword    ioVRefNum
}

# ── File Manager param blocks ───────────

struct IOParam {
    0  ParamBlockHeader header
    24 sword    ioRefNum
    26 sbyte    ioVersNum
    27 sbyte    ioPermssn
    28 Ptr      ioMisc
    32 Ptr      ioBuffer
    36 long     ioReqCount
    40 long     ioActCount
    44 sword    ioPosMode
    46 slong    ioPosOffset
}

struct FileParam {
    0  ParamBlockHeader header
    24 sword    ioFRefNum
    26 sbyte    ioFVersNum
    27 sbyte    filler1
    28 sword    ioFDirIndex
    30 sbyte    ioFlAttrib
    31 sbyte    ioFlVersNum
    32 FInfo    ioFlFndrInfo
    48 long     ioFlNum
    52 word     ioFlStBlk
    54 slong    ioFlLgLen
    58 slong    ioFlPyLen
    62 word     ioFlRStBlk
    64 slong    ioFlRLgLen
    68 slong    ioFlRPyLen
    72 long     ioFlCrDat
    76 long     ioFlMdDat
}

struct VolumeParam {
    0  ParamBlockHeader header
    24 sword    ioFRefNum
    26 sbyte    filler1
    27 sbyte    filler2
    28 sword    ioVolIndex
    30 long     ioVCrDate
    34 long     ioVLsMod
    38 sword    ioVAtrb
    40 sword    ioVNmFls
    42 word     ioVBitMap
    44 word     ioAllocPtr
    46 word     ioVNmAlBlks
    48 long     ioVAlBlkSiz
    52 long     ioVClpSiz
    56 word     ioAlBlSt
    58 long     ioVNxtCNID
    62 word     ioVFrBlk
}

# CInfoPBRec — the HFS catalog info param block.
# It's a union: file vs directory, determined by ioFlAttrib bit 4.

struct HFileInfo {
    0  ParamBlockHeader header
    24 sword    ioFRefNum
    26 sbyte    ioFVersNum
    27 sbyte    filler1
    28 sword    ioFDirIndex
    30 sbyte    ioFlAttrib
    31 sbyte    filler2
    32 FInfo    ioFlFndrInfo
    48 long     ioDirID          # directory ID or file number
    52 word     ioFlStBlk
    54 slong    ioFlLgLen
    58 slong    ioFlPyLen
    62 word     ioFlRStBlk
    64 slong    ioFlRLgLen
    68 slong    ioFlRPyLen
    72 long     ioFlCrDat
    76 long     ioFlMdDat
    80 long     ioFlBkDat
    84 FXInfo   ioFlXFndrInfo
    100 long    ioFlParID
    104 long    ioFlClpSiz
}

struct DirInfo {
    0  ParamBlockHeader header
    24 sword    ioFRefNum
    26 sword    filler1
    28 sword    ioFDirIndex
    30 sbyte    ioFlAttrib
    31 sbyte    filler2
    32 DInfo    ioDrUsrWds
    48 long     ioDrDirID
    52 word     ioDrNmFls
    54 sword    filler3[9]
    72 long     ioDrCrDat
    76 long     ioDrMdDat
    80 long     ioDrBkDat
    84 DXInfo   ioDrFndrInfo
    100 long    ioDrParID
}

union CInfoPBRec {
    file HFileInfo
    dir  DirInfo
}

struct WDParam {
    0  ParamBlockHeader header
    24 sword    filler1
    26 sword    ioWDIndex
    28 long     ioWDProcID
    32 sword    ioWDVRefNum
    34 sword    filler2[7]
    48 long     ioWDDirID
}

struct FCBPBRec {
    0  ParamBlockHeader header
    24 sword    ioFRefNum
    26 sword    filler
    28 sword    ioFCBIndx
    30 sword    filler2
    32 long     ioFCBFlNm
    36 sword    ioFCBFlags
    38 word     ioFCBStBlk
    40 long     ioFCBEOF
    44 long     ioFCBPLen
    48 long     ioFCBCrPs
    52 sword    ioFCBVRefNum
    54 long     ioFCBClpSiz
    58 long     ioFCBParID
}

# ── QuickDraw ───────────────────────────

struct BitMap {
    0  Ptr      baseAddr
    4  sword    rowBytes
    6  Rect     bounds
}

struct GrafPort {
    0  sword    device
    2  BitMap   portBits
    16 Rect     portRect
    24 Handle   visRgn           # RgnHandle
    28 Handle   clipRgn          # RgnHandle
    32 byte     bkPat[8]         # Pattern
    40 byte     fillPat[8]       # Pattern
    48 Point    pnLoc
    52 Point    pnSize
    56 sword    pnMode
    58 byte     pnPat[8]         # Pattern
    66 sword    pnVis
    68 sword    txFont
    70 byte     txFace           # Style
    71 sbyte    filler
    72 sword    txMode
    74 sword    txSize
    76 Fixed    spExtra
    80 slong    fgColor
    84 slong    bkColor
    88 sword    colrBit
    90 sword    patStretch
    92 Handle   picSave
    96 Handle   rgnSave
    100 Handle  polySave
    104 Ptr     grafProcs        # QDProcsPtr
}

# ── Window Manager ──────────────────────

struct WindowRecord {
    0   GrafPort  port
    108 sword     windowKind
    110 Boolean   visible
    111 Boolean   hilited
    112 Boolean   goAwayFlag
    113 Boolean   spareFlag
    114 Handle    strucRgn         # RgnHandle
    118 Handle    contRgn          # RgnHandle
    122 Handle    updateRgn        # RgnHandle
    126 Handle    windowDefProc
    130 Handle    dataHandle
    134 Handle    titleHandle      # StringHandle
    138 sword     titleWidth
    140 Handle    controlList      # ControlHandle
    144 Ptr       nextWindow       # WindowRecord *
    148 Handle    windowPic        # PicHandle
    152 slong     refCon
}
```

### Union Disambiguation

A union has multiple arms.  Consumers must tell the registry which
arm to use.  Two mechanisms:

1. **Explicit** — the consumer passes the variant tag string
   (`"file"` or `"dir"`).
2. **Auto-detect rule** (optional, future) — a predicate expression
   attached to the union:

```
union CInfoPBRec {
    file HFileInfo   when ioFlAttrib & 0x10 == 0
    dir  DirInfo     when ioFlAttrib & 0x10 != 0
}
```

The first version only supports explicit selection.  Auto-detect
rules are a future extension.

## C++ API

```cpp
// types.h — public interface

struct FieldValue {
    std::string_view name;      // "ioFlFndrInfo.fdType"
    uint32_t         offset;    // byte offset from struct base
    uint16_t         size;      // field size in bytes
    std::string      display;   // "'TEXT'" or "$00012345" or "-43 fnfErr"
};

class TypeRegistry {
public:
    // Load definitions from a .def file.  May be called multiple
    // times to layer additional definitions.
    void load(std::string_view path);

    // Return true if the named type exists.
    bool has(std::string_view typeName) const;

    // Return the total size of a type in bytes.
    uint16_t sizeOf(std::string_view typeName) const;

    // Read an instance of typeName at the given guest address.
    // Returns a flat list of leaf fields with formatted values.
    // For unions, pass the variant tag (e.g. "file").
    std::vector<FieldValue> read(
        std::string_view typeName,
        uint32_t addr,
        std::string_view variant = {}) const;

    // Format a multi-line dump (like the debugger would show).
    std::string format(
        std::string_view typeName,
        uint32_t addr,
        std::string_view variant = {}) const;

    // Read a single field by dotted path (e.g. "ioFlFndrInfo.fdType").
    // Returns the formatted value, or empty if not found.
    std::string readField(
        std::string_view typeName,
        uint32_t addr,
        std::string_view fieldPath,
        std::string_view variant = {}) const;
};

// Global singleton, loaded at startup.
TypeRegistry &typeRegistry();
```

## Output Format

`format()` produces one line per leaf field, indented for nested
structs:

```
(dbg) x/t $00FC1234 HFileInfo
  header.qLink:         $00000000
  header.qType:         0
  header.ioTrap:        $A260
  header.ioCmdAddr:     $0040F200
  header.ioCompletion:  $00000000
  header.ioResult:      0 noErr
  header.ioNamePtr:     $00FC2000  "Untitled"
  header.ioVRefNum:     -32000
  ioFRefNum:            0
  ioFDirIndex:          0
  ioFlAttrib:           $00
  ioFlFndrInfo.fdType:  'TEXT'
  ioFlFndrInfo.fdCreator: 'ttxt'
  ioFlFndrInfo.fdFlags: $0000
  ioFlFndrInfo.fdLocation: (40, 100)
  ioFlFndrInfo.fdFldr:  0
  ioDirID:              2
  ioFlStBlk:            $0000
  ioFlLgLen:            1024
  ...
```

For `Ptr`/`Handle`/`StringPtr` fields, the value is shown as a hex
address.  If the type is `StringPtr` or `Ptr` to a known type,
dereferencing is a future extension.

For `OSErr`, the numeric value is followed by the symbolic name from
`assets/errors.def` when available.

For `OSType`, the value is shown as `'ABCD'` when all bytes are
printable, `$XXXXXXXX` otherwise.

For `Point`, the value is shown as `(v, h)`.

For `Rect`, the value is shown as `(top, left, bottom, right)`.

For `Boolean`, the value is `true` (non-zero) or `false` (zero).

For arrays, each element is shown on its own line with index:

```
  bkPat[0]: $AA
  bkPat[1]: $55
  ...
```

## Consumers

### Debugger

New format specifier `t` for the `x` command:

```
(dbg) x/t <addr> <TypeName> [variant]
```

### ExtFS Logger

Replace the hand-coded `formatInput()` / `formatResult()` switch
logic with calls to `typeRegistry().read()`.  The logger picks the
relevant fields from the returned `FieldValue` list and formats its
single-line output.  The `TrapCategory` enum and per-category
formatting functions are eliminated.

### Trap Tracer

`traps.def` can reference type names for structured parameters:

```
A260 HFSDispatch os
  in pb:HFileInfo.A0  selector:word.D0
```

The tracer calls `typeRegistry().read()` to decode `pb`.

### Future: Memory Inspector Overlay

A GUI panel that lets the user type an address and a type name and
see a live, continuously-updated field-by-field view.

## Loading

The type registry is loaded early in emulator startup, before any
subsystem that uses it.  Definition files are loaded in order so
that forward references within a single file work (structs must be
defined before they are used as field types).

Multiple `.def` files can be loaded to separate concerns (e.g.,
`types_files.def`, `types_quickdraw.def`, `types_windows.def`), or
everything can live in one `types.def`.  The initial implementation
uses a single file.

## Design

See [TYPES_DESIGN.md](TYPES_DESIGN.md) (to be written).
