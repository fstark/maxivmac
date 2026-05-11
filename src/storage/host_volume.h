/*
	HostVolume — virtual HFS volume backed by a host filesystem directory

	Bridges Classic Mac file semantics (resource forks, MacRoman filenames,
	TEXT encoding, Catalog Node IDs) onto modern host filesystems.  Files
	are stored natively on disk; Finder metadata and resource forks live in
	AppleDouble sidecar files (._<name>).  TEXT-type files are stored as
	UTF-8 on the host but appear as MacRoman to the guest Mac OS.

	The volume uses a lazy, demand-populated catalog cache backed by a
	persistent CnidTable identity map.  Only directories the guest actually
	visits are scanned.  Periodic invalidation forces re-scan from disk,
	picking up external changes.
*/
#pragma once

#include "storage/appledouble.h"
#include "storage/drive_constants.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace storage
{

/* ── Mac OS result codes (IM II-308, IV-226) ──────── */
/*
	Standard OSErr values — negative, per Inside Macintosh convention.
	Used throughout the storage layer and returned directly to the guest.
*/
using OSErr = int16_t;

inline constexpr OSErr kNoErr = 0;
inline constexpr OSErr kNsvErr = -35;	 /* no such volume */
inline constexpr OSErr kIoErr = -36;	 /* I/O error */
inline constexpr OSErr kFnfErr = -43;	 /* file not found */
inline constexpr OSErr kWPrErr = -44;	 /* volume is write-protected */
inline constexpr OSErr kFBsyErr = -47;	 /* file busy (dir not empty) */
inline constexpr OSErr kDupFNErr = -48;	 /* duplicate filename */
inline constexpr OSErr kOpWrErr = -49;	 /* file already open for write */
inline constexpr OSErr kParamErr = -50;	 /* parameter error */
inline constexpr OSErr kRfNumErr = -51;	 /* bad refnum */
inline constexpr OSErr kDirNFErr = -120; /* directory not found */

/* ── Catalog entry ────────────────────────────────── */

/*
	One node in the in-memory catalog tree.  Every file and directory on
	the mounted volume gets a CatalogEntry, created either during the
	initial mount scan or by createFile()/createDir() at runtime.

	Each entry carries a CNID — a Catalog Node ID.  CNIDs are sequential
	32-bit identifiers (starting at 16) that uniquely tag an entry for
	its entire lifetime.  They are never reused within a mount session.
	CNID 1 is the root's parent (an HFS convention); CNID 2 is the root
	directory itself.

	Metadata (type, creator, resource-fork size, dates) is read from the
	AppleDouble sidecar at scan time and kept in sync by fork I/O and
	setFileInfo().  For TEXT-type files, dataForkSize reflects the
	MacRoman byte count the guest will see, not the UTF-8 size on disk.

	Entries are deleted by remove() and erased from the catalog vector.
*/
struct CatalogEntry
{
	uint32_t cnid = 0;		  // unique catalog node ID, assigned once
	uint32_t parentDirID = 0; // CNID of the parent directory
	bool isDirectory = false;
	std::string hostPath; // absolute host path
	std::string macName;  // Mac OS Roman, ≤31 bytes

	/* Cached metadata (populated from AppleDouble at scan time) */
	uint32_t type = 0;	  // Finder file type    (e.g. 'TEXT', 'APPL')
	uint32_t creator = 0; // Finder creator code  (e.g. 'ttxt', 'MSWD')
	uint16_t finderFlags = 0;
	uint32_t fdLocation = 0;   // Finder fdLocation (Point, encoded as uint32)
	uint16_t fdFldr = 0;	   // Finder fdFldr (window ID)
	uint32_t dataForkSize = 0; // Mac-visible size (MacRoman for TEXT)
	uint32_t rsrcForkSize = 0; // from AppleDouble sidecar
	uint32_t crDate = 0;	   // creation date, Mac epoch (seconds since 1904)
	uint32_t modDate = 0;	   // modification date, Mac epoch
	bool isText = false;	   // true when type == 'TEXT'; enables encoding conversion
	bool isVirtual = false;	   // true for synthesized entries (e.g. virtual Icon\r)

	/* Directory Finder info: DInfo (16 bytes) + DXInfo (16 bytes).
	   Only meaningful when isDirectory is true.  Stored in-memory and
	   persisted to the AppleDouble sidecar for the directory.  Contains
	   frRect, frFlags, frLocation, frView, frScroll, etc. */
	uint8_t dirFinderInfo[32] = {};
};

/* ── Open fork descriptor ─────────────────────────── */

// Classic Mac files have two forks: a data fork (arbitrary bytes) and a
// resource fork (structured resources).  Resource forks are stored in
// AppleDouble sidecar files on the host filesystem.
enum class ForkType
{
	Data,
	Resource
};

/* ── Sidecar sync mode ────────────────────────────── */

// Controls what metadata is persisted to AppleDouble sidecar files.
// Future modes may limit syncing to type/creator only, etc.
enum class SidecarMode
{
	Full // sync all Finder info (type, creator, flags, location, folder)
};

/* ── CNID identity table ──────────────────────────── */

// Identity key for the CNID table.  Matches Mac filesystem semantics:
// (parentDirID, macName) identifies a catalog node.  Comparison and
// hashing are case-insensitive (MacRoman folding) because Mac filenames
// are case-insensitive.
struct CnidKey
{
	uint32_t parentDirID = 0;
	std::string macName; // case-preserved Mac name

	bool operator==(const CnidKey &o) const;
};

// Case-insensitive hash for CnidKey.
// Combines parentDirID with a case-folded FNV-1a of macName.
struct CnidKeyHash
{
	std::size_t operator()(const CnidKey &k) const;
};

// Reverse-lookup value: stores identity and hostPath for a CNID.
struct CnidValue
{
	CnidKey key;		  // (parentDirID, macName) that owns this CNID
	std::string hostPath; // absolute host path at creation time
};

// Bidirectional identity map: (parentDirID, macName) ↔ CNID.
// Only grows during a mount session — CNIDs are never freed or
// reused.  Cleared on mount().
class CnidTable
{
public:
	// Look up an existing CNID for (parentDirID, macName), or
	// allocate a new one.  hostPath is stored for reverse lookup;
	// ignored if the CNID already exists.
	uint32_t resolve(uint32_t parentDirID, std::string_view macName, std::string_view hostPath);

	// Reverse lookup: CNID → (parentDirID, macName, hostPath).
	// Returns nullptr if cnid was never assigned.
	const CnidValue *reverse(uint32_t cnid) const;

	// Update identity after a guest rename or move.  Changes the
	// forward key and stored hostPath for an existing CNID.
	void updateKey(uint32_t cnid, uint32_t newParentDirID, std::string_view newMacName,
				   std::string_view newHostPath);

	// Update hostPath only (used after parent dir move/rename to
	// fix descendant paths).
	void updateHostPath(uint32_t cnid, std::string_view newHostPath);

	void clear();			   // mount-time reset
	uint32_t nextCnid() const; // next CNID that will be assigned

	// Directory scan tracking: prevents re-running directory_iterator
	// for the same parent within one invalidation cycle.
	bool isScanned(uint32_t dirID) const;
	void markScanned(uint32_t dirID);
	void clearScannedFor(uint32_t dirID); // single-directory invalidation
	void clearScanned();				  // called by invalidateAll()

	// Size of internal maps (for testing / diagnostics).
	std::size_t size() const;

private:
	std::unordered_map<CnidKey, uint32_t, CnidKeyHash> forward_;
	std::unordered_map<uint32_t, CnidValue> reverse_;
	std::unordered_set<uint32_t> scanned_;

	// HFS reserves CNIDs 1–15 (root parent, root dir, extents
	// overflow, catalog file, etc.).
	uint32_t nextCnid_ = 16;
};

/* ── HostVolume ───────────────────────────────────── */

/*
	A single virtual HFS volume rooted at a host directory.

	On mount(), the root directory is scanned to build an initial catalog.
	Subdirectories are scanned lazily when the guest accesses them.  A
	CnidTable maps (parentDirID, macName) ↔ CNID persistently within a
	mount session — CNIDs are stable even across cache invalidation.
	Periodic invalidation evicts cached entries; the next guest access
	re-scans from disk, picking up external additions and deletions.

	Fork I/O is handle-based: openFork() returns a uint32_t handle that
	subsequent read/write/setEOF/close calls operate on.  Data forks
	map to real files on disk; resource forks map to AppleDouble sidecars.
*/
class HostVolume
{
public:
	// Scan hostDir recursively and build the catalog.  Clears any
	// previous mount state.  Returns false if hostDir does not exist.
	bool mount(const std::filesystem::path &hostDir);
	bool isMounted() const;

	// Inject a virtual Icon\r entry into the catalog with the given
	// resource fork data.  Called after mount if no real Icon\r exists.
	void setVirtualIcon(std::vector<uint8_t> rsrcFork);

	// True if a virtual Icon\r is active (no real one on disk).
	bool hasVirtualIcon() const { return !virtualIconFork_.empty(); }

	static constexpr uint32_t kRootParentID = 1; // HFS convention: root's parent
	static constexpr uint32_t kRootDirID = 2;	 // HFS convention: root directory

	/* Per-instance volume identity.  setSlot() is called by
	   DriveManager after construction to assign the slot index,
	   which determines vRefNum and driveNum. */
	void setSlot(int slot);
	int slot() const;
	int16_t guestVRefNum() const;  // -(kBaseVRefNum + slot_)
	int16_t guestDriveNum() const; // kBaseDriveNum + slot_

	// Close all open fork handles (used during unmount).
	void closeAllForks();

	// Host directory backing this volume.
	const std::filesystem::path &rootPath() const { return rootPath_; }

	// Number of currently open fork handles.
	int openForkCount() const { return static_cast<int>(openForks_.size()); }

	/* ── Catalog queries ──────────────────────────── */

	// Look up a catalog entry by its CNID.  Returns nullptr if not found.
	// Resurrects evicted entries via CnidTable reverse lookup.
	const CatalogEntry *findByCNID(uint32_t cnid);

	// Find a child of parentDirID with the given Mac name (case-insensitive).
	const CatalogEntry *findByName(uint32_t parentDirID, std::string_view macName);

	// Find a catalog entry by an HFS path string (e.g. "Volume:dir:file").
	// If the path contains no colon, behaves identically to findByName(startDirID, path).
	// A leading colon makes the path relative to startDirID.
	// Otherwise the first component is the volume name (skipped) and walking
	// begins from the root directory.
	const CatalogEntry *findByPath(uint32_t startDirID, std::string_view hfsPath);

	// Return the nth child (0-based) of a directory.  Used for catalog iteration.
	const CatalogEntry *nthChild(uint32_t parentDirID, int index);

	// Count immediate children of a directory.
	int childCount(uint32_t parentDirID);

	// Aggregate file count, directory count, and total data-fork bytes.
	void volumeStats(uint32_t &outFiles, uint32_t &outDirs, uint32_t &outBytes);

	// Next CNID that will be assigned.
	uint32_t nextCnid() const;

	/* ── File/directory creation ──────────────────── */

	// Create an empty file in parentDirID.  Allocates a new CNID and
	// creates the file on the host filesystem.  Returns the CNID, or 0
	// on error (errOut set to kDupFNErr, kDirNFErr, etc.).
	uint32_t createFile(uint32_t parentDirID, std::string_view macName, OSErr &errOut);

	// Create a subdirectory.  Same return/error contract as createFile().
	uint32_t createDir(uint32_t parentDirID, std::string_view macName, OSErr &errOut);

	/* ── Deletion ─────────────────────────────────── */

	// Delete a file or empty directory.  Also removes its AppleDouble
	// sidecar.  Returns kFBsyErr for non-empty directories.
	OSErr remove(uint32_t parentDirID, std::string_view macName);

	/* ── Move / rename ────────────────────────────── */

	// Move a file or directory to a different parent.  Updates host paths
	// for the entry and all descendants.  Renames the sidecar too.
	OSErr move(uint32_t srcDirID, std::string_view macName, uint32_t dstDirID);

	// Rename a file or directory within its current parent.
	OSErr rename(uint32_t dirID, std::string_view oldMacName, std::string_view newMacName);

	/* ── Metadata ─────────────────────────────────── */

	// Update Finder type/creator/flags.  Writes through to the
	// AppleDouble sidecar.  Toggles isText and recalculates dataForkSize
	// if the type changes to/from 'TEXT'.
	OSErr setFileInfo(uint32_t cnid, uint32_t type, uint32_t creator, uint16_t flags,
					  uint32_t location = 0, uint16_t folder = 0);

	// Get directory Finder info (DInfo + DXInfo, 16 bytes each).
	// Returns false if cnid is not a directory.
	bool getDirInfo(uint32_t cnid, std::array<uint8_t, 16> &dinfo, std::array<uint8_t, 16> &dxinfo);

	// Set directory Finder info (DInfo + DXInfo, 16 bytes each).
	// Persists to AppleDouble sidecar.
	OSErr setDirInfo(uint32_t cnid, const std::array<uint8_t, 16> &dinfo,
					 const std::array<uint8_t, 16> &dxinfo);

	/* ── Fork I/O ─────────────────────────────────── */
	/*
		Handle-based I/O modelled after the Classic Mac File Manager.
		openFork() returns an opaque handle; all subsequent calls use it.

		Data forks map to the real host file.  Resource forks are read/
		written via AppleDouble sidecars (no host FILE* is held open).
		TEXT data forks are transparently converted between UTF-8 (on
		disk) and MacRoman (presented to the guest).
	*/

	// Open a fork and return a handle (or 0 on error).  outSize receives
	// the fork's current byte count (MacRoman size for TEXT data forks).
	uint32_t openFork(uint32_t cnid, ForkType fork, uint32_t &outSize, OSErr &errOut,
					  uint8_t permission = 0);

	// Read up to buf.size() bytes at offset.  outRead = bytes actually read.
	OSErr readFork(uint32_t handle, uint32_t offset, std::span<uint8_t> buf, uint32_t &outRead);

	// Write data at offset.  outWritten = bytes actually written.
	OSErr writeFork(uint32_t handle, uint32_t offset, std::span<const uint8_t> data,
					uint32_t &outWritten);

	// Truncate or extend the fork to newSize bytes.
	OSErr setEOF(uint32_t handle, uint32_t newSize);

	// Close the fork handle and release resources.
	void closeFork(uint32_t handle);

	/* ── Cache invalidation ───────────────────────── */

	// Evict cached children of dirID from catalog_.  Entries with
	// open forks or isVirtual are retained.  The next query for this
	// directory triggers re-scan from disk.
	void invalidateDir(uint32_t dirID);

	// Evict all non-virtual, non-open-fork entries from catalog_.
	// Clears the scanned set, forcing re-scan on next access.
	// Called periodically to sync with host filesystem changes.
	void invalidateAll();

	/* ── Catalog consistency ──────────────────────── */

	// Verify every catalog entry: host path exists, isDirectory matches
	// the filesystem, parentDirID chain is valid.  Returns false on any
	// inconsistency.
	bool validateCatalog() const;

	/* ── TEXT conversion stats ────────────────────── */

	// Counters for TEXT encoding conversions (MacRoman ↔ UTF-8).
	struct TextStats
	{
		uint64_t conversions = 0; // number of fork read/write ops converted
		uint64_t bytesIn = 0;	  // host (UTF-8) bytes consumed
		uint64_t bytesOut = 0;	  // guest (MacRoman) bytes produced
	};

	TextStats textConversionStats() const;
	void resetTextConversionStats();

private:
	std::filesystem::path rootPath_; // host directory backing this volume
	bool mounted_ = false;
	int slot_ = 0;						   // assigned by DriveManager::mount() via setSlot()
	std::vector<CatalogEntry> catalog_;	   // flat list; searched linearly by CNID or name
	uint8_t rootDirFinderInfo_[32] = {};   // DInfo(16) + DXInfo(16) for root dir
	std::vector<uint8_t> virtualIconFork_; // resource fork for virtual Icon\r
	[[maybe_unused]] SidecarMode sidecarMode_ = SidecarMode::Full;
	appledouble::TypeMap typeMap_; // per-volume extension→type mapping
	CnidTable cnidTable_;		   // CNID identity table — maps (parentDirID, macName) ↔ CNID

	// Per-open-fork state.  Data forks hold a FILE*; resource forks
	// have fp == nullptr (I/O goes through AppleDouble helpers).
	// hostPath and isText are copied from CatalogEntry at open time
	// so that fork I/O remains functional even if the catalog entry
	// is evicted by invalidation.
	struct OpenFork
	{
		uint32_t cnid = 0;
		ForkType fork = ForkType::Data;
		FILE *fp = nullptr;
		bool hasWrite = false;
		std::string hostPath; // absolute path, copied at open time
		bool isText = false;  // copied at open time
	};
	std::unordered_map<uint32_t, OpenFork> openForks_; // handle → OpenFork
	uint32_t nextHandle_ = 1;						   // monotonic handle allocator

	mutable TextStats textStats_; // mutable: updated by const-ish read paths

	// Lazily populate catalog entries for children of dirID.
	// Scans one directory level from the host filesystem.  Skips hidden
	// files and AppleDouble sidecars.  New entries get CNIDs from
	// cnidTable_; existing entries are updated in place.
	// Entries whose host file no longer exists are pruned.
	// Short-circuits if this directory was already scanned this
	// invalidation cycle (tracked by cnidTable_.scanned_).
	void ensureScanned(uint32_t dirID);

	// Build a CatalogEntry from a host filesystem entry.
	// Populates metadata (type, creator, sizes, dates) from the
	// AppleDouble sidecar and file stats.
	CatalogEntry buildCatalogEntry(const std::filesystem::directory_entry &entry,
								   uint32_t parentDirID, uint32_t cnid, std::string_view macName);

	CatalogEntry *mutableFindByCNID(uint32_t cnid);

	// Reconstruct the host path for a directory given its CNID.
	std::string resolveParentPath(uint32_t parentDirID) const;

	// Recalculate dataForkSize after the TEXT flag changes.  For TEXT
	// files this counts MacRoman characters; for binary files it reads
	// the raw filesystem size.
	void invalidateTextSize(CatalogEntry &entry);

	static uint32_t currentMacDate();
};

} // namespace storage
