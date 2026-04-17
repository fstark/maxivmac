
/*
 *  ControlMgr.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef	_ControlMgr_
#define _ControlMgr_

#ifndef	_MacTypes_
#include "MacTypes.h"
#endif	

#ifndef	_WindowMgr_
#include "WindowMgr.h"
#endif

/* control messages */
enum {
	drawCntl,
	testCntl,
	calcCRgns,
	initCntl,
	dispCntl,
	posCntl,
	thumbCntl,
	dragCntl,
	autoTrack
};

/* findcontrol result codes */
enum {
	inButton = 10,
	inCheckBox = 11,
	inUpButton = 20,
	inDownButton = 21,
	inPageUp = 22,
	inPageDown = 23,
	inThumb = 129
};

/* control def proc id's */
enum {
	pushButProc,
	checkBoxProc,
	radioButProc,
	useWFont = 8,
	scrollBarProc = 16
};

/*  Axis constraints for drag control  */
/*	#define noConstraint	0	defined in WindowMgr.h	*/
/*	#define hAxisOnly		1	defined in WindowMgr.h	*/
/*	#define vAxisOnly		2	defined in WindowMgr.h	*/


typedef	struct	ControlRecord
	{
	struct ControlRecord **	nextControl ;
	WindowPtr	contrlOwner ;
	Rect		contrlRect ;
	Byte		contrlVis ;
	Byte		contrlHilite ;
	int			contrlValue ;
	int			contrlMin ;
	int			contrlMax ;
	Handle		contrlDefProc ;
	Handle		contrlData ;
	ProcPtr		contrlAction ; 
	long		contrlRfCon ;
	Str255		contrlTitle ;
	} ControlRecord ;
typedef ControlRecord * ControlPtr ;
typedef ControlPtr *	ControlHandle ;


/*  functions returning non-integral values  */
pascal ControlHandle NewControl();
pascal ControlHandle GetNewControl();
pascal ProcPtr GetCtlAction();

#endif _ControlMgr_