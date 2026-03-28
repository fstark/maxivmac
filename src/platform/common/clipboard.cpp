#include "platform/common/osglu_ui.h"
#include "platform/common/osglu_ud.h"
#include "platform/common/osglu_common.h"
#include "platform/platform.h"
#include "platform/common/param_buffers.h"
#include "platform/common/mac_roman.h"

#include <SDL3/SDL.h>

#include <cstdlib>

tMacErr HTCEexport(tPbuf i)
{
	tMacErr err;
	char *p;
	uint8_t * s = static_cast<uint8_t *>(PbufDat[i]);
	uint32_t L = PbufSize[i];
	uint32_t sz = MacRoman2UniCodeSize(s, L);

	if (nullptr == (p = static_cast<char *>(malloc(sz + 1)))) {
		err = mnvm_miscErr;
	} else {
		MacRoman2UniCodeData(s, L, p);
		p[sz] = 0;

		if (0 != SDL_SetClipboardText(p)) {
			err = mnvm_miscErr;
		} else {
			err = mnvm_noErr;
		}
		free(p);
	}

	return err;
}

tMacErr HTCEimport(tPbuf *r)
{
	tMacErr err;
	uint32_t L;
	char *s = nullptr;
	tPbuf t = NotAPbuf;

	if (nullptr == (s = SDL_GetClipboardText())) {
		err = mnvm_miscErr;
	} else
	if (mnvm_noErr != (err =
		UniCodeStrLength(s, &L)))
	{
		/* fail */
	} else
	if (mnvm_noErr != (err =
		PbufNew(L, &t)))
	{
		/* fail */
	} else
	{
		err = mnvm_noErr;

		UniCodeStr2MacRoman(s, static_cast<char *>(PbufDat[t]));
		*r = t;
		t = NotAPbuf;
	}

	if (NotAPbuf != t) {
		PbufDispose(t);
	}
	if (nullptr != s) {
		SDL_free(s);
	}

	return err;
}
