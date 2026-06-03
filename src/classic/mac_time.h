/*
	Mac Time — Time conversion utilities

	Provides the Mac epoch offset constant used to convert between
	Macintosh epoch (1904) and Unix epoch (1970) timestamps.
*/
#pragma once

#include <cstdint>

namespace classic
{

inline constexpr uint32_t kMacEpochOffset = 2082844800u; // 1904->1970

} // namespace classic