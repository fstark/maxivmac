/*
	mac_file.cpp

	.mac file parser, directory scanner, and validator.
*/

#include "config/mac_file.h"
#include "core/config_loader.h"
#include "core/model_defs.h"
#include "core/md5.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

// Trim leading and trailing whitespace
static std::string_view trim(std::string_view s)
{
	while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
		s.remove_prefix(1);
	while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
		s.remove_suffix(1);
	return s;
}

// Parse RAM size: "4M", "8M" etc. Returns MB or 0 on error.
static uint32_t parseRamSpec(std::string_view val, std::string &errorOut)
{
	if (val.empty())
	{
		errorOut = "empty RAM value";
		return 0;
	}

	// Strip trailing whitespace
	val = trim(val);

	char suffix = val.back();
	bool hasMB = (suffix == 'M' || suffix == 'm');
	bool hasKB = (suffix == 'K' || suffix == 'k');

	if (!hasMB && !hasKB)
	{
		errorOut = "RAM size must end with M or K";
		return 0;
	}

	std::string_view digits = val.substr(0, val.size() - 1);
	if (digits.empty())
	{
		errorOut = "RAM size has no numeric value";
		return 0;
	}

	unsigned long n = 0;
	for (char c : digits)
	{
		if (!std::isdigit(static_cast<unsigned char>(c)))
		{
			errorOut = "RAM size contains non-digit";
			return 0;
		}
		n = n * 10 + (c - '0');
	}

	if (hasMB) return static_cast<uint32_t>(n);

	// KB: v1.0 only supports MB granularity
	if (n % 1024 != 0)
	{
		errorOut = "sub-MB RAM sizes not supported (use whole MB)";
		return 0;
	}
	return static_cast<uint32_t>(n / 1024);
}

// Parse screen spec: "640x480x8" → W, H, depth (log2 bpp)
static bool parseScreenSpec(std::string_view val, uint16_t &w, uint16_t &h, uint8_t &depth,
							std::string &errorOut)
{
	unsigned int tw = 0, th = 0, td = 0;
	// manual parse since string_view doesn't work with sscanf easily
	std::string s(val);
	if (sscanf(s.c_str(), "%ux%ux%u", &tw, &th, &td) == 3)
	{
		if (td == 0 || (td & (td - 1)) != 0 || td > 32)
		{
			errorOut = "invalid bit depth: must be 1, 2, 4, 8, 16, or 32";
			return false;
		}
		w = static_cast<uint16_t>(tw);
		h = static_cast<uint16_t>(th);
		uint8_t d = 0;
		unsigned int bits = td;
		while (bits > 1)
		{
			bits >>= 1;
			d++;
		}
		depth = d;
		return true;
	}
	errorOut = "invalid screen spec (expected WxHxD)";
	return false;
}

bool ParseMacFileFromString(std::string_view content, std::string_view path, MacFileEntry &out,
							std::string &errorOut)
{
	out = MacFileEntry{};
	out.filePath = std::string(path);

	std::string contentStr(content);
	std::istringstream stream(contentStr);
	std::string line;
	int lineNum = 0;
	bool hasName = false;
	bool hasModel = false;

	while (std::getline(stream, line))
	{
		lineNum++;

		// Strip comments
		auto hashPos = line.find('#');
		if (hashPos != std::string::npos) line.erase(hashPos);

		auto trimmed = trim(std::string_view(line));
		if (trimmed.empty()) continue;

		// Split on first '='
		auto eqPos = trimmed.find('=');
		if (eqPos == std::string_view::npos)
		{
			errorOut = std::string(path) + ":" + std::to_string(lineNum) + ": line missing '='";
			return false;
		}

		auto key = trim(trimmed.substr(0, eqPos));
		auto val = trim(trimmed.substr(eqPos + 1));

		if (key == "name")
		{
			out.name = std::string(val);
			hasName = true;
		}
		else if (key == "description")
		{
			out.description = std::string(val);
		}
		else if (key == "model")
		{
			if (!ParseModelName(std::string(val), out.model))
			{
				errorOut = std::string(path) + ":" + std::to_string(lineNum) + ": unknown model '" +
						   std::string(val) + "'";
				return false;
			}
			hasModel = true;
		}
		else if (key == "disk")
		{
			out.disks.push_back(std::string(val));
		}
		else if (key == "shared")
		{
			out.sharedDirs.push_back(std::string(val));
		}
		else if (key == "serial-a")
		{
			out.serialA = std::string(val);
		}
		else if (key == "ram")
		{
			out.ramOverrideMB = parseRamSpec(val, errorOut);
			if (!errorOut.empty())
			{
				errorOut = std::string(path) + ":" + std::to_string(lineNum) + ": " + errorOut;
				return false;
			}
		}
		else if (key == "screen")
		{
			if (!parseScreenSpec(val, out.screenW, out.screenH, out.screenDepth, errorOut))
			{
				errorOut = std::string(path) + ":" + std::to_string(lineNum) + ": " + errorOut;
				return false;
			}
		}
		else
		{
			errorOut = std::string(path) + ":" + std::to_string(lineNum) + ": unknown key '" +
					   std::string(key) + "'";
			return false;
		}
	}

	if (!hasName)
	{
		errorOut = std::string(path) + ": missing required field 'name'";
		return false;
	}
	if (!hasModel)
	{
		errorOut = std::string(path) + ": missing required field 'model'";
		return false;
	}

	return true;
}

