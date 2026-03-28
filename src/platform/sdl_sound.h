#ifndef SDL_SOUND_H
#define SDL_SOUND_H

#include <cstdint>

void MySound_Start();
void MySound_Stop();
bool MySound_Init();
void MySound_UnInit();
void MySound_SecondNotify();

bool MySound_AllocBuffer();
void MySound_FreeBuffer();

#endif /* SDL_SOUND_H */
