/*
	extfs_log.cpp — SharedDrive structured trap logging.

	Pretty-prints File Manager trap calls intercepted by the
	SharedDrive INIT.  The guest sends numeric parameters via
	a single RPC; this module reads the param block from guest
	RAM and formats a human-friendly log line.
*/

#include "core/extfs_log.h"
#include "core/extn_clip.h" /* guestConsoleAppend */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

/* ------------------------------------------------------------------ */
/*  Guest RAM access                                                  */
/* ------------------------------------------------------------------ */

extern uint8_t get_vm_byte(uint32_t addr);
extern uint16_t get_vm_word(uint32_t addr);
extern uint32_t get_vm_long(uint32_t addr);

/* ------------------------------------------------------------------ */
/*  Param block field offsets (mirroring init.c definitions)           */
/* ------------------------------------------------------------------ */

/* Shared header */
constexpr uint32_t kPB_ioNamePtr = 18;
constexpr uint32_t kPB_ioVRefNum = 22;
constexpr uint32_t kPB_ioRefNum = 24;

/* ioParam variant */
constexpr uint32_t kPB_ioMisc = 28;
constexpr uint32_t kPB_ioReqCount = 36;
constexpr uint32_t kPB_ioActCount = 40;
constexpr uint32_t kPB_ioPosMode = 44;
constexpr uint32_t kPB_ioPosOffset = 46;

/* fileParam / CInfoPBRec variant */
constexpr uint32_t kPB_ioFDirIndex = 28;
constexpr uint32_t kPB_ioFlAttrib = 30;
constexpr uint32_t kPB_ioFlFndrInfo = 32;
constexpr uint32_t kPB_ioFlNum = 48;
constexpr uint32_t kPB_ioFlLgLen = 54;
constexpr uint32_t kPB_ioFlRLgLen = 64;
constexpr uint32_t kPB_ioFlParID = 100;

/* volumeParam variant */
constexpr uint32_t kPB_ioVolIndex = 28;
constexpr uint32_t kPB_ioVNmAlBlks = 46;
constexpr uint32_t kPB_ioVFrBlk = 62;

/* WDParam variant */
constexpr uint32_t kPB_ioWDIndex = 26;
constexpr uint32_t kPB_ioWDVRefNum = 32;
constexpr uint32_t kPB_ioWDDirID = 48;

/* FCBPBRec variant */
constexpr uint32_t kPB_ioFCBIndx = 28;
constexpr uint32_t kPB_ioFCBFlNm = 32;
constexpr uint32_t kPB_ioFCBEOF = 40;
constexpr uint32_t kPB_ioFCBVRefNum = 52;
constexpr uint32_t kPB_ioFCBParID = 58;

/* HFS-specific dirID overlay */
constexpr uint32_t kPB_ioDirID = 48;

/* ------------------------------------------------------------------ */
/*  Helper: read guest Pascal string                                  */
/* ------------------------------------------------------------------ */

static std::string readPStr(uint32_t addr)
{
	if (addr == 0) return "<null>";
	uint8_t len = get_vm_byte(addr);
	std::string s;
	s.reserve(len);
	for (uint8_t i = 0; i < len; i++)
		s.push_back(static_cast<char>(get_vm_byte(addr + 1 + i)));
	return s;
}

/* ------------------------------------------------------------------ */
/*  Helper: read signed 16-bit from guest RAM                         */
/* ------------------------------------------------------------------ */

static int16_t readSWord(uint32_t addr)
{
	return static_cast<int16_t>(get_vm_word(addr));
}

/* ------------------------------------------------------------------ */
/*  FourCC formatter                                                  */
/* ------------------------------------------------------------------ */

static std::string formatFourCC(uint32_t val)
{
	char buf[12];
	char c[4];
	c[0] = static_cast<char>((val >> 24) & 0xFF);
	c[1] = static_cast<char>((val >> 16) & 0xFF);
	c[2] = static_cast<char>((val >> 8) & 0xFF);
	c[3] = static_cast<char>(val & 0xFF);

	bool printable = true;
	for (int i = 0; i < 4; i++)
	{
		if (c[i] < 0x20 || c[i] > 0x7E)
		{
			printable = false;
			break;
		}
	}

	if (printable)
		std::snprintf(buf, sizeof(buf), "'%c%c%c%c'", c[0], c[1], c[2], c[3]);
	else
		std::snprintf(buf, sizeof(buf), "$%08X", val);
	return buf;
}

