# Naming Migration Plan — Completed

All phases completed on 29 March 2026.

Commit range: `51a9e8b..68f6805` (9 commits)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Remove `mnvm_` backward-compat aliases | Completed |
| 2 | Remove `My` prefix from functions | Completed |
| 3 | Rename `t`-prefixed type aliases | Completed |
| 4 | File-scope statics → `s_camelCase` | Completed |
| 5 | Bare globals → `g_` prefix | Completed |
| 6 | PascalCase `#define` macros → `UPPER_SNAKE_CASE` + `kXxx` → `constexpr` | Completed |
| 7 | Address-map macros → inline functions | Completed |
| 8 | `Module_Function` free functions → camelCase / class methods | Completed |

## Notes

- Phase 5: `Wires` (uint8_t*) renamed to `g_wiresData` instead of `g_wires`
  to avoid collision with the existing `WireBus g_wires` instance.
- Phase 6: Unused `kXxx` constants kept as `#define` to avoid `-Wunused-const-variable`.
- Phase 8: Tasks 8.5–8.7 (Sony_Insert*, Drive_Transfer, Drive_Eject) left as
  file-scope statics — no natural class home per plan note.

