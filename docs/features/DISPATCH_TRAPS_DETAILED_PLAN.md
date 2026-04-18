# First-Class Dispatch Trap Subtraps — Implementation Plan

Design: [DISPATCH_TRAPS_DESIGN.md](DISPATCH_TRAPS_DESIGN.md)
Spec: [DISPATCH_TRAPS.md](DISPATCH_TRAPS.md)

## Completed

All 8 phases completed on 2026-04-18.
Commits: d8a9d5c..c1b44fd

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Data types and traps.def syntax extension | ✅ |
| 2 | Parser extension with HFSDispatch entries and unit tests | ✅ |
| 3 | Lookup API and name index with unit tests | ✅ |
| 4 | TrapTracer dispatch resolution | ✅ |
| 5 | Symbol resolution and debugger breakpoints | ✅ |
| 6 | Command wiring (break, trace) | ✅ |
| 7 | Trap counter, extfs_log retirement, remaining traps.def entries | ✅ |
| 8 | Integration tests and selftest | ✅ |

### Notes

- SlotManager (A06E) left as non-dispatch: its selector requires dereferencing a byte
  from the param block struct, not a simple register read. Could be added later with
  struct-aware selector reading.
- ScriptUtil subtrap list is partial (selectors 0x00–0x0E); more can be added as needed.
