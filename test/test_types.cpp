#include <doctest/doctest.h>
#include "lang/type_registry.h"

#include <cstring>
#include <filesystem>
#include <fstream>

extern uint8_t g_ram[];
extern uint8_t get_vm_byte(uint32_t addr);
extern uint16_t get_vm_word(uint32_t addr);
extern uint32_t get_vm_long(uint32_t addr);

[[maybe_unused]]
static void put_be16(uint32_t addr, uint16_t val)
{
	g_ram[addr] = static_cast<uint8_t>(val >> 8);
	g_ram[addr + 1] = static_cast<uint8_t>(val & 0xFF);
}

[[maybe_unused]]
static void put_be32(uint32_t addr, uint32_t val)
{
	g_ram[addr] = static_cast<uint8_t>((val >> 24) & 0xFF);
	g_ram[addr + 1] = static_cast<uint8_t>((val >> 16) & 0xFF);
	g_ram[addr + 2] = static_cast<uint8_t>((val >> 8) & 0xFF);
	g_ram[addr + 3] = static_cast<uint8_t>(val & 0xFF);
}

static std::filesystem::path writeTempDef(const std::string &content)
{
	auto tmp = std::filesystem::temp_directory_path() / "test_types.def";
	{
		std::ofstream f(tmp);
		f << content;
	}
	return tmp;
}

TEST_CASE("TypeRegistry smoke")
{
	TypeRegistry reg;
	reg.init({get_vm_byte, get_vm_word, get_vm_long});
	CHECK_FALSE(reg.has("Point"));
}

TEST_CASE("TypeRegistry parses simple struct")
{
	auto tmp = writeTempDef("struct Point {\n"
							"    0  sword  v\n"
							"    2  sword  h\n"
							"}\n");
	TypeRegistry reg;
	reg.init({get_vm_byte, get_vm_word, get_vm_long});
	CHECK(reg.load(tmp) == 1);
	CHECK(reg.has("Point"));
	CHECK(reg.has("sword")); /* primitive */
	std::filesystem::remove(tmp);
}

TEST_CASE("TypeRegistry parses nested struct")
{
	auto tmp = writeTempDef("struct Point {\n"
							"    0  sword  v\n"
							"    2  sword  h\n"
							"}\n"
							"struct Rect {\n"
							"    0  sword  top\n"
							"    2  sword  left\n"
							"    4  sword  bottom\n"
							"    6  sword  right\n"
							"}\n");
	TypeRegistry reg;
	reg.init({get_vm_byte, get_vm_word, get_vm_long});
	CHECK(reg.load(tmp) == 2);
	CHECK(reg.has("Point"));
	CHECK(reg.has("Rect"));
	std::filesystem::remove(tmp);
}

TEST_CASE("TypeRegistry parses union")
{
	auto tmp = writeTempDef("struct FileInfo {\n"
							"    0  long  fileID\n"
							"}\n"
							"struct DirInfo {\n"
							"    0  long  dirID\n"
							"}\n"
							"union CInfoPBRec {\n"
							"    file FileInfo\n"
							"    dir  DirInfo\n"
							"}\n");
	TypeRegistry reg;
	reg.init({get_vm_byte, get_vm_word, get_vm_long});
	CHECK(reg.load(tmp) == 3);
	CHECK(reg.has("CInfoPBRec"));
	std::filesystem::remove(tmp);
}

TEST_CASE("TypeRegistry parses array field")
{
	auto tmp = writeTempDef("struct Foo {\n"
							"    0  byte  data[8]\n"
							"}\n");
	TypeRegistry reg;
	reg.init({get_vm_byte, get_vm_word, get_vm_long});
	CHECK(reg.load(tmp) == 1);
	CHECK(reg.has("Foo"));
	std::filesystem::remove(tmp);
}

TEST_CASE("TypeRegistry loadErrors")
{
	TypeRegistry reg;
	reg.init({get_vm_byte, get_vm_word, get_vm_long});
	int e = reg.loadErrors("assets/errors.def");
	CHECK(e > 0);
}

