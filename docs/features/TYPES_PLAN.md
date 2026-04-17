# Type System — Implementation Plan

Design: [TYPES_DESIGN.md](TYPES_DESIGN.md)
Spec: [TYPES.md](TYPES.md)

All phases completed on 17 April 2026.
Commits: 9a96165..75b7307

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Header, data structures, primitive table, MemReader | ✅ |
| 2 | Parser (struct + union + arrays) with tests | ✅ |
| 3 | Read algorithm, primitive formatter, with tests | ✅ |
| 4 | format() / readField() / sizeOf() with tests | ✅ |
| 5 | assets/types.def — all structures from spec | ✅ |
| 6 | Build integration + startup loading | ✅ |
| 7 | Debugger x/t command | ✅ |
| 6 | Build integration + startup loading | |
| 7 | Debugger x/t command | |

Build gate: `cmake --build bld/macos`
Test gate:  `./bld/macos/tests`

---

## Phase 1 — Header, Data Structures, Primitive Table

Create the public header and the .cpp skeleton with all internal
types.  No logic yet — just the data model and the primitive lookup
table.  This establishes the compilation unit and the build.

### 1.1 — Create `src/lang/type_registry.h`

Create directory `src/lang/`.  Write the header exactly as specified
in Design §2, with these types:

- `MemReader` struct (three function pointers)
- `FieldValue` struct
- `TypeRegistry` class with all public methods declared (not defined)
- `init(MemReader)` method

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <utility>

struct MemReader
{
    uint8_t  (*readByte)(uint32_t addr) = nullptr;
    uint16_t (*readWord)(uint32_t addr) = nullptr;
    uint32_t (*readLong)(uint32_t addr) = nullptr;
};

struct FieldValue
{
    std::string name;
    uint32_t    offset = 0;
    uint16_t    size   = 0;
    std::string display;
};

class TypeRegistry
{
public:
    void init(MemReader reader);

    int load(const std::filesystem::path &path);
    int loadErrors(const std::filesystem::path &path);

    bool has(std::string_view typeName) const;
    uint16_t sizeOf(std::string_view typeName) const;

    std::vector<FieldValue> read(
        std::string_view typeName,
        uint32_t addr,
        std::string_view variant = {}) const;

    std::string format(
        std::string_view typeName,
        uint32_t addr,
        std::string_view variant = {}) const;

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
                                uint32_t addr) const;

    MemReader mem_{};
    std::vector<TypeEntry> types_;
    std::unordered_map<int16_t, std::string> errors_;
};

TypeRegistry &g_typeRegistry();
```

### 1.2 — Create `src/lang/type_registry.cpp` skeleton

Define:

- `PrimitiveKind` enum class (Design §3)
- `PrimitiveInfo` struct and `kPrimitives[]` table (Design §5)
- `FindPrimitive()` free function — linear scan, returns `const PrimitiveInfo *`
- `FieldDef`, `StructDef`, `UnionDef`, `TypeEntry` nested structs (Design §3)
- `g_typeRegistry()` — returns `static TypeRegistry s_instance;`
- `init()` — stores the `MemReader`
- Stub implementations of all public methods (return empty/zero/false)
- `FormatFourCC()` free function (copy logic from extfs_log.cpp)

### 1.3 — CMakeLists.txt

Add `src/lang/type_registry.cpp` to `MINIVMAC_SOURCES` (after the
Storage block).

Add `src/lang/type_registry.cpp` to the test executable's source list.

### 1.4 — Create `test/test_types.cpp`

Create file with `#include <doctest/doctest.h>` and
`#include "lang/type_registry.h"`.  One placeholder test:

```cpp
TEST_CASE("TypeRegistry smoke")
{
    TypeRegistry reg;
    reg.init({get_vm_byte, get_vm_word, get_vm_long});
    CHECK_FALSE(reg.has("Point"));
}
```

Add `test/test_types.cpp` to CMakeLists.txt test sources.

### Fence

- [ ] `src/lang/type_registry.h` exists with full public API
- [ ] `src/lang/type_registry.cpp` compiles with stubs + primitive table
- [ ] `test/test_types.cpp` exists with smoke test
- [ ] Full build clean, all tests pass
- [ ] Commit: `"types: phase 1 — header, data structures, primitive table"`

---

## Phase 2 — Parser

Implement `load()`: the line-oriented state machine that reads
`struct` and `union` blocks from a `.def` file.  Also implement
`loadErrors()`.

### 2.1 — Parser helpers

In `type_registry.cpp`, implement:

- `BlankOrComment()` — same pattern as `trap_defs.cpp` (line starts
  with `#` or is all whitespace).
