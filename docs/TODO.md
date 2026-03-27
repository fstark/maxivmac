* At one point we may want to fix the code so misconfigured VRAM doesn't crash (throw). (Unsure what is meant there)

* Check what extnBlockBase does

* Use68020 macro weirdness

* LeaPeaEACalcCyc returns an uint8_t, but should be uint16_t (bug in reference too)

* is68020OnlyKind and 68020 table build seems weird to me (I feel it should be simpler, with a Use68020 at execution time. May be more complicated than that). The disabling from M68KITAB_setup is equally weird to me.

* Fixed date at startup

* Acceleration and 1s clock management is not user friendly anymore

* Funky kExtnVideo management

* Rename the "NewDisk" stuff in sdl.cpp. It is used for FILE export, not DISK. Also, it has nothing SDL specific, it should be in the the generic code.

* File import/export between host and emulated Mac is currently a kludge: files are tunneled through the disk drive mechanism (inserted as fake "disks", read/written as raw bytes, then ejected). This makes the code confusing and limits functionality (e.g. no resource fork support, no metadata). A proper file transfer extension — a new extension ID with explicit read/write/list commands operating on host paths — would be cleaner and allow richer integration without abusing the disk slot array.

* Get rid of UnusedParam macro

## Future Work

- Key mappings (`MKC_formac_*`) should move to their own header.
- `WantInitRunInBackground` could become `EmulatorConfig::runInBackground`.
- `EmLocalTalk` could become runtime when networking gets a device abstraction.
- `dbglog_HAVE` / `WantAbnormalReports` could become runtime if debug
  logging overhead becomes negligible.

---

## Out of Scope

- Language (`MINIVMAC_LANGUAGE`) — separate plan.
- `MKC_formac_*` key mappings — separate concern, needs its own design.
- `WantCycByPriOp` — tracked in CLEANUP.md Step 4.
- `SCC_TrackMore` — tracked in CLEANUP.md Step 4.
- Moving globals into Machine — tracked in CLEANUP.md Step 3.
