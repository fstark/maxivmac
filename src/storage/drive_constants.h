/*
	Shared drive constants — vRefNum/driveNum base values and limits.

	Shared between host_volume.h and drive_manager.h to avoid circular includes.
*/
#pragma once

#include <cstdint>

namespace storage
{

inline constexpr int kMaxDrives = 8;
inline constexpr int16_t kBaseVRefNum = 32000; // vRefNum = -(kBaseVRefNum + slot)
inline constexpr int16_t kBaseDriveNum = 8;	   // driveNum = kBaseDriveNum + slot

} // namespace storage
