/*
	PMU — Power Manager Unit (Portable Mac)
*/
#pragma once

#include "devices/device.h"
#include <cstdint>

class PMUDevice : public Device
{
public:
	uint32_t access(uint32_t data, bool /*writeMem*/, uint32_t /*addr*/) override { return data; }
	void zap() override {}
	void reset() override;
	const char *name() const override { return "PMU"; }

	// Respond to VIA-driven ready-line transitions.
	void toReadyChangeNtfy();

	// ICT callback: process the current PMU command/response.
	void doTask();

private:
	static constexpr int kBuffSz = 8;

	uint8_t buffA_[kBuffSz] = {};
	uint8_t *p_ = nullptr;
	uint8_t rem_ = 0;
	uint8_t i_ = 0;

	int state_ = 0; // kPMUStateReadyForCommand
	uint8_t curCommand_ = 0;
	uint8_t sendNext_ = 0;
	uint8_t buffL_ = 0;
	bool sending_ = false;

	uint8_t paramRAM_[128] = {};

	void startSendResult(uint8_t resultCode, uint8_t len);
	void checkCommandOp();
	void checkCommandCompletion();
	void locBuffSetUpNextChunk();
	uint8_t getPMUbus() const;
	void setPMUbus(uint8_t v);
};
