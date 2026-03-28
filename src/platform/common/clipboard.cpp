#include "platform/common/osglu_ui.h"
#include "platform/common/osglu_ud.h"
#include "platform/common/osglu_common.h"
#include "platform/platform.h"
#include "platform/common/param_buffers.h"
#include "platform/common/mac_roman.h"

#include <SDL3/SDL.h>

#include <cstdlib>

tMacErr HTCEexport(PbufIndex i)
{
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
}

tMacErr HTCEimport(PbufIndex *r)
{
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
}
