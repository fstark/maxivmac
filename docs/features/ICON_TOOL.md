# Icon Extraction Tool

## Background

Classic Macintosh applications store icons in their resource fork using a family of
resource types: `ICN#` (1-bit icon + mask), `icl4` (4-bit color), and `icl8` (8-bit
color, 32×32, 1024 bytes using the standard Mac 256-color palette). When files are
preserved on a POSIX host using [AppleDouble](APPLEDOUBLE.md) sidecars, the resource
fork — including these icon resources — is stored in the `._` companion file.

There is currently no way to extract these icons into a modern image format for use
in documentation, branding, or tooling.

## Purpose

A standalone command-line binary that reads an AppleDouble sidecar file (or a raw
resource fork), locates all `icl8` icon resources, composites them against their
`ICN#` mask to produce transparency, and writes each icon as a 32×32 RGBA PNG file.

## Requirements

### Separate binary

- The tool is built as its own executable target (`iconextract` or similar), not
  part of the emulator binary.
- It may reuse the existing `appledouble.h`/`appledouble.cpp` implementation from
  maxivmac for parsing the sidecar envelope, or it may include its own minimal
  parser — whichever is simpler.

### Input

- Accepts one or more AppleDouble sidecar file paths as arguments.
- Exits with a clear error if the file is not a valid AppleDouble file or contains
  no resource fork.

### Resource fork parsing

- Parse the resource fork's internal structure (resource map, type list, reference
  list, resource data) to enumerate resources by type and ID.
- Extract all resources of type `icl8` (8-bit indexed color, 1024 bytes each,
  32×32 pixels).
- For each `icl8` resource, locate the corresponding `ICN#` resource with the same
  ID to obtain the 1-bit mask (128 bytes: 32×32 bits icon + 32×32 bits mask; the
  mask is the second 128-byte half).

### Color conversion

- Convert `icl8` pixel data from the standard Macintosh 8-bit system palette
  (256 entries) to 32-bit RGBA.
- Apply the `ICN#` mask as the alpha channel: mask bit 1 → opaque (alpha 255),
  mask bit 0 → transparent (alpha 0).
- If no matching `ICN#` mask is found for a given `icl8`, emit the icon fully
  opaque and print a warning.

### Output

- Write one PNG file per extracted icon.
- Default naming: `icon_<id>.png` (e.g. `icon_128.png`).
- Support an `--output-dir` / `-o` flag to control where PNGs are written
  (default: current directory).
- Use `stb_image_write.h` (already vendored in `libs/stb/`) for PNG encoding —
  no additional dependencies.

### Palette

- Embed the standard Mac 256-color palette as a compile-time table.
- Source: Apple Technical Note TN1023 or equivalent reference.

### Error handling

- Invalid or truncated resource data → skip that resource, print warning, continue.
- No `icl8` resources found → exit with non-zero status and a message.
- File I/O errors → immediate exit with descriptive message.

### Non-goals (out of scope)

- Extracting `icl4`, `ICN#`-only, `ics#`, `ics8`, or `icns` resources (can be
  added later).
- Writing ICNS or other Apple-specific output formats.
- GUI or interactive mode.
- Batch scanning of directories (the caller can use `find` + `xargs`).