/* ------------------------------------------------------------------ */
/*  Trap name tables                                                  */
/* ------------------------------------------------------------------ */

static const char *flatTrapName(uint16_t trapNum)
{
	switch (trapNum)
	{
		case 0x00:
			return "Open";
		case 0x01:
			return "Close";
		case 0x02:
			return "Read";
		case 0x03:
			return "Write";
		case 0x07:
			return "GetVolInfo";
		case 0x08:
			return "Create";
		case 0x09:
			return "Delete";
		case 0x0A:
			return "OpenRF";
		case 0x0B:
			return "Rename";
		case 0x0C:
			return "GetFileInfo";
		case 0x0D:
			return "SetFileInfo";
		case 0x0E:
			return "UnmountVol";
		case 0x10:
			return "Allocate";
		case 0x11:
			return "GetEOF";
		case 0x12:
			return "SetEOF";
		case 0x13:
			return "FlushVol";
		case 0x14:
			return "GetVol";
		case 0x15:
			return "SetVol";
		case 0x17:
			return "Eject";
		case 0x18:
			return "GetFPos";
		case 0x44:
			return "SetFPos";
		case 0x45:
			return "FlushFile";
		default:
		{
			static thread_local char buf[16];
			std::snprintf(buf, sizeof(buf), "Flat$%02X", trapNum);
			return buf;
		}
	}
}

static const char *hfsTrapName(uint16_t selector)
{
	switch (selector)
	{
		case 0x01:
			return "OpenWD";
		case 0x02:
			return "CloseWD";
		case 0x05:
			return "CatMove";
		case 0x06:
			return "DirCreate";
		case 0x07:
			return "GetWDInfo";
		case 0x08:
			return "GetFCBInfo";
		case 0x09:
			return "GetCatInfo";
		case 0x0A:
			return "SetCatInfo";
		case 0x0B:
			return "SetVInfo";
		case 0x10:
			return "CreateFileIDRef";
		case 0x11:
			return "DeleteFileIDRef";
		case 0x30:
			return "GetVolParms";
		default:
		{
			static thread_local char buf[16];
			std::snprintf(buf, sizeof(buf), "HFS$%02X", selector);
			return buf;
		}
	}
}

/* ------------------------------------------------------------------ */
/*  Error name table                                                  */
/* ------------------------------------------------------------------ */

static const char *osErrName(int16_t err)
{
	switch (err)
	{
		case 0:
			return "noErr";
		case -33:
			return "dirFulErr";
		case -34:
			return "dskFulErr";
		case -35:
			return "nsvErr";
		case -36:
			return "ioErr";
		case -37:
			return "bdNamErr";
		case -38:
			return "fnOpnErr";
		case -39:
			return "eofErr";
		case -40:
			return "posErr";
		case -42:
			return "tmfoErr";
		case -43:
			return "fnfErr";
		case -44:
			return "wPrErr";
		case -45:
			return "fLckdErr";
		case -46:
			return "vLckdErr";
		case -47:
			return "fBsyErr";
		case -48:
			return "dupFNErr";
		case -49:
			return "opWrErr";
		case -50:
			return "paramErr";
		case -51:
			return "rfNumErr";
		case -58:
			return "extFSErr";
		case -120:
			return "dirNFErr";
		default:
			return nullptr;
	}
}

static std::string formatErr(int16_t err)
{
	const char *name = osErrName(err);
	if (name) return name;
	char buf[16];
	std::snprintf(buf, sizeof(buf), "%d", err);
	return buf;
}

/* ------------------------------------------------------------------ */
/*  Param block input field formatters (per trap type)                */
/* ------------------------------------------------------------------ */

/* Read the ioNamePtr→name if available. */
static std::string pbName(uint32_t pb)
{
	uint32_t namePtr = get_vm_long(pb + kPB_ioNamePtr);
	if (namePtr == 0) return "";
	return readPStr(namePtr);
}

