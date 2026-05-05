/*
	mac_file.h

	.mac file parser, directory scanner, and validator.
	Part of the Model & Macintosh feature.
*/

#pragma once

#include "core/machine_config.h"
#include <string>
#include <string_view>
#include <vector>

struct MacFileEntry
{
	// Identity
	std::string name;
	std::string description;
	std::string filePath; // absolute path to the .mac file

	// Model
	MacModel model = MacModel::II;

	// Disks (first = boot disk)
	std::vector<std::string> disks;

	// Shared drives
	std::vector<std::string> sharedDirs;

	// Serial
	std::string serialA;

	// Icon (optional PNG path, resolved after validation)
	std::string iconPath;

	// Overrides (0 = use model default)
	uint32_t ramOverrideMB = 0;
	uint16_t screenW = 0;
	uint16_t screenH = 0;
	uint8_t screenDepth = 0;

	// Validation status (populated by ValidateMacEntry)
	bool romAvailable = false;
	std::string romPath;
	bool allDisksAvailable = false;
	std::string validationError;
};

// Parse a single .mac file. Returns true on parse success.
// On parse failure, sets errorOut.
bool ParseMacFile(std::string_view path, MacFileEntry &out, std::string &errorOut);

// Parse from a string buffer (for testing). path is informational only.
bool ParseMacFileFromString(std::string_view content, std::string_view path, MacFileEntry &out,
							std::string &errorOut);

// Scan a directory for .mac files, parse each, return all entries.
std::vector<MacFileEntry> ScanMacDirectory(std::string_view dirPath);

// Validate ROM and disk availability for a single entry.
void ValidateMacEntry(MacFileEntry &entry, std::string_view dataDir);

// Resolve a path from a .mac file value.
//   @path  → dataDir/path   (@ = data directory root)
//   /abs   → /abs           (absolute, used as-is)
//   rel    → rel            (CWD-relative, used as-is)
std::string ResolveMacPath(std::string_view dataDir, std::string_view raw);

// Resolve the data/ directory. Searches:
//   1. <appParent>/data/
//   2. CWD/data/
// Returns empty string if not found.
std::string ResolveDataDir(std::string_view appParent);
