/*
	extn_system.cpp — System extension handler.
	Handles the $0xx command range (InitIdent and future system commands).
*/

#include "core/extn_system.h"
#include "core/diag.h"
#include "platform/platform_config.h" // MAXIVMAC_VERSION

extern uint8_t get_vm_byte(uint32_t addr);

static constexpr uint16_t kCmdInitIdent = 0x0001;

// API version range the host accepts.
static constexpr int kMinApiVersion = 1;
static constexpr int kMaxApiVersion = 1;

static InitInfo s_initInfo;

// --- InitInfo implementation ---

bool InitInfo::isStale() const
{
	return loaded_ && version_ != MAXIVMAC_VERSION;
}

void InitInfo::populate(int apiVer, std::string_view version, int16_t vRefNum, int32_t dirID,
						std::string_view fileName, int machineType, int systemVersion)
{
	loaded_ = true;
	apiVersion_ = apiVer;
	version_ = version;
	vRefNum_ = vRefNum;
	dirID_ = dirID;
	fileName_ = fileName;
	machineType_ = machineType;
	systemVersion_ = systemVersion;
}

void InitInfo::reset()
{
	loaded_ = false;
	version_.clear();
	apiVersion_ = 0;
	vRefNum_ = 0;
	dirID_ = 0;
	fileName_.clear();
	machineType_ = 0;
	systemVersion_ = 0;
}

// --- Guest Pascal string reader ---

static std::string readGuestPascalStr(uint32_t addr)
{
	if (addr == 0) return {};
	uint8_t len = get_vm_byte(addr);
	std::string s;
	s.reserve(len);
	for (uint8_t i = 0; i < len; i++)
		s.push_back(static_cast<char>(get_vm_byte(addr + 1 + i)));
	return s;
}

// --- Dispatch ---

void ExtnSystemDispatch(uint16_t cmd, uint32_t regParam[], uint16_t &regResult)
{
	switch (cmd)
	{
		case kCmdInitIdent:
		{
			int apiVer = static_cast<int>(regParam[0]);
			std::string version = readGuestPascalStr(regParam[1]);
			auto vRefNum = static_cast<int16_t>(regParam[2]);
			auto dirID = static_cast<int32_t>(regParam[3]);
			std::string fileName = readGuestPascalStr(regParam[4]);
			int machType = static_cast<int>(regParam[5]);
			int sysVer = static_cast<int>(regParam[6]);

			s_initInfo.populate(apiVer, version, vRefNum, dirID, fileName, machType, sysVer);

			bool compatible = (apiVer >= kMinApiVersion && apiVer <= kMaxApiVersion);
			regResult = compatible ? 0 : 1;

			DIAG(INIT,
				 "InitIdent: api=%d ver=\"%s\" file=\"%s\" "
				 "machine=%d sysVer=$%04X → %s\n",
				 apiVer, version.c_str(), fileName.c_str(), machType, sysVer,
				 compatible ? "OK" : "REJECTED");
			break;
		}
		default:
			regResult = 0xFFFF;
			break;
	}
}

const InitInfo &ExtnSystemInitInfo()
{
	return s_initInfo;
}
