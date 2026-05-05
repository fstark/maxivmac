#include <doctest/doctest.h>
#include "resource_fork.h"
#include "icon_decode.h"
#include "mac_palette.h"
#include "storage/appledouble_internal.h"

#include <filesystem>
#include <fstream>
#include <vector>

namespace {

std::vector<uint8_t> ReadTestFile(const char *path)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    REQUIRE(f.is_open());
    auto size = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    f.read(reinterpret_cast<char *>(buf.data()), size);
    return buf;
}

// Extract raw resource fork from the AppleDouble sidecar
std::vector<uint8_t> ExtractForkFromSidecar(const std::vector<uint8_t> &sidecar)
{
    REQUIRE(sidecar.size() >= 26);
    using appledouble::detail::ReadBE16;
    using appledouble::detail::ReadBE32;

    REQUIRE(ReadBE32(sidecar.data()) == 0x00051607u);
    uint16_t numEntries = ReadBE16(sidecar.data() + 24);
    for (uint16_t i = 0; i < numEntries; ++i) {
        const uint8_t *e = sidecar.data() + 26 + i * 12;
        uint32_t entryId = ReadBE32(e);
        uint32_t offset = ReadBE32(e + 4);
        uint32_t length = ReadBE32(e + 8);
        if (entryId == 2) { // resource fork
            REQUIRE(offset + length <= sidecar.size());
            return {sidecar.begin() + offset,
                    sidecar.begin() + offset + length};
        }
    }
    FAIL("No resource fork entry in sidecar");
    return {};
}

} // namespace

TEST_CASE("ParseResourceFork — empty input")
{
    std::vector<uint8_t> empty;
    auto res = rsrc::ParseResourceFork(empty);
    CHECK(res.empty());
}

TEST_CASE("ParseResourceFork — truncated input")
{
    std::vector<uint8_t> small(10, 0);
    auto res = rsrc::ParseResourceFork(small);
    CHECK(res.empty());
}

TEST_CASE("ParseResourceFork — real sidecar (Macintosh 128K)")
{
    auto sidecar = ReadTestFile("test/fixtures/icon_128k_sidecar.bin");
    REQUIRE(!sidecar.empty());

    auto fork = ExtractForkFromSidecar(sidecar);
    REQUIRE(!fork.empty());

    auto resources = rsrc::ParseResourceFork(fork);
    REQUIRE(!resources.empty());

    // Should find icl8 resources (type = 'icl8' = 0x69636C38)
    auto icl8s = rsrc::FindByType(resources, 0x69636C38u);
    CHECK(!icl8s.empty());

    // Each icl8 should be exactly 1024 bytes
    for (auto *r : icl8s) {
        CHECK(r->data.size() == 1024);
    }

    // Should find ICN# resources (type = 'ICN#' = 0x49434E23)
    auto icns = rsrc::FindByType(resources, 0x49434E23u);
    CHECK(!icns.empty());

    // Each ICN# should be exactly 256 bytes (icon + mask)
    for (auto *r : icns) {
        CHECK(r->data.size() == 256);
    }

    // For each icl8, a matching ICN# with same ID should exist
    for (auto *icon : icl8s) {
        auto *mask = rsrc::FindByTypeAndId(resources, 0x49434E23u, icon->id);
        CHECK_MESSAGE(mask != nullptr,
                      "Missing ICN# mask for icl8 id=", icon->id);
    }
}

TEST_CASE("FindByTypeAndId — returns nullptr for missing")
{
    std::vector<rsrc::Resource> resources;
    resources.push_back({0x12345678u, 1, "", {}});
    CHECK(rsrc::FindByTypeAndId(resources, 0x12345678u, 99) == nullptr);
    CHECK(rsrc::FindByTypeAndId(resources, 0x12345678u, 1) != nullptr);
}

TEST_CASE("DecodeIcl8 — uniform color, no mask")
{
    // Fill icl8 with palette index 0 (should be white: 0xFFFFFFFF)
    std::array<uint8_t, 1024> icl8;
    icl8.fill(0);

    auto icon = rsrc::DecodeIcl8(icl8, std::nullopt);

    // First pixel should be R=0xFF, G=0xFF, B=0xFF, A=0xFF
    CHECK(icon.pixels[0] == 0xFF);
    CHECK(icon.pixels[1] == 0xFF);
    CHECK(icon.pixels[2] == 0xFF);
    CHECK(icon.pixels[3] == 0xFF);
}

TEST_CASE("DecodeIcl8 — black (index 255)")
{
    std::array<uint8_t, 1024> icl8;
    icl8.fill(255);

    auto icon = rsrc::DecodeIcl8(icl8, std::nullopt);

    // Black: R=0, G=0, B=0, A=0xFF
    CHECK(icon.pixels[0] == 0x00);
    CHECK(icon.pixels[1] == 0x00);
    CHECK(icon.pixels[2] == 0x00);
    CHECK(icon.pixels[3] == 0xFF);
}

TEST_CASE("DecodeIcl8 — mask zeroes alpha")
{
    std::array<uint8_t, 1024> icl8;
    icl8.fill(0);

    // All-zero mask -> all transparent
    std::array<uint8_t, 128> mask;
    mask.fill(0x00);

    auto icon = rsrc::DecodeIcl8(icl8, std::span<const uint8_t, 128>(mask));

    CHECK(icon.pixels[3] == 0x00);
}

TEST_CASE("DecodeIcl8 — mask ones preserve alpha")
{
    std::array<uint8_t, 1024> icl8;
    icl8.fill(0);

    // All-ones mask -> all opaque
    std::array<uint8_t, 128> mask;
    mask.fill(0xFF);

    auto icon = rsrc::DecodeIcl8(icl8, std::span<const uint8_t, 128>(mask));

    CHECK(icon.pixels[3] == 0xFF);
}

TEST_CASE("DecodeIcl8 — real data from sidecar")
{
    auto sidecar = ReadTestFile("test/fixtures/icon_128k_sidecar.bin");
    auto fork = ExtractForkFromSidecar(sidecar);
    auto resources = rsrc::ParseResourceFork(fork);

    auto icl8s = rsrc::FindByType(resources, 0x69636C38u);
    REQUIRE(!icl8s.empty());

    auto *first = icl8s.front();
    REQUIRE(first->data.size() == 1024);

    // Get mask from ICN# (second 128 bytes)
    auto *icn = rsrc::FindByTypeAndId(resources, 0x49434E23u, first->id);
    REQUIRE(icn != nullptr);
    REQUIRE(icn->data.size() == 256);

    std::span<const uint8_t, 128> maskSpan(icn->data.data() + 128, 128);
    std::span<const uint8_t, 1024> icl8Span(first->data.data(), 1024);

    auto icon = rsrc::DecodeIcl8(icl8Span, maskSpan);

    // Sanity: at least some opaque pixels exist
    bool hasOpaque = false;
    for (size_t i = 3; i < icon.pixels.size(); i += 4) {
        if (icon.pixels[i] == 0xFF) {
            hasOpaque = true;
            break;
        }
    }
    CHECK(hasOpaque);
}
