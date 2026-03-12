/*
	SONYEMDV.h

	Copyright (C) 2004 Paul C. Pratt

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
#include <string.h>

class SonyDevice : public Device {
public:
	uint32_t access(uint32_t data, bool writeMem, uint32_t addr) override
		{ return data; }
	void zap() override {}
	void reset() override;
	const char* name() const override { return "Sony"; }

	void extnDiskAccess(uint32_t p);
	void extnSonyAccess(uint32_t p);
	void setQuitOnEject();
	void ejectAllDisks();
	void update();
};

extern SonyDevice* g_sony;

// Backward-compatible free function API
extern void ExtnDisk_Access(uint32_t p);
extern void ExtnSony_Access(uint32_t p);
extern void Sony_SetQuitOnEject(void);
extern void Sony_EjectAllDisks(void);
extern void Sony_Reset(void);
extern void Sony_Update(void);
