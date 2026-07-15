/*
 * arsenic.c -- Arsenic decompressor (BWT + arithmetic coding)
 *
 * Ported from XADMaster XADStuffItArsenicHandle.m (The Unarchiver)
 * by MacPaw Inc.  Licensed under LGPL 2.1.
 */

#include "unsit.h"

/* -- Arithmetic decoder types (file-scope) --------- */

typedef struct
{
	int symbol, frequency;
} ArithSym;

typedef struct
{
	int totalfreq, increment, freqlimit, numsyms;
	ArithSym syms[128];
} ArithModel;

typedef struct
{
	BitReader br;
	s32 range, code;
} ArithDecoder;

/* -- Arithmetic model ------------------------------ */

static void arith_model_reset(ArithModel *m)
{
	int i;
	m->totalfreq = m->increment * m->numsyms;
	for (i = 0; i < m->numsyms; i++)
		m->syms[i].frequency = m->increment;
}

static void arith_model_init(ArithModel *m, int first, int last, int increment, int limit)
{
	int i;
	m->increment = increment;
	m->freqlimit = limit;
	m->numsyms = last - first + 1;
	for (i = 0; i < m->numsyms; i++)
		m->syms[i].symbol = i + first;
	arith_model_reset(m);
}

static void arith_model_bump(ArithModel *m, int idx)
{
	int i;
	m->syms[idx].frequency += m->increment;
	m->totalfreq += m->increment;

	if (m->totalfreq > m->freqlimit)
	{
		m->totalfreq = 0;
		for (i = 0; i < m->numsyms; i++)
		{
			m->syms[i].frequency++;
			m->syms[i].frequency >>= 1;
			m->totalfreq += m->syms[i].frequency;
		}
	}
}

/* -- Arithmetic decoder ---------------------------- */

#define ARITH_BITS 26
#define ARITH_ONE (1L << (ARITH_BITS - 1))
#define ARITH_HALF (1L << (ARITH_BITS - 2))

static void arith_init(ArithDecoder *dec, UFile *uf)
{
	br_init(&dec->br, uf);
	dec->range = ARITH_ONE;
	dec->code = (s32)br_bits(&dec->br, ARITH_BITS);
}

static void arith_read(ArithDecoder *dec, int symlow, int symsize, int symtot)
{
	s32 renorm = dec->range / symtot;
	s32 lowincr = renorm * (s32)symlow;
	int bit;

	dec->code -= lowincr;
	if (symlow + symsize == symtot)
		dec->range -= lowincr;
	else
		dec->range = (s32)symsize * renorm;

	while (dec->range <= ARITH_HALF)
	{
		dec->range <<= 1;
		BR_BIT(&dec->br, bit);
		dec->code = (dec->code << 1) | bit;
	}
}

static int arith_symbol(ArithDecoder *dec, ArithModel *m)
{
	ArithSym *p, *end;
	int freq, cumul, f;

	freq = dec->code / (dec->range / m->totalfreq);

	/* Pointer-walk: accumulate frequencies until we exceed freq.
	   Inner loop: one struct-member load, one add, one compare, one pointer bump.
	   All loop-invariant values (end, freq) hoisted into locals. */
	p = m->syms;
	end = p + m->numsyms - 1;
	cumul = 0;

	while (p < end)
	{
		f = p->frequency;
		cumul += f;
		if (cumul > freq) { cumul -= f; goto found; }
		p++;
	}
	/* Last symbol */
	f = p->frequency;

found:
	arith_read(dec, cumul, f, m->totalfreq);
	arith_model_bump(m, (int)(p - m->syms));

	return p->symbol;
}

static u32 arith_bitstring(ArithDecoder *dec, ArithModel *m, int bits)
{
	u32 res = 0;
	int i;
	for (i = 0; i < bits; i++)
		if (arith_symbol(dec, m)) res |= 1UL << i;
	return res;
}

/* -- Randomization table --------------------------- */

