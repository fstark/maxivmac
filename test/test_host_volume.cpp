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

/* ── Phase 3: create / delete ─────────────────────── */

TEST_CASE("HostVolume: createFile basic")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	uint32_t cnid = vol.createFile(storage::HostVolume::kRootDirID, "newfile.txt", err);
	CHECK(err == storage::FMErr::kNoErr);
	CHECK(cnid > 0);
	CHECK(fs::exists(td.path / "newfile.txt"));
	CHECK(vol.findByName(storage::HostVolume::kRootDirID, "newfile.txt") != nullptr);
}

TEST_CASE("HostVolume: createFile duplicate")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	vol.createFile(storage::HostVolume::kRootDirID, "dup.txt", err);
	CHECK(err == storage::FMErr::kNoErr);

	uint32_t cnid2 = vol.createFile(storage::HostVolume::kRootDirID, "dup.txt", err);
	CHECK(err == storage::FMErr::kDupFNErr);
	CHECK(cnid2 == 0);
}

TEST_CASE("HostVolume: createFile escaped name")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	vol.createFile(storage::HostVolume::kRootDirID, ":special", err);
	CHECK(err == storage::FMErr::kNoErr);
	CHECK(fs::exists(td.path / "^3Aspecial"));
}

TEST_CASE("HostVolume: createFile bad parent")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	uint32_t cnid = vol.createFile(9999, "file.txt", err);
	CHECK(err == storage::FMErr::kDirNFErr);
	CHECK(cnid == 0);
}

TEST_CASE("HostVolume: createDir basic")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	uint32_t cnid = vol.createDir(storage::HostVolume::kRootDirID, "newdir", err);
	CHECK(err == storage::FMErr::kNoErr);
	CHECK(cnid > 0);
	CHECK(fs::is_directory(td.path / "newdir"));

	auto *e = vol.findByCNID(cnid);
	REQUIRE(e != nullptr);
	CHECK(e->isDirectory);
}

TEST_CASE("HostVolume: remove file")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	vol.createFile(storage::HostVolume::kRootDirID, "gone.txt", err);
	CHECK(err == storage::FMErr::kNoErr);

	auto result = vol.remove(storage::HostVolume::kRootDirID, "gone.txt");
	CHECK(result == storage::FMErr::kNoErr);
	CHECK_FALSE(fs::exists(td.path / "gone.txt"));
	CHECK(vol.findByName(storage::HostVolume::kRootDirID, "gone.txt") == nullptr);
}

TEST_CASE("HostVolume: remove file with sidecar")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	vol.createFile(storage::HostVolume::kRootDirID, "withsc", err);
	appledouble::SetFinderInfo(td.path / "withsc",
							   {appledouble::FourCC("APPL"), appledouble::FourCC("test"), 0});
	CHECK(fs::exists(td.path / "._withsc"));

	auto result = vol.remove(storage::HostVolume::kRootDirID, "withsc");
	CHECK(result == storage::FMErr::kNoErr);
	CHECK_FALSE(fs::exists(td.path / "withsc"));
	CHECK_FALSE(fs::exists(td.path / "._withsc"));
}

TEST_CASE("HostVolume: remove non-empty directory")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	uint32_t dirCnid = vol.createDir(storage::HostVolume::kRootDirID, "notempty", err);
	vol.createFile(dirCnid, "child.txt", err);

	auto result = vol.remove(storage::HostVolume::kRootDirID, "notempty");
	CHECK(result == storage::FMErr::kFBsyErr);
}

TEST_CASE("HostVolume: remove empty directory")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	vol.createDir(storage::HostVolume::kRootDirID, "emptydir", err);

	auto result = vol.remove(storage::HostVolume::kRootDirID, "emptydir");
	CHECK(result == storage::FMErr::kNoErr);
	CHECK_FALSE(fs::exists(td.path / "emptydir"));
}

TEST_CASE("HostVolume: remove non-existent")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	auto result = vol.remove(storage::HostVolume::kRootDirID, "nope");
	CHECK(result == storage::FMErr::kFnfErr);
}

/* ── Phase 4: move/rename + metadata ──────────────── */

