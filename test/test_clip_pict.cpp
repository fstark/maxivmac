/*
	test_clip_pict.cpp — Unit tests for pict_convert.h/.cpp

	Tests compositing (1-bit and 32-bit two-pass alpha), PNG encode
	roundtrip, and RGBA↔Mac format conversions.
*/

#include <doctest/doctest.h>
#include "core/pict_convert.h"
#include "stb_image.h"

#include <cstring>

/* ── 1-bit compositing ────────────────────────────────── */

TEST_CASE("Composite1Bit opaque black")
{
	/* Both passes have bit=1 → opaque black */
	uint8_t white[] = {0x80}; /* bit 7 set = black ink */
	uint8_t black[] = {0x80};
	auto rgba = Composite1Bit(white, black, 1, 1, 1);
	REQUIRE(rgba.size() == 4);
	CHECK(rgba[0] == 0);
	CHECK(rgba[1] == 0);
	CHECK(rgba[2] == 0);
	CHECK(rgba[3] == 255);
}

TEST_CASE("Composite1Bit opaque white")
{
	/* Both passes have bit=0 → opaque white */
	uint8_t white[] = {0x00};
	uint8_t black[] = {0x00};
	auto rgba = Composite1Bit(white, black, 1, 1, 1);
	REQUIRE(rgba.size() == 4);
	CHECK(rgba[0] == 255);
	CHECK(rgba[1] == 255);
	CHECK(rgba[2] == 255);
	CHECK(rgba[3] == 255);
}

TEST_CASE("Composite1Bit transparent")
{
	/* White-pass bit=0 (white), black-pass bit=1 (black) → transparent */
	uint8_t white[] = {0x00};
	uint8_t black[] = {0x80};
	auto rgba = Composite1Bit(white, black, 1, 1, 1);
	REQUIRE(rgba.size() == 4);
	CHECK(rgba[0] == 0);
	CHECK(rgba[1] == 0);
	CHECK(rgba[2] == 0);
	CHECK(rgba[3] == 0);
}

TEST_CASE("Composite1Bit mixed row")
{
	/* 3 pixels: black, white, transparent (packed in one byte) */
	/*   bit7=1(black), bit6=0(white), bit5=0(transparent on white pass) */
	uint8_t white[] = {0x80}; /* 1 0 0 ... */
	uint8_t black[] = {0xA0}; /* 1 0 1 ... */

	auto rgba = Composite1Bit(white, black, 3, 1, 1);
	REQUIRE(rgba.size() == 12);

	/* Pixel 0: both=1 → opaque black */
	CHECK(rgba[0] == 0);
	CHECK(rgba[3] == 255);

	/* Pixel 1: both=0 → opaque white */
	CHECK(rgba[4] == 255);
	CHECK(rgba[7] == 255);

	/* Pixel 2: w=0, b=1 → transparent */
	CHECK(rgba[8] == 0);
	CHECK(rgba[11] == 0);
}

TEST_CASE("Composite1Bit multi-row")
{
	/* 3×2 bitmap, rowBytes=2 (word aligned) */
	uint8_t white[] = {0xE0, 0x00,				/* row 0: 111..... */
					   0xA0, 0x00};				/* row 1: 101..... */
	uint8_t black[] = {0xE0, 0x00, 0xE0, 0x00}; /* row 1: 111..... */

	auto rgba = Composite1Bit(white, black, 3, 2, 2);
	REQUIRE(rgba.size() == 24);

	/* Row 0: all opaque black (both=1) */
	CHECK(rgba[3] == 255);
	CHECK(rgba[7] == 255);
	CHECK(rgba[11] == 255);

	/* Row 1, pixel 0: both=1 → opaque black */
	CHECK(rgba[12] == 0);
	CHECK(rgba[15] == 255);

	/* Row 1, pixel 1: w=0, b=1 → transparent */
	CHECK(rgba[16 + 3] == 0);

	/* Row 1, pixel 2: both=1 → opaque black */
	CHECK(rgba[20 + 3] == 255);
}