- `ParseFieldLine()` — parses `<offset> <type> <name>` with optional
  `[N]` array suffix and `# comment` tail.  Returns a `FieldDef`.
- `ParseUnionArm()` — parses `<tag> <typename>`.

### 2.2 — `load()` state machine

Implement the three-state parser (Idle / Struct / Union) from
Design §4.  On `struct NAME {`, enter Struct state; accumulate
`FieldDef` entries; on `}`, push a `TypeEntry`.  Same for unions.

Validation:
- Offsets must be non-decreasing (≥ previous end).
- Duplicate type names: warn to stderr, overwrite.
- Unknown token in Idle: warn, skip.
- Missing `}` at EOF: warn, discard incomplete def.

Return value: number of types successfully defined.

### 2.3 — `loadErrors()`

Copy the pattern from `TrapDefs::loadErrors()`: read line-by-line,
split `<code> <name>`, populate `errors_` map.

### 2.4 — `has()` and `findType()`

`findType()`: linear scan of `types_` comparing `name` field.
`has()`: returns `findType(name) != nullptr || FindPrimitive(name) != nullptr`.

### 2.5 — Tests

In `test/test_types.cpp`:

```
TEST_CASE("TypeRegistry parses simple struct")
    — write Point def to temp file, load, verify has("Point"), field count == 2

TEST_CASE("TypeRegistry parses nested struct")
    — write Point + Rect (Rect contains Point-sized fields), load,
      verify has("Rect"), sizeOf later

TEST_CASE("TypeRegistry parses union")
    — write two structs + union, verify has("MyUnion")

TEST_CASE("TypeRegistry parses array field")
    — write struct with byte[8], verify FieldDef.arrayCount == 8

TEST_CASE("TypeRegistry loadErrors")
    — load assets/errors.def, verify errors_ populated (tested via
      OSErr formatting in phase 3)

TEST_CASE("TypeRegistry rejects overlapping offsets")
    — write struct with offset 0 long + offset 2 word, verify it
      is rejected (only 1 type or 0 types loaded, with stderr warning)
```

### Fence

- [ ] `load()` parses struct/union blocks from `.def` files
- [ ] `loadErrors()` loads error names
- [ ] `has()` returns true for defined types and primitives
- [ ] Parser tests pass
- [ ] Full build clean, all tests pass
- [ ] Commit: `"types: phase 2 — parser"`

---

## Phase 3 — Read Algorithm and Primitive Formatter

Implement the core: `read()` materializes a flat list of `FieldValue`
by walking a struct definition and reading guest memory.

### 3.1 — `formatPrimitive()`

Implement the switch on `PrimitiveKind` from Design §6.  Each case reads
from guest memory via `mem_.readByte/readWord/readLong` and formats:

- `Byte` → `"$%02X"`, `SByte` → `"%d"` (signed)
- `BooleanK` → `"true"` / `"false"`
- `Word` → `"$%04X"`, `SWord` → `"%d"` (signed)
- `Long` → `"$%08X"`, `SLong` → `"%d"` (signed)
- `Ptr`, `Handle`, `ProcPtr` → `"$%08X"`
- `OSType` → `FormatFourCC()`
- `OSErr` → `"%d"` + space + error name from `errors_` map (if found)
- `Fixed` → integer part + `.` + fractional part (4 decimal digits)
- `Str255`/`Str63`/`Str31` → read length byte, read chars, escape
  non-printable, wrap in `""`

### 3.2 — `readStruct()`

Implement the recursive walk from Design §6:

```
for each field in sd.fields:
    for i in 0..field.arrayCount-1:
        fieldAddr = baseAddr + field.offset + i * elementSize
        name = prefix + field.fieldName
        if arrayCount > 1: name += "[" + str(i) + "]"

        prim = FindPrimitive(field.typeName)
        if prim:
            display = formatPrimitive(field.typeName, fieldAddr)
            out.push_back({name, fieldAddr - origBase, prim->size, display})
        else:
            innerType = findType(field.typeName)
            if innerType && !innerType->isUnion:
                readStruct(innerType->structDef, fieldAddr, name + ".", out)
```

Element size for arrays: if primitive, use `prim->size`; if struct,
use `computeSize()` (sum of last field offset + last field size;
cache in `StructDef` at parse time).

### 3.3 — `read()`

Public entry point:
- Look up type.  If union, find matching variant arm, resolve to
  struct.  If no variant given for a union, use the first arm.
- Call `readStruct()`.

### 3.4 — `computeSize()` / `sizeOf()`

