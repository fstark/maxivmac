#include "tick_timer.h"

#include "platform/common/osglu_ui.h"
#include "platform/platform.h"

#include <ctime>

/* Monotonic millisecond timer — replaces GetTicksMs(). */
static uint32_t GetTicksMs()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

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

static uint32_t s_lastTime;

static uint32_t s_nextIntTime;
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
	uint32_t LatestTime;
	int32_t TimeDiff;

	LatestTime = GetTicksMs();
	if (LatestTime != s_lastTime) {
		s_lastTime = LatestTime;
		TimeDiff = (LatestTime - s_nextIntTime);
			/* this should work even when time wraps */
		if (TimeDiff >= 0) {
			/* One tick is due.  Never accumulate — if we fell
			   behind, just reset the deadline to now so the
			   next tick is due in ~16.6 ms. */
			++g_trueEmulatedTime;
			if (TimeDiff > 256) {
				/* big gap (debugger, sleep, etc.) */
				InitNextTime();
			} else {
				IncrNextTime();
				/* If still behind, drop the debt */
				if ((int32_t)(LatestTime - s_nextIntTime) >= 0) {
					InitNextTime();
				}
			}
			return true;
		} else {
			if (TimeDiff < -256) {
				/* clock went backwards, reset */
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
	s_lastTime = GetTicksMs();
	InitNextTime();
}

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

uint32_t GetTimerDelay()
{
	return s_nextIntTime - s_lastTime;
}
