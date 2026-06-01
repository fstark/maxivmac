/*
	tick_timer — System tick timer and Macintosh timekeeping

	Implements the emulated Macintosh tick timer that drives the
	guest OS clock.  Uses a fixed-point accumulator to generate
	ticks at the Macintosh rate of 60.14742 ticks per second.

	The tick counter g_trueEmulatedTime is the source of truth for
	all Macintosh timekeeping, including the RTC and date functions.

	#### FReD : I don't think the comments on the time/sound are correct
	In general, the tick handling is very poor and convoluted
	(which is unfortunate, as it is central to the emulation).
*/

#include "tick_timer.h"

#include "platform/common/osglu_ui.h"
#include "platform/platform.h"

#include <SDL3/SDL_timer.h>

/* Monotonic millisecond timer using SDL3. */
static uint32_t GetTicksMs()
{
	return (uint32_t)SDL_GetTicks();
}

#define DBGLOG_OSG_INIT 0

uint32_t g_trueEmulatedTime = 0;
/*
	Emulated Macintosh tick counter.  Increments at 60.14742 Hz
	(approximately every 16.6 ms).  Does not advance while emulation
	is paused or stepping in the debugger.

	Macintosh "ticks" are the fundamental time unit for the OS:
	they drive the RTC, task scheduling, and event timing.
*/

#define INV_TIME_DIV_POW 16
#define INV_TIME_DIV (1 << INV_TIME_DIV_POW)
#define INV_TIME_DIV_MASK (INV_TIME_DIV - 1)
#define INV_TIME_STEP 1089590 /* 1000 / 60.14742 * INV_TIME_DIV */

/* Millisecond timestamp of the last host time update. */
static uint32_t s_lastTime;

/* Next tick deadline in fixed-point: integer and fractional parts. */
static uint32_t s_nextIntTime;
static uint32_t s_nextFracTime;

/*
	Advance the next tick deadline by one tick interval using fixed-point
	accumulator.  INV_TIME_STEP encodes 1000/60.14742 milliseconds in
	Q16.16 format.  Adding this value to the fractional accumulator may
	carry into the integer part, indicating a tick is due.
*/
void IncrNextTime()
{
	s_nextFracTime += INV_TIME_STEP;
	s_nextIntTime += (s_nextFracTime >> INV_TIME_DIV_POW);
	s_nextFracTime &= INV_TIME_DIV_MASK;
}

/*
	Initialize the tick deadline to the current host time.
	Called at startup and after large time gaps.
*/
static void InitNextTime()
{
	s_nextIntTime = s_lastTime;
	s_nextFracTime = 0;
	IncrNextTime();
}

uint32_t g_newMacDateInSeconds;

/*
	Advance the tick counter if the host has advanced far enough.
	Returns true if a tick occurred.  Handles:
	- Normal single-tick advancement
	- Dropping ticks if we fall behind (e.g. debugger break)
	- Resetting deadline if host clock goes backwards
	- Resetting deadline after large gaps (sleep, pause)
*/
bool UpdateTrueEmulatedTime()
{
	uint32_t LatestTime;
	int32_t TimeDiff;

	LatestTime = GetTicksMs();
	if (LatestTime != s_lastTime)
	{
		s_lastTime = LatestTime;
		TimeDiff = (LatestTime - s_nextIntTime);
		/* this should work even when time wraps */
		if (TimeDiff >= 0)
		{
			/* One tick is due.  Never accumulate — if we fell
			   behind, just reset the deadline to now so the
			   next tick is due in ~16.6 ms. */
			++g_trueEmulatedTime;
			if (TimeDiff > 256)
			{
				/* big gap (debugger, sleep, etc.) */
				InitNextTime();
			}
			else
			{
				IncrNextTime();
				/* If still behind, drop the debt */
				if ((int32_t)(LatestTime - s_nextIntTime) >= 0)
				{
					InitNextTime();
				}
			}
			return true;
		}
		else
		{
			if (TimeDiff < -256)
			{
				/* clock went backwards, reset */
				InitNextTime();
			}
		}
	}

	return false;
}

/*
	Detect when the Mac second has changed.  Called periodically to
	notify sound code of second boundaries.  Uses the tick counter
	(not wall clock) as the time source.
*/
bool CheckDateTime()
{
	/* CurMacDateInSeconds is driven by tick counter in
	   SixtiethSecondNotify (60 ticks = 1 second), not wall clock.
	   Just detect transitions for sound notifications. */
	static uint32_t lastSeenDate = 0;
	if (g_curMacDateInSeconds != lastSeenDate)
	{
		lastSeenDate = g_curMacDateInSeconds;
		return true;
	}
	return false;
}

/*
	Initialize timer state at startup.  Sets s_lastTime to the current
	host timestamp and primes the tick deadline.
*/
void StartUpTimeAdjust()
{
	s_lastTime = GetTicksMs();
	InitNextTime();
}

/*
	Initialize location data.  Sets up the fixed Macintosh epoch date
	(14 March 1990 12:00:00 UTC) so the emulated RTC starts from a
	deterministic value independent of host clock.
*/
bool InitLocationDat()
{
#if DBGLOG_OSG_INIT
	dbglog_writeln("enter InitLocationDat");
#endif

	s_lastTime = GetTicksMs();
	InitNextTime();

	/* Fixed date: 14 March 1990 12:00:00 UTC (Mac epoch seconds).
	   Deterministic so emulated RTC doesn't depend on host clock. */
	g_newMacDateInSeconds = UINT32_C(0xA223E2C0);
	g_curMacDateInSeconds = g_newMacDateInSeconds;

	return true;
}

/*
	Return milliseconds until the next tick is due.  Used by the
	platform layer to wait or yield appropriately.
*/
uint32_t GetTimerDelay()
{
	return s_nextIntTime - s_lastTime;
}
