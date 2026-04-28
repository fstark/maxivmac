# MacRoman Conversion Unification — Detailed Design

Eliminate the duplicate MacRoman ↔ UTF-8 conversion code by promoting
the storage-layer primitives to a shared location and rewriting all
callers to use them.  This also fixes two bugs in the legacy clipboard
path (swapped continuation bytes in 3/4-byte UTF-8 decode, and
`UniCodeStrLength` returning UTF-8 byte count instead of MacRoman
character count).

All code must follow [STYLE.md](STYLE.md) and [NAMING.md](NAMING.md).

---

## 1. Directory Layout

```
src/
  util/
    macroman.h          ← promoted from src/storage/macroman.h (moved)
    macroman.cpp         ← NEW: houses MacRomanFromUTF8() (the string overload)
  storage/
    macroman.h           ← DELETED (moved to util/)
    text_convert.cpp     ← updated #include path
    filename_encoding.cpp← updated #include path
  platform/common/
    mac_roman.h          ← DELETED
    mac_roman.cpp        ← DELETED
    clipboard.cpp        ← rewritten to use util/macroman.h
    disk_io.h            ← updated #include path
    disk_io.cpp          ← rewritten to use util/macroman.h
  platform/
    imgui_lomem_tool.cpp ← rewritten to use util/macroman.h
    imgui_debug_windows.cpp ← rewritten to use util/macroman.h
    emulator_shell.cpp   ← updated #include path (no call sites)
```

**Rationale:** `src/util/` is a neutral location not tied to `storage/`
or `platform/`.  The primitives are pure data-conversion functions with
no dependencies on SDL, the file system, or emulator internals – they
belong in a utility layer.

---

## 2. Public Interface

### `src/util/macroman.h`

