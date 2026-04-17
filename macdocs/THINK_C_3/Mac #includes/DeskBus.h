
/*
 *  DeskBus.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef _DeskBus_
#define _DeskBus_

#ifndef _MacTypes_
#include "MacTypes.h"
#endif


typedef struct ADBOpBlock{
	Ptr				dataBuffPtr;
	Ptr				opServiceRtPtr;
	Ptr				opDataAreaPtr;
} ADBOpBlock, *ADBOpBPtr;

typedef struct ADBDataBlock{
	char			devType;
	char			origADBAddr;
	Ptr				dbServiceRtPtr;
	Ptr				dbDataAreaAddr;
} ADBDataBlock, *ADBDBlkPtr;

typedef struct ADBSetInfoBlock{
	Ptr				siServiceRtPtr;
	Ptr				siDataAreaAddr;
} ADBSetInfoBlock, *ADBSInfoPtr;

typedef char ADBAddress;

#endif _DeskBus_