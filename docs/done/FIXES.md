
### Bug Fixes (7)

1. **`WantCloserCyc` 0→1** (CNFUDPIC.h) — Enabled per-instruction cycle accounting to match reference build's timing.

2. **`LeaPeaEACalcCyc` return type `bool`→`uint8_t`** (m68k_tables.cpp) — Was `bool`, clamping all non-zero cycle costs to 1. Changed to `uint8_t` to match reference's truncation behavior.

3. **`M68KITAB_setup` Cycles fixup for reclassified opcodes** (m68k_tables.cpp) — When 68020/FPU/MMU instructions are reclassified to `kIKindIllegal`/`kIKindFdflt`, the Cycles field now gets patched to match the exception cost, not the original instruction's cost.

4. **VIA `putORA`/`putORB` bit iteration order 0→7 changed to 7→0** (via.cpp, via2.cpp) — Callback ordering now matches reference build.

5. **Mac Plus `extnBlockBase` 0x00F0C000→0x00F40000** (machine_config.cpp) — Extension block address was wrong for the Plus model.

6. **`kExtnVideo` enum reordering + count adjustment** (machine.h, machine.cpp) — Moved `kExtnVideo` to end of the enum so non-video models have contiguous IDs matching reference. `kCmndFindExtnCount` now subtracts 1 when `!emVidCard`.

7. **MOVEM.W predecrement never updated A7** (m68k.cpp) — `DoCodeMOVEMRmMW` had `#if ! Use68020` (compile-time, always false in multi-model build) guarding `*dstp = p`. Changed to `if (! s_cpuConfig->use68020)` (runtime check). **This was the critical 4-byte heap offset bug.**

### Determinism Fixes (3)

8. **RTC fixed to March 14, 1990 12:00 UTC** (cocoa.mm) — Was `[NSDate timeIntervalSinceReferenceDate]` (wall-clock). Now always `0xA223E2C0`.

9. **Tick counting made deterministic** (main.cpp) — Removed the `n > 8` cap and `ExtraTimeNotOver()` wall-clock gating from `RunEmulatedTicksToTrueTime`. All pending ticks now always complete.

10. **`OnTrueTime = TrueEmulatedTime` → `++OnTrueTime`** (cocoa.mm) — Tick counter advances by exactly 1 per call instead of syncing to wall-clock-derived `TrueEmulatedTime`.

### Config Alignment (3)

11. **`WantInitSpeedValue` 0→4** (CNFUDOSG.h) — Matches reference's speed setting.

12. **`CaretBlinkTime` 0x08→0x03, `DoubleClickTime` 0x08→0x05** (CNFUDPIC.h) — PRAM timing values now match reference.

13. **`DiskCacheSz` 1→4 for compact Macs** (rtc.cpp) — Was hardcoded 1; now 4 for non-II models (matching reference).

### Diagnostics

14. **PRAM dump to stderr** (rtc.cpp) — Dumps full PRAM + RTC seconds on init for cross-build comparison.

15. **Full 16-register instruction trace** (m68k.cpp) — Trace now logs D0-D7 + A0-A7 + cycle counter.