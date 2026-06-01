/*
	bp_screen.h — Screen breakpoint support

	Provides ScreenMatcher for matching live framebuffer against a
	reference PNG, and screen breakpoint checks.
*/
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

/*
	Loads a reference PNG and compares against the live framebuffer.
	Threshold defaults to 99.85% — pixels must match in RGB to trigger.
*/
struct ScreenMatcher
{
	std::vector<uint32_t> refPixels; // ARGB8888 from reference PNG
	int refWidth = 0;
	int refHeight = 0;
	float threshold = 99.85f; // percent match required

	// Load a reference PNG, convert to ARGB8888.
	bool loadReference(const std::filesystem::path &png);

	// Return true if framebuffer matches reference within threshold.
	bool matches(const uint8_t *framebuffer, int width, int height) const;
};

// Called once per tick (60 Hz). Checks all active screen breakpoints.
void CheckScreenBreakpoints();

// Save current framebuffer to PNG. Returns false on error.
bool SaveScreenshot(const std::filesystem::path &path);
