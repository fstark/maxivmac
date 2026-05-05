#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace rsrc {

struct IconRGBA {
    std::array<uint8_t, 32 * 32 * 4> pixels; // RGBA, row-major
};

// Decode an icl8 resource (1024 bytes, palette-indexed) into RGBA.
// If mask is provided, it's the second 128 bytes of the ICN# resource
// (the 1-bit mask half). Mask bit=1 → opaque, bit=0 → transparent.
// If no mask, all pixels are fully opaque.
IconRGBA DecodeIcl8(
    std::span<const uint8_t, 1024> icl8,
    std::optional<std::span<const uint8_t, 128>> mask);

} // namespace rsrc
