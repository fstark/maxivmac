#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace icns {

// A decoded icon: RGBA pixels at a given size.
struct DecodedIcon {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba; // width * height * 4 bytes
};

// Parse an icns container and extract the largest icon as RGBA.
// Input can be a standalone .icns file or the data of an 'icns' resource.
// Returns empty DecodedIcon (width==0) on failure.
DecodedIcon ExtractLargest(std::span<const uint8_t> data);

} // namespace icns