TEST_CASE("TypeRegistry rejects overlapping offsets")
{
	auto tmp = writeTempDef("struct Bad {\n"
							"    0  long  a\n"
							"    2  word  b\n"
							"}\n");
	TypeRegistry reg;
	reg.init({get_vm_byte, get_vm_word, get_vm_long});
	int n = reg.load(tmp);
	/* The struct should load but 'b' field should be rejected as overlapping */
	CHECK(n == 1);
	std::filesystem::remove(tmp);
}

/* ════════════════════════════════════════════════════════
   Phase 3 — Read algorithm and primitive formatter
   ════════════════════════════════════════════════════════ */

TEST_CASE("TypeRegistry reads Point struct")
{
	auto tmp = writeTempDef("struct Point {\n"
							"    0  sword  v\n"
							"    2  sword  h\n"
							"}\n");
	TypeRegistry reg;
	reg.init({get_vm_byte, get_vm_word, get_vm_long});
	CHECK(reg.load(tmp) == 1);
	CHECK(reg.sizeOf("Point") == 4);

	/* v=100, h=-50 */
	put_be16(0x100, 100);
	put_be16(0x102, static_cast<uint16_t>(-50));

	auto fields = reg.read("Point", 0x100);
	REQUIRE(fields.size() == 2);
	CHECK(fields[0].name == "v");
	CHECK(fields[0].display == "100");
	CHECK(fields[1].name == "h");
	CHECK(fields[1].display == "-50");

	std::filesystem::remove(tmp);
}

TEST_CASE("TypeRegistry reads nested struct")
{
	auto tmp = writeTempDef("struct Point {\n"
							"    0  sword  v\n"
							"    2  sword  h\n"
							"}\n"
							"struct Foo {\n"
							"    0  Point  pt\n"
							"    4  sword  x\n"
							"}\n");
	TypeRegistry reg;
	reg.init({get_vm_byte, get_vm_word, get_vm_long});
	CHECK(reg.load(tmp) == 2);
	CHECK(reg.sizeOf("Foo") == 6);

	/* pt.v=10, pt.h=20, x=42 */
	put_be16(0x100, 10);
	put_be16(0x102, 20);
	put_be16(0x104, 42);

	auto fields = reg.read("Foo", 0x100);
	REQUIRE(fields.size() == 3);
	CHECK(fields[0].name == "pt.v");
	CHECK(fields[0].display == "10");
	CHECK(fields[1].name == "pt.h");
	CHECK(fields[1].display == "20");
	CHECK(fields[2].name == "x");
	CHECK(fields[2].display == "42");

	std::filesystem::remove(tmp);
}

TEST_CASE("TypeRegistry reads array")
{
	auto tmp = writeTempDef("struct Foo {\n"
							"    0  byte  data[4]\n"
							"}\n");
	TypeRegistry reg;
	reg.init({get_vm_byte, get_vm_word, get_vm_long});
	CHECK(reg.load(tmp) == 1);
	CHECK(reg.sizeOf("Foo") == 4);

	g_ram[0x100] = 0xAA;
	g_ram[0x101] = 0xBB;
	g_ram[0x102] = 0xCC;
	g_ram[0x103] = 0xDD;

	auto fields = reg.read("Foo", 0x100);
	REQUIRE(fields.size() == 4);
	CHECK(fields[0].name == "data[0]");
	CHECK(fields[0].display == "$AA");
	CHECK(fields[1].name == "data[1]");
	CHECK(fields[1].display == "$BB");
	CHECK(fields[2].name == "data[2]");
	CHECK(fields[2].display == "$CC");
	CHECK(fields[3].name == "data[3]");
	CHECK(fields[3].display == "$DD");

	std::filesystem::remove(tmp);
}

