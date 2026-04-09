# Screen Convert Template Modernisation

**Completed** on 9 April 2026.

Replace the C-style multi-include macro template pattern (`screen_map.h` +
`screen_map_inst.h`) with a single C++23 function template.  The 12
macro-driven re-inclusions become 12 implicit template instantiations from
one `ScreenMapConvert<SrcDepth, DstDepth, Scale>()` function template.

## Commits

- Phase 0 — Upgrade project to C++23 (`CMakeLists.txt`)
- Phase 1 — Rewrite `screen_map.h` as C++23 function template
- Phase 2 — Replace 12 macro instantiations with `ScreenMapConvert<>` calls
- Phase 3 — Delete `screen_map_inst.h`

## Files Modified

| File | Action |
| --- | --- |
| `CMakeLists.txt` | Edit: C++17 → C++23 |
| `src/platform/common/screen_map.h` | Rewrite: macro template → C++23 function template |
| `src/platform/screen_map_inst.h` | Deleted |
| `src/platform/screen_convert.cpp` | Edit: removed 12 macro blocks, calls template directly |
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
