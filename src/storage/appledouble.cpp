#include "storage/appledouble.h"
#include "storage/appledouble_internal.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>

namespace appledouble
{

/* ════════════════════════════════════════════════════
   Type/Creator Mapping
   ════════════════════════════════════════════════════ */

namespace
{

struct TypeMapping
{
	std::string ext; // lowercased, e.g. ".txt"
	uint32_t type;
	uint32_t creator;
};

std::vector<TypeMapping> s_typeMappings;

constexpr FinderInfo kUnknownFinder = {FourCC("????"), FourCC("????"), 0};

uint32_t FourCCFromString(std::string_view s)
{
	if (s.size() != 4) return FourCC("????");
	return (static_cast<uint32_t>(static_cast<uint8_t>(s[0])) << 24) |
		   (static_cast<uint32_t>(static_cast<uint8_t>(s[1])) << 16) |
		   (static_cast<uint32_t>(static_cast<uint8_t>(s[2])) << 8) |
		   static_cast<uint32_t>(static_cast<uint8_t>(s[3]));
}

std::string ToLower(std::string_view sv)
{
	std::string result(sv);
	std::transform(result.begin(), result.end(), result.begin(),
				   [](unsigned char c) { return std::tolower(c); });
	return result;
}

} // anonymous namespace

int LoadTypeMappings(const std::filesystem::path &defPath)
{
	std::ifstream file(defPath);
	if (!file.is_open()) return -1;

	s_typeMappings.clear();
	std::string line;
	while (std::getline(file, line))
	{
		// Skip comments and blank lines
		if (line.empty()) continue;
		auto first = line.find_first_not_of(" \t");
		if (first == std::string::npos) continue;
		if (line[first] == '#') continue;

		std::istringstream iss(line);
		std::string ext, typeStr, creatorStr;
		if (!(iss >> ext >> typeStr >> creatorStr)) continue;

		s_typeMappings.push_back(
			{ToLower(ext), FourCCFromString(typeStr), FourCCFromString(creatorStr)});
	}
	return static_cast<int>(s_typeMappings.size());
}

FinderInfo FinderInfoFromExtension(std::string_view extension)
{
	if (s_typeMappings.empty()) return kUnknownFinder;
	auto lower = ToLower(extension);
	for (const auto &m : s_typeMappings)
	{
		if (m.ext == lower) return {m.type, m.creator, 0};
	}
	return kUnknownFinder;
}

/* ════════════════════════════════════════════════════
   Sidecar Path
   ════════════════════════════════════════════════════ */

std::filesystem::path SidecarPathFor(const std::filesystem::path &hostPath)
{
	return hostPath.parent_path() / ("._" + hostPath.filename().string());
}

/* ════════════════════════════════════════════════════
   Sidecar Binary Format (internal)
   ════════════════════════════════════════════════════ */

namespace detail
{

ParsedSidecar ParseSidecar(const std::filesystem::path &sidecarPath)
{
	ParsedSidecar result;

	std::ifstream file(sidecarPath, std::ios::binary);
	if (!file.is_open()) return result;

	// Read entire file
	std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
							  std::istreambuf_iterator<char>());

	if (data.size() < kHeaderSize) return result;

	uint32_t magic = ReadBE32(data.data());
	uint32_t version = ReadBE32(data.data() + 4);
	if (magic != kAppleDoubleMagic) return result;
	if (version != kAppleDoubleVersion) return result;

	uint16_t numEntries = ReadBE16(data.data() + 24);
	uint32_t descEnd = kHeaderSize + numEntries * kEntryDescSize;
	if (data.size() < descEnd) return result;