TEST_CASE("Composite1Bit row padding")
{
	/* Width=9 → rowBytes=2 (16 bits, word-aligned) */
	/* Only 9 pixels matter; padding bits should be ignored */
	uint8_t white[] = {0xFF, 0x80}; /* 9 bits set = all black ink */
	uint8_t black[] = {0xFF, 0x80};
	auto rgba = Composite1Bit(white, black, 9, 1, 2);
	REQUIRE(rgba.size() == 36);

	/* All 9 pixels should be opaque black */
	for (int i = 0; i < 9; i++)
	{
		CHECK(rgba[i * 4 + 0] == 0);
		CHECK(rgba[i * 4 + 3] == 255);
	}
}

/* ── PNG encode roundtrip ─────────────────────────────── */

TEST_CASE("EncodeRGBAPng roundtrip")
{
	/* Create a small 2×2 RGBA image */
	uint8_t pixels[16] = {
		255, 0,	  0,   255, /* red */
		0,	 255, 0,   255, /* green */
		0,	 0,	  255, 255, /* blue */
		255, 255, 0,   128, /* yellow, semi-transparent */
	};

	auto png = EncodeRGBAPng(pixels, 2, 2);
	REQUIRE(!png.empty());

	/* Decode with stb_image */
	int w, h, comp;
	uint8_t *decoded =
		stbi_load_from_memory(png.data(), static_cast<int>(png.size()), &w, &h, &comp, 4);
	REQUIRE(decoded != nullptr);
	CHECK(w == 2);
	CHECK(h == 2);

	/* Verify pixel values match */
	CHECK(decoded[0] == 255); /* R of red pixel */
	CHECK(decoded[1] == 0);
	CHECK(decoded[4] == 0); /* R of green pixel */
	CHECK(decoded[5] == 255);
	CHECK(decoded[8] == 0); /* R of blue pixel */
	CHECK(decoded[10] == 255);

	stbi_image_free(decoded);
}

/* ── RGBATo1Bit ───────────────────────────────────────── */

TEST_CASE("RGBATo1Bit threshold")
{
	/* Black pixel → bit=1, white pixel → bit=0, gray boundary */
	uint8_t pixels[12] = {
		0,	 0,	  0,   255, /* black → bit=1 (ink) */
		255, 255, 255, 255, /* white → bit=0 (paper) */
		127, 127, 127, 255, /* just below 50% → bit=1 */
	};

	int rowBytes = 0;
	auto bits = RGBATo1Bit(pixels, 3, 1, rowBytes);
	CHECK(rowBytes == 2); /* word-aligned */
	REQUIRE(bits.size() == 2);

	/* bit7=1(black), bit6=0(white), bit5=1(gray<128) */
	CHECK((bits[0] & 0x80) != 0); /* black → set */
	CHECK((bits[0] & 0x40) == 0); /* white → clear */
	CHECK((bits[0] & 0x20) != 0); /* gray → set (dark) */
}

TEST_CASE("RGBATo1Bit row alignment")
{
	/* Width=9 → rowBytes=2, verify padding is zeroed */
	uint8_t pixels[36];
	std::memset(pixels, 0, sizeof(pixels));
	/* Make all 9 pixels black */
	for (int i = 0; i < 9; i++)
	{
		pixels[i * 4 + 3] = 255; /* A=255, RGB=0 → black */
	}

	int rowBytes = 0;
	auto bits = RGBATo1Bit(pixels, 9, 1, rowBytes);
	CHECK(rowBytes == 2);
	REQUIRE(bits.size() == 2);

	CHECK(bits[0] == 0xFF);		  /* first 8 bits set */
	CHECK((bits[1] & 0x80) != 0); /* 9th bit set */
	CHECK((bits[1] & 0x7F) == 0); /* padding zeroed */
}

