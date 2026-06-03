/*
	Mac Types — Macintosh type aliases

	Defines type aliases for classic Macintosh types used throughout
	the emulator: FourCharCode, OSType, ResType, and CreatorCode.
*/
#pragma once

#include <cstdint>

namespace classic
{

using FourCharCode = uint32_t;
using OSType = FourCharCode;
using ResType = FourCharCode;
using CreatorCode = FourCharCode;

} // namespace classic