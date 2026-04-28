# Classic Mac OS Gotchas

Hard-won lessons from debugging the SharedDrive ExtFS against real
applications (THINK C, Finder, etc.).

---

## Default Directory Is Global, Not Per-Volume

Unlike DOS (which tracks a current directory per drive letter), classic
Mac OS has a **single global default directory** for the entire system.

`SetVol` / `PBHSetVol` replace both the default volume *and* the default
directory in one shot.  There is no memory of "where you were" on another
volume.

### Inside Macintosh references

> "if no directory is specified and the volume reference number passed is
> zero, the File Manager assumes that the file or directory is located in
> the default directory."  — IM IV, File Manager (line 25786)

> "PBHSetVol sets both the default volume and the default directory."
> — IM IV (line 27251)

> "Both the default volume and the default directory are used in calls
> made with no volume name and a volume reference number of zero."
> — IM IV (line 27253)

### Practical consequence

The default directory is only consulted when `vRefNum=0`.  When a caller
passes an explicit volume refnum (e.g. `-32000`) with `dirID=0`, that
does **not** mean "default directory on that volume" — it means the root
directory (dirID 2).

Applications that need to remember a directory across volumes must store
the directory ID themselves (or keep a WD refnum open) rather than
relying on `SetVol`.

### Bug history

THINK C stores the project directory's CNID and passes it via `ioDirID`
to `HGetFileInfo` / `HOpen`.  An earlier version of the SharedDrive INIT
returned a WD refnum from `GetVolInfo`, which masked the fact that
`resolveDir(rawVolumeRef, dirID=0)` resolved to root.  After the WD
refactor (phases 1–8), `GetVolInfo` correctly returns the raw volume
refnum per Inside Macintosh, and the bug resurfaced: THINK C calls
`HOpen(vRefNum=-32000, ioDirID=0)` and gets `fnfErr` because the file
lives in a subdirectory, not root.

The real question is: *why does THINK C pass `dirID=0` to `HOpen` when
it correctly passes the subdirectory CNID to `HGetFileInfo`?*  This is
still under investigation.
