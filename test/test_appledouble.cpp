#include <doctest/doctest.h>
#include "storage/appledouble.h"
#include "storage/appledouble_internal.h"
#include "util/macroman.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

using namespace appledouble;

namespace
{

std::filesystem::path writeTempFile(std::string_view content, std::string_view name = "test.txt")
{
	auto p = std::filesystem::temp_directory_path() / name;
	std::ofstream f(p, std::ios::binary);
	f.write(content.data(), static_cast<std::streamsize>(content.size()));
	return p;
}

// Clean up a temp file and its sidecar
void cleanup(const std::filesystem::path &p)
{
	std::filesystem::remove(SidecarPathFor(p));
	std::filesystem::remove(p);
}

} // namespace

/* ══════════════════════════════════════════════════════
   Phase 1 — FourCC, TypeMappings, SidecarPath
   ══════════════════════════════════════════════════════ */

TEST_CASE("FourCC produces correct values")
{
	CHECK(FourCC("TEXT") == 0x54455854u);
	CHECK(FourCC("ttxt") == 0x74747874u);
	CHECK(FourCC("????") == 0x3F3F3F3Fu);
}

TEST_CASE("LoadTypeMappings and FinderInfoFromExtension")
{
	auto p = std::filesystem::temp_directory_path() / "test_typemap.def";
	{
		std::ofstream f(p);
		f << "# test mappings\n"
		  << ".txt  TEXT ttxt\n"
		  << ".jpg  JPEG ogle\n"
		  << "\n"
		  << "# comment\n"
		  << ".bin  BINA hDmp\n";
	}
	int count = LoadTypeMappings(p);
	CHECK(count == 3);

	auto fi = FinderInfoFromExtension(".txt");
	CHECK(fi.type == FourCC("TEXT"));
	CHECK(fi.creator == FourCC("ttxt"));

	fi = FinderInfoFromExtension(".jpg");
	CHECK(fi.type == FourCC("JPEG"));
	CHECK(fi.creator == FourCC("ogle"));

	std::filesystem::remove(p);
}

TEST_CASE("FinderInfoFromExtension is case insensitive")
{
	// Relies on LoadTypeMappings from previous test
	auto lower = FinderInfoFromExtension(".txt");
	auto upper = FinderInfoFromExtension(".TXT");
	auto mixed = FinderInfoFromExtension(".Txt");
	CHECK(lower == upper);
	CHECK(lower == mixed);
}

TEST_CASE("FinderInfoFromExtension unknown returns ????")
{
	auto fi = FinderInfoFromExtension(".xyz");
	CHECK(fi.type == FourCC("????"));
	CHECK(fi.creator == FourCC("????"));
}

TEST_CASE("LoadTypeMappings returns -1 for missing file")
{
	CHECK(LoadTypeMappings("/nonexistent/typemap.def") == -1);
}

TEST_CASE("LoadTypeMappings loads actual assets/typemap.def")
{
	int count = LoadTypeMappings("data/debug/typemap.def");
	CHECK(count >= 19);
	auto fi = FinderInfoFromExtension(".txt");
	CHECK(fi.type == FourCC("TEXT"));
}

TEST_CASE("TypeMap standalone instance")
{
	auto p = std::filesystem::temp_directory_path() / "test_typemap2.def";
	{
		std::ofstream f(p);
		f << ".c    TEXT CWIE\n"
		  << ".pas  TEXT CWIE\n";
	}
	TypeMap tm;
	CHECK(tm.load(p) == 2);

	auto fi = tm.lookup(".c");
	CHECK(fi.type == FourCC("TEXT"));
	CHECK(fi.creator == FourCC("CWIE"));

	// Unknown extension returns ????
	fi = tm.lookup(".xyz");
	CHECK(fi.type == FourCC("????"));

	// Does not affect the global mapping
	auto global = FinderInfoFromExtension(".c");
	CHECK(global.creator == FourCC("KAHL")); // from assets/typemap.def

	std::filesystem::remove(p);
}

TEST_CASE("TypeMap load from missing file returns -1")
{
	TypeMap tm;
	CHECK(tm.load("/nonexistent/typemap.def") == -1);
	CHECK(tm.empty());
}

