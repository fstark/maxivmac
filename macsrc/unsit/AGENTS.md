# unsit -- StuffIt Archive Extractor

Portable C StuffIt extractor targeting classic Macintosh (System 6+, 68020+).
Handles both classic StuffIt (SIT!) and StuffIt 5 formats with all common
compression methods: None, Arsenic (BWT), LZ+Huffman, Compress (LZW), Huffman.

## Design docs

- [docs/features/UNSIT.md](../../docs/features/UNSIT.md) -- feature spec, phases, format details

## Source layout

- `unsit.h` -- shared types, UFile abstraction, function declarations
- `core.c` -- portable: decompress dispatch, format/MacBinary detection
- `unsit.c` -- Linux CLI: stdio UFile, Unix output, main()
- `unsit_mac.c` -- Mac console (THINK C): stdio UFile, native FS output, main()
- `sit1.c` / `sit5.c` -- archive parsers (classic / StuffIt 5)
- `arsenic.c`, `bwt.c` -- Arsenic decompressor (BWT + arithmetic coding)
- `lzss13.c`, `lzss13_tables.h` -- LZ+Huffman (Algorithm 13)
- `compress.c` -- LZW decompressor (Unix compress compatible)
- `huffman.c` -- Huffman-only decompressor
- `bitreader.c` -- MSB-first bit reader
- `crc.c` -- CRC-16 and CRC-32

## Build

- Linux: `make` (uses Makefile, builds `unsit` CLI)
- Mac: THINK C 5 project -- add all .c files except `unsit.c`, link MacTraps + MacTraps2

## THINK C constraints

- `int` is 16-bit; use `s32`/`u32` for values > 32767
- Static data must stay under 32K total (currently ~15K)
- No `snprintf` (shimmed in unsit.h)
- Source must be ASCII-only (no Unicode)