/* ── RGBATo32Bit ──────────────────────────────────────── */

TEST_CASE("RGBATo32Bit layout")
{
	/* Verify [X][R][G][B] byte order */
	uint8_t pixel[] = {0xAA, 0xBB, 0xCC, 0xFF}; /* RGBA */

	int rowBytes = 0;
	auto xrgb = RGBATo32Bit(pixel, 1, 1, rowBytes);
	CHECK(rowBytes == 4);
	REQUIRE(xrgb.size() == 4);

	CHECK(xrgb[0] == 0x00); /* X (unused) */
	CHECK(xrgb[1] == 0xAA); /* R */
	CHECK(xrgb[2] == 0xBB); /* G */
	CHECK(xrgb[3] == 0xCC); /* B */
}

TEST_CASE("RGBATo32Bit alpha discard")
{
	/* Input has alpha, output X byte is always 0 */
	uint8_t pixel[] = {0xFF, 0x00, 0x00, 0x80}; /* red, 50% alpha */

	int rowBytes = 0;
	auto xrgb = RGBATo32Bit(pixel, 1, 1, rowBytes);
	REQUIRE(xrgb.size() == 4);

	CHECK(xrgb[0] == 0x00); /* X byte is always 0, not alpha */
	CHECK(xrgb[1] == 0xFF); /* R preserved */
}

/* ── 32-bit compositing ───────────────────────────────── */

TEST_CASE("Composite32Bit opaque red")
{
	/* Both passes show same red → opaque, alpha=255 */
	uint8_t white[] = {0x00, 0xFF, 0x00, 0x00}; /* [X][R][G][B] */
	uint8_t black[] = {0x00, 0xFF, 0x00, 0x00};
	auto rgba = Composite32Bit(white, black, 1, 1, 4);
	REQUIRE(rgba.size() == 4);

	CHECK(rgba[0] == 255); /* R */
	CHECK(rgba[1] == 0);   /* G */
	CHECK(rgba[2] == 0);   /* B */
	CHECK(rgba[3] == 255); /* A */
}

TEST_CASE("Composite32Bit opaque white")
{
	uint8_t white[] = {0x00, 0xFF, 0xFF, 0xFF};
	uint8_t black[] = {0x00, 0xFF, 0xFF, 0xFF};
	auto rgba = Composite32Bit(white, black, 1, 1, 4);
	REQUIRE(rgba.size() == 4);
	CHECK(rgba[0] == 255);
	CHECK(rgba[1] == 255);
	CHECK(rgba[2] == 255);
	CHECK(rgba[3] == 255);
}

TEST_CASE("Composite32Bit fully transparent")
{
	/* White pass = BG color (white), black pass = BG color (black) → transparent */
	uint8_t white[] = {0x00, 0xFF, 0xFF, 0xFF}; /* white bg shines through */
	uint8_t black[] = {0x00, 0x00, 0x00, 0x00}; /* black bg shines through */
	auto rgba = Composite32Bit(white, black, 1, 1, 4);
	REQUIRE(rgba.size() == 4);
	CHECK(rgba[3] == 0); /* fully transparent */
}

TEST_CASE("Composite32Bit semi-transparent")
{
	/* 50% alpha red pixel:
	   on white BG: blended R = 0.5*255 + 0.5*255 = 255, G=B=128
	   on black BG: blended R = 0.5*255 + 0.5*0   = 128, G=B=0

	   So white pass = (0, 255, 128, 128), black pass = (0, 128, 0, 0) */
	uint8_t white[] = {0x00, 0xFF, 0x80, 0x80};
	uint8_t black[] = {0x00, 0x80, 0x00, 0x00};
	auto rgba = Composite32Bit(white, black, 1, 1, 4);
	REQUIRE(rgba.size() == 4);

	/* aR = 255-(255-128) = 128, aG = 255-(128-0) = 127, aB same
	   a = min(128,127,127) = 127 */
	CHECK(rgba[3] >= 126);
	CHECK(rgba[3] <= 128);

	/* Recovered R = 128*255/127 ≈ 257 → clamped to 255 */
	CHECK(rgba[0] == 255);
}

