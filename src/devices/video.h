/*
	VIDEMDEV.h

	Copyright (C) 2008 Paul C. Pratt

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

class VideoDevice : public Device {
public:
	uint32_t access(uint32_t data, bool writeMem, uint32_t addr) override
		{ return data; }
	void zap() override {}
	void reset() override;
	const char* name() const override { return "Video"; }

	bool init();
	uint16_t vidReset(); // returns mode value (128)
	void update();
	void extnVideoAccess(uint32_t p);
};

extern VideoDevice* g_video;

// Backward-compatible free function API
extern bool Vid_Init(void);
extern uint16_t Vid_Reset(void);
extern void Vid_Update(void);
extern void ExtnVideo_Access(uint32_t p);
