/*
	test_model_defs.cpp

	Tests for the constexpr ModelDef table and lookup functions.
*/

#include "doctest/doctest.h"
#include "core/model_defs.h"
#include "core/config_loader.h"

TEST_CASE("ModelDef table has 12 entries")
{
	CHECK(kModelDefs.size() == 12);
}

TEST_CASE("ModelDef table covers all MacModel values")
{
	constexpr MacModel allModels[] = {
		MacModel::Twig43,  MacModel::Twiggy, MacModel::Mac128K, MacModel::Mac512Ke,
		MacModel::Kanji,   MacModel::Plus,	 MacModel::SE,		MacModel::SEFDHD,
		MacModel::Classic, MacModel::PB100,	 MacModel::II,		MacModel::IIx,
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
		"Twig43", "Twiggy", "128K",	   "512Ke", "Kanji", "Plus",
		"SE",	  "SEFDHD", "Classic", "PB100", "II",	 "IIx",
	};
	for (const char *s : slugs)
	{
		INFO("slug: " << s);
		const ModelDef *def = ModelDefForSlug(s);
		REQUIRE(def != nullptr);
	}
}

// --- ParseModelName and ModelToString tests ---

TEST_CASE("ParseModelName accepts slug")
{
	MacModel m;
	CHECK(ParseModelName("Plus", m));
	CHECK(m == MacModel::Plus);
	CHECK(ParseModelName("II", m));
	CHECK(m == MacModel::II);
	CHECK(ParseModelName("IIx", m));
	CHECK(m == MacModel::IIx);
}

TEST_CASE("ParseModelName accepts legacy aliases")
{
	MacModel m;
	CHECK(ParseModelName("plus", m));
	CHECK(m == MacModel::Plus);
	CHECK(ParseModelName("se", m));
	CHECK(m == MacModel::SE);
	CHECK(ParseModelName("128k", m));
	CHECK(m == MacModel::Mac128K);
	CHECK(ParseModelName("powerbook100", m));
	CHECK(m == MacModel::PB100);
	CHECK(ParseModelName("kanji", m));
	CHECK(m == MacModel::Kanji);
}

TEST_CASE("ParseModelName rejects unknown")
{
	MacModel m;
	CHECK_FALSE(ParseModelName("Amiga", m));
	CHECK_FALSE(ParseModelName("", m));
}

TEST_CASE("ModelToString round-trips")
{
	constexpr MacModel allModels[] = {
		MacModel::Twig43,  MacModel::Twiggy, MacModel::Mac128K, MacModel::Mac512Ke,
		MacModel::Kanji,   MacModel::Plus,	 MacModel::SE,		MacModel::SEFDHD,
		MacModel::Classic, MacModel::PB100,	 MacModel::II,		MacModel::IIx,
	};
	for (auto m : allModels)
	{
		const char *str = ModelToString(m);
		INFO("Model: " << str);
		MacModel parsed;
		REQUIRE(ParseModelName(str, parsed));
		CHECK(parsed == m);
	}
}
