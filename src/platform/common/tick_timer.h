#ifndef TICK_TIMER_H
#define TICK_TIMER_H

#include <cstdint>

extern uint32_t TrueEmulatedTime;
extern uint32_t NewMacDateInSeconds;

void IncrNextTime();
bool UpdateTrueEmulatedTime();
bool CheckDateTime();
void StartUpTimeAdjust();
bool InitLocationDat();
uint32_t GetTimerDelay();

#endif /* TICK_TIMER_H */
