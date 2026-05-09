/*
	MOUSe EMulated DeVice

	Emulation of the mouse in the Mac Plus.

	This code descended from "Mouse-MacOS.c" in Richard F. Bannister's
	Macintosh port of vMac, by Philip Cummins.
*/

#include "core/common.h"
#include "core/rig.h"
#include "core/wire_bus.h"
#include "core/wire_ids.h"
#include "devices/scc.h"
#include "platform/common/event_queue.h"

#include "devices/mouse.h"

/*
	Mouse_Enabled() — model-dependent mouse gate.

	On Mac Plus/SE/128K (emClassicKbrd): the SCC.MIE bit prevents
	mouse updates from corrupting the boot-time memory test.  Once
	MIE has been set for the first time the boot test is over, so we
	latch that and never block mouse again — driver-level SCC resets
	(e.g. AppleTalk init) must not kill the cursor.

	On Mac II (emADB, !emClassicKbrd): the original code used
	!ADBMouseDisabled.  ADBMouseDisabled starts at 1 (wire default)
	and is cleared to 0 when the ADB manager first polls the mouse.
*/
static bool s_mouseEnabledLatch = false;

bool Mouse_Enabled()
{
	if (g_rig->config().emClassicKbrd)
	{
		if (s_mouseEnabledLatch) return true;
		if (auto *scc = g_rig->findDevice<SCCDevice>())
		{
			if (scc->interruptsEnabled())
			{
				s_mouseEnabledLatch = true;
				return true;
			}
		}
		return false;
	}
	return !ADBMouseDisabled;
}

/* Global singleton */

/* Process queued mouse events: apply position deltas or absolute
   coordinates into Mac low-memory globals, and handle button state
   for Classic keyboard models via VIA1 wire. */
void MouseDevice::update()
{
	if (0 != g_masterEvtQLock)
	{
		--g_masterEvtQLock;
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
	if (Mouse_Enabled())
	{
		if (0 == g_masterEvtQLock)
		{
			TimedEvent *p = EventQ_Peek(g_ict.nextCount);
			if (p != nullptr)
			{
				if (g_rig->config().emClassicKbrd && EvtQElKind::MouseDelta == p->kind)
				{
					if ((p->u.pos.h != 0) || (p->u.pos.v != 0))
					{
						put_ram_word(0x0828, get_ram_word(0x0828) + p->u.pos.v);
						put_ram_word(0x082A, get_ram_word(0x082A) + p->u.pos.h);
						put_ram_byte(0x08CE, get_ram_byte(0x08CF));
						/* Tell MacOS to redraw the Mouse */
					}
					EventQ_Pop();
				}
				else if (EvtQElKind::MousePos == p->kind)
				{
					uint32_t NewMouse = (p->u.pos.v << 16) | p->u.pos.h;

					if (get_ram_long(0x0828) != NewMouse)
					{
						put_ram_long(0x0828, NewMouse);
						/* Set Mouse Position */
						put_ram_long(0x082C, NewMouse);
						if (g_rig->config().emClassicKbrd)
						{
							put_ram_byte(0x08CE, get_ram_byte(0x08CF));
							/* Tell MacOS to redraw the Mouse */
						}
						else
						{
							put_ram_long(0x0830, NewMouse);
							put_ram_byte(0x08CE, 0xFF);
							/* Tell MacOS to redraw the Mouse */
						}
					}
					EventQ_Pop();
				}
			}
		}
	}

	if (rig_->config().emClassicKbrd)
	{
		if (0 == g_masterEvtQLock)
		{
			TimedEvent *p = EventQ_Peek(g_ict.nextCount);
			if (p != nullptr && EvtQElKind::MouseButton == p->kind)
			{
				g_wires.set(Wire_VIA1_iB3, p->u.press.down ? 0 : 1);
				EventQ_Pop();
				g_masterEvtQLock = 4;
			}
		}
	}
}

/* Feed current mouse position from Mac RAM back to the platform
   layer so the host cursor can track the emulated pointer. */
void MouseDevice::endTickNotify()
{
	if (Mouse_Enabled())
	{
		/* tell platform specific code where the mouse went */
		g_curMouseV = get_ram_word(0x082C);
		g_curMouseH = get_ram_word(0x082E);
	}
}