	for (uint16_t i = 0; i < numEntries; ++i)
	{
		const uint8_t *desc = data.data() + kHeaderSize + i * kEntryDescSize;
		SidecarEntry entry;
		entry.id = ReadBE32(desc);
		entry.offset = ReadBE32(desc + 4);
		entry.length = ReadBE32(desc + 8);
		result.entries.push_back(entry);

		// Validate the entry data fits
		if (entry.offset + entry.length > data.size()) return {};

		if (entry.id == kEntryIdFinderInfo && entry.length == kFinderInfoSize)
		{
			result.finderInfoData.assign(data.begin() + entry.offset,
										 data.begin() + entry.offset + entry.length);
		}
		else if (entry.id == kEntryIdResourceFork)
		{
			result.resourceForkData.assign(data.begin() + entry.offset,
										   data.begin() + entry.offset + entry.length);
		}
	}

	result.valid = true;
	return result;
}

void WriteSidecar(const std::filesystem::path &sidecarPath, const std::optional<FinderInfo> &finder,
				  const std::optional<std::vector<uint8_t>> &rsrcFork)
{
	// Count entries
	uint16_t numEntries = 0;
	if (finder.has_value()) ++numEntries;
	if (rsrcFork.has_value() && !rsrcFork->empty()) ++numEntries;

	if (numEntries == 0) return;

	uint32_t descLen = numEntries * kEntryDescSize;
	uint32_t dataOffset = kHeaderSize + descLen;

	// Build the file
	std::vector<uint8_t> output;

	// Header: magic(4) + version(4) + filler(16) + numEntries(2) = 26
	output.resize(kHeaderSize, 0);
	WriteBE32(output.data(), kAppleDoubleMagic);
	WriteBE32(output.data() + 4, kAppleDoubleVersion);
	// filler bytes [8..23] stay zero
	WriteBE16(output.data() + 24, numEntries);

	// Entry descriptors
	output.resize(kHeaderSize + descLen, 0);
	uint32_t currentOffset = dataOffset;
	uint16_t descIdx = 0;

	std::vector<uint8_t> finderBlob;
	if (finder.has_value())
	{
		finderBlob = BlobFromFinderInfo(*finder);
		uint8_t *desc = output.data() + kHeaderSize + descIdx * kEntryDescSize;
		WriteBE32(desc, kEntryIdFinderInfo);
		WriteBE32(desc + 4, currentOffset);
		WriteBE32(desc + 8, kFinderInfoSize);
		currentOffset += kFinderInfoSize;
		++descIdx;
	}

	if (rsrcFork.has_value() && !rsrcFork->empty())
	{
		uint8_t *desc = output.data() + kHeaderSize + descIdx * kEntryDescSize;
		WriteBE32(desc, kEntryIdResourceFork);
		WriteBE32(desc + 4, currentOffset);
		WriteBE32(desc + 8, static_cast<uint32_t>(rsrcFork->size()));
		++descIdx;
	}

	// Append data
	if (finder.has_value())
	{
		output.insert(output.end(), finderBlob.begin(), finderBlob.end());
	}
	if (rsrcFork.has_value() && !rsrcFork->empty())
	{
		output.insert(output.end(), rsrcFork->begin(), rsrcFork->end());
	}

	std::ofstream out(sidecarPath, std::ios::binary | std::ios::trunc);
	out.write(reinterpret_cast<const char *>(output.data()),
			  static_cast<std::streamsize>(output.size()));
}

FinderInfo FinderInfoFromBlob(const std::vector<uint8_t> &blob)
{
	if (blob.size() < 10) return {};
	FinderInfo info;
	info.type = ReadBE32(blob.data());
	info.creator = ReadBE32(blob.data() + 4);
	info.flags = ReadBE16(blob.data() + 8);
	return info;
}

std::vector<uint8_t> BlobFromFinderInfo(const FinderInfo &info)
{
	std::vector<uint8_t> blob(kFinderInfoSize, 0);
	WriteBE32(blob.data(), info.type);
	WriteBE32(blob.data() + 4, info.creator);
	WriteBE16(blob.data() + 8, info.flags);
	return blob;
}

} // namespace detail

