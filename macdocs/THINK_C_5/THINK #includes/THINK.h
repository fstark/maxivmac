
/*
 *  THINK.h - THINK C extensions to Apple headers
 *
 *  Copyright (c) 1991 Symantec Corporation.  All rights reserved.
 *
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985-1991.
 *
 *  This file contains material that has traditionally been part of
 *  the THINK C interfaces, but which does not appear in the Apple
 *  headers we are now using.  It is provided for backward compatibility.
 *
 */

#ifndef __THINK__
#define __THINK__

#ifndef __TYPES__
#include <Types.h>
#endif

#ifndef __FILES__
#include <Files.h>
#endif

#ifndef __QUICKDRAW__
#include <Quickdraw.h>
#endif


	/*  SystemEdit arguments  */

enum {
	undoCmd,
	cutCmd = 2,
	copyCmd,
	pasteCmd,
	clearCmd
};


	/*  dCtlFlags bits  */

#define	dNeedLock		0x4000
#define dNeedTime		0x2000
#define dNeedGoodBye	0x1000
#define dStatEnable		0x0800
#define dCtlEnable		0x0400
#define dWritEnable		0x0200
#define dReadEnable		0x0100
#define drvrActive		0x0080
#define dRAMBased		0x0040
#define dOpened			0x0020


	/*  I/O traps  */

#define aRdCmd			2
#define aWrCmd			3
#define asyncTrpBit		0x0400
#define noQueueBit		0x0200


	/*  buttons  */

enum {
	OK = 1,
	Cancel
};


	/*  QuickDraw globals  */

extern GrafPtr	thePort;
extern Pattern	white;
extern Pattern	black;
extern Pattern	gray;
extern Pattern	ltGray;
extern Pattern	dkGray;
extern Cursor	arrow;
extern BitMap	screenBits;
extern long		randSeed;


	/*  Rect macros  */

#define topLeft(r)		(((Point *) &(r))[0])
#define botRight(r)		(((Point *) &(r))[1])


	/*  Booleans  */

#define TRUE			1
#define FALSE			0


	/*  param blocks  */

typedef IOParam ioParam;
typedef FileParam fileParam;
typedef VolumeParam volumeParam;
typedef CntrlParam cntrlParam;


	/*  multi-segment non-applications  */

void UnloadA4Seg(void *);


	/*  menu bar height (from Script.h)  */

#define GetMBarHeight() (* (short*) 0x0BAA)


#endif // __THINK__
