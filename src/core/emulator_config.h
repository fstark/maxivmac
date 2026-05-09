/*
	emulator_config.h

	Emulator presentation configuration — how the emulation is
	displayed and controlled. Separate from MachineConfig (which
	describes the emulated hardware).

	EmulatorConfig is mutable at runtime: fullscreen, speed, sound,
	and window scale can be toggled via hotkeys or a future UI.
*/

#pragma once
#include <cstdint>

struct EmulatorConfig
{
	// Display
	bool fullscreen = false; // start fullscreen
	bool magnify = true;	 // enable pixel scaling
	uint8_t windowScale = 1; // 1x, 2x, 3x, ...

	// Audio
	bool soundEnabled = true; // audio output

	// Speed
	int speed = 4; // shift count: 0=1x, 1=2x, 2=4x, 3=8x, 4=16x, 5=32x
};
