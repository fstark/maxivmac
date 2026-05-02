/*
	model_defs.h

	Constexpr table of hardware facts for all 12 Mac models.
	Authoritative source — MachineConfigForModel() reads from this.
*/

#pragma once

#include "core/machine_config.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <string_view>

struct RomDef
{
	std::string_view filename;
	uint32_t size;
	uint32_t base;
	std::string_view md5; // expected MD5 hex string (lowercase)
};

struct ScreenDef
{
	uint16_t width;
	uint16_t height;
	uint8_t depth; // log2 bpp: 0=1bpp, 3=8bpp
};

struct ModelDef
{
	MacModel id;
	std::string_view name;
	std::string_view slug; // CLI / .mac file key

	// CPU
	bool use68020;
	bool emFPU;
	bool emMMU;

	// Memory
	uint32_t ramASize;
	uint32_t ramBSize;

	// ROM
	RomDef rom;

	// Screen
	ScreenDef screen;

	// Devices
	bool emVIA1;
	bool emVIA2;
	bool emADB;
	bool emClassicKbrd;
	bool emRTC;
	bool emPMU;
	bool emASC;
	bool emClassicSnd;
	bool emVidCard;
	bool includeVidMem;
	uint32_t vidMemSize;
	uint32_t vidROMSize;

	// Extension space
	uint32_t extnBlockBase;
	uint8_t extnLn2Spc;

	// Timing
	int clockMult;
	int autoSlowSubTicks;
	int autoSlowTime;
	int maxATTListN;
};

