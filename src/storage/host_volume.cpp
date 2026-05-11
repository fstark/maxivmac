#include "storage/host_volume.h"
#include "core/diag.h"
#include "platform/common/path_utils.h"
#include "util/macroman.h"

#define CACHE_LOG(fmt, ...) DIAG(CACHE, fmt "\n", ##__VA_ARGS__)

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <unistd.h>

namespace storage
{

namespace fs = std::filesystem;

/* ── CnidTable ────────────────────────────────────── */

bool CnidKey::operator==(const CnidKey &o) const
{
	if (parentDirID != o.parentDirID) return false;
	if (macName.size() != o.macName.size()) return false;
	for (size_t i = 0; i < macName.size(); ++i)
	{
		if (tolower(static_cast<unsigned char>(macName[i])) !=
			tolower(static_cast<unsigned char>(o.macName[i])))
			return false;
	}
	return true;
}

std::size_t CnidKeyHash::operator()(const CnidKey &k) const
{
	// FNV-1a hash of case-folded macName, mixed with parentDirID
	std::size_t h = 14695981039346656037ULL;
	for (unsigned char c : k.macName)
	{
		h ^= static_cast<std::size_t>(tolower(c));
		h *= 1099511628211ULL;
	}
	return h ^ std::hash<uint32_t>{}(k.parentDirID);
}

uint32_t CnidTable::resolve(uint32_t parentDirID, std::string_view macName,
							std::string_view hostPath)
{
	CnidKey key{parentDirID, std::string(macName)};
	auto it = forward_.find(key);
	if (it != forward_.end()) return it->second;

	uint32_t cnid = nextCnid_++;
	forward_[key] = cnid;
	reverse_[cnid] = CnidValue{key, std::string(hostPath)};

	CACHE_LOG("resolve: new cnid=%u parent=%u name=\"%s\"", cnid, parentDirID,
			  std::string(macName).c_str());
	return cnid;
}

const CnidValue *CnidTable::reverse(uint32_t cnid) const
{
	auto it = reverse_.find(cnid);
	return it != reverse_.end() ? &it->second : nullptr;
}

void CnidTable::updateKey(uint32_t cnid, uint32_t newParentDirID, std::string_view newMacName,
						  std::string_view newHostPath)
{
	auto rit = reverse_.find(cnid);
	if (rit == reverse_.end()) return;

	// Erase old forward entry
	forward_.erase(rit->second.key);

	// Build new key
	CnidKey newKey{newParentDirID, std::string(newMacName)};
	forward_[newKey] = cnid;
	rit->second.key = newKey;
	rit->second.hostPath = std::string(newHostPath);

	CACHE_LOG("updateKey: cnid=%u -> parent=%u name=\"%s\"", cnid, newParentDirID,
			  std::string(newMacName).c_str());
}

void CnidTable::updateHostPath(uint32_t cnid, std::string_view newHostPath)
{
	auto rit = reverse_.find(cnid);
	if (rit == reverse_.end()) return;
	rit->second.hostPath = std::string(newHostPath);

	CACHE_LOG("updateHostPath: cnid=%u -> \"%s\"", cnid, std::string(newHostPath).c_str());
}

void CnidTable::clear()
{
	CACHE_LOG("clear: dropping %zu entries", forward_.size());
	forward_.clear();
	reverse_.clear();
	scanned_.clear();
	nextCnid_ = 16;
}

uint32_t CnidTable::nextCnid() const
{
	return nextCnid_;
}

bool CnidTable::isScanned(uint32_t dirID) const
{
	return scanned_.contains(dirID);
}
void CnidTable::markScanned(uint32_t dirID)
{
	scanned_.insert(dirID);
}
void CnidTable::clearScannedFor(uint32_t dirID)
{
	scanned_.erase(dirID);
}
void CnidTable::clearScanned()
{
	scanned_.clear();
}

std::size_t CnidTable::size() const
{
	return forward_.size();
}

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
	openForks_.clear();
	nextHandle_ = 1;
	textStats_ = {};

