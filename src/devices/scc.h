/*
	SCC — Zilog 8530 Serial Communications Controller
*/
#pragma once

#include "devices/device.h"
#include <cstdint>

// Internal state remains as file-scope statics in scc.cpp for now
// due to heavy conditional compilation (EmLocalTalk, SCC_TrackMore).
class SCCDevice : public Device {
public:
	// Device interface
	uint32_t access(uint32_t data, bool writeMem, uint32_t addr) override;
	void zap() override {} // SCC has no separate zap
	void reset() override;
	const char* name() const override { return "SCC"; }

	// Return whether SCC master interrupt enable is set.
	bool interruptsEnabled();

#ifdef EmLocalTalk
	void localTalkTick();
#endif
};

