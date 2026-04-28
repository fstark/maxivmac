/*
	ExtFS dispatch — host-side handler for the SharedDrive file system extension.

	The guest INIT patches File Manager traps and forwards operations here
	via a register-based RPC mechanism.  Catalog operations pass the guest
	PB (parameter block) pointer directly; I/O and utility commands use
	register slots.  PB field layouts follow Inside Macintosh IV ch. 25.
*/
#include "core/extn_extfs.h"
#include "core/extn_clip.h"
#include "core/extfs_log.h"
#include "core/diag.h"
#include "debugger/debugger.h"
#include "cpu/trap_counter.h"
#include "cpu/disasm.h"
#include "core/machine.h"
#include "platform/platform.h"
#include "storage/host_volume.h"
#include "storage/drive_manager.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>
#include <cstring>
#include <utility>

/* Guest RAM access */
extern uint8_t get_vm_byte(uint32_t addr);
extern void put_vm_byte(uint32_t addr, uint8_t b);

/* ── Command codes ────────────────────────────────── */

static constexpr uint16_t kExtFSVersion = 0x200;
static constexpr uint16_t kExtFSGetVol = 0x201;
static constexpr uint16_t kExtFSRead = 0x205;
static constexpr uint16_t kExtFSClose = 0x206;
static constexpr uint16_t kExtFSDbgLog = 0x20D;
static constexpr uint16_t kExtFSGuestVars = 0x20E;
static constexpr uint16_t kExtFSLogTrap = 0x20F;
static constexpr uint16_t kExtFSWrite = 0x211;
static constexpr uint16_t kExtFSFatal = 0x214;
static constexpr uint16_t kExtFSSetEOF = 0x218;
static constexpr uint16_t kExtFSPollMount = 0x219;
static constexpr uint16_t kExtFSGetVolName = 0x21A;

/* ── DriveManager instance ─────────────────────────── */

static storage::DriveManager s_drives;

static constexpr uint32_t kRootParentID = storage::HostVolume::kRootParentID;
static constexpr uint32_t kRootDirID = storage::HostVolume::kRootDirID;
static constexpr int16_t kGuestDriveNum = storage::kBaseDriveNum;

/* ── Mac OS result codes ──────────────────────────── */
/*
	Most error codes are defined in storage/host_volume.h and used
	directly — no translation layer needed.  kNsvErr is dispatch-only
	(no HostVolume operation returns it).
*/
using storage::kDirNFErr;
using storage::kDupFNErr;
using storage::kFBsyErr;
using storage::kFnfErr;
using storage::kIoErr;
using storage::kNoErr;
using storage::kOpWrErr;
using storage::kParamErr;
using storage::kRfNumErr;
using storage::kWPrErr;
using storage::OSErr;

static constexpr OSErr kNotOursErr = -9999;	 /* volume is not ours — internal sentinel */
static constexpr uint16_t kNotOurs = 0xFFFE; /* returned to guest: "not our volume" */

/* ── Type-safe PB access ─────────────────────────── */

template <typename T> struct PBField
{
	uint32_t offset;
};

using Blob16 = std::array<uint8_t, 16>;

namespace detail
{
template <typename T> T pbRead(uint32_t addr);
template <typename T> void pbWrite(uint32_t addr, T v);

template <> inline uint8_t pbRead<uint8_t>(uint32_t a)
{
	return get_vm_byte(a);
}
template <> inline int8_t pbRead<int8_t>(uint32_t a)
{
	return static_cast<int8_t>(get_vm_byte(a));
}
template <> inline uint16_t pbRead<uint16_t>(uint32_t a)
{
	return uint16_t(get_vm_byte(a)) << 8 | get_vm_byte(a + 1);
}
template <> inline int16_t pbRead<int16_t>(uint32_t a)
{
	return static_cast<int16_t>(pbRead<uint16_t>(a));
}
template <> inline uint32_t pbRead<uint32_t>(uint32_t a)
{
	return uint32_t(get_vm_byte(a)) << 24 | uint32_t(get_vm_byte(a + 1)) << 16 |
		   uint32_t(get_vm_byte(a + 2)) << 8 | get_vm_byte(a + 3);
}
template <> inline int32_t pbRead<int32_t>(uint32_t a)
{
	return static_cast<int32_t>(pbRead<uint32_t>(a));
}

template <> inline void pbWrite<uint8_t>(uint32_t a, uint8_t v)
{
	put_vm_byte(a, v);
}
template <> inline void pbWrite<int8_t>(uint32_t a, int8_t v)
{
	put_vm_byte(a, static_cast<uint8_t>(v));
}
template <> inline void pbWrite<uint16_t>(uint32_t a, uint16_t v)
{
	put_vm_byte(a, (v >> 8) & 0xFF);
	put_vm_byte(a + 1, v & 0xFF);
}
template <> inline void pbWrite<int16_t>(uint32_t a, int16_t v)
{
	pbWrite<uint16_t>(a, static_cast<uint16_t>(v));
}
template <> inline void pbWrite<uint32_t>(uint32_t a, uint32_t v)
{
	put_vm_byte(a, (v >> 24) & 0xFF);
	put_vm_byte(a + 1, (v >> 16) & 0xFF);
	put_vm_byte(a + 2, (v >> 8) & 0xFF);
	put_vm_byte(a + 3, v & 0xFF);
}
template <> inline void pbWrite<int32_t>(uint32_t a, int32_t v)
{
	pbWrite<uint32_t>(a, static_cast<uint32_t>(v));
}
template <> inline Blob16 pbRead<Blob16>(uint32_t a)
{
	Blob16 b;
	for (int i = 0; i < 16; i++)
		b[i] = get_vm_byte(a + i);
	return b;
}
template <> inline void pbWrite<Blob16>(uint32_t a, Blob16 v)
{
	for (int i = 0; i < 16; i++)
		put_vm_byte(a + i, v[i]);
}
} // namespace detail

template <typename T> struct PBProxy
{
	uint32_t addr;
	operator T() const { return detail::pbRead<T>(addr); }
	template <typename U> PBProxy &operator=(U v)
	{
		if constexpr (sizeof(U) > sizeof(T))
		{
			if (static_cast<U>(static_cast<T>(v)) != v)
				DIAG(ExtFS, "*** PB truncation: %lld → %lld (field at +%u) ***\n",
					 static_cast<long long>(v), static_cast<long long>(static_cast<T>(v)), addr);
		}
		detail::pbWrite<T>(addr, static_cast<T>(v));
		return *this;
	}
};

