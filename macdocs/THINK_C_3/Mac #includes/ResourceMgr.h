
/*
 *  ResourceMgr.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef	_ResourceMgr_
#define _ResourceMgr_
	
#ifndef	_MacTypes_
#include "MacTypes.h"
#endif

/* resource attributes */
#define resSysRef	128
#define resSysHeap	64
#define resPurgeable	32
#define resLocked	16
#define resProtected	8
#define resPreload	4
#define resChanged	2

/* result codes	*/
enum {
	mapReadErr = -199,
	resAttrErr,
	rmvRefFailed,
	rmvResFailed,
	addRefFailed,
	addResFailed,
	resFNotFound,
	resNotFound
};

/* file attributes	*/
#define mapReadOnly	128
#define mapCompact	64
#define mapChanged	32

/* RomMapInsert values */
#define mapTrue		0xFFFF
#define mapFalse	0xFF00

/*  functions returning non-integral values  */
pascal Handle Get1Resource();
pascal Handle Get1IndResource();
pascal Handle Get1NamedResource();
pascal Handle RGetResource();

/*  low-memory globals  */
extern Handle TopMapHndl : 0xA50;
extern Handle SysMapHndl : 0xA54;
extern int SysMap : 0xA58;
extern int CurMap : 0xA5A;
extern Boolean ResLoad : 0xA5E;
extern int ResErr : 0xA60;
extern ProcPtr ResErrProc : 0xAF2;
extern char SysResName[] : 0xAD8;
extern int RomMapInsert : 0xB9E;


#endif _ResourceMgr_