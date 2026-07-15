/*
 * core.c -- Portable decompression dispatch and format detection
 *
 * Shared between Linux CLI (unsit.c) and Mac (unsit_mac.c).
 * No platform-specific code.
 */

#include "unsit.h"

/* ====================================================
   Progress reporting
   ==================================================== */

void (*progress_tick)(s32 bytes_so_far, s32 total) = NULL;
char progress_char = '.';

const char *method_name(int method)
{
	switch (method)
	{
		case 0:
			return "none";
		case 2:
			return "compress";
		case 3:
			return "huffman";
		case 8:
			return "MW";
		case 13:
			return "lz+huff";
		case 15:
			return "arsenic";
		default:
			return "???";
	}
}

/* ====================================================
   Decompression dispatch
   ==================================================== */

int decompress_none(UFile *uf, u8 *outbuf, s32 length, s32 comp_length)
{
	s32 to_read = comp_length < length ? comp_length : length;
	if (uf_read(uf, outbuf, to_read) != 0) return -1;
	/* If decompressed > compressed (shouldn't happen for stored),
	   zero-fill the rest */
	if (to_read < length) memset(outbuf + to_read, 0, (size_t)(length - to_read));
	PROGRESS_TICK(length, length);
	return 0;
}

int decompress(UFile *uf, int method, u8 *outbuf, s32 length, s32 comp_length)
{
	switch (method)
	{
		case 0:
			return decompress_none(uf, outbuf, length, comp_length);
		case 2:
			return decompress_compress(uf, outbuf, length);
		case 3:
			return decompress_huffman(uf, outbuf, length);
		case 13:
			return decompress_lzss13(uf, outbuf, length);
		case 15:
			return decompress_arsenic(uf, outbuf, length);
		default:
			fprintf(stderr, "unsit: unsupported compression method %d\n", method);
			return -1;
	}
}

/* ====================================================
   Format detection
   ==================================================== */

int detect_format(UFile *uf)
{
	u8 magic[8];
	uf_seek(uf, 0);
	if (uf_read(uf, magic, 8) != 0) return FMT_UNKNOWN;

	/* StuffIt 5: starts with "StuffIt" */
	if (memcmp(magic, "StuffIt", 7) == 0) return FMT_SIT5;

	/* Classic StuffIt: bytes 0-3 = "SIT!" */
	if (magic[0] == 'S' && magic[1] == 'I' && magic[2] == 'T' && magic[3] == '!')
	{
		/* Also verify bytes 10-13 = "rLau" */
		u8 sig2[4];
		uf_seek(uf, 10);
		if (uf_read(uf, sig2, 4) == 0 && sig2[0] == 'r' && sig2[1] == 'L' && sig2[2] == 'a' &&
			sig2[3] == 'u')
			return FMT_CLASSIC;
	}

	return FMT_UNKNOWN;
}

/* ====================================================
   MacBinary detection
   ==================================================== */

int detect_macbinary(UFile *uf)
{
	u8 mb[130];
	uf->base = 0;
	uf_seek(uf, 0);
	if (uf_read(uf, mb, 130) != 0) return 0;
	if (mb[0] == 0 && mb[74] == 0 && mb[82] == 0 &&
		(memcmp(mb + 128, "St", 2) == 0 || (mb[128] == 'S' && mb[129] == 'I')))
	{
		uf->base = 128;
		return 1;
	}
	return 0;
}
