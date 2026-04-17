
/*
 *  VRetraceMgr.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef _VRetraceMgr_
#define _VRetraceMgr_

#ifndef	_MacTypes_
#include "MacTypes.h"
#endif

typedef	struct VBLTask
	{
	struct QElem *	qLink;
 	int				qType;
	ProcPtr			vblAddr;
	int				vblCount;
	int				vblPhase;
	} VBLTask , *VBLQElPtr;

#define inVBL	0x40


/*  functions returning non-integral values  */
pascal struct QHdr *GetVBLQHdr();

/*  low-memory globals  */
extern struct QHdr VBLQueue : 0x160;


#endif _VRetraceMgr_