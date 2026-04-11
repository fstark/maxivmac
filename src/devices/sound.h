/*
	Sound — Audio output via square-wave inversion (compact Macs)
	or ASC (Mac II family)
*/
#pragma once

#include "devices/device.h"
#include <cstdint>

class VIA1Device;

class SoundDevice : public Device
{
public:
	uint32_t access(uint32_t data, bool /*writeMem*/, uint32_t /*addr*/) override { return data; }
	void zap() override {}
	void reset() override;
	const char *name() const override { return "Sound"; }

	void subTick(int subTick);

private:
	VIA1Device *via1() const;

	uint32_t soundInvertPhase_ = 0;
	uint16_t soundInvertState_ = 0;
};