TEST_CASE("Composite32Bit row padding")
{
	/* rowBytes=8 but only 1 pixel (4 bytes used), 4 bytes padding */
	uint8_t white[] = {0x00, 0x80, 0x80, 0x80, 0xDE, 0xAD, 0xBE, 0xEF};
	uint8_t black[] = {0x00, 0x80, 0x80, 0x80, 0xCA, 0xFE, 0xBA, 0xBE};
	auto rgba = Composite32Bit(white, black, 1, 1, 8);
	REQUIRE(rgba.size() == 4);

	/* Both passes identical for the pixel → opaque */
	CHECK(rgba[3] == 255);
	CHECK(rgba[0] == 128);
}

TEST_CASE("RGBATo1Bit roundtrip")
{
	/* Simple black-and-white image → 1-bit → verify bits */
	uint8_t pixels[] = {
		0,	 0,	  0,   255, /* black */
		255, 255, 255, 255, /* white */
		0,	 0,	  0,   255, /* black */
		255, 255, 255, 255, /* white */
	};

	int rowBytes = 0;
	auto bits = RGBATo1Bit(pixels, 2, 2, rowBytes);
	CHECK(rowBytes == 2);

	/* Row 0: bit7=1(black), bit6=0(white) → 0x80 */
	CHECK(bits[0] == 0x80);
	/* Row 1: same pattern */
	CHECK(bits[2] == 0x80);
}

/* ── Phase 8: additional import/conversion tests ──────── */

TEST_CASE("RGBATo1Bit black pixel")
{
	uint8_t pixel[] = {0, 0, 0, 255}; /* RGBA black */
	int rowBytes = 0;
	auto bits = RGBATo1Bit(pixel, 1, 1, rowBytes);
	CHECK(rowBytes == 2);
	CHECK((bits[0] & 0x80) != 0); /* bit = 1 (ink) */
}

TEST_CASE("RGBATo1Bit white pixel")
{
	uint8_t pixel[] = {255, 255, 255, 255}; /* RGBA white */
	int rowBytes = 0;
	auto bits = RGBATo1Bit(pixel, 1, 1, rowBytes);
	CHECK(rowBytes == 2);
	CHECK((bits[0] & 0x80) == 0); /* bit = 0 (paper) */
}

TEST_CASE("RGBATo1Bit gray boundary")
{
	/* 127 luminance → below 128 → ink (bit=1) */
	uint8_t pixel[] = {127, 127, 127, 255};
	int rowBytes = 0;
	auto bits = RGBATo1Bit(pixel, 1, 1, rowBytes);
	CHECK((bits[0] & 0x80) != 0);
}

TEST_CASE("RGBATo32Bit red pixel")
{
	uint8_t pixel[] = {255, 0, 0, 255}; /* RGBA red */
	int rowBytes = 0;
	auto xrgb = RGBATo32Bit(pixel, 1, 1, rowBytes);
	REQUIRE(xrgb.size() == 4);
	CHECK(xrgb[0] == 0x00);
	CHECK(xrgb[1] == 0xFF);
	CHECK(xrgb[2] == 0x00);
	CHECK(xrgb[3] == 0x00);
}

TEST_CASE("WritePixels row stride padding")
{
	/* 1-bit: width=1 → rowBytes=2, verify extra byte zeroed */
	uint8_t pixel[] = {0, 0, 0, 255}; /* black */
	int rowBytes = 0;
	auto bits = RGBATo1Bit(pixel, 1, 1, rowBytes);
	CHECK(rowBytes == 2);
	REQUIRE(bits.size() == 2);
	CHECK((bits[0] & 0x80) != 0); /* bit set */
	CHECK(bits[1] == 0);		  /* padding byte zeroed */
}
