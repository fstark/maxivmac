/*
	host_pasteboard.cpp — Unified host clipboard implementation.

	Merges the former clipboard.cpp, clipboard_image.cpp, and
	host_pasteboard.cpp into a single HostPasteboard class that
	owns all SDL clipboard interaction.
*/

#include "platform/host_pasteboard.h"
#include "core/emulation_config.h"
#include "platform/common/osglu_common.h"
#include "platform/common/param_buffers.h"
#include "core/diag.h"
#include "util/macroman.h"

#include <SDL3/SDL.h>
#include "stb_image.h"

#include <cstring>

/* ── Singleton ───────────────────────────────────────── */

static HostPasteboard s_pasteboard;

HostPasteboard &GetHostPasteboard()
{
	return s_pasteboard;
}

/* ── SDL event handler ───────────────────────────────── */

void HostPasteboard::onClipboardUpdate()
{
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
	std::lock_guard lock(mu_);

	if (newText == text_ && newPng == png_)
	{
		DIAG(CLIP, "SDL clipboard update: identical content, skipped\n");
		return;
	}

	uint32_t oldSeq = seq_;
	text_ = std::move(newText);
	png_ = std::move(newPng);
	imgW_ = newW;
	imgH_ = newH;
	seq_++;

	DIAG(CLIP, "SDL clipboard update: text=%zuB png=%zuB → seq %u→%u\n", text_.size(), png_.size(),
		 oldSeq, seq_);
}

/* ── Text ────────────────────────────────────────────── */

bool HostPasteboard::hasText() const
{
	std::lock_guard lock(mu_);
	return !text_.empty();
}

std::string HostPasteboard::getTextMacRoman() const
{
	std::lock_guard lock(mu_);
	return text_;
}

void HostPasteboard::setText(const uint8_t *macRoman, uint32_t len)
{
	std::string utf8 = UTF8FromMacRoman({macRoman, len});
	SDL_SetClipboardText(utf8.c_str());
}

/* ── Images ──────────────────────────────────────────── */

bool HostPasteboard::hasImage(int *width, int *height) const
{
	std::lock_guard lock(mu_);
	if (png_.empty()) return false;
	if (width) *width = imgW_;
	if (height) *height = imgH_;
	return true;
}

std::vector<uint8_t> HostPasteboard::getImageRGBA(int *width, int *height) const
{
	std::vector<uint8_t> pngCopy;
	{
		std::lock_guard lock(mu_);
		if (png_.empty()) return {};
		pngCopy = png_;
	}

	int w, h, comp;
	uint8_t *pixels =
		stbi_load_from_memory(pngCopy.data(), static_cast<int>(pngCopy.size()), &w, &h, &comp, 4);
	if (!pixels) return {};

	if (width) *width = w;
	if (height) *height = h;
	std::vector<uint8_t> result(pixels, pixels + static_cast<size_t>(w) * h * 4);
	stbi_image_free(pixels);
	return result;
}

static std::vector<uint8_t> s_clipBuffer;

static const void *ClipDataCallback(void * /*userdata*/, const char *mime, size_t *size)
{
	if (std::strcmp(mime, "image/png") == 0)
	{
		*size = s_clipBuffer.size();
		return s_clipBuffer.data();
	}
	*size = 0;
	return nullptr;
}

void HostPasteboard::setImage(const uint8_t *pngData, size_t len)
{
	s_clipBuffer.assign(pngData, pngData + len);
	static const char *mimes[] = {"image/png"};
	SDL_SetClipboardData(ClipDataCallback, nullptr, nullptr, mimes, 1);
}

/* ── Change tracking ─────────────────────────────────── */

uint32_t HostPasteboard::seq() const
{
	std::lock_guard lock(mu_);
	return seq_;
}

/* ── Legacy param-buffer interface ───────────────────── */

tMacErr HostPasteboard::legacyExport(PbufIndex i)
{
	uint8_t *s = static_cast<uint8_t *>(g_pbufDat[i]);
	uint32_t L = g_pbufSize[i];
	setText(s, L);
	return tMacErr::noErr;
}

tMacErr HostPasteboard::legacyImport(PbufIndex *r)
{
	std::string mr = getTextMacRoman();

	PbufIndex t;
	tMacErr err = PbufNew(static_cast<uint32_t>(mr.size()), &t);
	if (err != tMacErr::noErr) return err;

	std::memcpy(g_pbufDat[t], mr.data(), mr.size());
	*r = t;
	return tMacErr::noErr;
}
