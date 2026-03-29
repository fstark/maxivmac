#ifndef TICK_TIMER_H
#define TICK_TIMER_H

#include <cstdint>

extern uint32_t g_trueEmulatedTime;
extern uint32_t g_newMacDateInSeconds;

void IncrNextTime();
bool UpdateTrueEmulatedTime();
bool CheckDateTime();
void StartUpTimeAdjust();
bool InitLocationDat();
uint32_t GetTimerDelay();

#endif /* TICK_TIMER_H */