static int16_t pbVRefNum(uint32_t pb)
{
	return readSWord(pb + kPB_ioVRefNum);
}

static int16_t pbRefNum(uint32_t pb)
{
	return readSWord(pb + kPB_ioRefNum);
}

/* ------------------------------------------------------------------ */
/*  Per-trap input formatters                                         */
/* ------------------------------------------------------------------ */

enum class TrapCategory
{
	kOpenLike,	 /* Open, OpenRF, Create, Delete, Rename */
	kCloseLike,	 /* Close, FlushFile */
	kReadWrite,	 /* Read, Write */
	kFileInfo,	 /* GetFileInfo, SetFileInfo */
	kCatInfo,	 /* GetCatInfo (HFS) */
	kSetCatInfo, /* SetCatInfo (HFS) */
	kVolInfo,	 /* GetVolInfo */
	kEOF,		 /* GetEOF, SetEOF, Allocate */
	kFPos,		 /* GetFPos, SetFPos */
	kVolRef,	 /* SetVol, GetVol, FlushVol, Eject, UnmountVol */
	kOpenWD,	 /* HFS OpenWD */
	kCloseWD,	 /* HFS CloseWD */
	kGetWDInfo,	 /* HFS GetWDInfo */
	kGetFCBInfo, /* HFS GetFCBInfo */
	kCatMove,	 /* HFS CatMove */
	kDirCreate,	 /* HFS DirCreate */
	kRename,	 /* Rename */
	kGeneric,	 /* fallback */
};

static TrapCategory classifyFlat(uint16_t trapNum)
{
	switch (trapNum)
	{
		case 0x00:
		case 0x0A:
			return TrapCategory::kOpenLike;
		case 0x08:
			return TrapCategory::kOpenLike; /* Create */
		case 0x09:
			return TrapCategory::kOpenLike; /* Delete */
		case 0x01:
		case 0x45:
			return TrapCategory::kCloseLike;
		case 0x02:
			return TrapCategory::kReadWrite;
		case 0x03:
			return TrapCategory::kReadWrite;
		case 0x07:
			return TrapCategory::kVolInfo;
		case 0x0B:
			return TrapCategory::kRename;
		case 0x0C:
			return TrapCategory::kFileInfo;
		case 0x0D:
			return TrapCategory::kFileInfo;
		case 0x0E:
			return TrapCategory::kVolRef; /* UnmountVol */
		case 0x10:
			return TrapCategory::kEOF; /* Allocate */
		case 0x11:
			return TrapCategory::kEOF; /* GetEOF */
		case 0x12:
			return TrapCategory::kEOF; /* SetEOF */
		case 0x13:
			return TrapCategory::kVolRef; /* FlushVol */
		case 0x14:
			return TrapCategory::kVolRef; /* GetVol */
		case 0x15:
			return TrapCategory::kVolRef; /* SetVol */
		case 0x17:
			return TrapCategory::kVolRef; /* Eject */
		case 0x18:
			return TrapCategory::kFPos; /* GetFPos */
		case 0x44:
			return TrapCategory::kFPos; /* SetFPos */
		default:
			return TrapCategory::kGeneric;
	}
}

static TrapCategory classifyHFS(uint16_t selector)
{
	switch (selector)
	{
		case 0x01:
			return TrapCategory::kOpenWD;
		case 0x02:
			return TrapCategory::kCloseWD;
		case 0x05:
			return TrapCategory::kCatMove;
		case 0x06:
			return TrapCategory::kDirCreate;
		case 0x07:
			return TrapCategory::kGetWDInfo;
		case 0x08:
			return TrapCategory::kGetFCBInfo;
		case 0x09:
			return TrapCategory::kCatInfo;
		case 0x0A:
			return TrapCategory::kSetCatInfo;
		case 0x0B:
			return TrapCategory::kVolRef; /* SetVInfo */
		case 0x30:
			return TrapCategory::kVolRef; /* GetVolParms */
		default:
			return TrapCategory::kGeneric;
	}
}