struct PBRef
{
	uint32_t addr;
	template <typename T> PBProxy<T> operator[](PBField<T> f) const { return {addr + f.offset}; }

	/* Zero bytes in the PB.  Real HFS ROM code writes every output byte;
	   clearing prevents stale PB data from confusing callers (e.g. the
	   Finder reading residual ioACUser bits as access-denied flags). */
	void zero(int startOffset, int count) const
	{
		for (int i = 0; i < count; ++i)
			put_vm_byte(addr + startOffset + i, 0);
	}
};

/* ── PB field definitions (Inside Macintosh IV) ──── */

/* Shared header */
constexpr PBField<uint32_t> ioNamePtr{18};
constexpr PBField<int16_t> ioVRefNum{22};
[[maybe_unused]] constexpr PBField<int16_t> ioRefNum{24}; /* guest maps to FCB */
constexpr PBField<uint8_t> ioPermssn{27};
constexpr PBField<uint32_t> ioMisc{28};

/* ioParam variant — Read/Write/SetEOF use the register path;
   these fields are handled entirely by the guest (see TrapRead). */
[[maybe_unused]] constexpr PBField<uint32_t> ioBuffer{32};
[[maybe_unused]] constexpr PBField<uint32_t> ioReqCount{36};
[[maybe_unused]] constexpr PBField<uint32_t> ioActCount{40};
[[maybe_unused]] constexpr PBField<int16_t> ioPosMode{44};
[[maybe_unused]] constexpr PBField<int32_t> ioPosOffset{46};

/* fileParam / CInfoPBRec hFileInfo variant */
constexpr PBField<int16_t> ioFDirIndex{28};
constexpr PBField<uint8_t> ioFlAttrib{30};
constexpr PBField<uint8_t> ioACUser{31};
static constexpr uint8_t kFlAttribDir = 0x10; /* IM IV-155: bit 4 = directory */
/* FInfo sub-fields (IM IV-101): type(4) creator(4) flags(2) location(4) folder(2) */
constexpr PBField<uint32_t> ioFlFndrType{32};
constexpr PBField<uint32_t> ioFlFndrCreator{36};
constexpr PBField<uint16_t> ioFlFndrFlags{40};
constexpr PBField<uint32_t> ioFlFndrLocation{42};
constexpr PBField<uint16_t> ioFlFndrFolder{46};
constexpr PBField<uint32_t> ioFlNum{48};
constexpr PBField<int16_t> ioFlStBlk{52};
constexpr PBField<uint32_t> ioFlLgLen{54};
constexpr PBField<uint32_t> ioFlPyLen{58};
constexpr PBField<int16_t> ioFlRStBlk{62};
constexpr PBField<uint32_t> ioFlRLgLen{64};
constexpr PBField<uint32_t> ioFlRPyLen{68};
constexpr PBField<uint32_t> ioFlCrDat{72};
constexpr PBField<uint32_t> ioFlMdDat{76};
constexpr PBField<uint32_t> ioFlBkDat{80};
constexpr PBField<Blob16> ioFlXFndrInfo{84};
constexpr PBField<uint32_t> ioFlParID{100};
constexpr PBField<uint32_t> ioFlClpSiz{104};

/* CInfoPBRec dirInfo variant */
constexpr PBField<Blob16> ioDrUsrWds{32};
constexpr PBField<uint32_t> ioDrDirID{48};
constexpr PBField<int16_t> ioDrNmFls{52};
constexpr PBField<uint32_t> ioDrCrDat{72};
constexpr PBField<uint32_t> ioDrMdDat{76};
constexpr PBField<uint32_t> ioDrBkDat{80};
constexpr PBField<Blob16> ioDrFndrInfo{84};
constexpr PBField<uint32_t> ioDrParID{100};

/* WDParam variant */
constexpr PBField<int16_t> ioWDIndex{26};
constexpr PBField<uint32_t> ioWDProcID{28};
constexpr PBField<int16_t> ioWDVRefNum{32};
constexpr PBField<uint32_t> ioWDDirID{48};

/* volumeParam variant — GetVolInfo is handled guest-side;
   the host only provides raw stats via the register path. */
[[maybe_unused]] constexpr PBField<int16_t> ioVolIndex{28};
[[maybe_unused]] constexpr PBField<int16_t> ioVNmAlBlks{46};
[[maybe_unused]] constexpr PBField<uint32_t> ioVAlBlkSiz{48};
[[maybe_unused]] constexpr PBField<uint32_t> ioVClpSiz{52};
[[maybe_unused]] constexpr PBField<int16_t> ioVFrBlk{62};

/* CatMove */
constexpr PBField<uint32_t> ioNewDirID{36};

/* ── PB-based command codes ──────────────────────── */

static constexpr uint16_t kPB_GetCatInfo = 0x230;
static constexpr uint16_t kPB_GetFileInfo = 0x231;
static constexpr uint16_t kPB_Open = 0x232;
static constexpr uint16_t kPB_OpenRF = 0x233;
static constexpr uint16_t kPB_Create = 0x238;
static constexpr uint16_t kPB_Delete = 0x239;
static constexpr uint16_t kPB_Rename = 0x23A;
static constexpr uint16_t kPB_SetFileInfo = 0x23B;
static constexpr uint16_t kPB_SetCatInfo = 0x23C;
static constexpr uint16_t kPB_DirCreate = 0x23D;
static constexpr uint16_t kPB_CatMove = 0x23E;
static constexpr uint16_t kPB_OpenWD = 0x242;
static constexpr uint16_t kPB_CloseWD = 0x243;
static constexpr uint16_t kPB_GetWDInfo = 0x244;
static constexpr uint16_t kPB_SetVol = 0x0246;
static constexpr uint16_t kPB_GetVol = 0x0247;

/* ── Volume resolution helpers ────────────────────── */

