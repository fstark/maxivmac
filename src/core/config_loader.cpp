/*
	config_loader.cpp

	Command-line argument parsing and MachineConfig construction.
*/

#include "core/config_loader.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cctype>

static std::string toLower(const std::string& s)
{
	std::string result = s;
	std::transform(result.begin(), result.end(), result.begin(),
		[](unsigned char c){ return std::tolower(c); });
	return result;
}

bool ParseModelName(const std::string& name, MacModel& out)
{
	std::string lower = toLower(name);

	if (lower == "plus" || lower == "macplus")       { out = MacModel::Plus; return true; }
	if (lower == "se")                               { out = MacModel::SE; return true; }
	if (lower == "sefdhd")                           { out = MacModel::SEFDHD; return true; }
	if (lower == "classic")                          { out = MacModel::Classic; return true; }
	if (lower == "pb100" || lower == "powerbook100") { out = MacModel::PB100; return true; }
	if (lower == "ii" || lower == "macii")           { out = MacModel::II; return true; }
	if (lower == "iix" || lower == "maciix")         { out = MacModel::IIx; return true; }
	if (lower == "128k" || lower == "mac128k")       { out = MacModel::Mac128K; return true; }
	if (lower == "512ke" || lower == "mac512ke")     { out = MacModel::Mac512Ke; return true; }
	if (lower == "twig43")                           { out = MacModel::Twig43; return true; }
	if (lower == "twiggy")                           { out = MacModel::Twiggy; return true; }
	if (lower == "kanji")                            { out = MacModel::Kanji; return true; }

	return false;
}

// Extract the value from "--key=value" or nullptr if no '='
static const char* extractValue(const char* arg)
{
	const char* eq = strchr(arg, '=');
	return eq ? eq + 1 : nullptr;
}

// Parse RAM size string like "1M", "2M", "4M", "8M" or just a number in bytes
static uint32_t parseRAMSize(const char* s)
{
	char* end = nullptr;
	unsigned long val = strtoul(s, &end, 10);
	if (end && (*end == 'M' || *end == 'm')) {
		return (uint32_t)(val * 1024 * 1024);
	} else if (end && (*end == 'K' || *end == 'k')) {
		return (uint32_t)(val * 1024);
	}
	// If just a number and it's small, assume MB
	if (val > 0 && val <= 128) {
		return (uint32_t)(val * 1024 * 1024);
	}
	return (uint32_t)val;
}

// Parse screen spec "WxHxD" (e.g. "640x480x8")
static bool parseScreenSpec(const char* s, uint16_t& w, uint16_t& h, uint8_t& d)
{
	unsigned int tw, th, td;
	if (sscanf(s, "%ux%ux%u", &tw, &th, &td) == 3) {
		w = (uint16_t)tw;
		h = (uint16_t)th;
		// Convert bpp to log2: 1→0, 2→1, 4→2, 8→3, 16→4, 32→5
		uint8_t depth = 0;
		unsigned int bits = td;
		while (bits > 1) { bits >>= 1; depth++; }
		d = depth;
		return true;
	}
	if (sscanf(s, "%ux%u", &tw, &th) == 2) {
		w = (uint16_t)tw;
		h = (uint16_t)th;
		d = 0;  // keep model default
		return true;
	}
	return false;
}

