# AppleDouble Storage Library

## Background

Classic Macintosh files have two forks — a **data fork** and a **resource fork** — plus
metadata (Finder info, type/creator codes, dates). POSIX file systems have none of
these concepts. [AppleDouble](https://en.wikipedia.org/wiki/AppleSingle_and_AppleDouble_formats)
is a well-known convention for preserving this metadata: the data fork lives in a
normal file, and a companion `._`-prefixed sidecar file stores the resource fork and
Finder info in a structured binary format.

## Purpose

The Shared Drive currently mounts a host directory as a read-only HFS volume inside
the emulated Mac. It derives type/creator codes from file extensions and ignores
resource forks entirely. Adding AppleDouble support lets the guest preserve full
Macintosh metadata on the host file system, which is essential for future write
support (creating and modifying files from inside the guest).

This document specifies a standalone **AppleDouble library** — independent of the
emulator core — that manages reading and writing AppleDouble sidecar files behind a
clean abstraction.

## Requirements

### Metadata storage

- Store Finder info (type, creator, flags) and resource fork data in AppleDouble
  sidecar files (`._<filename>`).
- Use the standard AppleDouble entry IDs for resource fork (ID 2) and Finder info
  (ID 9).
- **Dates come from the host file system, not the sidecar.** Creation and
  modification dates are read from the POSIX file's `stat` timestamps and
  converted to the Mac epoch (seconds since 1904-01-01) on the fly. When a Mac
  API sets a file date, the library updates the host file's POSIX timestamp
  (via `utimes` or equivalent) rather than writing a sidecar entry. This ensures
  changes made by either Unix tools or the guest are always visible to both sides,
  and avoids creating a sidecar just to store dates.

### Lazy sidecar creation

- Do **not** create a sidecar file when all metadata matches the defaults. A plain
  host file with no resource fork, default type/creator (inferred from extension),
  and only POSIX timestamps needs no sidecar.
- Default type/creator mapping is derived from file extension (e.g. `.txt` →
  `TEXT/ttxt`, `.jpg` → `JPEG/ogle`). The mapping table should be configurable.

### File name mapping

Mac filenames may contain characters that are invalid on POSIX (and vice versa).
The library escapes these using a `^XX` hex encoding, following the convention
described in the [HELIOS EtherShare documentation](https://www.helios.de/support/manuals/esG8-e/file.html):

| Hex  | Character |
|------|-----------|
| `22` | `"`       |
| `2a` | `*`       |
| `2f` | `/`       |
| `3a` | `:`       |
| `3c` | `<`       |
| `3e` | `>`       |
| `3f` | `?`       |
| `5c` | `\`       |
| `5e` | `^`       |
| `7c` | `\|`      |

The mapping must be round-trippable: encoding then decoding always returns the
original Mac filename. Mac filenames are limited to 31 bytes; the library enforces
this and truncates with `~N` disambiguation when needed.

### Text encoding

- Text files (type `TEXT`) need encoding conversion between Mac OS Roman (guest)
  and UTF-8 (host).
- **Size mismatch problem:** UTF-8 characters may be 1–4 bytes while Mac OS Roman
  is always single-byte, so the Mac-visible file size can differ from the host file
  size. The library must convert the entire file content to determine the guest-side
  byte count. This makes directory scans potentially expensive for large TEXT files.
- This cost is acceptable provided there is a caching layer elsewhere (e.g. in the
  Shared Drive catalog) that avoids re-converting on every `GetCatInfo` call. The
  library itself does not cache — it converts on demand and reports the result.

### Atomicity and cleanup

- When a file is deleted, its sidecar is deleted with it. No orphaned `._` files.
- Resource fork writes may rewrite the entire sidecar. This is acceptable — these
  operations are infrequent and not performance-critical.

### Enumeration

- Directory listing must filter out `._` sidecar files so the guest never sees them
  as separate entries.
- If a user manually creates a file whose name collides with a sidecar name (e.g.
  creating `._README`), the library reports an error rather than silently
  corrupting data.

## Non-goals (out of scope)

- **Caching.** The library is a storage abstraction — it converts and reads on
  demand. The Shared Drive catalog is expected to cache converted sizes and
  metadata to avoid repeated work during directory scans.
- **File IDs / Directory IDs.** HFS catalog node IDs are managed by the Shared
  Drive, not this library.
- **Write-path integration.** This document defines the library API. Wiring it into
  the Shared Drive's write commands ($0210–$0213) is a separate task.
