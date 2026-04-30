/*
	app_main.cpp — unified entry point

	Selects ImGui or Headless backend at runtime based on --headless
	flag (or --verify, which implies headless).
*/

#include "platform/imgui_backend.h"
#include "platform/headless_backend.h"
#include "platform/emulator_shell.h"
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
	else
	{
		/* ImGui without --model: platform init only, show model selector */
		if (!shell.initPlatform(argc, argv))
		{
			shell.shutdown();
			ProgramCleanup();
			return 1;
		}
		if (!imguiBackend.createSelectorWindow())
		{
			shell.shutdown();
			ProgramCleanup();
			return 1;
		}
		imguiBackend.setUIState(UIState::ModelSelector);
	}

	backend.runLoop();

	shell.shutdown();
	ProgramCleanup();
	return 0;
}