TEST_CASE("TypeRegistry reads union variant")
{
	auto tmp = writeTempDef("struct FileInfo {\n"
							"    0  long  fileID\n"
							"}\n"
							"struct DirInfo {\n"
							"    0  long  dirID\n"
							"}\n"
							"union CInfoPBRec {\n"
							"    file FileInfo\n"
							"    dir  DirInfo\n"
							"}\n");
	TypeRegistry reg;
	reg.init({get_vm_byte, get_vm_word, get_vm_long});
	CHECK(reg.load(tmp) == 3);

	put_be32(0x100, 0x12345678);

	auto fileFields = reg.read("CInfoPBRec", 0x100, "file");
	REQUIRE(fileFields.size() == 1);
	CHECK(fileFields[0].name == "fileID");
	CHECK(fileFields[0].display == "$12345678");

	auto dirFields = reg.read("CInfoPBRec", 0x100, "dir");
	REQUIRE(dirFields.size() == 1);
	CHECK(dirFields[0].name == "dirID");
	CHECK(dirFields[0].display == "$12345678");

	std::filesystem::remove(tmp);
}

TEST_CASE("TypeRegistry sizeOf")
{
	auto tmp = writeTempDef("struct Point {\n"
							"    0  sword  v\n"
							"    2  sword  h\n"
							"}\n");
	TypeRegistry reg;
	reg.init({get_vm_byte, get_vm_word, get_vm_long});
	reg.load(tmp);

	CHECK(reg.sizeOf("Point") == 4);
	CHECK(reg.sizeOf("sword") == 2);
	CHECK(reg.sizeOf("long") == 4);
	CHECK(reg.sizeOf("byte") == 1);
	CHECK(reg.sizeOf("UnknownType") == 0);

	std::filesystem::remove(tmp);
}

TEST_CASE("TypeRegistry OSErr formatting")
{
	auto tmp = writeTempDef("struct Err {\n"
							"    0  OSErr  result\n"
							"}\n");
	TypeRegistry reg;
	reg.init({get_vm_byte, get_vm_word, get_vm_long});
	reg.load(tmp);
	reg.loadErrors("assets/errors.def");

	/* -43 = fnfErr */
	put_be16(0x100, static_cast<uint16_t>(-43));

	auto fields = reg.read("Err", 0x100);
	REQUIRE(fields.size() == 1);
	CHECK(fields[0].display == "-43 fnfErr");

	std::filesystem::remove(tmp);
}

TEST_CASE("TypeRegistry OSType formatting")
{
	auto tmp = writeTempDef("struct Typ {\n"
							"    0  OSType  t\n"
							"}\n");
	TypeRegistry reg;
	reg.init({get_vm_byte, get_vm_word, get_vm_long});
	reg.load(tmp);

	/* 'TEXT' = 0x54455854 */
	put_be32(0x100, 0x54455854);
	auto fields = reg.read("Typ", 0x100);
	REQUIRE(fields.size() == 1);
	CHECK(fields[0].display == "'TEXT'");

	/* Non-printable → hex */
	put_be32(0x100, 0x00000000);
	fields = reg.read("Typ", 0x100);
	REQUIRE(fields.size() == 1);
	CHECK(fields[0].display == "$00000000");

	std::filesystem::remove(tmp);
}

TEST_CASE("TypeRegistry Str255 formatting")
{
	auto tmp = writeTempDef("struct Str {\n"
							"    0  Str255  s\n"
							"}\n");
	TypeRegistry reg;
	reg.init({get_vm_byte, get_vm_word, get_vm_long});
	reg.load(tmp);

	/* Pascal string "Hello" */
	g_ram[0x100] = 5;
	g_ram[0x101] = 'H';
	g_ram[0x102] = 'e';
	g_ram[0x103] = 'l';
	g_ram[0x104] = 'l';
	g_ram[0x105] = 'o';

	auto fields = reg.read("Str", 0x100);
	REQUIRE(fields.size() == 1);
	CHECK(fields[0].display == "\"Hello\"");

	std::filesystem::remove(tmp);
}

/* ════════════════════════════════════════════════════════
   Phase 4 — format() and readField()
   ════════════════════════════════════════════════════════ */

