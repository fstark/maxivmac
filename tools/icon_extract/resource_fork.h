#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rsrc {

struct Resource {
    uint32_t type;
    int16_t id;
    std::string name;
    std::vector<uint8_t> data;
};

std::vector<Resource> ParseResourceFork(std::span<const uint8_t> fork);

std::vector<const Resource *> FindByType(
    const std::vector<Resource> &resources, uint32_t type);

const Resource *FindByTypeAndId(
    const std::vector<Resource> &resources, uint32_t type, int16_t id);

} // namespace rsrc
