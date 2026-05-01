/*
	device.cpp

	Default implementations for the Device interface.
*/

#include "devices/device.h"

uint32_t Device::access(uint32_t data, bool /*writeMem*/, uint32_t /*addr*/)
{
	// Default: no-op, return data unchanged.
	// Devices that are memory-mapped override this.
	return data;
}
