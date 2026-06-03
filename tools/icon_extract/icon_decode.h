/*
	Icon decoding — converts Macintosh icon resources to RGBA format

	Provides functions to decode Macintosh icon formats (icl8, icn#) into
	standard RGBA pixel arrays for display or conversion.
*/

#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace rsrc {

// 32x32 RGBA icon (128 bytes per row, row-major order)
struct IconRGBA {
    std::array<uint8_t, 32 * 32 * 4> pixels;
};

// Decode an icl8 resource (1024 bytes, palette-indexed) into RGBA
// If mask is provided, it's the second 128 bytes of the ICN# resource
// (the 1-bit mask half). Mask bit=1 → opaque, bit=0 → transparent.
// If no mask, all pixels are fully opaque.
IconRGBA DecodeIcl8(
    std::span<const uint8_t, 1024> icl8,
    std::optional<std::span<const uint8_t, 128>> mask);

} // namespace rsrc
