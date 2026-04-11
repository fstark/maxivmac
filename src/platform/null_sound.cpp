/*
	null_sound.cpp — stub sound implementation for headless builds

	Provides no-op implementations of all sound interface functions.
	Linked instead of sdl_sound.cpp when building without SDL.

	Sound_BeginWrite must return a real writeable buffer and accept
	the full requested sample count.  Device code (ASC, SoundDevice)
	advances internal FIFO pointers and interrupt state based on the
	number of samples written; returning actL=0 would skip that work
	and cause MacII's ASC→VIA2 interrupt timing to diverge.
*/

#include "platform/sound_interface.h"
#include "platform/platform.h"

/* Match the SDL backend's buffer geometry so the write-offset
   arithmetic stays in sync with what device code expects. */
#define kLn2SoundBuffers 4
#define kLnOneBuffLen 9
#define kLnAllBuffLen (kLn2SoundBuffers + kLnOneBuffLen)
#define kOneBuffLen (1UL << kLnOneBuffLen)
#define kAllBuffLen (1UL << kLnAllBuffLen)
#define kOneBuffMask (kOneBuffLen - 1)
#define kAllBuffMask (kAllBuffLen - 1)

static uint16_t s_soundBuffer[kAllBuffLen];
static uint16_t s_writeOffset;

void Sound_Start()
{
	s_writeOffset = 0;
}

void Sound_Stop() {}
bool Sound_Init()
{
	return true;
}
void Sound_UnInit() {}
void Sound_SecondNotify() {}
bool Sound_AllocBuffer()
{
	return true;
}
void Sound_FreeBuffer() {}

/*
	Sound_BeginWrite / Sound_EndWrite are called by device code
	(src/devices/sound.cpp, src/devices/asc.cpp).
*/
SoundSamplePtr Sound_BeginWrite(uint16_t n, uint16_t *actL)
{
	uint16_t WriteBuffContig = kOneBuffLen - (s_writeOffset & kOneBuffMask);

	if (WriteBuffContig < n)
	{
		n = WriteBuffContig;
	}

	*actL = n;
	return s_soundBuffer + (s_writeOffset & kAllBuffMask);
}

void Sound_EndWrite(uint16_t actL)
{
	s_writeOffset += actL;
}
