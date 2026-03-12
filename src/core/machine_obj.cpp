/*
	machine_obj.cpp

	Machine class implementation — owns all emulator state.

	Part of Phase 4: Device Interface & Machine Object.
*/

#include "core/machine_obj.h"
#include "devices/device.h"
#include <cstring>

// Global Machine pointer for backward compatibility during migration
Machine* g_machine = nullptr;

static constexpr uint32_t RAMSafetyMarginFudge = 4;

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
	// Allocate RAM
	uint32_t totalRam = config_.ramSize() + RAMSafetyMarginFudge;
	ram_ = std::make_unique<uint8_t[]>(totalRam);
	std::memset(ram_.get(), 0, totalRam);

	// Allocate video ROM if enabled
	if (config_.emVidCard && config_.vidROMSize > 0) {
		vidROM_ = std::make_unique<uint8_t[]>(config_.vidROMSize);
		std::memset(vidROM_.get(), 0, config_.vidROMSize);
	}

	// Allocate video memory if enabled
	if (config_.includeVidMem && config_.vidMemSize > 0) {
		uint32_t totalVidMem = config_.vidMemSize + RAMSafetyMarginFudge;
		vidMem_ = std::make_unique<uint8_t[]>(totalVidMem);
		std::memset(vidMem_.get(), 0, totalVidMem);
	}

	// Initialize WireBus
	// For now, use the wire count for current model configuration
	wireBus_.init(kMaxWires);

	// Set global pointer
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
