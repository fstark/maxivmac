#include "util/macroman.h"

std::string UTF8FromMacRoman(std::span<const uint8_t> macRoman)
{
	std::string result;
	result.reserve(macRoman.size() * 2);
	for (uint8_t b : macRoman)
	{
		if (b < 0x80)
			result += static_cast<char>(b);
		else
			AppendUTF8(result, kMacRomanToUnicode[b - 0x80]);
	}
	return result;
}

std::string MacRomanFromUTF8(std::string_view utf8)
{
	std::string result;
	result.reserve(utf8.size());
	size_t pos = 0;
	while (pos < utf8.size())
	{
		uint32_t cp = DecodeUTF8(utf8, pos);
		auto mr = MacRomanFromCodePoint(cp);
		result += static_cast<char>(mr.valid ? mr.byte : '?');
	}
	return result;
}
