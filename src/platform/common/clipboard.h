#pragma once

#include <string>
#include <cstdint>

/*
	SDL-free clipboard interface.
	Implemented in clipboard.cpp (which has SDL access).
	Used by extn_clip.cpp (which must not depend on SDL).
*/

bool hostClipHasText();
std::string hostClipGetTextMacRoman();
void HostClipSetText(const uint8_t *macRoman, uint32_t len);
