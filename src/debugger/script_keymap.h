// Script keymap — map characters and key specs to Mac virtual keycodes.
#pragma once

#include <cstdint>
#include <string_view>
#include <utility>

// Map a MacRoman byte to (Mac virtual keycode, needShift).
// Returns {0xFF, false} for unmappable characters.
std::pair<uint8_t, bool> CharToMacKey(uint8_t macRomanChar);

// Parse a modifier+key spec like "cmd-shift-S" or "return".
// Returns (keycode, modifierMask). modifierMask bits:
//   bit 0 = cmd, bit 1 = shift, bit 2 = option, bit 3 = ctrl
struct KeySpec
{
	uint8_t keycode;
	uint8_t modifiers;
};
KeySpec ParseKeySpec(std::string_view spec);

// Modifier mask bits
inline constexpr uint8_t kModCmd = 0x01;
inline constexpr uint8_t kModShift = 0x02;
inline constexpr uint8_t kModOption = 0x04;
inline constexpr uint8_t kModCtrl = 0x08;
