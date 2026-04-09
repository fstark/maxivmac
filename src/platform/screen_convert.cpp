/*
	screen_convert.cpp — framebuffer conversion (CLUT/BW → ARGB8888)

	Extracted from sdl.cpp. Contains the 12 ScrnMapr instantiations,
	CLUT table building, and rect conversion dispatching.
*/

#include "platform/screen_convert.h"
#include "platform/common/osglu_ui.h"
#include "platform/common/osglu_ud.h"
#include "platform/common/osglu_common.h"
#include "platform/platform.h"
#include "core/endian.h"

uint8_t* ScalingBuff = nullptr;
uint8_t* CLUT_final = nullptr;

/* --- ScrnMapr instantiations (BW: SrcDepth 0, DstDepth 3/4/5) --- */

#define ScrnMapr_DoMap UpdateBWDepth3Copy
#define ScrnMapr_SrcDepth 0
#define ScrnMapr_DstDepth 3
#include "platform/screen_map_inst.h"

#define ScrnMapr_DoMap UpdateBWDepth4Copy
#define ScrnMapr_SrcDepth 0
#define ScrnMapr_DstDepth 4
#include "platform/screen_map_inst.h"

#define ScrnMapr_DoMap UpdateBWDepth5Copy
#define ScrnMapr_SrcDepth 0
#define ScrnMapr_DstDepth 5
#include "platform/screen_map_inst.h"

/* --- ScrnMapr instantiations (Color: SrcDepth 1/2/3, DstDepth 3/4/5) --- */

#define ScrnMapr_DoMap UpdateColorSrc1Dst3Copy
#define ScrnMapr_SrcDepth 1
#define ScrnMapr_DstDepth 3
#include "platform/screen_map_inst.h"

#define ScrnMapr_DoMap UpdateColorSrc1Dst4Copy
#define ScrnMapr_SrcDepth 1
#define ScrnMapr_DstDepth 4
#include "platform/screen_map_inst.h"

#define ScrnMapr_DoMap UpdateColorSrc1Dst5Copy
#define ScrnMapr_SrcDepth 1
#define ScrnMapr_DstDepth 5
#include "platform/screen_map_inst.h"

#define ScrnMapr_DoMap UpdateColorSrc2Dst3Copy
#define ScrnMapr_SrcDepth 2
#define ScrnMapr_DstDepth 3
#include "platform/screen_map_inst.h"

#define ScrnMapr_DoMap UpdateColorSrc2Dst4Copy
#define ScrnMapr_SrcDepth 2
#define ScrnMapr_DstDepth 4
#include "platform/screen_map_inst.h"

#define ScrnMapr_DoMap UpdateColorSrc2Dst5Copy
#define ScrnMapr_SrcDepth 2
#define ScrnMapr_DstDepth 5
#include "platform/screen_map_inst.h"

#define ScrnMapr_DoMap UpdateColorSrc3Dst3Copy
#define ScrnMapr_SrcDepth 3
#define ScrnMapr_DstDepth 3
#include "platform/screen_map_inst.h"

#define ScrnMapr_DoMap UpdateColorSrc3Dst4Copy
#define ScrnMapr_SrcDepth 3
#define ScrnMapr_DstDepth 4
#include "platform/screen_map_inst.h"

#define ScrnMapr_DoMap UpdateColorSrc3Dst5Copy
#define ScrnMapr_SrcDepth 3
#define ScrnMapr_DstDepth 5
#include "platform/screen_map_inst.h"


/* --- CLUT table building (no SDL dependency) --- */

void BuildClutTable(int bpp)
{
	int i;
	int k;
	uint32_t v;
	uint32_t CLUT_pixel[CLUT_size];
	uint32_t BWLUT_pixel[2];
	uint8_t *p4 = CLUT_final;

	int PixPerByte =
		(g_useColorMode && vMacScreenDepth > 0 && vMacScreenDepth < 4)
		? (1 << (3 - vMacScreenDepth)) : 8;

	/* Build per-entry pixel values using direct ARGB8888 packing. */
	if (g_useColorMode && vMacScreenDepth > 0 && vMacScreenDepth < 4) {
		for (i = 0; i < CLUT_size; ++i) {
			uint8_t r = CLUT_reds[i] >> 8;
			uint8_t g = CLUT_greens[i] >> 8;
			uint8_t b = CLUT_blues[i] >> 8;
			CLUT_pixel[i] = (0xFFu << 24) | (r << 16) | (g << 8) | b;
		}
	} else {
		BWLUT_pixel[1] = (0xFFu << 24); /* black */
		BWLUT_pixel[0] = (0xFFu << 24) | (255 << 16) | (255 << 8) | 255; /* white */
	}

	/* Populate CLUT_final lookup. */
	for (i = 0; i < 256; ++i) {
		for (k = PixPerByte; --k >= 0; ) {

			if (g_useColorMode && vMacScreenDepth > 0 && vMacScreenDepth < 4) {
				v = CLUT_pixel[
					(vMacScreenDepth == 3) ? i :
					((i >> (k << vMacScreenDepth)) & (CLUT_size - 1))
				];
			} else {
				v = BWLUT_pixel[(i >> k) & 1];
			}

			switch (bpp) {
				case 1: /* 8-bpp */
					*p4++ = v;
					break;
				case 2: /* 16-bpp */
					*(uint16_t *)p4 = v;
					p4 += 2;
					break;
				case 4: /* 32-bpp */
					*(uint32_t *)p4 = v;
					p4 += 4;
					break;
			}
		}
	}
}


