# Type System — Detailed Design

Implements the specification in [TYPES.md](TYPES.md).

All code must follow [STYLE.md](../../docs/STYLE.md) and
[NAMING.md](../../docs/NAMING.md).

---

## 1. Directory Layout

```
src/
  lang/
    type_registry.h       Public header — TypeRegistry class + FieldValue
    type_registry.cpp      Parser + reader + formatter (~500 lines)
assets/
    types.def              Structure definitions (derived from Apple headers)
test/
    test_types.cpp         Unit tests
```

`src/lang/` is a new directory.  The type registry is a language-level
facility (describing data layouts) that does not belong in `cpu/`,
`core/`, `debugger/`, or `storage/`.  It has zero dependencies on any
of those modules.

---

## 2. Public Interface

```cpp
/* src/lang/type_registry.h */
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

struct FieldValue
{
    std::string name;        /* "ioFlFndrInfo.fdType" */
    uint32_t    offset;      /* byte offset from struct base */
    uint16_t    size;        /* field size in bytes */
    std::string display;     /* "'TEXT'" or "$00012345" or "-43 fnfErr" */
};

class TypeRegistry
{
public:
    /* Load definitions from a .def file.
       May be called multiple times to layer definitions.
       Returns the number of types defined. */
    int load(const std::filesystem::path &path);

    /* Load error name table (assets/errors.def) for OSErr display. */
    int loadErrors(const std::filesystem::path &path);

    /* Return true if the named type exists. */
    bool has(std::string_view typeName) const;

    /* Return the total size of a type in bytes, 0 if unknown. */
    uint16_t sizeOf(std::string_view typeName) const;

    /* Read an instance of typeName starting at guest address addr.
       Returns a flat list of leaf fields with formatted values.
       For unions, pass the variant tag (e.g. "file"). */
    std::vector<FieldValue> read(
        std::string_view typeName,
        uint32_t addr,
        std::string_view variant = {}) const;

    /* Format a multi-line dump suitable for debugger output. */
    std::string format(
        std::string_view typeName,
        uint32_t addr,
        std::string_view variant = {}) const;

    /* Read a single field by dotted path (e.g. "ioFlFndrInfo.fdType").
       Returns the formatted value, or empty string if not found. */
    std::string readField(
        std::string_view typeName,
        uint32_t addr,
        std::string_view fieldPath,
        std::string_view variant = {}) const;

private:
    struct FieldDef;
    struct StructDef;
    struct UnionDef;
    struct TypeEntry;

    const TypeEntry *findType(std::string_view name) const;
    void readStruct(const StructDef &sd,
                    uint32_t baseAddr,
                    std::string_view prefix,
                    std::vector<FieldValue> &out) const;
    std::string formatPrimitive(std::string_view typeName,
                                uint32_t addr,
                                uint16_t size) const;
    uint16_t computeSize(const TypeEntry &te) const;

    std::vector<TypeEntry> types_;
    std::unordered_map<int16_t, std::string> errors_;
};

/* Global singleton, loaded at startup. */
TypeRegistry &g_typeRegistry();
```

### Name lookup

`findType()` searches `types_` by name.  Types are defined in order
in the `.def` file, so typically < 100 entries.  A linear search is
fine for the expected scale; if profiling shows otherwise, add a
`std::unordered_map<std::string_view, size_t>` index.

---

## 3. Internal Data Structures

