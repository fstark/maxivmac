#pragma once

#include "storage/appledouble.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace storage
{

/* ── Error codes (match Mac OS File Manager) ──────── */

enum class FMErr : int16_t
{
	kNoErr = 0,
	kFnfErr = -43,	  // file not found
	kDupFNErr = -48,  // duplicate filename
	kParamErr = -50,  // bad parameter
	kRfNumErr = -51,  // bad file reference number
	kIoErr = -36,	  // I/O error
	kDirNFErr = -120, // directory not found
	kFBsyErr = -47,	  // file busy (dir not empty)
	kWPrErr = -44,	  // volume locked (optional, future)
};

/* ── Catalog entry ────────────────────────────────── */

struct CatalogEntry
{
	uint32_t cnid = 0;
	uint32_t parentDirID = 0;
	bool isDirectory = false;
	std::string hostPath; // absolute host path
	std::string macName;  // Mac OS Roman, ≤31 bytes

	/* Cached metadata (populated from AppleDouble at scan time) */
	uint32_t type = 0;
	uint32_t creator = 0;
	uint16_t finderFlags = 0;
	uint32_t dataForkSize = 0; // Mac-visible size (converted for TEXT)
	uint32_t rsrcForkSize = 0;
	uint32_t crDate = 0;  // Mac epoch
	uint32_t modDate = 0; // Mac epoch
	bool isText = false;
};

/* ── Open fork descriptor ─────────────────────────── */

enum class ForkType
{
	Data,
	Resource
};

/* ── HostVolume ───────────────────────────────────── */

class HostVolume
{
public:
	bool mount(const std::filesystem::path &hostDir);
	bool isMounted() const;

	static constexpr uint32_t kRootParentID = 1;
	static constexpr uint32_t kRootDirID = 2;

	/* ── Catalog queries ──────────────────────────── */

	const CatalogEntry *findByCNID(uint32_t cnid) const;
	const CatalogEntry *findByName(uint32_t parentDirID, std::string_view macName) const;
	const CatalogEntry *nthChild(uint32_t parentDirID, int index) const;
	int childCount(uint32_t parentDirID) const;

	void volumeStats(uint32_t &outFiles, uint32_t &outBytes) const;

	/* ── File/directory creation ──────────────────── */

	uint32_t createFile(uint32_t parentDirID, std::string_view macName, FMErr &errOut);

	uint32_t createDir(uint32_t parentDirID, std::string_view macName, FMErr &errOut);

	/* ── Deletion ─────────────────────────────────── */

	FMErr remove(uint32_t parentDirID, std::string_view macName);

	/* ── Move / rename ────────────────────────────── */

	FMErr move(uint32_t srcDirID, std::string_view macName, uint32_t dstDirID);

	FMErr rename(uint32_t dirID, std::string_view oldMacName, std::string_view newMacName);

	/* ── Metadata ─────────────────────────────────── */

	FMErr setFileInfo(uint32_t cnid, uint32_t type, uint32_t creator);

	/* ── Fork I/O ─────────────────────────────────── */

	uint32_t openFork(uint32_t cnid, ForkType fork, uint32_t &outSize, FMErr &errOut);

	FMErr readFork(uint32_t handle, uint32_t offset, std::span<uint8_t> buf, uint32_t &outRead);

	FMErr writeFork(uint32_t handle, uint32_t offset, std::span<const uint8_t> data,
					uint32_t &outWritten);

	void closeFork(uint32_t handle);

	/* ── Working directories ──────────────────────── */

	uint32_t openWD(uint32_t dirID);
	uint32_t wdToDirID(uint32_t wdRef) const;
	void closeWD(uint32_t wdRef);

	/* ── TEXT conversion stats ────────────────────── */

	struct TextStats
	{
		uint64_t conversions = 0;
		uint64_t bytesIn = 0;
		uint64_t bytesOut = 0;
	};

	TextStats textConversionStats() const;
	void resetTextConversionStats();

private:
	std::filesystem::path rootPath_;
	bool mounted_ = false;
	std::vector<CatalogEntry> catalog_;
	uint32_t nextCNID_ = 16;

	struct OpenFork
	{
		uint32_t cnid = 0;
		ForkType fork = ForkType::Data;
		FILE *fp = nullptr;
	};
	std::unordered_map<uint32_t, OpenFork> openForks_;
	uint32_t nextHandle_ = 1;

	std::unordered_map<uint32_t, uint32_t> wdTable_;
	uint32_t nextWD_ = 0x8000;

	mutable TextStats textStats_;

	void scanDirectory(const std::filesystem::path &hostDir, uint32_t parentDirID);
	CatalogEntry *mutableFindByCNID(uint32_t cnid);
	std::string resolveParentPath(uint32_t parentDirID) const;
	void invalidateTextSize(CatalogEntry &entry);

	static uint32_t currentMacDate();
};

} // namespace storage
