/*
	lomem_globals.h — Low-memory snapshot and formatting helpers

	Snapshot/diff utilities for the ImGui Low Memory viewer.
	The globals table itself is now in GlobalRegistry (assets/globals.def).
*/

#pragma once

#include <cstdint>

/* Take a 4 KB snapshot of low memory ($000–$FFF) from g_ram. */
void Lomem_SnapshotTake(uint8_t *snapshot);

/* Returns true if live RAM at [addr, addr+size) differs from snapshot. */
bool lomem_snapshot_changed(const uint8_t *snapshot, uint32_t addr, uint16_t size);