TEST_CASE("SidecarPathFor basic")
{
	namespace fs = std::filesystem;
	CHECK(SidecarPathFor("/dir/foo.txt") == fs::path("/dir/._foo.txt"));
	CHECK(SidecarPathFor("/a/b/README") == fs::path("/a/b/._README"));
	CHECK(SidecarPathFor("plain.bin") == fs::path("._plain.bin"));
}

/* ══════════════════════════════════════════════════════
   Phase 2 — Filename Escaping
   ══════════════════════════════════════════════════════ */

TEST_CASE("HostNameFromMac escapes structurally-invalid characters")
{
	CHECK(HostNameFromMac("My:File") == "My\x1B"
										"3AFile");
	CHECK(HostNameFromMac("A/B") == "A\x1B"
									"2FB");
	/* Only / : and ESC are escaped; others pass through */
	CHECK(HostNameFromMac("A^B") == "A^B");
	CHECK(HostNameFromMac("a\"b") == "a\"b");
	CHECK(HostNameFromMac("a*b") == "a*b");
	CHECK(HostNameFromMac("a<b>c") == "a<b>c");
	CHECK(HostNameFromMac("a?b") == "a?b");
	CHECK(HostNameFromMac("a\\b") == "a\\b");
	CHECK(HostNameFromMac("a|b") == "a|b");
}

TEST_CASE("HostNameFromMac no-op on clean names")
{
	CHECK(HostNameFromMac("readme.txt") == "readme.txt");
	CHECK(HostNameFromMac("") == "");
}

TEST_CASE("MacNameFromHost decodes ESC-XX sequences")
{
	CHECK(MacNameFromHost("My\x1B"
						  "3AFile") == "My:File");
	CHECK(MacNameFromHost("A\x1B"
						  "1BFile") == std::string("A\x1B"
												   "File"));
}

TEST_CASE("Filename escaping round-trips")
{
	for (int b = 0x20; b < 0x7F; ++b)
	{
		std::string mac(1, static_cast<char>(b));
		CHECK(MacNameFromHost(HostNameFromMac(mac)) == mac);
	}
	for (int b = 0x80; b <= 0xFF; ++b)
	{
		std::string mac(1, static_cast<char>(b));
		CHECK(MacNameFromHost(HostNameFromMac(mac)) == mac);
	}
}

TEST_CASE("MacNameFromHost handles trailing ESC gracefully")
{
	CHECK(MacNameFromHost("abc\x1B") == std::string("abc\x1B"));
	CHECK(MacNameFromHost("abc\x1B"
						  "2") == std::string("abc\x1B"
											  "2"));
}

TEST_CASE("MacNameFromHost rejects unmappable UTF-8")
{
	CHECK_FALSE(MacNameFromHost("\xe4\xb8\xad").has_value()); /* CJK character */
}

TEST_CASE("IsSidecar")
{
	CHECK(IsSidecar("._foo.txt"));
	CHECK(IsSidecar("._"));
	CHECK_FALSE(IsSidecar("foo.txt"));
	CHECK_FALSE(IsSidecar(".hidden"));
	CHECK_FALSE(IsSidecar(""));
	CHECK_FALSE(IsSidecar("."));
}

/* ══════════════════════════════════════════════════════
   Phase 3 — Text Conversion
   ══════════════════════════════════════════════════════ */

TEST_CASE("MacRomanSizeFromUTF8File ASCII")
{
	auto p = writeTempFile("hello");
	CHECK(MacRomanSizeFromUTF8File(p) == 5);
	std::filesystem::remove(p);
}

TEST_CASE("MacRomanSizeFromUTF8File multibyte")
{
	auto p = writeTempFile("caf\xC3\xA9");
	CHECK(MacRomanSizeFromUTF8File(p) == 4);
	std::filesystem::remove(p);
}

TEST_CASE("MacRomanSizeFromUTF8File empty")
{
	auto p = writeTempFile("", "empty.txt");
	CHECK(MacRomanSizeFromUTF8File(p) == 0);
	std::filesystem::remove(p);
}

TEST_CASE("MacRomanFromUTF8File content")
{
	auto p = writeTempFile("caf\xC3\xA9");
	auto result = MacRomanFromUTF8File(p);
	REQUIRE(result.size() == 4);
	CHECK(result[0] == 'c');
	CHECK(result[1] == 'a');
	CHECK(result[2] == 'f');
	CHECK(result[3] == 0x8E); // é in Mac Roman
	std::filesystem::remove(p);
}

