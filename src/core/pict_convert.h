/*
	pict_convert.h — Convert raw Mac pixel data to RGBA for PNG encoding.

	Two-pass alpha compositing: render on white, render on black.
	Pixels that differ between passes are transparent.
*/

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

/*
	Extract RGBA pixels from two 1-bit Mac BitMap buffers (white-bg
	and black-bg passes).

	Mac 1-bit format: packed MSB-first, 1 = black (ink), 0 = white.
	Each row is `rowBytes` bytes wide (padded to word boundary).

	Output: width * height * 4 bytes (R, G, B, A) top-to-bottom,
	left-to-right.
*/
std::vector<uint8_t> Composite1Bit(const uint8_t *whitePass, const uint8_t *blackPass, int width,
								   int height, int rowBytes);

/*
	Extract RGBA pixels from two 32-bit Mac PixMap buffers (white-bg
	and black-bg passes).

	Mac 32-bit PixMap format: 4 bytes per pixel [X][R][G][B],
	big-endian.  Each row is `rowBytes` bytes wide.

	Output: width * height * 4 bytes (R, G, B, A).
*/
std::vector<uint8_t> Composite32Bit(const uint8_t *whitePass, const uint8_t *blackPass, int width,
									int height, int rowBytes);

/*
	Encode RGBA pixel data as a PNG blob in memory.

	Returns empty vector on failure.
*/
std::vector<uint8_t> EncodeRGBAPng(const uint8_t *rgba, int width, int height);

/*
	Convert RGBA pixels to 1-bit Mac BitMap format.
	Thresholds at 50% luminance.  Packs MSB-first.
	Returns: rowBytes * height bytes.
	Sets outRowBytes to the row stride used.
*/
std::vector<uint8_t> RGBATo1Bit(const uint8_t *rgba, int width, int height, int &outRowBytes);

/*
	Convert RGBA pixels to 32-bit Mac PixMap format (XRGB).
	Returns: rowBytes * height bytes.
	Sets outRowBytes to the row stride used (width * 4, rounded up
	to even).
*/
std::vector<uint8_t> RGBATo32Bit(const uint8_t *rgba, int width, int height, int &outRowBytes);
