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

/* Encode/decode guest WD refnums.
   Guest vRefNum for a WD is -(wdRef + kBaseVRefNum). */
inline uint32_t DecodeGuestWDRef(int16_t guestVRefNum)
{
	return static_cast<uint32_t>(-(static_cast<int32_t>(guestVRefNum)) - kBaseVRefNum);
}

inline int16_t EncodeGuestWDRef(uint32_t wdRef)
{
	return static_cast<int16_t>(-(static_cast<int32_t>(wdRef) + kBaseVRefNum));
}

} // namespace storage
