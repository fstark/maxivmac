#include "core/extn_extfs.h"
#include "core/extn_clip.h"
#include "core/extfs_log.h"
#include "debugger/debugger.h"
#include "cpu/trap_counter.h"
#include "cpu/disasm.h"
#include "core/machine.h"
#include "platform/platform.h"
#include "storage/host_volume.h"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>
#include <cstring>

/* Guest RAM access */
extern uint8_t get_vm_byte(uint32_t addr);
extern void put_vm_byte(uint32_t addr, uint8_t b);

/* ── Command codes ────────────────────────────────── */

static constexpr uint16_t kExtFSVersion = 0x200;
static constexpr uint16_t kExtFSGetVol = 0x201;
static constexpr uint16_t kExtFSRead = 0x205;
static constexpr uint16_t kExtFSClose = 0x206;
static constexpr uint16_t kExtFSGetWDInfo = 0x20A;
static constexpr uint16_t kExtFSOpenWD = 0x20B;
static constexpr uint16_t kExtFSCloseWD = 0x20C;
static constexpr uint16_t kExtFSDbgLog = 0x20D;
static constexpr uint16_t kExtFSGuestVars = 0x20E;
static constexpr uint16_t kExtFSFatal = 0x0214;
static constexpr uint16_t kExtFSWrite = 0x211;
static constexpr uint16_t kExtFSSetEOF = 0x218;
static constexpr uint16_t kExtFSLogTrap = 0x20F;

/* ── HostVolume instance ──────────────────────────── */

static storage::HostVolume s_volume;

static constexpr uint32_t kRootParentID = storage::HostVolume::kRootParentID;
static constexpr uint32_t kRootDirID = storage::HostVolume::kRootDirID;
static constexpr int16_t kGuestVRefNum = storage::HostVolume::kGuestVRefNum;
static constexpr int16_t kGuestDriveNum = storage::HostVolume::kGuestDriveNum;

/* ── Error translation ────────────────────────────── */

static uint16_t fmErrToReg(storage::FMErr err)
{
	switch (err)
	{
		case storage::FMErr::kNoErr:
			return 0;
		case storage::FMErr::kFnfErr:
			return 43;
		case storage::FMErr::kDupFNErr:
			return 48;
		case storage::FMErr::kParamErr:
			return 50;
		case storage::FMErr::kRfNumErr:
			return 51;
		case storage::FMErr::kIoErr:
			return 36;
		case storage::FMErr::kDirNFErr:
			return 120;
		case storage::FMErr::kFBsyErr:
			return 47;
		case storage::FMErr::kWPrErr:
			return 44;
		case storage::FMErr::kOpWrErr:
			return 49;
	}
	return 43;
}

/* ── Type-safe PB access ─────────────────────────── */

template <typename T> struct PBField
{
	uint32_t offset;
};

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
} // namespace detail

template <typename T> struct PBProxy
{
	uint32_t addr;
	operator T() const { return detail::pbRead<T>(addr); }
	PBProxy &operator=(T v)
	{
		detail::pbWrite<T>(addr, v);
		return *this;
	}
};

struct PBRef
{
	uint32_t addr;
	template <typename T> PBProxy<T> operator[](PBField<T> f) const { return {addr + f.offset}; }
};

/* ── PB field definitions (Inside Macintosh IV) ──── */

/* Shared header */
[[maybe_unused]] constexpr PBField<int16_t> ioResult{16};
[[maybe_unused]] constexpr PBField<uint32_t> ioNamePtr{18};
[[maybe_unused]] constexpr PBField<int16_t> ioVRefNum{22};
[[maybe_unused]] constexpr PBField<int16_t> ioRefNum{24};
[[maybe_unused]] constexpr PBField<uint8_t> ioPermssn{27};
[[maybe_unused]] constexpr PBField<uint32_t> ioMisc{28};

/* ioParam variant */
[[maybe_unused]] constexpr PBField<uint32_t> ioBuffer{32};
[[maybe_unused]] constexpr PBField<uint32_t> ioReqCount{36};
[[maybe_unused]] constexpr PBField<uint32_t> ioActCount{40};
[[maybe_unused]] constexpr PBField<int16_t> ioPosMode{44};
[[maybe_unused]] constexpr PBField<int32_t> ioPosOffset{46};

