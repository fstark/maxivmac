#pragma once

#include "devices/device.h"
#include <cstdint>
#include <string.h>

class SonyDevice : public Device {
public:
	uint32_t access(uint32_t data, bool /*writeMem*/, uint32_t /*addr*/) override
		{ return data; }
	void zap() override {}
	void reset() override;
	const char* name() const override { return "Sony"; }

	void extnDiskAccess(uint32_t p);
	void extnSonyAccess(uint32_t p);
	void setQuitOnEject();
	void ejectAllDisks();
	void update();
};