```cpp
/* Inside type_registry.cpp */

enum class PrimKind
{
    Byte,       /* 1, $XX */
    SByte,      /* 1, signed decimal */
    BooleanK,   /* 1, true/false */
    Word,       /* 2, $XXXX */
    SWord,      /* 2, signed decimal */
    Long,       /* 4, $XXXXXXXX */
    SLong,      /* 4, signed decimal */
    Ptr,        /* 4, $XXXXXXXX */
    Handle,     /* 4, $XXXXXXXX */
    ProcPtr,    /* 4, $XXXXXXXX */
    OSType,     /* 4, 'ABCD' */
    OSErr,      /* 2, decimal + name */
    Fixed,      /* 4, fixed-point 16.16 */
    Fract,      /* 4, fraction 2.30 */
    Str255,     /* 256, Pascal string */
    Str63,      /* 64, Pascal string */
    Str31,      /* 32, Pascal string */
};

struct TypeRegistry::FieldDef
{
    uint16_t    offset;     /* byte offset from struct start */
    std::string typeName;   /* primitive name or struct name */
    std::string fieldName;  /* Apple field name */
    uint16_t    arrayCount; /* 1 for scalars, N for type[N] */
};

struct TypeRegistry::StructDef
{
    std::string name;
    std::vector<FieldDef> fields;
};

struct TypeRegistry::UnionDef
{
    std::string name;
    /* tag → struct name */
    std::vector<std::pair<std::string, std::string>> arms;
};

/* A TypeEntry is either a struct or a union. */
struct TypeRegistry::TypeEntry
{
    std::string name;
    bool isUnion = false;
    StructDef structDef;    /* valid when !isUnion */
    UnionDef  unionDef;     /* valid when isUnion  */
};
```

### Type resolution

A field's `typeName` is resolved at `read()` time, not at parse time.
This allows natural file ordering (Point before FInfo before
HFileInfo) without a separate resolution pass.  Primitives are
checked first by a constexpr table; only if not found there does
`findType()` search the user-defined types.

---

## 4. Parser

