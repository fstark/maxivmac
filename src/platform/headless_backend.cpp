/*
	headless_backend.cpp — headless backend implementation

	Runs emulation ticks as fast as possible with no rendering,
	audio, or event polling. Used for --verify golden-file testing.
*/

#include "platform/headless_backend.h"
#include "platform/emulator_shell.h"

#include <SDL3/SDL.h>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <unistd.h>
#include <sys/param.h>

bool HeadlessBackend::init(EmulatorShell *shell)
{
	shell_ = shell;
	/* Init SDL base so Sound_Init/clipboard calls don't crash. */
	SDL_Init(0);
	return true;
}

void HeadlessBackend::shutdown() {}

void HeadlessBackend::runLoop()
{
	if (!shell_) return;

	while (!shell_->shouldQuit())
	{
		shell_->processSavedTasks();
		if (shell_->shouldQuit()) break;

		if (shell_->tickIsDue() && !shell_->shouldQuit())
		{
			shell_->runOneTick();
		}
	}
}

/* --- Paths --- */

static char s_appParent[MAXPATHLEN] = "";

const char *HeadlessBackend::getAppParent()
{
	if (s_appParent[0] == '\0')
	{
		/* Use current working directory as fallback */
		if (getcwd(s_appParent, sizeof(s_appParent)) == nullptr)
		{
			s_appParent[0] = '.';
			s_appParent[1] = '/';
			s_appParent[2] = '\0';
		}
		else
		{
			size_t len = strlen(s_appParent);
			if (len > 0 && s_appParent[len - 1] != '/')
			{
				s_appParent[len] = '/';
				s_appParent[len + 1] = '\0';
			}
		}
	}
	return s_appParent;
}

char *HeadlessBackend::getPrefDir(const char * /*org*/, const char * /*app*/)
{
	/* headless builds don't need a preference directory */
	return strdup("/tmp/");
}

void HeadlessBackend::freePath(void *path)
{
	free(path);
}
