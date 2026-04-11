/*
	display_state.h — Consolidated display/video globals

	Replaces 14 scattered globals from osglu_common.cpp,
	screen_convert.cpp, and platform.h with a single struct
	owned by EmulatorShell.
*/

#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>

#define CLUT_STATE_SIZE 256

struct DisplayState
{
	/* Dimensions (set from MachineConfig at init) */
	uint16_t screenWidth = 640;
	uint16_t screenHeight = 480;
	uint8_t screenDepth = 3;

	/* Color mode */
	bool useColorMode = false;
	bool colorModeWorks = false;
	bool colorMappingChanged = false;
	bool colorTransValid = false;

	/* CLUT (Color Lookup Table) — written by video device */
	uint16_t clutReds[CLUT_STATE_SIZE] = {};
	uint16_t clutGreens[CLUT_STATE_SIZE] = {};
	uint16_t clutBlues[CLUT_STATE_SIZE] = {};

	/* Screen buffers & dirty tracking */
	uint8_t *screenCompareBuff = nullptr;
	bool screenChanged = false;

	/* New flat ARGB8888 palette (Phase 2) */
	uint32_t clut32[CLUT_STATE_SIZE] = {};

	/* --- Buffer lifecycle --- */

	bool allocBuffers(uint32_t screenNumBytes)
	{
		screenCompareBuff = static_cast<uint8_t *>(std::calloc(1, screenNumBytes));
		if (!screenCompareBuff) return false;
		std::memset(screenCompareBuff, 0xFF, screenNumBytes);
		return true;
	}

	void freeBuffers()
	{
		std::free(screenCompareBuff);
		screenCompareBuff = nullptr;
	}
};
