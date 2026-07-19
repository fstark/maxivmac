/*
	duration.h — Human-readable duration parser for debugger

	Converts duration strings to cycle counts and back.
*/
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

// Parse a human-readable duration into a cycle count.
// Accepts:
//   "5s", "500ms"    — wall-time, converted via emulated clock speed
//   "5M", "100k"     — SI multipliers on cycle counts
//   "40000000"       — raw cycle count
//   "off", "none", "0" — returns 0 (meaning infinite/disabled)
// Returns true on success.
bool ParseDuration(std::string_view text, uint64_t clockHz,
				   uint64_t &outCycles);

// Format a cycle count as a human-readable string.
// Uses the most natural unit (e.g., "5s" if evenly divisible).
std::string FormatDuration(uint64_t cycles, uint64_t clockHz);
