#include <doctest/doctest.h>
#include "storage/drive_manager.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace
{

fs::path makeTempDir(const char *name)
{
	auto p = fs::temp_directory_path() / name;
	fs::remove_all(p);
	fs::create_directories(p);
	return p;
}

struct TempDir
{
	fs::path path;
	TempDir(const char *n) : path(makeTempDir(n)) {}
	~TempDir() { fs::remove_all(path); }
};

} // namespace

TEST_CASE("DriveManager: MountOne")
{
	TempDir td("dm_mount1");
	storage::DriveManager dm;

	int slot = dm.mount(td.path);
	CHECK(slot == 0);
	CHECK(dm.volume(0) != nullptr);
	CHECK(dm.mountedCount() == 1);
}

TEST_CASE("DriveManager: MountMax")
{
	std::vector<TempDir> dirs;
	storage::DriveManager dm;

	for (int i = 0; i < storage::kMaxDrives; ++i)
	{
		std::string name = "dm_max_" + std::to_string(i);
		dirs.emplace_back(name.c_str());
		int slot = dm.mount(dirs.back().path);
		CHECK(slot == i);
	}
	CHECK(dm.mountedCount() == storage::kMaxDrives);

	// 9th mount should fail.
	TempDir extra("dm_max_extra");
	CHECK(dm.mount(extra.path) == -1);
}

TEST_CASE("DriveManager: Unmount")
{
	TempDir td("dm_unmount");
	storage::DriveManager dm;

	int slot = dm.mount(td.path);
	CHECK(slot == 0);
	CHECK(dm.unmount(0));
	CHECK(dm.volume(0) == nullptr);
	CHECK(dm.mountedCount() == 0);
}

TEST_CASE("DriveManager: Remount")
{
	TempDir td1("dm_remount1");
	TempDir td2("dm_remount2");
	TempDir td3("dm_remount3");
	storage::DriveManager dm;

	int s0 = dm.mount(td1.path);
	int s1 = dm.mount(td2.path);
	CHECK(s0 == 0);
	CHECK(s1 == 1);

	// Unmount slot 0, mount a new dir — should reuse slot 0.
	dm.unmount(0);
	int s2 = dm.mount(td3.path);
	CHECK(s2 == 0);
	CHECK(dm.mountedCount() == 2);
}

TEST_CASE("DriveManager: VRefNumLookup")
{
	TempDir td("dm_vref");
	storage::DriveManager dm;
	dm.mount(td.path);

	CHECK(dm.slotFromVRefNum(-32000) == 0);
	CHECK(dm.slotFromVRefNum(-32001) == -1); // slot 1 not mounted
	CHECK(dm.slotFromVRefNum(-31999) == -1); // out of range
	CHECK(dm.slotFromVRefNum(0) == -1);		 // not ours

	CHECK(dm.volumeByVRefNum(-32000) != nullptr);
	CHECK(dm.volumeByDriveNum(8) != nullptr);
	CHECK(dm.volumeByDriveNum(9) == nullptr);
}

TEST_CASE("DriveManager: NameDedup")
{
	storage::DriveManager dm;

	// Two dirs with the same name "tmp".
	auto p1 = fs::temp_directory_path() / "dm_dedup1" / "tmp";
	auto p2 = fs::temp_directory_path() / "dm_dedup2" / "tmp";
	fs::remove_all(p1);
	fs::remove_all(p2);
	fs::create_directories(p1);
	fs::create_directories(p2);

	int s0 = dm.mount(p1);
	int s1 = dm.mount(p2);
	CHECK(s0 >= 0);
	CHECK(s1 >= 0);

	auto n0 = dm.volumeName(s0);
	auto n1 = dm.volumeName(s1);
	CHECK(n0 == "tmp");
	CHECK(n1 == "tmp-2");

	fs::remove_all(fs::temp_directory_path() / "dm_dedup1");
	fs::remove_all(fs::temp_directory_path() / "dm_dedup2");
}

TEST_CASE("DriveManager: PendingQueue")
{
	TempDir td1("dm_pq1");
	TempDir td2("dm_pq2");
	storage::DriveManager dm;

	int s0 = dm.mount(td1.path);
	int s1 = dm.mount(td2.path);

	// Mount queues pending in FIFO order.
	CHECK(dm.popPendingMount() == s0);
	CHECK(dm.popPendingMount() == s1);
	CHECK(dm.popPendingMount() == -1);
}

TEST_CASE("DriveManager: HandleEncoding")
{
	CHECK(storage::SlotFromHandle(storage::EncodeHandle(3, 42)) == 3);
	CHECK(storage::LocalHandle(storage::EncodeHandle(3, 42)) == 42);
	CHECK(storage::EncodeHandle(0, 1) == 1); // slot 0 is pass-through
	CHECK(storage::SlotFromHandle(0x70000005) == 7);
	CHECK(storage::LocalHandle(0x70000005) == 5);
}
