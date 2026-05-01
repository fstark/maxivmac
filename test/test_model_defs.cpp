/*
	test_model_defs.cpp

	Tests for the constexpr ModelDef table and lookup functions.
*/

#include "doctest/doctest.h"
#include "core/model_defs.h"

// Declared in machine_config.cpp — preserved for round-trip testing
extern MachineConfig OldMachineConfigForModel(MacModel model);

TEST_CASE("ModelDef table has 12 entries")
{
	CHECK(kModelDefs.size() == 12);
}

TEST_CASE("ModelDef table covers all MacModel values")
{
	constexpr MacModel allModels[] = {
		MacModel::Twig43, MacModel::Twiggy, MacModel::Mac128K,
		MacModel::Mac512Ke, MacModel::Kanji, MacModel::Plus,
		MacModel::SE, MacModel::SEFDHD, MacModel::Classic,
		MacModel::PB100, MacModel::II, MacModel::IIx,
	};
	for (auto m : allModels)
	{
		const ModelDef *def = ModelDefFor(m);
		REQUIRE(def != nullptr);
		CHECK(def->id == m);
	}
}

TEST_CASE("ModelDefFor returns correct entry for Plus")
{
	const ModelDef *def = ModelDefFor(MacModel::Plus);
	REQUIRE(def != nullptr);
	CHECK(def->id == MacModel::Plus);
	CHECK(def->name == "MacPlus");
	CHECK(def->slug == "Plus");
	CHECK(def->use68020 == false);
	CHECK(def->rom.size == 0x00020000);
	CHECK(def->rom.base == 0x00400000);
	CHECK(def->rom.filename == "MacPlus.ROM");
	CHECK(def->screen.width == 512);
	CHECK(def->screen.height == 342);
	CHECK(def->screen.depth == 0);
	CHECK(def->extnBlockBase == 0x00F40000);
	CHECK(def->clockMult == 1);
}

TEST_CASE("ModelDefFor returns correct entry for II")
{
	const ModelDef *def = ModelDefFor(MacModel::II);
	REQUIRE(def != nullptr);
	CHECK(def->id == MacModel::II);
	CHECK(def->name == "MacII");
	CHECK(def->slug == "II");
	CHECK(def->use68020 == true);
	CHECK(def->emFPU == true);
	CHECK(def->emVIA2 == true);
	CHECK(def->rom.size == 0x00040000);
	CHECK(def->rom.base == 0x00800000);
	CHECK(def->screen.width == 640);
	CHECK(def->screen.height == 480);
	CHECK(def->screen.depth == 3);
	CHECK(def->clockMult == 2);
	CHECK(def->maxATTListN == 20);
	CHECK(def->ramBSize == 0x00400000);
}

TEST_CASE("ModelDefForSlug finds Plus")
{
	const ModelDef *def = ModelDefForSlug("Plus");
	REQUIRE(def != nullptr);
	CHECK(def->id == MacModel::Plus);
}

TEST_CASE("ModelDefForSlug is case-insensitive")
{
	const ModelDef *def = ModelDefForSlug("plus");
	REQUIRE(def != nullptr);
	CHECK(def->id == MacModel::Plus);

	def = ModelDefForSlug("PLUS");
	REQUIRE(def != nullptr);
	CHECK(def->id == MacModel::Plus);
}

TEST_CASE("ModelDefForSlug returns nullptr for unknown")
{
	CHECK(ModelDefForSlug("Amiga") == nullptr);
	CHECK(ModelDefForSlug("") == nullptr);
	CHECK(ModelDefForSlug("PowerMac") == nullptr);
}

TEST_CASE("ModelDefForSlug finds all models by slug")
{
	const char *slugs[] = {
		"Twig43", "Twiggy", "128K", "512Ke", "Kanji", "Plus",
		"SE", "SEFDHD", "Classic", "PB100", "II", "IIx",
	};
	for (const char *s : slugs)
	{
		INFO("slug: " << s);
		const ModelDef *def = ModelDefForSlug(s);
		REQUIRE(def != nullptr);
	}
}