// Resolve a PB's ioVRefNum to a HostVolume*.
// For vRefNum 0 (default volume), uses the slot last set via SetDefaultVRefNum.
// Returns nullptr + sets errOut = kNotOursErr if the volume is not ours.
static storage::HostVolume *volumeFromPB(PBRef pb, bool /*isHFS*/, storage::OSErr &errOut)
{
	int16_t vRefNum = pb[ioVRefNum];

	// vRefNum 0 means "default volume".
	if (vRefNum == 0)
	{
		int defSlot = -1;
		if (s_drives.isDefaultOurs(defSlot))
		{
			errOut = storage::kNoErr;
			return s_drives.volume(defSlot);
		}
		errOut = kNotOursErr;
		return nullptr;
	}

	// Direct vRefNum — try slot lookup.
	int slot = s_drives.slotFromVRefNum(vRefNum);
	if (slot >= 0)
	{
		errOut = storage::kNoErr;
		return s_drives.volume(slot);
	}

	// DriveNum?
	if (vRefNum >= storage::kBaseDriveNum && vRefNum < storage::kBaseDriveNum + storage::kMaxDrives)
	{
		auto *vol = s_drives.volumeByDriveNum(vRefNum);
		if (vol)
		{
			errOut = storage::kNoErr;
			return vol;
		}
	}

	// WD refnum?  Use DriveManager's global WD table.
	auto wdRef = storage::DecodeGuestWDRef(vRefNum);
	int wdSlot = s_drives.wdToSlot(wdRef);
	if (wdSlot >= 0)
	{
		errOut = storage::kNoErr;
		return s_drives.volume(wdSlot);
	}

	errOut = kNotOursErr;
	return nullptr;
}

// Resolve a handle to (HostVolume*, localHandle).
static std::pair<storage::HostVolume *, uint32_t> volumeFromHandle(uint32_t handle)
{
	return s_drives.resolveHandle(handle);
}

// Resolve the directory from a PB using DriveManager's global resolveDir.
static uint32_t pbResolveDir(PBRef pb, bool isHFS, storage::HostVolume & /*vol*/)
{
	int16_t vRefNum = pb[ioVRefNum];
	uint32_t dirID = isHFS ? static_cast<uint32_t>(pb[ioDrDirID]) : 0;
	int outSlot = -1;
	uint32_t resolved = s_drives.resolveDir(vRefNum, dirID, outSlot);
	return resolved != 0 ? resolved : kRootDirID;
}

// Translate an OSErr into a dispatch result, mapping kNotOursErr → kNotOurs.
static uint16_t translateResult(OSErr err)
{
	if (err == kNotOursErr) return kNotOurs;
	return static_cast<uint16_t>(err);
}

/* ── Guest RAM helpers ────────────────────────────── */

static void writePascalString(uint32_t addr, const std::string &s)
{
	uint8_t len = static_cast<uint8_t>(std::min(s.size(), size_t(31)));
	put_vm_byte(addr, len);
	for (uint8_t i = 0; i < len; i++)
		put_vm_byte(addr + 1 + i, static_cast<uint8_t>(s[i]));
}

static std::string readPascalString(uint32_t addr)
{
	uint8_t len = get_vm_byte(addr);
	std::string s;
	s.reserve(len);
	for (uint8_t i = 0; i < len; i++)
		s.push_back(static_cast<char>(get_vm_byte(addr + 1 + i)));
	return s;
}

/* ── PB-based standalone handlers ─────────────────── */

struct FInfo
{
	uint32_t type;
	uint32_t creator;
	uint16_t flags;
	uint32_t location;
	uint16_t folder;
};

/* Write a CatalogEntry's Finder info into the FInfo area of a PB (IM IV-101). */
static void pbWriteFInfo(PBRef pb, const storage::CatalogEntry *e)
{
	pb[ioFlFndrType] = e->type;
	pb[ioFlFndrCreator] = e->creator;
	pb[ioFlFndrFlags] = e->finderFlags;
	pb[ioFlFndrLocation] = e->fdLocation;
	pb[ioFlFndrFolder] = e->fdFldr;
}

/* Read FInfo fields from a PB — inverse of pbWriteFInfo. */
static FInfo pbReadFInfo(PBRef pb)
{
	return {
		pb[ioFlFndrType],	  pb[ioFlFndrCreator], pb[ioFlFndrFlags],
		pb[ioFlFndrLocation], pb[ioFlFndrFolder],
	};
}

/*
	Fill the file-variant fields of a CInfoPBRec / FileParam from a
	CatalogEntry.  When isHFS is false, stops at offset 80 (flat PB
	layout per IM IV-96); otherwise writes the extended HFS fields
	including FXInfo and ioFlParID (IM IV-155).
*/
static void pbWriteFileFields(PBRef pb, const storage::CatalogEntry *e, bool isHFS = true)
{
	pb.zero(24, 4); /* ioFRefNum(2) + ioFVersNum(1) + filler1(1) */
	pb[ioFlAttrib] = 0;
	pb[ioACUser] = 0; /* filler2 for files — must be zero */
	pbWriteFInfo(pb, e);
	pb[ioFlNum] = e->cnid;
	pb[ioFlStBlk] = 0;
	pb[ioFlLgLen] = e->dataForkSize;
	pb[ioFlPyLen] = e->dataForkSize;
	pb[ioFlRStBlk] = 0;
	pb[ioFlRLgLen] = e->rsrcForkSize;
	pb[ioFlRPyLen] = e->rsrcForkSize;
	pb[ioFlCrDat] = e->crDate;
	pb[ioFlMdDat] = e->modDate;
	if (!isHFS) return; /* flat FileParam ends at ioFlMdDat (offset 80) */
	pb[ioFlBkDat] = 0;
	pb[ioFlXFndrInfo] = Blob16{};
	pb[ioFlParID] = e->parentDirID;
	pb[ioFlClpSiz] = 0;
}

/* Fill the directory-variant fields of a CInfoPBRec (IM IV-155). */
static void pbWriteDirFields(PBRef pb, const storage::CatalogEntry *e, storage::HostVolume &vol)
{
	pb.zero(24, 4); /* ioFRefNum(2) + ioFVersNum(1) + filler1(1) */
	pb[ioFlAttrib] = kFlAttribDir;
	pb[ioACUser] = 0;

	/* DInfo + DXInfo — zero-initialized, filled if entry exists */
	Blob16 dinfo{}, dxinfo{};
	vol.getDirInfo(e->cnid, dinfo, dxinfo);
	pb[ioDrUsrWds] = dinfo;
	pb[ioDrFndrInfo] = dxinfo;

	pb[ioDrNmFls] = vol.childCount(e->cnid);
	pb[ioDrDirID] = e->cnid;
	pb.zero(54, 18); /* filler3 — 9 reserved int16s */
	pb[ioDrParID] = e->parentDirID;
	pb[ioDrCrDat] = e->crDate;
	pb[ioDrMdDat] = e->modDate;
	pb[ioDrBkDat] = 0;
}

