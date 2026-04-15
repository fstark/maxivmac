# HostVolume — Shared Drive Catalog Layer

## Background

The Shared Drive mounts a host directory as a virtual HFS volume inside the
emulated Mac. The guest INIT intercepts File Manager traps and forwards them
to the host via a register-block interface. On the host side, `extn_extfs.cpp`
translates these commands into POSIX file operations and maintains an in-memory
catalog of files.

Today `extn_extfs.cpp` is a monolith: it handles register marshalling, CNID
allocation, directory scanning, filename mapping, file I/O, type/creator
lookup, and resource fork access — all in one 1,100-line file with flat
functions and file-scope statics. It also:

- Stores type/creator metadata **only in memory** (lost on restart).
- Uses a non-standard `.rsrc` flat file for resource forks.
- Performs lossy filename mapping (`:` → `-`, multibyte → `?`).
- Reports incorrect sizes for TEXT files (UTF-8 bytes instead of Mac Roman).
- Cannot support more than one shared directory.

The AppleDouble library ([APPLEDOUBLE.md](APPLEDOUBLE.md)) solves the storage
problems — persistent metadata, standard sidecars, lossless filenames, correct
TEXT sizes — but it is a stateless, path-based library. It knows nothing about
CNIDs, open file handles, or the register-block protocol. A layer is needed
between the two.

## Purpose

HostVolume is that layer. It owns the in-memory catalog for a single mounted
host directory and translates between the guest's CNID/handle-oriented world
and the AppleDouble library's path-oriented world.

By encapsulating all catalog and file state in a class, multiple volumes
become natural: each shared directory gets its own `HostVolume` instance with
its own CNID space, open-file table, and working-directory table.

## Requirements

### Catalog management

- Scan a host directory tree at mount time, building an in-memory catalog.
- Filter out `._` sidecar files (via `appledouble::IsSidecar()`).
- Assign a unique CNID to every file and directory.
- Support lookup by CNID, by name within a parent directory (case-insensitive),
  and by index within a parent directory (1-based, for `GetCatInfo` indexed
  enumeration).

### Filename mapping

- Use `appledouble::MacNameFromHost()` when scanning host files to produce the
  Mac-visible name.
- Use `appledouble::HostNameFromMac()` when creating files from guest requests
  to produce valid host filenames.
- Truncate Mac names to 31 bytes.

### Metadata

- Populate type, creator, flags, dates, and fork sizes from
  `appledouble::GetFileInfo()` at scan time. This replaces the hardcoded
  `s_typeMap[]` and the lossy in-memory-only metadata.
- `SetFileInfo` persists type/creator via `appledouble::SetFinderInfo()` and
  updates the in-memory cache.
- Report the correct data fork size for TEXT files (Mac Roman byte count,
  from `appledouble::MacRomanSizeFromUTF8File()`), cached in the catalog
  entry to avoid re-converting on every `GetCatInfo` call.

### Fork I/O

- **Data fork:** Open/close via `std::fopen`, read/write via `fseek`/`fread`/
  `fwrite` as today. Track open handles in an internal table.
- **Resource fork:** Delegate entirely to `appledouble::ReadResourceFork()`,
  `WriteResourceFork()`, `SetResourceForkSize()`. No separate handle needed —
  the library operates on a path+offset model. The handle table must
  distinguish data-fork handles from resource-fork handles.
- After a write, update the catalog entry's size and modification date so
  subsequent `GetCatInfo` reflects the change.

### File lifecycle

- **Create:** Create the host file/directory, add a catalog entry. Use
  `HostNameFromMac()` for the host filename.
- **Delete:** Call `appledouble::DeleteWithSidecar()`, remove the catalog
  entry.
- **Move/Rename:** Call `appledouble::RenameWithSidecar()`, update the
  catalog entry's parent, host path, and all descendant paths.

### Working directories

- Maintain a WD ref → dirID table, as today.
- Each HostVolume instance has its own WD space.

### Multi-volume support

- Each `HostVolume` instance manages one host directory.
- The dispatch layer (ExtFS) can maintain a collection of `HostVolume`
  instances, one per mounted shared directory.
- CNID namespaces are per-volume (each volume starts at CNID 16).
- The dispatch layer routes commands to the correct volume instance, either
  by CNID lookup or by the vRefNum / WD ref associated with the request.

## Non-goals

- **Guest INIT changes.** The register-block protocol and command codes
  remain as they are. HostVolume is purely a host-side refactoring.
- **File-change watching.** HostVolume does not detect changes made to the
  host directory by external tools while the emulator is running. A manual
  rescan or restart is needed.
- **Disk image support.** HostVolume mounts host directories only, not
  `.dsk` or `.hfs` image files.
