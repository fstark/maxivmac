/*
	display_state.h — Consolidated display/video globals

	Replaces 14 scattered globals from osglu_common.cpp,
	screen_convert.cpp, and platform.h with a single struct
	owned by EmulatorShell.
*/

#pragma once

#include <cstdint>

#define CLUT_STATE_SIZE 256

struct DisplayState {
	/* Dimensions (set from MachineConfig at init) */
	uint16_t screenWidth  = 640;
	uint16_t screenHeight = 480;
	uint8_t  screenDepth  = 3;

	/* Color mode */
	bool useColorMode        = false;
	bool colorModeWorks      = false;
	bool colorMappingChanged = false;
	bool colorTransValid     = false;

	/* CLUT (Color Lookup Table) — written by video device */
	uint16_t clutReds[CLUT_STATE_SIZE]   = {};
	uint16_t clutGreens[CLUT_STATE_SIZE] = {};
	uint16_t clutBlues[CLUT_STATE_SIZE]  = {};

	/* Screen buffers & dirty tracking */
	uint8_t* screenCompareBuff = nullptr;
	bool     screenChanged     = false;

	/* Screen conversion output */
	uint8_t* scalingBuff = nullptr;
	uint8_t* clutFinal   = nullptr;
};
