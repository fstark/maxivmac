// Screen breakpoint matching — fires when framebuffer matches a reference PNG.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// Loads a reference PNG and compares against the live framebuffer.
struct ScreenMatcher
{
	std::vector<uint32_t> refPixels; // ARGB8888 from reference PNG
	int refWidth = 0;
	int refHeight = 0;
	float threshold = 99.85f; // percent match required

	bool loadReference(const std::filesystem::path &png);
	bool matches(const uint8_t *framebuffer, int width, int height) const;
};

// Called once per tick (60 Hz). Checks all active screen breakpoints.
void CheckScreenBreakpoints();

// Save the current framebuffer to a PNG file.
bool SaveScreenshot(const std::filesystem::path &path);

// Called when the guest requests power-off. Fires Kind::PowerOff breakpoints.
void CheckPowerOffBreakpoints();