bool ParseMacFile(std::string_view path, MacFileEntry &out, std::string &errorOut)
{
	std::string pathStr(path);
	std::ifstream f(pathStr);
	if (!f.is_open())
	{
		errorOut = std::string(path) + ": cannot open file";
		return false;
	}

	std::ostringstream ss;
	ss << f.rdbuf();
	return ParseMacFileFromString(ss.str(), path, out, errorOut);
}

std::vector<MacFileEntry> ScanMacDirectory(std::string_view dirPath)
{
	std::vector<MacFileEntry> entries;
	std::error_code ec;

	for (const auto &dirEntry : fs::directory_iterator(std::string(dirPath), ec))
	{
		if (!dirEntry.is_regular_file()) continue;
		if (dirEntry.path().extension() != ".mac") continue;

		MacFileEntry entry;
		std::string err;
		if (ParseMacFile(dirEntry.path().string(), entry, err)) entries.push_back(std::move(entry));
		// else: silently skip (logged via DIAG in production)
	}

	// Sort by name for deterministic order
	std::sort(entries.begin(), entries.end(),
			  [](const MacFileEntry &a, const MacFileEntry &b) { return a.name < b.name; });

	return entries;
}

// Convert 16 raw MD5 bytes to a 32-char hex string
static std::string md5ToHex(const uint8_t digest[16])
{
	char buf[33];
	for (int i = 0; i < 16; i++)
		snprintf(buf + i * 2, 3, "%02x", digest[i]);
	buf[32] = '\0';
	return buf;
}

void ValidateMacEntry(MacFileEntry &entry, std::string_view romDir, std::string_view diskDir)
{
	entry.romAvailable = false;
	entry.allDisksAvailable = true;
	entry.validationError.clear();

	// Look up model's ROM info
	const ModelDef *def = ModelDefFor(entry.model);
	if (!def)
	{
		entry.validationError = "unknown model";
		return;
	}

	// Resolve ROM path
	std::string romPath = std::string(romDir) + "/" + std::string(def->rom.filename);
	if (!fs::exists(romPath))
	{
		entry.validationError = "ROM missing: " + std::string(def->rom.filename);
		return;
	}
	entry.romPath = romPath;

	// Check ROM MD5 if specified
	if (!def->rom.md5.empty())
	{
		uint8_t digest[16];
		if (!md5_file(romPath.c_str(), digest))
		{
			entry.validationError = "ROM file unreadable";
			return;
		}
		std::string hexDigest = md5ToHex(digest);
		if (hexDigest != def->rom.md5)
		{
			entry.validationError = "ROM checksum mismatch";
			return;
		}
	}
	entry.romAvailable = true;

	// Check disks
	for (const auto &disk : entry.disks)
	{
		std::string diskPath = std::string(diskDir) + "/" + disk;
		if (!fs::exists(diskPath))
		{
			entry.allDisksAvailable = false;
			entry.validationError = "disk missing: " + disk;
			return;
		}
	}
}

std::string ResolveDataDir(std::string_view appParent)
{
	// 1. <appParent>/data/
	if (!appParent.empty())
	{
		std::string candidate = std::string(appParent) + "/data";
		if (fs::is_directory(candidate)) return candidate;
	}

	// 2. CWD/data/
	if (fs::is_directory("data")) return "data";

	return {};
}
