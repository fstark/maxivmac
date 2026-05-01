/*
	model_defs.cpp

	Runtime lookup by slug (case-insensitive).
*/

#include "core/model_defs.h"
#include <algorithm>
#include <cctype>
#include <string>

const ModelDef *ModelDefForSlug(std::string_view slug)
{
	auto toLowerChar = [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); };

	// Build a lowercase copy of the query
	std::string lower(slug.size(), '\0');
	std::transform(slug.begin(), slug.end(), lower.begin(), toLowerChar);

	for (const auto &def : kModelDefs)
	{
		// Compare against slug (lowercase)
		std::string defSlug(def.slug.size(), '\0');
		std::transform(def.slug.begin(), def.slug.end(), defSlug.begin(), toLowerChar);
		if (lower == defSlug) return &def;
	}
	return nullptr;
}
