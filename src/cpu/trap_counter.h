/*
	trap_counter.h — A-line trap call counter

	Tracks how many times each Mac Toolbox/OS trap has been
	invoked since boot.  The 68K CPU core calls
	trap_counter_record() from the A-line exception handler.
*/
#pragma once

#include <cstdint>

/* Called from DoCodeA() with the full 16-bit trap word ($Axxx). */
void trap_counter_record(uint16_t trapWord);

/* Reset all counters to zero (e.g. on machine reset). */
void trap_counter_reset();

struct TrapCountEntry {
	uint16_t trapWord;
	const char *name;
	uint32_t count;
};

/* Fill out[] with the watched traps, sorted by count descending.
   Returns the number of entries written (always <= maxOut). */
int trap_counter_snapshot(TrapCountEntry *out, int maxOut);
