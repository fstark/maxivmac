#include "storage/appledouble.h"
#include "storage/macroman.h"

#include <cstdio>

namespace appledouble
{

namespace
{

constexpr char kEsc = '\x1B';
constexpr char kHexDigits[] = "0123456789ABCDEF";

/* Characters that are structurally illegal in POSIX filenames,
   plus our escape character and the Mac path separator (:). */
bool NeedsEscape(uint8_t c)
{
	return c == '/' || c == ':' || c == static_cast<uint8_t>(kEsc);
}

int HexVal(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return -1;
}

} // anonymous namespace

/* Mac filename (MacRoman bytes) → host filename (UTF-8, escaped).
   Steps: escape / : ESC, then convert MacRoman→UTF-8. */
std::string HostNameFromMac(std::string_view macName)
{
	std::string result;
	result.reserve(macName.size() * 2);
	for (auto c : macName)
	{
		auto b = static_cast<uint8_t>(c);
		if (NeedsEscape(b))
		{
			result += kEsc;
			result += kHexDigits[(b >> 4) & 0xF];
			result += kHexDigits[b & 0xF];
		}
		else if (b >= 0x80)
		{
			AppendUTF8(result, kMacRomanToUnicode[b - 0x80]);
		}
		else
		{
			result += c;
		}
	}
	return result;
}

/* Host filename (UTF-8, possibly escaped) → Mac filename (MacRoman bytes).
   Returns empty string if any character is not representable in MacRoman. */
std::optional<std::string> MacNameFromHost(std::string_view hostName)
{
	/* First, convert to a plain UTF-8 string, unescaping ESC sequences */
	std::string utf8;
	utf8.reserve(hostName.size());
	for (size_t i = 0; i < hostName.size(); ++i)
	{
		if (hostName[i] == kEsc && i + 2 < hostName.size())
		{
			int hi = HexVal(hostName[i + 1]);
			int lo = HexVal(hostName[i + 2]);
			if (hi >= 0 && lo >= 0)
			{
				/* Escaped byte is already a raw MacRoman byte;
				   emit it directly as MacRoman, skip UTF-8→MacRoman. */
				utf8 += static_cast<char>(0x01); /* placeholder sentinel */
				utf8 += static_cast<char>((hi << 4) | lo);
				i += 2;
				continue;
			}
		}
		utf8 += hostName[i];
	}

	/* Now decode UTF-8 → MacRoman, honouring sentinels */
	std::string result;
	result.reserve(utf8.size());
	for (size_t i = 0; i < utf8.size();)
	{
		if (utf8[i] == 0x01 && i + 1 < utf8.size())
		{
			result += utf8[i + 1]; /* raw MacRoman byte from escape */
			i += 2;
			continue;
		}

		uint32_t cp = DecodeUTF8(utf8, i);
		auto mr = MacRomanFromCodePoint(cp);
		if (!mr.valid) return std::nullopt;
		result += static_cast<char>(mr.byte);
	}
	return result;
}

bool IsSidecar(std::string_view name)
{
	return name.size() >= 2 && name[0] == '.' && name[1] == '_';
}

} // namespace appledouble
