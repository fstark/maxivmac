/*
	RTC — Real-Time Clock (serial bit-bang protocol via VIA)
*/
#pragma once

#include "devices/device.h"
#include <cstdint>

class RTCDevice : public Device {
public:
	uint32_t access(uint32_t data, bool /*writeMem*/, uint32_t /*addr*/) override
		{ return data; } // Not memory-mapped
	void zap() override {}
	void reset() override {}
	const char* name() const override { return "RTC"; }

	bool init();

	// One-second tick: advance the clock counter.
	void interrupt();

	// VIA wire callbacks for the serial bit-bang protocol.
	void unEnabledChangeNtfy();
	void clockChangeNtfy();
	void dataLineChangeNtfy();
};

