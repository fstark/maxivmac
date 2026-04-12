/*
	trap_counter.h — A-line trap call counter

	Every A-line trap is counted unconditionally in a flat array
	indexed by trap number.  A separate watchlist selects which
	traps are displayed in the UI.

	The full trap dictionary (~681 entries) maps trap words to
	human-readable names for lookup and autocomplete.
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

/* ── Trap dictionary ──────────────────────────────── */

struct TrapInfo
{
	uint16_t trapWord;
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

/* ── Console tracing ──────────────────────────────── */

/* Nestable trap console tracing.  While active, every A-line
   trap is logged to stderr as [TRAP] $Axxx TrapName.
   Calls are reference-counted so nested Begin/End pairs work. */
void BeginTraceTraps();
void EndTraceTraps();

/* Called from DoCodeA — logs if tracing is active. */
void trap_trace_log(uint16_t trapWord);

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