/* fileParam / CInfoPBRec hFileInfo variant */
[[maybe_unused]] constexpr PBField<int16_t> ioFDirIndex{28};
[[maybe_unused]] constexpr PBField<uint8_t> ioFlAttrib{30};
[[maybe_unused]] constexpr uint32_t kOff_ioFlFndrInfo = 32;
[[maybe_unused]] constexpr PBField<uint32_t> ioFlNum{48};
[[maybe_unused]] constexpr PBField<int16_t> ioFlStBlk{52};
[[maybe_unused]] constexpr PBField<uint32_t> ioFlLgLen{54};
[[maybe_unused]] constexpr PBField<uint32_t> ioFlPyLen{58};
[[maybe_unused]] constexpr PBField<int16_t> ioFlRStBlk{62};
[[maybe_unused]] constexpr PBField<uint32_t> ioFlRLgLen{64};
[[maybe_unused]] constexpr PBField<uint32_t> ioFlRPyLen{68};
[[maybe_unused]] constexpr PBField<uint32_t> ioFlCrDat{72};
[[maybe_unused]] constexpr PBField<uint32_t> ioFlMdDat{76};
[[maybe_unused]] constexpr PBField<uint32_t> ioFlBkDat{80};
[[maybe_unused]] constexpr uint32_t kOff_ioFlXFndrInfo = 84;
[[maybe_unused]] constexpr PBField<uint32_t> ioFlParID{100};
[[maybe_unused]] constexpr PBField<uint32_t> ioFlClpSiz{104};

/* CInfoPBRec dirInfo variant */
[[maybe_unused]] constexpr uint32_t kOff_ioDrUsrWds = 32;
[[maybe_unused]] constexpr PBField<uint32_t> ioDrDirID{48};
[[maybe_unused]] constexpr PBField<int16_t> ioDrNmFls{52};
[[maybe_unused]] constexpr PBField<uint32_t> ioDrCrDat{72};
[[maybe_unused]] constexpr PBField<uint32_t> ioDrMdDat{76};
[[maybe_unused]] constexpr PBField<uint32_t> ioDrBkDat{80};
[[maybe_unused]] constexpr uint32_t kOff_ioDrFndrInfo = 84;
[[maybe_unused]] constexpr PBField<uint32_t> ioDrParID{100};

/* WDParam variant */
[[maybe_unused]] constexpr PBField<int16_t> ioWDIndex{26};
[[maybe_unused]] constexpr PBField<uint32_t> ioWDProcID{28};
[[maybe_unused]] constexpr PBField<int16_t> ioWDVRefNum{32};
[[maybe_unused]] constexpr PBField<uint32_t> ioWDDirID{48};

/* volumeParam variant */
[[maybe_unused]] constexpr PBField<int16_t> ioVolIndex{28};
[[maybe_unused]] constexpr PBField<int16_t> ioVNmAlBlks{46};
[[maybe_unused]] constexpr PBField<uint32_t> ioVAlBlkSiz{48};
[[maybe_unused]] constexpr PBField<uint32_t> ioVClpSiz{52};
[[maybe_unused]] constexpr PBField<int16_t> ioVFrBlk{62};

/* CatMove */
[[maybe_unused]] constexpr PBField<uint32_t> ioNewDirID{36};

/* ── PB-based command codes ──────────────────────── */

[[maybe_unused]] static constexpr uint16_t kPB_GetCatInfo = 0x230;
[[maybe_unused]] static constexpr uint16_t kPB_GetFileInfo = 0x231;
[[maybe_unused]] static constexpr uint16_t kPB_Open = 0x232;
[[maybe_unused]] static constexpr uint16_t kPB_OpenRF = 0x233;
[[maybe_unused]] static constexpr uint16_t kPB_Create = 0x238;
[[maybe_unused]] static constexpr uint16_t kPB_Delete = 0x239;
[[maybe_unused]] static constexpr uint16_t kPB_Rename = 0x23A;
[[maybe_unused]] static constexpr uint16_t kPB_SetFileInfo = 0x23B;
[[maybe_unused]] static constexpr uint16_t kPB_SetCatInfo = 0x23C;
[[maybe_unused]] static constexpr uint16_t kPB_DirCreate = 0x23D;
[[maybe_unused]] static constexpr uint16_t kPB_CatMove = 0x23E;
[[maybe_unused]] static constexpr uint16_t kPB_GetVolInfo = 0x23F;
[[maybe_unused]] static constexpr uint16_t kPB_GetVol = 0x240;
[[maybe_unused]] static constexpr uint16_t kPB_SetVol = 0x241;
[[maybe_unused]] static constexpr uint16_t kPB_OpenWD = 0x242;
[[maybe_unused]] static constexpr uint16_t kPB_CloseWD = 0x243;
[[maybe_unused]] static constexpr uint16_t kPB_GetWDInfo = 0x244;
[[maybe_unused]] static constexpr uint16_t kPB_SetDefaultVRefNum = 0x245;

