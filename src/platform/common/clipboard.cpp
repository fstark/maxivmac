#include "platform/common/osglu_ui.h"
#include "platform/common/osglu_ud.h"
#include "platform/common/osglu_common.h"
#include "platform/platform.h"
#include "platform/common/param_buffers.h"
#include "platform/common/mac_roman.h"
#include "platform/common/clipboard.h"

#include <cstdlib>
#include <string>

#ifdef HAVE_SDL
#include <SDL3/SDL.h>
#endif

tMacErr HTCEexport(PbufIndex i)
{
#ifdef HAVE_SDL
	tMacErr err;
	char *p;
	uint8_t * s = static_cast<uint8_t *>(PbufDat[i]);
	uint32_t L = PbufSize[i];
	uint32_t sz = MacRoman2UniCodeSize(s, L);

	if (nullptr == (p = static_cast<char *>(malloc(sz + 1)))) {
		err = tMacErr::miscErr;
	} else {
		MacRoman2UniCodeData(s, L, p);
		p[sz] = 0;

		if (0 != SDL_SetClipboardText(p)) {
			err = tMacErr::miscErr;
		} else {
			err = tMacErr::noErr;
		}
		free(p);
	}

	return err;
#else
	(void)i;
	return tMacErr::miscErr;
#endif
}

tMacErr HTCEimport(PbufIndex *r)
{
#ifdef HAVE_SDL
	tMacErr err;
	uint32_t L;
	char *s = nullptr;
	PbufIndex t = NOT_A_PBUF;

	if (nullptr == (s = SDL_GetClipboardText())) {
		err = tMacErr::miscErr;
	} else
	if (tMacErr::noErr != (err =
		UniCodeStrLength(s, &L)))
	{
		/* fail */
	} else
	if (tMacErr::noErr != (err =
		PbufNew(L, &t)))
	{
		/* fail */
	} else
	{
		err = tMacErr::noErr;

		UniCodeStr2MacRoman(s, static_cast<char *>(PbufDat[t]));
		*r = t;
		t = NOT_A_PBUF;
	}

	if (NOT_A_PBUF != t) {
		PbufDispose(t);
	}
	if (nullptr != s) {
		SDL_free(s);
	}

	return err;
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
	uint32_t len;
	if (UniCodeStrLength(utf8, &len) != tMacErr::noErr) {
		SDL_free(utf8);
		return {};
	}
	std::string result(len, '\0');
	UniCodeStr2MacRoman(utf8, result.data());
	SDL_free(utf8);
	return result;
#else
	return {};
#endif
}

void hostClipSetText(const uint8_t *macRoman, uint32_t len)
{
#ifdef HAVE_SDL
	uint32_t sz = MacRoman2UniCodeSize(const_cast<uint8_t *>(macRoman), len);
	std::string utf8(sz + 1, '\0');
	MacRoman2UniCodeData(const_cast<uint8_t *>(macRoman), len, utf8.data());
	utf8[sz] = '\0';
	SDL_SetClipboardText(utf8.c_str());
#else
	(void)macRoman;
	(void)len;
#endif
}