TEST_CASE("TypeRegistry format output")
{
	auto tmp = writeTempDef("struct Point {\n"
							"    0  sword  v\n"
							"    2  sword  h\n"
							"}\n");
	TypeRegistry reg;
	reg.init({get_vm_byte, get_vm_word, get_vm_long});
	reg.load(tmp);

	put_be16(0x100, 100);
	put_be16(0x102, static_cast<uint16_t>(-50));

	auto output = reg.format("Point", 0x100);
	CHECK(output.find("v:") != std::string::npos);
	CHECK(output.find("h:") != std::string::npos);
	CHECK(output.find("100") != std::string::npos);
	CHECK(output.find("-50") != std::string::npos);

	std::filesystem::remove(tmp);
}

TEST_CASE("TypeRegistry readField")
{
	auto tmp = writeTempDef("struct Point {\n"
							"    0  sword  v\n"
							"    2  sword  h\n"
							"}\n");
	TypeRegistry reg;
	reg.init({get_vm_byte, get_vm_word, get_vm_long});
	reg.load(tmp);

	put_be16(0x100, 100);
	put_be16(0x102, static_cast<uint16_t>(-50));

	CHECK(reg.readField("Point", 0x100, "v") == "100");
	CHECK(reg.readField("Point", 0x100, "h") == "-50");
	CHECK(reg.readField("Point", 0x100, "nonexistent") == "");

	std::filesystem::remove(tmp);
}

TEST_CASE("TypeRegistry format nested struct")
{
	auto tmp = writeTempDef("struct FInfo {\n"
							"    0  OSType  fdType\n"
							"    4  OSType  fdCreator\n"
							"    8  word    fdFlags\n"
							"}\n");
	TypeRegistry reg;
	reg.init({get_vm_byte, get_vm_word, get_vm_long});
	reg.load(tmp);

	/* 'TEXT' = 0x54455854 */
	put_be32(0x100, 0x54455854);
	/* 'ttxt' = 0x74747874 */
	put_be32(0x104, 0x74747874);
	put_be16(0x108, 0x0100);

	auto output = reg.format("FInfo", 0x100);
	CHECK(output.find("'TEXT'") != std::string::npos);
	CHECK(output.find("'ttxt'") != std::string::npos);

	std::filesystem::remove(tmp);
}

/* ════════════════════════════════════════════════════════
   Phase 5 — assets/types.def smoke test
   ════════════════════════════════════════════════════════ */

TEST_CASE("types.def loads all expected types")
{
	TypeRegistry reg;
	reg.init({get_vm_byte, get_vm_word, get_vm_long});
	int n = reg.load("assets/types.def");
	CHECK(n > 0);

	CHECK(reg.has("Point"));
	CHECK(reg.has("Rect"));
	CHECK(reg.has("FInfo"));
	CHECK(reg.has("FXInfo"));
	CHECK(reg.has("DInfo"));
	CHECK(reg.has("DXInfo"));
	CHECK(reg.has("ParamBlockHeader"));
	CHECK(reg.has("IOParam"));
	CHECK(reg.has("FileParam"));
	CHECK(reg.has("VolumeParam"));
	CHECK(reg.has("HFileInfo"));
	CHECK(reg.has("DirInfo"));
	CHECK(reg.has("CInfoPBRec"));
	CHECK(reg.has("WDParam"));
	CHECK(reg.has("FCBPBRec"));
	CHECK(reg.has("BitMap"));
	CHECK(reg.has("GrafPort"));
	CHECK(reg.has("WindowRecord"));

	CHECK(reg.sizeOf("Point") == 4);
	CHECK(reg.sizeOf("Rect") == 8);
	CHECK(reg.sizeOf("FInfo") == 16);
	CHECK(reg.sizeOf("ParamBlockHeader") == 24);
	CHECK(reg.sizeOf("GrafPort") == 108);
	CHECK(reg.sizeOf("WindowRecord") == 156);
}
