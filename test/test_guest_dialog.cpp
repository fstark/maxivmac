/*
	test_guest_dialog.cpp — Unit tests for guest Dialog Manager introspection.
*/
#include <doctest/doctest.h>
#include "guest/guest_dialog.h"
#include <cstring>

extern uint8_t g_ram[];

static void putWord(uint32_t addr, uint16_t val)
{
	g_ram[addr] = static_cast<uint8_t>(val >> 8);
	g_ram[addr + 1] = static_cast<uint8_t>(val & 0xFF);
}

static void putLong(uint32_t addr, uint32_t val)
{
	g_ram[addr] = static_cast<uint8_t>((val >> 24) & 0xFF);
	g_ram[addr + 1] = static_cast<uint8_t>((val >> 16) & 0xFF);
	g_ram[addr + 2] = static_cast<uint8_t>((val >> 8) & 0xFF);
	g_ram[addr + 3] = static_cast<uint8_t>(val & 0xFF);
}

static void clearRam()
{
	std::memset(g_ram, 0, 4096);
}

/*
	Layout in g_ram for tests:
	- WindowList low-mem at 0x09D6 → points to window at 0x0100
	- Window/DialogRecord at 0x0100:
	    +8  portBounds.top   (for global origin calc)
	    +10 portBounds.left
	    +16 portRect (8 bytes)
	    +108 windowKind = 2 (dialog)
	    +156 items handle → handle at 0x0300
	    +168 aDefItem
	- Handle at 0x0300 → points to DITL data at 0x0400
	- DITL data at 0x0400
*/

static constexpr uint32_t kWindowListAddr = 0x09D6;
static constexpr uint32_t kWindowAddr = 0x0100;
static constexpr uint32_t kHandleAddr = 0x0300;
static constexpr uint32_t kDITLAddr = 0x0400;

// Build a minimal dialog window + DITL with a single "OK" button
static void buildSimpleDialog()
{
	clearRam();

	// WindowList → window
	putLong(kWindowListAddr, kWindowAddr);

	// portBounds (for global origin): top=0, left=0 means origin is (0,0)
	putWord(kWindowAddr + 8, 0);   // bounds.top
	putWord(kWindowAddr + 10, 0);  // bounds.left

	// portRect: (10, 20, 60, 120) — top, left, bottom, right
	putWord(kWindowAddr + 16, 10);   // top
	putWord(kWindowAddr + 18, 20);   // left
	putWord(kWindowAddr + 20, 60);   // bottom
	putWord(kWindowAddr + 22, 120);  // right

	// windowKind = 2 (dialog)
	putWord(kWindowAddr + 108, 2);

	// items handle
	putLong(kWindowAddr + 156, kHandleAddr);

	// aDefItem = 1
	putWord(kWindowAddr + 168, 1);

	// Handle dereference: *handle → DITL data
	putLong(kHandleAddr, kDITLAddr);

	// DITL: 1 item (count - 1 = 0)
	uint32_t p = kDITLAddr;
	putWord(p, 0); // itemCount - 1
	p += 2;

	// Item 1: Button "OK"
	putLong(p, 0); // reserved handle
	p += 4;
	// bounds: top=30, left=50, bottom=50, right=90
	putWord(p, 30); p += 2;  // top
	putWord(p, 50); p += 2;  // left
	putWord(p, 50); p += 2;  // bottom
	putWord(p, 90); p += 2;  // right
	g_ram[p++] = 4; // type = button (4), enabled (no high bit)
	g_ram[p++] = 2; // dataLen = 2
	g_ram[p++] = 'O';
	g_ram[p++] = 'K';
	// dataLen=2 is even, no padding needed
}