TEST_CASE("HostVolume: move file between dirs")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	uint32_t dirA = vol.createDir(storage::HostVolume::kRootDirID, "dirA", err);
	uint32_t dirB = vol.createDir(storage::HostVolume::kRootDirID, "dirB", err);
	vol.createFile(dirA, "moveme.txt", err);

	auto result = vol.move(dirA, "moveme.txt", dirB);
	CHECK(result == storage::FMErr::kNoErr);

	CHECK(vol.findByName(dirA, "moveme.txt") == nullptr);
	auto *e = vol.findByName(dirB, "moveme.txt");
	REQUIRE(e != nullptr);
	CHECK(e->parentDirID == dirB);
	CHECK(fs::exists(td.path / "dirB" / "moveme.txt"));
	CHECK_FALSE(fs::exists(td.path / "dirA" / "moveme.txt"));
}

TEST_CASE("HostVolume: move file with sidecar")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	uint32_t dirA = vol.createDir(storage::HostVolume::kRootDirID, "srcdir", err);
	uint32_t dirB = vol.createDir(storage::HostVolume::kRootDirID, "dstdir", err);
	vol.createFile(dirA, "withsc", err);
	appledouble::SetFinderInfo(td.path / "srcdir" / "withsc",
							   {appledouble::FourCC("APPL"), appledouble::FourCC("test"), 0});

	auto result = vol.move(dirA, "withsc", dirB);
	CHECK(result == storage::FMErr::kNoErr);
	CHECK(fs::exists(td.path / "dstdir" / "withsc"));
	CHECK(fs::exists(td.path / "dstdir" / "._withsc"));
	CHECK_FALSE(fs::exists(td.path / "srcdir" / "withsc"));
}

TEST_CASE("HostVolume: move directory with children")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	uint32_t sub = vol.createDir(storage::HostVolume::kRootDirID, "sub", err);
	vol.createFile(sub, "child.txt", err);
	uint32_t dest = vol.createDir(storage::HostVolume::kRootDirID, "dest", err);

	auto result = vol.move(storage::HostVolume::kRootDirID, "sub", dest);
	CHECK(result == storage::FMErr::kNoErr);

	auto *child = vol.findByName(sub, "child.txt");
	REQUIRE(child != nullptr);
	CHECK(child->hostPath.find("dest/sub/child.txt") != std::string::npos);
}

TEST_CASE("HostVolume: move non-existent")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	auto result =
		vol.move(storage::HostVolume::kRootDirID, "nope", storage::HostVolume::kRootDirID);
	CHECK(result == storage::FMErr::kFnfErr);
}

TEST_CASE("HostVolume: setFileInfo basic")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	uint32_t cnid = vol.createFile(storage::HostVolume::kRootDirID, "meta", err);

	auto result = vol.setFileInfo(cnid, appledouble::FourCC("APPL"), appledouble::FourCC("test"));
	CHECK(result == storage::FMErr::kNoErr);

	auto *e = vol.findByCNID(cnid);
	REQUIRE(e != nullptr);
	CHECK(e->type == appledouble::FourCC("APPL"));
	CHECK(e->creator == appledouble::FourCC("test"));
	CHECK(fs::exists(td.path / "._meta"));
}

TEST_CASE("HostVolume: setFileInfo updates isText")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	uint32_t cnid = vol.createFile(storage::HostVolume::kRootDirID, "generic", err);

	auto *e = vol.findByCNID(cnid);
	REQUIRE(e != nullptr);
	CHECK_FALSE(e->isText);

	vol.setFileInfo(cnid, appledouble::FourCC("TEXT"), appledouble::FourCC("ttxt"));
	e = vol.findByCNID(cnid);
	CHECK(e->isText);
}

/* ── Phase 5: data fork I/O ───────────────────────── */

TEST_CASE("HostVolume: open/close data fork")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	uint32_t cnid = vol.createFile(storage::HostVolume::kRootDirID, "data.bin", err);
	uint32_t size = 0;
	uint32_t handle = vol.openFork(cnid, storage::ForkType::Data, size, err);
	CHECK(err == storage::FMErr::kNoErr);
	CHECK(handle > 0);
	CHECK(size == 0);
	vol.closeFork(handle);
}

