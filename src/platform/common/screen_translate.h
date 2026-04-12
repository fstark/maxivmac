/*
	SCReeN TRaNSlater
*/

/* required arguments for this template */

#ifndef ScrnTrns_DoTrans /* procedure to be created by this template */
#error "ScrnTrns_DoTrans not defined"
#endif
#ifndef ScrnTrns_Src
#error "ScrnTrns_Src not defined"
#endif
#ifndef ScrnTrns_Dst
#error "ScrnTrns_Dst not defined"
#endif
#ifndef ScrnTrns_SrcDepth
#error "ScrnTrns_SrcDepth not defined"
#endif
#ifndef ScrnTrns_DstDepth
#error "ScrnTrns_DstDepth not defined"
#endif

/* optional argument for this template */

#ifndef SCRN_TRNS_SCALE
#define SCRN_TRNS_SCALE 1
#endif

#ifndef SCRN_TRNS_DST_Z_LO
#define SCRN_TRNS_DST_Z_LO 0
#endif

/* check of parameters */

#if (ScrnTrns_SrcDepth < 4)
#error "bad ScrnTrns_SrcDepth"
#endif

#if (ScrnTrns_DstDepth < 4)
#error "bad ScrnTrns_Dst"
#endif

/* now define the procedure */

static void ScrnTrns_DoTrans(int16_t top, int16_t left, int16_t bottom, int16_t right)
{
	int i;
	int j;
	uint32_t t0;
	uint32_t t1;
	uint16_t jn = right - left;
	uint16_t SrcSkip = VMAC_SCREEN_BYTE_WIDTH - (jn << (ScrnTrns_SrcDepth - 3));
	uint8_t *pSrc = (static_cast<uint8_t *>(ScrnTrns_Src)) + (left << (ScrnTrns_SrcDepth - 3)) +
					VMAC_SCREEN_BYTE_WIDTH * (uint32_t)top;
	uint32_t *pDst = (reinterpret_cast<uint32_t *>(ScrnTrns_Dst)) + left * SCRN_TRNS_SCALE +
					 (uint32_t)VMAC_SCREEN_WIDTH * SCRN_TRNS_SCALE * SCRN_TRNS_SCALE * top;
	uint16_t DstSkip = (VMAC_SCREEN_WIDTH - jn) * SCRN_TRNS_SCALE;
#if SCRN_TRNS_SCALE > 1
	int k;
	uint32_t *p3;
	uint32_t *p4;
#endif

	for (i = bottom - top; --i >= 0;)
	{
#if SCRN_TRNS_SCALE > 1
		p3 = pDst;
#endif

		for (j = jn; --j >= 0;)
		{
#if 4 == ScrnTrns_SrcDepth
			t0 = do_get_mem_word(pSrc);
			pSrc += 2;
			t1 =
#if SCRN_TRNS_DST_Z_LO
				((t0 & 0x7C00) << 17) | ((t0 & 0x7000) << 12) | ((t0 & 0x03E0) << 14) |
				((t0 & 0x0380) << 9) | ((t0 & 0x001F) << 11) | ((t0 & 0x001C) << 6);
#else
				((t0 & 0x7C00) << 9) | ((t0 & 0x7000) << 4) | ((t0 & 0x03E0) << 6) |
				((t0 & 0x0380) << 1) | ((t0 & 0x001F) << 3) | ((t0 & 0x001C) >> 2);
#endif

#elif 5 == ScrnTrns_SrcDepth
			t0 = do_get_mem_long(pSrc);
			pSrc += 4;
#if SCRN_TRNS_DST_Z_LO
			t1 = t0 << 8;
#else
			t1 = t0;
#endif
#endif

#if SCRN_TRNS_SCALE > 1
			for (k = SCRN_TRNS_SCALE; --k >= 0;)
#endif
			{
				*pDst++ = t1;
			}
		}
		pSrc += SrcSkip;
		pDst += DstSkip;

#if SCRN_TRNS_SCALE > 1
#if SCRN_TRNS_SCALE > 2
		for (k = SCRN_TRNS_SCALE - 1; --k >= 0;)
#endif
		{
			p4 = p3;
			for (j = SCRN_TRNS_SCALE * jn; --j >= 0;)
			{
				*pDst++ = *p4++;
			}
			pDst += DstSkip;
		}
#endif /* SCRN_TRNS_SCALE > 1 */
	}
}

/* undefine template locals and parameters */

#undef ScrnTrns_DoTrans
#undef ScrnTrns_Src
#undef ScrnTrns_Dst
#undef ScrnTrns_SrcDepth
#undef ScrnTrns_DstDepth
#undef SCRN_TRNS_SCALE
#undef SCRN_TRNS_DST_Z_LO
