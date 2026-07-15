/*
 * lzss13.c -- LZ+Huffman (Algorithm 13) decompressor
 *
 * Ported from XADMaster XADStuffIt13Handle.m + XADPrefixCode.m
 * (The Unarchiver) by MacPaw Inc.  Licensed under LGPL 2.1.
 */

#include "unsit.h"

/* ===================================================
   LE Bit Reader
   =================================================== */

typedef struct
{
	UFile *uf;
	u32 buf;
	int bits;
} BrLE;

static void brle_init(BrLE *br, UFile *uf)
{
	br->uf = uf;
	br->buf = 0;
	br->bits = 0;
}

static void brle_ensure(BrLE *br, int n)
{
	while (br->bits < n)
	{
		br->buf |= (u32)uf_read8(br->uf) << br->bits;
		br->bits += 8;
	}
}

static int brle_bit(BrLE *br)
{
	int b;
	brle_ensure(br, 1);
	b = (int)(br->buf & 1);
	br->buf >>= 1;
	br->bits--;
	return b;
}

static u32 brle_bits(BrLE *br, int n)
{
	u32 v;
	brle_ensure(br, n);
	v = br->buf & ((1UL << n) - 1);
	br->buf >>= n;
	br->bits -= n;
	return v;
}

/* ===================================================
   Prefix Code (LE lookup table + MSB tree fallback)
   =================================================== */

#define MAX_NODES 2048
#define TBL_BITS 12
#define TBL_SIZE (1 << TBL_BITS)

typedef struct
{
	int branches[2];
} TNode;
typedef struct
{
	s16 value;
	s16 length;
} TblEntry;

typedef struct
{
	TNode nodes[MAX_NODES];
	int nnodes;
	TblEntry tbl[TBL_SIZE];
	int tbits;
} PCode;

static void pc_init(PCode *pc)
{
	pc->nnodes = 1;
	pc->nodes[0].branches[0] = -1;
	pc->nodes[0].branches[1] = -1;
}

static int pc_alloc(PCode *pc)
{
	int n = pc->nnodes++;
	if (n >= MAX_NODES) n = 0;
	pc->nodes[n].branches[0] = -1;
	pc->nodes[n].branches[1] = -1;
	return n;
}

/* Insert code MSB-first into tree */
static void pc_insert(PCode *pc, int sym, u32 code, int len)
{
	int node = 0, i;
	for (i = len - 1; i >= 0; i--)
	{
		int bit = (code >> i) & 1;
		if (pc->nodes[node].branches[bit] < 0) pc->nodes[node].branches[bit] = pc_alloc(pc);
		node = pc->nodes[node].branches[bit];
	}
	/* Mark as leaf: both branches = -(sym+2) */
	pc->nodes[node].branches[0] = -(sym + 2);
	pc->nodes[node].branches[1] = -(sym + 2);
}

#define IS_LEAF(n) ((n).branches[0] == (n).branches[1] && (n).branches[0] <= -2)
#define LEAF_SYM(n) (-(n).branches[0] - 2)

/* Build LE table using recursive splitting (matches XADMaster MakeTableLE) */
static void pc_fill_tbl(PCode *pc, int node, TblEntry *t, int depth, int maxd)
{
	int sz = 1 << (maxd - depth);
	int stride = 1 << depth;
	int i;

	if (node < 0)
	{
		for (i = 0; i < sz; i++)
		{
			t[i * stride].value = -1;
			t[i * stride].length = -1;
		}
	}
	else if (IS_LEAF(pc->nodes[node]))
	{
		int sym = LEAF_SYM(pc->nodes[node]);
		for (i = 0; i < sz; i++)
		{
			t[i * stride].value = (s16)sym;
			t[i * stride].length = (s16)depth;
		}
	}
	else if (depth == maxd)
	{
		t[0].value = (s16)node;
		t[0].length = (s16)(maxd + 1); /* signals tree-walk needed */
	}
	else
	{
		pc_fill_tbl(pc, pc->nodes[node].branches[0], t, depth + 1, maxd);
		pc_fill_tbl(pc, pc->nodes[node].branches[1], t + stride, depth + 1, maxd);
	}
}

