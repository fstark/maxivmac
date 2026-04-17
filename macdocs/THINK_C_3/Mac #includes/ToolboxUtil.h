
/*
 *  ToolboxUtil.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef	_ToolboxUtil_
#define _ToolboxUtil_
	
#ifndef	_Quickdraw_
#include "Quickdraw.h"
#endif

#define	sysPatListID	0

enum {
	iBeamCursor = 1,
	crossCursor,
	plusCursor,
	watchCursor
};

typedef	struct	
	{
	long	hiLong;
	long	loLong;
	} Int64Bit ;


/*  functions returning non-integral values  */
pascal StringHandle NewString();
pascal StringHandle GetString();
pascal Handle GetIcon();
pascal PatHandle GetPattern();
pascal CursHandle GetCursor();
pascal PicHandle GetPicture();
double Fix2X(Fixed);
double Frac2X(Fract);


#endif _ToolboxUtil_