/* Synthesize a CInfoPBRec for the volume root (dirID 2). */
static void pbWriteRootDir(PBRef pb, uint32_t nameAddr, storage::HostVolume &vol,
						   std::string_view volName)
{
	uint32_t now = static_cast<uint32_t>(std::time(nullptr)) + appledouble::kMacEpochOffset;
	pb.zero(24, 4); /* ioFRefNum(2) + ioFVersNum(1) + filler1(1) */
	pb[ioFlAttrib] = kFlAttribDir;
	pb[ioACUser] = 0;
	pb[ioDrUsrWds] = Blob16{};
	pb[ioDrFndrInfo] = Blob16{};
	pb[ioDrNmFls] = vol.childCount(kRootDirID);
	pb[ioDrDirID] = kRootDirID;
	pb.zero(54, 18); /* filler3 */
	pb[ioDrParID] = kRootParentID;
	pb[ioDrCrDat] = now;
	pb[ioDrMdDat] = now;
	pb[ioDrBkDat] = 0;
	if (nameAddr) writePascalString(nameAddr, std::string(volName));
}

/*
	PBGetCatInfo (IM IV-155).  Three modes:
	  ioFDirIndex > 0  — indexed enumeration of directory contents
	  ioFDirIndex == 0  — by-name lookup (or by-dirID if name is empty)
	  ioFDirIndex < 0  — info about the directory identified by ioDrDirID
	Returns file or directory fields depending on the entry found.
*/
static OSErr PbGetCatInfo(PBRef pb, bool isHFS)
{
	OSErr verr;
	auto *vol = volumeFromPB(pb, isHFS, verr);
	if (!vol) return verr;
	int slot = vol->slot();

	uint32_t dirID = pbResolveDir(pb, isHFS, *vol);
	int16_t index = pb[ioFDirIndex];
	uint32_t nameAddr = pb[ioNamePtr];

	DIAG(ExtFS, "PbGetCatInfo dir=%u idx=%d\n", dirID, index);

	const storage::CatalogEntry *e = nullptr;

	if (index > 0)
	{
		/* Indexed enumeration */
		if (dirID == kRootParentID)
		{
			if (index == 1)
			{
				pbWriteRootDir(pb, nameAddr, *vol, s_drives.volumeName(slot));
				return 0;
			}
			return kFnfErr;
		}
		e = vol->nthChild(dirID, index);
		if (!e) return kFnfErr;
	}
	else if (index == 0 && nameAddr != 0)
	{
		/* By-name lookup */
		std::string name = readPascalString(nameAddr);
		if (!name.empty())
			e = vol->findByPath(dirID, name);
		else
			e = vol->findByCNID(dirID);
	}
	else
	{
		/* index <= 0 or no name: info about dirID itself */
		e = vol->findByCNID(dirID);
	}

	/* Synthesize root if needed */
	if (!e && (dirID == kRootDirID || dirID == kRootParentID))
	{
		pbWriteRootDir(pb, nameAddr, *vol, s_drives.volumeName(slot));
		return 0;
	}

	if (!e) return kFnfErr;

	if (nameAddr) writePascalString(nameAddr, e->macName);

	if (e->isDirectory)
		pbWriteDirFields(pb, e, *vol);
	else
		pbWriteFileFields(pb, e);

	return 0;
}

/* PBGetFInfo / PBHGetFInfo (IM IV-97 / IV-148).  By-name file lookup only.
   Note: the real ROM's GetFileInfo clears HFSReq regardless of the H bit
   in the trap word, so it only writes the fileParam fields (offsets 30–79).
   CInfoPBRec-extended fields (ioFlBkDat, ioFlXFndrInfo, ioFlParID,
   ioFlClpSiz at offsets 80–107) are only written by PBGetCatInfo. */
static OSErr PbGetFileInfo(PBRef pb, bool isHFS)
{
	OSErr verr;
	auto *vol = volumeFromPB(pb, isHFS, verr);
	if (!vol) return verr;

	uint32_t dirID = pbResolveDir(pb, isHFS, *vol);
	uint32_t nameAddr = pb[ioNamePtr];
	if (nameAddr == 0) return kParamErr;

	std::string name = readPascalString(nameAddr);
	DIAG(ExtFS, "PbGetFileInfo dir=%u name=\"%s\"\n", dirID, name.c_str());

	auto *e = vol->findByPath(dirID, name);
	if (!e) return kFnfErr;

	pbWriteFileFields(pb, e, false); /* fileParam only — no CInfoPBRec extensions */
	return 0;
}

/*
	Shared implementation for PBOpen / PBOpenRF (IM IV-92 / IV-108).
	Resolves the file by name, opens the requested fork, and returns the
	fork handle + size + CNID + parent via regParam[] for the guest to
	populate its FCB.
*/
static OSErr PbOpenFork(PBRef pb, uint32_t regParam[], storage::ForkType forkType, bool isHFS)
{
	OSErr verr;
	auto *vol = volumeFromPB(pb, isHFS, verr);
	if (!vol) return verr;

	uint32_t dirID = pbResolveDir(pb, isHFS, *vol);
	uint32_t nameAddr = pb[ioNamePtr];
	uint8_t perm = pb[ioPermssn];

	if (nameAddr == 0) return kParamErr;

	std::string name = readPascalString(nameAddr);
	DIAG(ExtFS, "PbOpen%s dir=%u name=\"%s\"\n",
		 forkType == storage::ForkType::Resource ? "RF" : "", dirID, name.c_str());

	auto *e = vol->findByPath(dirID, name);
	if (!e) return kFnfErr;

	uint32_t size = 0;
	OSErr err;
	uint32_t localHandle = vol->openFork(e->cnid, forkType, size, err, perm);
	if (localHandle == 0) return err;

	regParam[0] = storage::EncodeHandle(vol->slot(), localHandle);
	regParam[1] = size;
	regParam[2] = e->cnid;
	regParam[3] = e->parentDirID;
	return 0;
}

static OSErr PbOpen(PBRef pb, uint32_t regParam[], bool isHFS)
{
	return PbOpenFork(pb, regParam, storage::ForkType::Data, isHFS);
}

static OSErr PbOpenRF(PBRef pb, uint32_t regParam[], bool isHFS)
{
	return PbOpenFork(pb, regParam, storage::ForkType::Resource, isHFS);
}

