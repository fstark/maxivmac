/*
	Localization — runtime language selection
*/

#pragma once

/* --- Available languages --- */

enum Language {
	kLangEnglish,
	kLangFrench,
	kLangGerman,
	kLangItalian,
	kLangSpanish,
	kLangDutch,
	kLangPortuguese,
	kLangPolish,
	kLangCzech,
	kLangSerbian,
	kLangCatalan,

	kLangCount
};

/* --- Localization API --- */

// Return the current UI language.
Language GetLanguage();

// Switch all future Localize() lookups to the given language.
void SetLanguage(Language lang);

// Look up a translated string for the current language.
// Returns the key itself as fallback if no translation exists.
const char *Localize(const char *key);

/* --- Language metadata --- */

// ISO 639-1 code → Language enum.  Returns false if unknown.
bool LanguageFromISO(const char *code, Language &out);

// Language enum → ISO 639-1 code (e.g. "en", "fr").  Never null.
const char *ISOFromLanguage(Language lang);

// Language enum → English name (e.g. "English", "French").  Never null.
const char *NameFromLanguage(Language lang);
