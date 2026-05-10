/*
	extn_clip_pict.cpp — PICT clipboard command handlers.

	Handles PictExport ($109), PictHasImage ($10A), PictImport ($10B).
	PictExport receives two-pass pixel data from the guest (white-bg
	and black-bg renders), composites to RGBA, encodes as PNG, and
	places on the host clipboard.
*/

#include "core/extn_clip_pict.h"
#include "core/extn_clip.h"
#include "core/pict_convert.h"
#include "platform/clipboard_image.h"
#include "platform/common/clipboard.h"

#include <cstdint>
#include <vector>

/* Guest RAM access */
extern uint8_t get_vm_byte(uint32_t addr);
extern void put_vm_byte(uint32_t addr, uint8_t b);
extern uint16_t get_vm_word(uint32_t addr);
extern uint32_t get_vm_long(uint32_t addr);

/* ── State for two-pass compositing ──────────────────── */

/* Pixel data from the white-background pass, held until the
   black-background pass arrives. */
static std::vector<uint8_t> s_passWhite;

/* Metadata from the first pass. */
static int s_passWidth = 0;
static int s_passHeight = 0;
static int s_passDepth = 0; /* 1 or 32 */
static int s_passRowBytes = 0;
static bool s_haveWhitePass = false;

/* ── Read pixel data from guest RAM ──────────────────── */

/*
	Read a BitMap or PixMap struct from guest RAM and bulk-read
	the pixel data it points to.

	Mac struct layouts (big-endian in guest RAM):

	BitMap (14 bytes):
	  +0  long   baseAddr
	  +4  word   rowBytes      (high bit = 0)
	  +6  Rect   bounds (8 bytes: top, left, bottom, right)

	PixMap (extends BitMap, 50 bytes total):
	  +4  word   rowBytes      (high bit = 1, mask 0x3FFF)
	  +34 word   pixelSize     (bits per pixel)

	Discrimination: if rawRowBytes & 0x8000 → PixMap.
*/
static void ReadPixelsFromGuest(uint32_t structPtr, std::vector<uint8_t> &pixels, int &width,
								int &height, int &depth, int &rowBytes)
{
	uint16_t rawRB = get_vm_word(structPtr + 4);
	bool isPixMap = (rawRB & 0x8000) != 0;
	rowBytes = rawRB & 0x3FFF;

	int16_t top = static_cast<int16_t>(get_vm_word(structPtr + 6));
	int16_t left = static_cast<int16_t>(get_vm_word(structPtr + 8));
	int16_t bottom = static_cast<int16_t>(get_vm_word(structPtr + 10));
	int16_t right = static_cast<int16_t>(get_vm_word(structPtr + 12));

	width = right - left;
	height = bottom - top;
	depth = isPixMap ? get_vm_word(structPtr + 34) : 1;

	uint32_t baseAddr = get_vm_long(structPtr);
	size_t bufSize = static_cast<size_t>(rowBytes) * height;

	pixels.resize(bufSize);
	for (size_t i = 0; i < bufSize; ++i)
		pixels[i] = get_vm_byte(baseAddr + static_cast<uint32_t>(i));
}

/* ── Command handlers ────────────────────────────────── */

void HandlePictExport(uint32_t regParam[], uint16_t &regResult)
{
	uint32_t structPtr = regParam[0];
	uint32_t pass = regParam[1]; /* 0 = white bg, 1 = black bg */

	std::vector<uint8_t> pixels;
	int width, height, depth, rowBytes;
	ReadPixelsFromGuest(structPtr, pixels, width, height, depth, rowBytes);

	if (width <= 0 || height <= 0 || pixels.empty())
	{
		regResult = 1;
		return;
	}

	if (pass == 0)
	{
		/* White-background pass — stash for later */
		s_passWhite = std::move(pixels);
		s_passWidth = width;
		s_passHeight = height;
		s_passDepth = depth;
		s_passRowBytes = rowBytes;
		s_haveWhitePass = true;
		regResult = 0;
		return;
	}

	/* Black-background pass — composite and push to host clipboard */
	if (!s_haveWhitePass || width != s_passWidth || height != s_passHeight)
	{
		s_haveWhitePass = false;
		regResult = 1;
		return;
	}

	std::vector<uint8_t> rgba;
	if (depth == 1)
		rgba = Composite1Bit(s_passWhite.data(), pixels.data(), width, height, rowBytes);
	else
		rgba = Composite32Bit(s_passWhite.data(), pixels.data(), width, height, rowBytes);

	auto png = EncodeRGBAPng(rgba.data(), width, height);
	if (!png.empty())
	{
		HostClipSetImage(png.data(), png.size());
		ExtnClipMarkImageExported();
	}

	s_passWhite.clear();
	s_haveWhitePass = false;
	regResult = 0;
}

void HandlePictHasImage(uint32_t regParam[], uint16_t &regResult)
{
	int w = 0, h = 0;
	bool has = HostClipHasImage(&w, &h);

	regParam[0] = has ? 1 : 0;
	regParam[1] = static_cast<uint32_t>(w);
	regParam[2] = static_cast<uint32_t>(h);
	regResult = 0;
}

void HandlePictImport(uint32_t regParam[], uint16_t &regResult)
{
	uint32_t bufAddr = regParam[0];
	uint32_t rowBytes = regParam[1];
	uint32_t depth = regParam[2];
	uint32_t width = regParam[3];
	uint32_t height = regParam[4];

	/* Decode PNG from host clipboard to RGBA */
	int imgW = 0, imgH = 0;
	auto rgba = HostClipGetImageRGBA(&imgW, &imgH);
	if (rgba.empty())
	{
		regResult = 1;
		return;
	}

	/* Require exact dimension match (guest allocates from PictHasImage) */
	if (static_cast<uint32_t>(imgW) != width || static_cast<uint32_t>(imgH) != height)
	{
		regResult = 2;
		return;
	}

	/* Convert RGBA to guest format and write into guest RAM */
	if (depth == 1)
	{
		int outRB = 0;
		auto bits = RGBATo1Bit(rgba.data(), imgW, imgH, outRB);

		for (int y = 0; y < imgH; ++y)
		{
			int copyBytes =
				(outRB < static_cast<int>(rowBytes)) ? outRB : static_cast<int>(rowBytes);
			for (int x = 0; x < copyBytes; ++x)
			{
				put_vm_byte(bufAddr + y * rowBytes + x, bits[y * outRB + x]);
			}
		}
	}
	else
	{
		int outRB = 0;
		auto xrgb = RGBATo32Bit(rgba.data(), imgW, imgH, outRB);

		for (int y = 0; y < imgH; ++y)
		{
			int copyBytes =
				(outRB < static_cast<int>(rowBytes)) ? outRB : static_cast<int>(rowBytes);
			for (int x = 0; x < copyBytes; ++x)
			{
				put_vm_byte(bufAddr + y * rowBytes + x, xrgb[y * outRB + x]);
			}
		}
	}

	regResult = 0;
}

void ExtnPictReset()
{
	s_passWhite.clear();
	s_passWidth = 0;
	s_passHeight = 0;
	s_passDepth = 0;
	s_passRowBytes = 0;
	s_haveWhitePass = false;
}
