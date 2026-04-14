#include "platform/sdl_sound.h"

#include "platform/common/osglu_ui.h"
#include "platform/common/osglu_ud.h"
#include "platform/common/osglu_common.h"
#include "platform/platform.h"
#include "platform/common/tick_timer.h"

#include <SDL3/SDL.h>

static constexpr int kLn2SoundBuffers = 4; /* kSoundBuffers must be a power of two */
static constexpr int kSoundBuffers = 1 << kLn2SoundBuffers;

static constexpr int kDesiredMinFilledSoundBuffs = 3;
/*
	if too big then sound lags behind emulation.
	if too small then sound will have pauses.
*/

static constexpr int kLnOneBuffLen = 9;
static constexpr int kLnAllBuffLen = kLn2SoundBuffers + kLnOneBuffLen;
static constexpr uint32_t kOneBuffLen = 1u << kLnOneBuffLen;
static constexpr uint32_t kAllBuffLen = 1u << kLnAllBuffLen;
static constexpr int kLnOneBuffSz = kLnOneBuffLen + 1;
static constexpr int kLnAllBuffSz = kLnAllBuffLen + 1;
static constexpr uint32_t kOneBuffSz = 1u << kLnOneBuffSz;
static constexpr uint32_t kAllBuffSz = 1u << kLnAllBuffSz;
static constexpr uint32_t kOneBuffMask = kOneBuffLen - 1;
static constexpr uint32_t kAllBuffMask = kAllBuffLen - 1;
static constexpr uint32_t kDbhBufferSize = kAllBuffSz + kOneBuffSz;

static constexpr bool kDbglogSoundStuff = false;
static constexpr bool kDbglogSoundBuffStats = false;
static constexpr bool kDbglogOsgInit = false;

using SoundSample = uint16_t;

static constexpr SoundSample kCenterTempSound = 0x8000;
static constexpr SoundSample kAudioStepVal = 0x0040;
static constexpr int kSoundSampleRate = 22255; /* = round(7833600 * 2 / 704) */

static constexpr SoundSample SampleFromNative(SoundSample v)
{
	return v + kCenterSound;
}
static constexpr SoundSample SampleToNative(SoundSample v)
{
	return v - kCenterSound;
}

static SoundSamplePtr s_soundBuffer = nullptr;
static volatile uint16_t s_playOffset;
static volatile uint16_t s_fillOffset;
static volatile uint16_t s_minFilledSoundBuffs;
[[maybe_unused]] static uint16_t s_maxFilledSoundBuffs;
static uint16_t s_writeOffset;

static SDL_AudioStream *s_stream = nullptr;

static void Sound_Init0()
{
	s_playOffset = 0;
	s_fillOffset = 0;
	s_writeOffset = 0;
}

static void Sound_Start0()
{
	s_minFilledSoundBuffs = kSoundBuffers + 1;
	if constexpr (kDbglogSoundBuffStats)
	{
		s_maxFilledSoundBuffs = 0;
	}
}

SoundSamplePtr Sound_BeginWrite(uint16_t n, uint16_t *actL)
{
	uint16_t toFillLen = kAllBuffLen - (s_writeOffset - s_playOffset);
	uint16_t writeBuffContig = kOneBuffLen - (s_writeOffset & kOneBuffMask);

	if (writeBuffContig < n)
	{
		n = writeBuffContig;
	}
	if (toFillLen < n)
	{
		/* overwrite previous buffer */
		if constexpr (kDbglogSoundStuff)
		{
			dbglog_writeln("sound buffer over flow");
		}
		s_writeOffset -= kOneBuffLen;
	}

	*actL = n;
	return s_soundBuffer + (s_writeOffset & kAllBuffMask);
}

static void ConvertSoundBlockToNative(SoundSamplePtr p)
{
	for (int i = kOneBuffLen; --i >= 0;)
	{
		*p++ -= 0x8000;
	}
}

static void Sound_WroteABlock()
{
	uint16_t prevWriteOffset = s_writeOffset - kOneBuffLen;
	SoundSamplePtr p = s_soundBuffer + (prevWriteOffset & kAllBuffMask);

	if constexpr (kDbglogSoundStuff)
	{
		dbglog_writeln("enter Sound_WroteABlock");
	}

	ConvertSoundBlockToNative(p);

	s_fillOffset = s_writeOffset;

	if constexpr (kDbglogSoundBuffStats)
	{
		uint16_t toPlayLen = s_fillOffset - s_playOffset;
		uint16_t toPlayBuffs = toPlayLen >> kLnOneBuffLen;

		if (toPlayBuffs > s_maxFilledSoundBuffs)
		{
			s_maxFilledSoundBuffs = toPlayBuffs;
		}
	}
}