/* Format the input (request) fields for the trap. */
static std::string formatInput(TrapCategory cat, uint32_t pb, bool isHFS)
{
	std::string s;
	char buf[256];

	switch (cat)
	{
		case TrapCategory::kOpenLike:
		{
			auto name = pbName(pb);
			int16_t vr = pbVRefNum(pb);
			if (isHFS)
			{
				uint32_t dirID = get_vm_long(pb + kPB_ioDirID);
				std::snprintf(buf, sizeof(buf), "vRefNum=%d name=\"%s\" dirID=%u", vr, name.c_str(),
							  dirID);
			}
			else
			{
				std::snprintf(buf, sizeof(buf), "vRefNum=%d name=\"%s\"", vr, name.c_str());
			}
			s = buf;
			break;
		}
		case TrapCategory::kCloseLike:
			std::snprintf(buf, sizeof(buf), "refNum=%d", pbRefNum(pb));
			s = buf;
			break;

		case TrapCategory::kReadWrite:
		{
			int16_t ref = pbRefNum(pb);
			uint32_t req = get_vm_long(pb + kPB_ioReqCount);
			int16_t mode = readSWord(pb + kPB_ioPosMode);
			uint32_t off = get_vm_long(pb + kPB_ioPosOffset);
			std::snprintf(buf, sizeof(buf), "refNum=%d reqCount=%u posMode=%d posOffset=%u", ref,
						  req, mode, off);
			s = buf;
			break;
		}
		case TrapCategory::kFileInfo:
		{
			auto name = pbName(pb);
			int16_t vr = pbVRefNum(pb);
			int16_t idx = readSWord(pb + kPB_ioFDirIndex);
			if (isHFS)
			{
				uint32_t dirID = get_vm_long(pb + kPB_ioDirID);
				std::snprintf(buf, sizeof(buf), "vRefNum=%d name=\"%s\" dirID=%u index=%d", vr,
							  name.c_str(), dirID, idx);
			}
			else
			{
				std::snprintf(buf, sizeof(buf), "vRefNum=%d name=\"%s\" index=%d", vr, name.c_str(),
							  idx);
			}
			s = buf;
			break;
		}
		case TrapCategory::kCatInfo:
		{
			auto name = pbName(pb);
			int16_t vr = pbVRefNum(pb);
			int16_t idx = readSWord(pb + kPB_ioFDirIndex);
			uint32_t dirID = get_vm_long(pb + kPB_ioDirID);
			std::snprintf(buf, sizeof(buf), "vRefNum=%d dirID=%u index=%d name=\"%s\"", vr, dirID,
						  idx, name.c_str());
			s = buf;
			break;
		}
		case TrapCategory::kSetCatInfo:
		{
			auto name = pbName(pb);
			int16_t vr = pbVRefNum(pb);
			uint32_t ioDirID = get_vm_long(pb + kPB_ioDirID);
			uint32_t ftype = get_vm_long(pb + kPB_ioFlFndrInfo);
			uint32_t fcreator = get_vm_long(pb + kPB_ioFlFndrInfo + 4);
			std::snprintf(buf, sizeof(buf),
						  "vRefNum=%d ioDirID=%u (dirID or fileNum) name=\"%s\" type=%s creator=%s",
						  vr, ioDirID, name.c_str(), formatFourCC(ftype).c_str(),
						  formatFourCC(fcreator).c_str());
			s = buf;
			break;
		}
		case TrapCategory::kVolInfo:
		{
			int16_t vr = pbVRefNum(pb);
			int16_t idx = readSWord(pb + kPB_ioVolIndex);
			std::snprintf(buf, sizeof(buf), "vRefNum=%d volIndex=%d", vr, idx);
			s = buf;
			break;
		}
		case TrapCategory::kEOF:
		{
			int16_t ref = pbRefNum(pb);
			uint32_t misc = get_vm_long(pb + kPB_ioMisc);
			std::snprintf(buf, sizeof(buf), "refNum=%d ioMisc=%u", ref, misc);
			s = buf;
			break;
		}
		case TrapCategory::kFPos:
		{
			int16_t ref = pbRefNum(pb);
			int16_t mode = readSWord(pb + kPB_ioPosMode);
			uint32_t off = get_vm_long(pb + kPB_ioPosOffset);
			std::snprintf(buf, sizeof(buf), "refNum=%d posMode=%d posOffset=%u", ref, mode, off);
			s = buf;
			break;
		}
		case TrapCategory::kVolRef:
		{
			int16_t vr = pbVRefNum(pb);
			auto name = pbName(pb);
			if (!name.empty())
				std::snprintf(buf, sizeof(buf), "vRefNum=%d name=\"%s\"", vr, name.c_str());
			else
				std::snprintf(buf, sizeof(buf), "vRefNum=%d", vr);
			s = buf;
			break;
		}
		case TrapCategory::kOpenWD:
		{
			int16_t vr = pbVRefNum(pb);
			uint32_t dirID = get_vm_long(pb + kPB_ioWDDirID);
			std::snprintf(buf, sizeof(buf), "vRefNum=%d wdDirID=%u", vr, dirID);
			s = buf;
			break;
		}
		case TrapCategory::kCloseWD:
			std::snprintf(buf, sizeof(buf), "vRefNum=%d", pbVRefNum(pb));
			s = buf;
			break;

		case TrapCategory::kGetWDInfo:
		{
			int16_t vr = pbVRefNum(pb);
			int16_t idx = readSWord(pb + kPB_ioWDIndex);
			std::snprintf(buf, sizeof(buf), "vRefNum=%d wdIndex=%d", vr, idx);
			s = buf;
			break;
		}
		case TrapCategory::kGetFCBInfo:
		{
			int16_t ref = pbRefNum(pb);
			int16_t idx = readSWord(pb + kPB_ioFCBIndx);
			std::snprintf(buf, sizeof(buf), "refNum=%d fcbIndex=%d", ref, idx);
			s = buf;
			break;
		}
		case TrapCategory::kCatMove:
		{
			auto name = pbName(pb);
			int16_t vr = pbVRefNum(pb);
			uint32_t dirID = get_vm_long(pb + kPB_ioDirID);
			uint32_t newDirID = get_vm_long(pb + 36);	   /* ioNewDirID at offset 36 */
			auto newName = readPStr(get_vm_long(pb + 28)); /* ioNewName */
			std::snprintf(buf, sizeof(buf),
						  "vRefNum=%d dirID=%u name=\"%s\" newDirID=%u newName=\"%s\"", vr, dirID,
						  name.c_str(), newDirID, newName.c_str());
			s = buf;
			break;
		}
		case TrapCategory::kDirCreate:
		{
			auto name = pbName(pb);
			int16_t vr = pbVRefNum(pb);
			uint32_t dirID = get_vm_long(pb + kPB_ioDirID);
			std::snprintf(buf, sizeof(buf), "vRefNum=%d dirID=%u name=\"%s\"", vr, dirID,
						  name.c_str());
			s = buf;
			break;
		}
		case TrapCategory::kRename:
		{
			auto name = pbName(pb);
			int16_t vr = pbVRefNum(pb);
			/* ioMisc holds the new name pointer for _Rename */
			uint32_t newNamePtr = get_vm_long(pb + kPB_ioMisc);
			auto newName = (newNamePtr != 0) ? readPStr(newNamePtr) : "<null>";
			if (isHFS)
			{
				uint32_t dirID = get_vm_long(pb + kPB_ioDirID);
				std::snprintf(buf, sizeof(buf), "vRefNum=%d dirID=%u name=\"%s\" newName=\"%s\"",
							  vr, dirID, name.c_str(), newName.c_str());
			}
			else
			{
				std::snprintf(buf, sizeof(buf), "vRefNum=%d name=\"%s\" newName=\"%s\"", vr,
							  name.c_str(), newName.c_str());
			}
			s = buf;
			break;
		}
		case TrapCategory::kGeneric:
		{
			int16_t vr = pbVRefNum(pb);
			auto name = pbName(pb);
			std::snprintf(buf, sizeof(buf), "vRefNum=%d name=\"%s\"", vr, name.c_str());
			s = buf;
			break;
		}
	}
	return s;
}