LaunchConfig ParseCommandLine(int argc, char* argv[])
{
	LaunchConfig lc;

	for (int i = 1; i < argc; ++i) {
		const char* arg = argv[i];

		if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
			lc.help = true;
			continue;
		}
		if (strcmp(arg, "--fullscreen") == 0) {
			lc.fullscreen = true;
			continue;
		}

		// --key=value style arguments
		if (strncmp(arg, "--model=", 8) == 0) {
			MacModel m;
			if (ParseModelName(arg + 8, m)) {
				lc.model = m;
			} else {
				fprintf(stderr, "Warning: unknown model '%s', using default (II)\n", arg + 8);
			}
			continue;
		}
		if (strncmp(arg, "--rom=", 6) == 0) {
			lc.romPath = arg + 6;
			continue;
		}
		if (strncmp(arg, "--ram=", 6) == 0) {
			uint32_t ramBytes = parseRAMSize(arg + 6);
			lc.ramMB = ramBytes / (1024 * 1024);
			if (lc.ramMB == 0) lc.ramMB = 1;
			continue;
		}
		if (strncmp(arg, "--screen=", 9) == 0) {
			uint16_t w, h;
			uint8_t d;
			if (parseScreenSpec(arg + 9, w, h, d)) {
				lc.screenW = w;
				lc.screenH = h;
				lc.screenDepth = d;
			} else {
				fprintf(stderr, "Warning: invalid screen spec '%s'\n", arg + 9);
			}
			continue;
		}
		if (strncmp(arg, "--speed=", 8) == 0) {
			lc.speed = atoi(arg + 8);
			continue;
		}

		// --key value (separate token) style
		if (strcmp(arg, "--model") == 0 && i + 1 < argc) {
			MacModel m;
			if (ParseModelName(argv[++i], m)) {
				lc.model = m;
			} else {
				fprintf(stderr, "Warning: unknown model '%s', using default (II)\n", argv[i]);
			}
			continue;
		}
		if (strcmp(arg, "--rom") == 0 && i + 1 < argc) {
			lc.romPath = argv[++i];
			continue;
		}
		if (strcmp(arg, "--ram") == 0 && i + 1 < argc) {
			uint32_t ramBytes = parseRAMSize(argv[++i]);
			lc.ramMB = ramBytes / (1024 * 1024);
			if (lc.ramMB == 0) lc.ramMB = 1;
			continue;
		}
		if (strcmp(arg, "--screen") == 0 && i + 1 < argc) {
			uint16_t w, h;
			uint8_t d;
			if (parseScreenSpec(argv[++i], w, h, d)) {
				lc.screenW = w;
				lc.screenH = h;
				lc.screenDepth = d;
			} else {
				fprintf(stderr, "Warning: invalid screen spec '%s'\n", argv[i]);
			}
			continue;
		}
		if (strcmp(arg, "--speed") == 0 && i + 1 < argc) {
			lc.speed = atoi(argv[++i]);
			continue;
		}

		// Short flags for GDB/rom path compat
		if (strcmp(arg, "-r") == 0 && i + 1 < argc) {
			lc.romPath = argv[++i];
			continue;
		}

		// Skip unknown flags
		if (arg[0] == '-') {
			fprintf(stderr, "Warning: unknown option '%s'\n", arg);
			continue;
		}

		// Positional arguments are disk image paths
		lc.diskPaths.push_back(arg);
	}

	return lc;
}

MachineConfig BuildMachineConfig(const LaunchConfig& launch)
{
	MachineConfig config = MachineConfigForModel(launch.model);

	// Apply overrides from command line
	if (launch.ramMB > 0) {
		uint32_t totalRAM = launch.ramMB * 1024 * 1024;
		if (config.ramBSize > 0) {
			// Two-bank model (Mac II): split evenly
			config.ramASize = totalRAM / 2;
			config.ramBSize = totalRAM / 2;
		} else {
			config.ramASize = totalRAM;
		}
	}

	if (launch.screenW > 0 && launch.screenH > 0) {
		config.screenWidth = launch.screenW;
		config.screenHeight = launch.screenH;
	}
	if (launch.screenDepth > 0) {
		config.screenDepth = launch.screenDepth;
	}

	return config;
}

void PrintUsage(const char* progname)
{
	fprintf(stderr,
		"Usage: %s [options] [disk1.img] [disk2.img] ...\n"
		"\n"
		"Options:\n"
		"  --model=MODEL    Mac model: Plus, SE, II, IIx, Classic, PB100, 128K, 512Ke\n"
		"                   (default: II)\n"
		"  --rom=PATH       Path to ROM file\n"
		"  --ram=SIZE       RAM size: 1M, 2M, 4M, 8M (default: model-specific)\n"
		"  --screen=WxHxD   Screen size: 512x342x1, 640x480x8, etc.\n"
		"  --speed=N        Emulation speed: 1 (1x), 2, 4, 8, 0 (all-out)\n"
		"  --fullscreen     Start in fullscreen mode\n"
		"  -r PATH          ROM path (short form)\n"
		"  -h, --help       Show this help\n"
		"\n"
		"Examples:\n"
		"  %s --model=II --rom=MacII.ROM system7.img\n"
		"  %s --model=Plus --rom=vMac.ROM --ram=4M disk.img\n",
		progname, progname, progname);
}
