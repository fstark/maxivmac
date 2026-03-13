/*
	machine_obj.cpp

	Machine class implementation — owns all emulator state.

	Part of Phase 4: Device Interface & Machine Object.
*/

#include "core/machine_obj.h"
#include "devices/device.h"

// Device headers — needed to create instances and set global pointers
#include "devices/iwm.h"
#include "devices/scc.h"
#include "devices/scsi.h"
#include "devices/via.h"
#include "devices/via2.h"
#include "devices/rtc.h"
#include "devices/adb.h"
#include "devices/asc.h"
#include "devices/screen.h"
#include "devices/video.h"
#include "devices/sony.h"
#include "devices/rom.h"
#include "devices/mouse.h"
#include "devices/keyboard.h"
#include "devices/sound.h"
#include "devices/pmu.h"

// Global Machine pointer for backward compatibility during migration
Machine* g_machine = nullptr;

Machine::Machine(MachineConfig config)
	: config_(std::move(config))
{
}

Machine::~Machine()
{
	// Clear all global device pointers before devices are destroyed.
	// The unique_ptrs in devices_ will be destroyed after this runs,
	// so we must null-out the raw pointers first.
	g_screen = nullptr;
	g_via1 = nullptr;
	g_via2 = nullptr;
	g_rtc = nullptr;

	if (g_machine == this) {
		g_machine = nullptr;
	}
}

bool Machine::init()
{
	// Set global pointer for backward compatibility.
	g_machine = this;

	// Set runtime screen globals from config (used by platform layer macros).
	extern uint16_t g_screenWidth;
	extern uint16_t g_screenHeight;
	extern uint8_t  g_screenDepth;
	g_screenWidth  = config_.screenWidth;
	g_screenHeight = config_.screenHeight;
	g_screenDepth  = config_.screenDepth;

	// Create device instances and set backward-compatible global pointers.
	// The order doesn't matter for construction, but we group by subsystem.

	// Always-present devices
	addDevice(std::make_unique<IWMDevice>());
	addDevice(std::make_unique<SCCDevice>());
	addDevice(std::make_unique<SCSIDevice>());
	addDevice(std::make_unique<ROMDevice>());
	addDevice(std::make_unique<SonyDevice>());
	addDevice(std::make_unique<MouseDevice>());
	{
		auto dev = std::make_unique<ScreenDevice>();
		g_screen = dev.get();
		addDevice(std::move(dev));
	}

	// Conditional devices based on config
	if (config_.emVIA1) {
		auto dev = std::make_unique<VIA1Device>();
		g_via1 = dev.get();
		addDevice(std::move(dev));
	}
	if (config_.emVIA2) {
		auto dev = std::make_unique<VIA2Device>();
		g_via2 = dev.get();
		addDevice(std::move(dev));
	}
	if (config_.emRTC) {
		auto dev = std::make_unique<RTCDevice>();
		g_rtc = dev.get();
		addDevice(std::move(dev));
	}
	if (config_.emADB) {
		addDevice(std::make_unique<ADBDevice>());
	}
	if (config_.emASC) {
		addDevice(std::make_unique<ASCDevice>());
	}
	if (config_.emVidCard) {
		addDevice(std::make_unique<VideoDevice>());
	}
	if (config_.emClassicKbrd) {
		addDevice(std::make_unique<KeyboardDevice>());
	}
	if (config_.emClassicSnd) {
		addDevice(std::make_unique<SoundDevice>());
	}
	if (config_.emPMU) {
		addDevice(std::make_unique<PMUDevice>());
	}

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
