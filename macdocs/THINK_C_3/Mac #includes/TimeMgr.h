
/*
 *  TimeMgr.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef	_TimeMgr_
#define _TimeMgr_

#ifndef	_MacTypes_
#include "MacTypes.h"
#endif


typedef struct TMTask {
	struct QElem		*qLink;
	int					qType;
	ProcPtr				tmAddr;
	long				tmCount;
} TMTask;


#endif _TimeMgr_