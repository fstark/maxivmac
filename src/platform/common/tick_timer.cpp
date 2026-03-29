#include "tick_timer.h"

#include "platform/common/osglu_ui.h"
#include "platform/platform.h"
#include <SDL3/SDL.h>

#define dbglog_TimeStuff 0
#define DBGLOG_OSG_INIT 0

uint32_t g_trueEmulatedTime = 0;
	/*
		The amount of time the program has
		been running, measured in Macintosh
		"ticks". There are 60.14742 ticks per
		second.

		(time when the emulation is
		stopped for more than a few ticks
		should not be counted.)
	*/

#define MyInvTimeDivPow 16
#define MyInvTimeDiv (1 << MyInvTimeDivPow)
#define MyInvTimeDivMask (MyInvTimeDiv - 1)
#define MyInvTimeStep 1089590 /* 1000 / 60.14742 * MyInvTimeDiv */

static Uint32 s_lastTime;

static Uint32 s_nextIntTime;
static uint32_t s_nextFracTime;

void IncrNextTime()
{
	s_nextFracTime += MyInvTimeStep;
	s_nextIntTime += (s_nextFracTime >> MyInvTimeDivPow);
	s_nextFracTime &= MyInvTimeDivMask;
}

static void InitNextTime()
{
	s_nextIntTime = s_lastTime;
	s_nextFracTime = 0;
	IncrNextTime();
}

uint32_t g_newMacDateInSeconds;

bool UpdateTrueEmulatedTime()
{
	Uint32 LatestTime;
	int32_t TimeDiff;

	LatestTime = SDL_GetTicks();
	if (LatestTime != s_lastTime) {
		s_lastTime = LatestTime;
		TimeDiff = (LatestTime - s_nextIntTime);
			/* this should work even when time wraps */
		if (TimeDiff >= 0) {
			if (TimeDiff > 256) {
				/* emulation interrupted, forget it */
				++g_trueEmulatedTime;
				InitNextTime();

#if dbglog_TimeStuff
				dbglog_writelnNum("emulation interrupted",
					g_trueEmulatedTime);
#endif
			} else {
				do {
					++g_trueEmulatedTime;
					IncrNextTime();
					TimeDiff = (LatestTime - s_nextIntTime);
				} while (TimeDiff >= 0);
			}
			return true;
		} else {
			if (TimeDiff < -256) {
#if dbglog_TimeStuff
				dbglog_writeln("clock set back");
#endif
				/* clock goofed if ever get here, reset */
				InitNextTime();
			}
		}
	}

	return false;
}

bool CheckDateTime()
{
	/* CurMacDateInSeconds is driven by tick counter in
	   SixtiethSecondNotify (60 ticks = 1 second), not wall clock.
	   Just detect transitions for sound/demo notifications. */
	static uint32_t lastSeenDate = 0;
	if (g_curMacDateInSeconds != lastSeenDate) {
		lastSeenDate = g_curMacDateInSeconds;
		return true;
	}
	return false;
}

void StartUpTimeAdjust()
{
	s_lastTime = SDL_GetTicks();
	InitNextTime();
}

bool InitLocationDat()
{
#if DBGLOG_OSG_INIT
	dbglog_writeln("enter InitLocationDat");
#endif

	s_lastTime = SDL_GetTicks();
	InitNextTime();

	/* Fixed date: 14 March 1990 12:00:00 UTC (Mac epoch seconds).
	   Deterministic so emulated RTC doesn't depend on host clock. */
	g_newMacDateInSeconds = UINT32_C(0xA223E2C0);
	g_curMacDateInSeconds = g_newMacDateInSeconds;

	return true;
}

uint32_t GetTimerDelay()
{
	return s_nextIntTime - s_lastTime;
}