/* PBCreate / PBHCreate (IM IV-90).  Create an empty file. */
static OSErr PbCreate(PBRef pb, bool isHFS)
{
	OSErr verr;
	auto *vol = volumeFromPB(pb, isHFS, verr);
	if (!vol) return verr;
	uint32_t dirID = pbResolveDir(pb, isHFS, *vol);
	uint32_t nameAddr = pb[ioNamePtr];
	if (nameAddr == 0) return kParamErr;

	std::string name = readPascalString(nameAddr);
	DIAG(ExtFS, "PbCreate dir=%u name=\"%s\"\n", dirID, name.c_str());

	OSErr err;
	uint32_t cnid = vol->createFile(dirID, name, err);
	if (cnid == 0) return err;

	DIAG(ExtFS, "  → cnid=%u\n", cnid);
	return 0;
}

/* PBDelete / PBHDelete (IM IV-91).  Remove a file or empty directory. */
static OSErr PbDelete(PBRef pb, bool isHFS)
{
	OSErr verr;
	auto *vol = volumeFromPB(pb, isHFS, verr);
	if (!vol) return verr;
	uint32_t dirID = pbResolveDir(pb, isHFS, *vol);
	uint32_t nameAddr = pb[ioNamePtr];
	if (nameAddr == 0) return kParamErr;

	std::string name = readPascalString(nameAddr);
	DIAG(ExtFS, "PbDelete dir=%u name=\"%s\"\n", dirID, name.c_str());

	auto err = vol->remove(dirID, name);
	return err;
}

/* PBRename / PBHRename (IM IV-95).  New name comes from ioMisc. */
static OSErr PbRename(PBRef pb, bool isHFS)
{
	OSErr verr;
	auto *vol = volumeFromPB(pb, isHFS, verr);
	if (!vol) return verr;
	uint32_t dirID = pbResolveDir(pb, isHFS, *vol);
	uint32_t nameAddr = pb[ioNamePtr];
	uint32_t newNameAddr = pb[ioMisc];
	if (nameAddr == 0 || newNameAddr == 0) return kParamErr;

	std::string oldName = readPascalString(nameAddr);
	std::string newName = readPascalString(newNameAddr);
	DIAG(ExtFS, "PbRename dir=%u old=\"%s\" new=\"%s\"\n", dirID, oldName.c_str(), newName.c_str());

	auto err = vol->rename(dirID, oldName, newName);
	return err;
}

/* PBDirCreate (IM IV-153).  Returns new dirID in ioDrDirID. */
static OSErr PbDirCreate(PBRef pb, bool isHFS)
{
	OSErr verr;
	auto *vol = volumeFromPB(pb, isHFS, verr);
	if (!vol) return verr;
	uint32_t dirID = pbResolveDir(pb, isHFS, *vol);
	uint32_t nameAddr = pb[ioNamePtr];
	if (nameAddr == 0) return kParamErr;

	std::string name = readPascalString(nameAddr);
	DIAG(ExtFS, "PbDirCreate dir=%u name=\"%s\"\n", dirID, name.c_str());

	OSErr err;
	uint32_t cnid = vol->createDir(dirID, name, err);
	if (cnid == 0) return err;

	pb[ioDrDirID] = cnid;
	DIAG(ExtFS, "  → cnid=%u\n", cnid);
	return 0;
}

/* PBCatMove (IM IV-157).  Move a file or directory to ioNewDirID. */
static OSErr PbCatMove(PBRef pb, bool isHFS)
{
	OSErr verr;
	auto *vol = volumeFromPB(pb, isHFS, verr);
	if (!vol) return verr;
	uint32_t dirID = pbResolveDir(pb, isHFS, *vol);
	uint32_t nameAddr = pb[ioNamePtr];
	if (nameAddr == 0) return kParamErr;

	uint32_t dstDirID = pb[ioNewDirID];
	std::string name = readPascalString(nameAddr);
	DIAG(ExtFS, "PbCatMove srcDir=%u name=\"%s\" dstDir=%u\n", dirID, name.c_str(), dstDirID);

	auto err = vol->move(dirID, name, dstDirID);
	return err;
}

/* PBSetFInfo / PBHSetFInfo (IM IV-100 / IV-150).  Write FInfo from the PB. */
static OSErr PbSetFileInfo(PBRef pb, bool isHFS)
{
	OSErr verr;
	auto *vol = volumeFromPB(pb, isHFS, verr);
	if (!vol) return verr;
	uint32_t dirID = pbResolveDir(pb, isHFS, *vol);
	uint32_t nameAddr = pb[ioNamePtr];
	if (nameAddr == 0) return kParamErr;

	std::string name = readPascalString(nameAddr);
	DIAG(ExtFS, "PbSetFileInfo dir=%u name=\"%s\"\n", dirID, name.c_str());

	auto *e = vol->findByPath(dirID, name);
	if (!e) return kFnfErr;

	auto fi = pbReadFInfo(pb);
	auto err = vol->setFileInfo(e->cnid, fi.type, fi.creator, fi.flags, fi.location, fi.folder);
	return err;
}

/*
	PBSetCatInfo (IM IV-156).  Sets Finder info for either a file (FInfo)
	or a directory (DInfo + DXInfo).  The entry type determines which
	PB fields are read.
*/
static OSErr PbSetCatInfo(PBRef pb, bool isHFS)
{
	OSErr verr;
	auto *vol = volumeFromPB(pb, isHFS, verr);
	if (!vol) return verr;
	uint32_t dirID = pbResolveDir(pb, isHFS, *vol);
	uint32_t nameAddr = pb[ioNamePtr];
	if (nameAddr == 0) return kParamErr;

	std::string name = readPascalString(nameAddr);
	DIAG(ExtFS, "PbSetCatInfo dir=%u name=\"%s\"\n", dirID, name.c_str());

	auto *e = vol->findByPath(dirID, name);
	if (!e) return kFnfErr;

	if (e->isDirectory)
	{
		Blob16 dinfo = pb[ioDrUsrWds];
		Blob16 dxinfo = pb[ioDrFndrInfo];
		auto err = vol->setDirInfo(e->cnid, dinfo, dxinfo);
		return err;
	}
	else
	{
		auto fi = pbReadFInfo(pb);
		auto err = vol->setFileInfo(e->cnid, fi.type, fi.creator, fi.flags, fi.location, fi.folder);
		return err;
	}
}