TEST_CASE("HostVolume: write then read data fork")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	uint32_t cnid = vol.createFile(storage::HostVolume::kRootDirID, "rw.bin", err);

	uint32_t size = 0;
	uint32_t handle = vol.openFork(cnid, storage::ForkType::Data, size, err);

	std::vector<uint8_t> writeData = {'h', 'e', 'l', 'l', 'o'};
	uint32_t written = 0;
	vol.writeFork(handle, 0, writeData, written);
	CHECK(written == 5);
	vol.closeFork(handle);

	handle = vol.openFork(cnid, storage::ForkType::Data, size, err);
	CHECK(size == 5);

	std::vector<uint8_t> readBuf(5);
	uint32_t got = 0;
	vol.readFork(handle, 0, readBuf, got);
	CHECK(got == 5);
	CHECK(readBuf == writeData);
	vol.closeFork(handle);
}

TEST_CASE("HostVolume: read at offset")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	uint32_t cnid = vol.createFile(storage::HostVolume::kRootDirID, "off.bin", err);
	uint32_t size = 0;
	uint32_t handle = vol.openFork(cnid, storage::ForkType::Data, size, err);

	std::vector<uint8_t> writeData = {'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd'};
	uint32_t written = 0;
	vol.writeFork(handle, 0, writeData, written);
	vol.closeFork(handle);

	handle = vol.openFork(cnid, storage::ForkType::Data, size, err);
	std::vector<uint8_t> readBuf(5);
	uint32_t got = 0;
	vol.readFork(handle, 6, readBuf, got);
	CHECK(got == 5);
	CHECK(readBuf == std::vector<uint8_t>{'w', 'o', 'r', 'l', 'd'});
	vol.closeFork(handle);
}

TEST_CASE("HostVolume: write updates catalog size")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	uint32_t cnid = vol.createFile(storage::HostVolume::kRootDirID, "sz.bin", err);
	uint32_t size = 0;
	uint32_t handle = vol.openFork(cnid, storage::ForkType::Data, size, err);

	std::vector<uint8_t> data(100, 0x42);
	uint32_t written = 0;
	vol.writeFork(handle, 0, data, written);
	vol.closeFork(handle);

	auto *e = vol.findByCNID(cnid);
	CHECK(e->dataForkSize == 100);
}

TEST_CASE("HostVolume: open non-existent CNID")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	uint32_t size = 0;
	uint32_t handle = vol.openFork(9999, storage::ForkType::Data, size, err);
	CHECK(err == storage::FMErr::kFnfErr);
	CHECK(handle == 0);
}

TEST_CASE("HostVolume: read with bad handle")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	std::vector<uint8_t> buf(10);
	uint32_t got = 0;
	auto err = vol.readFork(9999, 0, buf, got);
	CHECK(err == storage::FMErr::kRfNumErr);
	CHECK(got == 0);
}

TEST_CASE("HostVolume: write updates modDate")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	uint32_t cnid = vol.createFile(storage::HostVolume::kRootDirID, "dated.bin", err);
	uint32_t size = 0;
	uint32_t handle = vol.openFork(cnid, storage::ForkType::Data, size, err);

	auto *before = vol.findByCNID(cnid);
	uint32_t origMod = before->modDate;

	std::vector<uint8_t> data = {1, 2, 3};
	uint32_t written = 0;
	vol.writeFork(handle, 0, data, written);
	vol.closeFork(handle);

	auto *after = vol.findByCNID(cnid);
	CHECK(after->modDate >= origMod);
}

/* ── Phase 6: resource fork I/O ───────────────────── */

TEST_CASE("HostVolume: open resource fork (no sidecar)")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	uint32_t cnid = vol.createFile(storage::HostVolume::kRootDirID, "norsrc.bin", err);
	uint32_t size = 0;
	uint32_t handle = vol.openFork(cnid, storage::ForkType::Resource, size, err);
	CHECK(err == storage::FMErr::kNoErr);
	CHECK(handle > 0);
	CHECK(size == 0);
	vol.closeFork(handle);
}

TEST_CASE("HostVolume: write then read resource fork")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	uint32_t cnid = vol.createFile(storage::HostVolume::kRootDirID, "rsrc.bin", err);
	uint32_t size = 0;

	uint32_t handle = vol.openFork(cnid, storage::ForkType::Resource, size, err);
	std::vector<uint8_t> writeData = {0xDE, 0xAD, 0xBE, 0xEF};
	uint32_t written = 0;
	vol.writeFork(handle, 0, writeData, written);
	CHECK(written == 4);
	vol.closeFork(handle);

	handle = vol.openFork(cnid, storage::ForkType::Resource, size, err);
	CHECK(size == 4);

	std::vector<uint8_t> readBuf(4);
	uint32_t got = 0;
	vol.readFork(handle, 0, readBuf, got);
	CHECK(got == 4);
	CHECK(readBuf == writeData);
	vol.closeFork(handle);
}