This is the current `src/storage/macroman.h` moved verbatim, with the
namespace changed from `appledouble` to a top-level header (no namespace
on the primitives — they're general utilities), plus one new string-level
function.

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <span>

/* ── Table ─────────────────────────────────────────── */

inline constexpr uint32_t kMacRomanToUnicode[128] = { /* unchanged */ };

/* ── Primitive: encode one code point to UTF-8 ─────── */

inline void AppendUTF8(std::string &out, uint32_t cp);   // unchanged

/* ── Primitive: Unicode → MacRoman byte ────────────── */

struct MacRomanResult { bool valid; uint8_t byte; };

inline MacRomanResult MacRomanFromCodePoint(uint32_t cp); // unchanged

/* ── Primitive: decode one UTF-8 code point ────────── */

inline uint32_t DecodeUTF8(std::string_view data, size_t &pos); // param: string_view

/* ── String-level conversions ─────────────────────── */

// MacRoman bytes → UTF-8 string.
std::string UTF8FromMacRoman(std::span<const uint8_t> macRoman);

// UTF-8 string → MacRoman bytes.  Unmappable code points become '?'.
std::string MacRomanFromUTF8(std::string_view utf8);
```

Key changes from the current `storage/macroman.h`:

| Item | Before | After |
|------|--------|-------|
| Namespace | `appledouble` | none (global) |
| `DecodeUTF8` param | `const std::string &` | `std::string_view` |
| `MacRomanFromUTF8()` | not present (callers did manual loops) | new convenience function |
| `UTF8FromMacRoman()` | declared in `appledouble.h`, defined in `text_convert.cpp` | moved here |

### `src/util/macroman.cpp`

```cpp
#include "util/macroman.h"

std::string UTF8FromMacRoman(std::span<const uint8_t> macRoman)
{
    std::string result;
    result.reserve(macRoman.size() * 2);
    for (uint8_t b : macRoman)
    {
        if (b < 0x80)
            result += static_cast<char>(b);
        else
            AppendUTF8(result, kMacRomanToUnicode[b - 0x80]);
    }
    return result;
}

std::string MacRomanFromUTF8(std::string_view utf8)
{
    std::string result;
    result.reserve(utf8.size());
    size_t pos = 0;
    while (pos < utf8.size())
    {
        uint32_t cp = DecodeUTF8(utf8, pos);
        auto mr = MacRomanFromCodePoint(cp);
        result += static_cast<char>(mr.valid ? mr.byte : '?');
    }
    return result;
}
```

~25 lines.  Both functions are the only non-inline symbols; everything
else stays in the header.

---

## 3. Integration Points

### 3.1 — clipboard.cpp (4 call sites)

**Current** (lines 24, 32, 115, 121, 143, 145):

Uses `MacRoman2UniCodeSize` + `MacRoman2UniCodeData` (export) and
`UniCodeStrLength` + `UniCodeStr2MacRoman` (import).  The import path
has the two bugs.

**After:**

```cpp
#include "util/macroman.h"   // replaces #include "platform/common/mac_roman.h"

// hostClipGetTextMacRoman():
//   BEFORE: UniCodeStrLength + string(len,'\0') + UniCodeStr2MacRoman
//   AFTER:
std::string result = MacRomanFromUTF8(utf8);

// HostClipSetText():
//   BEFORE: MacRoman2UniCodeSize + MacRoman2UniCodeData
//   AFTER:
std::string utf8 = UTF8FromMacRoman({macRoman, len});

// HTCEexport():
//   BEFORE: MacRoman2UniCodeSize + malloc + MacRoman2UniCodeData
//   AFTER:
std::string utf8 = UTF8FromMacRoman({s, L});

// HTCEimport():
//   BEFORE: UniCodeStrLength + PbufNew + UniCodeStr2MacRoman
//   AFTER:
std::string mr = MacRomanFromUTF8(utf8);
```

Each call site shrinks from 2-3 function calls to 1.  Bug fixes come
for free — the new code uses `DecodeUTF8` (correct byte order) and
returns a properly-sized string.

### 3.2 — disk_io.cpp (line 147, 160) + disk_io.h (line 7)

**Current:** `UniCodeStrLength` + `PbufNew` + `UniCodeStr2MacRoman`.

**After:**

```cpp
#include "util/macroman.h"   // replaces #include "platform/common/mac_roman.h"

std::string mr = MacRomanFromUTF8(name);
// then: PbufNew(mr.size(), &t) + memcpy into g_pbufDat[t]
```

### 3.3 — imgui_lomem_tool.cpp (lines 208, 211)

**Current:** `MacRoman2UniCodeSize` + `MacRoman2UniCodeData` into a
`char buf[]`.

**After:**

```cpp
#include "util/macroman.h"

std::string u = UTF8FromMacRoman({raw, len});
snprintf(buf, bufSize, "\"%s\"", u.c_str());
```

### 3.4 — imgui_debug_windows.cpp (lines 404, 406)

**Current:** `MacRoman2UniCodeSize` + `MacRoman2UniCodeData` into
`std::string`.

**After:**

```cpp
#include "util/macroman.h"

std::string out = UTF8FromMacRoman({data, n});
```

### 3.5 — emulator_shell.cpp (line 15)

Only includes the header, no call sites.  Change `#include` to
`util/macroman.h` or remove if unused.

### 3.6 — storage layer (text_convert.cpp, filename_encoding.cpp,
appledouble.h)

Change `#include "storage/macroman.h"` → `#include "util/macroman.h"`.
Remove `namespace appledouble` wrapper from callers that used the
primitives directly.

`UTF8FromMacRoman` and the file-level functions
(`MacRomanFromUTF8File`, `MacRomanSizeFromUTF8File`) stay in the
`appledouble` namespace in `text_convert.cpp` / `appledouble.h`
because they are file-I/O wrappers.  They call through to the
now-global `UTF8FromMacRoman` / `MacRomanFromUTF8`.

### 3.7 — test/test_stubs.cpp (lines 79, 83)

Delete the `MacRoman2UniCodeSize` / `MacRoman2UniCodeData` stubs.
They exist only because test binaries linked against code that called
those functions.  After the migration, no test binary references them.

---

## 4. Files Deleted

| File | Lines | Reason |
|------|-------|--------|
| `src/platform/common/mac_roman.h` | 15 | All callers migrated |
| `src/platform/common/mac_roman.cpp` | ~1960 | Replaced by `util/macroman.h` + `util/macroman.cpp` |