	/* Load the global default mapping (once per process) */
	static bool s_typesLoaded = false;
	if (!s_typesLoaded)
	{
		appledouble::LoadTypeMappings("data/debug/typemap.def");
		s_typesLoaded = true;
	}

	/* Per-volume mapping from .maxivmac/typemap.def, else global default */
	auto volumeMap = hostDir / ".maxivmac" / "typemap.def";
	if (typeMap_.load(volumeMap) < 0) typeMap_ = appledouble::DefaultTypeMap();

	// Initialize CNID table.  Register root directory (CNID 2).
	cnidTable_.clear();
	cnidTable_.resolve(kRootParentID, rootPath_.filename().string(), rootPath_.string());

	// Scan root directory only — subdirectories are scanned lazily
	// when the guest accesses them.
	ensureScanned(kRootDirID);

	CACHE_LOG("mount: root=\"%s\" rootChildren=%d cnids=%zu", rootPath_.string().c_str(),
			  childCount(kRootDirID), cnidTable_.size());

	/* Load root directory Finder info from sidecar */
	appledouble::GetDirFinderInfo(hostDir, rootDirFinderInfo_, 32);

	mounted_ = true;
	return true;
}

bool HostVolume::isMounted() const
{
	return mounted_;
}

void HostVolume::setVirtualIcon(std::vector<uint8_t> rsrcFork)
{
	/* Remove any existing virtual Icon\r entry */
	std::erase_if(catalog_, [](const CatalogEntry &e) { return e.isVirtual; });
	virtualIconFork_ = std::move(rsrcFork);

	if (virtualIconFork_.empty())
	{
		printf("[VIcon] setVirtualIcon: empty fork, skipping\n");
		return;
	}

	CatalogEntry ce{};
	ce.cnid = cnidTable_.resolve(kRootDirID, "Icon\r", "");
	ce.parentDirID = kRootDirID;
	ce.isDirectory = false;
	ce.macName = std::string("Icon\r");
	ce.type = 0;			 /* no type */
	ce.creator = 0;			 /* no creator */
	ce.finderFlags = 0x4000; /* kIsInvisible */
	ce.dataForkSize = 0;
	ce.rsrcForkSize = static_cast<uint32_t>(virtualIconFork_.size());
	ce.isVirtual = true;
	catalog_.push_back(std::move(ce));
	printf("[VIcon] virtual Icon\\r entry: cnid=%u rsrc=%u bytes, macName=%zu chars: ",
		   catalog_.back().cnid, static_cast<unsigned>(virtualIconFork_.size()),
		   catalog_.back().macName.size());
	for (size_t i = 0; i < catalog_.back().macName.size(); ++i)
		printf("%02X ", (unsigned char)catalog_.back().macName[i]);
	printf("\n");
}

/* ── Slot identity ────────────────────────────────── */

void HostVolume::setSlot(int slot)
{
	slot_ = slot;
}

int HostVolume::slot() const
{
	return slot_;
}

int16_t HostVolume::guestVRefNum() const
{
	return static_cast<int16_t>(-(kBaseVRefNum + slot_));
}

int16_t HostVolume::guestDriveNum() const
{
	return static_cast<int16_t>(kBaseDriveNum + slot_);
}

void HostVolume::closeAllForks()
{
	for (auto &[handle, of] : openForks_)
	{
		if (of.fp)
		{
			std::fclose(of.fp);
			of.fp = nullptr;
		}
	}
	openForks_.clear();
}

/* ── Catalog queries ──────────────────────────────── */

const CatalogEntry *HostVolume::findByCNID(uint32_t cnid)
{
	// Fast path: entry is in catalog
	for (const auto &e : catalog_)
		if (e.cnid == cnid) return &e;

	// Resurrection: entry was evicted or never scanned.
	// Look up identity from cnidTable to find its parent, then
	// scan that parent directory to re-populate.
	auto *val = cnidTable_.reverse(cnid);
	if (!val) return nullptr;

	CACHE_LOG("findByCNID: resurrecting cnid=%u parent=%u name=\"%s\"", cnid, val->key.parentDirID,
			  val->key.macName.c_str());

	ensureScanned(val->key.parentDirID);

	// Retry after scanning
	for (const auto &e : catalog_)
		if (e.cnid == cnid) return &e;

	CACHE_LOG("findByCNID: resurrection failed cnid=%u (deleted from disk)", cnid);
	return nullptr;
}

