#ifndef SDL_SOUND_H
#define SDL_SOUND_H

#include <cstdint>

void Sound_Start();
void Sound_Stop();
bool Sound_Init();
void Sound_UnInit();
void Sound_SecondNotify();

bool Sound_AllocBuffer();
void Sound_FreeBuffer();

#endif /* SDL_SOUND_H */
