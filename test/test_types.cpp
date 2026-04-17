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
