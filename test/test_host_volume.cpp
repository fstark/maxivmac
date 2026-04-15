#include <doctest/doctest.h>
#include "storage/host_volume.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace
{

fs::path makeTempDir()
{
	auto p = fs::temp_directory_path() / "test_hv";
	fs::remove_all(p);
	fs::create_directories(p);
	return p;
}

void writeFile(const fs::path &p, std::string_view content)
{
	std::ofstream f(p, std::ios::binary);
	f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

struct TempDir
{
	fs::path path;
	TempDir() : path(makeTempDir()) {}
	~TempDir() { fs::remove_all(path); }
};

} // namespace

TEST_CASE("HostVolume: default state")
{
	storage::HostVolume vol;
	CHECK_FALSE(vol.isMounted());
	CHECK(vol.findByCNID(2) == nullptr);
	CHECK(vol.childCount(2) == 0);
}

TEST_CASE("HostVolume: mount empty directory")
{
	TempDir td;
	storage::HostVolume vol;
	CHECK(vol.mount(td.path));
	CHECK(vol.isMounted());
	CHECK(vol.childCount(storage::HostVolume::kRootDirID) == 0);
}

TEST_CASE("HostVolume: mount with files")
{
	TempDir td;
	writeFile(td.path / "hello.txt", "hello");
	writeFile(td.path / "image.png", "\x89PNG");

	storage::HostVolume vol;
	CHECK(vol.mount(td.path));
	CHECK(vol.childCount(storage::HostVolume::kRootDirID) == 2);

	auto *txt = vol.findByName(storage::HostVolume::kRootDirID, "hello.txt");
	CHECK(txt != nullptr);
	CHECK(txt->macName == "hello.txt");
	CHECK_FALSE(txt->isDirectory);

	auto *png = vol.findByName(storage::HostVolume::kRootDirID, "image.png");
	CHECK(png != nullptr);
}

TEST_CASE("HostVolume: mount with subdirectory")
{
	TempDir td;
	fs::create_directory(td.path / "subdir");
	writeFile(td.path / "subdir" / "inner.txt", "data");

	storage::HostVolume vol;
	CHECK(vol.mount(td.path));

	auto *sub = vol.findByName(storage::HostVolume::kRootDirID, "subdir");
	REQUIRE(sub != nullptr);
	CHECK(sub->isDirectory);
	CHECK(vol.childCount(sub->cnid) == 1);

	auto *inner = vol.nthChild(sub->cnid, 1);
	REQUIRE(inner != nullptr);
	CHECK(inner->macName == "inner.txt");
}

TEST_CASE("HostVolume: sidecar files hidden")
{
	TempDir td;
	writeFile(td.path / "test.txt", "data");
	writeFile(td.path / "._test.txt", "sidecar");

	storage::HostVolume vol;
	CHECK(vol.mount(td.path));
	CHECK(vol.childCount(storage::HostVolume::kRootDirID) == 1);
	CHECK(vol.findByName(storage::HostVolume::kRootDirID, "test.txt") != nullptr);
}

TEST_CASE("HostVolume: hidden files skipped")
{
	TempDir td;
	writeFile(td.path / ".hidden", "secret");

	storage::HostVolume vol;
	CHECK(vol.mount(td.path));
	CHECK(vol.childCount(storage::HostVolume::kRootDirID) == 0);
}

TEST_CASE("HostVolume: filename decoding")
{
	TempDir td;
	writeFile(td.path / "^3Acolon", "data");

	storage::HostVolume vol;
	CHECK(vol.mount(td.path));
	auto *e = vol.findByName(storage::HostVolume::kRootDirID, ":colon");
	REQUIRE(e != nullptr);
	CHECK(e->macName == ":colon");
}

TEST_CASE("HostVolume: findByName case-insensitive")
{
	TempDir td;
	writeFile(td.path / "Hello.txt", "data");

	storage::HostVolume vol;
	CHECK(vol.mount(td.path));
	CHECK(vol.findByName(storage::HostVolume::kRootDirID, "hello.txt") != nullptr);
	CHECK(vol.findByName(storage::HostVolume::kRootDirID, "HELLO.TXT") != nullptr);
}

TEST_CASE("HostVolume: findByCNID not found")
{
	TempDir td;
	storage::HostVolume vol;
	CHECK(vol.mount(td.path));
	CHECK(vol.findByCNID(9999) == nullptr);
}

TEST_CASE("HostVolume: nthChild out of range")
{
	TempDir td;
	writeFile(td.path / "only.txt", "x");

	storage::HostVolume vol;
	CHECK(vol.mount(td.path));
	CHECK(vol.nthChild(storage::HostVolume::kRootDirID, 2) == nullptr);
}

TEST_CASE("HostVolume: metadata from extension")
{
	TempDir td;
	writeFile(td.path / "readme.txt", "hello");

	storage::HostVolume vol;
	CHECK(vol.mount(td.path));
	auto *e = vol.findByName(storage::HostVolume::kRootDirID, "readme.txt");
	REQUIRE(e != nullptr);
	CHECK(e->type == appledouble::FourCC("TEXT"));
	CHECK(e->creator == appledouble::FourCC("ttxt"));
}

TEST_CASE("HostVolume: metadata from sidecar")
{
	TempDir td;
	writeFile(td.path / "app", "data");
	appledouble::SetFinderInfo(td.path / "app",
							   {appledouble::FourCC("APPL"), appledouble::FourCC("test"), 0});

	storage::HostVolume vol;
	CHECK(vol.mount(td.path));
	auto *e = vol.findByName(storage::HostVolume::kRootDirID, "app");
	REQUIRE(e != nullptr);
	CHECK(e->type == appledouble::FourCC("APPL"));
	CHECK(e->creator == appledouble::FourCC("test"));
}

TEST_CASE("HostVolume: mount nonexistent directory")
{
	storage::HostVolume vol;
	CHECK_FALSE(vol.mount("/tmp/nonexistent_hv_test_dir_xyz"));
	CHECK_FALSE(vol.isMounted());
}
