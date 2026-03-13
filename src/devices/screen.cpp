/*
	SCRNEMDV.c

	Copyright (C) 2006 Philip Cummins, Richard F. Bannister,
		Paul C. Pratt

	You can redistribute this file and/or modify it under the terms
	of version 2 of the GNU General Public License as published by
	the Free Software Foundation.  You should have received a copy
	of the license along with this file; see the file COPYING.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	license for more details.
*/

/*
	SCReeN EMulated DeVice

	Emulation of the screen in the Mac Plus.

	This code descended from "Screen-MacOS.c" in Richard F. Bannister's
	Macintosh port of vMac, by Philip Cummins.
*/

#include "core/common.h"
#include "core/machine_obj.h"

#include "devices/screen.h"


#define kMain_Offset      0x5900
#define kAlternate_Offset 0xD900

void ScreenDevice::endTickNotify()
{
	uint8_t * screencurrentbuff;

	if (g_machine->config().includeVidMem) {
		screencurrentbuff = VidMem;
	} else {
		/* Compact Macs: screen buffer in main RAM
		   (uses SCRNvPage2 wire for page selection — not yet wired) */
		screencurrentbuff = get_ram_address(
			g_machine->config().ramSize() - kMain_Offset);
	}

	Screen_OutputFrame(screencurrentbuff);
}