/* ════════════════════════════════════════════════════
   Finder Info Access
   ════════════════════════════════════════════════════ */

FinderInfo GetFinderInfo(const std::filesystem::path &hostPath)
{
	auto sc = detail::ParseSidecar(SidecarPathFor(hostPath));
	if (sc.valid && sc.HasFinderInfo())
	{
		return detail::FinderInfoFromBlob(sc.finderInfoData);
	}
	return FinderInfoFromExtension(hostPath.extension().string());
}

void SetFinderInfo(const std::filesystem::path &hostPath, const FinderInfo &info)
{
	auto ext = hostPath.extension().string();
	auto defaultInfo = FinderInfoFromExtension(ext);
	auto sidecarPath = SidecarPathFor(hostPath);
	auto sc = detail::ParseSidecar(sidecarPath);

	if (info == defaultInfo)
	{
		// Setting to default — remove Finder info from sidecar
		if (sc.valid && sc.HasResourceFork())
		{
			// Keep sidecar with resource fork only
			detail::WriteSidecar(sidecarPath, std::nullopt,
								 std::make_optional(sc.resourceForkData));
		}
		else if (sc.valid)
		{
			// No resource fork — delete sidecar entirely
			std::filesystem::remove(sidecarPath);
		}
		// else: no sidecar exists, nothing to do
	}
	else
	{
		// Non-default: create/update sidecar
		std::optional<std::vector<uint8_t>> rsrc;
		if (sc.valid && sc.HasResourceFork())
		{
			rsrc = sc.resourceForkData;
		}
		detail::WriteSidecar(sidecarPath, info, rsrc);
	}
}

/* ════════════════════════════════════════════════════
   Resource Fork Access
   ════════════════════════════════════════════════════ */

uint32_t ResourceForkSize(const std::filesystem::path &hostPath)
{
	auto sc = detail::ParseSidecar(SidecarPathFor(hostPath));
	if (!sc.valid) return 0;
	return static_cast<uint32_t>(sc.resourceForkData.size());
}

std::vector<uint8_t> ReadResourceFork(const std::filesystem::path &hostPath, uint32_t offset,
									  uint32_t count)
{
	auto sc = detail::ParseSidecar(SidecarPathFor(hostPath));
	if (!sc.valid || !sc.HasResourceFork()) return {};

	uint32_t forkSize = static_cast<uint32_t>(sc.resourceForkData.size());
	if (offset >= forkSize) return {};
	uint32_t avail = forkSize - offset;
	uint32_t toRead = std::min(count, avail);

	return {sc.resourceForkData.begin() + offset, sc.resourceForkData.begin() + offset + toRead};
}

void WriteResourceFork(const std::filesystem::path &hostPath, uint32_t offset,
					   std::span<const uint8_t> data)
{
	if (data.empty()) return;

	auto sidecarPath = SidecarPathFor(hostPath);
	auto sc = detail::ParseSidecar(sidecarPath);

	std::vector<uint8_t> fork;
	std::optional<FinderInfo> finder;

	if (sc.valid)
	{
		fork = std::move(sc.resourceForkData);
		if (sc.HasFinderInfo())
		{
			finder = detail::FinderInfoFromBlob(sc.finderInfoData);
		}
	}

	// Grow the fork if needed
	uint32_t needed = offset + static_cast<uint32_t>(data.size());
	if (needed > fork.size())
	{
		fork.resize(needed, 0);
	}

	// Copy new data in
	std::memcpy(fork.data() + offset, data.data(), data.size());

	detail::WriteSidecar(sidecarPath, finder, std::make_optional(std::move(fork)));
}

