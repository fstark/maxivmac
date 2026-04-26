/*
	DriveManager — slot lifecycle, lookup, mount/unmount.
*/
#include "storage/drive_manager.h"
#include "core/diag.h"

#include <algorithm>

namespace storage
{

namespace fs = std::filesystem;

/* ── Mount / unmount ──────────────────────────────── */

int DriveManager::mount(const std::filesystem::path &hostDir)
{
	if (!fs::is_directory(hostDir)) return -1;

	// Find first empty slot.
	int slot = -1;
	for (int i = 0; i < kMaxDrives; ++i)
	{
		if (!slots_[i].vol.has_value())
		{
			slot = i;
			break;
		}
	}
	if (slot < 0) return -1;

	auto &s = slots_[slot];
	s.vol.emplace();
	if (!s.vol->mount(hostDir))
	{
		s.vol.reset();
		return -1;
	}
	s.vol->setSlot(slot);

	// Derive volume name from directory basename, deduplicate.
	std::string baseName = hostDir.filename().string();
	if (baseName.empty()) baseName = "Shared";
	s.volumeName = deduplicateName(baseName);

	queuePendingMount(slot);

	DIAG(ExtFS, "DriveManager: mounted slot %d \"%s\" → %s\n", slot, s.volumeName.c_str(),
		 hostDir.c_str());
	return slot;
}

bool DriveManager::unmount(int slot)
{
	if (slot < 0 || slot >= kMaxDrives) return false;
	auto &s = slots_[slot];
	if (!s.vol.has_value()) return false;

	s.vol->closeAllForks();
	s.vol.reset();
	s.volumeName.clear();

	// Remove from pending queue if present.
	std::erase(pendingMounts_, slot);

	DIAG(ExtFS, "DriveManager: unmounted slot %d\n", slot);
	return true;
}

/* ── Lookup ───────────────────────────────────────── */

HostVolume *DriveManager::volume(int slot)
{
	if (slot < 0 || slot >= kMaxDrives) return nullptr;
	auto &s = slots_[slot];
	return s.vol.has_value() ? &*s.vol : nullptr;
}

const HostVolume *DriveManager::volume(int slot) const
{
	if (slot < 0 || slot >= kMaxDrives) return nullptr;
	auto &s = slots_[slot];
	return s.vol.has_value() ? &*s.vol : nullptr;
}

int DriveManager::slotFromVRefNum(int16_t vRefNum) const
{
	int slot = -(static_cast<int>(vRefNum)) - kBaseVRefNum;
	if (slot < 0 || slot >= kMaxDrives) return -1;
	if (!slots_[slot].vol.has_value()) return -1;
	return slot;
}

HostVolume *DriveManager::volumeByVRefNum(int16_t vRefNum)
{
	int slot = slotFromVRefNum(vRefNum);
	return slot >= 0 ? volume(slot) : nullptr;
}

const HostVolume *DriveManager::volumeByVRefNum(int16_t vRefNum) const
{
	int slot = slotFromVRefNum(vRefNum);
	return slot >= 0 ? volume(slot) : nullptr;
}

HostVolume *DriveManager::volumeByDriveNum(int16_t driveNum)
{
	int slot = driveNum - kBaseDriveNum;
	if (slot < 0 || slot >= kMaxDrives) return nullptr;
	return volume(slot);
}

int DriveManager::mountedCount() const
{
	int n = 0;
	for (int i = 0; i < kMaxDrives; ++i)
		if (slots_[i].vol.has_value()) ++n;
	return n;
}

std::string_view DriveManager::volumeName(int slot) const
{
	if (slot < 0 || slot >= kMaxDrives) return {};
	return slots_[slot].volumeName;
}

const std::filesystem::path &DriveManager::hostPath(int slot) const
{
	static const std::filesystem::path kEmpty;
	if (slot < 0 || slot >= kMaxDrives) return kEmpty;
	auto &s = slots_[slot];
	if (!s.vol.has_value()) return kEmpty;
	return s.vol->rootPath();
}

int DriveManager::openForkCount(int slot) const
{
	auto *v = volume(slot);
	return v ? v->openForkCount() : 0;
}

/* ── Fork handle encoding ─────────────────────────── */

uint32_t DriveManager::openFork(int slot, uint32_t cnid, ForkType fork, uint32_t &outSize,
								OSErr &errOut, uint8_t perm)
{
	auto *vol = volume(slot);
	if (!vol)
	{
		errOut = kNsvErr;
		return 0;
	}
	uint32_t local = vol->openFork(cnid, fork, outSize, errOut, perm);
	if (local == 0) return 0;
	return EncodeHandle(slot, local);
}

std::pair<HostVolume *, uint32_t> DriveManager::resolveHandle(uint32_t handle)
{
	int slot = SlotFromHandle(handle);
	uint32_t local = LocalHandle(handle);
	auto *vol = volume(slot);
	return {vol, local};
}

/* ── Pending mount queue ──────────────────────────── */

int DriveManager::popPendingMount()
{
	if (pendingMounts_.empty()) return -1;
	int slot = pendingMounts_.front();
	pendingMounts_.erase(pendingMounts_.begin());
	return slot;
}

void DriveManager::queuePendingMount(int slot)
{
	pendingMounts_.push_back(slot);
}

/* ── Name deduplication ───────────────────────────── */

std::string DriveManager::deduplicateName(std::string_view baseName) const
{
	// Truncate to 27 chars (HFS volume name limit).
	std::string name(baseName.substr(0, 27));

	// Check for collision with existing slot names.
	auto collides = [&](const std::string &candidate) -> bool
	{
		for (int i = 0; i < kMaxDrives; ++i)
			if (slots_[i].vol.has_value() && slots_[i].volumeName == candidate) return true;
		return false;
	};

	if (!collides(name)) return name;

	for (int suffix = 2; suffix < 100; ++suffix)
	{
		std::string candidate = std::string(baseName.substr(0, 24)) + "-" + std::to_string(suffix);
		if (!collides(candidate)) return candidate;
	}

	// Should not happen with 8 max drives.
	return name;
}

} // namespace storage
