#include "tick_timer.h"

#include "platform/common/osglu_ui.h"
#include "platform/platform.h"
#include <SDL3/SDL.h>

#define dbglog_TimeStuff (0 && dbglog_HAVE)
#define dbglog_OSGInit (0 && dbglog_HAVE)

uint32_t TrueEmulatedTime = 0;
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

static Uint32 LastTime;

static Uint32 NextIntTime;
static uint32_t NextFracTime;

void IncrNextTime()
{
	NextFracTime += MyInvTimeStep;
	NextIntTime += (NextFracTime >> MyInvTimeDivPow);
	NextFracTime &= MyInvTimeDivMask;
}

static void InitNextTime()
{
	NextIntTime = LastTime;
	NextFracTime = 0;
	IncrNextTime();
}

uint32_t NewMacDateInSeconds;

bool UpdateTrueEmulatedTime()
{
	Uint32 LatestTime;
	int32_t TimeDiff;

	LatestTime = SDL_GetTicks();
	if (LatestTime != LastTime) {
		LastTime = LatestTime;
		TimeDiff = (LatestTime - NextIntTime);
			/* this should work even when time wraps */
		if (TimeDiff >= 0) {
			if (TimeDiff > 256) {
				/* emulation interrupted, forget it */
				++TrueEmulatedTime;
				InitNextTime();

#if dbglog_TimeStuff
				dbglog_writelnNum("emulation interrupted",
					TrueEmulatedTime);
#endif
			} else {
				do {
					++TrueEmulatedTime;
					IncrNextTime();
					TimeDiff = (LatestTime - NextIntTime);
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
	if (CurMacDateInSeconds != lastSeenDate) {
		lastSeenDate = CurMacDateInSeconds;
		return true;
	}
	return false;
}

void StartUpTimeAdjust()
{
	LastTime = SDL_GetTicks();
	InitNextTime();
}

bool InitLocationDat()
{
#if dbglog_OSGInit
	dbglog_writeln("enter InitLocationDat");
#endif

	LastTime = SDL_GetTicks();
	InitNextTime();

	/* Fixed date: 14 March 1990 12:00:00 UTC (Mac epoch seconds).
	   Deterministic so emulated RTC doesn't depend on host clock. */
	NewMacDateInSeconds = UINT32_C(0xA223E2C0);
	CurMacDateInSeconds = NewMacDateInSeconds;

	return true;
}

uint32_t GetTimerDelay()
{
	return NextIntTime - LastTime;
}
