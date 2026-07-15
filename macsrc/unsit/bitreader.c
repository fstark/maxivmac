/*
 * bitreader.c -- MSB-first bit reader
 *
 * Ported from XADMaster (The Unarchiver) by MacPaw Inc.
 * Licensed under LGPL 2.1.
 */

#include "unsit.h"

void br_init(BitReader *br, UFile *uf)
{
    br->uf = uf;
    br->buf = 0;
    br->bits = 0;
}

int br_bit(BitReader *br)
{
    if (br->bits == 0) {
        br->buf = uf_read8(br->uf);
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
