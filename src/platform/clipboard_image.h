/*
	clipboard_image.h — Copy PNG data to the system clipboard via SDL3
*/

#pragma once
#include <cstddef>
#include <cstdint>

void HostClipSetImage(const uint8_t *pngData, size_t len);