TEST_CASE("HostVolume: resource fork write creates sidecar")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	vol.createFile(storage::HostVolume::kRootDirID, "sctest", err);
	CHECK_FALSE(fs::exists(td.path / "._sctest"));

	uint32_t cnid = vol.findByName(storage::HostVolume::kRootDirID, "sctest")->cnid;
	uint32_t size = 0;
	uint32_t handle = vol.openFork(cnid, storage::ForkType::Resource, size, err);

	std::vector<uint8_t> data = {1, 2, 3};
	uint32_t written = 0;
	vol.writeFork(handle, 0, data, written);
	vol.closeFork(handle);

	CHECK(fs::exists(td.path / "._sctest"));
}

TEST_CASE("HostVolume: resource fork at offset")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	uint32_t cnid = vol.createFile(storage::HostVolume::kRootDirID, "rsrcoff.bin", err);
	uint32_t size = 0;

	/* Write 10 bytes at offset 0 */
	uint32_t handle = vol.openFork(cnid, storage::ForkType::Resource, size, err);
	std::vector<uint8_t> first(10, 0xAA);
	uint32_t written = 0;
	vol.writeFork(handle, 0, first, written);
	vol.closeFork(handle);

	/* Write 5 bytes at offset 5 (overwrites bytes 5-9) */
	handle = vol.openFork(cnid, storage::ForkType::Resource, size, err);
	std::vector<uint8_t> second(5, 0xBB);
	vol.writeFork(handle, 5, second, written);
	vol.closeFork(handle);

	/* Read all 10 bytes */
	handle = vol.openFork(cnid, storage::ForkType::Resource, size, err);
	CHECK(size == 10);
	std::vector<uint8_t> readBuf(10);
	uint32_t got = 0;
	vol.readFork(handle, 0, readBuf, got);
	CHECK(got == 10);
	/* Bytes 0-4 should be 0xAA, bytes 5-9 should be 0xBB */
	for (int i = 0; i < 5; ++i)
		CHECK(readBuf[i] == 0xAA);
	for (int i = 5; i < 10; ++i)
		CHECK(readBuf[i] == 0xBB);
	vol.closeFork(handle);
}

TEST_CASE("HostVolume: resource fork updates rsrcForkSize")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	uint32_t cnid = vol.createFile(storage::HostVolume::kRootDirID, "rsrcsz.bin", err);
	uint32_t size = 0;
	uint32_t handle = vol.openFork(cnid, storage::ForkType::Resource, size, err);

	std::vector<uint8_t> data(42, 0xFF);
	uint32_t written = 0;
	vol.writeFork(handle, 0, data, written);
	vol.closeFork(handle);

	auto *e = vol.findByCNID(cnid);
	CHECK(e->rsrcForkSize == 42);
}

/* ── Phase 7: TEXT fork I/O + stats ───────────────── */

TEST_CASE("HostVolume: TEXT file read converts UTF-8")
{
	TempDir td;
	/* Write UTF-8 "café" = 63 61 66 C3 A9 (5 bytes UTF-8, 4 bytes Mac Roman) */
	writeFile(td.path / "cafe.txt", "caf\xC3\xA9");

	storage::HostVolume vol;
	vol.mount(td.path);

	auto *e = vol.findByName(storage::HostVolume::kRootDirID, "cafe.txt");
	REQUIRE(e != nullptr);
	CHECK(e->isText);
	CHECK(e->dataForkSize == 4); /* Mac Roman size */

	storage::FMErr err;
	uint32_t size = 0;
	uint32_t handle = vol.openFork(e->cnid, storage::ForkType::Data, size, err);
	CHECK(size == 4);

	std::vector<uint8_t> buf(4);
	uint32_t got = 0;
	vol.readFork(handle, 0, buf, got);
	CHECK(got == 4);
	/* 'c'=0x63, 'a'=0x61, 'f'=0x66, 'é'=0x8E in Mac Roman */
	CHECK(buf[0] == 'c');
	CHECK(buf[1] == 'a');
	CHECK(buf[2] == 'f');
	CHECK(buf[3] == 0x8E);
	vol.closeFork(handle);
}

