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

/* ── WD table tests ──────────────────────────────── */

TEST_CASE("DriveManager: WD open/close")
{
	TempDir td("dm_wd1");
	storage::DriveManager dm;
	dm.mount(td.path);

	uint32_t wd = dm.openWD(0, 42, 7);
	CHECK(wd > 0);
	CHECK(dm.wdToDirID(wd) == 42);
	CHECK(dm.wdToProcID(wd) == 7);
	CHECK(dm.wdToSlot(wd) == 0);

	dm.closeWD(wd);
	CHECK(dm.wdToDirID(wd) == 0);
	CHECK(dm.wdToSlot(wd) == -1);
}

TEST_CASE("DriveManager: WD monotonic")
{
	TempDir td("dm_wd_mono");
	storage::DriveManager dm;
	dm.mount(td.path);

	// Root WD was already created by mount, so the next ones continue the sequence.
	uint32_t wd1 = dm.openWD(0, 10);
	uint32_t wd2 = dm.openWD(0, 20);
	uint32_t wd3 = dm.openWD(0, 30);
	CHECK(wd2 == wd1 + 1);
	CHECK(wd3 == wd2 + 1);
}

TEST_CASE("DriveManager: WD default")
{
	storage::DriveManager dm;
	TempDir td("dm_wd_def");
	dm.mount(td.path);

	uint32_t wd = dm.openWD(0, 99);
	dm.setDefaultWD(wd);
	CHECK(dm.defaultWD() == wd);
}

TEST_CASE("DriveManager: WD root auto-created")
{
	TempDir td("dm_wd_root");
	storage::DriveManager dm;
	int slot = dm.mount(td.path);
	REQUIRE(slot == 0);

	uint32_t rwd = dm.rootWD(0);
	CHECK(rwd > 0);
	CHECK(dm.wdToDirID(rwd) == storage::HostVolume::kRootDirID);
	CHECK(dm.wdToSlot(rwd) == 0);
}

TEST_CASE("DriveManager: WD isOurWD")
{
	TempDir td("dm_wd_ours");
	storage::DriveManager dm;
	dm.mount(td.path);

	uint32_t wd = dm.openWD(0, 42);
	CHECK(dm.isOurWD(wd));
	dm.closeWD(wd);
	CHECK_FALSE(dm.isOurWD(wd));
}

TEST_CASE("DriveManager: WD default set on first mount")
{
	TempDir td("dm_wd_defmount");
	storage::DriveManager dm;
	dm.mount(td.path);

	// First mount should set defaultWD to rootWD of slot 0.
	CHECK(dm.defaultWD() == dm.rootWD(0));
}

/* ── Encode/Decode WD ref tests ──────────────────── */

TEST_CASE("DriveManager: EncodeDecodeGuestWDRef round-trip")
{
	for (uint32_t wdRef = 1; wdRef <= 10; ++wdRef)
	{
		int16_t encoded = storage::EncodeGuestWDRef(wdRef);
		uint32_t decoded = storage::DecodeGuestWDRef(encoded);
		CHECK(decoded == wdRef);
	}
}

/* ── resolveDir tests (non-DefVCBPtr paths) ──────── */

TEST_CASE("DriveManager: resolveDir explicit dirID")
{
	TempDir td("dm_rd_explicit");
	storage::DriveManager dm;
	dm.mount(td.path);

	int slot = -1;
	// vRefNum = -32000 (our slot 0), dirID = 17 → returns 17, slot = 0.
	uint32_t dir = dm.resolveDir(-32000, 17, slot);
	CHECK(dir == 17);
	CHECK(slot == 0);
}

TEST_CASE("DriveManager: resolveDir WD refnum")
{
	TempDir td("dm_rd_wd");
	storage::DriveManager dm;
	dm.mount(td.path);

	uint32_t wd = dm.openWD(0, 42);
	int16_t guestVRef = storage::EncodeGuestWDRef(wd);

	int slot = -1;
	uint32_t dir = dm.resolveDir(guestVRef, 0, slot);
	CHECK(dir == 42);
	CHECK(slot == 0);
}

TEST_CASE("DriveManager: resolveDir unknown WD")
{
	TempDir td("dm_rd_unkn");
	storage::DriveManager dm;
	dm.mount(td.path);

	// A WD refnum that was never opened — encoded as guest vRefNum.
	int16_t badVRef = storage::EncodeGuestWDRef(9999);

	int slot = -1;
	uint32_t dir = dm.resolveDir(badVRef, 0, slot);
	CHECK(dir == 0);
	CHECK(slot == -1);
}

TEST_CASE("DriveManager: resolveDir drive number")
{
	TempDir td("dm_rd_drv");
	storage::DriveManager dm;
	dm.mount(td.path);

	int slot = -1;
	uint32_t dir = dm.resolveDir(8, 0, slot); // driveNum 8 = slot 0
	CHECK(dir == storage::HostVolume::kRootDirID);
	CHECK(slot == 0);
}

TEST_CASE("DriveManager: slotFromName")
{
	TempDir td("dm_name");
	storage::DriveManager dm;
	dm.mount(td.path);

	auto name = dm.volumeName(0);
	CHECK(dm.slotFromName(name) == 0);
	CHECK(dm.slotFromName("nonexistent") == -1);
}
