/*
	screen_convert.cpp — framebuffer conversion (CLUT/BW → ARGB8888)

	BuildPalette() + ConvertScreen() replace the old BuildClutTable,
	ConvertRect, ConvertRectSlow, and ScreenMapConvert<> pipeline.
*/

#include "platform/screen_convert.h"
#include "platform/display_state.h"
#include "platform/platform.h"


/* --- CLUT table building (no SDL dependency) --- */

void BuildPalette()
{
	DisplayState& ds = GetDisplayState();
	int depth = (ds.useColorMode && ds.screenDepth > 0 && ds.screenDepth < 4)
		? ds.screenDepth : 0;

	if (depth == 0) {
		/* B&W mode */
		ds.clut32[0] = 0xFFFFFFFF; /* white */
		ds.clut32[1] = 0xFF000000; /* black */
	} else {
		int nColors = 1 << (1 << depth);
		for (int i = 0; i < nColors; ++i) {
			uint8_t r = ds.clutReds[i] >> 8;
			uint8_t g = ds.clutGreens[i] >> 8;
			uint8_t b = ds.clutBlues[i] >> 8;
			ds.clut32[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
		}
	}
}


/* --- New unified converter (Phase 3) --- */

namespace {

template<int Depth>
void ConvertScreenIndexed(
	const uint8_t* src, uint32_t* dst,
	const uint32_t* palette, int width, int height)
{
	constexpr int pixPerByte = 1 << (3 - Depth);
	constexpr int pixelMask  = (1 << (1 << Depth)) - 1;
	const int srcStride = width >> (3 - Depth);

	for (int row = 0; row < height; ++row) {
		const uint8_t* pSrc = src + row * srcStride;
		for (int col = 0; col < srcStride; ++col) {
			uint8_t val = *pSrc++;
			for (int k = pixPerByte - 1; k >= 0; --k) {
				int index = (val >> (k << Depth)) & pixelMask;
				*dst++ = palette[index];
			}
		}
	}
}

static void ConvertScreenDepth4(
	const uint8_t* src, uint32_t* dst, int width, int height)
{
	int nPixels = width * height;
	for (int i = 0; i < nPixels; ++i) {
		uint16_t rgb = (src[0] << 8) | src[1];
		uint8_t r = ((rgb >> 10) & 0x1F) * 255 / 31;
		uint8_t g = ((rgb >>  5) & 0x1F) * 255 / 31;
		uint8_t b = ((rgb >>  0) & 0x1F) * 255 / 31;
		*dst++ = 0xFF000000u | (r << 16) | (g << 8) | b;
		src += 2;
	}
}

static void ConvertScreenDepth5(
	const uint8_t* src, uint32_t* dst, int width, int height)
{
	int nPixels = width * height;
	for (int i = 0; i < nPixels; ++i) {
		*dst++ = 0xFF000000u | (src[1] << 16) | (src[2] << 8) | src[3];
		src += 4;
	}
}

} /* anonymous namespace */

void ConvertScreen(
	const uint8_t* src, uint32_t* dst,
	const uint32_t* palette, int depth, int width, int height)
{
	switch (depth) {
		case 0: ConvertScreenIndexed<0>(src, dst, palette, width, height); break;
		case 1: ConvertScreenIndexed<1>(src, dst, palette, width, height); break;
		case 2: ConvertScreenIndexed<2>(src, dst, palette, width, height); break;
		case 3: ConvertScreenIndexed<3>(src, dst, palette, width, height); break;
		case 4: ConvertScreenDepth4(src, dst, width, height); break;
		case 5: ConvertScreenDepth5(src, dst, width, height); break;
		default: break;
	}
}
