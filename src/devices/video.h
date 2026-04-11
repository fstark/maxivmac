/*
	Video — Display output and video mode management
*/
#pragma once

#include "devices/device.h"
#include <cstdint>

class VideoDevice : public Device {
public:
	uint32_t access(uint32_t data, bool /*writeMem*/, uint32_t /*addr*/) override
		{ return data; }
	void zap() override {}
	void reset() override;
	const char* name() const override { return "Video"; }

	bool init();
	uint16_t vidReset(); // returns mode value (128)
	void update();
	void extnVideoAccess(uint32_t p);
};

/* Set host desktop size before VideoDevice::init() so host-derived
   resolutions (displayModeIDs 100, 101) can be added to the table. */
void Vid_SetHostDesktop(uint16_t w, uint16_t h);

/* Return the maximum resolution dimensions across all resolutions
   (classic + host-derived).  Must be called after Vid_SetHostDesktop. */
void Vid_MaxResolutionSize(uint32_t* outW, uint32_t* outH);

/* Maximum VRAM that the NuBus slot 9 address space can map (6 MB). */
uint32_t Vid_MaxVRAM();

/* Check/clear the resolution-changed flag (set by SwitchMode). */
bool Vid_ResolutionChanged();
void Vid_ClearResolutionChanged();
uint16_t Vid_CurrentWidth();
uint16_t Vid_CurrentHeight();

