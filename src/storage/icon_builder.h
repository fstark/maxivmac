#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace storage
{

/*
	Build a complete Mac resource fork containing custom icon resources
	suitable for an Icon\r file.  Resources are at ID -16455 (the magic
	ID the Finder uses for custom volume/folder icons).

	bwPath:    32×32 RGBA PNG — black-and-white icon.  Alpha channel
			   is used as the mask.  Pixels with luminance < 128 become
			   black; the rest become white.
	colorPath: 32×32 RGBA PNG — color icon, quantized to the Apple
			   8-bit system palette for icl8/ics8.

	Returns an empty vector on failure (file not found, wrong size, etc.).

	Resources produced:
	  ICN# — 32×32 B&W icon + mask (256 bytes)
	  icl8 — 32×32 8-bit color     (1024 bytes)
	  ics# — 16×16 B&W icon + mask (64 bytes)
	  ics8 — 16×16 8-bit color     (256 bytes)
*/
std::vector<uint8_t> BuildIconResourceFork(const std::filesystem::path &bwPath,
										   const std::filesystem::path &colorPath);

} // namespace storage