`computeSize()`: for a struct, last field offset + last field size
(accounting for arrays).  For a union, max size of all arms.
Cache the result in `TypeEntry` after first computation.

`sizeOf()`: calls `computeSize()` if found, else checks primitive
table, else returns 0.

### 3.5 — Tests

```
TEST_CASE("TypeRegistry reads primitives")
    — define struct with one field of each primitive type
    — populate g_ram with known big-endian values
    — call read(), verify each FieldValue.display

TEST_CASE("TypeRegistry reads nested struct")
    — define Point + struct Foo { 0 Point pt; 4 sword x }
    — populate g_ram: pt.v=10, pt.h=20, x=42
    — verify fields: "pt.v"="10", "pt.h"="20", "x"="42"

TEST_CASE("TypeRegistry reads array")
    — define struct { 0 byte data[4] }
    — populate 4 bytes
    — verify "data[0]"="$AA", "data[1]"="$BB", etc.

TEST_CASE("TypeRegistry reads union variant")
    — define two structs + union
    — verify read with variant="file" gives HFileInfo fields
    — verify read with variant="dir" gives DirInfo fields

TEST_CASE("TypeRegistry sizeOf")
    — verify sizeOf("Point") == 4, sizeOf("sword") == 2,
      sizeOf("UnknownType") == 0

TEST_CASE("TypeRegistry OSErr formatting")
    — load errors.def, define struct with OSErr field
    — put -43 into g_ram, read, verify display == "-43 fnfErr"

TEST_CASE("TypeRegistry OSType formatting")
    — put 'TEXT' into g_ram, verify display == "'TEXT'"
    — put $00000000, verify display == "$00000000"

TEST_CASE("TypeRegistry Str255 formatting")
    — put Pascal string "Hello" (len=5) into g_ram
    — verify display == "\"Hello\""
```

### Fence

- [ ] `read()` returns correct FieldValue list for all primitive types
- [ ] Nested structs produce dotted field names
- [ ] Arrays produce indexed field names
- [ ] Unions dispatch to the correct arm
- [ ] `sizeOf()` returns correct sizes
- [ ] All tests pass
- [ ] Full build clean
- [ ] Commit: `"types: phase 3 — read algorithm and primitive formatter"`

---

## Phase 4 — format() and readField()

### 4.1 — `format()`

Implement Design §7:
- Call `read()` to get the field list.
- Find max name length (capped at 30).
- Emit one line per field: `"  " + padded(name + ":") + display + "\n"`.

### 4.2 — `readField()`

Implement Design §8:
- Call `read()`, scan for matching `name == fieldPath`.
- Return `display`, or empty string if not found.

### 4.3 — Tests

```
TEST_CASE("TypeRegistry format output")
    — define Point, populate memory
    — call format("Point", addr)
    — verify output contains "  v:" and "  h:" lines,
      properly aligned

TEST_CASE("TypeRegistry readField")
    — define Point, populate
    — readField("Point", addr, "v") returns "100"
    — readField("Point", addr, "h") returns "-50"
    — readField("Point", addr, "nonexistent") returns ""

TEST_CASE("TypeRegistry format nested struct")
    — define FInfo struct, populate with 'TEXT'/'ttxt'/flags
    — verify format output includes "fdType:  'TEXT'" and
      "fdCreator:  'ttxt'"
```

### Fence

- [ ] `format()` produces aligned multi-line output
- [ ] `readField()` returns single field values by dotted path
- [ ] Tests pass
- [ ] Full build clean
- [ ] Commit: `"types: phase 4 — format and readField"`

---

## Phase 5 — assets/types.def

Create the definition file with all structures from the spec.
Transcribe field names and offsets from the THINK C 5 headers.

### 5.1 — Create `assets/types.def`

Contents (in order, so dependencies are defined first):

1. `Point`, `Rect`
2. `FInfo`, `FXInfo`, `DInfo`, `DXInfo`
3. `ParamBlockHeader`
4. `IOParam`, `FileParam`, `VolumeParam`
5. `HFileInfo`, `DirInfo`
6. `CInfoPBRec` (union: file/dir)
7. `WDParam`, `FCBPBRec`
8. `BitMap`, `GrafPort`
9. `WindowRecord`

Use the exact content from the TYPES.md spec example section.  Verify
every offset against `macdocs/THINK_C_5/Apple #includes/Files.h`,
`Quickdraw.h`, and `Windows.h`.

### 5.2 — Smoke test with real file

Add a test that loads `assets/types.def` and verifies:

