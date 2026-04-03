/*
	trap_counter.cpp — A-line trap call counter implementation
*/

#include "cpu/trap_counter.h"
#include <algorithm>
#include <atomic>
#include <cstring>

/* ── Watched traps (the 20 most interesting) ──────── */

struct WatchedTrap {
	uint16_t trapWord;
	const char *name;
	std::atomic<uint32_t> count;
};

static WatchedTrap s_watched[] = {
	/* File I/O */
	{ 0xA000, "Open",            {} },
	{ 0xA001, "Close",           {} },
	{ 0xA002, "Read",            {} },
	{ 0xA003, "Write",           {} },

	/* Memory Manager */
	{ 0xA122, "NewHandle",       {} },
	{ 0xA023, "DisposHandle",    {} },
	{ 0xA11E, "NewPtr",          {} },
	{ 0xA01F, "DisposPtr",       {} },
	{ 0xA029, "HLock",           {} },
	{ 0xA02A, "HUnlock",         {} },
	{ 0xA02E, "BlockMove",       {} },

	/* Resources */
	{ 0xA9A0, "GetResource",     {} },
	{ 0xA9A2, "LoadResource",    {} },
	{ 0xA9A3, "ReleaseResource", {} },

	/* Events */
	{ 0xA970, "GetNextEvent",    {} },
	{ 0xA971, "EventAvail",      {} },
	{ 0xA02F, "PostEvent",       {} },

	/* Window / UI */
	{ 0xA92C, "FindWindow",      {} },
	{ 0xA913, "NewWindow",       {} },
	{ 0xA884, "DrawString",      {} },
};

static constexpr int kNumWatched =
	static_cast<int>(sizeof(s_watched) / sizeof(s_watched[0]));

/*
   Fast lookup: OS traps ($A0xx) use bits 7..0, Toolbox traps ($A8xx+)
   use bits 9..0.  We keep a small index table mapping trap word → watched
   slot, built lazily on first call.
*/

static constexpr int kIndexSize = 0x0400; /* 10-bit trap number space */
static int8_t s_index[kIndexSize];        /* -1 = not watched */
static bool   s_indexReady = false;

static void BuildIndex()
{
	std::memset(s_index, -1, sizeof(s_index));
	for (int i = 0; i < kNumWatched; ++i) {
		uint16_t w = s_watched[i].trapWord;
		uint16_t key;
		if (w & 0x0800) {
			/* Toolbox trap: 10-bit number */
			key = w & 0x03FF;
		} else {
			/* OS trap: 8-bit number */
			key = w & 0x00FF;
		}
		s_index[key] = static_cast<int8_t>(i);
	}
	s_indexReady = true;
}

void trap_counter_record(uint16_t trapWord)
{
	if (!s_indexReady) [[unlikely]]
		BuildIndex();

	uint16_t key;
	if (trapWord & 0x0800) {
		key = trapWord & 0x03FF;
	} else {
		key = trapWord & 0x00FF;
	}

	if (key < kIndexSize) {
		int8_t slot = s_index[key];
		if (slot >= 0) {
			s_watched[slot].count.fetch_add(1, std::memory_order_relaxed);
		}
	}
}

void trap_counter_reset()
{
	for (int i = 0; i < kNumWatched; ++i)
		s_watched[i].count.store(0, std::memory_order_relaxed);
}

int trap_counter_snapshot(TrapCountEntry *out, int maxOut)
{
	/* Copy current counts */
	TrapCountEntry tmp[kNumWatched];
	for (int i = 0; i < kNumWatched; ++i) {
		tmp[i].trapWord = s_watched[i].trapWord;
		tmp[i].name     = s_watched[i].name;
		tmp[i].count    = s_watched[i].count.load(std::memory_order_relaxed);
	}

	/* Sort descending by count */
	std::sort(tmp, tmp + kNumWatched,
		[](const TrapCountEntry &a, const TrapCountEntry &b) {
			return a.count > b.count;
		});

	int n = std::min(maxOut, kNumWatched);
	std::memcpy(out, tmp, static_cast<size_t>(n) * sizeof(TrapCountEntry));
	return n;
}
