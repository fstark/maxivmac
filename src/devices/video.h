/*
	Video — Display output and video mode management
*/
#pragma once

#include "devices/device.h"
#include <cstdint>

class VideoDevice : public Device {
public:
	uint32_t access(uint32_t data, bool /*writeMem*/, uint32_t /*addr*/) override
		{ return data; }
	void zap() override {}
	void reset() override;
	const char* name() const override { return "Video"; }

	bool init();
	uint16_t vidReset(); // returns mode value (128)
	void update();
	void extnVideoAccess(uint32_t p);
};

