/*
	SCReeN EMulated DeVice

	Emulation of the screen in the Mac Plus.

	This code descended from "Screen-MacOS.c" in Richard F. Bannister's
	Macintosh port of vMac, by Philip Cummins.
*/

#include "core/common.h"
#include "core/machine_obj.h"

#include "devices/screen.h"


static constexpr int kMain_Offset = 0x5900;
#define kAlternate_Offset 0xD900

/* Select the active framebuffer (VRAM for NuBus Macs, main RAM
   for compacts) and push the frame to the platform display layer. */
void ScreenDevice::endTickNotify()
{
	uint8_t * screencurrentbuff;

	if (g_machine->config().includeVidMem) {
		screencurrentbuff = g_vidMem;
	} else {
		/* Compact Macs: screen buffer in main RAM
		   (uses SCRNvPage2 wire for page selection — not yet wired) */
		screencurrentbuff = get_ram_address(
			g_machine->config().ramSize() - kMain_Offset);
	}

	Screen_OutputFrame(screencurrentbuff);
}
