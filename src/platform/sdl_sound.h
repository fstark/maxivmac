#pragma once

#include <cstdint>

void SoundStart();
void SoundStop();
bool SoundInit();
void SoundUnInit();
void SoundSecondNotify();

bool SoundAllocBuffer();
void SoundFreeBuffer();
