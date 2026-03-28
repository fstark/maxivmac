/*
	Screen — Framebuffer-to-host display refresh
*/
#pragma once

#include "devices/device.h"
#include <cstdint>

class ScreenDevice : public Device {
public:
	uint32_t access(uint32_t data, bool /*writeMem*/, uint32_t /*addr*/) override
		{ return data; }
	void zap() override {}
	void reset() override {}
	const char* name() const override { return "Screen"; }

	void endTickNotify();
};