TEST_CASE("MacRomanFromUTF8File unmappable becomes ?")
{
	auto p = writeTempFile("\xF0\x9F\x98\x80");
	auto result = MacRomanFromUTF8File(p);
	REQUIRE(result.size() == 1);
	CHECK(result[0] == '?');
	std::filesystem::remove(p);
}

TEST_CASE("UTF8FromMacRoman ASCII")
{
	std::vector<uint8_t> input = {'H', 'i'};
	CHECK(UTF8FromMacRoman(input) == "Hi");
}

TEST_CASE("UTF8FromMacRoman high byte")
{
	std::vector<uint8_t> input = {0x8E};
	CHECK(UTF8FromMacRoman(input) == "\xC3\xA9");
}

TEST_CASE("UTF8FromMacRoman empty")
{
	CHECK(UTF8FromMacRoman({}) == "");
}

TEST_CASE("UTF8FromMacRoman round-trip all 256 bytes")
{
	for (int b = 0; b < 256; ++b)
	{
		std::vector<uint8_t> input = {static_cast<uint8_t>(b)};
		auto utf8 = UTF8FromMacRoman(input);
		auto fname = "rt_" + std::to_string(b) + ".txt";
		auto p = writeTempFile(utf8, fname);
		auto back = MacRomanFromUTF8File(p);
		REQUIRE(back.size() == 1);
		CHECK(back[0] == static_cast<uint8_t>(b));
		std::filesystem::remove(p);
	}
}

/* ── MacRomanFromUTF8 (string-level) ────────────────── */

TEST_CASE("MacRomanFromUTF8 ASCII")
{
	CHECK(MacRomanFromUTF8("Hello") == "Hello");
}

TEST_CASE("MacRomanFromUTF8 2-byte")
{
	// é is U+00E9, UTF-8 C3 A9 → MacRoman 0x8E
	auto result = MacRomanFromUTF8("\xC3\xA9");
	REQUIRE(result.size() == 1);
	CHECK(static_cast<uint8_t>(result[0]) == 0x8E);
}

TEST_CASE("MacRomanFromUTF8 3-byte")
{
	// ™ is U+2122, UTF-8 E2 84 A2 → MacRoman 0xAA
	auto result = MacRomanFromUTF8("\xE2\x84\xA2");
	REQUIRE(result.size() == 1);
	CHECK(static_cast<uint8_t>(result[0]) == 0xAA);
}

TEST_CASE("MacRomanFromUTF8 unmappable becomes ?")
{
	// 😀 U+1F600 is not in MacRoman
	auto result = MacRomanFromUTF8("\xF0\x9F\x98\x80");
	REQUIRE(result.size() == 1);
	CHECK(result[0] == '?');
}

TEST_CASE("MacRomanFromUTF8 round-trip all 256 bytes")
{
	for (int b = 0; b < 256; ++b)
	{
		std::vector<uint8_t> input = {static_cast<uint8_t>(b)};
		auto utf8 = UTF8FromMacRoman(input);
		auto mr = MacRomanFromUTF8(utf8);
		REQUIRE(mr.size() == 1);
		CHECK(static_cast<uint8_t>(mr[0]) == static_cast<uint8_t>(b));
	}
}

TEST_CASE("MacRomanFromUTF8 + UTF8FromMacRoman identity")
{
	for (int b = 0; b < 256; ++b)
	{
		std::vector<uint8_t> original = {static_cast<uint8_t>(b)};
		auto utf8 = UTF8FromMacRoman(original);
		auto mr = MacRomanFromUTF8(utf8);
		auto utf8_again =
			UTF8FromMacRoman({reinterpret_cast<const uint8_t *>(mr.data()), mr.size()});
		CHECK(utf8_again == utf8);
	}
}

/* ══════════════════════════════════════════════════════
   Phase 4 — Sidecar Binary Format
   ══════════════════════════════════════════════════════ */

TEST_CASE("parseSidecar rejects missing file")
{
	auto sc = detail::ParseSidecar("/nonexistent/._file");
	CHECK_FALSE(sc.valid);
}

