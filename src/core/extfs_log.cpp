/*
	extfs_log.cpp — SharedDrive structured trap logging.

	Pretty-prints File Manager trap calls intercepted by the
	SharedDrive INIT.  Uses the TypeRegistry to format param blocks
	from guest RAM.
*/

#include "core/extfs_log.h"
#include "core/extn_clip.h" /* guestConsoleAppend */
#include "lang/type_registry.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

/* ioFlAttrib offset used to pick CInfoPBRec variant */
static constexpr uint32_t kPB_ioFlAttrib = 30;

extern uint8_t get_vm_byte(uint32_t addr);

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
/*  Trap → param block type mapping                                   */
/* ------------------------------------------------------------------ */

struct PBTypeInfo
{
	const char *typeName;
	const char *variant; /* for unions, else nullptr */
};

static PBTypeInfo flatPBType(uint16_t trapNum, bool isHFS)
{
	switch (trapNum)
	{
		case 0x00: /* Open */
		case 0x0A: /* OpenRF */
		case 0x01: /* Close */
		case 0x45: /* FlushFile */
		case 0x02: /* Read */
		case 0x03: /* Write */
		case 0x10: /* Allocate */
		case 0x11: /* GetEOF */
		case 0x12: /* SetEOF */
		case 0x18: /* GetFPos */
		case 0x44: /* SetFPos */
			return {"IOParam", nullptr};
		case 0x07: /* GetVolInfo */
		case 0x0E: /* UnmountVol */
		case 0x13: /* FlushVol */
		case 0x14: /* GetVol */
		case 0x15: /* SetVol */
		case 0x17: /* Eject */
			return {"VolumeParam", nullptr};
		case 0x08: /* Create */
		case 0x09: /* Delete */
		case 0x0C: /* GetFileInfo */
		case 0x0D: /* SetFileInfo */
			return isHFS ? PBTypeInfo{"HFileInfo", nullptr} : PBTypeInfo{"FileParam", nullptr};
		case 0x0B: /* Rename */
			return isHFS ? PBTypeInfo{"HFileInfo", nullptr} : PBTypeInfo{"IOParam", nullptr};
		default:
			return {"IOParam", nullptr};
	}
}

static PBTypeInfo hfsPBType(uint16_t selector)
{
	switch (selector)
	{
		case 0x01: /* OpenWD */
		case 0x02: /* CloseWD */
		case 0x07: /* GetWDInfo */
			return {"WDParam", nullptr};
		case 0x05: /* CatMove */
		case 0x06: /* DirCreate */
			return {"HFileInfo", nullptr};
		case 0x08: /* GetFCBInfo */
			return {"FCBPBRec", nullptr};
		case 0x09:							/* GetCatInfo */
		case 0x0A:							/* SetCatInfo */
			return {"CInfoPBRec", nullptr}; /* variant chosen dynamically */
		case 0x0B:							/* SetVInfo */
		case 0x30:							/* GetVolParms */
			return {"VolumeParam", nullptr};
		default:
			return {"IOParam", nullptr};
	}
}

/* ------------------------------------------------------------------ */
/*  Main entry point                                                  */
/* ------------------------------------------------------------------ */

static constexpr uint16_t kActionPassThrough = 0;
static constexpr uint16_t kActionHandled = 1;
static constexpr uint16_t kActionError = 2;

static constexpr uint16_t kFlagHFS = 0x0001;
[[maybe_unused]] static constexpr uint16_t kFlagPBMod = 0x0002;

void extfsLogTrap(uint16_t trapWord, uint32_t pbAddr, uint16_t action, int16_t err, uint16_t flags)
{
	bool isHFS = (flags & kFlagHFS) != 0;

	bool isHFSDispatch = isHFS && (trapWord <= 0xFF);
	uint16_t trapNum = (trapWord > 0xFF) ? (trapWord & 0xFF) : trapWord;

	const char *name = isHFSDispatch ? hfsTrapName(trapNum) : flatTrapName(trapNum);
	PBTypeInfo pbt = isHFSDispatch ? hfsPBType(trapNum) : flatPBType(trapNum, isHFS);

	/* Header line: trap name */
	std::string line = "SharedDrive | ";
	if (isHFS) line += "HFS:";
	line += '_';
	line += name;

	/* Result line */
	line += "\n            |   -> ";
	switch (action)
	{
		case kActionPassThrough:
			line += "PASS-THROUGH";
			break;
		case kActionHandled:
		case kActionError:
		{
			auto &tr = g_typeRegistry();
			auto errStr = tr.has("ParamBlockHeader")
							  ? tr.readField("ParamBlockHeader", pbAddr, "ioResult")
							  : std::to_string(err);
			line += (action == kActionError) ? "ERROR: " : "HANDLED: ";
			line += errStr;
			break;
		}
	}

	/* Dump param block via type registry */
	auto &tr = g_typeRegistry();
	if (tr.has(pbt.typeName))
	{
		/* For CInfoPBRec, pick "file" or "dir" variant from ioFlAttrib */
		std::string_view variant;
		if (std::strcmp(pbt.typeName, "CInfoPBRec") == 0)
		{
			uint8_t attrib = get_vm_byte(pbAddr + kPB_ioFlAttrib);
			variant = (attrib & 0x10) ? "dir" : "file";
		}
		else if (pbt.variant)
		{
			variant = pbt.variant;
		}

		std::string dump = tr.format(pbt.typeName, pbAddr, variant);
		if (!dump.empty())
		{
			line += '\n';
			line += dump;
		}
	}

	if (action == kActionPassThrough)
		std::fprintf(stderr, "[SD] %s\n", line.c_str());
	else
		guestConsoleAppend(line);
}