/* ------------------------------------------------------------------ */
/*  Per-trap result field formatters                                   */
/* ------------------------------------------------------------------ */

static std::string formatResult(TrapCategory cat, uint32_t pb)
{
	std::string s;
	char buf[256];

	switch (cat)
	{
		case TrapCategory::kOpenLike:
			std::snprintf(buf, sizeof(buf), "refNum=%d", pbRefNum(pb));
			s = buf;
			break;

		case TrapCategory::kReadWrite:
		{
			uint32_t act = get_vm_long(pb + kPB_ioActCount);
			uint32_t off = get_vm_long(pb + kPB_ioPosOffset);
			std::snprintf(buf, sizeof(buf), "actCount=%u posOffset=%u", act, off);
			s = buf;
			break;
		}
		case TrapCategory::kFileInfo:
		{
			auto name = pbName(pb);
			uint32_t ftype = get_vm_long(pb + kPB_ioFlFndrInfo);
			uint32_t fcreator = get_vm_long(pb + kPB_ioFlFndrInfo + 4);
			uint32_t dataEOF = get_vm_long(pb + kPB_ioFlLgLen);
			uint32_t rsrcEOF = get_vm_long(pb + kPB_ioFlRLgLen);
			std::snprintf(buf, sizeof(buf), "name=\"%s\" type=%s creator=%s dataEOF=%u rsrcEOF=%u",
						  name.c_str(), formatFourCC(ftype).c_str(), formatFourCC(fcreator).c_str(),
						  dataEOF, rsrcEOF);
			s = buf;
			break;
		}
		case TrapCategory::kCatInfo:
		{
			uint8_t attrib = get_vm_byte(pb + kPB_ioFlAttrib);
			bool isDir = (attrib & 0x10) != 0;
			auto name = pbName(pb);
			if (isDir)
			{
				uint32_t dirID = get_vm_long(pb + kPB_ioDirID);
				uint16_t nFiles = get_vm_word(pb + 52); /* ioDrNmFls */
				uint32_t parID = get_vm_long(pb + kPB_ioFlParID);
				std::snprintf(buf, sizeof(buf), "name=\"%s\" [DIR] dirID=%u nFiles=%u parentID=%u",
							  name.c_str(), dirID, nFiles, parID);
			}
			else
			{
				uint32_t cnid = get_vm_long(pb + kPB_ioFlNum);
				uint32_t ftype = get_vm_long(pb + kPB_ioFlFndrInfo);
				uint32_t fcreator = get_vm_long(pb + kPB_ioFlFndrInfo + 4);
				uint32_t dataEOF = get_vm_long(pb + kPB_ioFlLgLen);
				uint32_t rsrcEOF = get_vm_long(pb + kPB_ioFlRLgLen);
				uint32_t parID = get_vm_long(pb + kPB_ioFlParID);
				std::snprintf(
					buf, sizeof(buf),
					"name=\"%s\" cnid=%u type=%s creator=%s dataEOF=%u rsrcEOF=%u parentID=%u",
					name.c_str(), cnid, formatFourCC(ftype).c_str(), formatFourCC(fcreator).c_str(),
					dataEOF, rsrcEOF, parID);
			}
			s = buf;
			break;
		}
		case TrapCategory::kVolInfo:
		{
			auto name = pbName(pb);
			int16_t vr = pbVRefNum(pb);
			uint16_t freeBlks = get_vm_word(pb + kPB_ioVFrBlk);
			uint16_t totalBlks = get_vm_word(pb + kPB_ioVNmAlBlks);
			std::snprintf(buf, sizeof(buf), "name=\"%s\" vRefNum=%d freeBlks=%u totalBlks=%u",
						  name.c_str(), vr, freeBlks, totalBlks);
			s = buf;
			break;
		}
		case TrapCategory::kEOF:
		{
			uint32_t misc = get_vm_long(pb + kPB_ioMisc);
			std::snprintf(buf, sizeof(buf), "ioMisc(EOF)=%u", misc);
			s = buf;
			break;
		}
		case TrapCategory::kFPos:
		{
			int16_t mode = readSWord(pb + kPB_ioPosMode);
			uint32_t off = get_vm_long(pb + kPB_ioPosOffset);
			std::snprintf(buf, sizeof(buf), "posMode=%d posOffset=%u", mode, off);
			s = buf;
			break;
		}
		case TrapCategory::kOpenWD:
			std::snprintf(buf, sizeof(buf), "wdRefNum=%d", pbVRefNum(pb));
			s = buf;
			break;

		case TrapCategory::kGetWDInfo:
		{
			int16_t wdVRef = readSWord(pb + kPB_ioWDVRefNum);
			uint32_t wdDirID = get_vm_long(pb + kPB_ioWDDirID);
			std::snprintf(buf, sizeof(buf), "wdVRefNum=%d wdDirID=%u", wdVRef, wdDirID);
			s = buf;
			break;
		}
		case TrapCategory::kGetFCBInfo:
		{
			uint32_t fileNum = get_vm_long(pb + kPB_ioFCBFlNm);
			uint32_t eof = get_vm_long(pb + kPB_ioFCBEOF);
			int16_t fcbVRef = readSWord(pb + kPB_ioFCBVRefNum);
			uint32_t parID = get_vm_long(pb + kPB_ioFCBParID);
			auto name = pbName(pb);
			std::snprintf(buf, sizeof(buf), "fileNum=%u eof=%u vRefNum=%d parID=%u name=\"%s\"",
						  fileNum, eof, fcbVRef, parID, name.c_str());
			s = buf;
			break;
		}
		case TrapCategory::kDirCreate:
		{
			uint32_t newDirID = get_vm_long(pb + kPB_ioDirID);
			std::snprintf(buf, sizeof(buf), "newDirID=%u", newDirID);
			s = buf;
			break;
		}
		default:
			break;
	}
	return s;
}