TEST_CASE("parseSidecar rejects bad magic")
{
	auto p = std::filesystem::temp_directory_path() / "._bad_magic";
	{
		std::ofstream f(p, std::ios::binary);
		std::vector<uint8_t> data(30, 0);
		// Write wrong magic
		data[0] = 0x00;
		data[1] = 0x00;
		data[2] = 0x00;
		data[3] = 0x00;
		f.write(reinterpret_cast<const char *>(data.data()),
				static_cast<std::streamsize>(data.size()));
	}
	auto sc = detail::ParseSidecar(p);
	CHECK_FALSE(sc.valid);
	std::filesystem::remove(p);
}

TEST_CASE("parseSidecar rejects truncated header")
{
	auto p = std::filesystem::temp_directory_path() / "._truncated";
	{
		std::ofstream f(p, std::ios::binary);
		uint8_t data[10] = {};
		f.write(reinterpret_cast<const char *>(data), 10);
	}
	auto sc = detail::ParseSidecar(p);
	CHECK_FALSE(sc.valid);
	std::filesystem::remove(p);
}

TEST_CASE("writeSidecar Finder info only round-trips")
{
	auto p = std::filesystem::temp_directory_path() / "._fi_rt";
	FinderInfo fi{FourCC("APPL"), FourCC("myap"), 0x0100};
	detail::WriteSidecar(p, fi, std::nullopt);

	auto sc = detail::ParseSidecar(p);
	REQUIRE(sc.valid);
	CHECK(sc.HasFinderInfo());
	CHECK_FALSE(sc.HasResourceFork());
	auto parsed = detail::FinderInfoFromBlob(sc.finderInfoData);
	CHECK(parsed == fi);
	std::filesystem::remove(p);
}

TEST_CASE("writeSidecar resource fork only round-trips")
{
	auto p = std::filesystem::temp_directory_path() / "._rf_rt";
	std::vector<uint8_t> rsrc = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
	detail::WriteSidecar(p, std::nullopt, rsrc);

	auto sc = detail::ParseSidecar(p);
	REQUIRE(sc.valid);
	CHECK_FALSE(sc.HasFinderInfo());
	CHECK(sc.HasResourceFork());
	CHECK(sc.resourceForkData == rsrc);
	std::filesystem::remove(p);
}

TEST_CASE("writeSidecar both entries round-trips")
{
	auto p = std::filesystem::temp_directory_path() / "._both_rt";
	FinderInfo fi{FourCC("TEXT"), FourCC("ttxt"), 0};
	std::vector<uint8_t> rsrc = {1, 2, 3, 4, 5};
	detail::WriteSidecar(p, fi, rsrc);

	auto sc = detail::ParseSidecar(p);
	REQUIRE(sc.valid);
	CHECK(sc.HasFinderInfo());
	CHECK(sc.HasResourceFork());
	CHECK(detail::FinderInfoFromBlob(sc.finderInfoData) == fi);
	CHECK(sc.resourceForkData == rsrc);
	std::filesystem::remove(p);
}

TEST_CASE("parseSidecar skips unknown entry IDs")
{
	// Build a sidecar manually with an unknown entry ID
	auto p = std::filesystem::temp_directory_path() / "._unknown_id";
	{
		std::ofstream f(p, std::ios::binary);
		std::vector<uint8_t> data;

		// Header: magic + version + filler(16) + numEntries(1)
		uint8_t hdr[26] = {};
		detail::WriteBE32(hdr, kAppleDoubleMagic);
		detail::WriteBE32(hdr + 4, kAppleDoubleVersion);
		detail::WriteBE16(hdr + 24, 1);
		data.insert(data.end(), hdr, hdr + 26);

		// Entry descriptor for unknown ID=99, offset=38, length=4
		uint8_t desc[12] = {};
		detail::WriteBE32(desc, 99);
		detail::WriteBE32(desc + 4, 38);
		detail::WriteBE32(desc + 8, 4);
		data.insert(data.end(), desc, desc + 12);

		// Entry data
		data.push_back(0xAA);
		data.push_back(0xBB);
		data.push_back(0xCC);
		data.push_back(0xDD);

		f.write(reinterpret_cast<const char *>(data.data()),
				static_cast<std::streamsize>(data.size()));
	}
	auto sc = detail::ParseSidecar(p);
	CHECK(sc.valid);
	CHECK_FALSE(sc.HasFinderInfo());
	CHECK_FALSE(sc.HasResourceFork());
	CHECK(sc.entries.size() == 1);
	CHECK(sc.entries[0].id == 99);
	std::filesystem::remove(p);
}

