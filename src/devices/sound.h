/*
	SNDEMDEV.h

	Copyright (C) 2003 Philip Cummins, Paul C. Pratt

	You can redistribute this file and/or modify it under the terms
	of version 2 of the GNU General Public License as published by
	the Free Software Foundation.  You should have received a copy
	of the license along with this file; see the file COPYING.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	license for more details.
*/

#pragma once

#include "devices/device.h"
#include <cstdint>

class VIA1Device;

class SoundDevice : public Device {
public:
	uint32_t access(uint32_t data, bool writeMem, uint32_t addr) override
		{ return data; }
	void zap() override {}
	void reset() override;
	const char* name() const override { return "Sound"; }

#if MySoundEnabled
	void subTick(int subTick);
#endif

private:
	VIA1Device* via1() const;

	uint32_t soundInvertPhase_ = 0;
	uint16_t soundInvertState_ = 0;
};

extern SoundDevice* g_sound;

// Backward-compatible free function API
#if MySoundEnabled
extern void MacSound_SubTick(int SubTick);
#endif
