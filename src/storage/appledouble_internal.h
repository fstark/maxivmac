#pragma once
#include "storage/appledouble.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace appledouble::detail
{

/* ── Big-endian helpers ───────────────────────────── */

constexpr uint32_t ReadBE32(const uint8_t *p)
{
	return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
		   (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

constexpr uint16_t ReadBE16(const uint8_t *p)
{
	return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

inline void WriteBE32(uint8_t *p, uint32_t v)
{
	p[0] = static_cast<uint8_t>(v >> 24);
	p[1] = static_cast<uint8_t>(v >> 16);
	p[2] = static_cast<uint8_t>(v >> 8);
	p[3] = static_cast<uint8_t>(v);
}

inline void WriteBE16(uint8_t *p, uint16_t v)
{
	p[0] = static_cast<uint8_t>(v >> 8);
	p[1] = static_cast<uint8_t>(v);
}

/* ── Sidecar data structures ─────────────────────── */

struct SidecarEntry
{
	uint32_t id = 0;
	uint32_t offset = 0;
	uint32_t length = 0;
};

struct ParsedSidecar
{
	bool valid = false;
	std::vector<SidecarEntry> entries;
	std::vector<uint8_t> finderInfoData; // 32 bytes if present
	std::vector<uint8_t> resourceForkData;

	bool HasFinderInfo() const { return finderInfoData.size() == kFinderInfoSize; }
	bool HasResourceFork() const { return !resourceForkData.empty(); }
};

/* ── Sidecar parse / write ────────────────────────── */

ParsedSidecar ParseSidecar(const std::filesystem::path &sidecarPath);

void WriteSidecar(const std::filesystem::path &sidecarPath, const std::optional<FinderInfo> &finder,
				  const std::optional<std::vector<uint8_t>> &rsrcFork);

/* ── Finder info blob helpers ─────────────────────── */

FinderInfo FinderInfoFromBlob(const std::vector<uint8_t> &blob);
std::vector<uint8_t> BlobFromFinderInfo(const FinderInfo &info);

} // namespace appledouble::detail