/* ══════════════════════════════════════════════════════
   Phase 5 — Finder Info Get / Set
   ══════════════════════════════════════════════════════ */

TEST_CASE("GetFinderInfo with no sidecar returns extension default")
{
	// Ensure typemap is loaded
	LoadTypeMappings("data/debug/typemap.def");
	auto p = writeTempFile("hello", "fi_test.txt");
	auto fi = GetFinderInfo(p);
	CHECK(fi.type == FourCC("TEXT"));
	CHECK(fi.creator == FourCC("ttxt"));
	cleanup(p);
}

TEST_CASE("SetFinderInfo creates sidecar for non-default")
{
	auto p = writeTempFile("hello", "fi_create.txt");
	FinderInfo custom{FourCC("APPL"), FourCC("myap"), 0};
	SetFinderInfo(p, custom);
	CHECK(std::filesystem::exists(SidecarPathFor(p)));
	CHECK(GetFinderInfo(p) == custom);
	cleanup(p);
}

TEST_CASE("SetFinderInfo with default does not create sidecar")
{
	auto p = writeTempFile("hello", "fi_nosc.txt");
	auto def = FinderInfoFromExtension(".txt");
	SetFinderInfo(p, def);
	CHECK_FALSE(std::filesystem::exists(SidecarPathFor(p)));
	cleanup(p);
}

TEST_CASE("SetFinderInfo back to default removes sidecar")
{
	auto p = writeTempFile("hello", "fi_rm.txt");
	FinderInfo custom{FourCC("APPL"), FourCC("myap"), 0};
	SetFinderInfo(p, custom);
	REQUIRE(std::filesystem::exists(SidecarPathFor(p)));
	SetFinderInfo(p, FinderInfoFromExtension(".txt"));
	CHECK_FALSE(std::filesystem::exists(SidecarPathFor(p)));
	cleanup(p);
}

TEST_CASE("SetFinderInfo back to default keeps sidecar if resource fork exists")
{
	auto p = writeTempFile("hello", "fi_keep.txt");
	FinderInfo custom{FourCC("APPL"), FourCC("myap"), 0};
	SetFinderInfo(p, custom);
	std::vector<uint8_t> rsrc = {1, 2, 3};
	WriteResourceFork(p, 0, rsrc);

	// Set finder to default — sidecar should stay (has rsrc fork)
	SetFinderInfo(p, FinderInfoFromExtension(".txt"));
	CHECK(std::filesystem::exists(SidecarPathFor(p)));
	CHECK(ReadResourceFork(p, 0, 3) == rsrc);
	cleanup(p);
}

/* ══════════════════════════════════════════════════════
   Phase 6 — Resource Fork Access
   ══════════════════════════════════════════════════════ */

TEST_CASE("WriteResourceFork creates sidecar")
{
	auto p = writeTempFile("data", "rf_create.txt");
	std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
	WriteResourceFork(p, 0, data);
	CHECK(ResourceForkSize(p) == 10);
	cleanup(p);
}

TEST_CASE("ReadResourceFork matches written data")
{
	auto p = writeTempFile("data", "rf_read.txt");
	std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
	WriteResourceFork(p, 0, data);
	auto result = ReadResourceFork(p, 0, 4);
	CHECK(result == data);
	cleanup(p);
}

TEST_CASE("WriteResourceFork at offset grows fork")
{
	auto p = writeTempFile("data", "rf_grow.txt");
	std::vector<uint8_t> data = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
	WriteResourceFork(p, 20, data);
	CHECK(ResourceForkSize(p) == 25);
	auto gap = ReadResourceFork(p, 0, 20);
	CHECK(std::all_of(gap.begin(), gap.end(), [](uint8_t b) { return b == 0; }));
	CHECK(ReadResourceFork(p, 20, 5) == data);
	cleanup(p);
}

TEST_CASE("ReadResourceFork clamps to fork size")
{
	auto p = writeTempFile("data", "rf_clamp.txt");
	std::vector<uint8_t> data = {1, 2, 3};
	WriteResourceFork(p, 0, data);
	auto result = ReadResourceFork(p, 0, 100);
	CHECK(result.size() == 3);
	cleanup(p);
}

