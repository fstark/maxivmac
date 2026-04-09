#include "dbglog_platform.h"


#include <cstdio>
#include <cstring>

#ifdef HAVE_SDL
#include <SDL3/SDL.h>
#endif

#include "path_utils.h"

#ifndef dbglog_ToStdErr
#define dbglog_ToStdErr 0
#endif
#ifndef dbglog_ToSDL_Log
#define dbglog_ToSDL_Log 0
#endif

#if ! dbglog_ToStdErr
static FILE *dbglog_File = nullptr;
#endif

bool dbglog_open0(const char *appParent)
{
#if dbglog_ToStdErr || dbglog_ToSDL_Log
	return true;
#else
	if (nullptr == appParent)
	{
		dbglog_File = fopen("dbglog.txt", "w");
	}
	else {
		char *t = nullptr;

		if (tMacErr::noErr == ChildPath(const_cast<char*>(appParent), "dbglog.txt", &t)) {
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