```
TEST_CASE("types.def loads all expected types")
    — reg.load("assets/types.def")
    — CHECK(reg.has("Point"))
    — CHECK(reg.has("HFileInfo"))
    — CHECK(reg.has("CInfoPBRec"))
    — CHECK(reg.has("GrafPort"))
    — CHECK(reg.has("WindowRecord"))
    — CHECK(reg.sizeOf("Point") == 4)
    — CHECK(reg.sizeOf("Rect") == 8)
    — CHECK(reg.sizeOf("FInfo") == 16)
    — CHECK(reg.sizeOf("ParamBlockHeader") == 24)
    — CHECK(reg.sizeOf("GrafPort") == 108)
    — CHECK(reg.sizeOf("WindowRecord") == 156)
```

### Fence

- [ ] `assets/types.def` exists with all structures
- [ ] Loads without parse errors
- [ ] Size sanity checks pass in test
- [ ] Full build clean, all tests pass
- [ ] Commit: `"types: phase 5 — assets/types.def"`

---

## Phase 6 — Build Integration and Startup Loading

Wire the type registry into the emulator's startup path so it's
available to all consumers.

### 6.1 — `main.cpp` — load at startup

In `InitEmulation()`, after the `g_trapDefs` block
(line ~237 of `src/core/main.cpp`), add:

```cpp
    #include "lang/type_registry.h"
    // (at top of file)

    /* Load type definitions for structured memory display */
    {
        auto &tr = g_typeRegistry();
        tr.init({get_vm_byte, get_vm_word, get_vm_long});
        int n = tr.load("assets/types.def");
        int e = tr.loadErrors("assets/errors.def");
        if (n > 0)
            std::fprintf(stderr, "type_registry: loaded %d types, %d errors\n", n, e);
    }
```

### 6.2 — Verify startup

Build and run the emulator.  Confirm the stderr message appears:

```
type_registry: loaded N types, M errors
```

### Fence

- [ ] `main.cpp` loads type registry at startup
- [ ] Emulator prints load summary on stderr
- [ ] Full build clean, all tests pass
- [ ] Commit: `"types: phase 6 — startup loading"`

---

## Phase 7 — Debugger `x/t` Command

Add the `t` format letter to the debugger's `x` command.

### 7.1 — `ParseFmtSpec()` — accept `t`

In `src/debugger/cmd_memory.cpp`, add `'t'` to the format-letter
check (line ~87):

```cpp
    if (pos < len && (spec[pos] == 'x' || spec[pos] == 'd' ||
                      spec[pos] == 's' || spec[pos] == 'i' ||
                      spec[pos] == 't'))
```

And in the word-token check further down (~line 132):

```cpp
                 tok.text[0] == 'x' || tok.text[0] == 'd' ||
                 tok.text[0] == 's' || tok.text[0] == 'i' ||
                 tok.text[0] == 't'))
```

### 7.2 — `CmdExamine()` — `t` dispatch

After the string-mode block (`if (fmt.format == 's') { ... }`) and
before the hex/decimal dump, add:

```cpp
    /* Type-aware structured dump */
    if (fmt.format == 't')
    {
        /* Remaining args: TypeName [variant] */
        std::string typeName;
        std::string variant;

        /* Re-parse remaining tokens after the address */
        /* The address was already consumed; argIdx points past it.
           Scan for Word tokens. */
        for (int i = argIdx; i < static_cast<int>(args.size()); ++i)
        {
            if (args[i].kind == Token::Kind::End) break;
            if (args[i].kind == Token::Kind::Word)
            {
                if (typeName.empty())
                    typeName = args[i].text;
                else if (variant.empty())
                    variant = args[i].text;
            }
        }

        if (typeName.empty())
        {
            dbg.io().write("Usage: x/t <addr> <TypeName> [variant]\n");
            return;
        }

        auto &tr = g_typeRegistry();
        if (!tr.has(typeName))
        {
            dbg.io().write("Unknown type: %s\n", typeName.c_str());
            return;
        }

        dbg.io().write("%s", tr.format(typeName, addr, variant).c_str());
        return;
    }
```

Add `#include "lang/type_registry.h"` at the top of the file.

### 7.3 — Manual verification

Build, launch with `--debugger`, test:

```
(dbg) x/t $0000 Point
```

Should display the Point fields read from address 0.

### Fence

- [ ] `x/t $ADDR TypeName` displays formatted struct
- [ ] `x/t $ADDR CInfoPBRec file` displays HFileInfo variant
- [ ] Unknown type prints error message
- [ ] Missing type name prints usage
- [ ] Full build clean, all tests pass
- [ ] Commit: `"types: phase 7 — debugger x/t command"`