Net line change: **−1960** (deleted) **+25** (new .cpp) = **−1935**.

---

## 5. Key Algorithms

No new algorithms.  The promoted code is identical to the existing
`src/storage/macroman.h` primitives, which already pass a 256-byte
round-trip test (`test_appledouble.cpp` "UTF8FromMacRoman round-trip
all 256 bytes").

The one new function (`MacRomanFromUTF8`) is the string-level wrapper
that `text_convert.cpp::MacRomanFromUTF8File` already implements
inline — just factored out so clipboard and disk_io can call it
without reading a file.

---

## 6. Reused Infrastructure

| What | Where | How used |
|------|-------|----------|
| `kMacRomanToUnicode[128]` | current `storage/macroman.h` | Moved, not duplicated |
| `AppendUTF8()` | current `storage/macroman.h` | Moved, not duplicated |
| `DecodeUTF8()` | current `storage/macroman.h` | Moved, signature widened to `string_view` |
| `MacRomanFromCodePoint()` | current `storage/macroman.h` | Moved, not duplicated |
| `UTF8FromMacRoman()` | current `text_convert.cpp` | Moved to `util/macroman.cpp` |
| `PbufNew` / `g_pbufDat` | `param_buffers.h` | Still used by `disk_io.cpp` and `HTCEimport` |
| SDL clipboard API | `SDL3/SDL.h` | Still used by `clipboard.cpp` |

Nothing is duplicated.

---

## 7. Build Integration

### CMakeLists.txt changes

```cmake
# ADD to source list (in a new "Utility" section before Storage):
    src/util/macroman.cpp

# REMOVE from source list:
    src/platform/common/mac_roman.cpp
```

Test binary (line ~289): `text_convert.cpp` already in the test source
list; add `src/util/macroman.cpp`.  Remove the
`MacRoman2UniCodeSize`/`MacRoman2UniCodeData` stubs from
`test_stubs.cpp`.

---

## 8. Dependency Diagram

```
                    ┌──────────────────┐
                    │  util/macroman.h │  ← pure data conversion
                    │  util/macroman.cpp│     no dependencies
                    └────────┬─────────┘
                             │
            ┌────────────────┼────────────────┐
            │                │                │
   ┌────────▼──────┐  ┌─────▼──────┐  ┌──────▼──────────┐
   │ platform/     │  │ storage/   │  │ platform/        │
   │ clipboard.cpp │  │ text_conv  │  │ imgui_lomem_tool │
   │ disk_io.cpp   │  │ filename_  │  │ imgui_debug_win  │
   └───────────────┘  │ encoding   │  └──────────────────┘
                      └────────────┘
```

No circular dependencies.  `util/macroman.h` depends only on
`<cstdint>`, `<string>`, `<string_view>`, `<span>`.

---

## 9. Testing

### Existing tests (no changes needed)

- `test_appledouble.cpp` "UTF8FromMacRoman round-trip all 256 bytes"
  — validates every MacRoman byte survives a round-trip through
  `UTF8FromMacRoman` → `MacRomanFromUTF8File`.

### New tests to add (in test_appledouble.cpp or a new test_macroman.cpp)

| Test | What it verifies |
|------|-----------------|
| `MacRomanFromUTF8` ASCII | Pure ASCII survives unchanged |
| `MacRomanFromUTF8` 2-byte | e.g. `é` (C3 A9) → 0x8E |
| `MacRomanFromUTF8` 3-byte | e.g. `™` (E2 84 A2) → 0xAA |
| `MacRomanFromUTF8` unmappable | e.g. emoji → `?` |
| `MacRomanFromUTF8` round-trip all 256 | Same as existing test but through `MacRomanFromUTF8` instead of file I/O |
| `MacRomanFromUTF8` + `UTF8FromMacRoman` identity | For every MacRoman byte: `MacRomanFromUTF8(UTF8FromMacRoman({b})) == {b}` |

These confirm the clipboard path (which previously had no test
coverage) now works correctly.