// --- Round-trip regression: new table-driven vs old switch-based ---

static void checkVIAConfigEqual(const VIAConfig &a, const VIAConfig &b, const char *label)
{
	INFO(label);
	CHECK(a.oraFloatVal == b.oraFloatVal);
	CHECK(a.orbFloatVal == b.orbFloatVal);
	CHECK(a.oraCanIn == b.oraCanIn);
	CHECK(a.oraCanOut == b.oraCanOut);
	CHECK(a.orbCanIn == b.orbCanIn);
	CHECK(a.orbCanOut == b.orbCanOut);
	CHECK(a.ierNever0 == b.ierNever0);
	CHECK(a.ierNever1 == b.ierNever1);
	CHECK(a.cb2ModesAllowed == b.cb2ModesAllowed);
	CHECK(a.ca2ModesAllowed == b.ca2ModesAllowed);
	CHECK(a.portAWires == b.portAWires);
	CHECK(a.portBWires == b.portBWires);
	CHECK(a.cb2Wire == b.cb2Wire);
	CHECK(a.interruptWire == b.interruptWire);
}

TEST_CASE("ModelDef produces identical MachineConfig")
{
	constexpr MacModel allModels[] = {
		MacModel::Twig43, MacModel::Twiggy, MacModel::Mac128K,
		MacModel::Mac512Ke, MacModel::Kanji, MacModel::Plus,
		MacModel::SE, MacModel::SEFDHD, MacModel::Classic,
		MacModel::PB100, MacModel::II, MacModel::IIx,
	};

	for (auto m : allModels)
	{
		MachineConfig oldC = OldMachineConfigForModel(m);
		MachineConfig newC = MachineConfigForModel(m);
		const char *name = ModelDefFor(m)->name.data();
		INFO("Model: " << name);

		CHECK(oldC.model == newC.model);
		CHECK(oldC.use68020 == newC.use68020);
		CHECK(oldC.emFPU == newC.emFPU);
		CHECK(oldC.emMMU == newC.emMMU);
		CHECK(oldC.ramASize == newC.ramASize);
		CHECK(oldC.ramBSize == newC.ramBSize);
		CHECK(oldC.romSize == newC.romSize);
		CHECK(oldC.romBase == newC.romBase);
		CHECK(std::string(oldC.romFileName) == std::string(newC.romFileName));
		CHECK(oldC.extnBlockBase == newC.extnBlockBase);
		CHECK(oldC.extnLn2Spc == newC.extnLn2Spc);
		CHECK(oldC.emVIA1 == newC.emVIA1);
		CHECK(oldC.emVIA2 == newC.emVIA2);
		CHECK(oldC.emADB == newC.emADB);
		CHECK(oldC.emClassicKbrd == newC.emClassicKbrd);
		CHECK(oldC.emRTC == newC.emRTC);
		CHECK(oldC.emPMU == newC.emPMU);
		CHECK(oldC.emASC == newC.emASC);
		CHECK(oldC.emClassicSnd == newC.emClassicSnd);
		CHECK(oldC.emVidCard == newC.emVidCard);
		CHECK(oldC.includeVidMem == newC.includeVidMem);
		CHECK(oldC.vidMemSize == newC.vidMemSize);
		CHECK(oldC.vidROMSize == newC.vidROMSize);
		CHECK(oldC.clockMult == newC.clockMult);
		CHECK(oldC.autoSlowSubTicks == newC.autoSlowSubTicks);
		CHECK(oldC.autoSlowTime == newC.autoSlowTime);
		CHECK(oldC.maxATTListN == newC.maxATTListN);
		CHECK(oldC.screenWidth == newC.screenWidth);
		CHECK(oldC.screenHeight == newC.screenHeight);
		CHECK(oldC.screenDepth == newC.screenDepth);

		checkVIAConfigEqual(oldC.via1Config, newC.via1Config, "VIA1");
		checkVIAConfigEqual(oldC.via2Config, newC.via2Config, "VIA2");
	}
}
