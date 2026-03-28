/*
	SCSI — SCSI bus controller
*/
#pragma once

#include "devices/device.h"
#include <cstdint>

class SCSIDevice : public Device {
public:
	uint32_t access(uint32_t data, bool writeMem, uint32_t addr) override;
	void zap() override {}
	void reset() override;
	const char* name() const override { return "SCSI"; }
};

