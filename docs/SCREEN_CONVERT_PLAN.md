# Screen Convert Template Modernisation

Replace the C-style multi-include macro template pattern (`screen_map.h` +
`screen_map_inst.h`) with a single C++23 function template.  The 12
macro-driven re-inclusions become 12 implicit template instantiations from
one `ScreenMapConvert<SrcDepth, DstDepth, Scale>()` function template.

**Build gate:** `cmake --build bld/macos-headless`
**Test gate:** `cd test && ./verify.sh`

---

## Phase 0 — Upgrade project to C++23

### 0.1 CMake change

In `CMakeLists.txt`, change:

```cmake
# ---------------------------------------------------------------------------
# C++17
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
```

to:

```cmake
# ---------------------------------------------------------------------------
# C++23
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
```

### 0.2 Build gate

No code changes — just confirm the toolchain accepts C++23.

---

## Phase 1 — Rewrite screen_map.h as a C++23 function template

### 1.1 Rewrite `src/platform/common/screen_map.h`

Replace the entire macro-template content with a single function template:

```cpp
#ifndef SCREEN_MAP_H
#define SCREEN_MAP_H

#include <cstdint>
#include <type_traits>
#include "platform/platform.h"

template<int SrcDepth, int DstDepth, int Scale = 1>
  requires (SrcDepth >= 0 && SrcDepth <= 3 &&
            DstDepth >= 3 && DstDepth <= 5 &&
            DstDepth >= SrcDepth &&
            Scale >= 1)
static void ScreenMapConvert(
    const uint8_t* src,
    uint8_t*       dst,
    const uint8_t* map,
    int16_t top, int16_t left, int16_t bottom, int16_t right)
```

### 1.2 Compile-time constants

All the old `#define` calculations become `constexpr` locals inside the
function body:

```cpp
constexpr int MapElSz   = Scale << (DstDepth - SrcDepth);
constexpr int TranLn2Sz = (MapElSz % 4 == 0) ? 2
                        : (MapElSz % 2 == 0) ? 1
                        : 0;
constexpr int TranN      = MapElSz >> TranLn2Sz;

using TranT = std::conditional_t<(MapElSz % 4 == 0), uint32_t,
              std::conditional_t<(MapElSz % 2 == 0), uint16_t,
                                                      uint8_t>>;
```

### 1.3 Function body — no conditional compilation

The function body is a direct translation of the existing loop nest.
No `if constexpr` is used anywhere — the compiler sees `constexpr`
template parameters and eliminates dead code naturally:

- The inner copy loop `for (k = 0; k < TranN; ++k) *pDst++ = *pMap++;`
  is a plain loop.  The compiler unrolls when `TranN` is a small
  compile-time constant (1–4 in all current instantiations).

- The scale row-duplication loop `for (k = 0; k < Scale - 1; ++k)`
  is a zero-trip loop when `Scale == 1`.  The compiler eliminates it
  and the `p3` pointer entirely.

- `vMacScreenWidth` remains a runtime value, used as:
  `const uint16_t ScrnWB = vMacScreenWidth >> (3 - SrcDepth);`

### 1.4 Mapping table — old macros to new constructs

| Old macro construct | New C++23 construct |
| --- | --- |
| `#define ScrnMapr_MapElSz (Scale << (Dst - Src))` | `constexpr int MapElSz = Scale << (DstDepth - SrcDepth);` |
| `#if 0 == (MapElSz & 3)` → type selection chain | `using TranT = std::conditional_t<…>;` |
| `#define ScrnMapr_TranN (MapElSz >> TranLn2Sz)` | `constexpr int TranN = MapElSz >> TranLn2Sz;` |
| `#if ScrnMapr_TranN > 4` — manual unroll vs loop | Plain loop — compiler decides when to unroll |
| `#if ScrnMapr_Scale > 1` — row duplication block | Plain loop with `Scale - 1` trips — compiler eliminates when zero |
| `#error "bad ScrnMapr_SrcDepth"` | `requires` clause — compile-time constraint |
| `ScrnMapr_Src` / `ScrnMapr_Dst` / `ScrnMapr_Map` globals via `#define` | `src` / `dst` / `map` function arguments |

### 1.5 Build gate

The template is header-only; it won't be instantiated until Phase 2.
Verify the header parses without errors.

---

## Phase 2 — Update screen_convert.cpp to use the template

### 2.1 Remove macro instantiation blocks

Delete all 12 `#define` / `#include "platform/screen_map_inst.h"` blocks
from `src/platform/screen_convert.cpp` (approximately lines 19–85).

### 2.2 Add template include

Add to the include block:

```cpp
#include "platform/common/screen_map.h"
```

