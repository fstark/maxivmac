/*
	imgui_main.cpp — entry point for ImGui backend

	Same structure as app_main.cpp but creates an ImGuiBackend
	instead of SdlBackend.
*/

#include "platform/imgui_backend.h"
#include "platform/emulator_shell.h"
#include "core/main.h"

int main(int argc, char** argv)
{
	ProgramEarlyInit(argc, argv);
	const LaunchConfig& lc = GetLaunchConfig();
	if (lc.help) return 0;

	ImGuiBackend backend;
	EmulatorShell shell(&backend);

	if (!shell.init(argc, argv)) {
		shell.shutdown();
		ProgramCleanup();
		return 1;
	}

	backend.runLoop();

	shell.shutdown();
	ProgramCleanup();
	return 0;
}
