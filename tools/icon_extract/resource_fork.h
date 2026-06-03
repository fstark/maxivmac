/*
	Resource fork parsing — extracts Macintosh resources from AppleDouble format

	Provides functions to parse resource forks stored in AppleDouble format,
	retrieving resources by type and ID for icon and other resource extraction.
*/

#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rsrc {

// A Macintosh resource identified by type and ID
struct Resource {
    uint32_t type;          // 4-character resource type code (e.g., 'ICN#', 'ICON')
    int16_t id;             // Resource ID (typically 0 or positive)
    std::string name;       // Resource name (currently always empty)
    std::vector<uint8_t> data;  // Raw resource data
};

// Parse a Macintosh resource fork from raw data
// Returns empty vector on parse failure
std::vector<Resource> ParseResourceFork(std::span<const uint8_t> fork);

// Find all resources of a given type
std::vector<const Resource *> FindByType(
    const std::vector<Resource> &resources, uint32_t type);

// Find a specific resource by type and ID
const Resource *FindByTypeAndId(
    const std::vector<Resource> &resources, uint32_t type, int16_t id);

} // namespace rsrc
