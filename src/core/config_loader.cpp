/*
	config_loader.cpp

	Command-line argument parsing and MachineConfig construction.
*/

#include "core/config_loader.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
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

	// Canonical names (= ROM base names, case-insensitive)
	if (lower == "twig43")                           { out = MacModel::Twig43; return true; }
	if (lower == "twiggy")                           { out = MacModel::Twiggy; return true; }
	if (lower == "mac128k")                          { out = MacModel::Mac128K; return true; }
	if (lower == "mac512ke")                         { out = MacModel::Mac512Ke; return true; }
	if (lower == "macpluskanji")                     { out = MacModel::Kanji; return true; }
	if (lower == "macplus")                          { out = MacModel::Plus; return true; }
	if (lower == "macse")                            { out = MacModel::SE; return true; }
	if (lower == "sefdhd")                           { out = MacModel::SEFDHD; return true; }
	if (lower == "classic")                          { out = MacModel::Classic; return true; }
	if (lower == "pb100")                            { out = MacModel::PB100; return true; }
	if (lower == "macii")                            { out = MacModel::II; return true; }
	if (lower == "maciix")                           { out = MacModel::IIx; return true; }

	// Legacy aliases
	if (lower == "plus")                             { out = MacModel::Plus; return true; }
	if (lower == "se")                               { out = MacModel::SE; return true; }
	if (lower == "ii")                               { out = MacModel::II; return true; }
	if (lower == "iix")                              { out = MacModel::IIx; return true; }
	if (lower == "128k")                             { out = MacModel::Mac128K; return true; }
	if (lower == "512ke")                            { out = MacModel::Mac512Ke; return true; }
	if (lower == "kanji")                            { out = MacModel::Kanji; return true; }
	if (lower == "powerbook100")                     { out = MacModel::PB100; return true; }

	return false;
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