/* --- Fast-path dispatcher --- */

void ConvertRect(int bpp, int16_t top, int16_t left, int16_t bottom, int16_t right)
{
	if (g_useColorMode && vMacScreenDepth > 0 && vMacScreenDepth < 4) {
		switch (vMacScreenDepth) {
			case 1: switch (bpp) { case 1: UpdateColorSrc1Dst3Copy(top,left,bottom,right); break; case 2: UpdateColorSrc1Dst4Copy(top,left,bottom,right); break; case 4: UpdateColorSrc1Dst5Copy(top,left,bottom,right); break; } break;
			case 2: switch (bpp) { case 1: UpdateColorSrc2Dst3Copy(top,left,bottom,right); break; case 2: UpdateColorSrc2Dst4Copy(top,left,bottom,right); break; case 4: UpdateColorSrc2Dst5Copy(top,left,bottom,right); break; } break;
			case 3: switch (bpp) { case 1: UpdateColorSrc3Dst3Copy(top,left,bottom,right); break; case 2: UpdateColorSrc3Dst4Copy(top,left,bottom,right); break; case 4: UpdateColorSrc3Dst5Copy(top,left,bottom,right); break; } break;
		}
	} else {
		switch (bpp) {
			case 1:
				UpdateBWDepth3Copy(top, left, bottom, right);
				break;
			case 2:
				UpdateBWDepth4Copy(top, left, bottom, right);
				break;
			case 4:
				UpdateBWDepth5Copy(top, left, bottom, right);
				break;
		}
	}
}


/* --- Per-pixel fallback (slow path) --- */

void ConvertRectSlow(uint8_t* dest, int pitch, int bpp,
	uint16_t top, uint16_t left, uint16_t bottom, uint16_t right)
{
	uint8_t *the_data = g_screenCompareBuff;

	for (uint16_t i = top; i < bottom; ++i) {
		for (uint16_t j = left; j < right; ++j) {
			int i0 = i;
			int j0 = j;
			uint8_t *bufp = dest + i * pitch + j * bpp;
			uint8_t *p;
			uint32_t pixel;

			if (g_useColorMode && vMacScreenDepth > 0) {
				if (vMacScreenDepth < 4) {
					p = the_data + ((i0 * vMacScreenWidth + j0)
						>> (3 - vMacScreenDepth));
					{
						uint8_t k = (*p >> (((~ j0)
								& ((1 << (3 - vMacScreenDepth)) - 1))
							<< vMacScreenDepth))
							& (CLUT_size - 1);
						/* ARGB8888 packing */
						uint8_t r = CLUT_reds[k] >> 8;
						uint8_t g = CLUT_greens[k] >> 8;
						uint8_t b = CLUT_blues[k] >> 8;
						pixel = (0xFFu << 24) | (r << 16) | (g << 8) | b;
					}
				} else if (vMacScreenDepth == 4) {
					p = the_data + ((i0 * vMacScreenWidth + j0) << 1);
					{
						uint16_t t0 = do_get_mem_word(p);
						uint8_t r = ((t0 & 0x7C00) >> 7) | ((t0 & 0x7000) >> 12);
						uint8_t g = ((t0 & 0x03E0) >> 2) | ((t0 & 0x0380) >> 7);
						uint8_t b = ((t0 & 0x001F) << 3) | ((t0 & 0x001C) >> 2);
						pixel = (0xFFu << 24) | (r << 16) | (g << 8) | b;
					}
				} else { /* depth == 5 */
					p = the_data + ((i0 * vMacScreenWidth + j0) << 2);
					pixel = (0xFFu << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
				}
			} else {
				p = the_data + ((i0 * vMacScreenWidth + j0) / 8);
				uint8_t bit = (*p >> ((~ j0) & 0x7)) & 1;
				/* BW: 0=white, 1=black */
				if (bit) {
					pixel = (0xFFu << 24); /* black */
				} else {
					pixel = (0xFFu << 24) | (255 << 16) | (255 << 8) | 255; /* white */
				}
			}

			switch (bpp) {
				case 1: /* 8-bpp */
					*bufp = pixel;
					break;
				case 2: /* 16-bpp */
					*(uint16_t *)bufp = pixel;
					break;
				case 3:
					/* Slow 24-bpp mode */
#if BIG_ENDIAN_UNALIGNED
					bufp[0] = (pixel >> 16) & 0xff;
					bufp[1] = (pixel >> 8) & 0xff;
					bufp[2] = pixel & 0xff;
#else
					bufp[0] = pixel & 0xff;
					bufp[1] = (pixel >> 8) & 0xff;
					bufp[2] = (pixel >> 16) & 0xff;
#endif
					break;
				case 4: /* 32-bpp */
					*(uint32_t *)bufp = pixel;
					break;
			}
		}
	}
}