const CatalogEntry *HostVolume::findByName(uint32_t parentDirID, std::string_view macName)
{
	ensureScanned(parentDirID);
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

const CatalogEntry *HostVolume::findByPath(uint32_t startDirID, std::string_view hfsPath)
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

const CatalogEntry *HostVolume::nthChild(uint32_t parentDirID, int index)
{
	ensureScanned(parentDirID);
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

int HostVolume::childCount(uint32_t parentDirID)
{
	ensureScanned(parentDirID);
	int count = 0;
	for (const auto &e : catalog_)
		if (e.parentDirID == parentDirID) ++count;
	return count;
}

void HostVolume::volumeStats(uint32_t &outFiles, uint32_t &outDirs, uint32_t &outBytes)
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
	ce.cnid = cnidTable_.resolve(parentDirID, macName, hostPath);
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
	ce.cnid = cnidTable_.resolve(parentDirID, macName, hostPath);
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

	// Keep CnidTable in sync so that re-scans after invalidation
	// match the new location to the existing CNID.
	cnidTable_.updateKey(cnid, dstDirID, macName, newHostPath);
	CACHE_LOG("move: cnid=%u \"%s\" dir=%u -> dir=%u", cnid, std::string(macName).c_str(), srcDirID,
			  dstDirID);

	// For directory moves, update all descendants' host paths in cnidTable.
	if (isDir)
	{
		uint32_t descendantCount = 0;
		for (const auto &entry : catalog_)
		{
			if (entry.hostPath.size() > newHostPath.size() &&
				entry.hostPath.compare(0, newHostPath.size(), newHostPath) == 0 &&
				entry.hostPath[newHostPath.size()] == '/')
			{
				cnidTable_.updateHostPath(entry.cnid, entry.hostPath);
				++descendantCount;
			}
		}
		if (descendantCount > 0)
			CACHE_LOG("move: updated %u descendant hostPaths", descendantCount);
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

	// Keep CnidTable in sync so that re-scans after invalidation
	// match the new name to the existing CNID.
	cnidTable_.updateKey(cnid, dirID, newMacName, newHostPath);
	CACHE_LOG("rename: cnid=%u \"%s\" -> \"%s\" in dir=%u", cnid, std::string(oldMacName).c_str(),
			  std::string(newMacName).c_str(), dirID);

	// For directory renames, update all descendants' host paths in cnidTable.
	if (isDir)
	{
		uint32_t descendantCount = 0;
		for (const auto &entry : catalog_)
		{
			if (entry.hostPath.size() > newHostPath.size() &&
				entry.hostPath.compare(0, newHostPath.size(), newHostPath) == 0 &&
				entry.hostPath[newHostPath.size()] == '/')
			{
				cnidTable_.updateHostPath(entry.cnid, entry.hostPath);
				++descendantCount;
			}
		}
		if (descendantCount > 0)
			CACHE_LOG("rename: updated %u descendant hostPaths", descendantCount);
	}

	return kNoErr;
}

/* ── Metadata ─────────────────────────────────────── */

OSErr HostVolume::setFileInfo(uint32_t cnid, uint32_t type, uint32_t creator, uint16_t flags,
							  uint32_t location, uint16_t folder)
{
	CatalogEntry *e = mutableFindByCNID(cnid);
	if (!e || e->isDirectory) return kFnfErr;

	appledouble::SetFinderInfo(e->hostPath, {type, creator, flags, location, folder}, typeMap_);
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
							std::array<uint8_t, 16> &dxinfo)
{
	if (cnid == kRootDirID)
	{
		std::memcpy(dinfo.data(), rootDirFinderInfo_, 16);
		std::memcpy(dxinfo.data(), rootDirFinderInfo_ + 16, 16);
		return true;
	}
	const CatalogEntry *e = findByCNID(cnid);
	if (!e || !e->isDirectory) return false;
	std::memcpy(dinfo.data(), e->dirFinderInfo, 16);
	std::memcpy(dxinfo.data(), e->dirFinderInfo + 16, 16);
	return true;
}

OSErr HostVolume::setDirInfo(uint32_t cnid, const std::array<uint8_t, 16> &dinfo,
							 const std::array<uint8_t, 16> &dxinfo)
{
	if (cnid == kRootDirID)
	{
		std::memcpy(rootDirFinderInfo_, dinfo.data(), 16);
		std::memcpy(rootDirFinderInfo_ + 16, dxinfo.data(), 16);
		appledouble::SetDirFinderInfo(rootPath_, rootDirFinderInfo_, 32);
		return kNoErr;
	}
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
		if (of.cnid != cnid || of.fork != fork) continue;
		/* Exclusive open conflicts with any existing open of the same fork */
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

	if (e->isVirtual)
		printf("[VIcon] openFork cnid=%u fork=%s perm=%d\n", cnid,
			   fork == ForkType::Data ? "data" : "rsrc", permission);

	if (fork == ForkType::Data)
	{
		if (e->isVirtual)
		{
			/* Virtual entry: data fork is empty */
			outSize = 0;
			openForks_[handle] = {cnid, ForkType::Data, nullptr, false, {}, false};
			errOut = kNoErr;
			return handle;
		}

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

		openForks_[handle] = {cnid, ForkType::Data, fp, wantWrite, e->hostPath, e->isText};
	}
	else
	{
		/* Resource fork: no FILE*, handled by AppleDouble library or virtual data */
		openForks_[handle] = {cnid, ForkType::Resource, nullptr, wantWrite, e->hostPath, e->isText};
		if (e->isVirtual)
			outSize = static_cast<uint32_t>(virtualIconFork_.size());
		else
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

	// Entry may be null if evicted between open and read.
	// Fork I/O uses of.hostPath/of.isText copied at open time.
	const std::string &hostPath = e ? e->hostPath : of.hostPath;
	bool isText = e ? e->isText : of.isText;
	bool isVirtual = e && e->isVirtual;

	if (!e && !of.hostPath.empty())
		CACHE_LOG("readFork: cnid=%u evicted, using of.hostPath", of.cnid);

	if (of.fork == ForkType::Resource)
	{
		if (isVirtual)
		{
			printf("[VIcon] readFork virtual: off=%u req=%zu\n", offset, buf.size());
			uint32_t available = (offset < virtualIconFork_.size())
									 ? static_cast<uint32_t>(virtualIconFork_.size() - offset)
									 : 0;
			uint32_t toRead = std::min(static_cast<uint32_t>(buf.size()), available);
			std::memcpy(buf.data(), virtualIconFork_.data() + offset, toRead);
			outRead = toRead;
			return kNoErr;
		}

		auto data =
			appledouble::ReadResourceFork(hostPath, offset, static_cast<uint32_t>(buf.size()));
		uint32_t toRead = static_cast<uint32_t>(data.size());
		std::memcpy(buf.data(), data.data(), toRead);
		outRead = toRead;
		return kNoErr;
	}

	if (isText)
	{
		auto converted = appledouble::MacRomanFromUTF8File(hostPath);

		if (e)
		{
			std::error_code ec;
			textStats_.conversions++;
			textStats_.bytesIn += fs::file_size(hostPath, ec);
			textStats_.bytesOut += converted.size();
		}

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

	// Entry may be null if evicted between open and write.
	// Fork I/O uses of.hostPath/of.isText copied at open time.
	const std::string &hostPath = e ? e->hostPath : of.hostPath;
	bool isText = e ? e->isText : of.isText;
	bool isVirtual = e && e->isVirtual;

	if (!e && !of.hostPath.empty())
		CACHE_LOG("writeFork: cnid=%u evicted, using of.hostPath", of.cnid);

	if (of.fork == ForkType::Resource)
	{
		if (isVirtual)
		{
			outWritten = 0;
			return kWPrErr;
		}

		appledouble::WriteResourceFork(hostPath, offset, data);
		if (e)
		{
			e->rsrcForkSize = appledouble::ResourceForkSize(hostPath);
			e->modDate = currentMacDate();
		}
		outWritten = static_cast<uint32_t>(data.size());
		return kNoErr;
	}

	if (isText)
	{
		auto existing = appledouble::MacRomanFromUTF8File(hostPath);
		if (offset + data.size() > existing.size()) existing.resize(offset + data.size());
		std::memcpy(existing.data() + offset, data.data(), data.size());
		appledouble::UTF8FileFromMacRoman(hostPath, existing);
		outWritten = static_cast<uint32_t>(data.size());
		if (e)
		{
			e->dataForkSize = static_cast<uint32_t>(existing.size());
			e->modDate = currentMacDate();
		}
		return kNoErr;
	}

	/* Non-TEXT data fork */
	FILE *fp = of.fp;
	fseek(fp, static_cast<long>(offset), SEEK_SET);
	size_t wrote = fwrite(data.data(), 1, data.size(), fp);
	fflush(fp);

	if (e)
	{
		fseek(fp, 0, SEEK_END);
		e->dataForkSize = static_cast<uint32_t>(ftell(fp));
		e->modDate = currentMacDate();
	}
	outWritten = static_cast<uint32_t>(wrote);
	return kNoErr;
}

OSErr HostVolume::setEOF(uint32_t handle, uint32_t newSize)
{
	auto it = openForks_.find(handle);
	if (it == openForks_.end()) return kRfNumErr;

	const OpenFork &of = it->second;
	CatalogEntry *e = mutableFindByCNID(of.cnid);

	// Entry may be null if evicted between open and setEOF.
	const std::string &hostPath = e ? e->hostPath : of.hostPath;
	bool isVirtual = e && e->isVirtual;

	if (!e && !of.hostPath.empty())
		CACHE_LOG("setEOF: cnid=%u evicted, using of.hostPath", of.cnid);

	if (of.fork == ForkType::Resource)
	{
		if (isVirtual) return kWPrErr;

		appledouble::SetResourceForkSize(hostPath, newSize, typeMap_);
		if (e) e->rsrcForkSize = newSize;
	}
	else if (of.fp)
	{
		fflush(of.fp);
		int fd = fileno(of.fp);
		if (fd < 0 || ftruncate(fd, static_cast<off_t>(newSize)) != 0)
		{
			DIAG(ExtFS, "ftruncate failed for cnid=%u newSize=%u: %s\n", of.cnid, newSize,
				 strerror(errno));
			return kIoErr;
		}
		if (e) e->dataForkSize = newSize;
	}
	if (e) e->modDate = currentMacDate();
	return kNoErr;
}

void HostVolume::closeFork(uint32_t handle)
{
	auto it = openForks_.find(handle);
	if (it == openForks_.end()) return;
	if (it->second.fp) fclose(it->second.fp);
	openForks_.erase(it);
}

/* ── Cache invalidation ──────────────────────────── */

void HostVolume::invalidateDir(uint32_t dirID)
{
	// Collect CNIDs with open forks — these entries must survive eviction.
	std::unordered_set<uint32_t> openCnids;
	for (auto &[_, of] : openForks_)
		openCnids.insert(of.cnid);

	auto sizeBefore = catalog_.size();
	uint32_t retained = 0;

	std::erase_if(catalog_,
				  [&](const CatalogEntry &e)
				  {
					  if (e.parentDirID != dirID) return false;
					  if (e.isVirtual)
					  {
						  ++retained;
						  return false;
					  }
					  if (openCnids.contains(e.cnid))
					  {
						  ++retained;
						  return false;
					  }
					  return true;
				  });

	cnidTable_.clearScannedFor(dirID);

	uint32_t evicted = static_cast<uint32_t>(sizeBefore - catalog_.size());
	CACHE_LOG("invalidateDir: dir=%u evicted=%u retained=%u catalogSize=%zu", dirID, evicted,
			  retained, catalog_.size());
}

void HostVolume::invalidateAll()
{
	// Collect CNIDs with open forks — these entries must survive eviction.
	std::unordered_set<uint32_t> openCnids;
	for (auto &[_, of] : openForks_)
		openCnids.insert(of.cnid);

	auto sizeBefore = catalog_.size();
	uint32_t retained = 0;

	std::erase_if(catalog_,
				  [&](const CatalogEntry &e)
				  {
					  if (e.isVirtual)
					  {
						  ++retained;
						  return false;
					  }
					  if (openCnids.contains(e.cnid))
					  {
						  ++retained;
						  return false;
					  }
					  return true;
				  });

	cnidTable_.clearScanned();

	uint32_t evicted = static_cast<uint32_t>(sizeBefore - catalog_.size());
	CACHE_LOG("invalidateAll: evicted=%u retained=%u openForks=%zu cnids=%zu", evicted, retained,
			  openForks_.size(), cnidTable_.size());
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

CatalogEntry HostVolume::buildCatalogEntry(const fs::directory_entry &entry, uint32_t parentDirID,
										   uint32_t cnid, std::string_view macName)
{
	std::error_code ec;
	CatalogEntry ce{};
	ce.cnid = cnid;
	ce.parentDirID = parentDirID;
	ce.hostPath = entry.path().string();
	ce.macName = std::string(macName);

	if (entry.is_directory(ec))
	{
		ce.isDirectory = true;
		auto ftime = fs::last_write_time(entry.path(), ec);
		ce.crDate = ec ? 0 : appledouble::MacDateFromFileTime(ftime);
		ce.modDate = ce.crDate;
		appledouble::GetDirFinderInfo(entry.path(), ce.dirFinderInfo, 32);
	}
	else if (entry.is_regular_file(ec))
	{
		auto info = appledouble::GetFileInfo(entry.path(), typeMap_);
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
	}
	return ce;
}

/* Lazily populate catalog entries for children of dirID.
   Short-circuits when the directory was already scanned this invalidation
   cycle (tracked by cnidTable_.scanned_).  On the first call for a given
   dirID, walks the host directory with directory_iterator, resolving
   CNIDs for every child via cnidTable_.  Existing catalog entries are
   updated with fresh metadata; new entries are appended.  Children
   whose host file has been deleted since the last scan are pruned. */
void HostVolume::ensureScanned(uint32_t dirID)
{
	// Already scanned this invalidation cycle — nothing to do.
	if (cnidTable_.isScanned(dirID)) return;

	// Resolve the host path for this directory.
	std::string hostPath;
	if (dirID == kRootDirID)
		hostPath = rootPath_.string();
	else
	{
		auto *val = cnidTable_.reverse(dirID);
		if (!val) return;
		hostPath = val->hostPath;
	}

	// Bail if directory no longer exists on disk.
	std::error_code ec;
	if (hostPath.empty() || !fs::is_directory(hostPath, ec)) return;

	uint32_t added = 0, updated = 0, pruned = 0;

	// Walk one level of the host directory.
	for (const auto &entry : fs::directory_iterator(hostPath, ec))
	{
		if (ec) break;
		std::string name = entry.path().filename().string();
		if (name.empty() || name[0] == '.') continue;
		if (appledouble::IsSidecar(name)) continue;

		auto macName = appledouble::MacNameFromHost(name);
		if (!macName)
		{
			DIAG(ExtFS, "skipping '%s': not representable in MacRoman\n", name.c_str());
			continue;
		}
		if (macName->size() > 31) *macName = macName->substr(0, 31);

		uint32_t cnid = cnidTable_.resolve(dirID, *macName, entry.path().string());

		// Already in catalog?  Update metadata in place.
		CatalogEntry *existing = nullptr;
		for (auto &ce : catalog_)
		{
			if (ce.cnid == cnid)
			{
				existing = &ce;
				break;
			}
		}

		if (existing)
		{
			// Refresh metadata from disk (sizes, dates, Finder info)
			if (entry.is_regular_file(ec))
			{
				auto info = appledouble::GetFileInfo(entry.path(), typeMap_);
				existing->dataForkSize = info.dataForkSize;
				existing->rsrcForkSize = info.rsrcForkSize;
				existing->modDate = info.modDate;
				existing->type = info.finder.type;
				existing->creator = info.finder.creator;
				existing->finderFlags = info.finder.flags;
				existing->isText = info.isText;
			}
			else if (entry.is_directory(ec))
			{
				auto ftime = fs::last_write_time(entry.path(), ec);
				existing->modDate =
					ec ? existing->modDate : appledouble::MacDateFromFileTime(ftime);
			}
			existing->hostPath = entry.path().string();
			++updated;
			continue;
		}

		// New entry: build from disk and append.
		catalog_.push_back(buildCatalogEntry(entry, dirID, cnid, *macName));
		++added;
	}

	// Prune deleted: remove entries whose parent is dirID but whose
	// host file no longer exists on disk.
	auto before = catalog_.size();
	std::erase_if(catalog_,
				  [&](const CatalogEntry &e)
				  {
					  if (e.parentDirID != dirID || e.isVirtual) return false;
					  if (!fs::exists(e.hostPath, ec))
					  {
						  CACHE_LOG("ensureScanned: pruned cnid=%u name=\"%s\"", e.cnid,
									e.macName.c_str());
						  return true;
					  }
					  return false;
				  });
	pruned = static_cast<uint32_t>(before - catalog_.size());

	// Mark as scanned so subsequent calls this cycle short-circuit.
	cnidTable_.markScanned(dirID);

	CACHE_LOG("ensureScanned: dir=%u added=%u updated=%u pruned=%u total=%zu", dirID, added,
			  updated, pruned, catalog_.size());
}

uint32_t HostVolume::nextCnid() const
{
	return cnidTable_.nextCnid();
}

CatalogEntry *HostVolume::mutableFindByCNID(uint32_t cnid)
{
	for (auto &e : catalog_)
		if (e.cnid == cnid) return &e;

	// Resurrection: entry was evicted or never scanned.
	auto *val = cnidTable_.reverse(cnid);
	if (!val) return nullptr;

	CACHE_LOG("mutableFindByCNID: resurrecting cnid=%u parent=%u name=\"%s\"", cnid,
			  val->key.parentDirID, val->key.macName.c_str());

	ensureScanned(val->key.parentDirID);

	for (auto &e : catalog_)
		if (e.cnid == cnid) return &e;

	CACHE_LOG("mutableFindByCNID: resurrection failed cnid=%u (deleted from disk)", cnid);
	return nullptr;
}

std::string HostVolume::resolveParentPath(uint32_t parentDirID) const
{
	if (parentDirID == kRootDirID) return rootPath_.string();
	auto *val = cnidTable_.reverse(parentDirID);
	return val ? val->hostPath : std::string{};
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
			DIAG(ExtFS, "MISSING: cnid=%u parent=%u \"%s\" -> %s\n", e.cnid, e.parentDirID,
				 e.macName.c_str(), e.hostPath.c_str());
			ok = false;
			continue;
		}

		/* Check 2: directory flag must match */
		bool isDir = fs::is_directory(e.hostPath, ec);
		if (isDir != e.isDirectory)
		{
			DIAG(ExtFS, "TYPE MISMATCH: cnid=%u \"%s\" catalog=%s disk=%s\n", e.cnid,
				 e.macName.c_str(), e.isDirectory ? "dir" : "file", isDir ? "dir" : "file");
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
						path_str(actualParent).c_str());
				ok = false;
			}
		}
	}
	return ok;
}

} // namespace storage
