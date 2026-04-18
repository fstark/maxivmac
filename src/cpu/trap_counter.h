/*
	trap_counter.h — A-line trap call counter

	Every A-line trap is counted unconditionally in a flat array
	indexed by trap number.  A separate watchlist selects which
	traps are displayed in the UI.

	Trap name lookups delegate to TrapDefs (loaded from assets/traps.def).
*/
#pragma once

#include <cstdint>
#include <vector>

/* ── Counting (called from DoCodeA) ───────────────── */

/* Record one invocation of trapWord ($Axxx). */
void trap_counter_record(uint16_t trapWord);

/* Reset all counters to zero. */
void trap_counter_reset();

/* Read the current count for a single trap word. */
uint32_t trap_counter_get(uint16_t trapWord);

/* Record one invocation of a dispatch subtrap. */
void trap_counter_record_subtrap(uint16_t parentTrapWord, uint16_t selector);

/* Read the current count for a subtrap. */
uint32_t trap_counter_get_subtrap(uint16_t parentTrapWord, uint16_t selector);

/* ── Trap dictionary ──────────────────────────────── */

struct TrapInfo
{
	uint16_t trapWord;
	uint16_t subtrapSelector = 0;
	const char *name;
};

/* Number of entries in the built-in dictionary. */
int trap_dict_size();

/* Access dictionary entry by index (0 .. trap_dict_size()-1). */
const TrapInfo &trap_dict_entry(int index);

/* Look up a trap word → name.  Returns nullptr if unknown. */
const char *trap_dict_name(uint16_t trapWord);

/* Search dictionary for entries whose name starts with prefix
   (case-insensitive).  Appends matching TrapInfo to results.
   Stops after maxResults matches. */
void trap_dict_search(const char *prefix, std::vector<TrapInfo> &results, int maxResults = 20);

/* ── Watchlist ────────────────────────────────────── */

struct WatchEntry
{
	uint16_t trapWord;
	const char *name; /* points into dictionary — stable */
	uint32_t count;	  /* snapshot, not live */
};

/* Add a trap to the watchlist.  No-op if already present. */
void trap_watch_add(uint16_t trapWord);

/* Remove a trap from the watchlist. */
void trap_watch_remove(uint16_t trapWord);

/* Snapshot the current watchlist with live counts.
   Caller owns the returned vector. */
std::vector<WatchEntry> trap_watch_snapshot();

/* Load the default watchlist (20 common traps). */
void trap_watch_load_defaults();
