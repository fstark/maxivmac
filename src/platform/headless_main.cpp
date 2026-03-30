/*
	headless_main.cpp — entry point for headless builds

	Identical structure to app_main.cpp but uses HeadlessBackend.
*/

#include "platform/headless_backend.h"
#include "platform/emulator_shell.h"
#include "core/main.h"

int main(int argc, char** argv)
{
	ProgramEarlyInit(argc, argv);

	const LaunchConfig& lc = GetLaunchConfig();
	if (lc.help) return 0;

	HeadlessBackend backend;
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