TEST_CASE("ReadResourceFork beyond fork returns empty")
{
	auto p = writeTempFile("data", "rf_beyond.txt");
	std::vector<uint8_t> data = {1, 2, 3};
	WriteResourceFork(p, 0, data);
	CHECK(ReadResourceFork(p, 100, 10).empty());
	cleanup(p);
}

TEST_CASE("ReadResourceFork no sidecar returns empty")
{
	auto p = writeTempFile("data", "rf_noscar.txt");
	CHECK(ReadResourceFork(p, 0, 10).empty());
	CHECK(ResourceForkSize(p) == 0);
	cleanup(p);
}

TEST_CASE("WriteResourceFork in-place overwrite")
{
	auto p = writeTempFile("data", "rf_inplace.txt");
	std::vector<uint8_t> initial(20, 0xFF);
	WriteResourceFork(p, 0, initial);
	std::vector<uint8_t> patch = {0x00, 0x00};
	WriteResourceFork(p, 5, patch);
	CHECK(ResourceForkSize(p) == 20);
	auto all = ReadResourceFork(p, 0, 20);
	CHECK(all[4] == 0xFF);
	CHECK(all[5] == 0x00);
	CHECK(all[6] == 0x00);
	CHECK(all[7] == 0xFF);
	cleanup(p);
}

TEST_CASE("SetResourceForkSize truncate")
{
	auto p = writeTempFile("data", "rf_trunc.txt");
	std::vector<uint8_t> data(100, 0xAA);
	WriteResourceFork(p, 0, data);
	SetResourceForkSize(p, 10);
	CHECK(ResourceForkSize(p) == 10);
	auto result = ReadResourceFork(p, 0, 10);
	CHECK(std::all_of(result.begin(), result.end(), [](uint8_t b) { return b == 0xAA; }));
	cleanup(p);
}

TEST_CASE("SetResourceForkSize to zero removes sidecar")
{
	auto p = writeTempFile("data", "rf_zero.txt");
	std::vector<uint8_t> data = {1, 2, 3};
	WriteResourceFork(p, 0, data);
	REQUIRE(std::filesystem::exists(SidecarPathFor(p)));
	SetResourceForkSize(p, 0);
	CHECK_FALSE(std::filesystem::exists(SidecarPathFor(p)));
	cleanup(p);
}

TEST_CASE("SetResourceForkSize to zero keeps sidecar with Finder override")
{
	auto p = writeTempFile("data", "rf_zero_fi.txt");
	std::vector<uint8_t> data = {1, 2, 3};
	WriteResourceFork(p, 0, data);
	FinderInfo custom{FourCC("APPL"), FourCC("myap"), 0};
	SetFinderInfo(p, custom);
	SetResourceForkSize(p, 0);
	CHECK(std::filesystem::exists(SidecarPathFor(p)));
	CHECK(ResourceForkSize(p) == 0);
	CHECK(GetFinderInfo(p) == custom);
	cleanup(p);
}

TEST_CASE("Finder info + resource fork interaction")
{
	auto p = writeTempFile("data", "rf_fi_both.txt");
	FinderInfo custom{FourCC("APPL"), FourCC("myap"), 0};
	SetFinderInfo(p, custom);
	std::vector<uint8_t> rsrc = {0xCA, 0xFE};
	WriteResourceFork(p, 0, rsrc);

	CHECK(GetFinderInfo(p) == custom);
	CHECK(ReadResourceFork(p, 0, 2) == rsrc);

	// Remove Finder info → resource fork stays
	SetFinderInfo(p, FinderInfoFromExtension(".txt"));
	CHECK(ReadResourceFork(p, 0, 2) == rsrc);
	CHECK(std::filesystem::exists(SidecarPathFor(p)));

	// Remove resource fork → sidecar gone (Finder is default now)
	SetResourceForkSize(p, 0);
	CHECK_FALSE(std::filesystem::exists(SidecarPathFor(p)));
	cleanup(p);
}

/* ══════════════════════════════════════════════════════
   Phase 7 — Date Handling, Lifecycle, GetFileInfo
   ══════════════════════════════════════════════════════ */

TEST_CASE("MacDateFromFileTime round-trips through SetModDate")
{
	auto p = writeTempFile("data", "date_rt.txt");
	uint32_t macDate = 3'600'000'000u;
	SetModDate(p, macDate);
	auto ft = std::filesystem::last_write_time(p);
	uint32_t result = MacDateFromFileTime(ft);
	CHECK(result >= macDate - 1);
	CHECK(result <= macDate + 1);
	cleanup(p);
}

