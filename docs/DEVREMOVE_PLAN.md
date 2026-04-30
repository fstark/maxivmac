# DEVREMOVE_PLAN.md — Remove Developer Mode (Implementation Plan)

Reference design: [docs/DEVREMOVE.md](DEVREMOVE.md)

---

> **All phases completed 2026-04-30** (commits 46308f2..91aa2f5).

## Completed Phases

| Phase | Commit | Description |
|-------|--------|-------------|
| 1–3 | 46308f2 | Remove developer mode UI, tool framework, debug windows, overlay button |
| 4 | (included in 46308f2) | Clean overlay — remove Developer Mode button |
| 5 | 4b38df5 | Documentation sweep — remove developer mode references |
| 6 | (verification only) | Build + test + debugger smoke tests |
| 7 | 2e79774 | `info via` — VIA1/VIA2 register dump |
| 8 | 936d28d | `info scrap` — guest clipboard contents |
| 9 | 605dcc1 | `info globals --section` — section-filtered global listing |
| 10 | 3051a83 | `info console` — guest debug console output |
| 11 | 91aa2f5 | Smoke tests for new debugger commands |
| 12 | (verification only) | Clean build + all tests pass |
