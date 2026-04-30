/*
	clipboard_image.cpp — Copy PNG data to the system clipboard via SDL3
*/

#include "platform/clipboard_image.h"
#include <SDL3/SDL.h>
#include <cstring>
#include <vector>

static std::vector<uint8_t> s_clipBuffer;

static const void *clipDataCallback(void *userdata, const char *mime, size_t *size)
{
	(void)userdata;
	if (std::strcmp(mime, "image/png") == 0)
	{
		*size = s_clipBuffer.size();
		return s_clipBuffer.data();
	}
	*size = 0;
	return nullptr;
}

void HostClipSetImage(const uint8_t *pngData, size_t len)
{
	s_clipBuffer.assign(pngData, pngData + len);
	static const char *mimes[] = {"image/png"};
	SDL_SetClipboardData(clipDataCallback, nullptr, nullptr, mimes, 1);
}
