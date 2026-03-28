/*
	main — Program lifecycle and configuration accessors
*/
#pragma once

#include "core/config_loader.h"

/* --- Lifecycle --- */

// Parse command-line args and initialize configs.
extern void ProgramEarlyInit(int argc, char* argv[]);

// Allocate RAM, ROM, and video buffers.  Returns false on failure.
extern bool EmulationReserveAlloc();

extern void EmulationFreeAlloc();

// Enter the main emulation loop (does not return until quit).
extern void ProgramMain();

extern void ProgramCleanup();

// Access the parsed launch config (available after ProgramEarlyInit).
extern const LaunchConfig& GetLaunchConfig();

// Access the emulator config (available after ProgramEarlyInit).
extern const EmulatorConfig& GetEmulatorConfig();
extern EmulatorConfig& GetEmulatorConfigMut();
