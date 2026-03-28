#include "platform/sdl_sound.h"

#include "platform/common/osglu_ui.h"
#include "platform/common/osglu_ud.h"
#include "platform/common/osglu_common.h"
#include "platform/platform.h"
#include "platform/common/tick_timer.h"

#include <SDL3/SDL.h>

#define kLn2SoundBuffers 4 /* kSoundBuffers must be a power of two */
#define kSoundBuffers (1 << kLn2SoundBuffers)
#define kSoundBuffMask (kSoundBuffers - 1)

#define DesiredMinFilledSoundBuffs 3
	/*
		if too big then sound lags behind emulation.
		if too small then sound will have pauses.
	*/

#define kLnOneBuffLen 9
#define kLnAllBuffLen (kLn2SoundBuffers + kLnOneBuffLen)
#define kOneBuffLen (1UL << kLnOneBuffLen)
#define kAllBuffLen (1UL << kLnAllBuffLen)
#define kLnOneBuffSz (kLnOneBuffLen + 1)
#define kLnAllBuffSz (kLnAllBuffLen + 1)
#define kOneBuffSz (1UL << kLnOneBuffSz)
#define kAllBuffSz (1UL << kLnAllBuffSz)
#define kOneBuffMask (kOneBuffLen - 1)
#define kAllBuffMask (kAllBuffLen - 1)
#define dbhBufferSize (kAllBuffSz + kOneBuffSz)

#define dbglog_SoundStuff (0 && dbglog_HAVE)
#define dbglog_SoundBuffStats (0 && dbglog_HAVE)
#define dbglog_OSGInit (0 && dbglog_HAVE)

static SoundSamplePtr s_soundBuffer = nullptr;
volatile static uint16_t ThePlayOffset;
volatile static uint16_t TheFillOffset;
volatile static uint16_t MinFilledSoundBuffs;
#if dbglog_SoundBuffStats
static uint16_t s_maxFilledSoundBuffs;
#endif
static uint16_t s_writeOffset;

static SDL_AudioStream *stream = nullptr;

static void Sound_Init0()
{
	ThePlayOffset = 0;
	TheFillOffset = 0;
	s_writeOffset = 0;
}

static void Sound_Start0()
{
	/* Reset variables */
	MinFilledSoundBuffs = kSoundBuffers + 1;
#if dbglog_SoundBuffStats
	s_maxFilledSoundBuffs = 0;
#endif
}

SoundSamplePtr Sound_BeginWrite(uint16_t n, uint16_t *actL)
{
	uint16_t ToFillLen = kAllBuffLen - (s_writeOffset - ThePlayOffset);
	uint16_t WriteBuffContig =
		kOneBuffLen - (s_writeOffset & kOneBuffMask);

	if (WriteBuffContig < n) {
		n = WriteBuffContig;
	}
	if (ToFillLen < n) {
		/* overwrite previous buffer */
#if dbglog_SoundStuff
		dbglog_writeln("sound buffer over flow");
#endif
		s_writeOffset -= kOneBuffLen;
	}

	*actL = n;
	return s_soundBuffer + (s_writeOffset & kAllBuffMask);
}

static void ConvertSoundBlockToNative(SoundSamplePtr p)
{
	int i;

	for (i = kOneBuffLen; --i >= 0; ) {
		*p++ -= 0x8000;
	}
}

static void Sound_WroteABlock()
{
	uint16_t PrevWriteOffset = s_writeOffset - kOneBuffLen;
	SoundSamplePtr p = s_soundBuffer + (PrevWriteOffset & kAllBuffMask);

#if dbglog_SoundStuff
	dbglog_writeln("enter Sound_WroteABlock");
#endif

	ConvertSoundBlockToNative(p);

	TheFillOffset = s_writeOffset;

#if dbglog_SoundBuffStats
	{
		uint16_t ToPlayLen = TheFillOffset
			- ThePlayOffset;
		uint16_t ToPlayBuffs = ToPlayLen >> kLnOneBuffLen;

		if (ToPlayBuffs > s_maxFilledSoundBuffs) {
			s_maxFilledSoundBuffs = ToPlayBuffs;
		}
	}
#endif
}

static bool Sound_EndWrite0(uint16_t actL)
{
	bool v;

	s_writeOffset += actL;

	if (0 != (s_writeOffset & kOneBuffMask)) {
		v = false;
	} else {
		/* just finished a block */

		Sound_WroteABlock();

		v = true;
	}

	return v;
}

