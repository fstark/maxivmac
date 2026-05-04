#include "platform/common/osglu_ui.h"
#include "platform/common/osglu_ud.h"
#include "platform/common/osglu_common.h"
#include "platform/platform.h"
#include "platform/common/param_buffers.h"
#include "platform/common/clipboard.h"
#include "util/macroman.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef HAVE_SDL
#include <SDL3/SDL.h>
#endif

tMacErr HTCEexport(PbufIndex i)
{
#ifdef HAVE_SDL
	uint8_t *s = static_cast<uint8_t *>(g_pbufDat[i]);
	uint32_t L = g_pbufSize[i];
	std::string utf8 = UTF8FromMacRoman({s, L});

	if (0 != SDL_SetClipboardText(utf8.c_str())) return tMacErr::miscErr;

	return tMacErr::noErr;
#else
	(void)i;
	return tMacErr::miscErr;
#endif
}

tMacErr HTCEimport(PbufIndex *r)
{
#ifdef HAVE_SDL
	char *s = SDL_GetClipboardText();
	if (!s) return tMacErr::miscErr;

	std::string mr = MacRomanFromUTF8(s);
	SDL_free(s);

	PbufIndex t;
	tMacErr err = PbufNew(static_cast<uint32_t>(mr.size()), &t);
	if (err != tMacErr::noErr) return err;

	std::memcpy(g_pbufDat[t], mr.data(), mr.size());
	*r = t;
	return tMacErr::noErr;
#else
	(void)r;
	return tMacErr::miscErr;
#endif
}

/* --- New register-block clipboard wrappers (SDL-free interface) --- */

bool hostClipHasText()
{
#ifdef HAVE_SDL
	return SDL_HasClipboardText();
#else
	return false;
#endif
}

std::string hostClipGetTextMacRoman()
{
#ifdef HAVE_SDL
	char *utf8 = SDL_GetClipboardText();
	if (!utf8) return {};
	std::string result = MacRomanFromUTF8(utf8);
	SDL_free(utf8);
	return result;
#else
	return {};
#endif
}

void HostClipSetText(const uint8_t *macRoman, uint32_t len)
{
#ifdef HAVE_SDL
	std::string utf8 = UTF8FromMacRoman({macRoman, len});
	SDL_SetClipboardText(utf8.c_str());
#else
	(void)macRoman;
	(void)len;
#endif
}
