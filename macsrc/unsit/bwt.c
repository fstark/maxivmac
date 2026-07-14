/*
 * bwt.c — BWT inverse transform + MTF decoder (stub)
 *
 * Ported from XADMaster BWT.c (The Unarchiver) by MacPaw Inc.
 * Licensed under LGPL 2.1.
 */

#include "unsit.h"

void bwt_inverse(u32 *transform, u8 *block, s32 blocklen)
{
    (void)transform; (void)block; (void)blocklen;
}

void bwt_unsort(u8 *dest, u8 *src, s32 blocklen, s32 firstindex,
                u32 *transformbuf)
{
    (void)dest; (void)src; (void)blocklen;
    (void)firstindex; (void)transformbuf;
}

void mtf_reset(MTFState *st)
{
    int i;
    for (i = 0; i < 256; i++)
        st->table[i] = i;
}

int mtf_decode(MTFState *st, int symbol)
{
    int value = st->table[symbol];
    int i;
    for (i = symbol; i > 0; i--)
        st->table[i] = st->table[i - 1];
    st->table[0] = value;
    return value;
}
