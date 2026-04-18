/*
	lomem_globals.cpp — Low-memory snapshot and formatting helpers
*/

#include "platform/lomem_globals.h"
#include "core/machine.h"

#include <cstring>

/* --- Snapshot helpers --- */

static const uint32_t kSnapshotSize = 4096;

void Lomem_SnapshotTake(uint8_t *snapshot)
{
	if (!g_ram) return;
	memcpy(snapshot, g_ram, kSnapshotSize);
}

bool lomem_snapshot_changed(const uint8_t *snapshot, uint32_t addr, uint16_t size)
{
	if (addr + size > kSnapshotSize) return false;
	return memcmp(snapshot + addr, g_ram + addr, size) != 0;
}
