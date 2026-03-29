# Phase 3 — Always-Off Defines: Execution Plan

**STATUS: COMPLETE** — All 14 steps executed, tested, committed, and pushed.

## Summary

| Step | Define(s) | Action | Files touched |
|------|-----------|--------|---------------|
| 1 | `DisableLazyFlagAll`, `ForceFlagsEval`, `UseLazyZ`, `UseLazyCC` | Remove | `m68k.cpp` |
| 2 | `HaveGlbReg` | Remove | `m68k.cpp` |
| 3 | `FasterAlignedL` | Remove | `m68k.cpp` |
| 4 | `EXTRA_ABNORMAL_REPORTS` | Remove | `emulation_config.h`, `CNFUDPIC.h`, `m68k.cpp`, `sony.cpp`, `via_base.cpp`, `machine.cpp` |
| 5 | `SONY_VERIFY_CHECKSUMS` | Compile in | `emulation_config.h`, `CNFUDPIC.h`, `sony.cpp` |
| 6 | `GRAB_KEYS_MAX_FULL_SCREEN` | Remove | `osglu_common.h` |
| 7 | `EnableAltKeysMode` | Remove | `platform_config.h`, `control_mode.h`, `control_mode.cpp`, `intl_chars.cpp`, `intl_chars.h` |
| 8 | `NeedIntlChars` | Remove | `platform_config.h`, `intl_chars.h`, `intl_chars.cpp` |
| 9 | `WantInitRunInBackground` | Remove | `platform_config.h`, `intl_chars.cpp` |
| 10 | `MyAppIsBundle` | Remove | `sdl_config.h` |
| 11 | `WantAutoScrollBorder` | Compile in | `osglu_common.cpp` |
| 12 | `UseLargeScreenHack` | Compile in | `rom.cpp` |
| 13 | `C_INCLUDE_UNUSED`, `cIncludeFPUUnused` | Remove | `types.h`, `fpu_math.h` |
| 14 | `NeedCell2WinAsciiMap` | Remove | `intl_chars.h`, `intl_chars.cpp` |

**Total: 15 defines removed across 14 steps, 14 commits.**