static void pc_make_tbl(PCode *pc)
{
	pc->tbits = TBL_BITS;
	pc_fill_tbl(pc, 0, pc->tbl, 0, pc->tbits);
}

/* Build from canonical code lengths (shortestCodeIsZeros=YES) */
static void pc_build(PCode *pc, const int *lengths, int nsyms, int maxlen)
{
	int code = 0, len, i;
	pc_init(pc);
	for (len = 1; len <= maxlen; len++)
	{
		for (i = 0; i < nsyms; i++)
		{
			if (lengths[i] != len) continue;
			pc_insert(pc, i, (u32)code, len);
			code++;
		}
		code <<= 1;
	}
	pc_make_tbl(pc);
}

static u32 bit_reverse(u32 v, int n)
{
	u32 r = 0;
	int i;
	for (i = 0; i < n; i++)
	{
		r = (r << 1) | (v & 1);
		v >>= 1;
	}
	return r;
}

/* Build from explicit LE codes (for meta-code) */
static void pc_build_explicit(PCode *pc, const u32 *codes, const int *lens, int n)
{
	int i;
	pc_init(pc);
	for (i = 0; i < n; i++)
	{
		u32 hbf = bit_reverse(codes[i], lens[i]);
		pc_insert(pc, i, hbf, lens[i]);
	}
	pc_make_tbl(pc);
}

/* Decode one symbol from LE bitstream */
static int pc_decode(PCode *pc, BrLE *br)
{
	int tb = pc->tbits;
	u32 peek;
	int idx;
	TblEntry e;

	brle_ensure(br, tb);
	peek = br->buf & ((1UL << tb) - 1);
	idx = (int)peek;
	e = pc->tbl[idx];

	if (e.length < 0) return -1;

	if (e.length <= tb)
	{
		br->buf >>= e.length;
		br->bits -= e.length;
		return (int)e.value;
	}

	/* Tree-walk fallback for long codes */
	br->buf >>= tb;
	br->bits -= tb;
	{
		int node = (int)e.value;
		while (!IS_LEAF(pc->nodes[node]))
		{
			int bit = brle_bit(br);
			node = pc->nodes[node].branches[bit];
			if (node < 0) return -1;
		}
		return LEAF_SYM(pc->nodes[node]);
	}
}

/* ===================================================
   Meta-code (for dynamic tree construction)
   =================================================== */

static const u32 MetaCodes[37] = {
	0x5d8, 0x058, 0x040, 0x0c0, 0x000, 0x078, 0x02b, 0x014, 0x00c, 0x01c, 0x01b, 0x00b, 0x010,
	0x020, 0x038, 0x018, 0x0d8, 0xbd8, 0x180, 0x680, 0x380, 0xf80, 0x780, 0x480, 0x080, 0x280,
	0x3d8, 0xfd8, 0x7d8, 0x9d8, 0x1d8, 0x004, 0x001, 0x002, 0x007, 0x003, 0x008};
static const int MetaCodeLens[37] = {11, 8,	 8,	 8,	 8,	 7,	 6,	 5,	 5,	 5,	 5,	 6,	 5,
									 6,	 7,	 7,	 9,	 12, 10, 11, 11, 12, 12, 11, 11, 11,
									 12, 12, 12, 12, 12, 5,	 2,	 2,	 3,	 4,	 5};

static void parse_dynamic(PCode *pc, BrLE *br, PCode *meta, int ncodes)
{
	int lengths[512];
	int len = 0, i;
	memset(lengths, 0, sizeof(lengths));

	/* Matches XADMaster's allocAndParseCodeOfSize:metaCode: exactly.
	   The for-loop increments i unconditionally each iteration,
	   and lengths[i]=length is written AFTER the switch for every symbol. */
	for (i = 0; i < ncodes; i++)
	{
		int val = pc_decode(meta, br);
		if (val < 0) break;
		switch (val)
		{
			case 31:
				len = -1;
				break;
			case 32:
				len++;
				break;
			case 33:
				len--;
				break;
			case 34:
				if (brle_bit(br)) lengths[i++] = len;
				break;
			case 35:
			{
				int r = (int)brle_bits(br, 3) + 2;
				while (r-- > 0)
					lengths[i++] = len;
				break;
			}
			case 36:
			{
				int r = (int)brle_bits(br, 6) + 10;
				while (r-- > 0)
					lengths[i++] = len;
				break;
			}
			default:
				len = val + 1;
				break;
		}
		lengths[i] = len;
	}

	pc_build(pc, lengths, ncodes, 32);
}

