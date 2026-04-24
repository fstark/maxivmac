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
static constexpr uint16_t kExtFSGetCatInfo = 0x202;
static constexpr uint16_t kExtFSGetCatInfoName = 0x203;
static constexpr uint16_t kExtFSOpen = 0x204;
static constexpr uint16_t kExtFSRead = 0x205;
static constexpr uint16_t kExtFSClose = 0x206;
static constexpr uint16_t kExtFSGetFileInfo = 0x207;
static constexpr uint16_t kExtFSReadDir = 0x208;
static constexpr uint16_t kExtFSObjByName = 0x209;
static constexpr uint16_t kExtFSGetWDInfo = 0x20A;
static constexpr uint16_t kExtFSOpenWD = 0x20B;
static constexpr uint16_t kExtFSCloseWD = 0x20C;
static constexpr uint16_t kExtFSDbgLog = 0x20D;
static constexpr uint16_t kExtFSGuestVars = 0x20E;
static constexpr uint16_t kExtFSFatal = 0x0214;
static constexpr uint16_t kExtFSCreateFile = 0x210;
static constexpr uint16_t kExtFSWrite = 0x211;
static constexpr uint16_t kExtFSDeleteFile = 0x212;
static constexpr uint16_t kExtFSSetFileInfo = 0x213;
static constexpr uint16_t kExtFSCreateDir = 0x215;
static constexpr uint16_t kExtFSCatMove = 0x216;
static constexpr uint16_t kExtFSRename = 0x217;
static constexpr uint16_t kExtFSSetEOF = 0x218;
static constexpr uint16_t kExtFSGetDirInfo = 0x219;
static constexpr uint16_t kExtFSSetDirInfo = 0x21A;
static constexpr uint16_t kExtFSLogTrap = 0x20F;

/* ── Coarse commands (Phase 1) ────────────────────── */
static constexpr uint16_t kExtFSOpenByName = 0x220;
static constexpr uint16_t kExtFSGetCatInfoFull = 0x221;
static constexpr uint16_t kExtFSGetFileInfoByName = 0x222;
static constexpr uint16_t kExtFSResolveAndOpen = 0x223;
static constexpr uint16_t kExtFSGetCatInfoResolved = 0x224;
static constexpr uint16_t kExtFSFileOpByName = 0x225;

/* FileOpByName sub-opcodes */
static constexpr uint32_t kFileOpCreate = 0;
static constexpr uint32_t kFileOpDelete = 1;
static constexpr uint32_t kFileOpRename = 2;
static constexpr uint32_t kFileOpSetFileInfo = 3;
static constexpr uint32_t kFileOpSetCatInfo = 4;

/* ── HostVolume instance ──────────────────────────── */

static storage::HostVolume s_volume;

static constexpr uint32_t kRootParentID = storage::HostVolume::kRootParentID;
static constexpr uint32_t kRootDirID = storage::HostVolume::kRootDirID;

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

/* ── GetCatInfoFull helper ─────────────────────────── */

