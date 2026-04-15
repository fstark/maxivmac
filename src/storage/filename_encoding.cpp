#include "storage/appledouble.h"

#include <cctype>

namespace appledouble
{

namespace
{

// Characters that must be escaped for POSIX filesystems:
// " * / : < > ? \ ^ |
bool NeedsEscape(uint8_t c)
{
	switch (c)
	{
		case 0x22: // "
		case 0x2A: // *
		case 0x2F: // /
		case 0x3A: // :
		case 0x3C: // <
		case 0x3E: // >
		case 0x3F: // ?
		case 0x5C: // backslash
		case 0x5E: // ^
		case 0x7C: // |
			return true;
		default:
			return false;
	}
}

constexpr char kHexDigits[] = "0123456789ABCDEF";

int HexVal(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return -1;
}

} // anonymous namespace

std::string HostNameFromMac(std::string_view macName)
{
	std::string result;
	result.reserve(macName.size());
	for (auto c : macName)
	{
		auto b = static_cast<uint8_t>(c);
		if (NeedsEscape(b))
		{
			result += '^';
			result += kHexDigits[(b >> 4) & 0xF];
			result += kHexDigits[b & 0xF];
		}
		else
		{
			result += c;
		}
	}
	return result;
}

std::string MacNameFromHost(std::string_view hostName)
{
	std::string result;
	result.reserve(hostName.size());
	for (size_t i = 0; i < hostName.size(); ++i)
	{
		if (hostName[i] == '^' && i + 2 < hostName.size())
		{
			int hi = HexVal(hostName[i + 1]);
			int lo = HexVal(hostName[i + 2]);
			if (hi >= 0 && lo >= 0)
			{
				result += static_cast<char>((hi << 4) | lo);
				i += 2;
				continue;
			}
		}
		result += hostName[i];
	}
	return result;
}

bool IsSidecar(std::string_view name)
{
	return name.size() >= 2 && name[0] == '.' && name[1] == '_';
}

} // namespace appledouble
