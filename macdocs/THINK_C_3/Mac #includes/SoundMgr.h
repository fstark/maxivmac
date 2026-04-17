
/*
 *  SoundMgr.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef _SoundMgr_
#define _SoundMgr_

#ifndef _MacTypes_
#include "MacTypes.h"
#endif


#define synthCodeRsrc		'snth'
#define soundListRsrc		'snd '

enum {
	noteSynth = 2,
	waveTableSynth,
	midiSynth,
	sampledSynth
};

#define twelfthRootTwo		1.05946309434


enum {
	nullCmd,
	initCmd,
	freeCmd,
	quietCmd,
	flushCmd,
	waitCmd = 10,
	pauseCmd,
	resumeCmd,
	callBackCmd,
	syncCmd,
	emptyCmd,
	tickleCmd = 20,
	requestNextCmd,
	howOftenCmd,
	wakeUpCmd,
	availableCmd,
	noteCmd = 40,
	restCmd,
	freqCmd,
	ampCmd,
	timbreCmd,
	waveTableCmd = 60,
	phaseCmd,
	soundCmd = 80,
	bufferCmd,
	rateCmd,
	midiDataCmd = 100
};

#define setPtrBit			0x8000
#define stdQLength			128


/* Error codes */
enum {
	badFormat = -206,
	badChannel,
	resProblem,
	queueFull,
	notEnoughHardware = -201,
	noHardware
};


/* Wave Table Synthesizer */
#define initChanLeft		0x02
#define initChanRight		0x03
#define initChan0			0x04
#define initChan1			0x05
#define initChan2			0x06
#define initChan3			0x07
#define initSRate22k		0x20
#define initSRate44k		0x30
#define initMono			0x80
#define initStereo			0xC0


/*typedef	long				Time;				/* in half milliseconds */
#define infiniteTime		0x7FFFFFFF


typedef struct SndCommand {
	short					cmd;
	short					param1;
	long					param2;
} SndCommand;


typedef struct ModifierStub {
	struct ModifierStub		*nextStub;
	ProcPtr					code;
	long					userInfo;
	long /* Time */			count;
	long /* Time */			every;
	char					flags;
	char					hState;
} ModifierStub, *ModifierStubPtr;


typedef struct SndChannel {
	struct SndChannel		*nextChan;
	ModifierStubPtr			firstMod;
	ProcPtr					callBack;
	long					userInfo;
	long /* Time */			wait;
	SndCommand				cmdInProgress;
	short					flags;
	short					qLength;
	short					qHead;
	short					qTail;
	SndCommand				queue[stdQLength];
} SndChannel, *SndChannelPtr;


typedef struct SoundHeader {
	Ptr					samplePtr;
	long				length;
	Fixed				sampleRate;
	long				loopStart;
	long				loopEnd;
	short				baseNote;
	char				sampleArea[];
} SoundHeader, *SoundHeaderPtr;


#endif _SoundMgr_