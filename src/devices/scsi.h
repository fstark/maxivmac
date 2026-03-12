/*
	SCSIEMDV.h

	Copyright (C) 2004 Philip Cummins, Paul C. Pratt

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

class SCSIDevice : public Device {
public:
	uint32_t access(uint32_t data, bool writeMem, uint32_t addr) override;
	void zap() override {}
	void reset() override;
	const char* name() const override { return "SCSI"; }
};

extern SCSIDevice* g_scsi;

// Backward-compatible free function API
extern void SCSI_Reset(void);
extern uint32_t SCSI_Access(uint32_t Data, bool WriteMem, uint32_t addr);
