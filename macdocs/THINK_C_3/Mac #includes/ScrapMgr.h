
/*
 *  ScrapMgr.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef	_ScrapMgr_
#define _ScrapMgr_
	
#ifndef	_MacTypes_
#include "MacTypes.h"
#endif

/* result codes */
enum { noTypeErr = -102, noScrapErr = -100 };


typedef struct	ScrapStuff
	{
	long		scrapSize;
	Handle		scrapHandle;
	int			scrapCount;
	int 		scrapState;
	StringPtr 	scrapName; 
 	} ScrapStuff,* PScrapStuff;


/*  functions returning non-integral values  */
pascal PScrapStuff InfoScrap();

/*  low-memory globals  */
extern ScrapStuff ScrapInfo : 0x960;


#endif _ScrapMgr_