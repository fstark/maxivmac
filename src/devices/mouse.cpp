/*
	MOUSEMDV.c

	Copyright (C) 2006 Philip Cummins, Paul C. Pratt

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
	MOUSe EMulated DeVice

	Emulation of the mouse in the Mac Plus.

	This code descended from "Mouse-MacOS.c" in Richard F. Bannister's
	Macintosh port of vMac, by Philip Cummins.
*/

#include "core/common.h"
#include "core/machine_obj.h"
#include "core/wire_bus.h"
#include "core/wire_ids.h"
#include "devices/scc.h"

#include "devices/mouse.h"

/* Global singleton */
MouseDevice* g_mouse = nullptr;

void MouseDevice::update()
{
	if (0 != MasterMyEvtQLock) {
		--MasterMyEvtQLock;
	}

	/*
		Check mouse position first. After mouse button or key event,
		can't process another mouse position until following tick,
		otherwise button or key will be in wrong place.
	*/

	/*
		if start doing this too soon after boot,
		will mess up memory check
	*/
	if (Mouse_Enabled()) {
		MyEvtQEl *p;

		if (
			(0 == MasterMyEvtQLock) &&
			(nullptr != (p = MyEvtQOutP())))
		{
#if EnableMouseMotion
			if (g_machine->config().emClassicKbrd
				&& MyEvtQElKindMouseDelta == p->kind)
			{
				if ((p->u.pos.h != 0) || (p->u.pos.v != 0)) {
					put_ram_word(0x0828,
						get_ram_word(0x0828) + p->u.pos.v);
					put_ram_word(0x082A,
						get_ram_word(0x082A) + p->u.pos.h);
					put_ram_byte(0x08CE, get_ram_byte(0x08CF));
						/* Tell MacOS to redraw the Mouse */
				}
				MyEvtQOutDone();
			} else
#endif
			if (MyEvtQElKindMousePos == p->kind) {
				uint32_t NewMouse = (p->u.pos.v << 16) | p->u.pos.h;

				if (get_ram_long(0x0828) != NewMouse) {
					put_ram_long(0x0828, NewMouse);
						/* Set Mouse Position */
					put_ram_long(0x082C, NewMouse);
					if (g_machine->config().emClassicKbrd) {
						put_ram_byte(0x08CE, get_ram_byte(0x08CF));
							/* Tell MacOS to redraw the Mouse */
					} else {
						put_ram_long(0x0830, NewMouse);
						put_ram_byte(0x08CE, 0xFF);
							/* Tell MacOS to redraw the Mouse */
					}
				}
				MyEvtQOutDone();
			}
		}
	}

	if (machine_->config().emClassicKbrd)
	{
		MyEvtQEl *p;

		if (
			(0 == MasterMyEvtQLock) &&
			(nullptr != (p = MyEvtQOutP())))
		{
			if (MyEvtQElKindMouseButton == p->kind) {
				g_wires.set(Wire_VIA1_iB3, p->u.press.down ? 0 : 1);
				MyEvtQOutDone();
				MasterMyEvtQLock = 4;
			}
		}
	}
}

void MouseDevice::endTickNotify()
{
	if (Mouse_Enabled()) {
		/* tell platform specific code where the mouse went */
		CurMouseV = get_ram_word(0x082C);
		CurMouseH = get_ram_word(0x082E);
	}
}

/* ===== Backward-compatible free function API ===== */

void Mouse_Update(void) { g_mouse->update(); }
void Mouse_EndTickNotify(void) { g_mouse->endTickNotify(); }
