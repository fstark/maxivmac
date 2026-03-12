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
#if EmClassicSnd
#include "devices/sound.h"
#endif
#if EmPMU
#include "devices/pmu.h"
#endif

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
	g_iwm = nullptr;
	g_scc = nullptr;
	g_scsi = nullptr;
	g_rom = nullptr;
	g_sony = nullptr;
	g_mouse = nullptr;
	g_screen = nullptr;
	g_via1 = nullptr;
	g_via2 = nullptr;
	g_rtc = nullptr;
	g_adb = nullptr;
	g_asc = nullptr;
	g_video = nullptr;
	g_keyboard = nullptr;
#if EmClassicSnd
	g_sound = nullptr;
#endif
#if EmPMU
	g_pmu = nullptr;
#endif

	if (g_machine == this) {
		g_machine = nullptr;
	}
}

bool Machine::init()
{
	// Set global pointer for backward compatibility.
	g_machine = this;

	// Create device instances and set backward-compatible global pointers.
	// The order doesn't matter for construction, but we group by subsystem.

	// Always-present devices
	{
		auto dev = std::make_unique<IWMDevice>();
		g_iwm = dev.get();
		addDevice(std::move(dev));
	}
	{
		auto dev = std::make_unique<SCCDevice>();
		g_scc = dev.get();
		addDevice(std::move(dev));
	}
	{
		auto dev = std::make_unique<SCSIDevice>();
		g_scsi = dev.get();
		addDevice(std::move(dev));
	}
	{
		auto dev = std::make_unique<ROMDevice>();
		g_rom = dev.get();
		addDevice(std::move(dev));
	}
	{
		auto dev = std::make_unique<SonyDevice>();
		g_sony = dev.get();
		addDevice(std::move(dev));
	}
	{
		auto dev = std::make_unique<MouseDevice>();
		g_mouse = dev.get();
		addDevice(std::move(dev));
	}
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
		auto dev = std::make_unique<ADBDevice>();
		g_adb = dev.get();
		addDevice(std::move(dev));
	}
	if (config_.emASC) {
		auto dev = std::make_unique<ASCDevice>();
		g_asc = dev.get();
		addDevice(std::move(dev));
	}
	if (config_.emVidCard) {
		auto dev = std::make_unique<VideoDevice>();
		g_video = dev.get();
		addDevice(std::move(dev));
	}
	if (config_.emClassicKbrd) {
		auto dev = std::make_unique<KeyboardDevice>();
		g_keyboard = dev.get();
		addDevice(std::move(dev));
	}
#if EmClassicSnd
	if (config_.emClassicSnd) {
		auto dev = std::make_unique<SoundDevice>();
		g_sound = dev.get();
		addDevice(std::move(dev));
	}
#endif
#if EmPMU
	if (config_.emPMU) {
		auto dev = std::make_unique<PMUDevice>();
		g_pmu = dev.get();
		addDevice(std::move(dev));
	}
#endif

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
