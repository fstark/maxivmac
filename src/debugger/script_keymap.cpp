// Script keymap — map characters and key specs to Mac virtual keycodes.
#include "debugger/script_keymap.h"
#include "platform/keycodes.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <vector>

// US keyboard layout: ASCII char → (keycode, needShift)
// Indexed by ASCII value 0x20..0x7E (95 entries)
struct CharEntry
{
	uint8_t keycode;
	bool shift;
};

static constexpr CharEntry kCharMap[95] = {
	{MKC_Space, false},		   // 0x20 ' '
	{MKC_1, true},			   // 0x21 '!'
	{MKC_SingleQuote, true},   // 0x22 '"'
	{MKC_3, true},			   // 0x23 '#'
	{MKC_4, true},			   // 0x24 '$'
	{MKC_5, true},			   // 0x25 '%'
	{MKC_7, true},			   // 0x26 '&'
	{MKC_SingleQuote, false},  // 0x27 '\''
	{MKC_9, true},			   // 0x28 '('
	{MKC_0, true},			   // 0x29 ')'
	{MKC_8, true},			   // 0x2A '*'
	{MKC_Equal, true},		   // 0x2B '+'
	{MKC_Comma, false},		   // 0x2C ','
	{MKC_Minus, false},		   // 0x2D '-'
	{MKC_Period, false},	   // 0x2E '.'
	{MKC_Slash, false},		   // 0x2F '/'
	{MKC_0, false},			   // 0x30 '0'
	{MKC_1, false},			   // 0x31 '1'
	{MKC_2, false},			   // 0x32 '2'
	{MKC_3, false},			   // 0x33 '3'
	{MKC_4, false},			   // 0x34 '4'
	{MKC_5, false},			   // 0x35 '5'
	{MKC_6, false},			   // 0x36 '6'
	{MKC_7, false},			   // 0x37 '7'
	{MKC_8, false},			   // 0x38 '8'
	{MKC_9, false},			   // 0x39 '9'
	{MKC_SemiColon, true},	   // 0x3A ':'
	{MKC_SemiColon, false},	   // 0x3B ';'
	{MKC_Comma, true},		   // 0x3C '<'
	{MKC_Equal, false},		   // 0x3D '='
	{MKC_Period, true},		   // 0x3E '>'
	{MKC_Slash, true},		   // 0x3F '?'
	{MKC_2, true},			   // 0x40 '@'
	{MKC_A, true},			   // 0x41 'A'
	{MKC_B, true},			   // 0x42 'B'
	{MKC_C, true},			   // 0x43 'C'
	{MKC_D, true},			   // 0x44 'D'
	{MKC_E, true},			   // 0x45 'E'
	{MKC_F, true},			   // 0x46 'F'
	{MKC_G, true},			   // 0x47 'G'
	{MKC_H, true},			   // 0x48 'H'
	{MKC_I, true},			   // 0x49 'I'
	{MKC_J, true},			   // 0x4A 'J'
	{MKC_K, true},			   // 0x4B 'K'
	{MKC_L, true},			   // 0x4C 'L'
	{MKC_M, true},			   // 0x4D 'M'
	{MKC_N, true},			   // 0x4E 'N'
	{MKC_O, true},			   // 0x4F 'O'
	{MKC_P, true},			   // 0x50 'P'
	{MKC_Q, true},			   // 0x51 'Q'
	{MKC_R, true},			   // 0x52 'R'
	{MKC_S, true},			   // 0x53 'S'
	{MKC_T, true},			   // 0x54 'T'
	{MKC_U, true},			   // 0x55 'U'
	{MKC_V, true},			   // 0x56 'V'
	{MKC_W, true},			   // 0x57 'W'
	{MKC_X, true},			   // 0x58 'X'
	{MKC_Y, true},			   // 0x59 'Y'
	{MKC_Z, true},			   // 0x5A 'Z'
	{MKC_LeftBracket, false},  // 0x5B '['
	{MKC_BackSlash, false},	   // 0x5C '\\'
	{MKC_RightBracket, false}, // 0x5D ']'
	{MKC_6, true},			   // 0x5E '^'
	{MKC_Minus, true},		   // 0x5F '_'
	{MKC_Grave, false},		   // 0x60 '`'
	{MKC_A, false},			   // 0x61 'a'
	{MKC_B, false},			   // 0x62 'b'
	{MKC_C, false},			   // 0x63 'c'
	{MKC_D, false},			   // 0x64 'd'
	{MKC_E, false},			   // 0x65 'e'
	{MKC_F, false},			   // 0x66 'f'
	{MKC_G, false},			   // 0x67 'g'
	{MKC_H, false},			   // 0x68 'h'
	{MKC_I, false},			   // 0x69 'i'
	{MKC_J, false},			   // 0x6A 'j'
	{MKC_K, false},			   // 0x6B 'k'
	{MKC_L, false},			   // 0x6C 'l'
	{MKC_M, false},			   // 0x6D 'm'
	{MKC_N, false},			   // 0x6E 'n'
	{MKC_O, false},			   // 0x6F 'o'
	{MKC_P, false},			   // 0x70 'p'
	{MKC_Q, false},			   // 0x71 'q'
	{MKC_R, false},			   // 0x72 'r'
	{MKC_S, false},			   // 0x73 's'
	{MKC_T, false},			   // 0x74 't'
	{MKC_U, false},			   // 0x75 'u'
	{MKC_V, false},			   // 0x76 'v'
	{MKC_W, false},			   // 0x77 'w'
	{MKC_X, false},			   // 0x78 'x'
	{MKC_Y, false},			   // 0x79 'y'
	{MKC_Z, false},			   // 0x7A 'z'
	{MKC_LeftBracket, true},   // 0x7B '{'
	{MKC_BackSlash, true},	   // 0x7C '|'
	{MKC_RightBracket, true},  // 0x7D '}'
	{MKC_Grave, true},		   // 0x7E '~'
};

