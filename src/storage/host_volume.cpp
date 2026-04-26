#include "storage/host_volume.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <unistd.h>

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
	nextWD_ = 1;
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

const CatalogEntry *HostVolume::findByPath(uint32_t startDirID, std::string_view hfsPath) const
{
	/* No colon → plain name lookup in the given directory */
	auto firstColon = hfsPath.find(':');
	if (firstColon == std::string_view::npos) return findByName(startDirID, hfsPath);

	uint32_t dir;
	std::string_view rem;

	if (hfsPath.front() == ':')
	{
		/* Relative path ":foo:bar" — walk from startDirID */
		dir = startDirID;
		rem = hfsPath.substr(1);
	}
	else
	{
		/* Absolute path "VolName:foo:bar" — skip the volume name, start at root */
		dir = kRootDirID;
		rem = hfsPath.substr(firstColon + 1);
	}

	while (!rem.empty())
	{
		auto sep = rem.find(':');
		std::string_view component = (sep == std::string_view::npos) ? rem : rem.substr(0, sep);
		bool last = (sep == std::string_view::npos);

		if (!component.empty())
		{
			const CatalogEntry *e = findByName(dir, component);
			if (!e) return nullptr;
			if (last) return e;
			if (!e->isDirectory) return nullptr;
			dir = e->cnid;
		}

		if (last) break;
		rem = rem.substr(sep + 1);
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

void HostVolume::volumeStats(uint32_t &outFiles, uint32_t &outDirs, uint32_t &outBytes) const
{
	outFiles = 0;
	outDirs = 0;
	outBytes = 0;
	for (const auto &e : catalog_)
	{
		if (e.isDirectory)
		{
			outDirs++;
		}
		else
		{
			outFiles++;
			outBytes += e.dataForkSize;
		}
	}
}

/* ── File/directory creation ──────────────────────── */

uint32_t HostVolume::createFile(uint32_t parentDirID, std::string_view macName, OSErr &errOut)
{
	if (findByName(parentDirID, macName))
	{
		errOut = kDupFNErr;
		return 0;
	}

	std::string parentPath = resolveParentPath(parentDirID);
	if (parentPath.empty())
	{
		errOut = kDirNFErr;
		return 0;
	}

	std::string hostName = appledouble::HostNameFromMac(macName);
	std::string hostPath = parentPath + "/" + hostName;

	FILE *fp = fopen(hostPath.c_str(), "wb");
	if (!fp)
	{
		errOut = kIoErr;
		return 0;
	}
	fclose(fp);

	CatalogEntry ce{};
	ce.cnid = nextCNID_++;
	ce.parentDirID = parentDirID;
	ce.hostPath = hostPath;
	ce.macName = std::string(macName);
	ce.isDirectory = false;
	ce.dataForkSize = 0;
	ce.crDate = currentMacDate();
	ce.modDate = ce.crDate;
	catalog_.push_back(std::move(ce));

	errOut = kNoErr;
	return catalog_.back().cnid;
}

uint32_t HostVolume::createDir(uint32_t parentDirID, std::string_view macName, OSErr &errOut)
{
	if (findByName(parentDirID, macName))
	{
		errOut = kDupFNErr;
		return 0;
	}

	std::string parentPath = resolveParentPath(parentDirID);
	if (parentPath.empty())
	{
		errOut = kDirNFErr;
		return 0;
	}

	std::string hostName = appledouble::HostNameFromMac(macName);
	std::string hostPath = parentPath + "/" + hostName;

	std::error_code ec;
	if (!fs::create_directory(hostPath, ec))
	{
		errOut = kIoErr;
		return 0;
	}

	CatalogEntry ce{};
	ce.cnid = nextCNID_++;
	ce.parentDirID = parentDirID;
	ce.hostPath = hostPath;
	ce.macName = std::string(macName);
	ce.isDirectory = true;
	ce.crDate = currentMacDate();
	ce.modDate = ce.crDate;
	catalog_.push_back(std::move(ce));

	errOut = kNoErr;
	return catalog_.back().cnid;
}

/* ── Deletion ─────────────────────────────────────── */

OSErr HostVolume::remove(uint32_t parentDirID, std::string_view macName)
{
	const CatalogEntry *e = findByName(parentDirID, macName);
	if (!e) return kFnfErr;

	if (e->isDirectory)
	{
		if (childCount(e->cnid) > 0) return kFBsyErr;
		std::error_code ec;
		fs::remove(e->hostPath, ec);
	}
	else
	{
		appledouble::DeleteWithSidecar(e->hostPath);
	}

	uint32_t cnid = e->cnid;
	for (auto it = catalog_.begin(); it != catalog_.end(); ++it)
	{
		if (it->cnid == cnid)
		{
			catalog_.erase(it);
			break;
		}
	}
	return kNoErr;
}

/* ── Move / rename ────────────────────────────────── */

OSErr HostVolume::move(uint32_t srcDirID, std::string_view macName, uint32_t dstDirID)
{
	const CatalogEntry *e = findByName(srcDirID, macName);
	if (!e) return kFnfErr;

	std::string dstPath = resolveParentPath(dstDirID);
	if (dstPath.empty()) return kFnfErr;

	std::string newHostPath = dstPath + "/" + fs::path(e->hostPath).filename().string();

	if (e->isDirectory)
	{
		std::error_code ec;
		fs::rename(e->hostPath, newHostPath, ec);
		if (ec) return kIoErr;
	}
	else
	{
		if (!appledouble::RenameWithSidecar(e->hostPath, newHostPath)) return kIoErr;
	}

	uint32_t cnid = e->cnid;
	std::string oldHostPath = e->hostPath;
	bool isDir = e->isDirectory;

	for (auto &entry : catalog_)
	{
		if (entry.cnid == cnid)
		{
			entry.parentDirID = dstDirID;
			entry.hostPath = newHostPath;
		}
		else if (isDir && entry.hostPath.size() > oldHostPath.size() &&
				 entry.hostPath.compare(0, oldHostPath.size(), oldHostPath) == 0 &&
				 entry.hostPath[oldHostPath.size()] == '/')
		{
			entry.hostPath = newHostPath + entry.hostPath.substr(oldHostPath.size());
		}
	}
	return kNoErr;
}

OSErr HostVolume::rename(uint32_t dirID, std::string_view oldMacName, std::string_view newMacName)
{
	if (newMacName.empty() || newMacName.size() > 31) return kParamErr;

	const CatalogEntry *e = findByName(dirID, oldMacName);
	if (!e) return kFnfErr;

	if (findByName(dirID, newMacName)) return kDupFNErr;

	std::string parentPath = resolveParentPath(dirID);
	if (parentPath.empty()) return kDirNFErr;

	std::string newHostName = appledouble::HostNameFromMac(newMacName);
	std::string newHostPath = parentPath + "/" + newHostName;

	if (e->isDirectory)
	{
		std::error_code ec;
		fs::rename(e->hostPath, newHostPath, ec);
		if (ec) return kIoErr;
	}
	else
	{
		if (!appledouble::RenameWithSidecar(e->hostPath, newHostPath)) return kIoErr;
	}

	uint32_t cnid = e->cnid;
	std::string oldHostPath = e->hostPath;
	bool isDir = e->isDirectory;

	for (auto &entry : catalog_)
	{
		if (entry.cnid == cnid)
		{
			entry.hostPath = newHostPath;
			entry.macName = std::string(newMacName);
		}
		else if (isDir && entry.hostPath.size() > oldHostPath.size() &&
				 entry.hostPath.compare(0, oldHostPath.size(), oldHostPath) == 0 &&
				 entry.hostPath[oldHostPath.size()] == '/')
		{
			entry.hostPath = newHostPath + entry.hostPath.substr(oldHostPath.size());
		}
	}
	return kNoErr;
}

/* ── Metadata ─────────────────────────────────────── */

OSErr HostVolume::setFileInfo(uint32_t cnid, uint32_t type, uint32_t creator, uint16_t flags,
							  uint32_t location, uint16_t folder)
{
	CatalogEntry *e = mutableFindByCNID(cnid);
	if (!e || e->isDirectory) return kFnfErr;

	appledouble::SetFinderInfo(e->hostPath, {type, creator, flags, location, folder});
	e->type = type;
	e->creator = creator;
	e->finderFlags = flags;
	e->fdLocation = location;
	e->fdFldr = folder;

	bool wasText = e->isText;
	e->isText = (type == appledouble::FourCC("TEXT"));
	if (e->isText != wasText) invalidateTextSize(*e);

	return kNoErr;
}

bool HostVolume::getDirInfo(uint32_t cnid, std::array<uint8_t, 16> &dinfo,
							std::array<uint8_t, 16> &dxinfo) const
{
	const CatalogEntry *e = findByCNID(cnid);
	if (!e || !e->isDirectory) return false;
	std::memcpy(dinfo.data(), e->dirFinderInfo, 16);
	std::memcpy(dxinfo.data(), e->dirFinderInfo + 16, 16);
	return true;
}

OSErr HostVolume::setDirInfo(uint32_t cnid, const std::array<uint8_t, 16> &dinfo,
							 const std::array<uint8_t, 16> &dxinfo)
{
	CatalogEntry *e = mutableFindByCNID(cnid);
	if (!e || !e->isDirectory) return kFnfErr;
	std::memcpy(e->dirFinderInfo, dinfo.data(), 16);
	std::memcpy(e->dirFinderInfo + 16, dxinfo.data(), 16);
	appledouble::SetDirFinderInfo(e->hostPath, e->dirFinderInfo, 32);
	return kNoErr;
}

/* ── Fork I/O ─────────────────────────────────────── */

uint32_t HostVolume::openFork(uint32_t cnid, ForkType fork, uint32_t &outSize, OSErr &errOut,
							  uint8_t permission)
{
	const CatalogEntry *e = findByCNID(cnid);
	if (!e || e->isDirectory)
	{
		errOut = kFnfErr;
		return 0;
	}

	/* ── Conflict check (IM IV rules) ─────────────────
	   permission 0 = fsCurPerm (default/write)
	   permission 1 = fsRdPerm  (read only)
	   permission 2 = fsWrPerm  (write)
	   permission 3 = fsRdWrPerm (exclusive read/write) */
	for (auto &[_, of] : openForks_)
	{
		if (of.cnid != cnid) continue;
		/* Exclusive open conflicts with any existing path */
		if (permission == 3)
		{
			errOut = kOpWrErr;
			return 0;
		}
		/* Write or default open conflicts with existing write path */
		if ((permission == 0 || permission == 2) && of.hasWrite)
		{
			errOut = kOpWrErr;
			return 0;
		}
	}

	bool wantWrite = (permission != 1);
	uint32_t handle = nextHandle_++;

	if (fork == ForkType::Data)
	{
		FILE *fp = fopen(e->hostPath.c_str(), "r+b");
		if (!fp) fp = fopen(e->hostPath.c_str(), "rb");
		if (!fp) fp = fopen(e->hostPath.c_str(), "w+b");
		if (!fp)
		{
			errOut = kIoErr;
			return 0;
		}

		if (e->isText)
		{
			outSize = e->dataForkSize;
		}
		else
		{
			fseek(fp, 0, SEEK_END);
			outSize = static_cast<uint32_t>(ftell(fp));
			fseek(fp, 0, SEEK_SET);
		}

		openForks_[handle] = {cnid, ForkType::Data, fp, wantWrite};
	}
	else
	{
		/* Resource fork: no FILE*, handled by AppleDouble library */
		openForks_[handle] = {cnid, ForkType::Resource, nullptr, wantWrite};
		outSize = appledouble::ResourceForkSize(e->hostPath);
	}

	errOut = kNoErr;
	return handle;
}

OSErr HostVolume::readFork(uint32_t handle, uint32_t offset, std::span<uint8_t> buf,
						   uint32_t &outRead)
{
	auto it = openForks_.find(handle);
	if (it == openForks_.end())
	{
		outRead = 0;
		return kRfNumErr;
	}

	const OpenFork &of = it->second;
	CatalogEntry *e = mutableFindByCNID(of.cnid);
	if (!e)
	{
		outRead = 0;
		return kFnfErr;
	}

	if (of.fork == ForkType::Resource)
	{
		auto data =
			appledouble::ReadResourceFork(e->hostPath, offset, static_cast<uint32_t>(buf.size()));
		uint32_t toRead = static_cast<uint32_t>(data.size());
		std::memcpy(buf.data(), data.data(), toRead);
		outRead = toRead;
		return kNoErr;
	}

	if (e->isText)
	{
		auto converted = appledouble::MacRomanFromUTF8File(e->hostPath);

		std::error_code ec;
		textStats_.conversions++;
		textStats_.bytesIn += fs::file_size(e->hostPath, ec);
		textStats_.bytesOut += converted.size();

		uint32_t available =
			(offset < converted.size()) ? static_cast<uint32_t>(converted.size() - offset) : 0;
		uint32_t toRead = std::min(static_cast<uint32_t>(buf.size()), available);
		std::memcpy(buf.data(), converted.data() + offset, toRead);
		outRead = toRead;
		return kNoErr;
	}

	/* Non-TEXT data fork */
	FILE *fp = of.fp;
	fseek(fp, static_cast<long>(offset), SEEK_SET);
	size_t got = fread(buf.data(), 1, buf.size(), fp);
	outRead = static_cast<uint32_t>(got);
	return kNoErr;
}

OSErr HostVolume::writeFork(uint32_t handle, uint32_t offset, std::span<const uint8_t> data,
							uint32_t &outWritten)
{
	auto it = openForks_.find(handle);
	if (it == openForks_.end())
	{
		outWritten = 0;
		return kRfNumErr;
	}

	const OpenFork &of = it->second;
	CatalogEntry *e = mutableFindByCNID(of.cnid);
	if (!e)
	{
		outWritten = 0;
		return kFnfErr;
	}

	if (of.fork == ForkType::Resource)
	{
		appledouble::WriteResourceFork(e->hostPath, offset, data);
		e->rsrcForkSize = appledouble::ResourceForkSize(e->hostPath);
		e->modDate = currentMacDate();
		outWritten = static_cast<uint32_t>(data.size());
		return kNoErr;
	}

	if (e->isText)
	{
		auto existing = appledouble::MacRomanFromUTF8File(e->hostPath);
		if (offset + data.size() > existing.size()) existing.resize(offset + data.size());
		std::memcpy(existing.data() + offset, data.data(), data.size());
		auto utf8 = appledouble::UTF8FromMacRoman(existing);
		std::ofstream out(e->hostPath, std::ios::binary | std::ios::trunc);
		out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
		out.close();
		outWritten = static_cast<uint32_t>(data.size());
		e->dataForkSize = static_cast<uint32_t>(existing.size());
		e->modDate = currentMacDate();
		return kNoErr;
	}

	/* Non-TEXT data fork */
	FILE *fp = of.fp;
	fseek(fp, static_cast<long>(offset), SEEK_SET);
	size_t wrote = fwrite(data.data(), 1, data.size(), fp);
	fflush(fp);

	fseek(fp, 0, SEEK_END);
	e->dataForkSize = static_cast<uint32_t>(ftell(fp));
	e->modDate = currentMacDate();
	outWritten = static_cast<uint32_t>(wrote);
	return kNoErr;
}

OSErr HostVolume::setEOF(uint32_t handle, uint32_t newSize)
{
	auto it = openForks_.find(handle);
	if (it == openForks_.end()) return kRfNumErr;

	const OpenFork &of = it->second;
	CatalogEntry *e = mutableFindByCNID(of.cnid);
	if (!e) return kFnfErr;

	if (of.fork == ForkType::Resource)
	{
		appledouble::SetResourceForkSize(e->hostPath, newSize);
		e->rsrcForkSize = newSize;
	}
	else if (of.fp)
	{
		fflush(of.fp);
		int fd = fileno(of.fp);
		if (fd >= 0) ftruncate(fd, static_cast<off_t>(newSize));
		e->dataForkSize = newSize;
	}
	e->modDate = currentMacDate();
	return kNoErr;
}

void HostVolume::closeFork(uint32_t handle)
{
	auto it = openForks_.find(handle);
	if (it == openForks_.end()) return;
	if (it->second.fp) fclose(it->second.fp);
	openForks_.erase(it);
}

/* ── Working directories ──────────────────────────── */

uint32_t HostVolume::openWD(uint32_t dirID, uint32_t procID)
{
	uint32_t wdRef = nextWD_++;
	wdTable_[wdRef] = {dirID, procID};
	return wdRef;
}

uint32_t HostVolume::wdToDirID(uint32_t wdRef) const
{
	auto it = wdTable_.find(wdRef);
	return (it != wdTable_.end()) ? it->second.dirID : 0;
}

uint32_t HostVolume::wdToProcID(uint32_t wdRef) const
{
	auto it = wdTable_.find(wdRef);
	return (it != wdTable_.end()) ? it->second.procID : 0;
}

void HostVolume::closeWD(uint32_t wdRef)
{
	wdTable_.erase(wdRef);
}

void HostVolume::setDefaultVRefNum(int16_t vRefNum)
{
	defaultVRefNum_ = vRefNum;
}

/* ── Directory resolution ─────────────────────────── */

uint32_t HostVolume::resolveDir(int16_t vRefNum, uint32_t rawDirID) const
{
	if (rawDirID != 0) return rawDirID;
	/* vRefNum 0 = "default volume" — substitute the stored default */
	if (vRefNum == 0) vRefNum = defaultVRefNum_;
	/* Raw volume ref or drive number with dirID=0: if the user has
	   a non-root default WD, substitute it so that apps passing the
	   volume ref without a dirID land in the current directory. */
	if (vRefNum == kGuestVRefNum || vRefNum == kGuestDriveNum)
	{
		if (defaultVRefNum_ != kGuestVRefNum && defaultVRefNum_ != kGuestDriveNum)
			vRefNum = defaultVRefNum_;
		else
			return kRootDirID;
	}
	/* Decode WD refnum: guest encodes as -(wdRef + 32000) */
	auto wdRef = static_cast<uint32_t>(-(static_cast<int32_t>(vRefNum)) - 32000);
	uint32_t dirID = wdToDirID(wdRef);
	return dirID != 0 ? dirID : kRootDirID;
}

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

		auto macName = appledouble::MacNameFromHost(name);
		if (!macName)
		{
			fprintf(stderr, "[ExtFS] skipping '%s': not representable in MacRoman\n", name.c_str());
			continue;
		}
		if (macName->size() > 31) *macName = macName->substr(0, 31);

		CatalogEntry ce{};
		ce.cnid = nextCNID_++;
		ce.parentDirID = parentDirID;
		ce.hostPath = entry.path().string();
		ce.macName = std::move(*macName);

		if (entry.is_directory(ec))
		{
			ce.isDirectory = true;
			auto ftime = fs::last_write_time(entry.path(), ec);
			ce.crDate = ec ? 0 : appledouble::MacDateFromFileTime(ftime);
			ce.modDate = ce.crDate;
			appledouble::GetDirFinderInfo(entry.path(), ce.dirFinderInfo, 32);
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
			ce.fdLocation = info.finder.location;
			ce.fdFldr = info.finder.folder;
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

void HostVolume::invalidateTextSize(CatalogEntry &entry)
{
	if (entry.isDirectory) return;
	if (entry.isText)
	{
		entry.dataForkSize = appledouble::MacRomanSizeFromUTF8File(entry.hostPath);
	}
	else
	{
		std::error_code ec;
		entry.dataForkSize = static_cast<uint32_t>(fs::file_size(entry.hostPath, ec));
	}
}

bool HostVolume::validateCatalog() const
{
	bool ok = true;
	for (const auto &e : catalog_)
	{
		/* Check 1: hostPath must exist on disk */
		std::error_code ec;
		bool exists = fs::exists(e.hostPath, ec);
		if (!exists)
		{
			fprintf(stderr, "[ValidateCatalog] MISSING: cnid=%u parent=%u \"%s\" -> %s\n", e.cnid,
					e.parentDirID, e.macName.c_str(), e.hostPath.c_str());
			ok = false;
			continue;
		}

		/* Check 2: directory flag must match */
		bool isDir = fs::is_directory(e.hostPath, ec);
		if (isDir != e.isDirectory)
		{
			fprintf(stderr, "[ValidateCatalog] TYPE MISMATCH: cnid=%u \"%s\" catalog=%s disk=%s\n",
					e.cnid, e.macName.c_str(), e.isDirectory ? "dir" : "file",
					isDir ? "dir" : "file");
			ok = false;
		}

		/* Check 3: hostPath parent must match resolveParentPath(parentDirID) */
		std::string expectedParent = resolveParentPath(e.parentDirID);
		if (expectedParent.empty() && e.parentDirID != kRootParentID)
		{
			fprintf(stderr,
					"[ValidateCatalog] ORPHAN: cnid=%u parent=%u \"%s\" (parent not in catalog)\n",
					e.cnid, e.parentDirID, e.macName.c_str());
			ok = false;
		}
		else if (!expectedParent.empty())
		{
			fs::path actualParent = fs::path(e.hostPath).parent_path();
			fs::path expected = fs::path(expectedParent);
			if (fs::weakly_canonical(actualParent, ec) != fs::weakly_canonical(expected, ec))
			{
				fprintf(stderr,
						"[ValidateCatalog] PATH MISMATCH: cnid=%u \"%s\"\n"
						"  catalog parent=%u -> \"%s\"\n"
						"  hostPath parent  -> \"%s\"\n",
						e.cnid, e.macName.c_str(), e.parentDirID, expectedParent.c_str(),
						actualParent.string().c_str());
				ok = false;
			}
		}
	}
	return ok;
}

} // namespace storage
