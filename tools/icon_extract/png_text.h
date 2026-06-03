/*
	PNG text chunk support — embeds iTXt metadata in PNG files

	Provides functions to write PNG images with optional international text
	chunks (iTXt) as specified in the PNG standard. Used by the icon extraction
	tool to embed metadata in generated PNG files.
*/

#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>

namespace png {

/*
	Represents an iTXt chunk's keyword-value pair.
	Keyword must be 1–79 ASCII bytes; text is UTF-8 encoded.
*/
struct TextChunk {
    std::string_view keyword; // 1–79 bytes, ASCII
    std::string_view text;    // UTF-8 value
};

/*
	Writes a PNG image with optional iTXt text chunks.

	Generates a PNG file at the specified path with the given RGBA pixel data.
	If textChunks is non-empty, inserts iTXt chunks after the IHDR chunk to
	embed metadata (e.g., title, description) in compliance with the PNG
	standard.

	Returns false on write failure or if the generated PNG is malformed.
*/
bool WritePngWithText(
    const std::filesystem::path &path,
    int width, int height,
    std::span<const uint8_t> rgba,
    std::span<const TextChunk> textChunks);

} // namespace png
