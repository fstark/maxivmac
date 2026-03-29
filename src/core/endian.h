/*
	ENDIAN ACcess

	Deals with endian issues in memory access.

	This code is adapted from code in the Un*x Amiga Emulator by
	Bernd Schmidt, as found in vMac by Philip Cummins.
*/

#pragma once


#define do_get_mem_byte(a) ((uint8_t)*((uint8_t *)(a)))

#if BIG_ENDIAN_UNALIGNED
#define do_get_mem_word(a) ((uint16_t)*((uint16_t *)(a)))
#else
static inline uint16_t do_get_mem_word(uint8_t * a)
{
#if LITTLE_ENDIAN_UNALIGNED
	uint16_t b = (*((uint16_t *)(a)));

	return ((b & 0x00FF) << 8) | ((b >> 8) & 0x00FF);
#else
	return (((uint16_t)*a) << 8) | ((uint16_t)*(a + 1));
#endif
}
#endif

#if BIG_ENDIAN_UNALIGNED
#define do_get_mem_long(a) ((uint32_t)*((uint32_t *)(a)))
#elif HAVE_SWAP_UI5R && LITTLE_ENDIAN_UNALIGNED
#define do_get_mem_long(a) (MySwapUi5r((uint32_t)*((uint32_t *)(a))))
#else
static inline uint32_t do_get_mem_long(uint8_t * a)
{
#if LITTLE_ENDIAN_UNALIGNED
	uint32_t b = (*((uint32_t *)(a)));
	uint16_t b1 = b;
	uint16_t b2 = b >> 16;
	uint16_t c1 = ((b1 & 0x00FF) << 8) | ((b1 >> 8) & 0x00FF);
	uint16_t c2 = ((b2 & 0x00FF) << 8) | ((b2 >> 8) & 0x00FF);

	return (((uint32_t)c1) << 16) | ((uint32_t)c2);
	/*
		better, though still doesn't use BSWAP
		instruction with apple tools for intel.
	*/
#else
	return (((uint32_t)*a) << 24) | (((uint32_t)*(a + 1)) << 16)
		| (((uint32_t)*(a + 2)) << 8) | ((uint32_t)*(a + 3));
#endif
}
#endif

#define do_put_mem_byte(a, v) ((*((uint8_t *)(a))) = (v))

#if BIG_ENDIAN_UNALIGNED
#define do_put_mem_word(a, v) ((*((uint16_t *)(a))) = (v))
#else
static inline void do_put_mem_word(uint8_t * a, uint16_t v)
{
#if LITTLE_ENDIAN_UNALIGNED
	uint16_t b = ((v & 0x00FF) << 8) | ((v >> 8) & 0x00FF);

	*(uint16_t *)a = b;
#else
	*a = v >> 8;
	*(a + 1) = v;
#endif
}
#endif

#if BIG_ENDIAN_UNALIGNED
#define do_put_mem_long(a, v) ((*((uint32_t *)(a))) = (v))
#elif HAVE_SWAP_UI5R && LITTLE_ENDIAN_UNALIGNED
#define do_put_mem_long(a, v) ((*((uint32_t *)(a))) = MySwapUi5r(v))
#else
static inline void do_put_mem_long(uint8_t * a, uint32_t v)
{
#if LITTLE_ENDIAN_UNALIGNED
	uint16_t b1 = v;
	uint16_t b2 = v >> 16;
	uint16_t c1 = ((b1 & 0x00FF) << 8) | ((b1 >> 8) & 0x00FF);
	uint16_t c2 = ((b2 & 0x00FF) << 8) | ((b2 >> 8) & 0x00FF);

	*(uint32_t *)a = (c1 << 16) | c2;
#else
	*a = v >> 24;
	*(a + 1) = v >> 16;
	*(a + 2) = v >> 8;
	*(a + 3) = v;
#endif
}
#endif
