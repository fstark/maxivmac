#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

/*
	Single source of truth for the host clipboard.
	Updated by the SDL event handler (main thread).
	Read by clipboard commands on the emu thread.
	All access must hold mu.
*/
struct HostPasteboard
{
	std::string text;		  // MacRoman-encoded text (empty = none)
	std::vector<uint8_t> png; // PNG-encoded image data (empty = none)
	int imgW = 0;
	int imgH = 0;
	uint32_t seq = 0;
	std::mutex mu;
};

/* Global pasteboard instance. */
HostPasteboard &GetHostPasteboard();

/*
	Called from the SDL event loop on SDL_EVENT_CLIPBOARD_UPDATE.
	Reads the current system clipboard, compares with the snapshot,
	and increments seq if content changed.
*/
void HostPasteboardOnClipboardUpdate();
