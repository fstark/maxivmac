
/*
 *  SoundDvr.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef	_SoundDvr_
#define _SoundDvr_

/* synthesizer modes */
enum { swMode = -1, ffMode, ftMode };

typedef	char	FreeWave[1]	;	/* array[0..30000] of byte; */

typedef	struct	FFSynthRec
	{
	int			mode;
	long		count;
	FreeWave	waveBytes;
	} FFSynthRec, *FFSynthPtr ;


typedef	struct	
	{
	int			count;
	int			amplitude;
	int			duration;
	} Tone ;
	
typedef Tone Tones[1];	/* array[0..5000] of Tone */

typedef	struct	
	{
	int			mode;
	Tones		triplets;
	} SWSynthRec, *SWSynthPtr ;

typedef char	Wave[256];
typedef	Wave *	WavePtr;

typedef	struct	
	{
	int			duration;
	long		sound1Rate;
	long		sound1Phase;
	long		sound2Rate;
	long		sound2Phase;
	long		sound3Rate;
	long		sound3Phase;
	long		sound4Rate;
	long		sound4Phase;
	WavePtr		sound1Wave;
	WavePtr		sound2Wave;
	WavePtr		sound3Wave;
	WavePtr		sound4Wave;
	} FTSoundRec, *FTSndRecPtr ;

typedef	struct	
	{
	int			mode;
	FTSndRecPtr	sndRec;
	} FTSynthRec, * FTSynthPtr;


/*  low-memory globals  */
extern char SdVolume : 0x260;


#endif _SoundDvr_