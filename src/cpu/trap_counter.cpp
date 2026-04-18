/*
	trap_counter.cpp — A-line trap call counter implementation

	Architecture:
	  - Flat uint32_t array indexed by trap number (OS 0-255, Toolbox 256-1279)
		incremented unconditionally from DoCodeA().  Zero runtime overhead beyond
		one array write per trap.
	  - Dictionary lookups delegate to TrapDefs (loaded from assets/traps.def).
	  - Watchlist: small vector of trap words the UI cares about.
*/

#include "cpu/trap_counter.h"
#include "cpu/trap_defs.h"
#include <algorithm>
#include <atomic>
#include <string_view>
#include <unordered_map>

extern TrapDefs g_trapDefs;

/* ── Counter array ────────────────────────────────── */

/*
   Index layout:
	 0..255   = OS trap numbers   (trap word & 0x00FF)
	 256..1279 = Toolbox trap numbers (trap word & 0x03FF) + 256
*/
static constexpr int kCounterSize = 1280;
static std::atomic<uint32_t> s_counters[kCounterSize];

static inline int TrapIndex(uint16_t trapWord)
{
	if (trapWord & 0x0800)
		return 256 + (trapWord & 0x03FF);
	else
		return trapWord & 0x00FF;
}

void trap_counter_record(uint16_t trapWord)
{
	int idx = TrapIndex(trapWord);
	s_counters[idx].fetch_add(1, std::memory_order_relaxed);
}

void trap_counter_reset()
{
	for (int i = 0; i < kCounterSize; ++i)
		s_counters[i].store(0, std::memory_order_relaxed);
}

uint32_t trap_counter_get(uint16_t trapWord)
{
	return s_counters[TrapIndex(trapWord)].load(std::memory_order_relaxed);
}

/* ── Subtrap counters (diagnostic, not hot-path) ───── */

static std::unordered_map<uint32_t, std::atomic<uint32_t>> s_subtrapCounters;

void trap_counter_record_subtrap(uint16_t parentTrapWord, uint16_t selector)
{
	uint32_t key = (static_cast<uint32_t>(parentTrapWord) << 16) | selector;
	s_subtrapCounters[key].fetch_add(1, std::memory_order_relaxed);
}

uint32_t trap_counter_get_subtrap(uint16_t parentTrapWord, uint16_t selector)
{
	uint32_t key = (static_cast<uint32_t>(parentTrapWord) << 16) | selector;
	auto it = s_subtrapCounters.find(key);
	if (it != s_subtrapCounters.end()) return it->second.load(std::memory_order_relaxed);
	return 0;
}

/* ── Trap dictionary (delegates to g_trapDefs) ────── */

static const std::vector<TrapInfo> &CachedDict()
{
	static std::vector<TrapInfo> cache;
	if (cache.empty())
	{
		int n = g_trapDefs.size();
		cache.reserve(n);
		for (int i = 0; i < n; ++i)
		{
			auto [key, sv] = g_trapDefs.entry(i);
			TrapInfo info;
			if (key > 0xFFFF)
			{
				info.trapWord = static_cast<uint16_t>(key >> 16);
				info.subtrapSelector = static_cast<uint16_t>(key & 0xFFFF);
			}
			else
			{
				info.trapWord = static_cast<uint16_t>(key);
				info.subtrapSelector = 0;
			}
			info.name = sv.data();
			cache.push_back(info);
		}
	}
	return cache;
}

int trap_dict_size()
{
	return g_trapDefs.size();
}

const TrapInfo &trap_dict_entry(int index)
{
	return CachedDict()[index];
}

const char *trap_dict_name(uint16_t trapWord)
{
	auto sv = g_trapDefs.nameOf(trapWord);
	return sv.empty() ? nullptr : sv.data();
}

void trap_dict_search(const char *prefix, std::vector<TrapInfo> &results, int maxResults)
{
	results.clear();
	if (!prefix || !prefix[0]) return;
	std::vector<std::pair<uint32_t, std::string_view>> raw;
	g_trapDefs.search(prefix, raw, maxResults);
	for (auto &[key, name] : raw)
	{
		TrapInfo info;
		if (key > 0xFFFF)
		{
			info.trapWord = static_cast<uint16_t>(key >> 16);
			info.subtrapSelector = static_cast<uint16_t>(key & 0xFFFF);
		}
		else
		{
			info.trapWord = static_cast<uint16_t>(key);
			info.subtrapSelector = 0;
		}
		info.name = name.data();
		results.push_back(info);
	}
}

/* ── Watchlist ────────────────────────────────────── */

static std::vector<uint16_t> s_watchlist;

static const uint16_t kDefaults[] = {
	0xA000, 0xA001, 0xA002, 0xA003, /* Open Close Read Write */
	0xA122, 0xA023, 0xA11E, 0xA01F, /* NewHandle DisposHandle NewPtr DisposPtr */
	0xA029, 0xA02A, 0xA02E,			/* HLock HUnlock BlockMove */
	0xA9A0, 0xA9A2, 0xA9A3,			/* GetResource LoadResource ReleaseResource */
	0xA970, 0xA971, 0xA02F,			/* GetNextEvent EventAvail PostEvent */
	0xA92C, 0xA913, 0xA884,			/* FindWindow NewWindow DrawString */
};

void trap_watch_load_defaults()
{
	s_watchlist.clear();
	for (uint16_t tw : kDefaults)
		s_watchlist.push_back(tw);
}

void trap_watch_add(uint16_t trapWord)
{
	for (uint16_t tw : s_watchlist)
		if (tw == trapWord) return;
	s_watchlist.push_back(trapWord);
}

void trap_watch_remove(uint16_t trapWord)
{
	auto it = std::find(s_watchlist.begin(), s_watchlist.end(), trapWord);
	if (it != s_watchlist.end()) s_watchlist.erase(it);
}

std::vector<WatchEntry> trap_watch_snapshot()
{
	std::vector<WatchEntry> out;
	out.reserve(s_watchlist.size());
	for (uint16_t tw : s_watchlist)
	{
		WatchEntry e;
		e.trapWord = tw;
		e.name = trap_dict_name(tw);
		if (!e.name) e.name = "???";
		e.count = trap_counter_get(tw);
		out.push_back(e);
	}
	return out;
}
