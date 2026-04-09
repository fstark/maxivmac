/*
	screen_map.h — Screen mapper function template (C++23)

	Converts a rectangular region of the emulated Mac framebuffer
	from a packed source depth (1/2/4/8 bpp) to a wider destination
	depth (8/16/32 bpp) using a pre-computed colour lookup table.

	Replaces the old macro-template pattern that was #included 12 times
	with different #define parameters.
*/

#ifndef SCREEN_MAP_H
#define SCREEN_MAP_H

#include <cstdint>
#include <type_traits>
#include "platform/platform.h"

template<int SrcDepth, int DstDepth, int Scale = 1>
	requires (SrcDepth >= 0 && SrcDepth <= 3 &&
	          DstDepth >= 3 && DstDepth <= 5 &&
	          DstDepth >= SrcDepth &&
	          Scale >= 1)
static void ScreenMapConvert(
	const uint8_t* src,
	uint8_t*       dst,
	const uint8_t* map,
	int16_t top, int16_t left, int16_t bottom, int16_t right)
{
	constexpr int MapElSz   = Scale << (DstDepth - SrcDepth);
	constexpr int TranLn2Sz = (MapElSz % 4 == 0) ? 2
	                        : (MapElSz % 2 == 0) ? 1
	                        : 0;
	constexpr int TranN     = MapElSz >> TranLn2Sz;

	using TranT = std::conditional_t<(MapElSz % 4 == 0), uint32_t,
	              std::conditional_t<(MapElSz % 2 == 0), uint16_t,
	                                                      uint8_t>>;

	const uint16_t ScrnWB = vMacScreenWidth >> (3 - SrcDepth);

	uint16_t leftB  = left >> (3 - SrcDepth);
	uint16_t rightB = (right + (1 << (3 - SrcDepth)) - 1) >> (3 - SrcDepth);
	uint16_t jn      = rightB - leftB;
	uint16_t SrcSkip = ScrnWB - jn;

	const uint8_t *pSrc = src + leftB + ScrnWB * static_cast<uint32_t>(top);

	TranT *pDst = reinterpret_cast<TranT *>(dst)
		+ (leftB + ScrnWB * Scale * static_cast<uint32_t>(top)) * TranN;

	uint32_t DstSkip = SrcSkip * TranN;

	const TranT *mapT = reinterpret_cast<const TranT *>(map);

	for (int i = bottom - top; --i >= 0; ) {
		TranT *p3 = pDst;

		for (int j = jn; --j >= 0; ) {
			uint32_t t0 = *pSrc++;
			const TranT *pMap = &mapT[t0 * TranN];

			for (int k = 0; k < TranN; ++k) {
				*pDst++ = pMap[k];
			}
		}

		pSrc += SrcSkip;
		pDst += DstSkip;

		for (int s = 0; s < Scale - 1; ++s) {
			const TranT *pCopy = p3;
			for (int j = TranN * jn; --j >= 0; ) {
				*pDst++ = *pCopy++;
			}
			pDst += DstSkip;
		}
	}
}

#endif /* SCREEN_MAP_H */
