
/*
 *  PackageMgr.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef	_PackageMgr_
#define _PackageMgr_

#ifndef _MacTypes_
#include "MacTypes.h"
#endif

/* package IDs */
enum {
	listMgr,
	dskInit = 2,
	stdFile,
	flPoint,
	trFunc,
	intUtil,
	bdConv
};

/*  low-memory globals  */
extern Handle AppPacks[] : 0xAB8;


#endif _PackageMgr_