/* PBOpenWD (IM IV-159).  Allocate a WD refnum for ioWDDirID. */
static OSErr PbOpenWD(PBRef pb)
{
	int16_t vRefNum = pb[ioVRefNum];
	uint32_t dirID = pb[ioWDDirID];
	uint32_t procID = pb[ioWDProcID];

	// Determine which slot this vRefNum belongs to.
	int slot = s_drives.slotFromVRefNum(vRefNum);
	if (slot < 0)
	{
		// Try WD refnum.
		auto wdRef = storage::DecodeGuestWDRef(vRefNum);
		slot = s_drives.wdToSlot(wdRef);
	}
	if (slot < 0) return kNotOursErr;

	DIAG(ExtFS, "PbOpenWD dir=%u proc=%u slot=%d\n", dirID, procID, slot);

	uint32_t wdRef = s_drives.openWD(slot, dirID, procID);
	pb[ioVRefNum] = storage::EncodeGuestWDRef(wdRef);
	return 0;
}

/* PBCloseWD (IM IV-160).  Release a WD refnum. */
static OSErr PbCloseWD(PBRef pb)
{
	int16_t vRefNum = pb[ioVRefNum];
	auto wdRef = storage::DecodeGuestWDRef(vRefNum);
	DIAG(ExtFS, "PbCloseWD vRefNum=%d wdRef=%u\n", vRefNum, wdRef);

	if (!s_drives.isOurWD(wdRef)) return kNotOursErr;
	s_drives.closeWD(wdRef);
	return 0;
}

/*
	PBGetWDInfo (IM IV-161).  Return the dirID and procID for a WD refnum.
	Only supports direct lookup (ioWDIndex == 0); indexed enumeration of
	all WDs is not implemented.
*/
static OSErr PbGetWDInfo(PBRef pb)
{
	int16_t vRefNum = pb[ioVRefNum];
	int16_t wdIndex = pb[ioWDIndex];

	DIAG(ExtFS, "PbGetWDInfo vRefNum=%d wdIndex=%d\n", vRefNum, wdIndex);

	if (wdIndex != 0) return kNotOursErr;

	// Check if it's a direct vRefNum for any of our volumes.
	int slot = s_drives.slotFromVRefNum(vRefNum);
	if (slot >= 0 || vRefNum == kGuestDriveNum)
	{
		if (slot < 0) slot = 0; // driveNum 8 = slot 0
		pb[ioWDProcID] = 0;
		pb[ioWDDirID] = kRootDirID;
	}
	else
	{
		auto wdRef = storage::DecodeGuestWDRef(vRefNum);
		if (!s_drives.isOurWD(wdRef)) return kNotOursErr;
		pb[ioWDProcID] = s_drives.wdToProcID(wdRef);
		pb[ioWDDirID] = s_drives.wdToDirID(wdRef);
		slot = s_drives.wdToSlot(wdRef);
	}

	pb[ioWDVRefNum] = static_cast<int16_t>(-(static_cast<int16_t>(storage::kBaseVRefNum) + slot));

	uint32_t nameAddr = pb[ioNamePtr];
	if (nameAddr != 0)
	{
		auto name = s_drives.volumeName(slot);
		writePascalString(nameAddr, name.empty() ? std::string("Shared") : std::string(name));
	}

	return 0;
}

/* PBSetVol / PBHSetVol — host-authoritative default directory. */
static void PbSetVol(uint32_t regParam[], uint16_t &regResult)
{
	PBRef pb{regParam[0]};
	bool isHFS = regParam[1] != 0;

	int16_t vRefNum = pb[ioVRefNum];
	uint32_t nameAddr = pb[ioNamePtr];

	int slot = -1;
	if (vRefNum == 0 && nameAddr != 0)
	{
		std::string name = readPascalString(nameAddr);
		slot = s_drives.slotFromName(name);
	}
	else
	{
		slot = s_drives.slotFromVRefNum(vRefNum);
		if (slot < 0)
		{
			auto wdRef = storage::DecodeGuestWDRef(vRefNum);
			int wdSlot = s_drives.wdToSlot(wdRef);
			if (wdSlot >= 0) slot = wdSlot;
		}
	}

	if (slot < 0)
	{
		regResult = kNotOurs;
		return;
	}

	uint32_t wdRef;
	if (isHFS)
	{
		uint32_t dirID = static_cast<uint32_t>(pb[ioDrDirID]);
		if (dirID != 0 && dirID != kRootDirID)
		{
			wdRef = s_drives.openWD(slot, dirID, 0);
		}
		else if (vRefNum != 0)
		{
			auto decoded = storage::DecodeGuestWDRef(vRefNum);
			if (s_drives.isOurWD(decoded))
				wdRef = decoded;
			else
				wdRef = s_drives.rootWD(slot);
		}
		else
		{
			wdRef = s_drives.rootWD(slot);
		}
	}
	else
	{
		wdRef = s_drives.rootWD(slot);
	}

	s_drives.setDefaultWD(wdRef);

	regParam[0] = static_cast<uint32_t>(slot);
	regResult = 0;
}

/* PBGetVol / PBHGetVol — host-authoritative current directory query. */
static void PbGetVol(uint32_t regParam[], uint16_t &regResult)
{
	PBRef pb{regParam[0]};
	bool isHFS = regParam[1] != 0;

	int defSlot = -1;
	if (!s_drives.isDefaultOurs(defSlot))
	{
		regResult = kNotOurs;
		return;
	}

	auto *vol = s_drives.volume(defSlot);
	if (!vol)
	{
		regResult = kNotOurs;
		return;
	}

	uint32_t defWD = s_drives.defaultWD();
	pb[ioVRefNum] = storage::EncodeGuestWDRef(defWD);

	uint32_t nameAddr = pb[ioNamePtr];
	if (nameAddr != 0)
	{
		auto name = s_drives.volumeName(defSlot);
		writePascalString(nameAddr, name.empty() ? std::string("Shared") : std::string(name));
	}

	if (isHFS)
	{
		pb[ioWDVRefNum] = storage::EncodeGuestWDRef(s_drives.rootWD(defSlot));
		pb[ioWDProcID] = 0u;
		uint32_t dirID = s_drives.wdToDirID(defWD);
		pb[ioWDDirID] = dirID != 0 ? dirID : kRootDirID;
	}

	regResult = 0;
}

/* ── Register-based handlers ──────────────────────── */

