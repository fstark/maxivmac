/*
	symbols.cpp — Symbol lookup implementation

	Builds sorted indexes over the trap dictionary and low-memory
	global tables for fast name and address lookup.
*/

#include "debugger/symbols.h"

#include "cpu/trap_counter.h"
#include "platform/lomem_globals.h"

#include <algorithm>
#include <cctype>
#include <cstring>

/* ── Sorted index over low-memory globals ──────────── */

struct GlobalByName
{
	const LMGlobal *g;
};

struct GlobalByAddr
{
	const LMGlobal *g;
};

static std::vector<GlobalByName> s_globalsByName;
static std::vector<GlobalByAddr> s_globalsByAddr;
static bool s_initialized = false;

static int CaseInsensitiveCmp(std::string_view a, std::string_view b)
{
	auto len = std::min(a.size(), b.size());
	for (size_t i = 0; i < len; ++i)
	{
		int ca = std::tolower(static_cast<unsigned char>(a[i]));
		int cb = std::tolower(static_cast<unsigned char>(b[i]));
		if (ca != cb) return ca - cb;
	}
	if (a.size() < b.size()) return -1;
	if (a.size() > b.size()) return 1;
	return 0;
}

static bool CaseInsensitiveEqual(std::string_view a, std::string_view b)
{
	return CaseInsensitiveCmp(a, b) == 0;
}

static bool CaseInsensitiveStartsWith(std::string_view str, std::string_view prefix)
{
	if (prefix.size() > str.size()) return false;
	return CaseInsensitiveCmp(str.substr(0, prefix.size()), prefix) == 0;
}

void SymbolsInit()
{
	if (s_initialized) return;
	s_initialized = true;

	/* Build globals-by-name index */
	s_globalsByName.reserve(kLowMemCount);
	for (int i = 0; i < kLowMemCount; ++i)
	{
		s_globalsByName.push_back({&kLowMemGlobals[i]});
	}
	std::sort(s_globalsByName.begin(), s_globalsByName.end(),
			  [](const GlobalByName &a, const GlobalByName &b)
			  { return CaseInsensitiveCmp(a.g->name, b.g->name) < 0; });

	/* Build globals-by-address index */
	s_globalsByAddr.reserve(kLowMemCount);
	for (int i = 0; i < kLowMemCount; ++i)
	{
		s_globalsByAddr.push_back({&kLowMemGlobals[i]});
	}
	std::sort(s_globalsByAddr.begin(), s_globalsByAddr.end(),
			  [](const GlobalByAddr &a, const GlobalByAddr &b) { return a.g->addr < b.g->addr; });
}

int SymbolsTrapCount()
{
	return trap_dict_size();
}

int SymbolsGlobalCount()
{
	return kLowMemCount;
}

bool SymbolsResolve(std::string_view name, uint32_t &outAddr, uint16_t &outTrapWord)
{
	/* Try trap dictionary first (exact, case-insensitive) */
	std::vector<TrapInfo> results;
	trap_dict_search(std::string(name).c_str(), results, 100);
	for (auto &ti : results)
	{
		if (CaseInsensitiveEqual(ti.name, name))
		{
			outAddr = 0;
			outTrapWord = ti.trapWord;
			return true;
		}
	}

	/* Binary search the globals-by-name index */
	auto it = std::lower_bound(s_globalsByName.begin(), s_globalsByName.end(), name,
							   [](const GlobalByName &entry, std::string_view n)
							   { return CaseInsensitiveCmp(entry.g->name, n) < 0; });
	if (it != s_globalsByName.end() && CaseInsensitiveEqual(it->g->name, name))
	{
		outAddr = it->g->addr;
		outTrapWord = 0;
		return true;
	}

	return false;
}

std::string_view SymbolsAtAddress(uint32_t addr)
{
	/* Binary search globals-by-address */
	auto it =
		std::lower_bound(s_globalsByAddr.begin(), s_globalsByAddr.end(), addr,
						 [](const GlobalByAddr &entry, uint32_t a) { return entry.g->addr < a; });
	if (it != s_globalsByAddr.end() && it->g->addr == addr)
	{
		return it->g->name;
	}
	return {};
}

void SymbolsSearch(std::string_view prefix, char kind, std::vector<SymbolEntry> &results,
				   int maxResults)
{
	int count = 0;

	/* Search traps */
	if (kind == 't' || kind == '*')
	{
		std::vector<TrapInfo> trapResults;
		trap_dict_search(std::string(prefix).c_str(), trapResults, maxResults);
		for (auto &ti : trapResults)
		{
			if (count >= maxResults) break;
			results.push_back({ti.name, 0, 0, true, ti.trapWord});
			++count;
		}
	}

	/* Search globals */
	if (kind == 'g' || kind == '*')
	{
		auto it = std::lower_bound(s_globalsByName.begin(), s_globalsByName.end(), prefix,
								   [](const GlobalByName &entry, std::string_view p)
								   { return CaseInsensitiveCmp(entry.g->name, p) < 0; });
		while (it != s_globalsByName.end() && count < maxResults)
		{
			if (!CaseInsensitiveStartsWith(it->g->name, prefix)) break;
			results.push_back({it->g->name, it->g->addr, it->g->size, false, 0});
			++count;
			++it;
		}
	}
}

uint16_t SymbolsSizeAt(uint32_t addr)
{
	auto it =
		std::lower_bound(s_globalsByAddr.begin(), s_globalsByAddr.end(), addr,
						 [](const GlobalByAddr &entry, uint32_t a) { return entry.g->addr < a; });
	if (it != s_globalsByAddr.end() && it->g->addr == addr)
	{
		return it->g->size;
	}
	return 0;
}