static void Sound_SecondNotify0()
{
	if (MinFilledSoundBuffs <= kSoundBuffers) {
		if (MinFilledSoundBuffs > DesiredMinFilledSoundBuffs) {
#if dbglog_SoundStuff
			dbglog_writeln("MinFilledSoundBuffs too high");
#endif
			IncrNextTime();
		} else if (MinFilledSoundBuffs < DesiredMinFilledSoundBuffs) {
#if dbglog_SoundStuff
			dbglog_writeln("MinFilledSoundBuffs too low");
#endif
			++TrueEmulatedTime;
		}
#if dbglog_SoundBuffStats
		dbglog_writelnNum("MinFilledSoundBuffs",
			MinFilledSoundBuffs);
		dbglog_writelnNum("s_maxFilledSoundBuffs",
			s_maxFilledSoundBuffs);
		s_maxFilledSoundBuffs = 0;
#endif
		MinFilledSoundBuffs = kSoundBuffers + 1;
	}
}

typedef uint16_t SoundTemp;

#define kCenterTempSound 0x8000

#define AudioStepVal 0x0040

#define ConvertTempSoundSampleFromNative(v) ((v) + kCenterSound)

#define ConvertTempSoundSampleToNative(v) ((v) - kCenterSound)

static void SoundRampTo(SoundTemp *last_val, SoundTemp dst_val,
	SoundSamplePtr *stream, int *len)
{
	SoundTemp diff;
	SoundSamplePtr p = *stream;
	int n = *len;
	SoundTemp v1 = *last_val;

	while ((v1 != dst_val) && (0 != n)) {
		if (v1 > dst_val) {
			diff = v1 - dst_val;
			if (diff > AudioStepVal) {
				v1 -= AudioStepVal;
			} else {
				v1 = dst_val;
			}
		} else {
			diff = dst_val - v1;
			if (diff > AudioStepVal) {
				v1 += AudioStepVal;
			} else {
				v1 = dst_val;
			}
		}

		--n;
		*p++ = ConvertTempSoundSampleToNative(v1);
	}

	*stream = p;
	*len = n;
	*last_val = v1;
}

struct SoundR {
	SoundSamplePtr fTheSoundBuffer;
	volatile uint16_t (*fPlayOffset);
	volatile uint16_t (*fFillOffset);
	volatile uint16_t (*fMinFilledSoundBuffs);

	volatile SoundTemp lastv;

	bool wantplaying;
	bool HaveStartedPlaying;
};

static void my_audio_callback(void *udata, Uint8 *stream, int len)
{
	uint16_t ToPlayLen;
	uint16_t FilledSoundBuffs;
	int i;
	SoundR *datp = (SoundR *)udata;
	SoundSamplePtr CurSoundBuffer = datp->fTheSoundBuffer;
	uint16_t CurPlayOffset = *datp->fPlayOffset;
	SoundTemp v0 = datp->lastv;
	SoundTemp v1 = v0;
	SoundSamplePtr dst = (SoundSamplePtr)stream;

	len >>= 1; /* convert byte length to sample count (16-bit samples) */

#if dbglog_SoundStuff
	dbglog_writeln("Enter my_audio_callback");
	dbglog_writelnNum("len", len);
#endif

	while (len > 0) {
		ToPlayLen = *datp->fFillOffset - CurPlayOffset;
		FilledSoundBuffs = ToPlayLen >> kLnOneBuffLen;

		if (! datp->wantplaying) {
#if dbglog_SoundStuff
			dbglog_writeln("playing end transistion");
#endif

			SoundRampTo(&v1, kCenterTempSound, &dst, &len);

			ToPlayLen = 0;
		} else if (! datp->HaveStartedPlaying) {
#if dbglog_SoundStuff
			dbglog_writeln("playing start block");
#endif

			if ((ToPlayLen >> kLnOneBuffLen) < 8) {
				ToPlayLen = 0;
			} else {
				SoundSamplePtr p = datp->fTheSoundBuffer
					+ (CurPlayOffset & kAllBuffMask);
				SoundTemp v2 = ConvertTempSoundSampleFromNative(*p);

#if dbglog_SoundStuff
				dbglog_writeln("have enough samples to start");
#endif

				SoundRampTo(&v1, v2, &dst, &len);

				if (v1 == v2) {
#if dbglog_SoundStuff
					dbglog_writeln("finished start transition");
#endif

					datp->HaveStartedPlaying = true;
				}
			}
		}

		if (0 == len) {
			/* done */

			if (FilledSoundBuffs < *datp->fMinFilledSoundBuffs) {
				*datp->fMinFilledSoundBuffs = FilledSoundBuffs;
			}
		} else if (0 == ToPlayLen) {

#if dbglog_SoundStuff
			dbglog_writeln("under run");
#endif

			for (i = 0; i < len; ++i) {
				*dst++ = ConvertTempSoundSampleToNative(v1);
			}
			*datp->fMinFilledSoundBuffs = 0;
			break;
		} else {
			uint16_t PlayBuffContig = kAllBuffLen
				- (CurPlayOffset & kAllBuffMask);
			SoundSamplePtr p = CurSoundBuffer
				+ (CurPlayOffset & kAllBuffMask);

			if (ToPlayLen > PlayBuffContig) {
				ToPlayLen = PlayBuffContig;
			}
			if (ToPlayLen > len) {
				ToPlayLen = len;
			}

			for (i = 0; i < ToPlayLen; ++i) {
				*dst++ = *p++;
			}
			v1 = ConvertTempSoundSampleFromNative(p[-1]);

			CurPlayOffset += ToPlayLen;
			len -= ToPlayLen;

			*datp->fPlayOffset = CurPlayOffset;
		}
	}

	datp->lastv = v1;
}
static void SDLCALL sdl3_audio_callback(void *udata, SDL_AudioStream *stream, int addAmount, int /*amount*/) {
	/* https://github.com/libsdl-org/sdl/blob/main/docs/README-migration.md#sdl_audioh */
	if (addAmount > 0) {
		Uint8 *data = SDL_stack_alloc(Uint8, addAmount);
		if (data) {
			my_audio_callback(udata, data, addAmount);
			SDL_PutAudioStreamData(stream, data, addAmount);
			SDL_stack_free(data);
		}
	}
}

