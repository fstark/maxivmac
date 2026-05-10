/*
	pict_convert.cpp — Convert raw Mac pixel data to RGBA for PNG encoding.

	Implements two-pass alpha compositing (white-bg + black-bg) and
	format conversion between RGBA and Mac BitMap/PixMap layouts.
*/

#include "core/pict_convert.h"
#include "stb_image_write.h"

#include <algorithm>
#include <cstring>

static void PngWriteFunc(void *context, void *data, int size)
{
	auto *vec = static_cast<std::vector<uint8_t> *>(context);
	auto *bytes = static_cast<const uint8_t *>(data);
	vec->insert(vec->end(), bytes, bytes + size);
}

std::vector<uint8_t> Composite1Bit(const uint8_t *whitePass, const uint8_t *blackPass, int width,
								   int height, int rowBytes)
{
	std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4);

	for (int y = 0; y < height; y++)
	{
		int rowOff = y * rowBytes;
		for (int x = 0; x < width; x++)
		{
			int byteIdx = rowOff + (x / 8);
			uint8_t bitMask = 0x80u >> (x % 8);

			/* Mac convention: bit=1 → black ink, bit=0 → white paper */
			bool wBit = (whitePass[byteIdx] & bitMask) != 0;
			bool bBit = (blackPass[byteIdx] & bitMask) != 0;

			size_t outIdx = (static_cast<size_t>(y) * width + x) * 4;

			if (wBit && bBit)
			{
				/* Both passes drew black ink → opaque black */
				rgba[outIdx + 0] = 0;
				rgba[outIdx + 1] = 0;
				rgba[outIdx + 2] = 0;
				rgba[outIdx + 3] = 255;
			}
			else if (!wBit && !bBit)
			{
				/* Both passes show white → opaque white */
				rgba[outIdx + 0] = 255;
				rgba[outIdx + 1] = 255;
				rgba[outIdx + 2] = 255;
				rgba[outIdx + 3] = 255;
			}
			else if (!wBit && bBit)
			{
				/* White on white-pass, black on black-pass → transparent */
				rgba[outIdx + 0] = 0;
				rgba[outIdx + 1] = 0;
				rgba[outIdx + 2] = 0;
				rgba[outIdx + 3] = 0;
			}
			else
			{
				/* wBit=1, bBit=0: shouldn't happen with normal QD drawing.
				   Treat as opaque black (conservative). */
				rgba[outIdx + 0] = 0;
				rgba[outIdx + 1] = 0;
				rgba[outIdx + 2] = 0;
				rgba[outIdx + 3] = 255;
			}
		}
	}

	return rgba;
}

std::vector<uint8_t> Composite32Bit(const uint8_t *whitePass, const uint8_t *blackPass, int width,
									int height, int rowBytes)
{
	std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4);

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			int off = y * rowBytes + x * 4;

			/* Mac 32-bit PixMap: [X][R][G][B] big-endian */
			uint8_t wR = whitePass[off + 1];
			uint8_t wG = whitePass[off + 2];
			uint8_t wB = whitePass[off + 3];

			uint8_t bR = blackPass[off + 1];
			uint8_t bG = blackPass[off + 2];
			uint8_t bB = blackPass[off + 3];

			/* Alpha per channel: alpha = 255 - (white - black)
			   Fully opaque: white == black → alpha = 255
			   Fully transparent: white=255, black=0 → alpha = 0 */
			int aR = 255 - (static_cast<int>(wR) - static_cast<int>(bR));
			int aG = 255 - (static_cast<int>(wG) - static_cast<int>(bG));
			int aB = 255 - (static_cast<int>(wB) - static_cast<int>(bB));

			/* Use minimum alpha (most transparent channel wins) */
			int a = std::min({aR, aG, aB});
			a = std::clamp(a, 0, 255);

			size_t outIdx = (static_cast<size_t>(y) * width + x) * 4;

			/* Recover un-premultiplied color from black-pass values */
			if (a > 0)
			{
				rgba[outIdx + 0] =
					static_cast<uint8_t>(std::clamp(static_cast<int>(bR) * 255 / a, 0, 255));
				rgba[outIdx + 1] =
					static_cast<uint8_t>(std::clamp(static_cast<int>(bG) * 255 / a, 0, 255));
				rgba[outIdx + 2] =
					static_cast<uint8_t>(std::clamp(static_cast<int>(bB) * 255 / a, 0, 255));
			}
			else
			{
				rgba[outIdx + 0] = 0;
				rgba[outIdx + 1] = 0;
				rgba[outIdx + 2] = 0;
			}
			rgba[outIdx + 3] = static_cast<uint8_t>(a);
		}
	}

	return rgba;
}

std::vector<uint8_t> EncodeRGBAPng(const uint8_t *rgba, int width, int height)
{
	std::vector<uint8_t> result;
	int ok = stbi_write_png_to_func(PngWriteFunc, &result, width, height, 4, rgba, width * 4);
	if (!ok) return {};
	return result;
}

std::vector<uint8_t> RGBATo1Bit(const uint8_t *rgba, int width, int height, int &outRowBytes)
{
	/* Word-aligned row stride (Mac BitMap convention) */
	outRowBytes = ((width + 15) / 16) * 2;
	std::vector<uint8_t> bits(static_cast<size_t>(outRowBytes) * height, 0);

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			size_t srcIdx = (static_cast<size_t>(y) * width + x) * 4;
			/* Luminance: 77R + 150G + 29B (integer approximation of BT.601) */
			int lum = (rgba[srcIdx + 0] * 77 + rgba[srcIdx + 1] * 150 + rgba[srcIdx + 2] * 29) >> 8;

			if (lum < 128)
			{
				/* Dark → set bit (= black ink in Mac convention) */
				int byteIdx = y * outRowBytes + (x / 8);
				bits[byteIdx] |= (0x80u >> (x % 8));
			}
			/* Light → bit stays 0 (= white paper) */
		}
	}

	return bits;
}

std::vector<uint8_t> RGBATo32Bit(const uint8_t *rgba, int width, int height, int &outRowBytes)
{
	outRowBytes = width * 4;
	std::vector<uint8_t> xrgb(static_cast<size_t>(outRowBytes) * height);

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			size_t srcIdx = (static_cast<size_t>(y) * width + x) * 4;
			size_t dstIdx = static_cast<size_t>(y) * outRowBytes + x * 4;

			xrgb[dstIdx + 0] = 0x00;			 /* X (unused byte) */
			xrgb[dstIdx + 1] = rgba[srcIdx + 0]; /* R */
			xrgb[dstIdx + 2] = rgba[srcIdx + 1]; /* G */
			xrgb[dstIdx + 3] = rgba[srcIdx + 2]; /* B */
												 /* Alpha is discarded (Mac PixMap has no alpha) */
		}
	}

	return xrgb;
}
