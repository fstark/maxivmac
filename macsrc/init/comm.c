/*
	maxivmac INIT — comm.c
	Extension discovery, register access, debug logging,
	KV helpers, and utility functions.
*/

#include "defs.h"

/* ---- Extension discovery ---- */

char *find_reg_base(void)
{
	MyDriverDat_R *sv;
	sv = *(MyDriverDat_R **)kSonyVarsPtr;
	if (sv == NULL) return NULL;
	if (sv->zeroes[0] != 0 || sv->zeroes[1] != 0 || sv->zeroes[2] != 0) return NULL;
	if (sv->checkval != kCheckVal) return NULL;
	if (sv->pokeaddr == 0) return NULL;
	return (char *)(sv->pokeaddr + 0x20);
}

/* ---- Register access ---- */

void reg_set(char *base, int n, unsigned long v)
{
	*(unsigned long *)(base + REG_P(n)) = v;
}

unsigned long reg_get(char *base, int n)
{
	return *(unsigned long *)(base + REG_P(n));
}

void reg_command(char *base, unsigned short cmd)
{
	*(unsigned short *)(base + REG_COMMAND) = cmd;
}

unsigned short reg_result(char *base)
{
	return *(unsigned short *)(base + REG_RESULT);
}

/* ---- Guest globals via host storage ---- */

Globals *get_globals(void)
{
	char *base = find_reg_base();
	if (base == NULL) return NULL;
	reg_set(base, 0, 0);
	reg_set(base, 1, 0); /* GET */
	reg_command(base, 0x020E);
	return (Globals *)reg_get(base, 0);
}

void set_globals(char *base, Globals *g)
{
	reg_set(base, 0, (unsigned long)g);
	reg_set(base, 1, 1); /* SET */
	reg_command(base, 0x020E);
}

/* ---- KV helpers (clipboard host-side storage) ---- */

void kv_set(char *regBase, unsigned long key, unsigned long val)
{
	reg_set(regBase, 0, key);
	reg_set(regBase, 1, val);
	reg_command(regBase, kClipKVSet);
}

unsigned long kv_get(char *regBase, unsigned long key)
{
	reg_set(regBase, 0, key);
	reg_command(regBase, kClipKVGet);
	return reg_get(regBase, 0);
}

/* ---- Debug log (drive subsystem, $020D) ---- */

void dbg_log6(char *base, char *fmt, unsigned long a, unsigned long b, unsigned long c,
			  unsigned long d, unsigned long e, unsigned long f)
{
	reg_set(base, 0, (unsigned long)fmt);
	reg_set(base, 1, a);
	reg_set(base, 2, b);
	reg_set(base, 3, c);
	reg_set(base, 4, d);
	reg_set(base, 5, e);
	reg_set(base, 6, f);
	reg_command(base, 0x020D);
}

/* ---- Structured trap logging (host-side pretty-print) ---- */

void log_trap(char *base, unsigned short trapWord, char *pb, short action, short err,
			  short flags)
{
	reg_set(base, 0, (unsigned long)trapWord);
	reg_set(base, 1, (unsigned long)pb);
	reg_set(base, 2, (unsigned long)action);
	reg_set(base, 3, (unsigned long)(short)err);
	reg_set(base, 4, (unsigned long)flags);
	reg_command(base, 0x020F);
}

/* ---- Fatal shutdown ---- */

void dbg_fatal6(char *base, char *fmt, unsigned long a, unsigned long b, unsigned long c,
				unsigned long d, unsigned long e, unsigned long f)
{
	reg_set(base, 0, (unsigned long)fmt);
	reg_set(base, 1, a);
	reg_set(base, 2, b);
	reg_set(base, 3, c);
	reg_set(base, 4, d);
	reg_set(base, 5, e);
	reg_set(base, 6, f);
	reg_command(base, 0x0214);
}

/* ---- Hex dump ---- */

static char s_hexChars[] = "0123456789ABCDEF";
static char s_hexLine[80];

void dbg_hexdump(char *regBase, char *label, unsigned char *addr, short len)
{
	short i, col;
	char *p;

	dbg_log2(regBase, "DUMP %s at %lx:", (unsigned long)label, (unsigned long)addr);

	for (i = 0; i < len; i += 16)
	{
		p = s_hexLine;
		*p++ = s_hexChars[(i >> 12) & 0xF];
		*p++ = s_hexChars[(i >> 8) & 0xF];
		*p++ = s_hexChars[(i >> 4) & 0xF];
		*p++ = s_hexChars[i & 0xF];
		*p++ = ':';
		*p++ = ' ';
		for (col = 0; col < 16 && (i + col) < len; col++)
		{
			unsigned char b = addr[i + col];
			*p++ = s_hexChars[b >> 4];
			*p++ = s_hexChars[b & 0xF];
			*p++ = ' ';
		}
		for (; col < 16; col++)
		{
			*p++ = ' ';
			*p++ = ' ';
			*p++ = ' ';
		}
		*p++ = ' ';
		for (col = 0; col < 16 && (i + col) < len; col++)
		{
			unsigned char b = addr[i + col];
			*p++ = (b >= 0x20 && b < 0x7F) ? b : '.';
		}
		*p = 0;
		dbg_log1(regBase, " %s", (unsigned long)s_hexLine);
	}
}

/* ---- String/memory helpers ---- */

void pstr_copy(char *dst, char *src)
{
	short i, len = (unsigned char)src[0];
	for (i = 0; i <= len; i++)
		dst[i] = src[i];
}

void pstr_copy_max(char *dst, char *src, short maxLen)
{
	short len = (unsigned char)src[0];
	if (len > maxLen) len = maxLen;
	dst[0] = len;
	{
		short i;
		for (i = 0; i < len; i++)
			dst[1 + i] = src[1 + i];
	}
}

void mem_zero(char *dst, short len)
{
	short i;
	for (i = 0; i < len; i++)
		dst[i] = 0;
}

void mem_copy(char *dst, char *src, short len)
{
	short i;
	for (i = 0; i < len; i++)
		dst[i] = src[i];
}

Boolean pstr_equal(unsigned char *a, unsigned char *b)
{
	short i, len = a[0];
	if (b[0] != len) return false;
	for (i = 1; i <= len; i++)
		if (a[i] != b[i]) return false;
	return true;
}

OSErr host_err(char *base)
{
	return (short)reg_result(base);
}

/* Like host_err but returns kPassThrough (1) when the host says
   kNotOurs — used by non-refBased PB handlers so that the dispatch
   loop passes the call through to ROM. */
OSErr host_err_passthrough(char *base)
{
	if ((unsigned short)reg_result(base) == kNotOurs) return kPassThrough;
	return (short)reg_result(base);
}
