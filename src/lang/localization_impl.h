/*
	Localization implementation details — not part of the public API.
	Included by localization.cpp and lang_*.cpp data files only.
*/

#pragma once

/* --- Language pair (key + translated value) --- */

struct LangPair {
	const char *key;
	const char *value;
};
