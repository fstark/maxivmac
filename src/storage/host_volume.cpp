#include "storage/host_volume.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>

namespace storage
{

namespace fs = std::filesystem;

/* ── Helpers ──────────────────────────────────────── */

uint32_t HostVolume::currentMacDate()
{
	auto now = std::chrono::system_clock::now();
	auto secs = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
	return static_cast<uint32_t>(secs + appledouble::kMacEpochOffset);
}

/* ── Mount ────────────────────────────────────────── */

bool HostVolume::mount(const std::filesystem::path &hostDir)
{
	if (!fs::is_directory(hostDir)) return false;

	rootPath_ = hostDir;
	catalog_.clear();
	nextCNID_ = 16;
	openForks_.clear();
	nextHandle_ = 1;
	wdTable_.clear();
	nextWD_ = 0x8000;
	textStats_ = {};

	static bool s_typesLoaded = false;
	if (!s_typesLoaded)
	{
		appledouble::LoadTypeMappings("assets/typemap.def");
		s_typesLoaded = true;
	}

	scanDirectory(hostDir, kRootDirID);
	mounted_ = true;
	return true;
}

bool HostVolume::isMounted() const
{
	return mounted_;
}

/* ── Catalog queries ──────────────────────────────── */

const CatalogEntry *HostVolume::findByCNID(uint32_t cnid) const
{
	for (const auto &e : catalog_)
		if (e.cnid == cnid) return &e;
	return nullptr;
}

const CatalogEntry *HostVolume::findByName(uint32_t parentDirID, std::string_view macName) const
{
	for (const auto &e : catalog_)
	{
		if (e.parentDirID != parentDirID) continue;
		if (e.macName.size() != macName.size()) continue;
		bool match = true;
		for (size_t i = 0; i < macName.size(); ++i)
		{
			if (tolower(static_cast<unsigned char>(e.macName[i])) !=
				tolower(static_cast<unsigned char>(macName[i])))
			{
				match = false;
				break;
			}
		}
		if (match) return &e;
	}
	return nullptr;
}

const CatalogEntry *HostVolume::nthChild(uint32_t parentDirID, int index) const
{
	int count = 0;
	for (const auto &e : catalog_)
	{
		if (e.parentDirID == parentDirID)
		{
			++count;
			if (count == index) return &e;
		}
	}
	return nullptr;
}

int HostVolume::childCount(uint32_t parentDirID) const
{
	int count = 0;
	for (const auto &e : catalog_)
		if (e.parentDirID == parentDirID) ++count;
	return count;
}

void HostVolume::volumeStats(uint32_t &outFiles, uint32_t &outBytes) const
{
	outFiles = 0;
	outBytes = 0;
}

/* ── File/directory creation ──────────────────────── */

uint32_t HostVolume::createFile(uint32_t /*parentDirID*/, std::string_view /*macName*/,
								FMErr &errOut)
{
	errOut = FMErr::kFnfErr;
	return 0;
}

uint32_t HostVolume::createDir(uint32_t /*parentDirID*/, std::string_view /*macName*/,
							   FMErr &errOut)
{
	errOut = FMErr::kFnfErr;
	return 0;
}

/* ── Deletion ─────────────────────────────────────── */

FMErr HostVolume::remove(uint32_t /*parentDirID*/, std::string_view /*macName*/)
{
	return FMErr::kFnfErr;
}

/* ── Move / rename ────────────────────────────────── */

FMErr HostVolume::move(uint32_t /*srcDirID*/, std::string_view /*macName*/, uint32_t /*dstDirID*/)
{
	return FMErr::kFnfErr;
}

/* ── Metadata ─────────────────────────────────────── */

FMErr HostVolume::setFileInfo(uint32_t /*cnid*/, uint32_t /*type*/, uint32_t /*creator*/)
{
	return FMErr::kFnfErr;
}

/* ── Fork I/O ─────────────────────────────────────── */

uint32_t HostVolume::openFork(uint32_t /*cnid*/, ForkType /*fork*/, uint32_t & /*outSize*/,
							  FMErr &errOut)
{
	(void)nextHandle_;
	errOut = FMErr::kFnfErr;
	return 0;
}

FMErr HostVolume::readFork(uint32_t /*handle*/, uint32_t /*offset*/, std::span<uint8_t> /*buf*/,
						   uint32_t &outRead)
{
	outRead = 0;
	return FMErr::kRfNumErr;
}

FMErr HostVolume::writeFork(uint32_t /*handle*/, uint32_t /*offset*/,
							std::span<const uint8_t> /*data*/, uint32_t &outWritten)
{
	outWritten = 0;
	return FMErr::kRfNumErr;
}

void HostVolume::closeFork(uint32_t /*handle*/) {}

/* ── Working directories ──────────────────────────── */

uint32_t HostVolume::openWD(uint32_t /*dirID*/)
{
	(void)nextWD_;
	return 0;
}

uint32_t HostVolume::wdToDirID(uint32_t /*wdRef*/) const
{
	return 0;
}

void HostVolume::closeWD(uint32_t /*wdRef*/) {}

/* ── TEXT conversion stats ────────────────────────── */

HostVolume::TextStats HostVolume::textConversionStats() const
{
	return textStats_;
}

void HostVolume::resetTextConversionStats()
{
	textStats_ = {};
}

/* ── Private helpers ──────────────────────────────── */

void HostVolume::scanDirectory(const std::filesystem::path &hostDir, uint32_t parentDirID)
{
	std::error_code ec;
	for (const auto &entry : fs::directory_iterator(hostDir, ec))
	{
		if (ec) break;
		std::string name = entry.path().filename().string();
		if (name.empty() || name[0] == '.') continue;
		if (appledouble::IsSidecar(name)) continue;

		std::string macName = appledouble::MacNameFromHost(name);
		if (macName.size() > 31) macName = macName.substr(0, 31);
		if (macName.empty()) continue;

		CatalogEntry ce{};
		ce.cnid = nextCNID_++;
		ce.parentDirID = parentDirID;
		ce.hostPath = entry.path().string();
		ce.macName = macName;

		if (entry.is_directory(ec))
		{
			ce.isDirectory = true;
			auto ftime = fs::last_write_time(entry.path(), ec);
			ce.crDate = ec ? 0 : appledouble::MacDateFromFileTime(ftime);
			ce.modDate = ce.crDate;
			uint32_t thisDirID = ce.cnid;
			catalog_.push_back(std::move(ce));
			scanDirectory(entry.path(), thisDirID);
		}
		else if (entry.is_regular_file(ec))
		{
			auto info = appledouble::GetFileInfo(entry.path());
			ce.isDirectory = false;
			ce.type = info.finder.type;
			ce.creator = info.finder.creator;
			ce.finderFlags = info.finder.flags;
			ce.dataForkSize = info.dataForkSize;
			ce.rsrcForkSize = info.rsrcForkSize;
			ce.crDate = info.crDate;
			ce.modDate = info.modDate;
			ce.isText = info.isText;
			catalog_.push_back(std::move(ce));
		}
	}
}

CatalogEntry *HostVolume::mutableFindByCNID(uint32_t cnid)
{
	for (auto &e : catalog_)
		if (e.cnid == cnid) return &e;
	return nullptr;
}

std::string HostVolume::resolveParentPath(uint32_t parentDirID) const
{
	if (parentDirID == kRootDirID) return rootPath_.string();
	for (const auto &e : catalog_)
		if (e.cnid == parentDirID && e.isDirectory) return e.hostPath;
	return {};
}

void HostVolume::invalidateTextSize(CatalogEntry & /*entry*/) {}

} // namespace storage
