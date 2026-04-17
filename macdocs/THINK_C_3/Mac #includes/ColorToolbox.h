
/*
 *  ColorToolbox.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef	_ColorToolbox_
#define _ColorToolbox_

#ifndef _Color_
#include "Color.h"
#endif

#ifndef _ControlMgr_
#include "ControlMgr.h"
#endif


/* ---------- Control Manager ---------- */


	/* Constants for the colors of control parts */

enum {
	cFrameColor,
	cBodyColor,
	cTextColor,
	cThumbColor
};

	
typedef struct CtlCTab{
	long			ccSeed;
	short			ccRider;
	short			ctSize;
	ColorSpec		ctTable[4];
} CtlCTab, *CCTabPtr, **CCTabHandle;
					  	

typedef struct AuxCtlRec{
	Handle			acNext;
	ControlHandle	acOwner;
	CCTabHandle		acCTable;
	short			acFlags;
	long			acReserved;
	long			acRefCon;
} AuxCtlRec, *AuxCtlPtr, **AuxCtlHndl;


/* ---------- Menu Manager ---------- */


#define mctAllItems		-98
#define mctLastIDIndic	-99


typedef struct MCEntry{
	short	   		mctID;
	short	   		mctItem;
	RGBColor	   	mctRGB1;
	RGBColor	    mctRGB2;
	RGBColor	    mctRGB3;
	RGBColor		mctRGB4;
	short	    	mctReserved;
} MCEntry, *MCEntryPtr;

typedef MCEntry MCTable[1], *MCTablePtr, **MCTableHandle;


/* ---------- Window Manager ---------- */


/*  color table entries  */
enum {
	wContentColor,
	wFrameColor,
	wTextColor,
	wHiliteColor,
	wTitleBarColor
};

typedef struct AuxWinRec{
	struct AuxWinRec **awNext;
	WindowPtr		awOwner;
	CTabHandle		awCTable;
	Handle			dialogCTable;
	long			awFlags;
	long			awResrv;
	long			awRefCon;
} AuxWinRec, *AuxWinPtr, **AuxWinHndl;
	
typedef struct WinCTab{
	long			wCSeed;
	short			wCReserved;
	short			ctSize;
	ColorSpec		ctTable[5];
} WinCTab, *WCTabPtr, **WCTabHandle;


/* ---------- */


/*  functions returning non-integral values  */
pascal WindowPtr NewCDialog();		/*  DialogPtr  */
pascal MCTableHandle GetMCInfo();
pascal MCEntryPtr GetMCEntry();
pascal WindowPtr NewCWindow();
pascal WindowPtr GetNewCWindow();


/*  low-memory globals  */
extern AuxWinHndl AuxWinHead : 0xCD0;
extern AuxCtlHndl AuxCtlHead : 0xCD4;
extern MCTableHandle MenuCInfo : 0xD50;


#endif _ColorToolbox_