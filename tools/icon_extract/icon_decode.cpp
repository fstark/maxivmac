/*
	Icon decoding — converts Macintosh icon resources to RGBA format

	Provides functions to decode Macintosh icon formats (icl8, icn#) into
	standard RGBA pixel arrays for display or conversion.
*/

#include "icon_decode.h"
#include "mac_palette.h"

namespace rsrc {

// Decode an icl8 icon resource to RGBA format
IconRGBA DecodeIcl8(
    std::span<const uint8_t, 1024> icl8,
    std::optional<std::span<const uint8_t, 128>> mask)
{
    IconRGBA icon{};

    // Convert palette-indexed pixels to RGBA with optional mask
    for (int row = 0; row < 32; ++row) {
        for (int col = 0; col < 32; ++col) {
            uint8_t paletteIdx = icl8[row * 32 + col];
            uint32_t rgba = kMacSystemPalette[paletteIdx];

            // Apply mask if provided (1-bit mask, 1=opaque, 0=transparent)
            uint8_t alpha = 255;
            if (mask) {
                int byteIdx = row * 4 + (col / 8);
                int bitIdx = 7 - (col % 8);
                alpha = (((*mask)[byteIdx] >> bitIdx) & 1) ? 255 : 0;
            }

            // Store RGBA (row-major, 4 bytes per pixel)
            size_t px = static_cast<size_t>((row * 32 + col) * 4);
            icon.pixels[px + 0] = static_cast<uint8_t>((rgba >> 24) & 0xFF);
            icon.pixels[px + 1] = static_cast<uint8_t>((rgba >> 16) & 0xFF);
            icon.pixels[px + 2] = static_cast<uint8_t>((rgba >> 8) & 0xFF);
            icon.pixels[px + 3] = alpha;
        }
    }
    return icon;
}

} // namespace rsrc
