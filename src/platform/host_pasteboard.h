/*
	host_pasteboard.h — Unified host clipboard interface.

	Single class that owns all host ↔ guest clipboard state.
	Updated by the SDL event handler on the main thread;
	read by clipboard commands on the emu thread.
*/

#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "platform/platform.h"

class HostPasteboard
{
public:
	/*
		Called from the SDL event loop on SDL_EVENT_CLIPBOARD_UPDATE.
		Reads the system clipboard, compares with the snapshot,
		and increments seq if content changed.
	*/
	void onClipboardUpdate();

	/* ── Text ──────────────────────────────────────── */

	bool hasText() const;
	std::string getTextMacRoman() const;
	void setText(const uint8_t *macRoman, uint32_t len);

	/* ── Images ────────────────────────────────────── */

	bool hasImage(int *width, int *height) const;
	std::vector<uint8_t> getImageRGBA(int *width, int *height) const;
	void setImage(const uint8_t *pngData, size_t len);

	/* ── Change tracking ───────────────────────────── */

	uint32_t seq() const;
	/* ── Legacy param-buffer interface (machine.cpp) ── */

	tMacErr legacyExport(PbufIndex i);
	tMacErr legacyImport(PbufIndex *r);

private:
	mutable std::mutex mu_;
	std::string text_;		   // MacRoman-encoded (empty = none)
	std::vector<uint8_t> png_; // PNG-encoded image data (empty = none)
	int imgW_ = 0;
	int imgH_ = 0;
	uint32_t seq_ = 0;
};

HostPasteboard &GetHostPasteboard();
