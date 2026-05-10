#pragma once

#include <string>
#include <cstdint>
#include <vector>

/*
	SDL-free clipboard interface.
	Implemented in clipboard.cpp (which has SDL access).
	Used by extn_clip.cpp (which must not depend on SDL).
*/

bool hostClipHasText();
std::string hostClipGetTextMacRoman();
void HostClipSetText(const uint8_t *macRoman, uint32_t len);

/*
	Check if the host clipboard contains an image.
	If yes, sets width and height and returns true.
	width/height may be null if not needed.
*/
bool HostClipHasImage(int *width, int *height);

/*
	Decode the host clipboard image to RGBA pixels.
	Returns empty vector on failure.
	Sets width and height on success.
*/
std::vector<uint8_t> HostClipGetImageRGBA(int *width, int *height);
