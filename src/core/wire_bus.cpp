/*
	wire_bus.cpp

	Runtime inter-device signal routing implementation.

	Part of Phase 4: Device Interface & Machine Object.
*/

#include "core/wire_bus.h"

WireBus g_wires;

void WireBus::init(int numWires)
{
	numWires_ = numWires;
	// Initialize all wires to 1 (matching AddrSpac_Init behavior)
	wires_.fill(1);
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
