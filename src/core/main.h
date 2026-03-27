#pragma once

#include "core/config_loader.h"

extern void ProgramEarlyInit(int argc, char* argv[]);
extern bool EmulationReserveAlloc();
extern void EmulationFreeAlloc();
extern void ProgramMain();
extern void ProgramCleanup();

// Access the parsed launch config (available after ProgramEarlyInit).
extern const LaunchConfig& GetLaunchConfig();

// Access the emulator config (available after ProgramEarlyInit).
extern const EmulatorConfig& GetEmulatorConfig();
extern EmulatorConfig& GetEmulatorConfigMut();