// clang-format off
inline constexpr std::array<ModelDef, 12> kModelDefs = {{
	// --- Twig43 ---
	{
		MacModel::Twig43, "Twig43", "Twig43",
		false, false, false,                         // CPU
		0x00020000, 0,                               // RAM: 128K
		{"Twig43.ROM", 0x00010000, 0x00400000,
		 "e4faf3ecb169b875a1e66abbd5306b52"},         // ROM: 64KB
		{512, 342, 0},                               // Screen: 1bpp
		true, false, false, true, true, false,        // VIA1, no VIA2, no ADB, classic kbd, RTC, no PMU
		false, true,                                  // no ASC, classic sound
		false, false, 0, 0,                           // no vid card
		0x00F0C000, 7,                               // extn
		1, 16384, 60, 16                             // timing
	},
	// --- Twiggy ---
	{
		MacModel::Twiggy, "Twiggy", "Twiggy",
		false, false, false,
		0x00020000, 0,
		{"Twiggy.ROM", 0x00010000, 0x00400000,
		 "4f28b54a2c6d699b596a1e6072a57f58"},
		{512, 342, 0},
		true, false, false, true, true, false,
		false, true,
		false, false, 0, 0,
		0x00F0C000, 7,
		1, 16384, 60, 16
	},
	// --- Mac128K ---
	{
		MacModel::Mac128K, "Mac128K", "128K",
		false, false, false,
		0x00020000, 0,
		{"Mac128K.ROM", 0x00010000, 0x00400000,
		 "db7e6d3205a2b48023fba5aa867ac6d6"},
		{512, 342, 0},
		true, false, false, true, true, false,
		false, true,
		false, false, 0, 0,
		0x00F0C000, 7,
		1, 16384, 60, 16
	},
	// --- Mac512Ke ---
	{
		MacModel::Mac512Ke, "Mac512Ke", "512Ke",
		false, false, false,
		0x00080000, 0,                               // RAM: 512K
		{"Mac512Ke.ROM", 0x00020000, 0x00400000,
		 "8a41e0754ffd1bb00d8183875c55164c"},         // ROM: 128KB
		{512, 342, 0},
		true, false, false, true, true, false,
		false, true,
		false, false, 0, 0,
		0x00F0C000, 7,
		1, 16384, 60, 16
	},
	// --- Kanji ---
	{
		MacModel::Kanji, "MacPlusKanji", "Kanji",
		false, false, false,
		0x00400000, 0,                               // RAM: 4MB
		{"MacPlusKanji.ROM", 0x00040000, 0x00400000,
		 "56737a4960e70635e310db0a7fb5332c"},         // ROM: 256KB
		{512, 342, 0},
		true, false, false, true, true, false,
		false, true,
		false, false, 0, 0,
		0x00F0C000, 7,
		1, 16384, 60, 16
	},
	// --- Plus ---
	{
		MacModel::Plus, "MacPlus", "Plus",
		false, false, false,
		0x00400000, 0,                               // RAM: 4MB
		{"MacPlus.ROM", 0x00020000, 0x00400000,
		 "8a41e0754ffd1bb00d8183875c55164c"},         // ROM: 128KB
		{512, 342, 0},
		true, false, false, true, true, false,
		false, true,
		false, false, 0, 0,
		0x00F40000, 7,                               // Plus has different extn base
		1, 16384, 60, 16
	},
	// --- SE ---
	{
		MacModel::SE, "MacSE", "SE",
		false, false, false,
		0x00400000, 0,                               // RAM: 4MB
		{"MacSE.ROM", 0x00040000, 0x00400000,
		 "9fb38bdcc0d53d9d380897ee53dc1322"},         // ROM: 256KB
		{512, 342, 0},
		true, false, true, false, true, false,        // VIA1, no VIA2, ADB, no classic kbd
		false, true,                                  // no ASC, classic sound
		false, false, 0, 0,
		0x00F0C000, 7,
		1, 16384, 60, 16
	},
	// --- SEFDHD ---
	{
		MacModel::SEFDHD, "SEFDHD", "SEFDHD",
		false, false, false,
		0x00400000, 0,
		{"SEFDHD.ROM", 0x00040000, 0x00400000,
		 "886444d7abc1185112391b8656c7e448"},
		{512, 342, 0},
		true, false, true, false, true, false,
		false, true,
		false, false, 0, 0,
		0x00F0C000, 7,
		1, 16384, 60, 16
	},
	// --- Classic ---
	{
		MacModel::Classic, "Classic", "Classic",
		false, false, false,
		0x00400000, 0,
		{"Classic.ROM", 0x00080000, 0x00400000,
		 "c229bb677cb41b84b780c9e38a09173e"},         // ROM: 512KB
		{512, 342, 0},
		true, false, true, false, true, false,
		false, true,
		false, false, 0, 0,
		0x00F0C000, 7,
		1, 16384, 60, 16
	},
	// --- PB100 ---
	{
		MacModel::PB100, "PB100", "PB100",
		false, false, false,
		0x00400000, 0,
		{"PB100.ROM", 0x00040000, 0x00900000,
		 "dd390f7c86a730caac46fd522f8b2665"},         // ROM: 256KB, base 0x900000
		{640, 400, 0},                               // PB100 has 640x400 screen
		true, false, false, false, false, true,       // VIA1, no VIA2, no ADB, no classic kbd, no RTC, PMU
		true, false,                                  // ASC, no classic sound
		false, true, 0x00008000, 0,                   // no vid card, but includeVidMem=true, 32KB
		0x00F40000, 7,                               // PB100 extn base
		1, 16384, 60, 16
	},
	// --- II ---
	{
		MacModel::II, "MacII", "II",
		true, true, false,                            // 68020, FPU, no MMU
		0x00400000, 0x00400000,                       // RAM: 4MB+4MB
		{"MacII.ROM", 0x00040000, 0x00800000,
		 "2a8a4c7f2a38e0ab0771f59a9a0f1ee4"},         // ROM: 256KB, base 0x800000
		{640, 480, 3},                               // 8bpp
		true, true, true, false, true, false,         // VIA1, VIA2, ADB, no classic kbd, RTC, no PMU
		true, false,                                  // ASC, no classic sound
		true, true, 0x00080000, 0x002000,             // vid card, vid mem 512KB, vid ROM 8KB
		0x50F0C000, 7,                               // Mac II extn base
		2, 16384, 60, 20                             // clockMult=2, maxATTListN=20
	},
	// --- IIx ---
	{
		MacModel::IIx, "MacIIx", "IIx",
		true, true, false,                            // 68030 (use68020=true), FPU, no MMU
		0x00400000, 0x00400000,
		{"MacIIx.ROM", 0x00040000, 0x00800000,
		 "2a8a4c7f2a38e0ab0771f59a9a0f1ee4"},
		{640, 480, 3},
		true, true, true, false, true, false,
		true, false,
		true, true, 0x00080000, 0x002000,
		0x50F0C000, 7,
		2, 16384, 60, 20
	},
}};
// clang-format on

// Lookup by MacModel enum. Returns nullptr if not found (should never happen).
constexpr const ModelDef *ModelDefFor(MacModel model)
{
	for (const auto &def : kModelDefs)
	{
		if (def.id == model) return &def;
	}
	return nullptr;
}

