/*
	DriveManager — central slot manager for multiple shared drives.

	Owns a fixed-size array of HostVolume slots.  Each slot has a unique
	vRefNum and driveNum derived from its index.  Handle encoding helpers
	allow O(1) routing from a file handle back to the owning volume.
*/
#pragma once

#include "storage/drive_constants.h"
#include "storage/host_volume.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace storage
{

class DriveManager
{
public:
	// Mount hostDir in the next free slot.  Returns the slot index,
	// or -1 if all slots are full or the path is invalid.
	int mount(const std::filesystem::path &hostDir);

	// Unmount slot: close all open forks, free catalog, release slot.
	// Returns false if the slot is empty or out of range.
	bool unmount(int slot);

	// Look up by slot index.  Returns nullptr if slot empty/invalid.
	HostVolume *volume(int slot);
	const HostVolume *volume(int slot) const;

	// Look up by guest vRefNum.  Returns nullptr if no match.
	HostVolume *volumeByVRefNum(int16_t vRefNum);
	const HostVolume *volumeByVRefNum(int16_t vRefNum) const;

	// Look up by guest drive number.  Returns nullptr if no match.
	HostVolume *volumeByDriveNum(int16_t driveNum);

	// Slot index from vRefNum.  Returns -1 if not ours.
	int slotFromVRefNum(int16_t vRefNum) const;

	// Number of currently mounted drives.
	int mountedCount() const;

	// Volume name for a slot (derived from host dir basename, deduped).
	std::string_view volumeName(int slot) const;

	// Host path for a slot.
	const std::filesystem::path &hostPath(int slot) const;

	// Open-fork count across all forks in a slot.
	int openForkCount(int slot) const;

	// Iterate all mounted slots.  Callback receives (slot, HostVolume&).
	template <typename Fn> void forEach(Fn &&fn) const
	{
		for (int i = 0; i < kMaxDrives; ++i)
			if (slots_[i].vol.has_value()) fn(i, const_cast<HostVolume &>(*slots_[i].vol));
	}

	// Check whether a new drive has been mounted and is pending
	// guest discovery.  Returns the slot index, or -1 if none.
	int popPendingMount();

	// Queue a drive for later guest discovery.
	void queuePendingMount(int slot);

private:
	struct Slot
	{
		std::optional<HostVolume> vol;
		std::string volumeName; // deduped Mac-visible name
	};
	std::array<Slot, kMaxDrives> slots_;
	std::vector<int> pendingMounts_;

	std::string deduplicateName(std::string_view baseName) const;
};

} // namespace storage