std::pair<uint8_t, bool> CharToMacKey(uint8_t macRomanChar)
{
	if (macRomanChar < 0x20 || macRomanChar > 0x7E) return {0xFF, false};
	auto &entry = kCharMap[macRomanChar - 0x20];
	return {entry.keycode, entry.shift};
}

// Named key lookup
struct NamedKey
{
	const char *name;
	uint8_t keycode;
};

static constexpr NamedKey kNamedKeys[] = {
	{"return", MKC_Return},
	{"enter", MKC_Enter},
	{"tab", MKC_Tab},
	{"escape", MKC_Escape},
	{"esc", MKC_Escape},
	{"delete", MKC_BackSpace},
	{"backspace", MKC_BackSpace},
	{"space", MKC_Space},
	{"left", MKC_Left},
	{"right", MKC_Right},
	{"up", MKC_Up},
	{"down", MKC_Down},
	{"home", MKC_Home},
	{"end", MKC_End},
	{"pageup", MKC_PageUp},
	{"pagedown", MKC_PageDown},
	{"fwddel", MKC_ForwardDel},
	{"f1", MKC_F1},
	{"f2", MKC_F2},
	{"f3", MKC_F3},
	{"f4", MKC_F4},
	{"f5", MKC_F5},
	{"f6", MKC_F6},
	{"f7", MKC_F7},
	{"f8", MKC_F8},
	{"f9", MKC_F9},
	{"f10", MKC_F10},
	{"f11", MKC_F11},
	{"f12", MKC_F12},
};

static uint8_t LookupKeyName(std::string_view name)
{
	std::string lower(name);
	std::transform(lower.begin(), lower.end(), lower.begin(),
				   [](unsigned char c) { return std::tolower(c); });

	for (const auto &nk : kNamedKeys)
	{
		if (lower == nk.name) return nk.keycode;
	}

	// Single character → use CharToMacKey
	if (name.size() == 1)
	{
		auto [kc, _] = CharToMacKey(static_cast<uint8_t>(name[0]));
		return kc;
	}

	return 0xFF;
}

KeySpec ParseKeySpec(std::string_view spec)
{
	KeySpec result{0xFF, 0};

	// Split on '-'
	std::string s(spec);
	std::vector<std::string> parts;
	size_t start = 0;
	for (size_t i = 0; i <= s.size(); ++i)
	{
		if (i == s.size() || s[i] == '-')
		{
			if (i > start) parts.emplace_back(s.substr(start, i - start));
			start = i + 1;
		}
	}

	if (parts.empty()) return result;

	// Last part is the key, preceding parts are modifiers
	for (size_t i = 0; i < parts.size() - 1; ++i)
	{
		std::string mod = parts[i];
		std::transform(mod.begin(), mod.end(), mod.begin(),
					   [](unsigned char c) { return std::tolower(c); });
		if (mod == "cmd" || mod == "command")
			result.modifiers |= kModCmd;
		else if (mod == "shift")
			result.modifiers |= kModShift;
		else if (mod == "opt" || mod == "option")
			result.modifiers |= kModOption;
		else if (mod == "ctrl" || mod == "control")
			result.modifiers |= kModCtrl;
	}

	result.keycode = LookupKeyName(parts.back());
	return result;
}
