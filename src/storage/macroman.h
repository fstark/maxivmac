/* MacRoman ↔ Unicode conversion primitives.
   Internal header shared by text_convert.cpp and filename_encoding.cpp. */
#pragma once

#include <cstdint>
#include <string>

namespace appledouble
{

/* MacRoman byte (0x80..0xFF) → Unicode code point */
inline constexpr uint32_t kMacRomanToUnicode[128] = {
	0x00C4, 0x00C5, 0x00C7, 0x00C9, 0x00D1, 0x00D6, 0x00DC, 0x00E1, // 80-87
	0x00E0, 0x00E2, 0x00E4, 0x00E3, 0x00E5, 0x00E7, 0x00E9, 0x00E8, // 88-8F
	0x00EA, 0x00EB, 0x00ED, 0x00EC, 0x00EE, 0x00EF, 0x00F1, 0x00F3, // 90-97
	0x00F2, 0x00F4, 0x00F6, 0x00F5, 0x00FA, 0x00F9, 0x00FB, 0x00FC, // 98-9F
	0x2020, 0x00B0, 0x00A2, 0x00A3, 0x00A7, 0x2022, 0x00B6, 0x00DF, // A0-A7
	0x00AE, 0x00A9, 0x2122, 0x00B4, 0x00A8, 0x2260, 0x00C6, 0x00D8, // A8-AF
	0x221E, 0x00B1, 0x2264, 0x2265, 0x00A5, 0x00B5, 0x2202, 0x2211, // B0-B7
	0x220F, 0x03C0, 0x222B, 0x00AA, 0x00BA, 0x03A9, 0x00E6, 0x00F8, // B8-BF
	0x00BF, 0x00A1, 0x00AC, 0x221A, 0x0192, 0x2248, 0x2206, 0x00AB, // C0-C7
	0x00BB, 0x2026, 0x00A0, 0x00C0, 0x00C3, 0x00D5, 0x0152, 0x0153, // C8-CF
	0x2013, 0x2014, 0x201C, 0x201D, 0x2018, 0x2019, 0x00F7, 0x25CA, // D0-D7
	0x00FF, 0x0178, 0x2044, 0x20AC, 0x2039, 0x203A, 0xFB01, 0xFB02, // D8-DF
	0x2021, 0x00B7, 0x201A, 0x201E, 0x2030, 0x00C2, 0x00CA, 0x00C1, // E0-E7
	0x00CB, 0x00C8, 0x00CD, 0x00CE, 0x00CF, 0x00CC, 0x00D3, 0x00D4, // E8-EF
	0xF8FF, 0x00D2, 0x00DA, 0x00DB, 0x00D9, 0x0131, 0x02C6, 0x02DC, // F0-F7
	0x00AF, 0x02D8, 0x02D9, 0x02DA, 0x00B8, 0x02DD, 0x02DB, 0x02C7, // F8-FF
};

/* Encode a Unicode code point as UTF-8, appending to output. */
inline void AppendUTF8(std::string &out, uint32_t cp)
{
	if (cp < 0x80)
	{
		out += static_cast<char>(cp);
	}
	else if (cp < 0x800)
	{
		out += static_cast<char>(0xC0 | (cp >> 6));
		out += static_cast<char>(0x80 | (cp & 0x3F));
	}
	else if (cp < 0x10000)
	{
		out += static_cast<char>(0xE0 | (cp >> 12));
		out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (cp & 0x3F));
	}
	else if (cp < 0x110000)
	{
		out += static_cast<char>(0xF0 | (cp >> 18));
		out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
		out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (cp & 0x3F));
	}
}

/* Unicode code point → MacRoman byte.
   Returns {true, byte} on success, {false, 0} if unmappable. */
struct MacRomanResult
{
	bool valid;
	uint8_t byte;
};

inline MacRomanResult MacRomanFromCodePoint(uint32_t cp)
{
	if (cp < 128) return {true, static_cast<uint8_t>(cp)};
	for (int i = 0; i < 128; ++i)
		if (kMacRomanToUnicode[i] == cp) return {true, static_cast<uint8_t>(0x80 + i)};
	return {false, 0};
}

/* Decode one UTF-8 code point from data starting at pos.
   Advances pos past the consumed bytes.  Returns U+FFFD for invalid sequences. */
inline uint32_t DecodeUTF8(const std::string &data, size_t &pos)
{
	uint8_t c = static_cast<uint8_t>(data[pos]);
	if (c < 0x80)
	{
		++pos;
		return c;
	}

	uint32_t cp;
	int trailing;

	if ((c & 0xE0) == 0xC0)
	{
		cp = c & 0x1F;
		trailing = 1;
	}
	else if ((c & 0xF0) == 0xE0)
	{
		cp = c & 0x0F;
		trailing = 2;
	}
	else if ((c & 0xF8) == 0xF0)
	{
		cp = c & 0x07;
		trailing = 3;
	}
	else
	{
		++pos;
		return 0xFFFD;
	}

	++pos;
	for (int i = 0; i < trailing; ++i)
	{
		if (pos >= data.size()) return 0xFFFD;
		uint8_t b = static_cast<uint8_t>(data[pos]);
		if ((b & 0xC0) != 0x80) return 0xFFFD;
		cp = (cp << 6) | (b & 0x3F);
		++pos;
	}

	return cp;
}

} // namespace appledouble
