#include "icns_decode.h"
#include "icon_decode.h"
#include "mac_palette.h"

#include <algorithm>
#include <cstring>
#include <optional>
#include <unordered_map>

namespace icns {
namespace {

uint32_t ReadBE32(const uint8_t *p)
{
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
           (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

uint32_t FourCC(const char *s)
{
    return (uint32_t(uint8_t(s[0])) << 24) | (uint32_t(uint8_t(s[1])) << 16) |
           (uint32_t(uint8_t(s[2])) << 8) | uint32_t(uint8_t(s[3]));
}

struct Element {
    uint32_t type;
    std::span<const uint8_t> data;
};

std::vector<Element> ParseContainer(std::span<const uint8_t> blob)
{
    std::vector<Element> elems;
    if (blob.size() < 8) return elems;
    if (ReadBE32(blob.data()) != FourCC("icns")) return elems;

    uint32_t totalLen = ReadBE32(blob.data() + 4);
    if (totalLen > blob.size()) totalLen = static_cast<uint32_t>(blob.size());

    size_t offset = 8;
    while (offset + 8 <= totalLen) {
        uint32_t type = ReadBE32(blob.data() + offset);
        uint32_t len = ReadBE32(blob.data() + offset + 4);
        if (len < 8 || offset + len > totalLen) break;
        elems.push_back({type, blob.subspan(offset + 8, len - 8)});
        offset += len;
    }
    return elems;
}

// icns RLE decompression (used for il32, is32, it32, ih32 RGB channels).
// Each channel is compressed independently. Returns false on corrupt data.
bool DecompressChannel(const uint8_t *&src, const uint8_t *srcEnd,
                       uint8_t *dst, size_t pixelCount)
{
    size_t written = 0;
    while (written < pixelCount && src < srcEnd) {
        uint8_t n = *src++;
        if (n >= 128) {
            // Run: repeat next byte (n - 125) times
            size_t count = static_cast<size_t>(n) - 125;
            if (src >= srcEnd) return false;
            uint8_t val = *src++;
            if (written + count > pixelCount) count = pixelCount - written;
            std::memset(dst + written, val, count);
            written += count;
        } else {
            // Literal: copy next (n + 1) bytes
            size_t count = static_cast<size_t>(n) + 1;
            if (src + count > srcEnd) return false;
            if (written + count > pixelCount) count = pixelCount - written;
            std::memcpy(dst + written, src, count);
            src += count;
            written += count;
        }
    }
    return written == pixelCount;
}

// Decode il32/is32/ih32/it32 compressed RGB data into planar R, G, B buffers.
bool DecodeRGB(std::span<const uint8_t> data, int size, std::vector<uint8_t> &rgb)
{
    size_t pixels = static_cast<size_t>(size) * size;
    rgb.resize(pixels * 3);

    const uint8_t *src = data.data();
    const uint8_t *srcEnd = src + data.size();

    // it32 (128×128) has a 4-byte header of zeros
    if (size == 128 && data.size() > 4) {
        if (src[0] == 0 && src[1] == 0 && src[2] == 0 && src[3] == 0)
            src += 4;
    }

    // Decompress R, G, B channels sequentially
    if (!DecompressChannel(src, srcEnd, rgb.data(), pixels)) return false;
    if (!DecompressChannel(src, srcEnd, rgb.data() + pixels, pixels))
        return false;
    if (!DecompressChannel(src, srcEnd, rgb.data() + pixels * 2, pixels))
        return false;

    return true;
}

// Combine planar RGB + alpha mask into interleaved RGBA.
std::vector<uint8_t> CombineRGBA(const std::vector<uint8_t> &rgb,
                                  std::span<const uint8_t> alphaMask,
                                  int size)
{
    size_t pixels = static_cast<size_t>(size) * size;
    std::vector<uint8_t> rgba(pixels * 4);

    for (size_t i = 0; i < pixels; ++i) {
        rgba[i * 4 + 0] = rgb[i];              // R
        rgba[i * 4 + 1] = rgb[pixels + i];     // G
        rgba[i * 4 + 2] = rgb[pixels * 2 + i]; // B
        rgba[i * 4 + 3] = (alphaMask.size() >= pixels)
                               ? alphaMask[i]
                               : 0xFF;
    }
    return rgba;
}

// Check if data looks like a PNG (starts with PNG signature).
bool IsPNG(std::span<const uint8_t> data)
{
    static constexpr uint8_t kPngSig[] = {0x89, 'P', 'N', 'G',
                                           '\r', '\n', 0x1A, '\n'};
    return data.size() >= 8 && std::memcmp(data.data(), kPngSig, 8) == 0;
}

// Check if data looks like JPEG 2000 (starts with JP2 signature box).
bool IsJP2(std::span<const uint8_t> data)
{
    if (data.size() >= 12) {
        static constexpr uint8_t kJP2Sig[] = {0x00, 0x00, 0x00, 0x0C,
                                               0x6A, 0x50, 0x20, 0x20};
        if (std::memcmp(data.data(), kJP2Sig, 8) == 0) return true;
    }
    if (data.size() >= 2 && data[0] == 0xFF && data[1] == 0x4F) return true;
    return false;
}

// Map from RGB type → corresponding mask type.
uint32_t MaskForRGB(uint32_t rgbType)
{
    if (rgbType == FourCC("it32")) return FourCC("t8mk");
    if (rgbType == FourCC("ih32")) return FourCC("h8mk");
    if (rgbType == FourCC("il32")) return FourCC("l8mk");
    if (rgbType == FourCC("is32")) return FourCC("s8mk");
    return 0;
}

int SizeForRGB(uint32_t type)
{
    if (type == FourCC("it32")) return 128;
    if (type == FourCC("ih32")) return 48;
    if (type == FourCC("il32")) return 32;
    if (type == FourCC("is32")) return 16;
    return 0;
}



} // namespace

DecodedIcon ExtractLargest(std::span<const uint8_t> data)
{
    auto elements = ParseContainer(data);
    if (elements.empty()) return {};

    // Build a map of type → element data for quick lookup
    std::unordered_map<uint32_t, std::span<const uint8_t>> byType;
    for (auto &e : elements)
        byType[e.type] = e.data;

    // Strategy: try elements from largest to smallest.
    // 1. PNG/JP2 elements (already complete images)
    // 2. Compressed RGB + mask
    // 3. icl8 (palette-indexed)

    // --- Try PNG/JP2 elements (largest first) ---
    static constexpr uint32_t kPngTypes[] = {
        'ic10', 'ic14', 'ic13', 'ic09', 'ic08', 'ic07', 'ic12', 'ic11'};

    for (uint32_t type : kPngTypes) {
        auto it = byType.find(type);
        if (it == byType.end()) continue;
        auto &d = it->second;
        if (IsPNG(d)) {
            // Return the raw PNG data — caller will write it directly.
            // We signal this with negative width (hack to avoid extra struct fields).
            // Actually let's use a cleaner approach: width > 0 means decoded RGBA,
            // and we'll just return the data as-is with a special marker.
            DecodedIcon icon;
            icon.width = -1; // Signals raw PNG data
            icon.height = 0;
            icon.rgba.assign(d.begin(), d.end());
            return icon;
        }
        // JPEG 2000 — we can't decode this ourselves, skip
        if (IsJP2(d)) continue;
    }

    // --- Try compressed RGB types (largest first) ---
    static constexpr uint32_t kRGBTypes[] = {
        'it32', 'ih32', 'il32', 'is32'};

    for (uint32_t type : kRGBTypes) {
        auto it = byType.find(type);
        if (it == byType.end()) continue;

        int size = SizeForRGB(type);
        if (size == 0) continue;

        std::vector<uint8_t> rgb;
        if (!DecodeRGB(it->second, size, rgb)) continue;

        // Look for corresponding alpha mask
        uint32_t maskType = MaskForRGB(type);
        std::span<const uint8_t> alphaMask;
        if (maskType) {
            auto mit = byType.find(maskType);
            if (mit != byType.end())
                alphaMask = mit->second;
        }

        auto rgba = CombineRGBA(rgb, alphaMask, size);
        return {size, size, std::move(rgba)};
    }

    // --- Try icl8 (32×32 palette-indexed) ---
    {
        auto it = byType.find(FourCC("icl8"));
        if (it != byType.end() && it->second.size() == 1024) {
            // Look for ICN# mask
            std::optional<std::span<const uint8_t, 128>> mask;
            auto mit = byType.find(FourCC("ICN#"));
            if (mit != byType.end() && mit->second.size() == 256)
                mask = std::span<const uint8_t, 128>(
                    mit->second.data() + 128, 128);

            std::span<const uint8_t, 1024> icl8Span(it->second.data(), 1024);
            auto icon = rsrc::DecodeIcl8(icl8Span, mask);
            return {32, 32, {icon.pixels.begin(), icon.pixels.end()}};
        }
    }

    return {};
}

} // namespace icns
