/*
	app_main.cpp — Application entry point

	Thin main() that wires the SdlBackend and EmulatorShell together.
	Replaces the former sdl.cpp.
*/

#include "platform/sdl_backend.h"
#include "platform/emulator_shell.h"
#include "core/main.h"

int main(int argc, char** argv)
{
	ProgramEarlyInit(argc, argv);

	const LaunchConfig& lc = GetLaunchConfig();
	if (lc.help) return 0;

	SdlBackend backend;
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
