/*
 * compress.c — LZW (Compress) decompressor
 *
 * Ported from XADMaster XADCompressHandle.m + LZW.c
 * (The Unarchiver) by MacPaw Inc.  Licensed under LGPL 2.1.
 *
 * StuffIt classic method 2: Unix compress compatible.
 * Flag byte 0x8E = 14-bit max codes, block mode.
 */

#include "unsit.h"

/* ── LZW table ───────────────────────────────────── */

#define LZW_MAX_SYMBOLS (1 << 14) /* 14-bit = 16384 */

typedef struct
{
	u8 chr;
	s16 parent;
} LZWNode;

typedef struct
{
	LZWNode nodes[LZW_MAX_SYMBOLS];
	int numsyms;
	int maxsyms;
	int prevsym;
	int symsize;
	int blockmode;
} LZWState;

static void lzw_init(LZWState *st, int flags)
{
	int i;
	st->blockmode = (flags & 0x80) != 0;
	st->maxsyms = 1 << (flags & 0x1F);
	if (st->maxsyms > LZW_MAX_SYMBOLS) st->maxsyms = LZW_MAX_SYMBOLS;

	for (i = 0; i < 256; i++)
	{
		st->nodes[i].chr = (u8)i;
		st->nodes[i].parent = -1;
	}
	st->numsyms = 256 + (st->blockmode ? 1 : 0);
	st->prevsym = -1;
	st->symsize = 9;
}

static void lzw_clear(LZWState *st)
{
	st->numsyms = 256 + (st->blockmode ? 1 : 0);
	st->prevsym = -1;
	st->symsize = 9;
}

static u8 lzw_first_byte(LZWState *st, int sym)
{
	while (st->nodes[sym].parent >= 0)
		sym = st->nodes[sym].parent;
	return st->nodes[sym].chr;
}

/* Output the string for a symbol into buf (reversed, then fix order).
   Returns length. */
static int lzw_output(LZWState *st, int sym, u8 *buf, int bufsz)
{
	int n = 0, s = sym;
	/* Count length */
	while (s >= 0)
	{
		n++;
		s = st->nodes[s].parent;
	}
	if (n > bufsz) n = bufsz;
	/* Fill backwards */
	{
		int pos = n;
		s = sym;
		while (pos > 0 && s >= 0)
		{
			buf[--pos] = st->nodes[s].chr;
			s = st->nodes[s].parent;
		}
	}
	return n;
}

/* Process next LZW symbol. Returns 0 on success, -1 on error. */
static int lzw_next(LZWState *st, int sym)
{
	int postfix;

	if (st->prevsym < 0)
	{
		if (sym >= st->numsyms) return -1;
		st->prevsym = sym;
		return 0;
	}

	if (sym < st->numsyms)
		postfix = lzw_first_byte(st, sym);
	else if (sym == st->numsyms)
		postfix = lzw_first_byte(st, st->prevsym);
	else
		return -1;

	/* Add new entry */
	if (st->numsyms < st->maxsyms)
	{
		st->nodes[st->numsyms].parent = (s16)st->prevsym;
		st->nodes[st->numsyms].chr = (u8)postfix;
		st->numsyms++;
		/* Grow symbol size if needed */
		if (st->numsyms < st->maxsyms && (st->numsyms & (st->numsyms - 1)) == 0) st->symsize++;
	}
	/* Table full: just update prevsym, no new entry */

	st->prevsym = sym;
	return 0;
}

/* ── LE bit reader (reused from lzss13 pattern) ─── */

typedef struct
{
	UFile *uf;
	u32 buf;
	int bits;
} BrLE2;

static void br2_init(BrLE2 *br, UFile *uf)
{
	br->uf = uf;
	br->buf = 0;
	br->bits = 0;
}

static u32 br2_bits(BrLE2 *br, int n)
{
	u32 v;
	while (br->bits < n)
	{
		br->buf |= (u32)uf_read8(br->uf) << br->bits;
		br->bits += 8;
	}
	v = br->buf & ((1UL << n) - 1);
	br->buf >>= n;
	br->bits -= n;
	return v;
}

/* ── Main decompressor ───────────────────────────── */

int decompress_compress(UFile *uf, u8 *outbuf, s32 length)
{
	LZWState *st;
	BrLE2 br;
	s32 outpos = 0;
	int symcounter = 0;
	u8 strbuf[4096];

	st = (LZWState *)malloc(sizeof(LZWState));
	if (!st) return -1;

	lzw_init(st, 0x8E); /* StuffIt always uses flags 0x8E */
	br2_init(&br, uf);

	while (outpos < length)
	{
		int sym, n;

		sym = (int)br2_bits(&br, st->symsize);
		symcounter++;

		/* Block mode: symbol 256 = clear code */
		if (sym == 256 && st->blockmode)
		{
			/* Skip padding bits to byte boundary (groups of 8 symbols).
			   The skip can be large (e.g. 84 bits) so drain in chunks. */
			int symbolsize = st->symsize;
			if (symcounter % 8)
			{
				int skip = symbolsize * (8 - symcounter % 8);
				while (skip > 0)
				{
					int chunk = skip > 24 ? 24 : skip;
					br2_bits(&br, chunk);
					skip -= chunk;
				}
			}
			lzw_clear(st);
			symcounter = 0;
			continue;
		}

		if (lzw_next(st, sym) != 0)
		{
			free(st);
			return -1;
		}

		n = lzw_output(st, st->prevsym, strbuf, (int)sizeof(strbuf));
		if (outpos + n > length) n = (int)(length - outpos);
		memcpy(outbuf + outpos, strbuf, (size_t)n);
		outpos += n;
	}

	free(st);
	return 0;
}