static void doCatInfoFull(uint32_t dirID, int32_t index, uint32_t nameAddr, uint32_t nameBuf,
						  uint32_t regParam[], uint16_t &regResult)
{
	dbg_printf("[ExtFS] GetCatInfoFull dir=%u idx=%d\n", dirID, index);

	const storage::CatalogEntry *e = nullptr;

	if (index > 0)
	{
		/* Indexed enumeration */
		if (dirID == kRootParentID)
		{
			if (index == 1)
			{
				/* Return root volume itself */
				regParam[0] = kRootDirID;
				regParam[1] = 0x10; /* isDir */
				regParam[2] = static_cast<uint32_t>(s_volume.childCount(kRootDirID));
				regParam[3] = 0; /* rsrcSize=0 for dirs */
				regParam[4] = kRootParentID;
				regParam[5] = 0;
				regParam[6] = 0; /* type/creator */
				{
					uint32_t now =
						static_cast<uint32_t>(std::time(nullptr)) + appledouble::kMacEpochOffset;
					regParam[7] = now;
					regParam[8] = now;
				}
				regParam[9] = 0; /* finderFlags */
				if (nameBuf) writePascalString(nameBuf, "Shared");
				regResult = 0;
				return;
			}
			regResult = fmErrToReg(storage::FMErr::kFnfErr);
			return;
		}
		e = s_volume.nthChild(dirID, index);
		if (!e)
		{
			regResult = fmErrToReg(storage::FMErr::kFnfErr);
			return;
		}
	}
	else if (index == 0 && nameAddr != 0)
	{
		/* By-name lookup */
		std::string name = readPascalString(nameAddr);
		if (!name.empty())
		{
			e = s_volume.findByName(dirID, name);
		}
		else
		{
			/* Empty name = info about dirID itself */
			e = s_volume.findByCNID(dirID);
		}
	}
	else
	{
		/* index <= 0: info about dirID itself */
		e = s_volume.findByCNID(dirID);
	}

	/* Synthesize root if needed */
	if (!e && (dirID == kRootDirID || dirID == kRootParentID))
	{
		uint32_t now = static_cast<uint32_t>(std::time(nullptr)) + appledouble::kMacEpochOffset;
		regParam[0] = kRootDirID;
		regParam[1] = 0x10;
		regParam[2] = static_cast<uint32_t>(s_volume.childCount(kRootDirID));
		regParam[3] = 0;
		regParam[4] = kRootParentID;
		regParam[5] = 0;
		regParam[6] = 0;
		regParam[7] = now;
		regParam[8] = now;
		regParam[9] = 0;
		if (nameBuf) writePascalString(nameBuf, "Shared");
		regResult = 0;
		return;
	}

	if (!e)
	{
		regResult = fmErrToReg(storage::FMErr::kFnfErr);
		return;
	}

	regParam[0] = e->cnid;
	regParam[1] = e->isDirectory ? 0x10u : 0u;
	regParam[2] =
		e->isDirectory ? static_cast<uint32_t>(s_volume.childCount(e->cnid)) : e->dataForkSize;
	regParam[3] = e->isDirectory ? 0u : e->rsrcForkSize;
	regParam[4] = e->parentDirID;
	regParam[5] = e->type;
	regParam[6] = e->creator;
	regParam[7] = e->crDate;
	regParam[8] = e->modDate;
	regParam[9] = e->finderFlags;
	regParam[10] = e->fdLocation;
	regParam[11] = e->fdFldr;
	if (nameBuf) writePascalString(nameBuf, e->macName);
	regResult = 0;
}

/* ── Dispatch ─────────────────────────────────────── */

