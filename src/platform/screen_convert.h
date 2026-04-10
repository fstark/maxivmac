/*
	screen_convert.h — framebuffer conversion (CLUT/BW → ARGB8888)

	All depth-copy functions, CLUT table building, and rect conversion
	extracted from sdl.cpp into a standalone compilation unit.
*/

#ifndef SCREEN_CONVERT_H
#define SCREEN_CONVERT_H

#include <cstdint>

#define CLUT_FINAL_SZ (256 * 8 * 4)
	/*
		256 possible values of one byte
		8 pixels per byte maximum (when black and white)
		4 bytes per destination pixel maximum
	*/

/* Build the CLUT_final lookup table from current color mode/depth.
   Uses direct ARGB8888 packing — no SDL dependency. */
void BuildClutTable(int bpp);

/* Build the flat uint32_t palette from current CLUT state.
   Fills clut32[256] in the DisplayState.  For use with ConvertScreen(). */
void BuildPalette();

/* New unified screen converter (replaces ConvertRect + ConvertRectSlow).
   Converts the full Mac framebuffer to ARGB8888.
   palette is clut32[] for indexed depths (0−3), or nullptr for direct (4−5). */
void ConvertScreen(
	const uint8_t* src,
	uint32_t*      dst,
	const uint32_t* palette,
	int depth,
	int width,
	int height);

/* Fast-path dispatcher: selects the correct depth-copy based on
   vMacScreenDepth, g_useColorMode, and bpp. */
void ConvertRect(int bpp, int16_t top, int16_t left, int16_t bottom, int16_t right);

/* Per-pixel fallback path for unusual pitch/bpp combinations.
   Writes into caller-provided buffer with given pitch. */
void ConvertRectSlow(uint8_t* dest, int pitch, int bpp,
	uint16_t top, uint16_t left, uint16_t bottom, uint16_t right);

#endif /* SCREEN_CONVERT_H */
