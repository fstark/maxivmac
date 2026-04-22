/*
	HostVolume — virtual HFS volume backed by a host filesystem directory

	Bridges Classic Mac file semantics (resource forks, MacRoman filenames,
	TEXT encoding, Catalog Node IDs) onto modern host filesystems.  Files
	are stored natively on disk; Finder metadata and resource forks live in
	AppleDouble sidecar files (._<name>).  TEXT-type files are stored as
	UTF-8 on the host but appear as MacRoman to the guest Mac OS.

	The volume is populated by a recursive directory scan at mount time.
*/
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
	uint32_t dataForkSize = 0; // Mac-visible size (MacRoman for TEXT)
	uint32_t rsrcForkSize = 0; // from AppleDouble sidecar
	uint32_t crDate = 0;	   // creation date, Mac epoch (seconds since 1904)
	uint32_t modDate = 0;	   // modification date, Mac epoch
	bool isText = false;	   // true when type == 'TEXT'; enables encoding conversion

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

/* ── HostVolume ───────────────────────────────────── */

/*
	A single virtual HFS volume rooted at a host directory.

	On mount(), the host directory tree is scanned recursively to build an
	in-memory catalog of CatalogEntry nodes, each assigned a unique CNID.
	After mounting, callers can query the catalog, create/delete files and
	directories, perform fork-level I/O, and open working directories —
	mirroring the Classic Mac OS File Manager interface.

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

	static constexpr uint32_t kRootParentID = 1; // HFS convention: root's parent
	static constexpr uint32_t kRootDirID = 2;	 // HFS convention: root directory

	/* Guest-side volume identifiers.  The INIT uses these when
	   building the VCB and drive queue entry.  They must match
	   the #defines in macsrc/shareddrive/init.c. */
	static constexpr int16_t kGuestVRefNum = -32000;
	static constexpr int16_t kGuestDriveNum = 8;

	/* Resolve a guest (vRefNum, dirID) pair to a catalog dirID.
	   If rawDirID is non-zero, returns it directly.
	   Otherwise decodes vRefNum: our volume / drive → root,
	   WD refnum → wdToDirID lookup, 0 → root.
	   See SHAREDRIVE_DESIGN.md §3.4. */
	uint32_t resolveDir(int16_t vRefNum, uint32_t rawDirID) const;

	/* ── Catalog queries ──────────────────────────── */

	// Look up a catalog entry by its CNID.  Returns nullptr if not found.
	const CatalogEntry *findByCNID(uint32_t cnid) const;

	// Find a child of parentDirID with the given Mac name (case-insensitive).
	const CatalogEntry *findByName(uint32_t parentDirID, std::string_view macName) const;

	// Return the nth child (0-based) of a directory.  Used for catalog iteration.
	const CatalogEntry *nthChild(uint32_t parentDirID, int index) const;

	// Count immediate children of a directory.
	int childCount(uint32_t parentDirID) const;

	// Aggregate file count and total data-fork bytes for the whole volume.
	void volumeStats(uint32_t &outFiles, uint32_t &outBytes) const;

	/* ── File/directory creation ──────────────────── */

	// Create an empty file in parentDirID.  Allocates a new CNID and
	// creates the file on the host filesystem.  Returns the CNID, or 0
	// on error (errOut set to kDupFNErr, kDirNFErr, etc.).
	uint32_t createFile(uint32_t parentDirID, std::string_view macName, FMErr &errOut);

	// Create a subdirectory.  Same return/error contract as createFile().
	uint32_t createDir(uint32_t parentDirID, std::string_view macName, FMErr &errOut);

	/* ── Deletion ─────────────────────────────────── */

	// Delete a file or empty directory.  Also removes its AppleDouble
	// sidecar.  Returns kFBsyErr for non-empty directories.
	FMErr remove(uint32_t parentDirID, std::string_view macName);

	/* ── Move / rename ────────────────────────────── */

	// Move a file or directory to a different parent.  Updates host paths
	// for the entry and all descendants.  Renames the sidecar too.
	FMErr move(uint32_t srcDirID, std::string_view macName, uint32_t dstDirID);

	// Rename a file or directory within its current parent.
	FMErr rename(uint32_t dirID, std::string_view oldMacName, std::string_view newMacName);

	/* ── Metadata ─────────────────────────────────── */

	// Update Finder type/creator/flags.  Writes through to the
	// AppleDouble sidecar.  Toggles isText and recalculates dataForkSize
	// if the type changes to/from 'TEXT'.
	FMErr setFileInfo(uint32_t cnid, uint32_t type, uint32_t creator, uint16_t flags);

	// Get directory Finder info (DInfo + DXInfo, 32 bytes).
	// Returns false if cnid is not a directory.
	bool getDirInfo(uint32_t cnid, uint8_t outBuf[32]) const;

	// Set directory Finder info (DInfo + DXInfo, 32 bytes).
	// Persists to AppleDouble sidecar.
	FMErr setDirInfo(uint32_t cnid, const uint8_t buf[32]);

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
	uint32_t openFork(uint32_t cnid, ForkType fork, uint32_t &outSize, FMErr &errOut);

	// Read up to buf.size() bytes at offset.  outRead = bytes actually read.
	FMErr readFork(uint32_t handle, uint32_t offset, std::span<uint8_t> buf, uint32_t &outRead);

	// Write data at offset.  outWritten = bytes actually written.
	FMErr writeFork(uint32_t handle, uint32_t offset, std::span<const uint8_t> data,
					uint32_t &outWritten);

	// Truncate or extend the fork to newSize bytes.
	FMErr setEOF(uint32_t handle, uint32_t newSize);

	// Close the fork handle and release resources.
	void closeFork(uint32_t handle);

	/* ── Working directories ──────────────────────── */
	/*
		Classic Mac "working directory" references — lightweight aliases
		for directory CNIDs.  Some old Mac apps and System calls use WD
		refs instead of raw dirIDs.  Each openWD() returns a new opaque
		ref that maps back to a dirID via wdToDirID().
	*/

	uint32_t openWD(uint32_t dirID);
	uint32_t wdToDirID(uint32_t wdRef) const;
	void closeWD(uint32_t wdRef);

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
	std::vector<CatalogEntry> catalog_; // flat list; searched linearly by CNID or name
	uint32_t nextCNID_ = 16;			// monotonic counter; 1-2 are reserved for root

	// Per-open-fork state.  Data forks hold a FILE*; resource forks
	// have fp == nullptr (I/O goes through AppleDouble helpers).
	struct OpenFork
	{
		uint32_t cnid = 0;
		ForkType fork = ForkType::Data;
		FILE *fp = nullptr;
	};
	std::unordered_map<uint32_t, OpenFork> openForks_; // handle → OpenFork
	uint32_t nextHandle_ = 1;						   // monotonic handle allocator

	std::unordered_map<uint32_t, uint32_t> wdTable_; // wdRef → dirID
	uint32_t nextWD_ = 1;

	mutable TextStats textStats_; // mutable: updated by const-ish read paths

	// Recursively walk hostDir, creating CatalogEntry nodes for every
	// file and subdirectory.  Skips hidden files and AppleDouble sidecars.
	void scanDirectory(const std::filesystem::path &hostDir, uint32_t parentDirID);

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
