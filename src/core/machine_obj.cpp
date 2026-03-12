/*
	machine_obj.cpp

	Machine class implementation — owns all emulator state.

	Part of Phase 4: Device Interface & Machine Object.
*/

#include "core/machine_obj.h"
#include "devices/device.h"

// Global Machine pointer for backward compatibility during migration
Machine* g_machine = nullptr;

Machine::Machine(MachineConfig config)
	: config_(std::move(config))
{
}

Machine::~Machine()
{
	if (g_machine == this) {
		g_machine = nullptr;
	}
}

bool Machine::init()
{
	// Set global pointer for backward compatibility.
	// Memory allocation is handled by EmulationReserveAlloc() called
	// from platform code — we don't duplicate that here.
	g_machine = this;

	return true;
}

void Machine::reset()
{
	for (auto& dev : devices_) {
		dev->reset();
	}
}

void Machine::zap()
{
	for (auto& dev : devices_) {
		dev->zap();
	}
}

void Machine::addDevice(std::unique_ptr<Device> dev)
{
	dev->machine_ = this;
	devices_.push_back(std::move(dev));
}

void Machine::recalcIPL()
{
	// Placeholder — will be populated from VIAorSCCinterruptChngNtfy()
	// logic in machine.cpp during Step 4.12/4.14.
}
