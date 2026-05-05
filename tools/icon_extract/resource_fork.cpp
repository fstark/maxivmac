#include "resource_fork.h"
#include "storage/appledouble_internal.h"

using appledouble::detail::ReadBE16;
using appledouble::detail::ReadBE32;

namespace rsrc {

std::vector<Resource> ParseResourceFork(std::span<const uint8_t> fork)
{
    std::vector<Resource> result;
    if (fork.size() < 16) return result;

    uint32_t dataOff = ReadBE32(fork.data());
    uint32_t mapOff = ReadBE32(fork.data() + 4);

    if (mapOff + 28 > fork.size()) return result;

    const uint8_t *map = fork.data() + mapOff;
    uint16_t typeListOff = ReadBE16(map + 24);
    uint16_t nameListOff = ReadBE16(map + 26);
    (void)nameListOff; // reserved for future name extraction

    const uint8_t *typeList = map + typeListOff;
    if (typeList + 2 > fork.data() + fork.size()) return result;

    uint16_t numTypes = ReadBE16(typeList) + 1;

    for (uint16_t i = 0; i < numTypes; ++i) {
        const uint8_t *entry = typeList + 2 + i * 8;
        if (entry + 8 > fork.data() + fork.size()) break;

        uint32_t type = ReadBE32(entry);
        uint16_t numRes = ReadBE16(entry + 4) + 1;
        uint16_t refOff = ReadBE16(entry + 6);

        const uint8_t *refList = typeList + refOff;

        for (uint16_t j = 0; j < numRes; ++j) {
            const uint8_t *ref = refList + j * 12;
            if (ref + 12 > fork.data() + fork.size()) break;

            int16_t id = static_cast<int16_t>(ReadBE16(ref));
            uint32_t dataOff3 = (static_cast<uint32_t>(ref[5]) << 16) |
                                (static_cast<uint32_t>(ref[6]) << 8) |
                                static_cast<uint32_t>(ref[7]);

            size_t absDataOff = dataOff + dataOff3;
            if (absDataOff + 4 > fork.size()) continue;

            uint32_t len = ReadBE32(fork.data() + absDataOff);
            if (absDataOff + 4 + len > fork.size()) continue;

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

std::vector<const Resource *> FindByType(
    const std::vector<Resource> &resources, uint32_t type)
{
    std::vector<const Resource *> out;
    for (auto &r : resources)
        if (r.type == type) out.push_back(&r);
    return out;
}

const Resource *FindByTypeAndId(
    const std::vector<Resource> &resources, uint32_t type, int16_t id)
{
    for (auto &r : resources)
        if (r.type == type && r.id == id) return &r;
    return nullptr;
}

} // namespace rsrc