static bool Sound_EndWrite0(uint16_t actL)
{
	s_writeOffset += actL;

	if ((s_writeOffset & kOneBuffMask) != 0)
	{
		return false;
	}

	/* just finished a block */
	Sound_WroteABlock();
	return true;
}

static void Sound_SecondNotify0()
{
	if (s_minFilledSoundBuffs > kSoundBuffers)
	{
		return;
	}

	if (s_minFilledSoundBuffs > kDesiredMinFilledSoundBuffs)
	{
		if constexpr (kDbglogSoundStuff)
		{
			dbglog_writeln("s_minFilledSoundBuffs too high");
		}
		IncrNextTime();
	}
	else if (s_minFilledSoundBuffs < kDesiredMinFilledSoundBuffs)
	{
		if constexpr (kDbglogSoundStuff)
		{
			dbglog_writeln("s_minFilledSoundBuffs too low");
		}
		++g_trueEmulatedTime;
	}

	if constexpr (kDbglogSoundBuffStats)
	{
		dbglog_writelnNum("s_minFilledSoundBuffs", s_minFilledSoundBuffs);
		dbglog_writelnNum("s_maxFilledSoundBuffs", s_maxFilledSoundBuffs);
		s_maxFilledSoundBuffs = 0;
	}

	s_minFilledSoundBuffs = kSoundBuffers + 1;
}

static void SoundRampTo(SoundSample *lastVal, SoundSample dstVal, SoundSamplePtr *stream, int *len)
{
	SoundSamplePtr p = *stream;
	int n = *len;
	SoundSample v1 = *lastVal;

	while (v1 != dstVal && n != 0)
	{
		if (v1 > dstVal)
		{
			SoundSample diff = v1 - dstVal;
			v1 = (diff > kAudioStepVal) ? v1 - kAudioStepVal : dstVal;
		}
		else
		{
			SoundSample diff = dstVal - v1;
			v1 = (diff > kAudioStepVal) ? v1 + kAudioStepVal : dstVal;
		}

		--n;
		*p++ = SampleToNative(v1);
	}

	*stream = p;
	*len = n;
	*lastVal = v1;
}

struct SoundState
{
	SoundSamplePtr soundBuffer_;
	volatile uint16_t *playOffset_;
	volatile uint16_t *fillOffset_;
	volatile uint16_t *minFilledSoundBuffs_;

	volatile SoundSample lastv_;

	bool wantPlaying_;
	bool haveStartedPlaying_;
};

static void FillAudioCallback(void *udata, uint8_t *rawStream, int len)
{
	auto *datp = static_cast<SoundState *>(udata);
	SoundSamplePtr curSoundBuffer = datp->soundBuffer_;
	uint16_t curPlayOffset = *datp->playOffset_;
	SoundSample v1 = datp->lastv_;
	auto *dst = reinterpret_cast<SoundSamplePtr>(rawStream);

	len >>= 1; /* convert byte length to sample count (16-bit samples) */

	if constexpr (kDbglogSoundStuff)
	{
		dbglog_writeln("Enter FillAudioCallback");
		dbglog_writelnNum("len", len);
	}

	while (len > 0)
	{
		uint16_t toPlayLen = *datp->fillOffset_ - curPlayOffset;
		uint16_t filledSoundBuffs = toPlayLen >> kLnOneBuffLen;

		if (!datp->wantPlaying_)
		{
			if constexpr (kDbglogSoundStuff)
			{
				dbglog_writeln("playing end transition");
			}

			SoundRampTo(&v1, kCenterTempSound, &dst, &len);
			toPlayLen = 0;
		}
		else if (!datp->haveStartedPlaying_)
		{
			if constexpr (kDbglogSoundStuff)
			{
				dbglog_writeln("playing start block");
			}

			if ((toPlayLen >> kLnOneBuffLen) < 8)
			{
				toPlayLen = 0;
			}
			else
			{
				SoundSamplePtr p = datp->soundBuffer_ + (curPlayOffset & kAllBuffMask);
				SoundSample v2 = SampleFromNative(*p);

				if constexpr (kDbglogSoundStuff)
				{
					dbglog_writeln("have enough samples to start");
				}

				SoundRampTo(&v1, v2, &dst, &len);

				if (v1 == v2)
				{
					if constexpr (kDbglogSoundStuff)
					{
						dbglog_writeln("finished start transition");
					}
					datp->haveStartedPlaying_ = true;
				}
			}
		}

		if (len == 0)
		{
			/* done */
			if (filledSoundBuffs < *datp->minFilledSoundBuffs_)
			{
				*datp->minFilledSoundBuffs_ = filledSoundBuffs;
			}
		}
		else if (toPlayLen == 0)
		{
			if constexpr (kDbglogSoundStuff)
			{
				dbglog_writeln("under run");
			}

			for (int i = 0; i < len; ++i)
			{
				*dst++ = SampleToNative(v1);
			}
			*datp->minFilledSoundBuffs_ = 0;
			break;
		}
		else
		{
			uint16_t playBuffContig = kAllBuffLen - (curPlayOffset & kAllBuffMask);
			SoundSamplePtr p = curSoundBuffer + (curPlayOffset & kAllBuffMask);

			if (toPlayLen > playBuffContig)
			{
				toPlayLen = playBuffContig;
			}
			if (toPlayLen > static_cast<uint16_t>(len))
			{
				toPlayLen = static_cast<uint16_t>(len);
			}

			for (int i = 0; i < toPlayLen; ++i)
			{
				*dst++ = *p++;
			}
			v1 = SampleFromNative(p[-1]);

			curPlayOffset += toPlayLen;
			len -= toPlayLen;

			*datp->playOffset_ = curPlayOffset;
		}
	}

	datp->lastv_ = v1;
}

