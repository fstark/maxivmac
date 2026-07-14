/*
 * compress.c — LZW (Compress) decompressor (stub)
 *
 * Ported from XADMaster (The Unarchiver) by MacPaw Inc.
 * Licensed under LGPL 2.1.
 */

#include "unsit.h"

int decompress_compress(UFile *uf, u8 *outbuf, s32 length)
{
    (void)uf; (void)outbuf; (void)length;
    fprintf(stderr, "unsit: Compress (LZW) decompressor not yet implemented\n");
    return -1;
}
