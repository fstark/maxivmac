/*
	SCCEMDEV.h

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

// Forward: SCC_dolog and SCC_TrackMore are defined in scc.cpp

// SCC Device class wrapping the original SCC emulation.
// Internal state remains as file-scope statics in scc.cpp for now
// due to heavy conditional compilation (EmLocalTalk, SCC_TrackMore).
// Full state migration will happen in a later phase.
class SCCDevice : public Device {
public:
	// Device interface
	uint32_t access(uint32_t data, bool writeMem, uint32_t addr) override;
	void zap() override {} // SCC has no separate zap
	void reset() override;
	const char* name() const override { return "SCC"; }

	bool interruptsEnabled();

#if EmLocalTalk
	void localTalkTick();
#endif
};

// Global singleton pointer (for backward compatibility during migration)
extern SCCDevice* g_scc;

// Backward-compatible free function API (forwards to g_scc)
extern void SCC_Reset(void);
extern uint32_t SCC_Access(uint32_t Data, bool WriteMem, uint32_t addr);
extern bool SCC_InterruptsEnabled(void);

#if EmLocalTalk
extern void LocalTalkTick(void);
#endif
