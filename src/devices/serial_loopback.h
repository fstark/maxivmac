/*
	LoopbackBackend — echoes every TX byte back as RX.

	Useful for verifying the SCC ↔ backend data path works end to end.
*/
#pragma once

#include "devices/serial_backend.h"
#include <queue>

class LoopbackBackend : public SerialBackend
{
public:
	void txByte(uint8_t byte) override { queue_.push(byte); }
	bool rxReady() override { return !queue_.empty(); }
	uint8_t rxByte() override
	{
		uint8_t b = queue_.front();
		queue_.pop();
		return b;
	}
	void poll() override {}
	const char *name() const override { return "loopback"; }

private:
	std::queue<uint8_t> queue_;
};