/* Return the number of mounted drives. */
static void RegVersion(uint32_t regParam[], uint16_t &regResult)
{
	/* Protocol version — the guest checks this to decide whether to proceed.
	   Zero means no shared drive support; non-zero means "alive".
	   All drive delivery happens via PollMount; the version is not a count. */
	regParam[0] = (s_drives.mountedCount() > 0) ? 2u : 0u;
	regResult = 0;
	DIAG(ExtFS, "version query → %u\n", regParam[0]);
}

/* Return volume statistics for slot 0 (legacy: file count, dir count, total bytes). */
static void RegGetVol(uint32_t regParam[], uint16_t &regResult)
{
	auto *vol = s_drives.volume(0);
	if (!vol)
	{
		regParam[0] = regParam[1] = regParam[2] = 0;
		regResult = 0;
		return;
	}
	uint32_t files, dirs, bytes;
	vol->volumeStats(files, dirs, bytes);
	regParam[0] = files;
	regParam[1] = bytes;
	regParam[2] = dirs;
	regResult = 0;
	DIAG(ExtFS, "GetVol → %u files, %u dirs, %u bytes\n", files, dirs, bytes);
}

/* Read bytes from an open fork into guest RAM. */
static void RegRead(uint32_t regParam[], uint16_t &regResult)
{
	uint32_t handle = regParam[0];
	uint32_t offset = regParam[1];
	uint32_t count = regParam[2];
	uint32_t guestBuf = regParam[3];

	auto [vol, local] = volumeFromHandle(handle);
	if (!vol)
	{
		regResult = storage::kRfNumErr;
		return;
	}

	DIAG(ExtFS, "Read h=%u(s%d l%u) off=%u cnt=%u\n", handle, vol->slot(), local, offset, count);

	std::vector<uint8_t> buf(count);
	uint32_t got = 0;
	auto err = vol->readFork(local, offset, buf, got);
	if (err != kNoErr)
	{
		regResult = err;
		return;
	}

	for (uint32_t i = 0; i < got; i++)
		put_vm_byte(guestBuf + i, buf[i]);
	regParam[0] = got;
	DIAG(ExtFS, "  → read %u bytes\n", got);
	regResult = 0;
}

/* Close an open fork handle. */
static void RegClose(uint32_t regParam[], uint16_t &regResult)
{
	uint32_t handle = regParam[0];
	auto [vol, local] = volumeFromHandle(handle);
	if (!vol)
	{
		regResult = storage::kRfNumErr;
		return;
	}
	DIAG(ExtFS, "Close h=%u(s%d l%u)\n", handle, vol->slot(), local);
	vol->closeFork(local);
	regResult = 0;
}

/* Look up the dirID and procID for a WD refnum (register path). */
/* Format and display a debug log line from the guest. */
static void RegDbgLog(uint32_t regParam[], uint16_t &regResult)
{
	std::string line = guestFormatLog(regParam[0], regParam);
	DIAG(ExtFS, "GUEST: %s\n", line.c_str());
	guestConsoleAppend(line);
	regResult = 0;
}

/* Record a trap call from the guest for the trap-trace log. */
static void RegLogTrap(uint32_t regParam[], uint16_t &regResult)
{
	extfsLogTrap(static_cast<uint16_t>(regParam[0]), regParam[1],
				 static_cast<uint16_t>(regParam[2]), static_cast<int16_t>(regParam[3]),
				 static_cast<uint16_t>(regParam[4]));
	regResult = 0;
}

/* Get/set the guest-side Globals pointer (used by the debugger). */
static void RegGuestVars(uint32_t regParam[], uint16_t &regResult)
{
	static uint32_t s_guestVarsPtr = 0;
	if (regParam[1] != 0) s_guestVarsPtr = regParam[0];
	regParam[0] = s_guestVarsPtr;
	regResult = 0;
}

/* Guest fatal error — log, dump disasm, break into debugger or exit. */
static void RegFatal(uint32_t regParam[], uint16_t &regResult)
{
	std::string msg = guestFormatLog(regParam[0], regParam);
	DIAG(ExtFS, "GUEST FATAL: %s\n", msg.c_str());
	guestConsoleAppend("FATAL: " + msg);
	fprintf(stderr, "\n[GUEST FATAL] (insn #%u) %s\n", (unsigned)g_instructionCount, msg.c_str());
	DumpRecentDisasm();
	fflush(stderr);
	if (g_debuggerActive)
	{
		Debugger::instance()->stop("GUEST FATAL: " + msg);
		regResult = 0;
		return;
	}
	std::exit(EXIT_FAILURE);
}

/* Write bytes from guest RAM into an open fork. */
static void RegWrite(uint32_t regParam[], uint16_t &regResult)
{
	uint32_t handle = regParam[0];
	uint32_t offset = regParam[1];
	uint32_t count = regParam[2];
	uint32_t guestBuf = regParam[3];

	auto [vol, local] = volumeFromHandle(handle);
	if (!vol)
	{
		regResult = storage::kRfNumErr;
		return;
	}

	DIAG(ExtFS, "Write h=%u(s%d l%u) off=%u cnt=%u\n", handle, vol->slot(), local, offset, count);

	std::vector<uint8_t> data(count);
	for (uint32_t i = 0; i < count; i++)
		data[i] = get_vm_byte(guestBuf + i);

	uint32_t written = 0;
	auto err = vol->writeFork(local, offset, data, written);
	if (err != kNoErr)
	{
		regResult = err;
		return;
	}
	regParam[0] = written;
	DIAG(ExtFS, "  → wrote %u bytes\n", written);
	regResult = 0;
}

/* Set the logical end-of-file for an open fork. */
static void RegSetEOF(uint32_t regParam[], uint16_t &regResult)
{
	uint32_t handle = regParam[0];
	uint32_t newSize = regParam[1];
	auto [vol, local] = volumeFromHandle(handle);
	if (!vol)
	{
		regResult = storage::kRfNumErr;
		return;
	}
	DIAG(ExtFS, "SetEOF h=%u(s%d l%u) size=%u\n", handle, vol->slot(), local, newSize);
	auto err = vol->setEOF(local, newSize);
	regResult = err;
}

