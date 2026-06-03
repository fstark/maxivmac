#include "resource_fork.h"
#include "storage/appledouble_internal.h"

using appledouble::detail::ReadBE16;
using appledouble::detail::ReadBE32;

namespace rsrc {

// Parse a Macintosh resource fork into a list of resources
// Returns empty vector on parse failure
std::vector<Resource> ParseResourceFork(std::span<const uint8_t> fork)
{
    std::vector<Resource> result;
    if (fork.size() < 16) return result;

    // Resource fork header: [dataOff][mapOff][dataLen][mapLen]
    uint32_t dataOff = ReadBE32(fork.data());
    uint32_t mapOff = ReadBE32(fork.data() + 4);

    // Resource map must fit within the fork
    if (mapOff + 28 > fork.size()) return result;

    const uint8_t *map = fork.data() + mapOff;
    
    // Resource map header offsets (bytes 24-27 of map header)
    uint16_t typeListOff = ReadBE16(map + 24);
    uint16_t nameListOff = ReadBE16(map + 26);
    (void)nameListOff; // reserved for future name extraction

    // Type list must be within bounds
    const uint8_t *typeList = map + typeListOff;
    if (typeList + 2 > fork.data() + fork.size()) return result;

    // Number of resource types (stored as count-1)
    uint16_t numTypes = ReadBE16(typeList) + 1;

    // Iterate through each resource type
    for (uint16_t i = 0; i < numTypes; ++i) {
        const uint8_t *entry = typeList + 2 + i * 8;
        if (entry + 8 > fork.data() + fork.size()) break;

        // Type list entry: [type][count-1][refOff]
        uint32_t type = ReadBE32(entry);
        uint16_t numRes = ReadBE16(entry + 4) + 1;
        uint16_t refOff = ReadBE16(entry + 6);

        const uint8_t *refList = typeList + refOff;

        // Iterate through each resource of this type
        for (uint16_t j = 0; j < numRes; ++j) {
            const uint8_t *ref = refList + j * 12;
            if (ref + 12 > fork.data() + fork.size()) break;

            // Resource reference: [id-16][dataOff3-byte][attr]
            int16_t id = static_cast<int16_t>(ReadBE16(ref));
            
            // 3-byte data offset (big-endian)
            uint32_t dataOff3 = (static_cast<uint32_t>(ref[5]) << 16) |
                                (static_cast<uint32_t>(ref[6]) << 8) |
                                static_cast<uint32_t>(ref[7]);

            // Calculate absolute data offset within data section
            size_t absDataOff = dataOff + dataOff3;
            if (absDataOff + 4 > fork.size()) continue;

            // Resource data header: 4-byte length
            uint32_t len = ReadBE32(fork.data() + absDataOff);
            if (absDataOff + 4 + len > fork.size()) continue;

            // Extract resource data (skip the 4-byte length header)
            Resource r;
            r.type = type;
            r.id = id;
            r.data.assign(fork.data() + absDataOff + 4,
                          fork.data() + absDataOff + 4 + len);
            result.push_back(std::move(r));
        }
    }
    return result;
}

// Find all resources of a given type
std::vector<const Resource *> FindByType(
    const std::vector<Resource> &resources, uint32_t type)
{
    std::vector<const Resource *> out;
    for (const auto &r : resources)
        if (r.type == type) out.push_back(&r);
    return out;
}

// Find a specific resource by type and ID
const Resource *FindByTypeAndId(
    const std::vector<Resource> &resources, uint32_t type, int16_t id)
{
    for (const auto &r : resources)
        if (r.type == type && r.id == id) return &r;
    return nullptr;
}

} // namespace rsrc
