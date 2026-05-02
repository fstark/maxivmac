/*
	imgui_main.cpp — entry point for ImGui backend

	Two-phase init:
	- Without --model: platform init only → model selector UI
	- With --model: full init → emulation starts immediately
*/

#include "platform/imgui_backend.h"
#include "platform/emulator_shell.h"
#include "config/mac_file.h"
#include "core/config_loader.h"
#include "core/main.h"

int main(int argc, char **argv)
{
	ProgramEarlyInit(argc, argv);
	const LaunchConfig &lc = GetLaunchConfig();
	if (lc.help) return 0;

	ImGuiBackend backend;
	EmulatorShell shell(&backend);

	if (lc.modelExplicit)
	{
		/* Model specified on command line — full init, boot directly */
		if (!shell.init(argc, argv))
		{
			shell.shutdown();
			ProgramCleanup();
			return 1;
		}
		backend.setUIState(UIState::Windowed);
	}
	else if (!lc.macFilePath.empty())
	{
		/* Direct .mac file launch — parse, validate, boot */
		MacFileEntry entry;
		std::string err;
		if (!ParseMacFile(lc.macFilePath, entry, err))
		{
			fprintf(stderr, "%s\n", err.c_str());
			ProgramCleanup();
			return 1;
		}
		std::string dataDir = ResolveDataDir("");
		ValidateMacEntry(entry, dataDir + "/roms", dataDir + "/disks");
		if (!entry.romAvailable)
		{
			fprintf(stderr, "%s: %s\n", lc.macFilePath.c_str(), entry.validationError.c_str());
			ProgramCleanup();
			return 1;
		}
		LaunchConfig macLc = LaunchConfigFromMacEntry(entry, dataDir);
		SetLaunchConfig(macLc);
		if (!shell.init(argc, argv))
		{
			shell.shutdown();
			ProgramCleanup();
			return 1;
		}
		backend.setUIState(UIState::Windowed);
	}
	else
	{
		/* No model — platform init only, show Launcher */
		if (!shell.initPlatform(argc, argv))
		{
			shell.shutdown();
			ProgramCleanup();
			return 1;
		}

		std::string dataDir = ResolveDataDir("");
		if (dataDir.empty())
		{
			fprintf(stderr, "Error: data/ directory not found\n");
			shell.shutdown();
			ProgramCleanup();
			return 1;
		}

		auto entries = ScanMacDirectory(dataDir + "/macs");
		std::string romDir = dataDir + "/roms";
		std::string diskDir = dataDir + "/disks";
		for (auto &e : entries)
			ValidateMacEntry(e, romDir, diskDir);

		if (!backend.createLauncher(std::move(entries)))
		{
			shell.shutdown();
			ProgramCleanup();
			return 1;
		}
		backend.setUIState(UIState::Launcher);
	}

	backend.runLoop();

	shell.shutdown();
	ProgramCleanup();
	return 0;
}