TEST_CASE("HostVolume: TEXT file read at offset")
{
	TempDir td;
	writeFile(td.path / "off.txt", "caf\xC3\xA9");

	storage::HostVolume vol;
	vol.mount(td.path);

	auto *e = vol.findByName(storage::HostVolume::kRootDirID, "off.txt");
	REQUIRE(e != nullptr);

	storage::FMErr err;
	uint32_t size = 0;
	uint32_t handle = vol.openFork(e->cnid, storage::ForkType::Data, size, err);

	std::vector<uint8_t> buf(2);
	uint32_t got = 0;
	vol.readFork(handle, 2, buf, got);
	CHECK(got == 2);
	CHECK(buf[0] == 'f');
	CHECK(buf[1] == 0x8E);
	vol.closeFork(handle);
}

TEST_CASE("HostVolume: TEXT file write converts to UTF-8")
{
	TempDir td;
	storage::HostVolume vol;
	vol.mount(td.path);

	storage::FMErr err;
	uint32_t cnid = vol.createFile(storage::HostVolume::kRootDirID, "write.txt", err);
	vol.setFileInfo(cnid, appledouble::FourCC("TEXT"), appledouble::FourCC("ttxt"));

	uint32_t size = 0;
	uint32_t handle = vol.openFork(cnid, storage::ForkType::Data, size, err);

	/* Write Mac Roman "café" = 63 61 66 8E */
	std::vector<uint8_t> macData = {0x63, 0x61, 0x66, 0x8E};
	uint32_t written = 0;
	vol.writeFork(handle, 0, macData, written);
	CHECK(written == 4);
	vol.closeFork(handle);

	/* Read back the host file — should be valid UTF-8 */
	std::ifstream f(td.path / "write.txt", std::ios::binary);
	std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	CHECK(content == "caf\xC3\xA9");
}

TEST_CASE("HostVolume: TEXT conversion stats")
{
	TempDir td;
	writeFile(td.path / "stats.txt", "hello");

	storage::HostVolume vol;
	vol.mount(td.path);

	auto *e = vol.findByName(storage::HostVolume::kRootDirID, "stats.txt");
	REQUIRE(e != nullptr);

	storage::FMErr err;
	uint32_t size = 0;

	for (int i = 0; i < 3; ++i)
	{
		uint32_t handle = vol.openFork(e->cnid, storage::ForkType::Data, size, err);
		std::vector<uint8_t> buf(5);
		uint32_t got = 0;
		vol.readFork(handle, 0, buf, got);
		vol.closeFork(handle);
	}

	auto stats = vol.textConversionStats();
	CHECK(stats.conversions == 3);
	CHECK(stats.bytesOut == 15); /* 3 × 5 */
}

TEST_CASE("HostVolume: reset text stats")
{
	TempDir td;
	writeFile(td.path / "rs.txt", "x");

	storage::HostVolume vol;
	vol.mount(td.path);

	auto *e = vol.findByName(storage::HostVolume::kRootDirID, "rs.txt");

	storage::FMErr err;
	uint32_t size = 0;
	uint32_t handle = vol.openFork(e->cnid, storage::ForkType::Data, size, err);
	std::vector<uint8_t> buf(1);
	uint32_t got = 0;
	vol.readFork(handle, 0, buf, got);
	vol.closeFork(handle);

	CHECK(vol.textConversionStats().conversions == 1);
	vol.resetTextConversionStats();
	CHECK(vol.textConversionStats().conversions == 0);
}

TEST_CASE("HostVolume: TEXT file dataForkSize in catalog")
{
	TempDir td;
	/* UTF-8 "résumé" = 72 C3 A9 73 75 6D C3 A9 (8 bytes UTF-8, 6 bytes Mac Roman) */
	writeFile(td.path / "resume.txt", "r\xC3\xA9sum\xC3\xA9");

	storage::HostVolume vol;
	vol.mount(td.path);

	auto *e = vol.findByName(storage::HostVolume::kRootDirID, "resume.txt");
	REQUIRE(e != nullptr);
	CHECK(e->isText);
	CHECK(e->dataForkSize == 6);
}
