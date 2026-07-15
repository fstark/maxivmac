/*
 * bwt.c -- BWT inverse transform + MTF decoder
 *
 * Ported from XADMaster BWT.c (The Unarchiver) by MacPaw Inc.
 * Licensed under LGPL 2.1.
 */

#include "unsit.h"

void bwt_inverse(u32 *transform, u8 *block, s32 blocklen)
{
	s32 counts[256];
	s32 cumulative[256];
	s32 i, total;

	for (i = 0; i < 256; i++)
		counts[i] = 0;
	for (i = 0; i < blocklen; i++)
		counts[block[i]]++;

	total = 0;
	for (i = 0; i < 256; i++)
	{
		cumulative[i] = total;
		total += counts[i];
		counts[i] = 0;
	}

	for (i = 0; i < blocklen; i++)
	{
		transform[cumulative[block[i]] + counts[block[i]]] = (u32)i;
		counts[block[i]]++;
	}
}

void bwt_unsort(u8 *dest, u8 *src, s32 blocklen, s32 firstindex, u32 *transformbuf)
{
	s32 i;
	s32 idx;

	bwt_inverse(transformbuf, src, blocklen);

	idx = firstindex;
	for (i = 0; i < blocklen; i++)
	{
		idx = (s32)transformbuf[idx];
		dest[i] = src[idx];
	}
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