static const u16 rand_table[256] = {
	0xee,  0x56, 0xf8, 0xc3, 0x9d, 0x9f,  0xae, 0x2c, 0xad,	 0xcd, 0x24, 0x9d,	0xa6, 0x101, 0x18,
	0xb9,  0xa1, 0x82, 0x75, 0xe9, 0x9f,  0x55, 0x66, 0x6a,	 0x86, 0x71, 0xdc,	0x84, 0x56,	 0x96,
	0x56,  0xa1, 0x84, 0x78, 0xb7, 0x32,  0x6a, 0x3,  0xe3,	 0x2,  0x11, 0x101, 0x8,  0x44,	 0x83,
	0x100, 0x43, 0xe3, 0x1c, 0xf0, 0x86,  0x6a, 0x6b, 0xf,	 0x3,  0x2d, 0x86,	0x17, 0x7b,	 0x10,
	0xf6,  0x80, 0x78, 0x7a, 0xa1, 0xe1,  0xef, 0x8c, 0xf6,	 0x87, 0x4b, 0xa7,	0xe2, 0x77,	 0xfa,
	0xb8,  0x81, 0xee, 0x77, 0xc0, 0x9d,  0x29, 0x20, 0x27,	 0x71, 0x12, 0xe0,	0x6b, 0xd1,	 0x7c,
	0xa,   0x89, 0x7d, 0x87, 0xc4, 0x101, 0xc1, 0x31, 0xaf,	 0x38, 0x3,	 0x68,	0x1b, 0x76,	 0x79,
	0x3f,  0xdb, 0xc7, 0x1b, 0x36, 0x7b,  0xe2, 0x63, 0x81,	 0xee, 0xc,	 0x63,	0x8b, 0x78,	 0x38,
	0x97,  0x9b, 0xd7, 0x8f, 0xdd, 0xf2,  0xa3, 0x77, 0x8c,	 0xc3, 0x39, 0x20,	0xb3, 0x12,	 0x11,
	0xe,   0x17, 0x42, 0x80, 0x2c, 0xc4,  0x92, 0x59, 0xc8,	 0xdb, 0x40, 0x76,	0x64, 0xb4,	 0x55,
	0x1a,  0x9e, 0xfe, 0x5f, 0x6,  0x3c,  0x41, 0xef, 0xd4,	 0xaa, 0x98, 0x29,	0xcd, 0x1f,	 0x2,
	0xa8,  0x87, 0xd2, 0xa0, 0x93, 0x98,  0xef, 0xc,  0x43,	 0xed, 0x9d, 0xc2,	0xeb, 0x81,	 0xe9,
	0x64,  0x23, 0x68, 0x1e, 0x25, 0x57,  0xde, 0x9a, 0xcf,	 0x7f, 0xe5, 0xba,	0x41, 0xea,	 0xea,
	0x36,  0x1a, 0x28, 0x79, 0x20, 0x5e,  0x18, 0x4e, 0x7c,	 0x8e, 0x58, 0x7a,	0xef, 0x91,	 0x2,
	0x93,  0xbb, 0x56, 0xa1, 0x49, 0x1b,  0x79, 0x92, 0xf3,	 0x58, 0x4f, 0x52,	0x9c, 0x2,	 0x77,
	0xaf,  0x2a, 0x8f, 0x49, 0xd0, 0x99,  0x4d, 0x98, 0x101, 0x60, 0x93, 0x100, 0x75, 0x31,	 0xce,
	0x49,  0x20, 0x56, 0x57, 0xe2, 0xf5,  0x26, 0x2b, 0x8a,	 0xbf, 0xde, 0xd0,	0x83, 0x34,	 0xf4,
	0x17};

/* -- Main decompressor ----------------------------- */