// Lookup by slug (case-insensitive). Implemented in model_defs.cpp.
const ModelDef *ModelDefForSlug(std::string_view slug);

// --- Static assertions for critical fields ---

// Table has the right size
static_assert(kModelDefs.size() == 12, "kModelDefs must have 12 entries");

// Verify every model has correct id, rom size/base, CPU, and screen
static_assert(kModelDefs[0].id == MacModel::Twig43);
static_assert(kModelDefs[0].rom.size == 0x00010000);
static_assert(kModelDefs[0].rom.base == 0x00400000);
static_assert(kModelDefs[0].use68020 == false);
static_assert(kModelDefs[0].screen.width == 512);
static_assert(kModelDefs[0].screen.height == 342);

static_assert(kModelDefs[1].id == MacModel::Twiggy);
static_assert(kModelDefs[1].rom.size == 0x00010000);
static_assert(kModelDefs[1].rom.base == 0x00400000);
static_assert(kModelDefs[1].use68020 == false);
static_assert(kModelDefs[1].screen.width == 512);

static_assert(kModelDefs[2].id == MacModel::Mac128K);
static_assert(kModelDefs[2].rom.size == 0x00010000);
static_assert(kModelDefs[2].rom.base == 0x00400000);
static_assert(kModelDefs[2].use68020 == false);

static_assert(kModelDefs[3].id == MacModel::Mac512Ke);
static_assert(kModelDefs[3].rom.size == 0x00020000);
static_assert(kModelDefs[3].ramASize == 0x00080000);

static_assert(kModelDefs[4].id == MacModel::Kanji);
static_assert(kModelDefs[4].rom.size == 0x00040000);
static_assert(kModelDefs[4].ramASize == 0x00400000);

static_assert(kModelDefs[5].id == MacModel::Plus);
static_assert(kModelDefs[5].rom.size == 0x00020000);
static_assert(kModelDefs[5].rom.base == 0x00400000);
static_assert(kModelDefs[5].extnBlockBase == 0x00F40000);

static_assert(kModelDefs[6].id == MacModel::SE);
static_assert(kModelDefs[6].rom.size == 0x00040000);
static_assert(kModelDefs[6].emADB == true);
static_assert(kModelDefs[6].emClassicKbrd == false);

static_assert(kModelDefs[7].id == MacModel::SEFDHD);
static_assert(kModelDefs[7].rom.size == 0x00040000);
static_assert(kModelDefs[7].emADB == true);

static_assert(kModelDefs[8].id == MacModel::Classic);
static_assert(kModelDefs[8].rom.size == 0x00080000);
static_assert(kModelDefs[8].emADB == true);

static_assert(kModelDefs[9].id == MacModel::PB100);
static_assert(kModelDefs[9].rom.base == 0x00900000);
static_assert(kModelDefs[9].emPMU == true);
static_assert(kModelDefs[9].emRTC == false);
static_assert(kModelDefs[9].screen.width == 640);
static_assert(kModelDefs[9].screen.height == 400);

static_assert(kModelDefs[10].id == MacModel::II);
static_assert(kModelDefs[10].use68020 == true);
static_assert(kModelDefs[10].emFPU == true);
static_assert(kModelDefs[10].emVIA2 == true);
static_assert(kModelDefs[10].rom.base == 0x00800000);
static_assert(kModelDefs[10].screen.width == 640);
static_assert(kModelDefs[10].screen.height == 480);
static_assert(kModelDefs[10].screen.depth == 3);
static_assert(kModelDefs[10].clockMult == 2);
static_assert(kModelDefs[10].maxATTListN == 20);

static_assert(kModelDefs[11].id == MacModel::IIx);
static_assert(kModelDefs[11].use68020 == true);
static_assert(kModelDefs[11].emFPU == true);
static_assert(kModelDefs[11].emVIA2 == true);
static_assert(kModelDefs[11].rom.base == 0x00800000);
static_assert(kModelDefs[11].clockMult == 2);
static_assert(kModelDefs[11].maxATTListN == 20);

// Verify ModelDefFor works at compile time for key models
static_assert(ModelDefFor(MacModel::Plus)->rom.size == 0x00020000);
static_assert(ModelDefFor(MacModel::II)->use68020 == true);
static_assert(ModelDefFor(MacModel::PB100)->emPMU == true);
