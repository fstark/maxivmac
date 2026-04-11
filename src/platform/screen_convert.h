/*
	screen_convert.h — framebuffer conversion (CLUT/BW → ARGB8888)

	BuildPalette() populates a flat uint32_t[256] palette.
	ConvertScreen() converts the full Mac framebuffer to ARGB8888.
*/

#ifndef SCREEN_CONVERT_H
#define SCREEN_CONVERT_H

#include <cstdint>

/* Build the flat uint32_t palette from current CLUT state.
   Fills clut32[256] in the DisplayState. */
void BuildPalette();

/* Unified screen converter.
   Converts the full Mac framebuffer to ARGB8888.
   palette is clut32[] for indexed depths (0–3), or nullptr for direct (4–5). */
void ConvertScreen(const uint8_t *src, uint32_t *dst, const uint32_t *palette, int depth, int width,
				   int height);

#endif /* SCREEN_CONVERT_H */
