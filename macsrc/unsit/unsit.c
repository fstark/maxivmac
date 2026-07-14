/*
 * unsit.c — StuffIt archive extractor, main driver
 *
 * Ported from XADMaster (The Unarchiver) by MacPaw Inc.
 * Licensed under LGPL 2.1.
 */

#include "unsit.h"

/* ════════════════════════════════════════════════════
   UFile — I/O abstraction (Phase 1: stdio)
   ════════════════════════════════════════════════════ */

int uf_open(UFile *uf, const char *path)
{
    uf->fp = fopen(path, "rb");
    if (!uf->fp)
        return -1;
    uf->base = 0;
    fseek(uf->fp, 0, SEEK_END);
    uf->length = (s32)ftell(uf->fp);
    fseek(uf->fp, 0, SEEK_SET);
    return 0;
}

void uf_close(UFile *uf)
{
    if (uf->fp) {
        fclose(uf->fp);
        uf->fp = NULL;
    }
}

int uf_seek(UFile *uf, s32 offset)
{
    return fseek(uf->fp, uf->base + offset, SEEK_SET);
}

int uf_read(UFile *uf, void *buf, s32 count)
{
    size_t n = fread(buf, 1, (size_t)count, uf->fp);
    return (s32)n == count ? 0 : -1;
}

s32 uf_tell(UFile *uf)
{
    return (s32)ftell(uf->fp) - uf->base;
}

u8 uf_read8(UFile *uf)
{
    int c = fgetc(uf->fp);
    return (u8)(c == EOF ? 0 : c);
}

u16 uf_read16(UFile *uf)
{
    u8 b[2];
    if (fread(b, 1, 2, uf->fp) != 2)
        return 0;
    return (u16)((u16)b[0] << 8 | b[1]);
}

u32 uf_read32(UFile *uf)
{
    u8 b[4];
    if (fread(b, 1, 4, uf->fp) != 4)
        return 0;
    return (u32)b[0] << 24 | (u32)b[1] << 16 | (u32)b[2] << 8 | b[3];
}

void uf_skip(UFile *uf, s32 count)
{
    fseek(uf->fp, count, SEEK_CUR);
}

/* ════════════════════════════════════════════════════
   Decompression dispatch
   ════════════════════════════════════════════════════ */

int decompress_none(UFile *uf, u8 *outbuf, s32 length, s32 comp_length)
{
    s32 to_read = comp_length < length ? comp_length : length;
    if (uf_read(uf, outbuf, to_read) != 0)
        return -1;
    /* If decompressed > compressed (shouldn't happen for stored),
       zero-fill the rest */
    if (to_read < length)
        memset(outbuf + to_read, 0, (size_t)(length - to_read));
    return 0;
}

int decompress(UFile *uf, int method, u8 *outbuf, s32 length, s32 comp_length)
{
    switch (method) {
    case 0:  return decompress_none(uf, outbuf, length, comp_length);
    case 2:  return decompress_compress(uf, outbuf, length);
    case 3:  return decompress_huffman(uf, outbuf, length);
    case 13: return decompress_lzss13(uf, outbuf, length);
    case 15: return decompress_arsenic(uf, outbuf, length);
    default:
        fprintf(stderr, "unsit: unsupported compression method %d\n", method);
        return -1;
    }
}

/* ════════════════════════════════════════════════════
   Format detection
   ════════════════════════════════════════════════════ */

#define FMT_UNKNOWN  0
#define FMT_SIT5     1
#define FMT_CLASSIC  2

static int detect_format(UFile *uf)
{
    u8 magic[8];
    uf_seek(uf, 0);
    if (uf_read(uf, magic, 8) != 0)
        return FMT_UNKNOWN;

    /* StuffIt 5: starts with "StuffIt" */
    if (memcmp(magic, "StuffIt", 7) == 0)
        return FMT_SIT5;

    /* Classic StuffIt: bytes 0-3 = "SIT!" */
    if (magic[0] == 'S' && magic[1] == 'I' &&
        magic[2] == 'T' && magic[3] == '!') {
        /* Also verify bytes 10-13 = "rLau" */
        u8 sig2[4];
        uf_seek(uf, 10);
        if (uf_read(uf, sig2, 4) == 0 &&
            sig2[0] == 'r' && sig2[1] == 'L' &&
            sig2[2] == 'a' && sig2[3] == 'u')
            return FMT_CLASSIC;
    }

    return FMT_UNKNOWN;
}

/* ════════════════════════════════════════════════════
   Main
   ════════════════════════════════════════════════════ */

int main(int argc, char **argv)
{
    UFile uf;
    int fmt;

    if (argc < 2) {
        fprintf(stderr, "usage: unsit <archive.sit>\n");
        return 1;
    }

    if (uf_open(&uf, argv[1]) != 0) {
        fprintf(stderr, "unsit: cannot open '%s'\n", argv[1]);
        return 1;
    }

    fmt = detect_format(&uf);
    switch (fmt) {
    case FMT_SIT5:
        printf("StuffIt 5 archive\n");
        break;
    case FMT_CLASSIC:
        printf("Classic StuffIt archive\n");
        break;
    default:
        printf("Not a StuffIt archive\n");
        break;
    }

    uf_close(&uf);
    return 0;
}