static void SDLCALL Sdl3AudioCallback(void *udata, SDL_AudioStream *stream, int addAmount,
									  int /*amount*/)
{
	if (addAmount > 0)
	{
		auto *data = static_cast<uint8_t *>(SDL_stack_alloc(Uint8, addAmount));
		if (data)
		{
			FillAudioCallback(udata, data, addAmount);
			SDL_PutAudioStreamData(stream, data, addAmount);
			SDL_stack_free(data);
		}
	}
}

static SoundState s_curAudio;

static bool s_haveSoundOut = false;

void Sound_Stop()
{
	if constexpr (kDbglogSoundStuff)
	{
		dbglog_writeln("enter Sound_Stop");
	}

	if (s_curAudio.wantPlaying_ && s_haveSoundOut)
	{
		uint16_t retryLimit = 50; /* half of a second */

		s_curAudio.wantPlaying_ = false;

		while (s_curAudio.lastv_ != kCenterTempSound && --retryLimit != 0)
		{
			/*
				give time back, particularly important
				if got here on a suspend event.
			*/
			if constexpr (kDbglogSoundStuff)
			{
				dbglog_writeln("busy, so sleep");
			}

			SDL_Delay(10);
		}

		if constexpr (kDbglogSoundStuff)
		{
			if (s_curAudio.lastv_ == kCenterTempSound)
			{
				dbglog_writeln("reached kCenterTempSound");
			}
			else
			{
				dbglog_writeln("retry limit reached");
			}
		}

		SDL_PauseAudioDevice(SDL_GetAudioStreamDevice(s_stream));
	}

	if constexpr (kDbglogSoundStuff)
	{
		dbglog_writeln("leave Sound_Stop");
	}
}

void Sound_Start()
{
	if (!s_curAudio.wantPlaying_ && s_haveSoundOut)
	{
		Sound_Start0();
		s_curAudio.lastv_ = kCenterTempSound;
		s_curAudio.haveStartedPlaying_ = false;
		s_curAudio.wantPlaying_ = true;

		SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(s_stream));
	}
}

void Sound_UnInit()
{
	if (s_haveSoundOut)
	{
		SDL_DestroyAudioStream(s_stream);
	}
}

bool Sound_Init()
{
	if constexpr (kDbglogOsgInit)
	{
		dbglog_writeln("enter Sound_Init");
	}

	Sound_Init0();

	s_curAudio.soundBuffer_ = s_soundBuffer;
	s_curAudio.playOffset_ = &s_playOffset;
	s_curAudio.fillOffset_ = &s_fillOffset;
	s_curAudio.minFilledSoundBuffs_ = &s_minFilledSoundBuffs;
	s_curAudio.wantPlaying_ = false;

	SDL_AudioSpec desired;
	desired.freq = kSoundSampleRate;
	desired.format = SDL_AUDIO_S16;
	desired.channels = 1;

	s_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired,
										 Sdl3AudioCallback, static_cast<void *>(&s_curAudio));

	if (s_stream == nullptr)
	{
		fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
	}
	else
	{
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
	Sound_EndWrite0(actL);
}

void Sound_SecondNotify()
{
	if (s_haveSoundOut)
	{
		Sound_SecondNotify0();
	}
}

bool Sound_AllocBuffer()
{
	return AllocBlock(reinterpret_cast<uint8_t **>(&s_soundBuffer), kDbhBufferSize, false);
}

void Sound_FreeBuffer()
{
	free(s_soundBuffer);
	s_soundBuffer = nullptr;
}
