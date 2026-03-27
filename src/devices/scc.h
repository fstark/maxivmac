#pragma once

#include "devices/device.h"
#include <cstdint>

// Forward: SCC_dolog and SCC_TrackMore are defined in scc.cpp

// SCC Device class wrapping the original SCC emulation.
// Internal state remains as file-scope statics in scc.cpp for now
// due to heavy conditional compilation (EmLocalTalk, SCC_TrackMore).
// Full state migration will happen in a later phase.
class SCCDevice : public Device {
public:
	// Device interface
	uint32_t access(uint32_t data, bool writeMem, uint32_t addr) override;
	void zap() override {} // SCC has no separate zap
	void reset() override;
	const char* name() const override { return "SCC"; }

	bool interruptsEnabled();

#ifdef EmLocalTalk
	void localTalkTick();
#endif
};

