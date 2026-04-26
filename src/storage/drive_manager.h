/*
	DriveManager — central slot manager for multiple shared drives.

	Owns a fixed-size array of HostVolume slots.  Each slot has a unique
	vRefNum and driveNum derived from its index.  Handle encoding helpers
	allow O(1) routing from a file handle back to the owning volume.
*/
#pragma once

#include <cstdint>

namespace storage
{

inline constexpr int kMaxDrives = 8;
inline constexpr int16_t kBaseVRefNum = 32000; // vRefNum = -(kBaseVRefNum + slot)
inline constexpr int16_t kBaseDriveNum = 8;	   // driveNum = kBaseDriveNum + slot

} // namespace storage
