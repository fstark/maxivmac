/*
	main — Program lifecycle and configuration accessors
*/
#pragma once

#include "core/config_loader.h"

/* --- Lifecycle --- */

// Parse command-line args and initialize configs.
extern void ProgramEarlyInit(int argc, char *argv[]);

// Allocate RAM, ROM, and video buffers.  Returns false on failure.
extern bool EmulationReserveAlloc();

extern void EmulationFreeAlloc();

// Initialize emulation subsystems. Returns false on failure.
extern bool ProgramMain();

extern void ProgramCleanup();

// Core emulation functions — called by EmulatorShell.
extern bool InitEmulation();
extern void RunEmulatedTicksToTrueTime();
extern void DoEmulateExtraTime();

// Access the parsed launch config (available after ProgramEarlyInit).
extern const LaunchConfig &GetLaunchConfig();

// Replace the launch config and rebuild derived configs (machine, emulator).
// Used by the model selector to apply user choices before initMachine().
extern void SetLaunchConfig(const LaunchConfig &lc);

// Access the emulator config (available after ProgramEarlyInit).
extern const EmulatorConfig &GetEmulatorConfig();
extern EmulatorConfig &GetEmulatorConfigMut();
