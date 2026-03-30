/*
	sound_interface.h — platform-independent sound interface

	Declares the sound functions that the emulator shell and core
	call. Implemented by sdl_sound.cpp (SDL builds) or
	null_sound.cpp (headless builds).
*/

#ifndef SOUND_INTERFACE_H
#define SOUND_INTERFACE_H

#include <cstdint>

void Sound_Start();
void Sound_Stop();
bool Sound_Init();
void Sound_UnInit();
void Sound_SecondNotify();

bool Sound_AllocBuffer();
void Sound_FreeBuffer();

#endif /* SOUND_INTERFACE_H */
