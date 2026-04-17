
/*
 *  WindowMgr.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef	_WindowMgr_
#define _WindowMgr_

#ifndef	_Quickdraw_	
#include "Quickdraw.h"
#endif

/*  window def proc ID's  */
enum {
	documentProc,
	dBoxProc,
	plainDBox,
	altDBoxProc,
	noGrowDocProc,
	rDocProc = 16
};

/* types of windows  */
#define	dialogKind	2
#define userKind	8

/*  FindWindow values  */
enum {
	inDesk,
	inMenuBar,
	inSysWindow,
	inContent,
	inDrag,
	inGrow,
	inGoAway,
	inZoomIn,
	inZoomOut
};

/* Axis constraints for DragGrayRgn */
enum { noConstraint, hAxisOnly, vAxisOnly };

/*  window defproc messages  */
enum {
	wDraw,
	wHit,
	wCalcRgns,
	wNew,
	wDispose,
	wGrow,
	wDrawGIcon
};

/*  hit test codes  */
enum {
	wNoHit,
	wInContent,
	wInDrag,
	wInGrow,
	wInGoAway,
	wInZoomIn,
	wInZoomOut
};

/*  rsrc ID of desktop pattern  */
#define deskPatID	16


typedef	struct WindowRecord
	{
	GrafPort		port;
	int				windowKind;
	char			visible;
	char			hilited;
	char			goAwayFlag;
	char			spareFlag;
	RgnHandle 		strucRgn;
	RgnHandle 		contRgn;
	RgnHandle 		updateRgn;
	Handle			windowDefProc;
	Handle			dataHandle;
	StringHandle 	titleHandle;
	int				titleWidth;
	struct ControlRecord ** controlList;
	struct WindowRecord * nextWindow;
	PicHandle 		windowPic;
	long			refCon;
	} WindowRecord, *WindowPeek ;

typedef GrafPtr	WindowPtr;

typedef struct WStateData {
	Rect			userState;
	Rect			stdState;
} WStateData;


/*  functions returning non-integral values  */
pascal WindowPtr NewWindow();
pascal WindowPtr GetNewWindow();
pascal WindowPtr FrontWindow();
pascal PicHandle GetWindowPic();
pascal RgnHandle GetGrayRgn();

/*  low-memory globals  */
extern WindowPeek WindowList : 0x9D6;
extern int SaveUpdate : 0x9DA;
extern int PaintWhite : 0x9DC;
extern GrafPtr WMgrPort : 0x9DE;
extern RgnHandle GrayRgn : 0x9EE;
extern ProcPtr DragHook : 0x9F6;
extern Pattern DragPattern : 0xA34;
extern Pattern DeskPattern : 0xA3C;
extern WindowPtr CurActivate : 0xA64;
extern WindowPtr CurDeactive : 0xA68;
extern ProcPtr DeskHook : 0xA6C;
extern WindowPtr GhostWindow : 0xA84;


#endif _WindowMgr_