/* ------------------------------------------------------------------ */
/*  Main entry point                                                  */
/* ------------------------------------------------------------------ */

static constexpr uint16_t kActionPassThrough = 0;
static constexpr uint16_t kActionHandled = 1;
static constexpr uint16_t kActionError = 2;

static constexpr uint16_t kFlagHFS = 0x0001;
static constexpr uint16_t kFlagPBMod = 0x0002;

void extfsLogTrap(uint16_t trapWord, uint32_t pbAddr, uint16_t action, int16_t err, uint16_t flags)
{
	bool isHFS = (flags & kFlagHFS) != 0;
	bool pbMod = (flags & kFlagPBMod) != 0;

	/* Flat traps arrive as full trap words ($A002, $A207, …);
	   only the low byte indexes into the 256-entry OS trap table.
	   HFS selectors come from D0.W and are always < 0x100.
	   Hierarchical flat traps ($A2xx) have isHFS set but are still
	   flat traps — distinguish by trapWord > 0xFF. */
	bool isHFSDispatch = isHFS && (trapWord <= 0xFF);
	uint16_t trapNum = (trapWord > 0xFF) ? (trapWord & 0xFF) : trapWord;

	const char *name = isHFSDispatch ? hfsTrapName(trapNum) : flatTrapName(trapNum);
	TrapCategory cat = isHFSDispatch ? classifyHFS(trapNum) : classifyFlat(trapNum);

	/* Line 1: trap name + input fields */
	std::string line = "SharedDrive | ";
	if (isHFS) line += "HFS:";
	line += '_';
	line += name;
	line += ' ';
	line += formatInput(cat, pbAddr, isHFS);

	/* Line 2: result */
	line += "\n            |   -> ";
	switch (action)
	{
		case kActionPassThrough:
			line += "PASS-THROUGH";
			break;
		case kActionHandled:
			line += "HANDLED: ";
			line += formatErr(err);
			break;
		case kActionError:
			line += "ERROR: ";
			line += formatErr(err);
			break;
	}

	/* Line 3 (optional): output fields */
	if (pbMod && action != kActionPassThrough)
	{
		std::string result = formatResult(cat, pbAddr);
		if (!result.empty())
		{
			line += "\n            |     ";
			line += result;
		}
	}

	/* Send to stderr directly (bypass console deque for pass-through) */
	if (action == kActionPassThrough)
	{
		std::fprintf(stderr, "[SD] %s\n", line.c_str());
	}
	else
	{
		guestConsoleAppend(line);
	}
}
