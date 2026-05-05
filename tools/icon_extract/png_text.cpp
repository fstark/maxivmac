#include "png_text.h"
#include <stb_image_write.h>
#include <cstring>
#include <fstream>
#include <vector>

namespace png {
namespace {

// CRC-32 lookup table (PNG/zlib polynomial 0xEDB88320)
constexpr std::array<uint32_t, 256> BuildCrcTable()
{
    std::array<uint32_t, 256> table{};
    for (uint32_t n = 0; n < 256; ++n) {
        uint32_t c = n;
        for (int k = 0; k < 8; ++k) {
            if (c & 1)
                c = 0xEDB88320u ^ (c >> 1);
            else
                c >>= 1;
        }
        table[n] = c;
    }
    return table;
}

inline constexpr auto kCrcTable = BuildCrcTable();

uint32_t Crc32(const uint8_t *buf, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i)
        crc = kCrcTable[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

void AppendBE32(std::vector<uint8_t> &out, uint32_t v)
{
    out.push_back(static_cast<uint8_t>(v >> 24));
    out.push_back(static_cast<uint8_t>(v >> 16));
    out.push_back(static_cast<uint8_t>(v >> 8));
    out.push_back(static_cast<uint8_t>(v));
}

// Build a single iTXt chunk (uncompressed, UTF-8).
std::vector<uint8_t> BuildITxtChunk(std::string_view keyword,
                                    std::string_view text)
{
    std::vector<uint8_t> chunk;

    // 4-byte length placeholder (filled at end)
    size_t lengthPos = chunk.size();
    chunk.resize(chunk.size() + 4);

    // Chunk type: "iTXt"
    size_t typePos = chunk.size();
    chunk.push_back('i');
    chunk.push_back('T');
    chunk.push_back('X');
    chunk.push_back('t');

    // Keyword + null separator
    chunk.insert(chunk.end(), keyword.begin(), keyword.end());
    chunk.push_back(0);

    // Compression flag = 0, compression method = 0
    chunk.push_back(0);
    chunk.push_back(0);

    // Language tag (empty) + null
    chunk.push_back(0);

    // Translated keyword (empty) + null
    chunk.push_back(0);

    // UTF-8 text (no null terminator needed)
    chunk.insert(chunk.end(), text.begin(), text.end());

    // Fill in data length (everything after length+type, before CRC)
    uint32_t dataLen = static_cast<uint32_t>(chunk.size() - lengthPos - 4 - 4);
    chunk[lengthPos + 0] = static_cast<uint8_t>(dataLen >> 24);
    chunk[lengthPos + 1] = static_cast<uint8_t>(dataLen >> 16);
    chunk[lengthPos + 2] = static_cast<uint8_t>(dataLen >> 8);
    chunk[lengthPos + 3] = static_cast<uint8_t>(dataLen);

    // CRC over type + data
    uint32_t crc = Crc32(chunk.data() + typePos, chunk.size() - typePos);
    AppendBE32(chunk, crc);

    return chunk;
}

void PngWriteCallback(void *context, void *data, int size)
{
    auto *vec = static_cast<std::vector<uint8_t> *>(context);
    auto *bytes = static_cast<const uint8_t *>(data);
    vec->insert(vec->end(), bytes, bytes + size);
}

} // namespace

bool WritePngWithText(
    const std::filesystem::path &path,
    int width, int height,
    std::span<const uint8_t> rgba,
    std::span<const TextChunk> textChunks)
{
    // Generate raw PNG in memory via callback
    std::vector<uint8_t> pngData;
    int ok = stbi_write_png_to_func(
        PngWriteCallback, &pngData,
        width, height, 4, rgba.data(), width * 4);
    if (!ok || pngData.empty()) return false;

    if (textChunks.empty()) {
        std::ofstream f(path, std::ios::binary);
        if (!f) return false;
        f.write(reinterpret_cast<const char *>(pngData.data()),
                static_cast<std::streamsize>(pngData.size()));
        return f.good();
    }

    // Find insertion point: after IHDR chunk
    // PNG signature(8) + length(4) + type(4) + data(13) + crc(4) = 33
    constexpr size_t kAfterIHDR = 8 + 4 + 4 + 13 + 4;
    if (pngData.size() < kAfterIHDR) return false;

    // Build output: [sig + IHDR] [iTXt chunks...] [rest of PNG]
    std::vector<uint8_t> output;
    output.reserve(pngData.size() + textChunks.size() * 64);

    output.insert(output.end(), pngData.begin(), pngData.begin() + kAfterIHDR);

    for (const auto &tc : textChunks) {
        auto itxt = BuildITxtChunk(tc.keyword, tc.text);
        output.insert(output.end(), itxt.begin(), itxt.end());
    }

    output.insert(output.end(), pngData.begin() + kAfterIHDR, pngData.end());

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char *>(output.data()),
            static_cast<std::streamsize>(output.size()));
    return f.good();
}

} // namespace png
