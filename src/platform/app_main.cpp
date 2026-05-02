/*
	app_main.cpp — unified entry point

	Selects ImGui or Headless backend at runtime based on --headless
	flag (or --verify, which implies headless).
*/

#include "platform/imgui_backend.h"
#include "platform/headless_backend.h"
#include "platform/emulator_shell.h"
#include "config/mac_file.h"
#include "core/config_loader.h"
#include "core/main.h"

int main(int argc, char **argv)
{
	ProgramEarlyInit(argc, argv);
	const LaunchConfig &lc = GetLaunchConfig();
	if (lc.help) return 0;

	/* Pick backend based on runtime flags */
	ImGuiBackend imguiBackend;
	HeadlessBackend headlessBackend;
	PlatformBackend &backend = lc.headless ? static_cast<PlatformBackend &>(headlessBackend)
										   : static_cast<PlatformBackend &>(imguiBackend);

	EmulatorShell shell(&backend);

	if (lc.headless)
	{
		/* Headless: require --model, boot directly */
		if (!shell.init(argc, argv))
		{
			shell.shutdown();
			ProgramCleanup();
			return 1;
		}
	}
	else if (lc.modelExplicit)
	{
		/* ImGui with --model: full init, boot directly */
		if (!shell.init(argc, argv))
		{
			shell.shutdown();
			ProgramCleanup();
			return 1;
		}
		imguiBackend.setUIState(UIState::Windowed);
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
		imguiBackend.setUIState(UIState::Windowed);
	}
	else
	{
		/* ImGui without --model: platform init only, show Launcher */
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

		if (!imguiBackend.createLauncher(std::move(entries)))
		{
			shell.shutdown();
			ProgramCleanup();
			return 1;
		}
		imguiBackend.setUIState(UIState::Launcher);
	}

	backend.runLoop();

	shell.shutdown();
	ProgramCleanup();
	return 0;
}