/* ── PB resolve helper ───────────────────────────── */

[[maybe_unused]] static uint32_t pbResolveDir(PBRef pb, bool isHFS)
{
	int16_t vRefNum = pb[ioVRefNum];
	uint32_t dirID = isHFS ? static_cast<uint32_t>(pb[ioDrDirID]) : 0;
	return s_volume.resolveDir(vRefNum, dirID);
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

static void pbWriteFInfo(uint32_t pbAddr, const storage::CatalogEntry *e)
{
	/* FInfo: type(4) creator(4) flags(2) location(4) folder(2) = 16 bytes */
	detail::pbWrite<uint32_t>(pbAddr + kOff_ioFlFndrInfo, e->type);
	detail::pbWrite<uint32_t>(pbAddr + kOff_ioFlFndrInfo + 4, e->creator);
	detail::pbWrite<uint16_t>(pbAddr + kOff_ioFlFndrInfo + 8, e->finderFlags);
	detail::pbWrite<uint32_t>(pbAddr + kOff_ioFlFndrInfo + 10, e->fdLocation);
	detail::pbWrite<uint16_t>(pbAddr + kOff_ioFlFndrInfo + 14, e->fdFldr);
}

static void pbWriteFileFields(PBRef pb, const storage::CatalogEntry *e, bool isHFS = true)
{
	pb[ioFlAttrib] = uint8_t(0);
	pbWriteFInfo(pb.addr, e);
	pb[ioFlNum] = e->cnid;
	pb[ioFlStBlk] = int16_t(0);
	pb[ioFlLgLen] = e->dataForkSize;
	pb[ioFlPyLen] = e->dataForkSize;
	pb[ioFlRStBlk] = int16_t(0);
	pb[ioFlRLgLen] = e->rsrcForkSize;
	pb[ioFlRPyLen] = e->rsrcForkSize;
	pb[ioFlCrDat] = e->crDate;
	pb[ioFlMdDat] = e->modDate;
	if (!isHFS) return; /* flat FileParam ends at ioFlMdDat (offset 80) */
	pb[ioFlBkDat] = uint32_t(0);
	/* Zero FXInfo (16 bytes at offset 84) */
	for (int i = 0; i < 16; i++)
		put_vm_byte(pb.addr + kOff_ioFlXFndrInfo + i, 0);
	pb[ioFlParID] = e->parentDirID;
	pb[ioFlClpSiz] = uint32_t(0);
}

static void pbWriteDirFields(PBRef pb, const storage::CatalogEntry *e)
{
	pb[ioFlAttrib] = uint8_t(0x10);
	put_vm_byte(pb.addr + 31, 0); /* ioACUser */

	/* DInfo + DXInfo (32 bytes total) */
	uint8_t dirBuf[32] = {};
	if (s_volume.getDirInfo(e->cnid, dirBuf))
	{
		for (int i = 0; i < 16; i++)
			put_vm_byte(pb.addr + kOff_ioDrUsrWds + i, dirBuf[i]);
		for (int i = 0; i < 16; i++)
			put_vm_byte(pb.addr + kOff_ioDrFndrInfo + i, dirBuf[16 + i]);
	}
	else
	{
		for (int i = 0; i < 16; i++)
			put_vm_byte(pb.addr + kOff_ioDrUsrWds + i, 0);
		for (int i = 0; i < 16; i++)
			put_vm_byte(pb.addr + kOff_ioDrFndrInfo + i, 0);
	}

	pb[ioDrNmFls] = int16_t(s_volume.childCount(e->cnid));
	pb[ioDrDirID] = e->cnid;
	pb[ioDrParID] = e->parentDirID;
	pb[ioDrCrDat] = e->crDate;
	pb[ioDrMdDat] = e->modDate;
	pb[ioDrBkDat] = uint32_t(0);
}

static void pbWriteRootDir(PBRef pb, uint32_t nameAddr)
{
	uint32_t now = static_cast<uint32_t>(std::time(nullptr)) + appledouble::kMacEpochOffset;
	pb[ioFlAttrib] = uint8_t(0x10);
	put_vm_byte(pb.addr + 31, 0); /* ioACUser */
	for (int i = 0; i < 16; i++)
		put_vm_byte(pb.addr + kOff_ioDrUsrWds + i, 0);
	for (int i = 0; i < 16; i++)
		put_vm_byte(pb.addr + kOff_ioDrFndrInfo + i, 0);
	pb[ioDrNmFls] = int16_t(s_volume.childCount(kRootDirID));
	pb[ioDrDirID] = kRootDirID;
	pb[ioDrParID] = kRootParentID;
	pb[ioDrCrDat] = now;
	pb[ioDrMdDat] = now;
	pb[ioDrBkDat] = uint32_t(0);
	if (nameAddr) writePascalString(nameAddr, "Shared");
}

static uint16_t PbGetCatInfo(PBRef pb, bool isHFS)
{
	uint32_t dirID = pbResolveDir(pb, isHFS);
	int16_t index = pb[ioFDirIndex];
	uint32_t nameAddr = pb[ioNamePtr];

	dbg_printf("[ExtFS] PbGetCatInfo dir=%u idx=%d\n", dirID, index);

	const storage::CatalogEntry *e = nullptr;

	if (index > 0)
	{
		/* Indexed enumeration */
		if (dirID == kRootParentID)
		{
			if (index == 1)
			{
				pbWriteRootDir(pb, nameAddr);
				return 0;
			}
			return 43; /* fnfErr */
		}
		e = s_volume.nthChild(dirID, index);
		if (!e) return 43; /* fnfErr — end of enumeration */
	}
	else if (index == 0 && nameAddr != 0)
	{
		/* By-name lookup */
		std::string name = readPascalString(nameAddr);
		if (!name.empty())
			e = s_volume.findByPath(dirID, name);
		else
			e = s_volume.findByCNID(dirID);
	}
	else
	{
		/* index <= 0 or no name: info about dirID itself */
		e = s_volume.findByCNID(dirID);
	}

	/* Synthesize root if needed */
	if (!e && (dirID == kRootDirID || dirID == kRootParentID))
	{
		pbWriteRootDir(pb, nameAddr);
		return 0;
	}

	if (!e) return 43; /* fnfErr */

	if (nameAddr) writePascalString(nameAddr, e->macName);

	if (e->isDirectory)
		pbWriteDirFields(pb, e);
	else
		pbWriteFileFields(pb, e);

	return 0;
}

static uint16_t PbGetFileInfo(PBRef pb, bool isHFS)
{
	uint32_t dirID = pbResolveDir(pb, isHFS);
	uint32_t nameAddr = pb[ioNamePtr];
	if (nameAddr == 0) return 50; /* paramErr */

	std::string name = readPascalString(nameAddr);
	dbg_printf("[ExtFS] PbGetFileInfo dir=%u name=\"%s\"\n", dirID, name.c_str());

	auto *e = s_volume.findByPath(dirID, name);
	if (!e) return 43; /* fnfErr */

	pbWriteFileFields(pb, e, isHFS);
	return 0;
}

static uint16_t PbOpenFork(PBRef pb, uint32_t regParam[], storage::ForkType forkType, bool isHFS)
{
	uint32_t dirID = pbResolveDir(pb, isHFS);
	uint32_t nameAddr = pb[ioNamePtr];
	uint8_t perm = pb[ioPermssn];

	if (nameAddr == 0) return 50; /* paramErr */

	std::string name = readPascalString(nameAddr);
	dbg_printf("[ExtFS] PbOpen%s dir=%u name=\"%s\"\n",
			   forkType == storage::ForkType::Resource ? "RF" : "", dirID, name.c_str());

	auto *e = s_volume.findByPath(dirID, name);
	if (!e) return fmErrToReg(storage::FMErr::kFnfErr);

	uint32_t size = 0;
	storage::FMErr err;
	uint32_t handle = s_volume.openFork(e->cnid, forkType, size, err, perm);
	if (handle == 0) return fmErrToReg(err);

	regParam[0] = handle;
	regParam[1] = size;
	regParam[2] = e->cnid;
	regParam[3] = e->parentDirID;
	return 0;
}

static uint16_t PbOpen(PBRef pb, uint32_t regParam[], bool isHFS)
{
	return PbOpenFork(pb, regParam, storage::ForkType::Data, isHFS);
}

static uint16_t PbOpenRF(PBRef pb, uint32_t regParam[], bool isHFS)
{
	return PbOpenFork(pb, regParam, storage::ForkType::Resource, isHFS);
}

/* ── Phase 4: mutation handlers ───────────────────── */

static uint16_t PbCreate(PBRef pb, bool isHFS)
{
	uint32_t dirID = pbResolveDir(pb, isHFS);
	uint32_t nameAddr = pb[ioNamePtr];
	if (nameAddr == 0) return 50; /* paramErr */

	std::string name = readPascalString(nameAddr);
	dbg_printf("[ExtFS] PbCreate dir=%u name=\"%s\"\n", dirID, name.c_str());

	storage::FMErr err;
	uint32_t cnid = s_volume.createFile(dirID, name, err);
	if (cnid == 0) return fmErrToReg(err);

	dbg_printf("[ExtFS]   → cnid=%u\n", cnid);
	return 0;
}

static uint16_t PbDelete(PBRef pb, bool isHFS)
{
	uint32_t dirID = pbResolveDir(pb, isHFS);
	uint32_t nameAddr = pb[ioNamePtr];
	if (nameAddr == 0) return 50; /* paramErr */

	std::string name = readPascalString(nameAddr);
	dbg_printf("[ExtFS] PbDelete dir=%u name=\"%s\"\n", dirID, name.c_str());

	auto err = s_volume.remove(dirID, name);
	return fmErrToReg(err);
}

static uint16_t PbRename(PBRef pb, bool isHFS)
{
	uint32_t dirID = pbResolveDir(pb, isHFS);
	uint32_t nameAddr = pb[ioNamePtr];
	uint32_t newNameAddr = pb[ioMisc];
	if (nameAddr == 0 || newNameAddr == 0) return 50; /* paramErr */

	std::string oldName = readPascalString(nameAddr);
	std::string newName = readPascalString(newNameAddr);
	dbg_printf("[ExtFS] PbRename dir=%u old=\"%s\" new=\"%s\"\n", dirID, oldName.c_str(),
			   newName.c_str());

	auto err = s_volume.rename(dirID, oldName, newName);
	return fmErrToReg(err);
}

static uint16_t PbDirCreate(PBRef pb, bool isHFS)
{
	uint32_t dirID = pbResolveDir(pb, isHFS);
	uint32_t nameAddr = pb[ioNamePtr];
	if (nameAddr == 0) return 50; /* paramErr */

	std::string name = readPascalString(nameAddr);
	dbg_printf("[ExtFS] PbDirCreate dir=%u name=\"%s\"\n", dirID, name.c_str());

	storage::FMErr err;
	uint32_t cnid = s_volume.createDir(dirID, name, err);
	if (cnid == 0) return fmErrToReg(err);

	pb[ioDrDirID] = cnid;
	dbg_printf("[ExtFS]   → cnid=%u\n", cnid);
	return 0;
}

static uint16_t PbCatMove(PBRef pb, bool isHFS)
{
	uint32_t dirID = pbResolveDir(pb, isHFS);
	uint32_t nameAddr = pb[ioNamePtr];
	if (nameAddr == 0) return 50; /* paramErr */

	uint32_t dstDirID = pb[ioNewDirID];
	std::string name = readPascalString(nameAddr);
	dbg_printf("[ExtFS] PbCatMove srcDir=%u name=\"%s\" dstDir=%u\n", dirID, name.c_str(),
			   dstDirID);

	auto err = s_volume.move(dirID, name, dstDirID);
	return fmErrToReg(err);
}

/* ── Phase 5: metadata handlers ───────────────────── */

static uint16_t PbSetFileInfo(PBRef pb, bool isHFS)
{
	uint32_t dirID = pbResolveDir(pb, isHFS);
	uint32_t nameAddr = pb[ioNamePtr];
	if (nameAddr == 0) return 50; /* paramErr */

	std::string name = readPascalString(nameAddr);
	dbg_printf("[ExtFS] PbSetFileInfo dir=%u name=\"%s\"\n", dirID, name.c_str());

	auto *e = s_volume.findByPath(dirID, name);
	if (!e) return fmErrToReg(storage::FMErr::kFnfErr);

	/* Read FInfo (16 bytes) directly from the PB at offset 32 */
	uint32_t pbAddr = pb.addr;
	uint32_t type = detail::pbRead<uint32_t>(pbAddr + kOff_ioFlFndrInfo);
	uint32_t creator = detail::pbRead<uint32_t>(pbAddr + kOff_ioFlFndrInfo + 4);
	uint16_t flags = detail::pbRead<uint16_t>(pbAddr + kOff_ioFlFndrInfo + 8);
	uint32_t location = detail::pbRead<uint32_t>(pbAddr + kOff_ioFlFndrInfo + 10);
	uint16_t folder = detail::pbRead<uint16_t>(pbAddr + kOff_ioFlFndrInfo + 14);

	auto err = s_volume.setFileInfo(e->cnid, type, creator, flags, location, folder);
	return fmErrToReg(err);
}

static uint16_t PbSetCatInfo(PBRef pb, bool isHFS)
{
	uint32_t dirID = pbResolveDir(pb, isHFS);
	uint32_t nameAddr = pb[ioNamePtr];
	if (nameAddr == 0) return 50; /* paramErr */

	std::string name = readPascalString(nameAddr);
	dbg_printf("[ExtFS] PbSetCatInfo dir=%u name=\"%s\"\n", dirID, name.c_str());

	auto *e = s_volume.findByPath(dirID, name);
	if (!e) return fmErrToReg(storage::FMErr::kFnfErr);

	uint32_t pbAddr = pb.addr;
	if (e->isDirectory)
	{
		/* Read DInfo (16) + DXInfo (16) from PB offsets 32 and 84 */
		uint8_t buf[32];
		for (int i = 0; i < 16; i++)
			buf[i] = get_vm_byte(pbAddr + kOff_ioDrUsrWds + i);
		for (int i = 0; i < 16; i++)
			buf[16 + i] = get_vm_byte(pbAddr + kOff_ioDrFndrInfo + i);
		auto err = s_volume.setDirInfo(e->cnid, buf);
		return fmErrToReg(err);
	}
	else
	{
		/* Read FInfo from PB offset 32 */
		uint32_t type = detail::pbRead<uint32_t>(pbAddr + kOff_ioFlFndrInfo);
		uint32_t creator = detail::pbRead<uint32_t>(pbAddr + kOff_ioFlFndrInfo + 4);
		uint16_t flags = detail::pbRead<uint16_t>(pbAddr + kOff_ioFlFndrInfo + 8);
		uint32_t location = detail::pbRead<uint32_t>(pbAddr + kOff_ioFlFndrInfo + 10);
		uint16_t folder = detail::pbRead<uint16_t>(pbAddr + kOff_ioFlFndrInfo + 14);
		auto err = s_volume.setFileInfo(e->cnid, type, creator, flags, location, folder);
		return fmErrToReg(err);
	}
}

/* ── Phase 6: WD handlers ─────────────────────────── */

static uint16_t PbOpenWD(PBRef pb)
{
	uint32_t dirID = pb[ioWDDirID];
	uint32_t procID = pb[ioWDProcID];
	dbg_printf("[ExtFS] PbOpenWD dir=%u proc=%u\n", dirID, procID);

	uint32_t wdRef = s_volume.openWD(dirID, procID);
	pb[ioVRefNum] = static_cast<int16_t>(-(static_cast<int32_t>(wdRef) + 32000));
	return 0;
}

static uint16_t PbCloseWD(PBRef pb)
{
	int16_t vRefNum = pb[ioVRefNum];
	auto wdRef = static_cast<uint32_t>(-(static_cast<int32_t>(vRefNum)) - 32000);
	dbg_printf("[ExtFS] PbCloseWD vRefNum=%d wdRef=%u\n", vRefNum, wdRef);

	s_volume.closeWD(wdRef);
	return 0;
}

static uint16_t PbGetWDInfo(PBRef pb)
{
	int16_t vRefNum = pb[ioVRefNum];
	int16_t wdIndex = pb[ioWDIndex];

	dbg_printf("[ExtFS] PbGetWDInfo vRefNum=%d wdIndex=%d\n", vRefNum, wdIndex);

	/* Only handle direct lookup (ioWDIndex == 0) */
	if (wdIndex != 0) return 35; /* nsvErr */

	if (vRefNum == kGuestVRefNum || vRefNum == kGuestDriveNum)
	{
		pb[ioWDProcID] = uint32_t(0);
		pb[ioWDDirID] = uint32_t(kRootDirID);
	}
	else
	{
		auto wdRef = static_cast<uint32_t>(-(static_cast<int32_t>(vRefNum)) - 32000);
		uint32_t dirID = s_volume.wdToDirID(wdRef);
		if (dirID == 0) return 35; /* nsvErr */
		pb[ioWDProcID] = s_volume.wdToProcID(wdRef);
		pb[ioWDDirID] = dirID;
	}

	pb[ioWDVRefNum] = static_cast<int16_t>(kGuestVRefNum);

	/* Write volume name "Shared" to ioNamePtr if set */
	uint32_t nameAddr = pb[ioNamePtr];
	if (nameAddr != 0) writePascalString(nameAddr, "Shared");

	return 0;
}

/* ── Register-based handlers ──────────────────────── */

static void RegVersion(uint32_t regParam[], uint16_t &regResult)
{
	if (!s_volume.isMounted()) s_volume.mount("shared");
	regParam[0] = s_volume.isMounted() ? 1 : 0;
	regResult = 0;
	dbglog_writeCStr((char *)"ExtFS: version query → ");
	dbglog_writeNum(regParam[0]);
	dbglog_writeCStr((char *)"\n");
}

static void RegGetVol(uint32_t regParam[], uint16_t &regResult)
{
	uint32_t files, dirs, bytes;
	s_volume.volumeStats(files, dirs, bytes);
	regParam[0] = files;
	regParam[1] = bytes;
	regParam[2] = dirs;
	regResult = 0;
	dbg_printf("[ExtFS] GetVol → %u files, %u dirs, %u bytes\n", files, dirs, bytes);
}

static void RegRead(uint32_t regParam[], uint16_t &regResult)
{
	uint32_t handle = regParam[0];
	uint32_t offset = regParam[1];
	uint32_t count = regParam[2];
	uint32_t guestBuf = regParam[3];

	dbg_printf("[ExtFS] Read h=%u off=%u cnt=%u\n", handle, offset, count);

	std::vector<uint8_t> buf(count);
	uint32_t got = 0;
	auto err = s_volume.readFork(handle, offset, buf, got);
	if (err != storage::FMErr::kNoErr)
	{
		regResult = fmErrToReg(err);
		return;
	}

	for (uint32_t i = 0; i < got; i++)
		put_vm_byte(guestBuf + i, buf[i]);
	regParam[0] = got;
	dbg_printf("[ExtFS]   → read %u bytes\n", got);
	regResult = 0;
}

static void RegClose(uint32_t regParam[], uint16_t &regResult)
{
	uint32_t handle = regParam[0];
	dbg_printf("[ExtFS] Close h=%u\n", handle);
	s_volume.closeFork(handle);
	regResult = 0;
}

static void RegGetWDInfo(uint32_t regParam[], uint16_t &regResult)
{
	uint32_t wdRef = regParam[0];
	dbg_printf("[ExtFS] GetWDInfo wd=%u\n", wdRef);
	uint32_t dirID = s_volume.wdToDirID(wdRef);
	if (dirID != 0)
	{
		regParam[0] = s_volume.wdToProcID(wdRef);
		regParam[1] = dirID;
		regResult = 0;
	}
	else
	{
		regResult = 43;
	}
}

static void RegOpenWD(uint32_t regParam[], uint16_t &regResult)
{
	uint32_t dirID = regParam[1];
	uint32_t procID = regParam[2];
	uint32_t wdRef = s_volume.openWD(dirID, procID);
	regParam[0] = wdRef;
	dbg_printf("[ExtFS] OpenWD dir=%u proc=%u → wd=%u\n", dirID, procID, wdRef);
	regResult = 0;
}

static void RegCloseWD(uint32_t regParam[], uint16_t &regResult)
{
	uint32_t wdRef = regParam[0];
	dbg_printf("[ExtFS] CloseWD wd=%u\n", wdRef);
	s_volume.closeWD(wdRef);
	regResult = 0;
}

static void RegDbgLog(uint32_t regParam[], uint16_t &regResult)
{
	std::string line = guestFormatLog(regParam[0], regParam);
	guestConsoleAppend(line);
	regResult = 0;
}

static void RegLogTrap(uint32_t regParam[], uint16_t &regResult)
{
	extfsLogTrap(static_cast<uint16_t>(regParam[0]), regParam[1],
				 static_cast<uint16_t>(regParam[2]), static_cast<int16_t>(regParam[3]),
				 static_cast<uint16_t>(regParam[4]));
	regResult = 0;
}

static void RegGuestVars(uint32_t regParam[], uint16_t &regResult)
{
	static uint32_t s_guestVarsPtr = 0;
	if (regParam[1] != 0) s_guestVarsPtr = regParam[0];
	regParam[0] = s_guestVarsPtr;
	regResult = 0;
}

static void RegFatal(uint32_t regParam[], uint16_t &regResult)
{
	std::string msg = guestFormatLog(regParam[0], regParam);
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

static void RegWrite(uint32_t regParam[], uint16_t &regResult)
{
	uint32_t handle = regParam[0];
	uint32_t offset = regParam[1];
	uint32_t count = regParam[2];
	uint32_t guestBuf = regParam[3];

	dbg_printf("[ExtFS] Write h=%u off=%u cnt=%u\n", handle, offset, count);

	std::vector<uint8_t> data(count);
	for (uint32_t i = 0; i < count; i++)
		data[i] = get_vm_byte(guestBuf + i);

	uint32_t written = 0;
	auto err = s_volume.writeFork(handle, offset, data, written);
	if (err != storage::FMErr::kNoErr)
	{
		regResult = fmErrToReg(err);
		return;
	}
	regParam[0] = written;
	dbg_printf("[ExtFS]   → wrote %u bytes\n", written);
	regResult = 0;
}

static void RegSetEOF(uint32_t regParam[], uint16_t &regResult)
{
	uint32_t handle = regParam[0];
	uint32_t newSize = regParam[1];
	dbg_printf("[ExtFS] SetEOF h=%u size=%u\n", handle, newSize);
	auto err = s_volume.setEOF(handle, newSize);
	regResult = fmErrToReg(err);
}

/* ── Dispatch ─────────────────────────────────────── */

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
		case kExtFSGetWDInfo:
			RegGetWDInfo(regParam, regResult);
			break;
		case kExtFSOpenWD:
			RegOpenWD(regParam, regResult);
			break;
		case kExtFSCloseWD:
			RegCloseWD(regParam, regResult);
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

		/* PB-based commands */
		case kPB_GetCatInfo:
			regResult = PbGetCatInfo(PBRef{regParam[0]}, regParam[1] != 0);
			break;
		case kPB_GetFileInfo:
			regResult = PbGetFileInfo(PBRef{regParam[0]}, regParam[1] != 0);
			break;
		case kPB_Open:
			regResult = PbOpen(PBRef{regParam[0]}, regParam, regParam[1] != 0);
			break;
		case kPB_OpenRF:
			regResult = PbOpenRF(PBRef{regParam[0]}, regParam, regParam[1] != 0);
			break;
		case kPB_Create:
			regResult = PbCreate(PBRef{regParam[0]}, regParam[1] != 0);
			break;
		case kPB_Delete:
			regResult = PbDelete(PBRef{regParam[0]}, regParam[1] != 0);
			break;
		case kPB_Rename:
			regResult = PbRename(PBRef{regParam[0]}, regParam[1] != 0);
			break;
		case kPB_DirCreate:
			regResult = PbDirCreate(PBRef{regParam[0]}, regParam[1] != 0);
			break;
		case kPB_CatMove:
			regResult = PbCatMove(PBRef{regParam[0]}, regParam[1] != 0);
			break;
		case kPB_SetFileInfo:
			regResult = PbSetFileInfo(PBRef{regParam[0]}, regParam[1] != 0);
			break;
		case kPB_SetCatInfo:
			regResult = PbSetCatInfo(PBRef{regParam[0]}, regParam[1] != 0);
			break;
		case kPB_OpenWD:
			regResult = PbOpenWD(PBRef{regParam[0]});
			break;
		case kPB_CloseWD:
			regResult = PbCloseWD(PBRef{regParam[0]});
			break;
		case kPB_GetWDInfo:
			regResult = PbGetWDInfo(PBRef{regParam[0]});
			break;
		case kPB_SetDefaultVRefNum:
			s_volume.setDefaultVRefNum(static_cast<int16_t>(regParam[0]));
			regResult = 0;
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
			if (!s_volume.validateCatalog())
				dbg_printf("[ExtFS] *** CATALOG VALIDATION FAILED after cmd=0x%03x ***\n", cmd);
			break;
		default:
			break;
	}
}
