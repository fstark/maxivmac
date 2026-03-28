/*
	Localization — runtime language selection implementation
*/

#include <cstdio>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "lang/localization.h"
#include "lang/localization_impl.h"

/* --- Extern references to per-language pair arrays --- */

extern const LangPair kLangPairsEnglish[];
extern const LangPair kLangPairsFrench[];
extern const LangPair kLangPairsGerman[];
extern const LangPair kLangPairsItalian[];
extern const LangPair kLangPairsSpanish[];
extern const LangPair kLangPairsDutch[];
extern const LangPair kLangPairsPortuguese[];
extern const LangPair kLangPairsPolish[];
extern const LangPair kLangPairsCzech[];
extern const LangPair kLangPairsSerbian[];
extern const LangPair kLangPairsCatalan[];

/* --- Language table index --- */

static const LangPair *g_langTables[kLangCount] = {
	kLangPairsEnglish,
	kLangPairsFrench,
	kLangPairsGerman,
	kLangPairsItalian,
	kLangPairsSpanish,
	kLangPairsDutch,
	kLangPairsPortuguese,
	kLangPairsPolish,
	kLangPairsCzech,
	kLangPairsSerbian,
	kLangPairsCatalan,
};

/* --- State --- */

static Language g_currentLang = kLangEnglish;
static std::unordered_map<std::string, const char *> g_strings;
static std::unordered_set<std::string> g_warnedKeys;
static bool g_initialized = false;

/* --- Language metadata tables --- */

struct LangMeta {
	const char *iso;
	const char *name;
};

static const LangMeta g_langMeta[kLangCount] = {
	{"en", "English"},
	{"fr", "French"},
	{"de", "German"},
	{"it", "Italian"},
	{"es", "Spanish"},
	{"nl", "Dutch"},
	{"pt", "Portuguese"},
	{"pl", "Polish"},
	{"cs", "Czech"},
	{"sr", "Serbian"},
	{"ca", "Catalan"},
};

// Map a 2-char ISO 639 code to the Language enum.
bool LanguageFromISO(const char *code, Language &out)
{
	for (int i = 0; i < kLangCount; ++i) {
		// Case-insensitive compare of 2-char codes
		if (g_langMeta[i].iso[0] == (code[0] | 0x20) &&
		    g_langMeta[i].iso[1] == (code[1] | 0x20) &&
		    code[2] == '\0') {
			out = static_cast<Language>(i);
			return true;
		}
	}
	return false;
}

const char *ISOFromLanguage(Language lang)
{
	if (lang >= 0 && lang < kLangCount)
		return g_langMeta[lang].iso;
	return "??";
}

const char *NameFromLanguage(Language lang)
{
	if (lang >= 0 && lang < kLangCount)
		return g_langMeta[lang].name;
	return "Unknown";
}

/* --- Implementation --- */

static void PopulateMap(const LangPair *pairs)
{
	g_strings.clear();
	for (const LangPair *p = pairs; p->key != nullptr; ++p) {
		g_strings[p->key] = p->value;
	}
}

/* Switch the active language: repopulate the string map from
   the language table and warn about missing translations. */
void SetLanguage(Language lang)
{
	if (lang < 0 || lang >= kLangCount) {
		return;
	}
	g_currentLang = lang;
	PopulateMap(g_langTables[lang]);
	g_initialized = true;
	g_warnedKeys.clear();

	/* Warn about keys present in English but missing in the selected language */
	if (lang != kLangEnglish) {
		for (const LangPair *p = g_langTables[kLangEnglish]; p->key != nullptr; ++p) {
			if (g_strings.find(p->key) == g_strings.end()) {
				std::fprintf(stderr, "Warning: lang '%s' missing key '%s'\n",
					ISOFromLanguage(lang), p->key);
			}
		}
	}
}

Language GetLanguage()
{
	return g_currentLang;
}

/* Look up a translated string by key, falling back to the
   key itself if no translation exists. Warns once per miss. */
const char *Localize(const char *key)
{
	if (!g_initialized) {
		SetLanguage(kLangEnglish);
	}
	auto it = g_strings.find(key);
	if (it != g_strings.end()) {
		return it->second;
	}
	/* Warn once per missing key at runtime */
	if (g_warnedKeys.insert(key).second) {
		std::fprintf(stderr, "Warning: lang '%s' no translation for '%s'\n",
			ISOFromLanguage(g_currentLang), key);
	}
	/* fallback: return the key itself */
	return key;
}
