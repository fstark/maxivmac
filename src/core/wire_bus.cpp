/*
	wire_bus.cpp

	Runtime inter-device signal routing implementation.

	Part of Phase 4: Device Interface & Machine Object.
*/

#include "core/wire_bus.h"
#include "core/wire_ids.h"

WireBus g_wires;

void WireBus::init(int numWires)
{
	numWires_ = numWires;
	/*
		Initialize all wires to 1.  This matches the original AddrSpac_Init
		behavior and the idle state of most bus signals:
		  - ADBMouseDisabled=1: mouse disabled until ADB polls (Mac II), or
		    until Mouse_Enabled() uses SCC path (Plus/SE/128K)
		  - VBLintunenbl=1: VBL interrupts disabled until ROM video driver
		    calls kCmndVideoSetIntEnbl
		  - VBLinterrupt=1: no VBL interrupt pending (goes to 0 to fire)
		  - SoundDisable=1: sound disabled until ROM configures VIA port B

		The VIA/SCC Zap/Reset routines clear the interrupt request wires
		to 0, so we pre-set those here to match post-Zap state.
	*/
	wires_.fill(1);

	/* Interrupt request lines: 0 = no interrupt pending.
	   These match what VIA1_Zap, VIA2_Zap, SCC_Reset set. */
	wires_[Wire_VIA1_InterruptRequest] = 0;
	wires_[Wire_VIA2_InterruptRequest] = 0;
	wires_[Wire_SCCInterruptRequest] = 0;

	for (auto& cbs : changeCallbacks_) cbs.clear();
	for (auto& cbs : pulseCallbacks_)  cbs.clear();
}

void WireBus::set(int wireId, uint8_t val)
{
	if (wires_[wireId] != val) {
		wires_[wireId] = val;
		for (auto& cb : changeCallbacks_[wireId]) {
			cb();
		}
	}
}

void WireBus::onChange(int wireId, ChangeCallback cb)
{
	changeCallbacks_[wireId].push_back(std::move(cb));
}

void WireBus::onPulse(int wireId, ChangeCallback cb)
{
	pulseCallbacks_[wireId].push_back(std::move(cb));
}

void WireBus::pulse(int wireId)
{
	for (auto& cb : pulseCallbacks_[wireId]) {
		cb();
	}
}