### 2.3 Update `ConvertRect()` dispatch

Replace named function calls with direct template calls.  The runtime
switch on `vMacScreenDepth` and `bpp` stays — those are runtime values.

Mapping (`bpp` 1 → DstDepth 3, `bpp` 2 → DstDepth 4, `bpp` 4 → DstDepth 5):

| Old call | New call |
| --- | --- |
| `UpdateBWDepth3Copy(t,l,b,r)` | `ScreenMapConvert<0,3>(g_screenCompareBuff, ScalingBuff, CLUT_final, t,l,b,r)` |
| `UpdateBWDepth4Copy(t,l,b,r)` | `ScreenMapConvert<0,4>(g_screenCompareBuff, ScalingBuff, CLUT_final, t,l,b,r)` |
| `UpdateBWDepth5Copy(t,l,b,r)` | `ScreenMapConvert<0,5>(g_screenCompareBuff, ScalingBuff, CLUT_final, t,l,b,r)` |
| `UpdateColorSrc1Dst3Copy(t,l,b,r)` | `ScreenMapConvert<1,3>(g_screenCompareBuff, ScalingBuff, CLUT_final, t,l,b,r)` |
| `UpdateColorSrc1Dst4Copy(t,l,b,r)` | `ScreenMapConvert<1,4>(g_screenCompareBuff, ScalingBuff, CLUT_final, t,l,b,r)` |
| `UpdateColorSrc1Dst5Copy(t,l,b,r)` | `ScreenMapConvert<1,5>(g_screenCompareBuff, ScalingBuff, CLUT_final, t,l,b,r)` |
| `UpdateColorSrc2Dst3Copy(t,l,b,r)` | `ScreenMapConvert<2,3>(g_screenCompareBuff, ScalingBuff, CLUT_final, t,l,b,r)` |
| `UpdateColorSrc2Dst4Copy(t,l,b,r)` | `ScreenMapConvert<2,4>(g_screenCompareBuff, ScalingBuff, CLUT_final, t,l,b,r)` |
| `UpdateColorSrc2Dst5Copy(t,l,b,r)` | `ScreenMapConvert<2,5>(g_screenCompareBuff, ScalingBuff, CLUT_final, t,l,b,r)` |
| `UpdateColorSrc3Dst3Copy(t,l,b,r)` | `ScreenMapConvert<3,3>(g_screenCompareBuff, ScalingBuff, CLUT_final, t,l,b,r)` |
| `UpdateColorSrc3Dst4Copy(t,l,b,r)` | `ScreenMapConvert<3,4>(g_screenCompareBuff, ScalingBuff, CLUT_final, t,l,b,r)` |
| `UpdateColorSrc3Dst5Copy(t,l,b,r)` | `ScreenMapConvert<3,5>(g_screenCompareBuff, ScalingBuff, CLUT_final, t,l,b,r)` |

### 2.4 Build gate + test gate

---

## Phase 3 — Delete screen_map_inst.h

### 3.1 Delete file

Remove `src/platform/screen_map_inst.h`.

### 3.2 Verify no remaining references

```sh
grep -rn 'screen_map_inst\|ScrnMapr_' src/
```

Must return nothing.

### 3.3 Build gate + test gate

---

## Files Modified

| File | Action |
| --- | --- |
| `CMakeLists.txt` | Edit: C++17 → C++23 |
| `src/platform/common/screen_map.h` | Rewrite: macro template → C++23 function template |
| `src/platform/screen_map_inst.h` | Delete |
| `src/platform/screen_convert.cpp` | Edit: remove 12 macro blocks, call template directly |
| `src/platform/screen_convert.h` | No change |

## Design Decisions

- **`SrcDepth`, `DstDepth`, `Scale`** are `int` template parameters —
  all compile-time optimisation preserved.
- **`src`, `dst`, `map`** become function arguments instead of
  `#define`-injected globals — cleaner interface, more testable.
- **`Scale` defaults to 1** — preserves future scaling support without
  current overhead.
- **`requires` clause** replaces `#error` checks — clearer diagnostics
  at the point of instantiation.
- **No manual unrolling.** The old `#if TranN > 4` hand-unroll is
  deleted.  Plain loops over `constexpr` trip counts; the compiler
  unrolls when profitable.
- **No `if constexpr`.** All branches controlled by `constexpr`
  template parameters are eliminated by the compiler as dead code.
  Zero-trip loops, unused variables, and unreachable assignments are
  optimised away without explicit conditional compilation.
- **Runtime switch in `ConvertRect()` stays** — `vMacScreenDepth` and
  `bpp` are runtime values; no way to avoid dispatching.