// Build a dialog with two buttons: "Cancel" and "Yes"
static void buildTwoButtonDialog()
{
	clearRam();

	putLong(kWindowListAddr, kWindowAddr);
	putWord(kWindowAddr + 8, 0xFFD8);   // bounds.top = -40 (window at global y=40)
	putWord(kWindowAddr + 10, 0xFFF6);  // bounds.left = -10 (window at global x=10)
	putWord(kWindowAddr + 16, 0);
	putWord(kWindowAddr + 18, 0);
	putWord(kWindowAddr + 20, 100);
	putWord(kWindowAddr + 22, 200);
	putWord(kWindowAddr + 108, 2); // dialog
	putLong(kWindowAddr + 156, kHandleAddr);
	putWord(kWindowAddr + 168, 1); // default = item 1
	putLong(kHandleAddr, kDITLAddr);

	uint32_t p = kDITLAddr;
	putWord(p, 1); // 2 items (count - 1 = 1)
	p += 2;

	// Item 1: Button "Cancel" (7 bytes, odd → needs padding)
	putLong(p, 0); p += 4;
	putWord(p, 10); p += 2;  // top
	putWord(p, 10); p += 2;  // left
	putWord(p, 30); p += 2;  // bottom
	putWord(p, 80); p += 2;  // right
	g_ram[p++] = 4; // button, enabled
	g_ram[p++] = 6; // "Cancel" length
	g_ram[p++] = 'C'; g_ram[p++] = 'a'; g_ram[p++] = 'n';
	g_ram[p++] = 'c'; g_ram[p++] = 'e'; g_ram[p++] = 'l';
	// dataLen=6 is even, no padding

	// Item 2: Button "Yes" (3 bytes, odd → needs padding)
	putLong(p, 0); p += 4;
	putWord(p, 40); p += 2;  // top
	putWord(p, 10); p += 2;  // left
	putWord(p, 60); p += 2;  // bottom
	putWord(p, 80); p += 2;  // right
	g_ram[p++] = 4; // button, enabled
	g_ram[p++] = 3; // "Yes" length
	g_ram[p++] = 'Y'; g_ram[p++] = 'e'; g_ram[p++] = 's';
	g_ram[p++] = 0; // pad to even
}

TEST_CASE("guest_dialog: no front window")
{
	clearRam();
	// WindowList = 0 (no windows)
	putLong(kWindowListAddr, 0);
	auto dlg = guest::readFrontDialog();
	CHECK(!dlg.has_value());
}

TEST_CASE("guest_dialog: non-dialog window")
{
	clearRam();
	putLong(kWindowListAddr, kWindowAddr);
	putWord(kWindowAddr + 108, 8); // windowKind = 8 (not 2)
	auto dlg = guest::readFrontDialog();
	CHECK(!dlg.has_value());
}

TEST_CASE("guest_dialog: single button dialog")
{
	buildSimpleDialog();
	auto dlg = guest::readFrontDialog();
	REQUIRE(dlg.has_value());

	CHECK(dlg->windowPtr == kWindowAddr);
	CHECK(dlg->defaultItem == 1);
	CHECK(dlg->items.size() == 1);
	CHECK(dlg->items[0].type == guest::DialogItemType::Button);
	CHECK(dlg->items[0].text == "OK");
	CHECK(dlg->items[0].index == 1);
	CHECK(dlg->items[0].enabled == true);
	CHECK(dlg->items[0].bounds.top == 30);
	CHECK(dlg->items[0].bounds.left == 50);
	CHECK(dlg->items[0].bounds.bottom == 50);
	CHECK(dlg->items[0].bounds.right == 90);
}

TEST_CASE("guest_dialog: findButton case-insensitive")
{
	buildSimpleDialog();
	auto dlg = guest::readFrontDialog();
	REQUIRE(dlg.has_value());

	auto *btn = guest::findButton(*dlg, "ok");
	REQUIRE(btn != nullptr);
	CHECK(btn->text == "OK");

	auto *btn2 = guest::findButton(*dlg, "OK");
	REQUIRE(btn2 != nullptr);

	auto *none = guest::findButton(*dlg, "Cancel");
	CHECK(none == nullptr);
}

TEST_CASE("guest_dialog: itemCenter with origin offset")
{
	buildTwoButtonDialog();
	auto dlg = guest::readFrontDialog();
	REQUIRE(dlg.has_value());

	// Origin = -portBounds.topLeft = -(−40, −10) = (10, 40)
	CHECK(dlg->origin.h == 10);
	CHECK(dlg->origin.v == 40);

	auto *yes = guest::findButton(*dlg, "Yes");
	REQUIRE(yes != nullptr);
	// bounds: top=40, left=10, bottom=60, right=80
	// center local: h=(10+80)/2=45, v=(40+60)/2=50
	// center global: h=45+10=55, v=50+40=90
	auto pt = guest::itemCenter(*dlg, *yes);
	CHECK(pt.h == 55);
	CHECK(pt.v == 90);
}

TEST_CASE("guest_dialog: two buttons findButton")
{
	buildTwoButtonDialog();
	auto dlg = guest::readFrontDialog();
	REQUIRE(dlg.has_value());

	CHECK(dlg->items.size() == 2);

	auto *cancel = guest::findButton(*dlg, "Cancel");
	REQUIRE(cancel != nullptr);
	CHECK(cancel->index == 1);

	auto *yes = guest::findButton(*dlg, "Yes");
	REQUIRE(yes != nullptr);
	CHECK(yes->index == 2);
}
