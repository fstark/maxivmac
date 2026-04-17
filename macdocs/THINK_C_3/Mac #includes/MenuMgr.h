
/*
 *  MenuMgr.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef	_MenuMgr_
#define _MenuMgr_
	
#ifndef	_MacTypes_
#include "MacTypes.h"
#endif

#define maxItem		31

/* special chars */
enum {
	noMark,
	commandMark = 0x11,
	checkMark,
	diamondMark,
	appleMark
};

/*  menu defproc messages  */
enum { mDrawMsg, mChooseMsg, mSizeMsg, mPopUpMsg };

/*  MDEF id  */
#define textMenuProc	0

/*  hierarchical menus  */
#define hMenuCmd		0x1B
#define hierMenu		-1


typedef struct
	{
	int		menuID;
	int		menuWidth;
	int		menuHeight;
	Handle 	menuProc;
	long	enableFlags; 
	Str255	menuData;
	} MenuInfo,* MenuPtr, ** MenuHandle;


/*  functions returning non-integral values  */
pascal MenuHandle NewMenu();
pascal MenuHandle GetMenu();
pascal Handle GetNewMBar();
pascal Handle GetMenuBar();
pascal MenuHandle GetMHandle();

/*  low-memory globals  */
extern int TopMenuItem : 0xA0A;
extern int AtMenuBottom : 0xA0C;
extern Handle MenuList : 0xA1C;
extern int MBarEnable : 0xA20;
extern int MenuFlash : 0xA24;
extern int TheMenu : 0xA26;
extern ProcPtr MBarHook : 0xA2C;
extern ProcPtr MenuHook : 0xA30;
extern long MenuDisable : 0xB54;
extern int MBarHeight : 0xBAA;


#endif _MenuMgr_