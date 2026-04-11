/*
	SCC — Zilog 8530 Serial Communications Controller
*/
#pragma once

#include "devices/device.h"
#include <cstdint>
#include <memory>

class SerialBackend;

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

	// Attach a serial backend to a channel (0 = A/modem, 1 = B/printer).
	// Takes ownership.  Pass nullptr to detach.
	void setBackend(int chan, std::unique_ptr<SerialBackend> backend);

	// Poll attached backends and deliver received bytes into the SCC.
	// Called once per 1/60s tick from SixtiethSecondNotify.
	void serialTick();

#ifdef EmLocalTalk
	void localTalkTick();
#endif
};

