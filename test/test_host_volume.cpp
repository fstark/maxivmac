#include <doctest/doctest.h>
#include "storage/host_volume.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("HostVolume: default state")
{
	storage::HostVolume vol;
	CHECK_FALSE(vol.isMounted());
	CHECK(vol.findByCNID(2) == nullptr);
	CHECK(vol.childCount(2) == 0);
}