The parser is a simple line-oriented state machine.  It reuses the
same comment and blank-line conventions as `traps.def`
(`BlankOrComment()` pattern from
[trap_defs.cpp](../../src/cpu/trap_defs.cpp#L26)).

### States

```
Idle        → sees "struct NAME {" → enter Struct
Struct      → sees "OFFSET TYPE NAME" → add FieldDef
            → sees "}" → finalize StructDef, push TypeEntry, → Idle
Idle        → sees "union NAME {" → enter Union
Union       → sees "TAG TYPENAME" → add arm
            → sees "}" → finalize UnionDef, push TypeEntry, → Idle
```

### Field line grammar

```
<field> := <offset> <type> <name> [# comment]
<type>  := <ident> | <ident> "[" <digits> "]"
<offset> := <digits>
```

The offset is validated: it must be ≥ the previous field's
offset + size.  Gaps (for fillers) are allowed.  Overlaps are
rejected.

### Array syntax

`sword fdUnused[3]` → `FieldDef { .typeName = "sword",
.fieldName = "fdUnused", .arrayCount = 3 }`.

### Error reporting

Parse errors are reported to stderr with file name and line number,
matching the `trap_defs.cpp` diagnostic style:

```
types.def:42: unknown type 'FooBar' in struct HFileInfo
```

Fatal errors (missing `}`, duplicate type names) skip the current
definition and continue — the registry is usable with partial data.

---

## 5. Primitive Table

```cpp
struct PrimInfo
{
    std::string_view name;
    PrimKind kind;
    uint16_t size;
};

static constexpr PrimInfo kPrimitives[] = {
    {"byte",    PrimKind::Byte,     1},
    {"sbyte",   PrimKind::SByte,    1},
    {"Boolean", PrimKind::BooleanK, 1},
    {"word",    PrimKind::Word,     2},
    {"sword",   PrimKind::SWord,    2},
    {"long",    PrimKind::Long,     4},
    {"slong",   PrimKind::SLong,    4},
    {"Ptr",     PrimKind::Ptr,      4},
    {"Handle",  PrimKind::Handle,   4},
    {"ProcPtr", PrimKind::ProcPtr,  4},
    {"OSType",  PrimKind::OSType,   4},
    {"OSErr",   PrimKind::OSErr,    2},
    {"Fixed",   PrimKind::Fixed,    4},
    {"Fract",   PrimKind::Fract,    4},
    {"Str255",  PrimKind::Str255,   256},
    {"Str63",   PrimKind::Str63,    64},
    {"Str31",   PrimKind::Str31,    32},
};
```

Lookup: linear scan of `kPrimitives` (17 entries).  Returns `nullptr`
if not found, meaning the name is a user-defined struct/union.

---

## 6. Read Algorithm

`read()` is the central entry point.  It returns a flat
`vector<FieldValue>` of leaf fields.

```
read(typeName, addr, variant):
    te = findType(typeName)
    if te is a union:
        find arm matching variant tag
        te = findType(arm's struct name)
    call readStruct(te.structDef, addr, prefix="", out)
    return out
```

```
readStruct(sd, baseAddr, prefix, out):
    for each field in sd.fields:
        for i in 0..field.arrayCount-1:
            fieldAddr = baseAddr + field.offset + i * elementSize
            name = prefix + field.fieldName
            if arrayCount > 1: name += "[" + i + "]"

            if field.typeName is a primitive:
                val = formatPrimitive(field.typeName, fieldAddr, primSize)
                out.push_back({name, fieldAddr - baseAddr, primSize, val})
            else:
                innerType = findType(field.typeName)
                if innerType is a struct:
                    readStruct(innerType.structDef, fieldAddr, name + ".", out)
                else:
                    /* nested union without variant tag — skip */
```

### formatPrimitive

```
formatPrimitive(typeName, addr, size):
    switch primKind:
        Byte:     "$%02X" % get_vm_byte(addr)
        SByte:    "%d" % (int8_t)get_vm_byte(addr)
        BooleanK: get_vm_byte(addr) ? "true" : "false"
        Word:     "$%04X" % get_vm_word(addr)
        SWord:    "%d" % (int16_t)get_vm_word(addr)
        Long:     "$%08X" % get_vm_long(addr)
        SLong:    "%d" % (int32_t)get_vm_long(addr)
        Ptr, Handle, ProcPtr:  "$%08X" % get_vm_long(addr)
        OSType:   formatFourCC(get_vm_long(addr))
        OSErr:    v = (int16_t)get_vm_word(addr)
                  name = errors_[v] or ""
                  "%d %s" % (v, name)
        Fixed:    raw = get_vm_long(addr)
                  "%d.%04d" % (raw >> 16, frac part)
        Str255/63/31:
                  len = get_vm_byte(addr)
                  read len bytes from addr+1, escape non-printable
                  "\"%s\"" % result
```

The FourCC formatter reuses the same logic as
[extfs_log.cpp#L100–L128](../../src/core/extfs_log.cpp#L100):
printable 4 bytes → `'ABCD'`, otherwise `$XXXXXXXX`.  Extract as a
free function `FormatFourCC()` in `type_registry.cpp`.

---

## 7. Format Algorithm

`format()` calls `read()` and aligns the output into columns:

```
format(typeName, addr, variant):
    fields = read(typeName, addr, variant)
    maxNameLen = max(f.name.size() for f in fields)  // capped at 30
    for each f in fields:
        line = "  " + pad(f.name + ":", maxNameLen + 2) + f.display
        append line + "\n"
    return result
```

Output example:

```
  header.ioResult:         0 noErr
  header.ioNamePtr:        $00FC2000
  header.ioVRefNum:        -32000
  ioFlFndrInfo.fdType:     'TEXT'
  ioFlFndrInfo.fdCreator:  'ttxt'
```

---

## 8. readField Algorithm

`readField()` provides single-field access for consumers that only
need one or two values (e.g., ExtFS logger formatting a one-line
summary).

```
readField(typeName, addr, fieldPath, variant):
    fields = read(typeName, addr, variant)
    find f where f.name == fieldPath
    return f.display   (or "" if not found)
```

For hot paths, a future optimization can walk the struct definition
directly without materializing the full vector.  The initial
implementation uses the simple approach above.

---

## 9. Error Table

`loadErrors()` reuses the same file format as `TrapDefs::loadErrors()`
([trap_defs.cpp#L343](../../src/cpu/trap_defs.cpp#L343)):
`<decimal-code> <name>`, one per line, `#` comments.

This avoids duplicating the error table.  The type registry loads the
same `assets/errors.def` that `TrapDefs` uses.

---

## 10. Integration Points

### 10.1 Startup — `InitEmulation()` in main.cpp

Insert after the existing `g_trapDefs` loading block at
[main.cpp#L234](../../src/core/main.cpp#L234):

```cpp
    /* Load external trap definitions for the hierarchical tracer */
    {
        int n = g_trapDefs.load("assets/traps.def");
        int e = g_trapDefs.loadErrors("assets/errors.def");
        if (n > 0) std::fprintf(stderr, "trap_defs: loaded %d traps, %d errors\n", n, e);
    }

    /* Load type definitions for structured memory display */
    {
        int n = g_typeRegistry().load("assets/types.def");
        int e = g_typeRegistry().loadErrors("assets/errors.def");
        if (n > 0) std::fprintf(stderr, "type_registry: loaded %d types, %d errors\n", n, e);
    }
```

**Cost:** One-time file parse at startup.  Zero runtime cost when no
consumer calls `read()`.

### 10.2 Debugger `x/t` command — cmd_memory.cpp

Add `'t'` to the accepted format letters in `ParseFmtSpec()` at
[cmd_memory.cpp#L89](../../src/debugger/cmd_memory.cpp#L89):

```cpp
    /* Format letter */
    if (pos < len && (spec[pos] == 'x' || spec[pos] == 'd' ||
                      spec[pos] == 's' || spec[pos] == 'i' ||
                      spec[pos] == 't'))
    {
        f.format = spec[pos++];
    }
```

Add the format letter to the word-token check at
[cmd_memory.cpp#L132](../../src/debugger/cmd_memory.cpp#L132):

```cpp
                 tok.text[0] == 'x' || tok.text[0] == 'd' ||
                 tok.text[0] == 's' || tok.text[0] == 'i' ||
                 tok.text[0] == 't'))
```

Add a new dispatch block after the string-mode block, before the
hex/decimal dump, at
[cmd_memory.cpp#L189](../../src/debugger/cmd_memory.cpp#L189):

```cpp
    /* Type-aware structured dump */
    if (fmt.format == 't')
    {
        /* Next non-end token is the type name */
        std::string typeName;
        std::string variant;
        /* (parse remaining tokens for type name and optional variant) */

        if (!g_typeRegistry().has(typeName))
        {
            dbg.io().write("Unknown type: %s\n", typeName.c_str());
            return;
        }
        dbg.io().write("%s", g_typeRegistry().format(typeName, addr, variant).c_str());
        return;
    }
```

The `x/t` syntax becomes: `x/t $ADDR TypeName [variant]`.  The count
and size fields of `FmtSpec` are ignored for `t` format.

### 10.3 ExtFS Logger — extfs_log.cpp

The logger can call `g_typeRegistry().read()` to get field values and
build its single-line summaries.  This replaces the hand-coded
`formatInput()` and `formatResult()` functions (combined ~350 lines).

The replacement is a future step — it requires the type registry to
be loaded and working first.  The initial implementation leaves
`extfs_log.cpp` unchanged; integration is a separate plan phase.

### 10.4 Trap Tracer — trap_tracer.cpp

`traps.def` can reference struct type names as parameter types.  When
the tracer sees a structured parameter (e.g., `pb:HFileInfo.A0`), it
calls `g_typeRegistry().read()` to decode the fields instead of just
showing the raw pointer value.

This is also a future step, since it requires extending
`ParamType` in `trap_defs.h` and the `formatParam()` method in
`trap_tracer.cpp`.

---

## 11. Build Integration

### CMakeLists.txt — Main binary

Add to the `MINIVMAC_SOURCES` list after the Storage section:

```cmake
    # Language / type system
    src/lang/type_registry.cpp
```

### CMakeLists.txt — Test binary

Add to the test executable sources:

```cmake
    test/test_types.cpp
    src/lang/type_registry.cpp
```

### Include paths

The existing `target_include_directories` for both targets already
includes `"${CMAKE_SOURCE_DIR}/src"`, so `#include "lang/type_registry.h"`
works.

---

## 12. Dependency Diagram

```
assets/types.def ──→ TypeRegistry (src/lang/)
assets/errors.def ─┘      │
                           │ read/format
              ┌────────────┼────────────┐
              ▼            ▼            ▼
   cmd_memory.cpp    extfs_log.cpp  trap_tracer.cpp
   (debugger/)       (core/)        (cpu/)
```

Arrows show "depends on" / "calls into".  The type registry has no
reverse dependencies.  It only calls `get_vm_byte/word/long` which
are extern functions declared in `m68k.h`.

To avoid including `cpu/m68k.h` (which the spec disallows), the
type registry declares its own function-pointer triad:

```cpp
/* In type_registry.h */
struct MemReader
{
    uint8_t  (*readByte)(uint32_t addr);
    uint16_t (*readWord)(uint32_t addr);
    uint32_t (*readLong)(uint32_t addr);
};
```

The global singleton is initialized with the real `get_vm_*` function
pointers by `InitEmulation()`.  Tests inject their own stubs (the
existing `g_ram[]` array from `test_stubs.cpp`).

---

## 13. Testing

### Test file: `test/test_types.cpp`

Uses doctest, same framework as all other tests.

**Test categories:**

1. **Parser tests** — load a small inline `.def` string (write to a
   temp file, load it).  Verify `has()`, `sizeOf()`, field count.

2. **Primitive formatting** — populate `g_ram[]` with known values,
   call `readField()` for each primitive type, check formatted output.

3. **Nested struct** — define Point + Rect + a struct containing Rect,
   verify dotted field names and values.

4. **Array** — define `byte foo[4]`, verify indexed field names
   `foo[0]`..`foo[3]`.

5. **Union** — define CInfoPBRec with file/dir arms, verify that
   variant selection produces the correct field names.

6. **OSErr formatting** — load `assets/errors.def`, verify
   `readField()` for an OSErr field shows `"-43 fnfErr"`.

7. **Edge cases** — empty struct, unknown type in variant, field path
   not found, address 0.

**Example test:**

```cpp
TEST_CASE("TypeRegistry reads Point struct")
{
    /* Write temp def file */
    auto tmp = std::filesystem::temp_directory_path() / "test_types.def";
    {
        std::ofstream f(tmp);
        f << "struct Point {\n"
             "    0  sword  v\n"
             "    2  sword  h\n"
             "}\n";
    }

    TypeRegistry reg;
    reg.init({get_vm_byte, get_vm_word, get_vm_long});
    CHECK(reg.load(tmp) == 1);
    CHECK(reg.has("Point"));
    CHECK(reg.sizeOf("Point") == 4);

    /* Populate guest memory: v=100, h=-50 */
    put_be16(0x1000, 100);
    put_be16(0x1002, static_cast<uint16_t>(-50));

    auto fields = reg.read("Point", 0x1000);
    REQUIRE(fields.size() == 2);
    CHECK(fields[0].name == "v");
    CHECK(fields[0].display == "100");
    CHECK(fields[1].name == "h");
    CHECK(fields[1].display == "-50");

    std::filesystem::remove(tmp);
}
```

---

## 14. Memory Reader Injection

The type registry must not include `cpu/m68k.h`.  To read guest
memory it receives function pointers at initialization:

```cpp
/* In TypeRegistry */
void init(MemReader reader);
```

**Main binary** — `InitEmulation()` calls:
```cpp
g_typeRegistry().init({get_vm_byte, get_vm_word, get_vm_long});
```

**Test binary** — test setup calls:
```cpp
reg.init({get_vm_byte, get_vm_word, get_vm_long});
```

where the test's `get_vm_*` read from `g_ram[]` (already defined in
`test_stubs.cpp`).

---

## 15. assets/types.def — Initial Content

The initial `types.def` contains the structures from the TYPES.md
spec example:

- `Point`, `Rect` — basic composites
- `FInfo`, `FXInfo`, `DInfo`, `DXInfo` — Finder metadata
- `ParamBlockHeader` — shared File Manager prefix
- `IOParam`, `FileParam`, `VolumeParam` — flat trap param blocks
- `HFileInfo`, `DirInfo` — CInfoPBRec arms
- `CInfoPBRec` — union (file/dir)
- `WDParam`, `FCBPBRec` — HFS param blocks
- `BitMap`, `GrafPort` — QuickDraw
- `WindowRecord` — Window Manager

All field names, offsets, and types are transcribed from the THINK C 5
`Apple #includes/Files.h`, `Quickdraw.h`, and `Windows.h` headers
in `macdocs/THINK_C_5/`.

---

## 16. File Size Estimates

| File | Lines |
|------|-------|
| `src/lang/type_registry.h` | ~70 |
| `src/lang/type_registry.cpp` | ~450 |
| `assets/types.def` | ~250 |
| `test/test_types.cpp` | ~200 |

Total new code: ~970 lines.
