#pragma once

#include <cstdint>
#include <string>
#include <string_view>

/*
	System extension handler for the register-block interface.
	Called from regDispatch() in machine.cpp when command codes
	$000–$0FF are written to the register block.
*/

class InitInfo
{
public:
	bool loaded() const { return loaded_; }
	std::string_view version() const { return version_; }
	int apiVersion() const { return apiVersion_; }
	bool isStale() const;

	// Guest environment
	int machineType() const { return machineType_; }
	int systemVersion() const { return systemVersion_; }

	// File spec (for future auto-update)
	int16_t vRefNum() const { return vRefNum_; }
	int32_t dirID() const { return dirID_; }
	std::string_view fileName() const { return fileName_; }

	// Called by ExtnSystemDispatch — not for external use.
	void populate(int apiVer, std::string_view version, int16_t vRefNum, int32_t dirID,
				  std::string_view fileName, int machineType, int systemVersion);
	void reset();

private:
	bool loaded_ = false;
	std::string version_;
	int apiVersion_ = 0;
	int16_t vRefNum_ = 0;
	int32_t dirID_ = 0;
	std::string fileName_;
	int machineType_ = 0;
	int systemVersion_ = 0;
};

void ExtnSystemDispatch(uint16_t cmd, uint32_t regParam[], uint16_t &regResult);
const InitInfo &ExtnSystemInitInfo();
