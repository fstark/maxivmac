/*
	Big-endian memory access for 68k emulation.

	The 68k is big-endian. These portable byte-at-a-time accessors
	are optimally compiled by any modern compiler (GCC/Clang emit
	bswap+load on x86/ARM at -O2).
*/

#pragma once

#include <cstdint>

static inline uint8_t do_get_mem_byte(uint8_t *a)
{
	return *a;
}

static inline uint16_t do_get_mem_word(uint8_t *a)
{
	return ((uint16_t)a[0] << 8) | (uint16_t)a[1];
}

static inline uint32_t do_get_mem_long(uint8_t *a)
{
	return ((uint32_t)a[0] << 24) | ((uint32_t)a[1] << 16)
		| ((uint32_t)a[2] << 8) | (uint32_t)a[3];
}

static inline void do_put_mem_byte(uint8_t *a, uint8_t v)
{
	*a = v;
}

static inline void do_put_mem_word(uint8_t *a, uint16_t v)
{
	a[0] = v >> 8;
	a[1] = v;
}

static inline void do_put_mem_long(uint8_t *a, uint32_t v)
{
	a[0] = v >> 24;
	a[1] = v >> 16;
	a[2] = v >> 8;
	a[3] = v;
}
