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

/* ── Handle encoding ──────────────────────────────── */

// Encode a slot index into the upper 4 bits of a file handle.
inline constexpr uint32_t EncodeHandle(int slot, uint32_t localHandle)
{
	return (static_cast<uint32_t>(slot) << 28) | (localHandle & 0x0FFF'FFFF);
}

inline constexpr int SlotFromHandle(uint32_t handle)
{
	return static_cast<int>(handle >> 28);
}

inline constexpr uint32_t LocalHandle(uint32_t handle)
{
	return handle & 0x0FFF'FFFF;
}

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

	// Open a fork on a specific slot, encoding the slot in the handle.
	uint32_t openFork(int slot, uint32_t cnid, ForkType fork, uint32_t &outSize, OSErr &errOut,
					  uint8_t perm = 0);

	// Decode a handle into (HostVolume*, localHandle).
	std::pair<HostVolume *, uint32_t> resolveHandle(uint32_t handle);

	// Queue a drive for later guest discovery.
	void queuePendingMount(int slot);

	/* ── Working directories (global, cross-volume) ── */

	struct WDEntry
	{
		int slot = -1;
		uint32_t dirID = 0;
		uint32_t procID = 0;
	};

	uint32_t openWD(int slot, uint32_t dirID, uint32_t procID = 0);
	uint32_t wdToDirID(uint32_t wdRef) const;
	uint32_t wdToProcID(uint32_t wdRef) const;
	int wdToSlot(uint32_t wdRef) const;
	void closeWD(uint32_t wdRef);
	bool isOurWD(uint32_t wdRef) const;

	void setDefaultWD(uint32_t wdRef);
	uint32_t defaultWD() const;

	// Root WD for a slot (created automatically on mount).
	uint32_t rootWD(int slot) const;

	// Read DefVCBPtr from guest RAM, check if it points to one of
	// our VCBs.  Returns true and sets outSlot if ours.
	bool isDefaultOurs(int &outSlot) const;

	// Resolve (vRefNum, rawDirID) → actual dirID + slot.
	// Returns 0 if the vRefNum doesn't match any of our volumes.
	uint32_t resolveDir(int16_t vRefNum, uint32_t rawDirID, int &outSlot) const;

	// Volume name lookup by case-insensitive name comparison.
	int slotFromName(std::string_view name) const;

private:
	struct Slot
	{
		std::optional<HostVolume> vol;
		std::string volumeName; // deduped Mac-visible name
	};
	std::array<Slot, kMaxDrives> slots_;
	std::vector<int> pendingMounts_;

	std::unordered_map<uint32_t, WDEntry> wdTable_;
	uint32_t nextWD_ = 1;
	uint32_t defaultWD_ = 0;
	std::array<uint32_t, kMaxDrives> rootWD_{};

	std::string deduplicateName(std::string_view baseName) const;
};

} // namespace storage
