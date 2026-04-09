/*
	config_loader.h

	Command-line argument parsing for runtime model/g_rom/g_ram/screen
	configuration. Part of Phase 5 multi-model support.
*/

#pragma once

#include "core/machine_config.h"
#include "core/emulator_config.h"
#include <string>
#include <vector>

struct LaunchConfig {
	MacModel model      = MacModel::II;
	bool     modelExplicit = false; // true only if --model was passed
	std::string romPath;
	std::vector<std::string> diskPaths;
	uint32_t ramMB      = 0;   // 0 = use model default
	uint16_t screenW    = 0;   // 0 = use model default
	uint16_t screenH    = 0;
	uint8_t  screenDepth = 0;  // log2 bpp
	int      speed      = 0;   // 0 = model default
	int      scale      = 0;   // 0 = use default (2)
	uint32_t logStart   = 0;   // first instruction to log (0 = no logging)
	uint32_t logCount   = 0;   // how many instructions to log (0 = no logging)
	bool     fullscreen = false;
	bool     silent     = false;
	bool     help       = false;
	std::string title;   // --title: window title (platform-specific)
	std::string romDir;  // --romdir: directory to search for ROM files

	// StateRecorder options
	std::string recordPath;       // --record=<path>
	std::string verifyPath;       // --verify=<path>
	std::string tracePath;        // --trace=<path> (CPU+IO text)
	std::string traceCpuPath;     // --trace-cpu=<path> (CPU-only text)
	uint32_t    snapshotInterval = 0; // --snapshot-interval=N (0=default 100K)
	uint64_t    maxInstructions  = 0; // --max-instructions=N (0=default 20M)
};

// Parse command-line arguments into a LaunchConfig.
LaunchConfig ParseCommandLine(int argc, char* argv[]);

// Convert LaunchConfig → MachineConfig (applies overrides on top of model defaults).
MachineConfig BuildMachineConfig(const LaunchConfig& launch);

// Convert LaunchConfig → EmulatorConfig (presentation/UX settings).
EmulatorConfig BuildEmulatorConfig(const LaunchConfig& launch);

// Print usage/help to stderr.
void PrintUsage(const char* progname);

// Parse a model name string (case-insensitive) to MacModel enum.
// Returns true on success.
bool ParseModelName(const std::string& name, MacModel& out);

// Return the canonical string name for a model (e.g. "MacPlus").
// This is also the ROM base name: DefaultRomFileName = ModelToString + ".ROM".
const char* ModelToString(MacModel model);

// Return the default ROM filename for a model (e.g. "MacPlus.ROM").
const char* DefaultRomFileName(MacModel model);

// Resolve ROM path: if romPath is set, return it; otherwise search
// CWD, romDir (if non-empty), and CWD/roms/ for the model's default ROM filename.
// Returns empty string if not found.
std::string ResolveRomPath(const std::string& romPath, MacModel model, const std::string& romDir = {});