void SetResourceForkSize(const std::filesystem::path &hostPath, uint32_t newSize)
{
	auto sidecarPath = SidecarPathFor(hostPath);
	auto sc = detail::ParseSidecar(sidecarPath);

	std::vector<uint8_t> fork;
	std::optional<FinderInfo> finder;

	if (sc.valid)
	{
		fork = std::move(sc.resourceForkData);
		if (sc.HasFinderInfo())
		{
			finder = detail::FinderInfoFromBlob(sc.finderInfoData);
		}
	}

	fork.resize(newSize, 0);

	if (newSize == 0)
	{
		if (finder.has_value())
		{
			// Keep sidecar with Finder info only
			auto ext = hostPath.extension().string();
			auto defaultInfo = FinderInfoFromExtension(ext);
			if (*finder == defaultInfo)
			{
				// Finder info is default and no rsrc fork → delete
				std::filesystem::remove(sidecarPath);
			}
			else
			{
				detail::WriteSidecar(sidecarPath, finder, std::nullopt);
			}
		}
		else
		{
			// No finder info, no resource fork → delete sidecar
			std::filesystem::remove(sidecarPath);
		}
	}
	else
	{
		detail::WriteSidecar(sidecarPath, finder, std::make_optional(std::move(fork)));
	}
}

/* ════════════════════════════════════════════════════
   Date Handling
   ════════════════════════════════════════════════════ */

uint32_t MacDateFromFileTime(std::filesystem::file_time_type ft)
{
	auto sys = std::chrono::file_clock::to_sys(ft);
	auto epoch = sys.time_since_epoch();
	auto secs = std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
	return static_cast<uint32_t>(secs + kMacEpochOffset);
}

void SetModDate(const std::filesystem::path &hostPath, uint32_t macDate)
{
	auto secs = static_cast<int64_t>(macDate) - static_cast<int64_t>(kMacEpochOffset);
	auto sys = std::chrono::system_clock::time_point(std::chrono::seconds(secs));
	auto ft = std::chrono::file_clock::from_sys(sys);
	std::filesystem::last_write_time(hostPath, ft);
}

/* ════════════════════════════════════════════════════
   Composite Query
   ════════════════════════════════════════════════════ */

FileInfo GetFileInfo(const std::filesystem::path &hostPath)
{
	FileInfo info;
	info.finder = GetFinderInfo(hostPath);
	info.rsrcForkSize = ResourceForkSize(hostPath);
	info.isText = (info.finder.type == FourCC("TEXT"));

	auto ft = std::filesystem::last_write_time(hostPath);
	info.modDate = MacDateFromFileTime(ft);
	info.crDate = info.modDate; // POSIX has no creation date generally

	if (info.isText)
	{
		info.dataForkSize = MacRomanSizeFromUTF8File(hostPath);
	}
	else
	{
		info.dataForkSize = static_cast<uint32_t>(std::filesystem::file_size(hostPath));
	}

	return info;
}

/* ════════════════════════════════════════════════════
   Sidecar Lifecycle
   ════════════════════════════════════════════════════ */

bool DeleteWithSidecar(const std::filesystem::path &hostPath)
{
	try
	{
		if (std::filesystem::is_directory(hostPath))
		{
			// Also check for sidecar in parent
			auto sidecar = SidecarPathFor(hostPath);
			std::filesystem::remove(sidecar); // OK if doesn't exist
			return std::filesystem::remove(hostPath);
		}
		auto sidecar = SidecarPathFor(hostPath);
		std::filesystem::remove(sidecar); // OK if doesn't exist
		return std::filesystem::remove(hostPath);
	}
	catch (const std::filesystem::filesystem_error &)
	{
		return false;
	}
}

bool RenameWithSidecar(const std::filesystem::path &oldPath, const std::filesystem::path &newPath)
{
	try
	{
		std::filesystem::rename(oldPath, newPath);
		auto oldSidecar = SidecarPathFor(oldPath);
		if (std::filesystem::exists(oldSidecar))
		{
			auto newSidecar = SidecarPathFor(newPath);
			std::filesystem::rename(oldSidecar, newSidecar);
		}
		return true;
	}
	catch (const std::filesystem::filesystem_error &)
	{
		return false;
	}
}

} // namespace appledouble
