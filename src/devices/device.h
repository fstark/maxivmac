/*
	device.h

	Abstract Device interface for all emulated hardware devices.

	Every emulated hardware component (VIA, SCC, SCSI, etc.) implements
	this interface. The Machine object owns all devices and dispatches
	memory-mapped I/O through Device::access().

	Part of Phase 4: Device Interface & Machine Object.
*/

#pragma once
#include <cstdint>

class Machine;  // forward declaration

class Device {
public:
	virtual ~Device() = default;

	// Memory-mapped I/O access (for devices on the bus).
	// Not all devices are memory-mapped (e.g., RTC is wire-driven).
	// Default implementation reports abnormal access and returns Data.
	virtual uint32_t access(uint32_t data, bool writeMem, uint32_t addr);

	// Lifecycle
	virtual void zap()   {}  // Cold start (power-on)
	virtual void reset() {}  // Warm reset

	// Identity
	virtual const char* name() const = 0;

protected:
	Machine* machine_ = nullptr;  // set by Machine when device is registered
	friend class Machine;
};
