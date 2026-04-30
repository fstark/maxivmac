/*
	imgui_main.cpp — entry point for ImGui backend

	Two-phase init:
	- Without --model: platform init only → model selector UI
	- With --model: full init → emulation starts immediately
*/

#include "platform/imgui_backend.h"
#include "platform/emulator_shell.h"
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
	else
	{
		/* No model — platform init only, show model selector */
		if (!shell.initPlatform(argc, argv))
		{
			shell.shutdown();
			ProgramCleanup();
			return 1;
		}
		if (!backend.createSelectorWindow())
		{
			shell.shutdown();
			ProgramCleanup();
			return 1;
		}
		backend.setUIState(UIState::ModelSelector);
	}

	backend.runLoop();

	shell.shutdown();
	ProgramCleanup();
	return 0;
}
