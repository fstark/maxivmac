/*
 * bitreader.c -- MSB-first bit reader with read buffer
 *
 * Ported from XADMaster (The Unarchiver) by MacPaw Inc.
 * Licensed under LGPL 2.1.
 */

#include "unsit.h"

/* Single static buffer -- only one BitReader active at a time */
static u8 br_sbuf[BR_BUFSZ];

void br_init(BitReader *br, UFile *uf)
{
    br->uf = uf;
    br->buf = 0;
    br->bits = 0;
    br->rp = br_sbuf;
    br->rend = br_sbuf;
}

/* Refill the read buffer and return next byte (slow path) */
u8 br_nextbyte(BitReader *br)
{
    s32 n = BR_BUFSZ;
    s32 avail = br->uf->length - uf_tell(br->uf);
    if (n > avail) n = avail;
    if (n <= 0) return 0;
    uf_read(br->uf, br_sbuf, n);
    br->rp = br_sbuf + 1;
    br->rend = br_sbuf + n;
    return br_sbuf[0];
}

int br_bit(BitReader *br)
{
    if (br->bits == 0) {
        br->buf = (br->rp < br->rend) ? *br->rp++ : br_nextbyte(br);
        br->bits = 8;
    }
    br->bits--;
    return (br->buf >> br->bits) & 1;
}

u32 br_bits(BitReader *br, int n)
{
    u32 v = 0;
    int i;
    for (i = 0; i < n; i++)
        v = (v << 1) | (u32)br_bit(br);
    return v;
}
