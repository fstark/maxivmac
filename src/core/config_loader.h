/*
	config_loader.h

	Command-line argument parsing for runtime model/ROM/RAM/screen
	configuration. Part of Phase 5 multi-model support.
*/

#pragma once

#include "core/machine_config.h"
#include <string>
#include <vector>

struct LaunchConfig {
	MacModel model      = MacModel::II;
	std::string romPath;
	std::vector<std::string> diskPaths;
	uint32_t ramMB      = 0;   // 0 = use model default
	uint16_t screenW    = 0;   // 0 = use model default
	uint16_t screenH    = 0;
	uint8_t  screenDepth = 0;  // log2 bpp
	int      speed      = 0;   // 0 = model default
	bool     fullscreen = false;
	bool     help       = false;
};

// Parse command-line arguments into a LaunchConfig.
LaunchConfig ParseCommandLine(int argc, char* argv[]);

// Convert LaunchConfig → MachineConfig (applies overrides on top of model defaults).
MachineConfig BuildMachineConfig(const LaunchConfig& launch);

// Print usage/help to stderr.
void PrintUsage(const char* progname);

// Parse a model name string (case-insensitive) to MacModel enum.
// Returns true on success.
bool ParseModelName(const std::string& name, MacModel& out);
