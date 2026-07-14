/*
 * unsit.h — StuffIt archive extractor
 *
 * Portable C targeting THINK C 5 compatibility.
 * Ported from XADMaster (The Unarchiver) by MacPaw Inc.
 * Licensed under LGPL 2.1.
 */

#ifndef UNSIT_H
#define UNSIT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Portable typedefs (no <stdint.h> on THINK C) ---- */

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned long  u32;
typedef signed char    s8;
typedef signed short   s16;
typedef signed long    s32;

/* ---- I/O abstraction ---- */

/*
 * Phase 1 (Linux): UFile wraps FILE*.
 * Phase 2/3 (Mac): UFile wraps a refNum from FSOpen.
 * All I/O goes through these functions so the algorithm
 * code never touches stdio or Mac Toolbox directly.
 */
typedef struct UFile {
    FILE *fp;       /* Phase 1 only — replaced with short refNum on Mac */
    s32 base;       /* 0 normally, 128 if MacBinary-wrapped */
    s32 length;     /* total file length (from base) */
} UFile;

int  uf_open(UFile *uf, const char *path);
void uf_close(UFile *uf);
int  uf_seek(UFile *uf, s32 offset);
int  uf_read(UFile *uf, void *buf, s32 count);
s32  uf_tell(UFile *uf);
u8   uf_read8(UFile *uf);
u16  uf_read16(UFile *uf);    /* big-endian */
u32  uf_read32(UFile *uf);    /* big-endian */
void uf_skip(UFile *uf, s32 count);

/* ---- Bit reader ---- */

typedef struct BitReader {
    UFile *uf;
    u32 buf;
    int bits;       /* bits remaining in buf */
} BitReader;

void br_init(BitReader *br, UFile *uf);
int  br_bit(BitReader *br);
u32  br_bits(BitReader *br, int n);    /* MSB-first, up to 26 bits */

/* ---- Finder info (first 16 bytes match Mac FInfo record) ---- */

typedef struct FinderInfo {
    u32 type;           /* file type FourCC */
    u32 creator;        /* creator FourCC */
    u16 flags;          /* Finder flags */
    u16 location_v;     /* icon position in folder window */
    u16 location_h;
    u16 folder;         /* directory ID of containing folder */
    u32 creation;       /* Mac epoch (seconds since 1904-01-01) */
    u32 modification;   /* Mac epoch */
} FinderInfo;

/* ---- Archive entry ---- */

#define MAX_FILENAME 256

typedef struct SitEntry {
    char name[MAX_FILENAME];
    int  is_dir;

    /* Data fork */
    s32  data_offset;       /* absolute offset in archive */
    s32  data_length;       /* decompressed size */
    s32  data_comp_length;  /* compressed size */
    int  data_method;       /* compression method */
    u16  data_crc;          /* CRC-16 (classic) or CRC from header */

    /* Resource fork */
    s32  rsrc_offset;
    s32  rsrc_length;
    s32  rsrc_comp_length;
    int  rsrc_method;
    u16  rsrc_crc;

    FinderInfo info;
} SitEntry;

/* ---- Container parsers ---- */

/*
 * Callback invoked for each entry found in the archive.
 * ctx is the user context pointer passed to the parse function.
 * path is the full path including any directory prefix.
 * Returns 0 to continue, non-zero to abort.
 */
typedef int (*EntryCallback)(const char *path, const SitEntry *entry,
                             UFile *archive, void *ctx);

int sit5_parse(UFile *uf, EntryCallback cb, void *ctx);
int sit1_parse(UFile *uf, EntryCallback cb, void *ctx);

/* ---- Decompression ---- */

/*
 * Each decompressor reads compressed data from the archive at the
 * current UFile position and writes decompressed bytes to outbuf.
 * Returns 0 on success, non-zero on error.
 *
 * outbuf must be pre-allocated to 'length' bytes by the caller.
 */

int decompress_none(UFile *uf, u8 *outbuf, s32 length, s32 comp_length);
int decompress_arsenic(UFile *uf, u8 *outbuf, s32 length);
int decompress_lzss13(UFile *uf, u8 *outbuf, s32 length);
int decompress_compress(UFile *uf, u8 *outbuf, s32 length);
int decompress_huffman(UFile *uf, u8 *outbuf, s32 length);

/* Dispatch: call the right decompressor based on method byte */
int decompress(UFile *uf, int method, u8 *outbuf, s32 length, s32 comp_length);

/* ---- CRC ---- */

u16 crc16(const u8 *data, s32 length);
u32 crc32(u32 crc, const u8 *data, s32 length);

/* ---- BWT ---- */

void bwt_inverse(u32 *transform, u8 *block, s32 blocklen);
void bwt_unsort(u8 *dest, u8 *src, s32 blocklen, s32 firstindex,
                u32 *transformbuf);

/* MTF */
typedef struct MTFState {
    int table[256];
} MTFState;

void mtf_reset(MTFState *st);
int  mtf_decode(MTFState *st, int symbol);

#endif /* UNSIT_H */
