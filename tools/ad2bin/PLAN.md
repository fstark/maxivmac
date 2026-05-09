# ad2bin — AppleDouble → MacBinary converter

Small host-side CLI tool.  Reads an AppleDouble sidecar (`._foo`) and
the companion data fork, writes a single MacBinary II file suitable for
`hcopy -m`.

## Usage

```
ad2bin <file>
```

Reads `<file>` (the data fork) and its sidecar `._<file>`.  Writes
`<file>.bin` (MacBinary II) alongside it.

```sh
# Example: convert the built INIT
ad2bin "shared/Shared Drive Project/Shared Drive Maxi vMac"
# → shared/Shared Drive Project/Shared Drive Maxi vMac.bin

# Then copy into an HFS disk image
hcopy -m "shared/Shared Drive Project/Shared Drive Maxi vMac.bin" disk.hfs:
```

## MacBinary II format (128-byte header + forks)

| Offset | Size | Field                        |
|--------|------|------------------------------|
| 0      | 1    | 0x00 (old version)           |
| 1      | 1    | Filename length (1–63)       |
| 2      | 63   | Filename (Pascal-style)      |
| 65     | 4    | File type (from FinderInfo)  |
| 69     | 4    | Creator (from FinderInfo)    |
| 73     | 1    | Finder flags (high byte)     |
| 74     | 1    | 0x00                         |
| 75     | 2    | Vertical position            |
| 77     | 2    | Horizontal position          |
| 79     | 2    | Folder ID                    |
| 81     | 1    | Protected flag               |
| 82     | 1    | 0x00                         |
| 83     | 4    | Data fork length             |
| 87     | 4    | Resource fork length         |
| 91     | 4    | Creation date                |
| 95     | 4    | Modification date            |
| 99     | 2    | Get Info comment length      |
| 101    | 1    | Finder flags (low byte, MB II)|
| 102    | 4    | Signature 'mBIN' (MB II)     |
| 106    | 1    | Script of filename           |
| 107    | 1    | Extended Finder flags        |
| 108    | 8    | Reserved (0)                 |
| 116    | 4    | Total unpacked length (0)    |
| 120    | 2    | Secondary header length (0)  |
| 122    | 1    | Version (129 for MB II)      |
| 123    | 1    | Min version to read (129)    |
| 124    | 2    | CRC-16 of bytes 0–123        |
| 126    | 2    | 0x0000 (padding)             |

Data fork follows header at offset 128, padded to 128-byte boundary.
Resource fork follows data fork, also padded to 128-byte boundary.

## Implementation

Single file: `tools/ad2bin/ad2bin.cpp` (~150 LOC).

### Dependencies from main project

Reuse `src/storage/appledouble.{h,cpp}` for parsing the sidecar:

- `appledouble::SidecarPathFor()` — compute `._<file>` path
- `appledouble::GetFinderInfo()` — type, creator, flags
- `appledouble::ReadResourceFork()` — full resource fork data
- `appledouble::ResourceForkSize()` — resource fork length

Also needs `src/storage/filename_encoding.cpp`,
`src/storage/text_convert.cpp`, `src/util/macroman.cpp` (transitive
deps of appledouble.cpp).

### Steps

1. Parse args → get `hostPath`
2. `appledouble::GetFinderInfo(hostPath)` → type, creator, flags
3. Read data fork: `std::ifstream` on `hostPath`
4. `appledouble::ReadResourceFork(hostPath, 0, rsrcSize)` → rsrc bytes
5. Derive filename from `hostPath.filename()` (truncate to 63 chars)
6. Build 128-byte MacBinary II header
7. Compute CRC-16 (CCITT) of header bytes 0–123
8. Write header + data fork (pad to 128) + rsrc fork (pad to 128)
9. Output path: `hostPath` + `.bin`

### CRC-16

MacBinary II uses CRC-16/CCITT (poly 0x1021, init 0).  ~15 lines.

### CMake

```cmake
add_executable(ad2bin
    tools/ad2bin/ad2bin.cpp
    src/storage/appledouble.cpp
    src/storage/filename_encoding.cpp
    src/storage/text_convert.cpp
    src/util/macroman.cpp
)
target_include_directories(ad2bin PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_compile_options(ad2bin PRIVATE -Wall -Wextra -Werror)
```

### Error cases

- Missing sidecar → error, exit 1
- No Finder info in sidecar → warn, use type=`????` creator=`????`
- Filename > 63 chars → truncate
- Data fork doesn't exist → treat as 0-byte data fork (valid for INITs)

## Testing

```sh
# Round-trip test: convert and verify with hcopy
ad2bin testfile
hcopy -m testfile.bin test.hfs:
hmount test.hfs
hls -la
```