void PrintUsage(const char* progname)
{
	fprintf(stderr,
		"Usage: %s [options] [disk1.img] [disk2.img] ...\n"
		"\n"
		"Options:\n"
		"  --model=MODEL    Mac model (= ROM base name): MacPlus, MacSE, MacII, MacIIx,\n"
		"                   Classic, PB100, SEFDHD, Mac128K, Mac512Ke, MacPlusKanji,\n"
		"                   Twig43, Twiggy  (default: MacII)\n"
		"  --rom=PATH       Path to ROM file (auto-detected from model if omitted)\n"
		"  --romdir=DIR     Directory to search for ROM files\n"
		"  --ram=SIZE       RAM size: 1M, 2M, 4M, 8M (default: model-specific)\n"
		"  --screen=WxHxD   Screen size: 512x342x1, 640x480x8, etc.\n"
		"  --speed=N        Emulation speed: 1 (1x), 2, 4, 8, 0 (all-out)\n"
		"  --scale=N        Window scale factor (default: 2)\n"
		"  --fullscreen     Start in fullscreen mode\n"
		"  --silent         Disable audio output\n"
		"  --title=TEXT     Window title\n"
		"  --record=PATH    Record golden file for non-regression testing\n"
		"  --verify=PATH    Verify against golden file (exit 0=pass, 1=fail)\n"
		"  --trace=PATH     Write CPU+IO text trace to file\n"
		"  --trace-cpu=PATH Write CPU-only text trace to file\n"
		"  --snapshot-interval=N  Instructions between snapshots (default: 100000)\n"
		"  --max-instructions=N   Instruction budget (default: 20000000)\n"
		"  -h, --help       Show this help\n"
		"\n"
		"ROM auto-detection searches: ./<MODEL>.ROM, <romdir>/<MODEL>.ROM, roms/<MODEL>.ROM\n"
		"\n"
		"Examples:\n"
		"  %s --model=MacII system7.img\n"
		"  %s --model=MacPlus disk.img\n",
		progname, progname, progname);
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
		if (strcmp(arg, "--silent") == 0) {
			lc.silent = true;
			continue;
		}

		// --key=value style arguments
		if (strncmp(arg, "--model=", 8) == 0) {
			MacModel m;
			if (ParseModelName(arg + 8, m)) {
				lc.model = m;
			} else {
				fprintf(stderr, "Warning: unknown model '%s', using default (MacII)\n", arg + 8);
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
		if (strncmp(arg, "--scale=", 8) == 0) {
			lc.scale = atoi(arg + 8);
			continue;
		}
		if (strncmp(arg, "--log-start=", 12) == 0) {
			lc.logStart = (uint32_t)strtoul(arg + 12, nullptr, 10);
			continue;
		}
		if (strncmp(arg, "--log-count=", 12) == 0) {
			lc.logCount = (uint32_t)strtoul(arg + 12, nullptr, 10);
			continue;
		}
		if (strncmp(arg, "--record=", 9) == 0) {
			lc.recordPath = arg + 9;
			continue;
		}
		if (strncmp(arg, "--verify=", 9) == 0) {
			lc.verifyPath = arg + 9;
			continue;
		}
		if (strncmp(arg, "--trace=", 8) == 0) {
			lc.tracePath = arg + 8;
			continue;
		}
		if (strncmp(arg, "--trace-cpu=", 12) == 0) {
			lc.traceCpuPath = arg + 12;
			continue;
		}
		if (strncmp(arg, "--snapshot-interval=", 20) == 0) {
			lc.snapshotInterval = (uint32_t)strtoul(arg + 20, nullptr, 10);
			continue;
		}
		if (strncmp(arg, "--max-instructions=", 19) == 0) {
			lc.maxInstructions = (uint64_t)strtoull(arg + 19, nullptr, 10);
			continue;
		}
		if (strncmp(arg, "--title=", 8) == 0) {
			lc.title = arg + 8;
			continue;
		}
		if (strncmp(arg, "--romdir=", 9) == 0) {
			lc.romDir = arg + 9;
			continue;
		}

		// --key value (separate token) style
		if (strcmp(arg, "--model") == 0 && i + 1 < argc) {
			MacModel m;
			if (ParseModelName(argv[++i], m)) {
				lc.model = m;
			} else {
				fprintf(stderr, "Warning: unknown model '%s', using default (MacII)\n", argv[i]);
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
		if (strcmp(arg, "--scale") == 0 && i + 1 < argc) {
			lc.scale = atoi(argv[++i]);
			continue;
		}
		if (strcmp(arg, "--title") == 0 && i + 1 < argc) {
			lc.title = argv[++i];
			continue;
		}
		if (strcmp(arg, "--romdir") == 0 && i + 1 < argc) {
			lc.romDir = argv[++i];
			continue;
		}
		if (strcmp(arg, "--log-start") == 0 && i + 1 < argc) {
			lc.logStart = (uint32_t)strtoul(argv[++i], nullptr, 10);
			continue;
		}
		if (strcmp(arg, "--log-count") == 0 && i + 1 < argc) {
			lc.logCount = (uint32_t)strtoul(argv[++i], nullptr, 10);
			continue;
		}

		// Reject unknown flags
		if (arg[0] == '-') {
			fprintf(stderr, "Error: unknown option '%s'\n\n", arg);
			PrintUsage(argv[0]);
			exit(1);
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

EmulatorConfig BuildEmulatorConfig(const LaunchConfig& launch)
{
	EmulatorConfig ec;
	if (launch.fullscreen) ec.fullscreen = true;
	if (launch.silent) ec.soundEnabled = false;
	if (launch.speed > 0) ec.speed = launch.speed;
	if (launch.scale > 0) ec.windowScale = launch.scale;
	return ec;
}

const char* ModelToString(MacModel model)
{
	switch (model) {
		case MacModel::Twig43:   return "Twig43";
		case MacModel::Twiggy:   return "Twiggy";
		case MacModel::Mac128K:  return "Mac128K";
		case MacModel::Mac512Ke: return "Mac512Ke";
		case MacModel::Kanji:    return "MacPlusKanji";
		case MacModel::Plus:     return "MacPlus";
		case MacModel::SE:       return "MacSE";
		case MacModel::SEFDHD:   return "SEFDHD";
		case MacModel::Classic:  return "Classic";
		case MacModel::PB100:    return "PB100";
		case MacModel::II:       return "MacII";
		case MacModel::IIx:      return "MacIIx";
	}
	return "MacII";
}

const char* DefaultRomFileName(MacModel model)
{
	static char buf[64];
	snprintf(buf, sizeof(buf), "%s.ROM", ModelToString(model));
	return buf;
}

static bool fileExists(const std::string& path)
{
	FILE* f = fopen(path.c_str(), "rb");
	if (f) { fclose(f); return true; }
	return false;
}

std::string ResolveRomPath(const std::string& romPath, MacModel model, const std::string& romDir)
{
	if (!romPath.empty())
		return romPath;

	const char* name = DefaultRomFileName(model);

	// Try CWD/<name>
	std::string p = name;
	if (fileExists(p))
		return p;

	// Try <romDir>/<name>
	if (!romDir.empty()) {
		p = romDir + "/" + name;
		if (fileExists(p))
			return p;
	}

	// Try roms/<name>
	p = std::string("roms/") + name;
	if (fileExists(p))
		return p;

	return {};
}
