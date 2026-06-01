#pragma once

#include "classic/mac_types.h"

#include <cstdint>

namespace classic
{

constexpr FourCharCode FourCC(const char (&s)[5])
{
	return (static_cast<uint32_t>(static_cast<uint8_t>(s[0])) << 24) |
		   (static_cast<uint32_t>(static_cast<uint8_t>(s[1])) << 16) |
		   (static_cast<uint32_t>(static_cast<uint8_t>(s[2])) << 8) |
		   static_cast<uint32_t>(static_cast<uint8_t>(s[3]));
}

} // namespace classic