static SoundR cur_audio;

static bool s_haveSoundOut = false;

void Sound_Stop()
{
#if dbglog_SoundStuff
	dbglog_writeln("enter Sound_Stop");
#endif

	if (cur_audio.wantplaying && s_haveSoundOut) {
		uint16_t retry_limit = 50; /* half of a second */

		cur_audio.wantplaying = false;

		while (kCenterTempSound != cur_audio.lastv
			&& --retry_limit != 0)
		{
			/*
				give time back, particularly important
				if got here on a suspend event.
			*/

#if dbglog_SoundStuff
			dbglog_writeln("busy, so sleep");
#endif

			(void) SDL_Delay(10);
		}

#if dbglog_SoundStuff
		if (kCenterTempSound == cur_audio.lastv) {
			dbglog_writeln("reached kCenterTempSound");
		} else {
			dbglog_writeln("retry limit reached");
		}
#endif

		SDL_PauseAudioDevice(
			SDL_GetAudioStreamDevice(stream)
		);
	}

#if dbglog_SoundStuff
	dbglog_writeln("leave Sound_Stop");
#endif
}

void Sound_Start()
{
	if ((! cur_audio.wantplaying) && s_haveSoundOut) {
		Sound_Start0();
		cur_audio.lastv = kCenterTempSound;
		cur_audio.HaveStartedPlaying = false;
		cur_audio.wantplaying = true;

		SDL_ResumeAudioDevice(
			SDL_GetAudioStreamDevice(stream)
		);
	}
}

void Sound_UnInit()
{
	if (s_haveSoundOut) {
		SDL_DestroyAudioStream(stream);
	}
}

#define SOUND_SAMPLERATE 22255 /* = round(7833600 * 2 / 704) */

bool Sound_Init()
{
#if dbglog_OSGInit
	dbglog_writeln("enter Sound_Init");
#endif

	SDL_AudioSpec desired;

	Sound_Init0();

	cur_audio.fTheSoundBuffer = s_soundBuffer;
	cur_audio.fPlayOffset = &ThePlayOffset;
	cur_audio.fFillOffset = &TheFillOffset;
	cur_audio.fMinFilledSoundBuffs = &MinFilledSoundBuffs;
	cur_audio.wantplaying = false;

	desired.freq = SOUND_SAMPLERATE;
	desired.format =
	SDL_AUDIO_S16;

	desired.channels = 1;
	stream = SDL_OpenAudioDeviceStream(
		SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
		&desired,
		(SDL_AudioStreamCallback)sdl3_audio_callback,
		(void *)&cur_audio
	);

	/* Open the audio device */
	if (
		stream == nullptr
	) {
		fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
	} else {
		s_haveSoundOut = true;

		Sound_Start();
			/*
				This should be taken care of by LeaveSpeedStopped,
				but since takes a while to get going properly,
				start early.
			*/
	}

	return true; /* keep going, even if no sound */
}

void Sound_EndWrite(uint16_t actL)
{
	if (Sound_EndWrite0(actL)) {
	}
}

void Sound_SecondNotify()
{
	if (s_haveSoundOut) {
		Sound_SecondNotify0();
	}
}

bool Sound_AllocBuffer()
{
	return AllocBlock((uint8_t **)&s_soundBuffer, dbhBufferSize, false);
}

void Sound_FreeBuffer()
{
	free(s_soundBuffer);
	s_soundBuffer = nullptr;
}
