/*
	ADB — Apple Desktop Bus controller (Mac II family)
*/
#pragma once

#include "devices/device.h"
#include <cstdint>

class ADBDevice : public Device
{
public:
	uint32_t access(uint32_t data, bool /*writeMem*/, uint32_t /*addr*/) override
	{
		return data;
	} // Not memory-mapped
	void zap() override {}
	void reset() override {}
	const char *name() const override { return "ADB"; }

	void stateChangeNtfy();
	void doNewState();
	void dataLineChngNtfy();
	void update();
};