/* ===================================================
   Static code tables
   =================================================== */

#include "lzss13_tables.h"

/* ===================================================
   LZSS Decoder
   =================================================== */

#define WINSZ 65536

int decompress_lzss13(UFile *uf, u8 *outbuf, s32 length)
{
	BrLE br;
	PCode *fc, *sc, *oc;
	PCode *cur;
	u8 *window;
	s32 outpos = 0, wp = 0;
	int val, code, rc = 0;

	fc = (PCode *)calloc(1, sizeof(PCode));
	sc = (PCode *)calloc(1, sizeof(PCode));
	oc = (PCode *)calloc(1, sizeof(PCode));
	window = (u8 *)calloc(1, WINSZ);
	if (!fc || !sc || !oc || !window)
	{
		rc = -1;
		goto done;
	}

	brle_init(&br, uf);
	val = (int)brle_bits(&br, 8);
	code = val >> 4;

	if (code == 0)
	{
		PCode *meta = (PCode *)malloc(sizeof(PCode));
		if (!meta) { rc = -1; goto done; }
		memset(meta, 0, sizeof(*meta));
		pc_build_explicit(meta, MetaCodes, MetaCodeLens, 37);
		parse_dynamic(fc, &br, meta, 321);
		if (val & 0x08)
		{
			free(sc);
			sc = fc;
		}
		else
			parse_dynamic(sc, &br, meta, 321);
		parse_dynamic(oc, &br, meta, (val & 0x07) + 10);
		free(meta);
	}
	else if (code >= 1 && code <= 5)
	{
		pc_build(fc, FirstCodeLengths[code - 1], 321, 32);
		pc_build(sc, SecondCodeLengths[code - 1], 321, 32);
		pc_build(oc, OffsetCodeLengths[code - 1], OffsetCodeSize[code - 1], 32);
	}
	else
	{
		fprintf(stderr, "unsit: lzss13: bad code type %d\n", code);
		rc = -1;
		goto done;
	}

	cur = fc;

	while (outpos < length)
	{
		val = pc_decode(cur, &br);
		if (val < 0)
		{
			rc = -1;
			break;
		}

		if (val < 0x100)
		{
			cur = fc;
			window[wp] = (u8)val;
			wp = (wp + 1) & (WINSZ - 1);
			outbuf[outpos++] = (u8)val;
		}
		else
		{
			s32 mlen, off;
			int bl;
			s32 i;
			cur = sc;

			if (val < 0x13e)
				mlen = val - 0x100 + 3;
			else if (val == 0x13e)
				mlen = (s32)brle_bits(&br, 10) + 65;
			else if (val == 0x13f)
				mlen = (s32)brle_bits(&br, 15) + 65;
			else
				break; /* 0x140 = end */

			bl = pc_decode(oc, &br);
			if (bl < 0)
			{
				rc = -1;
				break;
			}
			if (bl == 0)
				off = 1;
			else if (bl == 1)
				off = 2;
			else
				off = (1L << (bl - 1)) + (s32)brle_bits(&br, bl - 1) + 1;

			for (i = 0; i < mlen && outpos < length; i++)
			{
				u8 byte = window[(wp - off + WINSZ) & (WINSZ - 1)];
				window[wp] = byte;
				wp = (wp + 1) & (WINSZ - 1);
				outbuf[outpos++] = byte;
			}
		}
		if ((outpos & 0x3FF) == 0) PROGRESS_TICK(outpos, length);
	}

done:
	free(window);
	if (sc != fc) free(sc);
	free(fc);
	free(oc);
	return rc;
}
