#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>

namespace png {

struct TextChunk {
    std::string_view keyword; // 1–79 bytes, ASCII
    std::string_view text;    // UTF-8 value
};

// Write a 32×32 RGBA PNG with optional iTXt text chunks.
bool WritePngWithText(
    const std::filesystem::path &path,
    int width, int height,
    std::span<const uint8_t> rgba,
    std::span<const TextChunk> textChunks);

} // namespace png
