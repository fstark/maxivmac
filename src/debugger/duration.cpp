/*
	duration.cpp — Human-readable duration parser
*/

#include "debugger/duration.h"

#include <cctype>
#include <cstdlib>

bool ParseDuration(std::string_view text, uint64_t clockHz,
				   uint64_t &outCycles)
{
	if (text.empty()) return false;

	// "off" / "none" → 0 (infinite)
	if (text == "off" || text == "none")
	{
		outCycles = 0;
		return true;
	}

	// Try to parse numeric prefix
	const char *start = text.data();
	const char *end = start + text.size();
	char *numEnd = nullptr;
	unsigned long long val = std::strtoull(start, &numEnd, 10);

	if (numEnd == start) return false; // no digits at all

	std::string_view suffix(numEnd, static_cast<size_t>(end - numEnd));

	if (suffix.empty() || suffix == "c")
	{
		// Raw cycle count
		outCycles = val;
		return true;
	}

	if (suffix == "s")
	{
		outCycles = val * clockHz;
		return true;
	}

	if (suffix == "ms")
	{
		outCycles = val * clockHz / 1000;
		return true;
	}

	if (suffix == "M")
	{
		outCycles = val * 1'000'000;
		return true;
	}

	if (suffix == "k" || suffix == "K")
	{
		outCycles = val * 1'000;
		return true;
	}

	return false;
}

std::string FormatDuration(uint64_t cycles, uint64_t clockHz)
{
	if (cycles == 0) return "off";

	// Try seconds
	if (clockHz > 0 && cycles % clockHz == 0)
	{
		uint64_t secs = cycles / clockHz;
		return std::to_string(secs) + "s";
	}

	// Try milliseconds
	if (clockHz > 0 && (cycles * 1000) % clockHz == 0)
	{
		uint64_t ms = (cycles * 1000) / clockHz;
		return std::to_string(ms) + "ms";
	}

	// Try M suffix
	if (cycles % 1'000'000 == 0)
	{
		return std::to_string(cycles / 1'000'000) + "M";
	}

	// Try k suffix
	if (cycles % 1'000 == 0)
	{
		return std::to_string(cycles / 1'000) + "k";
	}

	// Raw cycle count
	return std::to_string(cycles);
}
