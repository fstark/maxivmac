
/*
 *  ListMgr.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef	_ListMgr_
#define _ListMgr_

#ifndef	_ControlMgr_
#include "ControlMgr.h"
#endif


/*  masks for automatic scrolling  */
#define lDoHAutoScroll	1
#define lDoVAutoScroll	2

/*  masks for selection flags  */
#define lNoNilHilite	2
#define lUseSense		4
#define lNoRect			8
#define lNoExtend		16
#define lNoDisjoint		32
#define lExtendDrag		64
#define lOnlyOne		128

/*  message to defproc  */
enum { lInitMsg, lDrawMsg, lHiliteMsg, lCloseMsg };


typedef Point Cell;

typedef char DataArray[32000];
typedef DataArray *DataPtr, **DataHandle;

typedef struct ListRec {
	Rect				rView;
	GrafPtr				port;
	Point				indent;
	Point				cellSize;
	Rect				visible;
	ControlHandle		vScroll;
	ControlHandle		hScroll;
	char				selFlags;
	Boolean				lActive;
	char				lReserved;
	char				listFlags;
	long				clikTime;
	Point				clikLoc;
	Point				mouseLoc;
	Ptr					lClikLoop;
	Cell				lastClick;
	long				refCon;
	Handle				listDefProc;
	Handle				userHandle;
	Rect				dataBounds;
	DataHandle			cells;
	int					maxIndex;
	int					cellArray[];
} ListRec, *ListPtr, **ListHandle;


/*  functions returning non-integral values  */
pascal ListHandle LNew();
/*pascal Cell LLastClick();  -- actually returns a long  */


#endif _ListMgr_