int decompress_arsenic(UFile *uf, u8 *outbuf, s32 length)
{
	ArithDecoder dec;
	ArithModel initial, selector, mtfm[7];
	MTFState mtf;
	u8 *block = NULL;
	u32 *transform = NULL;
	s32 outpos = 0;
	int blockbits, endofblocks;
	s32 blocksize;
	u32 compcrc, crc;
	int rc = 0;

	/* Init decoder */
	arith_init(&dec, uf);

	/* Init models */
	arith_model_init(&initial, 0, 1, 1, 256);
	arith_model_init(&selector, 0, 10, 8, 1024);
	arith_model_init(&mtfm[0], 2, 3, 8, 1024);
	arith_model_init(&mtfm[1], 4, 7, 4, 1024);
	arith_model_init(&mtfm[2], 8, 15, 4, 1024);
	arith_model_init(&mtfm[3], 16, 31, 4, 1024);
	arith_model_init(&mtfm[4], 32, 63, 2, 1024);
	arith_model_init(&mtfm[5], 64, 127, 2, 1024);
	arith_model_init(&mtfm[6], 128, 255, 1, 1024);

	/* Stream header */
	if (arith_bitstring(&dec, &initial, 8) != 'A' || arith_bitstring(&dec, &initial, 8) != 's')
	{
		fprintf(stderr, "unsit: arsenic: bad stream header\n");
		return -1;
	}

	blockbits = (int)arith_bitstring(&dec, &initial, 4) + 9;
	blocksize = 1L << blockbits;

#ifdef __THINK__
	/* On classic Mac, malloc can "succeed" returning memory that overlaps
	   the stack. Check FreeMem before allocating. Need block + transform
	   + 16K stack headroom. */
	{
		long need = blocksize + (blocksize * (s32)sizeof(u32)) + 16384L;
		if ((long)FreeMem() < need)
		{
			fprintf(stderr, "unsit: arsenic: insufficient memory (need %ld, have %ld)\n",
					need, (long)FreeMem());
			return -1;
		}
	}
#endif

	block = (u8 *)malloc((size_t)blocksize);
	transform = (u32 *)malloc((size_t)blocksize * sizeof(u32));
	if (!block || !transform)
	{
		fprintf(stderr, "unsit: arsenic: out of memory (need %ld bytes for blocksize %ld)\n",
				(long)blocksize * 5, (long)blocksize);
		rc = -1;
		goto done;
	}

	crc = 0;
	compcrc = 0;
	endofblocks = arith_symbol(&dec, &initial);

	/* -- Output state (persists across blocks) - */
	{
		s32 numbytes = 0, bytecount = 0, transformindex = 0;
		int count = 0, last = 0, repeat = 0;
		int randomized = 0, randindex = 0;
		s32 randcount = 0;

		while (outpos < length)
		{
			int outbyte;

			if (repeat > 0)
			{
				repeat--;
				outbyte = last;
			}
			else
			{
				int byte;
			retry:
				/* Need a new block? */
				if (bytecount >= numbytes)
				{
					int sel;
					s32 i;

					if (endofblocks) break;

					/* -- Decode next block ----------- */
					progress_char = '*';
					mtf_reset(&mtf);
					randomized = arith_symbol(&dec, &initial);
					transformindex = arith_bitstring(&dec, &initial, blockbits);
					numbytes = 0;

					for (;;)
					{
						sel = arith_symbol(&dec, &selector);
						if (sel == 0 || sel == 1)
						{
							s32 zerostate = 1;
							s32 zerocount = 0;
							while (sel < 2)
							{
								if (sel == 0)
									zerocount += zerostate;
								else if (sel == 1)
									zerocount += 2 * zerostate;
								zerostate *= 2;
								sel = arith_symbol(&dec, &selector);
							}
							if (numbytes + zerocount > blocksize)
							{
								rc = -1;
								goto done;
							}
							memset(&block[numbytes], (u8)mtf_decode(&mtf, 0), (size_t)zerocount);
							numbytes += zerocount;
							if ((numbytes & 0x3FF) == 0) PROGRESS_TICK(outpos + numbytes, length);
						}

						if (sel == 10) break;

						{
							int symbol;
							if (sel == 2)
								symbol = 1;
							else
								symbol = arith_symbol(&dec, &mtfm[sel - 3]);
							if (numbytes >= blocksize)
							{
								rc = -1;
								goto done;
							}
							block[numbytes++] = (u8)mtf_decode(&mtf, symbol);
							if ((numbytes & 0x3FF) == 0) PROGRESS_TICK(outpos + numbytes, length);
						}
					}

					if (transformindex >= numbytes)
					{
						rc = -1;
						goto done;
					}

					arith_model_reset(&selector);
					for (i = 0; i < 7; i++)
						arith_model_reset(&mtfm[i]);

					if (arith_symbol(&dec, &initial))
					{
						compcrc = (u32)arith_bitstring(&dec, &initial, 32);
						endofblocks = 1;
					}

					bwt_inverse(transform, block, numbytes);

					progress_char = '.';

					bytecount = 0;
					count = 0;
					last = 0;
					randindex = 0;
					randcount = rand_table[0];
				}

				transformindex = (s32)transform[transformindex];
				byte = block[transformindex];

				if (randomized && randcount == bytecount)
				{
					byte ^= 1;
					randindex = (randindex + 1) & 255;
					randcount += rand_table[randindex];
				}

				bytecount++;

				if (count == 4)
				{
					count = 0;
					if (byte == 0) goto retry;
					repeat = byte - 1;
					outbyte = last;
				}
				else
				{
					if (byte == last)
						count++;
					else
					{
						count = 1;
						last = byte;
					}
					outbyte = byte;
				}
			}

			outbuf[outpos++] = (u8)outbyte;
			if ((outpos & 0x3FF) == 0) PROGRESS_TICK(outpos, length);
		}
	}

	/* CRC verification -- compute over entire output */
	crc = crc32(0, outbuf, outpos);
	if (compcrc != 0 && compcrc != crc)
	{
		fprintf(stderr,
				"unsit: arsenic: CRC mismatch "
				"(expected %08lx, got %08lx)\n",
				(unsigned long)compcrc, (unsigned long)crc);
	}

done:
	free(block);
	free(transform);
	return rc;
}
