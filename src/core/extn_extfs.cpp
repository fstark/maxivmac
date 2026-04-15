#include "core/extn_extfs.h"
#include "core/extn_clip.h"
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
			uint32_t files, bytes;
			s_volume.volumeStats(files, bytes);
			regParam[0] = files;
			regParam[1] = bytes;
			regResult = 0;
			fprintf(stderr, "[ExtFS] GetVol → %u files, %u bytes\n", files, bytes);
		}
		break;

		case kExtFSGetCatInfo:
		{
			uint32_t dirID = regParam[0];
			int32_t index = static_cast<int32_t>(regParam[1]);
			uint32_t nameBuf = regParam[2];

			fprintf(stderr, "[ExtFS] GetCatInfo dir=%u idx=%d\n", dirID, index);

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
			fprintf(stderr, "[ExtFS] GetCatInfoByName dir=%u name=\"%s\"\n", parentDir,
					name.c_str());

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
			uint32_t size = 0;
			storage::FMErr err;
			uint32_t handle = s_volume.openFork(cnid, forkType, size, err);
			if (handle == 0)
			{
				regResult = fmErrToReg(err);
				break;
			}
			regParam[0] = handle;
			regParam[1] = size;
			fprintf(stderr, "[ExtFS] Open cnid=%u → handle=%u size=%u\n", cnid, handle, size);
			regResult = 0;
		}
		break;

		case kExtFSRead:
		{
			uint32_t handle = regParam[0];
			uint32_t offset = regParam[1];
			uint32_t count = regParam[2];
			uint32_t guestBuf = regParam[3];

			fprintf(stderr, "[ExtFS] Read h=%u off=%u cnt=%u\n", handle, offset, count);

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
			fprintf(stderr, "[ExtFS]   → read %u bytes\n", got);
			regResult = 0;
		}
		break;

		case kExtFSClose:
		{
			uint32_t handle = regParam[0];
			fprintf(stderr, "[ExtFS] Close h=%u\n", handle);
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
			fprintf(stderr, "[ExtFS] ReadDir dir=%u → %u children\n", dirID, regParam[0]);
			regResult = 0;
		}
		break;

		case kExtFSObjByName:
		{
			uint32_t parentDir = regParam[0];
			std::string name = readPascalString(regParam[1]);
			auto *e = s_volume.findByName(parentDir, name);
			regParam[0] = e ? e->cnid : 0;
			fprintf(stderr, "[ExtFS] ObjByName dir=%u name=\"%s\" → cnid=%u\n", parentDir,
					name.c_str(), regParam[0]);
			regResult = 0;
		}
		break;

		case kExtFSGetWDInfo:
		{
			uint32_t wdRef = regParam[0];
			fprintf(stderr, "[ExtFS] GetWDInfo wd=%u\n", wdRef);
			uint32_t dirID = s_volume.wdToDirID(wdRef);
			if (dirID != 0)
			{
				regParam[0] = 0;
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
			uint32_t wdRef = s_volume.openWD(dirID);
			regParam[0] = wdRef;
			fprintf(stderr, "[ExtFS] OpenWD dir=%u → wd=%u\n", dirID, wdRef);
			regResult = 0;
		}
		break;

		case kExtFSCloseWD:
		{
			uint32_t wdRef = regParam[0];
			fprintf(stderr, "[ExtFS] CloseWD wd=%u\n", wdRef);
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
			fprintf(stderr, "[ExtFS] CreateFile dir=%u name=\"%s\"\n", parentDir, macName.c_str());

			storage::FMErr err;
			uint32_t cnid = s_volume.createFile(parentDir, macName, err);
			if (cnid == 0)
			{
				regResult = fmErrToReg(err);
				break;
			}
			regParam[0] = cnid;
			fprintf(stderr, "[ExtFS]   → cnid=%u\n", cnid);
			regResult = 0;
		}
		break;

		case kExtFSWrite:
		{
			uint32_t handle = regParam[0];
			uint32_t offset = regParam[1];
			uint32_t count = regParam[2];
			uint32_t guestBuf = regParam[3];

			fprintf(stderr, "[ExtFS] Write h=%u off=%u cnt=%u\n", handle, offset, count);

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
			fprintf(stderr, "[ExtFS]   → wrote %u bytes\n", written);
			regResult = 0;
		}
		break;

		case kExtFSDeleteFile:
		{
			uint32_t parentDir = regParam[0];
			std::string macName = readPascalString(regParam[1]);
			fprintf(stderr, "[ExtFS] Delete dir=%u name=\"%s\"\n", parentDir, macName.c_str());
			auto err = s_volume.remove(parentDir, macName);
			regResult = fmErrToReg(err);
		}
		break;

		case kExtFSSetFileInfo:
		{
			uint32_t cnid = regParam[0];
			uint32_t type = regParam[1];
			uint32_t creator = regParam[2];
			fprintf(stderr, "[ExtFS] SetFileInfo cnid=%u\n", cnid);
			auto err = s_volume.setFileInfo(cnid, type, creator);
			regResult = fmErrToReg(err);
		}
		break;

		case kExtFSCreateDir:
		{
			uint32_t parentDir = regParam[0];
			std::string macName = readPascalString(regParam[1]);
			fprintf(stderr, "[ExtFS] CreateDir dir=%u name=\"%s\"\n", parentDir, macName.c_str());

			storage::FMErr err;
			uint32_t cnid = s_volume.createDir(parentDir, macName, err);
			if (cnid == 0)
			{
				regResult = fmErrToReg(err);
				break;
			}
			regParam[0] = cnid;
			fprintf(stderr, "[ExtFS]   → cnid=%u\n", cnid);
			regResult = 0;
		}
		break;

		case kExtFSCatMove:
		{
			uint32_t srcDir = regParam[0];
			std::string macName = readPascalString(regParam[1]);
			uint32_t dstDir = regParam[2];
			fprintf(stderr, "[ExtFS] CatMove srcDir=%u name=\"%s\" dstDir=%u\n", srcDir,
					macName.c_str(), dstDir);
			auto err = s_volume.move(srcDir, macName, dstDir);
			regResult = fmErrToReg(err);
		}
		break;

		case kExtFSRename:
		{
			uint32_t dirID = regParam[0];
			std::string oldName = readPascalString(regParam[1]);
			std::string newName = readPascalString(regParam[2]);
			fprintf(stderr, "[ExtFS] Rename dir=%u old=\"%s\" new=\"%s\"\n", dirID, oldName.c_str(),
					newName.c_str());
			auto err = s_volume.rename(dirID, oldName, newName);
			regResult = fmErrToReg(err);
		}
		break;

		default:
			regResult = 0xFFFF;
			break;
	}
}
