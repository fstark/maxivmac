/*
	machine_obj.h

	Machine class — owns all emulator state: config, CPU, devices,
	wire bus, ICT scheduler, and memory buffers.

	Separate from existing machine.h to avoid conflicts during migration.

	Part of Phase 4: Device Interface & Machine Object.
*/

#pragma once
#include <memory>
#include <vector>
#include "core/machine_config.h"
#include "core/wire_bus.h"
#include "core/ict_scheduler.h"

class Device;

class Machine {
public:
	explicit Machine(MachineConfig config);
	~Machine();

	// Lifecycle
	bool init();        // allocate buffers, create devices, build ATT
	void reset();       // warm reset all devices
	void zap();         // cold start (power-on reset)

	// Accessors
	const MachineConfig& config() const { return config_; }
	WireBus&       wireBus()       { return wireBus_; }
	ICTScheduler&  ict()           { return ict_; }

	// Memory buffers
	uint8_t* ram()    const { return ram_.get(); }
	uint8_t* rom()    const { return rom_; }
	uint8_t* vidMem() const { return vidMem_.get(); }
	uint8_t* vidROM() const { return vidROM_.get(); }

	void setRom(uint8_t* r) { rom_ = r; }

	uint32_t ramSize() const { return config_.ramSize(); }

	// Device registry
	void addDevice(std::unique_ptr<Device> dev);

	template<typename T>
	T* findDevice() const {
		for (auto& dev : devices_) {
			if (auto* p = dynamic_cast<T*>(dev.get())) {
				return p;
			}
		}
		return nullptr;
	}

	// Interrupt priority logic
	void recalcIPL();
	uint8_t curIPL() const { return curIPL_; }

private:
	MachineConfig config_;
	WireBus       wireBus_;
	ICTScheduler  ict_;

	// Memory buffers
	std::unique_ptr<uint8_t[]> ram_;
	uint8_t*                   rom_ = nullptr;  // set externally
	std::unique_ptr<uint8_t[]> vidMem_;
	std::unique_ptr<uint8_t[]> vidROM_;

	// Devices
	std::vector<std::unique_ptr<Device>> devices_;

	// Interrupt state
	uint8_t curIPL_ = 0;
	bool    interruptButton_ = false;
};

// Global Machine pointer — backward compatibility during migration.
// Removed in final cleanup (Step 4.19).
extern Machine* g_machine;
