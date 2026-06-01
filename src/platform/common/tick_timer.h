/*
	tick_timer — System tick timer and Macintosh timekeeping

	Implements the emulated Macintosh tick timer that drives the
	guest OS clock.  Uses a fixed-point accumulator to generate
	ticks at the Macintosh rate of 60.14742 ticks per second.

	The tick counter g_trueEmulatedTime is the source of truth for
	all Macintosh timekeeping, including the RTC and date functions.
*/

#pragma once

#include <cstdint>

extern uint32_t g_trueEmulatedTime;
extern uint32_t g_newMacDateInSeconds;

// Advance the next tick deadline by one tick interval.
void IncrNextTime();

// Update emulated time based on host monotonic time.  Returns true if a tick occurred.
bool UpdateTrueEmulatedTime();

// Detect when the Mac second has changed (for sound/demo notifications).
bool CheckDateTime();

// Initialize timer state at startup.
void StartUpTimeAdjust();

// Initialize location data (fixed Mac epoch date).
bool InitLocationDat();

// Return milliseconds until the next tick is due.
uint32_t GetTimerDelay();