void ExtnExtFSDispatch(uint16_t cmd, uint32_t regParam[], uint16_t &regResult)
{
	switch (cmd)
	{
		case kExtFSVersion:
		{
			if (!s_volume.isMounted()) s_volume.mount("shared");
			regParam[0] = s_volume.isMounted() ? 1 : 0;
			regResult = 0;
			dbglog_writeCStr((char *)"ExtFS: version query → ");
			dbglog_writeNum(regParam[0]);
			dbglog_writeCStr((char *)"\n");
		}
		break;

		case kExtFSGetVol:
		{
			uint32_t files, dirs, bytes;
			s_volume.volumeStats(files, dirs, bytes);
			regParam[0] = files;
			regParam[1] = bytes;
			regParam[2] = dirs;
			regResult = 0;
			dbg_printf("[ExtFS] GetVol → %u files, %u dirs, %u bytes\n", files, dirs, bytes);
		}
		break;

		case kExtFSGetCatInfo:
		{
			uint32_t dirID = regParam[0];
			int32_t index = static_cast<int32_t>(regParam[1]);
			uint32_t nameBuf = regParam[2];

			dbg_printf("[ExtFS] GetCatInfo dir=%u idx=%d\n", dirID, index);

			if (index == 0)
			{
				if (dirID == kRootParentID || dirID == kRootDirID)
				{
					regParam[0] = kRootDirID;
					regParam[1] = 0x10;
					regParam[2] = static_cast<uint32_t>(s_volume.childCount(kRootDirID));
					regParam[3] = kRootParentID;
					if (nameBuf) writePascalString(nameBuf, "Shared");
					regResult = 0;
				}
				else
				{
					auto *e = s_volume.findByCNID(dirID);
					if (e && e->isDirectory)
					{
						regParam[0] = e->cnid;
						regParam[1] = 0x10;
						regParam[2] = static_cast<uint32_t>(s_volume.childCount(e->cnid));
						regParam[3] = e->parentDirID;
						if (nameBuf) writePascalString(nameBuf, e->macName);
						regResult = 0;
					}
					else
					{
						regResult = 43;
					}
				}
			}
			else if (index > 0)
			{
				if (dirID == kRootParentID)
				{
					if (index == 1)
					{
						regParam[0] = kRootDirID;
						regParam[1] = 0x10;
						regParam[2] = static_cast<uint32_t>(s_volume.childCount(kRootDirID));
						regParam[3] = kRootParentID;
						if (nameBuf) writePascalString(nameBuf, "Shared");
						regResult = 0;
					}
					else
					{
						regResult = 43;
					}
					break;
				}
				auto *e = s_volume.nthChild(dirID, index);
				if (e)
				{
					regParam[0] = e->cnid;
					regParam[1] = e->isDirectory ? 0x10u : 0u;
					regParam[2] = e->isDirectory
									  ? static_cast<uint32_t>(s_volume.childCount(e->cnid))
									  : e->dataForkSize;
					regParam[3] = e->parentDirID;
					regParam[4] = e->isDirectory ? 0u : e->rsrcForkSize;
					if (nameBuf) writePascalString(nameBuf, e->macName);
					regResult = 0;
				}
				else
				{
					regResult = 43;
				}
			}
			else
			{
				regResult = 50;
			}
		}
		break;

		case kExtFSGetCatInfoName:
		{
			uint32_t parentDir = regParam[0];
			uint32_t nameAddr = regParam[1];
			uint32_t nameBuf = regParam[2];

			std::string name = readPascalString(nameAddr);
			dbg_printf("[ExtFS] GetCatInfoByName dir=%u name=\"%s\"\n", parentDir, name.c_str());

			if (parentDir == kRootParentID)
			{
				std::string volName = "Shared";
				bool match = (name.size() == volName.size());
				if (match)
				{
					for (size_t i = 0; i < name.size(); i++)
					{
						if (tolower(static_cast<unsigned char>(name[i])) !=
							tolower(static_cast<unsigned char>(volName[i])))
						{
							match = false;
							break;
						}
					}
				}
				if (match)
				{
					regParam[0] = kRootDirID;
					regParam[1] = 0x10;
					regParam[2] = static_cast<uint32_t>(s_volume.childCount(kRootDirID));
					regParam[3] = kRootParentID;
					if (nameBuf) writePascalString(nameBuf, volName);
					regResult = 0;
					break;
				}
				regResult = 43;
				break;
			}

			auto *e = s_volume.findByName(parentDir, name);
			if (e)
			{
				regParam[0] = e->cnid;
				regParam[1] = e->isDirectory ? 0x10u : 0u;
				regParam[2] = e->isDirectory ? static_cast<uint32_t>(s_volume.childCount(e->cnid))
											 : e->dataForkSize;
				regParam[3] = e->parentDirID;
				regParam[4] = e->isDirectory ? 0u : e->rsrcForkSize;
				if (nameBuf) writePascalString(nameBuf, e->macName);
				regResult = 0;
			}
			else
			{
				regResult = 43;
			}
		}
		break;

		case kExtFSOpen:
		{
			uint32_t cnid = regParam[0];
			auto forkType =
				(regParam[1] == 1) ? storage::ForkType::Resource : storage::ForkType::Data;
			uint8_t permission = static_cast<uint8_t>(regParam[2]);
			uint32_t size = 0;
			storage::FMErr err;
			uint32_t handle = s_volume.openFork(cnid, forkType, size, err, permission);
			if (handle == 0)
			{
				regResult = fmErrToReg(err);
				break;
			}
			regParam[0] = handle;
			regParam[1] = size;
			dbg_printf("[ExtFS] Open cnid=%u → handle=%u size=%u\n", cnid, handle, size);
			regResult = 0;
		}
		break;

		case kExtFSRead:
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
				break;
			}

			for (uint32_t i = 0; i < got; i++)
				put_vm_byte(guestBuf + i, buf[i]);
			regParam[0] = got;
			dbg_printf("[ExtFS]   → read %u bytes\n", got);
			regResult = 0;
		}
		break;

		case kExtFSClose:
		{
			uint32_t handle = regParam[0];
			dbg_printf("[ExtFS] Close h=%u\n", handle);
			s_volume.closeFork(handle);
			regResult = 0;
		}
		break;

		case kExtFSGetFileInfo:
		{
			uint32_t cnid = regParam[0];

			if (cnid == kRootDirID || cnid == kRootParentID)
			{
				uint32_t now =
					static_cast<uint32_t>(std::time(nullptr)) + appledouble::kMacEpochOffset;
				regParam[0] = 0;
				regParam[1] = 0;
				regParam[2] = now;
				regParam[3] = now;
				regParam[4] = 0;
				regResult = 0;
				break;
			}

			auto *e = s_volume.findByCNID(cnid);
			if (e)
			{
				regParam[0] = e->type;
				regParam[1] = e->creator;
				regParam[2] = e->crDate;
				regParam[3] = e->modDate;
				regParam[4] = e->finderFlags;
				regResult = 0;
			}
			else
			{
				regResult = 43;
			}
		}
		break;

		case kExtFSReadDir:
		{
			uint32_t dirID = regParam[0];
			regParam[0] = static_cast<uint32_t>(s_volume.childCount(dirID));
			dbg_printf("[ExtFS] ReadDir dir=%u → %u children\n", dirID, regParam[0]);
			regResult = 0;
		}
		break;

		case kExtFSObjByName:
		{
			uint32_t parentDir = regParam[0];
			std::string name = readPascalString(regParam[1]);
			auto *e = s_volume.findByName(parentDir, name);
			regParam[0] = e ? e->cnid : 0;
			dbg_printf("[ExtFS] ObjByName dir=%u name=\"%s\" → cnid=%u\n", parentDir, name.c_str(),
					   regParam[0]);
			regResult = 0;
		}
		break;

		case kExtFSGetWDInfo:
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
		break;

		case kExtFSOpenWD:
		{
			uint32_t dirID = regParam[1];
			uint32_t procID = regParam[2];
			uint32_t wdRef = s_volume.openWD(dirID, procID);
			regParam[0] = wdRef;
			dbg_printf("[ExtFS] OpenWD dir=%u proc=%u → wd=%u\n", dirID, procID, wdRef);
			regResult = 0;
		}
		break;

		case kExtFSCloseWD:
		{
			uint32_t wdRef = regParam[0];
			dbg_printf("[ExtFS] CloseWD wd=%u\n", wdRef);
			s_volume.closeWD(wdRef);
			regResult = 0;
		}
		break;

		case kExtFSDbgLog:
		{
			std::string line = guestFormatLog(regParam[0], regParam);
			guestConsoleAppend(line);
			regResult = 0;
		}
		break;

		case kExtFSLogTrap:
		{
			extfsLogTrap(static_cast<uint16_t>(regParam[0]), regParam[1],
						 static_cast<uint16_t>(regParam[2]), static_cast<int16_t>(regParam[3]),
						 static_cast<uint16_t>(regParam[4]));
			regResult = 0;
		}
		break;

		case kExtFSGuestVars:
		{
			static uint32_t s_guestVarsPtr = 0;
			if (regParam[1] != 0) s_guestVarsPtr = regParam[0];
			regParam[0] = s_guestVarsPtr;
			regResult = 0;
		}
		break;

		case kExtFSFatal:
		{
			std::string msg = guestFormatLog(regParam[0], regParam);
			guestConsoleAppend("FATAL: " + msg);
			fprintf(stderr, "\n[GUEST FATAL] (insn #%u) %s\n", (unsigned)g_instructionCount,
					msg.c_str());
			DumpRecentDisasm();
			fflush(stderr);
			if (g_debuggerActive)
			{
				Debugger::instance()->stop("GUEST FATAL: " + msg);
				regResult = 0;
				break;
			}
			std::exit(EXIT_FAILURE);
		}
		break;

		case kExtFSCreateFile:
		{
			uint32_t parentDir = regParam[0];
			std::string macName = readPascalString(regParam[1]);
			dbg_printf("[ExtFS] CreateFile dir=%u name=\"%s\"\n", parentDir, macName.c_str());

			storage::FMErr err;
			uint32_t cnid = s_volume.createFile(parentDir, macName, err);
			if (cnid == 0)
			{
				regResult = fmErrToReg(err);
				break;
			}
			regParam[0] = cnid;
			dbg_printf("[ExtFS]   → cnid=%u\n", cnid);
			regResult = 0;
		}
		break;

		case kExtFSWrite:
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
				break;
			}
			regParam[0] = written;
			dbg_printf("[ExtFS]   → wrote %u bytes\n", written);
			regResult = 0;
		}
		break;

		case kExtFSDeleteFile:
		{
			uint32_t parentDir = regParam[0];
			std::string macName = readPascalString(regParam[1]);
			dbg_printf("[ExtFS] Delete dir=%u name=\"%s\"\n", parentDir, macName.c_str());
			auto err = s_volume.remove(parentDir, macName);
			regResult = fmErrToReg(err);
		}
		break;

		case kExtFSSetFileInfo:
		{
			uint32_t cnid = regParam[0];
			uint32_t type = regParam[1];
			uint32_t creator = regParam[2];
			uint16_t flags = static_cast<uint16_t>(regParam[3]);
			uint32_t location = regParam[4];
			uint16_t folder = static_cast<uint16_t>(regParam[5]);
			dbg_printf("[ExtFS] setFileInfo cnid=%u type='%.4s' creator='%.4s' flags=0x%04x\n",
					   cnid, reinterpret_cast<const char *>(&type),
					   reinterpret_cast<const char *>(&creator), flags);
			auto err = s_volume.setFileInfo(cnid, type, creator, flags, location, folder);
			regResult = fmErrToReg(err);
		}
		break;

		case kExtFSCreateDir:
		{
			uint32_t parentDir = regParam[0];
			std::string macName = readPascalString(regParam[1]);
			dbg_printf("[ExtFS] CreateDir dir=%u name=\"%s\"\n", parentDir, macName.c_str());

			storage::FMErr err;
			uint32_t cnid = s_volume.createDir(parentDir, macName, err);
			if (cnid == 0)
			{
				regResult = fmErrToReg(err);
				break;
			}
			regParam[0] = cnid;
			dbg_printf("[ExtFS]   → cnid=%u\n", cnid);
			regResult = 0;
		}
		break;

		case kExtFSCatMove:
		{
			uint32_t srcDir = regParam[0];
			std::string macName = readPascalString(regParam[1]);
			uint32_t dstDir = regParam[2];
			dbg_printf("[ExtFS] CatMove srcDir=%u name=\"%s\" dstDir=%u\n", srcDir, macName.c_str(),
					   dstDir);
			auto err = s_volume.move(srcDir, macName, dstDir);
			regResult = fmErrToReg(err);
		}
		break;

		case kExtFSRename:
		{
			uint32_t dirID = regParam[0];
			std::string oldName = readPascalString(regParam[1]);
			std::string newName = readPascalString(regParam[2]);
			dbg_printf("[ExtFS] Rename dir=%u old=\"%s\" new=\"%s\"\n", dirID, oldName.c_str(),
					   newName.c_str());
			auto err = s_volume.rename(dirID, oldName, newName);
			regResult = fmErrToReg(err);
		}
		break;

		case kExtFSSetEOF:
		{
			uint32_t handle = regParam[0];
			uint32_t newSize = regParam[1];
			dbg_printf("[ExtFS] SetEOF h=%u size=%u\n", handle, newSize);
			auto err = s_volume.setEOF(handle, newSize);
			regResult = fmErrToReg(err);
		}
		break;

		case kExtFSGetDirInfo:
		{
			uint32_t cnid = regParam[0];
			uint32_t guestBuf = regParam[1];
			uint8_t buf[32] = {};
			if (s_volume.getDirInfo(cnid, buf))
			{
				for (int i = 0; i < 32; i++)
					put_vm_byte(guestBuf + i, buf[i]);
				regResult = 0;
			}
			else
			{
				regResult = 43;
			}
		}
		break;

		case kExtFSSetDirInfo:
		{
			uint32_t cnid = regParam[0];
			uint32_t guestBuf = regParam[1];
			uint8_t buf[32];
			for (int i = 0; i < 32; i++)
				buf[i] = get_vm_byte(guestBuf + i);
			auto err = s_volume.setDirInfo(cnid, buf);
			regResult = fmErrToReg(err);
		}
		break;

			/* ── Coarse commands (Phase 1) ────────────────── */

		case kExtFSOpenByName:
		{
			uint32_t dirID = regParam[0];
			std::string name = readPascalString(regParam[1]);
			auto forkType =
				(regParam[2] == 1) ? storage::ForkType::Resource : storage::ForkType::Data;
			uint8_t permission = static_cast<uint8_t>(regParam[3]);

			dbg_printf("[ExtFS] OpenByName dir=%u name=\"%s\" fork=%u\n", dirID, name.c_str(),
					   regParam[2]);

			auto *e = s_volume.findByName(dirID, name);
			if (!e)
			{
				regResult = fmErrToReg(storage::FMErr::kFnfErr);
				break;
			}

			uint32_t size = 0;
			storage::FMErr err;
			uint32_t handle = s_volume.openFork(e->cnid, forkType, size, err, permission);
			if (handle == 0)
			{
				regResult = fmErrToReg(err);
				break;
			}

			regParam[0] = handle;
			regParam[1] = size;
			regParam[2] = e->cnid;
			regResult = 0;
		}
		break;

		case kExtFSGetFileInfoByName:
		{
			int16_t vRefNum = static_cast<int16_t>(regParam[0]);
			uint32_t rawDirID = regParam[1];
			std::string name = readPascalString(regParam[2]);

			uint32_t dirID = s_volume.resolveDir(vRefNum, rawDirID);
			dbg_printf("[ExtFS] GetFileInfoByName dir=%u name=\"%s\"\n", dirID, name.c_str());

			auto *e = s_volume.findByPath(dirID, name);
			if (!e)
			{
				regResult = fmErrToReg(storage::FMErr::kFnfErr);
				break;
			}

			regParam[0] = e->cnid;
			regParam[1] = e->dataForkSize;
			regParam[2] = e->rsrcForkSize;
			regParam[3] = e->type;
			regParam[4] = e->creator;
			regParam[5] = e->crDate;
			regParam[6] = e->modDate;
			regParam[7] = e->finderFlags;
			regParam[8] = e->parentDirID;
			regParam[9] = e->fdLocation;
			regParam[10] = e->fdFldr;
			regResult = 0;
		}
		break;

		case kExtFSGetCatInfoFull:
		{
			uint32_t dirID = regParam[0];
			int32_t index = static_cast<int32_t>(regParam[1]);
			uint32_t nameAddr = regParam[2];
			uint32_t nameBuf = regParam[3];
			doCatInfoFull(dirID, index, nameAddr, nameBuf, regParam, regResult);
		}
		break;

		case kExtFSResolveAndOpen:
		{
			int16_t vRefNum = static_cast<int16_t>(regParam[0]);
			uint32_t rawDirID = regParam[1];
			std::string name = readPascalString(regParam[2]);
			auto forkType =
				(regParam[3] == 1) ? storage::ForkType::Resource : storage::ForkType::Data;
			uint8_t permission = static_cast<uint8_t>(regParam[4]);

			uint32_t dirID = s_volume.resolveDir(vRefNum, rawDirID);
			dbg_printf("[ExtFS] ResolveAndOpen vref=%d dir=%u→%u name=\"%s\"\n", vRefNum, rawDirID,
					   dirID, name.c_str());

			auto *e = s_volume.findByPath(dirID, name);
			if (!e)
			{
				regResult = fmErrToReg(storage::FMErr::kFnfErr);
				break;
			}

			uint32_t size = 0;
			storage::FMErr err;
			uint32_t handle = s_volume.openFork(e->cnid, forkType, size, err, permission);
			if (handle == 0)
			{
				regResult = fmErrToReg(err);
				break;
			}

			regParam[0] = handle;
			regParam[1] = size;
			regParam[2] = e->cnid;
			regParam[3] = dirID; /* resolved dirID for guest FCB */
			regResult = 0;
		}
		break;

		case kExtFSGetCatInfoResolved:
		{
			int16_t vRefNum = static_cast<int16_t>(regParam[0]);
			uint32_t rawDirID = regParam[1];
			int32_t index = static_cast<int32_t>(regParam[2]);
			uint32_t nameAddr = regParam[3];
			uint32_t nameBuf = regParam[4];

			uint32_t dirID = s_volume.resolveDir(vRefNum, rawDirID);
			dbg_printf("[ExtFS] GetCatInfoResolved vref=%d dir=%u→%u idx=%d\n", vRefNum, rawDirID,
					   dirID, index);

			doCatInfoFull(dirID, index, nameAddr, nameBuf, regParam, regResult);
		}
		break;

		case kExtFSFileOpByName:
		{
			int16_t vRefNum = static_cast<int16_t>(regParam[0]);
			uint32_t rawDirID = regParam[1];
			uint32_t nameAddr = regParam[2];
			uint32_t opcode = regParam[3];

			uint32_t dirID = s_volume.resolveDir(vRefNum, rawDirID);
			std::string name = readPascalString(nameAddr);

			dbg_printf("[ExtFS] FileOpByName op=%u dir=%u name=\"%s\"\n", opcode, dirID,
					   name.c_str());

			switch (opcode)
			{
				case kFileOpCreate:
				{
					storage::FMErr err;
					uint32_t cnid = s_volume.createFile(dirID, name, err);
					if (cnid == 0)
					{
						regResult = fmErrToReg(err);
						break;
					}
					regParam[0] = cnid;
					regResult = 0;
				}
				break;

				case kFileOpDelete:
				{
					auto err = s_volume.remove(dirID, name);
					regResult = fmErrToReg(err);
				}
				break;

				case kFileOpRename:
				{
					std::string newName = readPascalString(regParam[4]);
					auto err = s_volume.rename(dirID, name, newName);
					regResult = fmErrToReg(err);
				}
				break;

				case kFileOpSetFileInfo:
				{
					auto *e = s_volume.findByName(dirID, name);
					if (!e)
					{
						regResult = fmErrToReg(storage::FMErr::kFnfErr);
						break;
					}
					uint32_t type = regParam[4];
					uint32_t creator = regParam[5];
					uint16_t flags = static_cast<uint16_t>(regParam[6]);
					uint32_t location = regParam[7];
					uint16_t folder = static_cast<uint16_t>(regParam[8]);
					auto err =
						s_volume.setFileInfo(e->cnid, type, creator, flags, location, folder);
					regResult = fmErrToReg(err);
				}
				break;

				case kFileOpSetCatInfo:
				{
					auto *e = s_volume.findByName(dirID, name);
					if (!e)
					{
						regResult = fmErrToReg(storage::FMErr::kFnfErr);
						break;
					}
					if (e->isDirectory)
					{
						uint32_t guestBuf = regParam[4];
						uint8_t buf[32];
						for (int i = 0; i < 32; i++)
							buf[i] = get_vm_byte(guestBuf + i);
						auto err = s_volume.setDirInfo(e->cnid, buf);
						regResult = fmErrToReg(err);
					}
					else
					{
						uint32_t type = regParam[4];
						uint32_t creator = regParam[5];
						uint16_t flags = static_cast<uint16_t>(regParam[6]);
						uint32_t location = regParam[7];
						uint16_t folder = static_cast<uint16_t>(regParam[8]);
						auto err =
							s_volume.setFileInfo(e->cnid, type, creator, flags, location, folder);
						regResult = fmErrToReg(err);
					}
				}
				break;

				default:
					regResult = fmErrToReg(storage::FMErr::kParamErr);
					break;
			}
		}
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
		case kExtFSCreateFile:
		case kExtFSCreateDir:
		case kExtFSDeleteFile:
		case kExtFSCatMove:
		case kExtFSRename:
		case kExtFSSetFileInfo:
		case kExtFSFileOpByName:
			if (!s_volume.validateCatalog())
				dbg_printf("[ExtFS] *** CATALOG VALIDATION FAILED after cmd=0x%03x ***\n", cmd);
			break;
		default:
			break;
	}
}