TEST_CASE("DeleteWithSidecar removes file and sidecar")
{
	auto p = writeTempFile("data", "del_test.txt");
	FinderInfo custom{FourCC("APPL"), FourCC("myap"), 0};
	SetFinderInfo(p, custom);
	REQUIRE(std::filesystem::exists(p));
	REQUIRE(std::filesystem::exists(SidecarPathFor(p)));
	CHECK(DeleteWithSidecar(p));
	CHECK_FALSE(std::filesystem::exists(p));
	CHECK_FALSE(std::filesystem::exists(SidecarPathFor(p)));
}

TEST_CASE("DeleteWithSidecar file with no sidecar")
{
	auto p = writeTempFile("data", "del_nsc.txt");
	CHECK(DeleteWithSidecar(p));
	CHECK_FALSE(std::filesystem::exists(p));
}

TEST_CASE("DeleteWithSidecar empty directory")
{
	auto d = std::filesystem::temp_directory_path() / "del_emptydir_test";
	std::filesystem::create_directory(d);
	CHECK(DeleteWithSidecar(d));
	CHECK_FALSE(std::filesystem::exists(d));
}

TEST_CASE("DeleteWithSidecar non-empty directory fails")
{
	auto d = std::filesystem::temp_directory_path() / "del_fulldir_test";
	std::filesystem::create_directories(d);
	std::ofstream(d / "child.txt") << "x";
	CHECK_FALSE(DeleteWithSidecar(d));
	std::filesystem::remove_all(d);
}

TEST_CASE("RenameWithSidecar moves file and sidecar")
{
	auto old_p = writeTempFile("data", "ren_old.txt");
	FinderInfo custom{FourCC("APPL"), FourCC("myap"), 0};
	SetFinderInfo(old_p, custom);

	auto new_p = std::filesystem::temp_directory_path() / "ren_new.txt";
	CHECK(RenameWithSidecar(old_p, new_p));
	CHECK_FALSE(std::filesystem::exists(old_p));
	CHECK_FALSE(std::filesystem::exists(SidecarPathFor(old_p)));
	CHECK(std::filesystem::exists(new_p));
	CHECK(std::filesystem::exists(SidecarPathFor(new_p)));
	CHECK(GetFinderInfo(new_p) == custom);
	DeleteWithSidecar(new_p);
}

TEST_CASE("RenameWithSidecar file with no sidecar")
{
	auto old_p = writeTempFile("data", "ren_noscar.txt");
	auto new_p = std::filesystem::temp_directory_path() / "ren_noscar2.txt";
	CHECK(RenameWithSidecar(old_p, new_p));
	CHECK(std::filesystem::exists(new_p));
	CHECK_FALSE(std::filesystem::exists(SidecarPathFor(new_p)));
	std::filesystem::remove(new_p);
}

TEST_CASE("GetFileInfo non-TEXT file")
{
	auto p = writeTempFile("hello", "info_bin.jpg");
	auto info = GetFileInfo(p);
	CHECK(info.finder.type == FourCC("JPEG"));
	CHECK(info.dataForkSize == 5);
	CHECK(info.rsrcForkSize == 0);
	CHECK(info.isText == false);
	CHECK(info.modDate > 0);
	cleanup(p);
}

TEST_CASE("GetFileInfo TEXT file")
{
	auto p = writeTempFile("caf\xC3\xA9", "info_text.txt");
	auto info = GetFileInfo(p);
	CHECK(info.finder.type == FourCC("TEXT"));
	CHECK(info.dataForkSize == 4);
	CHECK(info.isText == true);
	cleanup(p);
}

TEST_CASE("GetFileInfo with sidecar")
{
	auto p = writeTempFile("hello", "info_scar.txt");
	FinderInfo custom{FourCC("APPL"), FourCC("myap"), 0};
	SetFinderInfo(p, custom);
	std::vector<uint8_t> rsrc = {1, 2, 3};
	WriteResourceFork(p, 0, rsrc);

	auto info = GetFileInfo(p);
	CHECK(info.finder == custom);
	CHECK(info.rsrcForkSize == 3);
	CHECK(info.dataForkSize == 5);
	CHECK(info.isText == false);
	DeleteWithSidecar(p);
}
