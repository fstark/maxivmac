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
	int speed = 4; // 0=all-out, 1=1x, 2=2x, 3=4x, 4=8x, 5=16x
};
