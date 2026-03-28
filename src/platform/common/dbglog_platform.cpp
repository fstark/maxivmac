#include "dbglog_platform.h"

#if dbglog_HAVE

#include <cstdio>
#include <cstring>
#include <SDL3/SDL.h>

#include "path_utils.h"

/* app_parent is owned by sdl.cpp */
extern char *app_parent;

#ifndef dbglog_ToStdErr
#define dbglog_ToStdErr 0
#endif
#ifndef dbglog_ToSDL_Log
#define dbglog_ToSDL_Log 0
#endif

#if ! dbglog_ToStdErr
static FILE *dbglog_File = nullptr;
#endif

bool dbglog_open0()
{
#if dbglog_ToStdErr || dbglog_ToSDL_Log
	return true;
#else
	if (nullptr == app_parent)
	{
		dbglog_File = fopen("dbglog.txt", "w");
	}
	else {
		char *t = nullptr;

		if (tMacErr::noErr == ChildPath(app_parent, "dbglog.txt", &t)) {
			dbglog_File = fopen(t, "w");
		}

		free(t);
	}

	return (nullptr != dbglog_File);
#endif
}

void dbglog_write0(char *s, uint32_t L)
{
#if dbglog_ToStdErr
	(void) fwrite(s, 1, L, stderr);
#elif dbglog_ToSDL_Log
	char t[256 + 1];

	if (L > 256) {
		L = 256;
	}
	(void) memcpy(t, s, L);
	t[L] = 1;

	SDL_Log("%s", t);
#else
	if (dbglog_File != nullptr) {
		(void) fwrite(s, 1, L, dbglog_File);
	}
#endif
}

void dbglog_close0()
{
#if ! dbglog_ToStdErr
	if (dbglog_File != nullptr) {
		fclose(dbglog_File);
		dbglog_File = nullptr;
	}
#endif
}

#endif
