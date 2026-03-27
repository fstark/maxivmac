#pragma once

#include "devices/device.h"
#include <cstdint>

class ASCDevice : public Device {
public:
	uint32_t access(uint32_t data, bool writeMem, uint32_t addr) override;
	void zap() override {}
	void reset() override {} // ASC has no separate reset
	const char* name() const override { return "ASC"; }

	void subTick(int subTick);
};

