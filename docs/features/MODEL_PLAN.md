# Model and Macintosh — Implementation Plan

Design: [MODEL_DESIGN.md](MODEL_DESIGN.md)
Spec: [MODEL.md](MODEL.md)

## Completed — 2 May 2026

All 9 phases implemented. Commit range: `5ac811b..cb17271`.

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Constexpr ModelDef table and lookup functions | 5ac811b |
| 2 | Refactor MachineConfigForModel() to use ModelDef table | 7cf1b30 |
| 3 | Simplify ParseModelName() and ModelToString() | 1fe93c1 |
| 4 | .mac file parser and validator | 8923d14 |
| 5 | LaunchConfigFromMacEntry adapter | ebb3bce |
| 6 | Data directory resolution + asset migration (ROMs, .def files) | 87e6c63 |
| 7 | Launcher UI (replaces model selector) | 0e234d1 |
| 8 | Boot path integration | e2e4f93 |
| 9 | Human testing + parse-error display | cb17271 |

### Deferred items (moved to TODO.md)

- Resolution change window sizing bug (unrelated to this plan)
