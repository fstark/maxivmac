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

#include "stb_image.h"

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

bool HostClipHasImage(int *width, int *height)
{
#ifdef HAVE_SDL
	size_t dataLen = 0;
	void *data = SDL_GetClipboardData("image/png", &dataLen);
	if (!data || dataLen == 0) return false;

	int w, h, comp;
	bool ok = stbi_info_from_memory(static_cast<const uint8_t *>(data), static_cast<int>(dataLen),
									&w, &h, &comp) != 0;
	SDL_free(data);
	if (!ok) return false;

	if (width) *width = w;
	if (height) *height = h;
	return true;
#else
	(void)width;
	(void)height;
	return false;
#endif
}

std::vector<uint8_t> HostClipGetImageRGBA(int *width, int *height)
{
#ifdef HAVE_SDL
	size_t dataLen = 0;
	void *data = SDL_GetClipboardData("image/png", &dataLen);
	if (!data || dataLen == 0) return {};

	int w, h, comp;
	uint8_t *pixels = stbi_load_from_memory(static_cast<const uint8_t *>(data),
											static_cast<int>(dataLen), &w, &h, &comp, 4);
	SDL_free(data);
	if (!pixels) return {};

	*width = w;
	*height = h;
	std::vector<uint8_t> result(pixels, pixels + static_cast<size_t>(w) * h * 4);
	stbi_image_free(pixels);
	return result;
#else
	(void)width;
	(void)height;
	return {};
#endif
}