/* Guest polls for newly mounted drives.  Returns slot info or 0xFFFFFFFF. */
static void RegPollMount(uint32_t regParam[], uint16_t &regResult)
{
	int slot = s_drives.popPendingMount();
	if (slot < 0)
	{
		regParam[0] = 0xFFFFFFFF;
		regResult = 0;
		return;
	}
	auto *vol = s_drives.volume(slot);
	regParam[0] = static_cast<uint32_t>(slot);
	regParam[1] = static_cast<uint32_t>(vol->guestVRefNum());
	regParam[2] = static_cast<uint32_t>(vol->guestDriveNum());
	DIAG(ExtFS, "PollMount → slot %d vRef=%d drv=%d\n", slot, vol->guestVRefNum(),
		 vol->guestDriveNum());
	regResult = 0;
}

/* Return the Mac-visible volume name for a slot (Pascal string to guest buffer). */
static void RegGetVolName(uint32_t regParam[], uint16_t &regResult)
{
	int slot = static_cast<int>(regParam[0]);
	uint32_t guestBuf = regParam[1];
	auto name = s_drives.volumeName(slot);
	if (name.empty())
	{
		regResult = storage::kNsvErr;
		return;
	}
	if (guestBuf != 0) writePascalString(guestBuf, std::string(name));
	regResult = 0;
}

/* ── Dispatch ─────────────────────────────────────── */

/*
	Main entry point — called by the extension mechanism when the guest
	issues a SharedDrive RPC.  Routes each command code to its handler,
	then runs catalog validation after any mutating operation.
*/
void ExtnExtFSDispatch(uint16_t cmd, uint32_t regParam[], uint16_t &regResult)
{
	switch (cmd)
	{
		/* Register-based commands */
		case kExtFSVersion:
			RegVersion(regParam, regResult);
			break;
		case kExtFSGetVol:
			RegGetVol(regParam, regResult);
			break;
		case kExtFSRead:
			RegRead(regParam, regResult);
			break;
		case kExtFSClose:
			RegClose(regParam, regResult);
			break;
		case kExtFSDbgLog:
			RegDbgLog(regParam, regResult);
			break;
		case kExtFSLogTrap:
			RegLogTrap(regParam, regResult);
			break;
		case kExtFSGuestVars:
			RegGuestVars(regParam, regResult);
			break;
		case kExtFSFatal:
			RegFatal(regParam, regResult);
			break;
		case kExtFSWrite:
			RegWrite(regParam, regResult);
			break;
		case kExtFSSetEOF:
			RegSetEOF(regParam, regResult);
			break;
		case kExtFSPollMount:
			RegPollMount(regParam, regResult);
			break;
		case kExtFSGetVolName:
			RegGetVolName(regParam, regResult);
			break;

		/* PB-based commands */
		case kPB_GetCatInfo:
			regResult = translateResult(PbGetCatInfo(PBRef{regParam[0]}, regParam[1] != 0));
			break;
		case kPB_GetFileInfo:
			regResult = translateResult(PbGetFileInfo(PBRef{regParam[0]}, regParam[1] != 0));
			break;
		case kPB_Open:
			regResult = translateResult(PbOpen(PBRef{regParam[0]}, regParam, regParam[1] != 0));
			break;
		case kPB_OpenRF:
			regResult = translateResult(PbOpenRF(PBRef{regParam[0]}, regParam, regParam[1] != 0));
			break;
		case kPB_Create:
			regResult = translateResult(PbCreate(PBRef{regParam[0]}, regParam[1] != 0));
			break;
		case kPB_Delete:
			regResult = translateResult(PbDelete(PBRef{regParam[0]}, regParam[1] != 0));
			break;
		case kPB_Rename:
			regResult = translateResult(PbRename(PBRef{regParam[0]}, regParam[1] != 0));
			break;
		case kPB_DirCreate:
			regResult = translateResult(PbDirCreate(PBRef{regParam[0]}, regParam[1] != 0));
			break;
		case kPB_CatMove:
			regResult = translateResult(PbCatMove(PBRef{regParam[0]}, regParam[1] != 0));
			break;
		case kPB_SetFileInfo:
			regResult = translateResult(PbSetFileInfo(PBRef{regParam[0]}, regParam[1] != 0));
			break;
		case kPB_SetCatInfo:
			regResult = translateResult(PbSetCatInfo(PBRef{regParam[0]}, regParam[1] != 0));
			break;
		case kPB_OpenWD:
			regResult = translateResult(PbOpenWD(PBRef{regParam[0]}));
			break;
		case kPB_CloseWD:
			regResult = translateResult(PbCloseWD(PBRef{regParam[0]}));
			break;
		case kPB_GetWDInfo:
			regResult = translateResult(PbGetWDInfo(PBRef{regParam[0]}));
			break;
		case kPB_SetVol:
			PbSetVol(regParam, regResult);
			break;
		case kPB_GetVol:
			PbGetVol(regParam, regResult);
			break;

		default:
			regResult = 0xFFFF;
			break;
	}

	/* After any mutating operation, validate that the catalog
	   matches the actual filesystem.  Only runs for commands
	   that can modify state (create/delete/move/rename/write). */
	switch (cmd)
	{
		case kPB_Create:
		case kPB_Delete:
		case kPB_Rename:
		case kPB_DirCreate:
		case kPB_CatMove:
		case kPB_SetFileInfo:
		case kPB_SetCatInfo:
			s_drives.forEach(
				[&](int, storage::HostVolume &vol)
				{
					if (!vol.validateCatalog())
						DIAG(ExtFS,
							 "*** CATALOG VALIDATION FAILED (slot %d) after cmd=0x%03x ***\n",
							 vol.slot(), cmd);
				});
			break;
		default:
			break;
	}
}

/* ── Public mount/unmount API ─────────────────────── */

int ExtFSMountDrive(const std::filesystem::path &hostDir)
{
	return s_drives.mount(hostDir);
}

bool ExtFSUnmountDrive(int slot)
{
	return s_drives.unmount(slot);
}

void ExtFSDriveList(void (*printFn)(void *ctx, const char *line), void *ctx)
{
	printFn(ctx, " Slot  Volume          Host path                    Forks");
	s_drives.forEach(
		[&](int slot, storage::HostVolume &)
		{
			char buf[256];
			std::snprintf(buf, sizeof(buf), " %-5d %-15.*s %-28.*s %d", slot,
						  static_cast<int>(s_drives.volumeName(slot).size()),
						  s_drives.volumeName(slot).data(),
						  static_cast<int>(s_drives.hostPath(slot).string().size()),
						  s_drives.hostPath(slot).c_str(), s_drives.openForkCount(slot));
			printFn(ctx, buf);
		});
}
