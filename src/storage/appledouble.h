#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace appledouble
{

/* ── Constants ────────────────────────────────────── */

inline constexpr uint32_t kMacEpochOffset = 2082844800u; // 1904→1970

inline constexpr uint32_t kAppleDoubleMagic = 0x00051607u;
inline constexpr uint32_t kAppleDoubleVersion = 0x00020000u;
inline constexpr uint32_t kEntryIdResourceFork = 2u;
inline constexpr uint32_t kEntryIdFinderInfo = 9u;
inline constexpr uint32_t kFinderInfoSize = 32u;
inline constexpr uint32_t kHeaderSize = 26u;	// magic+ver+filler+nEntries
inline constexpr uint32_t kEntryDescSize = 12u; // per entry descriptor

/* ── FourCC helper ────────────────────────────────── */

constexpr uint32_t FourCC(const char (&s)[5])
{
	return (static_cast<uint32_t>(static_cast<uint8_t>(s[0])) << 24) |
		   (static_cast<uint32_t>(static_cast<uint8_t>(s[1])) << 16) |
		   (static_cast<uint32_t>(static_cast<uint8_t>(s[2])) << 8) |
		   static_cast<uint32_t>(static_cast<uint8_t>(s[3]));
}

/* ── Finder info ──────────────────────────────────── */

struct FinderInfo
{
	uint32_t type = 0;
	uint32_t creator = 0;
	uint16_t flags = 0;
	uint32_t location = 0;
	uint16_t folder = 0;

	bool operator==(const FinderInfo &) const = default;
};

/* ── Per-file metadata snapshot ───────────────────── */

struct FileInfo
{
	FinderInfo finder;
	uint32_t dataForkSize = 0;
	uint32_t rsrcForkSize = 0;
	uint32_t crDate = 0;
	uint32_t modDate = 0;
	bool isText = false;
};

/* ── Type/creator mapping ─────────────────────────── */

int LoadTypeMappings(const std::filesystem::path &defPath);
FinderInfo FinderInfoFromExtension(std::string_view extension);

/* ── Sidecar path ─────────────────────────────────── */

std::filesystem::path SidecarPathFor(const std::filesystem::path &hostPath);

/* ── Finder info access ───────────────────────────── */

FinderInfo GetFinderInfo(const std::filesystem::path &hostPath);
void SetFinderInfo(const std::filesystem::path &hostPath, const FinderInfo &info);

/* ── Directory Finder info (DInfo+DXInfo, raw 32 bytes) ─── */

// Read the raw 32-byte FinderInfo blob for a directory from its sidecar.
// Returns number of bytes copied (0 if no sidecar exists).
size_t GetDirFinderInfo(const std::filesystem::path &hostPath, uint8_t *outBuf, size_t len);

// Write the raw 32-byte FinderInfo blob for a directory to its sidecar.
void SetDirFinderInfo(const std::filesystem::path &hostPath, const uint8_t *data, size_t len);

/* ── Resource fork access ─────────────────────────── */

uint32_t ResourceForkSize(const std::filesystem::path &hostPath);

std::vector<uint8_t> ReadResourceFork(const std::filesystem::path &hostPath, uint32_t offset,
									  uint32_t count);

void WriteResourceFork(const std::filesystem::path &hostPath, uint32_t offset,
					   std::span<const uint8_t> data);

void SetResourceForkSize(const std::filesystem::path &hostPath, uint32_t newSize);

/* ── Composite query ──────────────────────────────── */

FileInfo GetFileInfo(const std::filesystem::path &hostPath);

/* ── Date handling ────────────────────────────────── */

uint32_t MacDateFromFileTime(std::filesystem::file_time_type ft);
void SetModDate(const std::filesystem::path &hostPath, uint32_t macDate);

/* ── Text conversion (whole-file) ─────────────────── */

std::vector<uint8_t> MacRomanFromUTF8File(const std::filesystem::path &hostPath);
uint32_t MacRomanSizeFromUTF8File(const std::filesystem::path &hostPath);
std::string UTF8FromMacRoman(std::span<const uint8_t> macRoman);

/* ── Filename escaping ────────────────────────────── */

std::string HostNameFromMac(std::string_view macName);
std::string MacNameFromHost(std::string_view hostName);

/* ── Directory enumeration ────────────────────────── */

bool IsSidecar(std::string_view name);

/* ── Sidecar lifecycle ────────────────────────────── */

bool DeleteWithSidecar(const std::filesystem::path &hostPath);
bool RenameWithSidecar(const std::filesystem::path &oldPath, const std::filesystem::path &newPath);

} // namespace appledouble
