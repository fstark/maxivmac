/*
	SCReeN EMulated DeVice

	Emulation of the screen in the Mac Plus.

	This code descended from "Screen-MacOS.c" in Richard F. Bannister's
	Macintosh port of vMac, by Philip Cummins.
*/

#include "core/common.h"
#include "core/rig.h"

#include "devices/screen.h"


static constexpr int kMain_Offset = 0x5900;
#define kAlternate_Offset 0xD900

/* Select the active framebuffer (VRAM for NuBus Macs, main RAM
   for compacts) and push the frame to the platform display layer. */
void ScreenDevice::endTickNotify()
{
	uint8_t *screencurrentbuff;

	if (g_rig->config().includeVidMem)
	{
		screencurrentbuff = g_vidMem;
	}
	else
	{
		/* Compact Macs: screen buffer in main RAM.
		   VIA1 port A bit 6 (vPage2): 1 = main, 0 = alternate. */
		uint32_t offset = g_wires.get(Wire_VIA1_iA6) ? kMain_Offset : kAlternate_Offset;
		screencurrentbuff = get_ram_address(g_rig->config().ramSize() - offset);
	}

	Screen_OutputFrame(screencurrentbuff);
}
