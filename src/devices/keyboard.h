/*
	KBRDEMDV.h

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

class KeyboardDevice : public Device {
public:
	uint32_t access(uint32_t data, bool writeMem, uint32_t addr) override
		{ return data; } // Not memory-mapped
	void zap() override {}
	void reset() override {}
	const char* name() const override { return "Keyboard"; }

	void dataLineChngNtfy();
	void receiveEndCommand();
	void receiveCommand();
	void update();
};

extern KeyboardDevice* g_keyboard;

// Backward-compatible free function API
extern void Kybd_DataLineChngNtfy(void);
extern void DoKybd_ReceiveEndCommand(void);
extern void DoKybd_ReceiveCommand(void);
extern void KeyBoard_Update(void);
