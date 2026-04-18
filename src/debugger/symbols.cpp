/*
	symbols.cpp — Symbol lookup implementation

	Builds sorted indexes over the trap dictionary and low-memory
	global tables for fast name and address lookup.
*/

#include "debugger/symbols.h"

#include "cpu/trap_counter.h"
#include "lang/global_registry.h"

#include <cstdio>

#include <algorithm>
#include <cctype>
#include <cstring>

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
	/* GlobalRegistry is initialized at startup in InitEmulation().
	   Nothing else to do here. */
}

int SymbolsTrapCount()
{
	return trap_dict_size();
}

int SymbolsGlobalCount()
{
	return g_globalRegistry().count();
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

	/* Lookup in GlobalRegistry */
	const GlobalDef *gd = g_globalRegistry().findByName(name);
	if (gd)
	{
		outAddr = gd->addr;
		outTrapWord = 0;
		return true;
	}

	return false;
}

std::string_view SymbolsAtAddress(uint32_t addr)
{
	const GlobalDef *gd = g_globalRegistry().findByAddr(addr);
	if (gd) return gd->name;
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

	/* Search globals — linear scan with prefix match */
	if (kind == 'g' || kind == '*')
	{
		for (auto &gd : g_globalRegistry().globals())
		{
			if (count >= maxResults) break;
			if (CaseInsensitiveStartsWith(gd.name, prefix))
			{
				results.push_back({gd.name, gd.addr, gd.size, false, 0});
				++count;
			}
		}
	}
}

uint16_t SymbolsSizeAt(uint32_t addr)
{
	const GlobalDef *gd = g_globalRegistry().findByAddr(addr);
	if (gd) return gd->size;
	return 0;
}

const char *SymbolsTrapName(uint16_t tw)
{
	const char *name = trap_dict_name(tw);
	if (name) return name;
	static char buf[8];
	std::snprintf(buf, sizeof(buf), "$%04X", tw);
	return buf;
}
