
/*
 *  MemoryMgr.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */


#ifndef	_MemoryMgr_
#define _MemoryMgr_

#ifndef	_MacTypes_
#include "MacTypes.h"
#endif

/* maximum block size */
#define	maxSize		0x800000

/* result codes	*/
enum {
	memLockedErr = -117,
	memSCErr,
	memBCErr,
	memPCErr,
	memAZErr,
	memPurErr,
	memWZErr,
	memAdrErr,
	nilHandleErr,
	memFullErr,
	memROZErr = -99
};

typedef	struct 
	{
	Ptr			bkLim;
	Ptr			purgePtr;
	Ptr			hFstFree;
	long		zcbFree;
	ProcPtr		gzProc;
	int			moreMast;	
	int			flags;	
	int			cntRel;
	int			maxRel;
	int			cntNRel;
	int			maxNRel;
	int			cntEmpty;
	int			cntHandles;
	long		minCBFree;
	ProcPtr		purgeProc;	
	Ptr			sparePtr;
	Ptr			allocPtr;
	int			heapData;
	}Zone, * THz ;		

/*  functions returning non-integral values  */
pascal THz GetZone();
pascal THz SystemZone();
pascal THz ApplicZone();
pascal THz HandleZone();
pascal Handle RecoverHandle();
pascal THz PtrZone();
pascal Handle GZSaveHnd();
pascal Ptr TopMem();
pascal Ptr GetApplLimit();
pascal Handle NewEmptyHandle();

/*  low-memory globals  */
extern Ptr MemTop : 0x108;
extern Ptr BufPtr : 0x10C;
extern Ptr HeapEnd : 0x114;
extern THz TheZone : 0x118;
extern Ptr ApplLimit : 0x130;
extern int MemErr : 0x220;
extern THz SysZone : 0x2A6;
extern THz ApplZone : 0x2AA;
extern Ptr ROMBase : 0x2AE;
extern Ptr RAMBase : 0x2B2;
extern long Lo3Bytes : 0x31A;
extern ProcPtr IAZNotify : 0x33C;
extern Ptr CurrentA5 : 0x904;
extern Ptr CurStackBase : 0x908;


#endif _MemoryMgr_