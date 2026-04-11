/*
	Keyboard — Mac keyboard I/O via VIA1 shift register
*/
#pragma once

#include "devices/device.h"
#include <cstdint>

class VIA1Device;

class KeyboardDevice : public Device
{
public:
	uint32_t access(uint32_t data, bool /*writeMem*/, uint32_t /*addr*/) override
	{
		return data;
	} // Not memory-mapped
	void zap() override {}
	void reset() override;
	const char *name() const override { return "Keyboard"; }

	// VIA1 data-line change: start receiving a command byte.
	void dataLineChngNtfy();

	// ICT callbacks for keyboard command protocol phases.
	void receiveEndCommand();
	void receiveCommand();

	// Poll for pending key events during inquiry.
	void update();

private:
	// Helper to access VIA1
	VIA1Device *via1() const;
	void gotKeyBoardData(uint8_t v);
	bool attemptToFinishInquiry();

	enum KybdState
	{
		kKybdStateIdle,
		kKybdStateRecievingCommand,
		kKybdStateRecievedCommand,
		kKybdStateRecievingEndCommand,
		kKybdStates
	};

	KybdState kybdState_ = kKybdStateIdle;
	bool haveKeyBoardResult_ = false;
	uint8_t keyBoardResult_ = 0;
	uint8_t instantCommandData_ = 0x7B;
	int inquiryCommandTimer_ = 0;
};
