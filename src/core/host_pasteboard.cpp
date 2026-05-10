#include "core/host_pasteboard.h"
#include "core/diag.h"
#include "util/macroman.h"

#include <cstring>

#ifdef HAVE_SDL
#include <SDL3/SDL.h>
#endif

#include "stb_image.h"

static HostPasteboard s_pasteboard;

HostPasteboard &GetHostPasteboard()
{
	return s_pasteboard;
}

void HostPasteboardOnClipboardUpdate()
{
#ifdef HAVE_SDL
	/* Read from SDL — outside the lock (may block on IPC) */
	char *utf8 = SDL_GetClipboardText();
	std::string newText;
	if (utf8)
	{
		newText = MacRomanFromUTF8(utf8);
		SDL_free(utf8);
	}

	size_t pngLen = 0;
	void *pngRaw = SDL_GetClipboardData("image/png", &pngLen);
	std::vector<uint8_t> newPng;
	int newW = 0, newH = 0;
	if (pngRaw && pngLen > 0)
	{
		newPng.assign(static_cast<uint8_t *>(pngRaw), static_cast<uint8_t *>(pngRaw) + pngLen);
		SDL_free(pngRaw);

		int comp = 0;
		stbi_info_from_memory(newPng.data(), static_cast<int>(newPng.size()), &newW, &newH, &comp);
	}

	/* Compare and update under the lock */
	std::lock_guard lock(s_pasteboard.mu);

	if (newText == s_pasteboard.text && newPng == s_pasteboard.png)
	{
		DIAG(CLIP, "SDL clipboard update: identical content, skipped\n");
		return;
	}

	uint32_t oldSeq = s_pasteboard.seq;
	s_pasteboard.text = std::move(newText);
	s_pasteboard.png = std::move(newPng);
	s_pasteboard.imgW = newW;
	s_pasteboard.imgH = newH;
	s_pasteboard.seq++;

	DIAG(CLIP, "SDL clipboard update: text=%zuB png=%zuB → seq %u→%u\n", s_pasteboard.text.size(),
		 s_pasteboard.png.size(), oldSeq, s_pasteboard.seq);
#endif
}
