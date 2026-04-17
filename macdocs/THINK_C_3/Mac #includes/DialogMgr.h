
/*
 *  DialogMgr.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef	_DialogMgr_
#define	_DialogMgr_
	
#ifndef	_MacTypes_
#include "MacTypes.h"
#endif

#ifndef	_WindowMgr_
#include "WindowMgr.h"
#endif

#ifndef	_TextEdit_
#include "TextEdit.h"
#endif

/*  item types  */
enum {
	userItem = 0,
	ctrlItem = 4,
	statText = 8,
	editText = 16,
	iconItem = 32,
	picItem = 64,
	itemDisable = 128
};

/*  control item types  */
enum {
	btnCtrl,
	chkCtrl,
	radCtrl,
	resCtrl
};

/*  buttons  */
enum {
	OK = 1,
	Cancel
};

/*  alert icons  */
enum {
	stopIcon,
	noteIcon,
	cautionIcon
};
#define	ctnIcon		cautionIcon


typedef	struct	DialogRecord
	{
	WindowRecord	window;
	Handle			items ;
	TEHandle		textH;
	int				editField;
	int				editOpen;
	int				aDefItem;
	} DialogRecord ;
typedef DialogRecord * 	DialogPeek;
typedef WindowPtr 	DialogPtr ;

typedef	struct	DialogTemplate
	{
	Rect			boundsRect;
	int				procID;
	char			visible;
	char			filler1;
	char			goAwayFlag;
	char			filler2;
	long			refCon;
	int				itemsID;
	Str255			title;
	} DialogTemplate;
typedef DialogTemplate *	DialogTPtr;
typedef	DialogTPtr *		DialogTHndl;

typedef	struct	AlertTemplate
	{
	Rect			boundsRect;
	int				itemsID;
	unsigned int	boldItm4	: 1; /* this is StageList */
	unsigned int	boxDrwn4	: 1;
	unsigned int	sound4		: 2;	
	unsigned int	boldItm3	: 1;
	unsigned int	boxDrwn3	: 1;
	unsigned int	sound3		: 2;	
	unsigned int	boldItm2	: 1;
	unsigned int	boxDrwn2	: 1;
	unsigned int	sound2		: 2;	
	unsigned int	boldItm1	: 1;
	unsigned int	boxDrwn1	: 1;
	unsigned int	sound1		: 2;
	} AlertTemplate;
typedef AlertTemplate *		AlertTPtr;
typedef	AlertTPtr *		AlertTHndl;



/*  functions returning non-integral values  */
pascal DialogPtr NewDialog();
pascal DialogPtr GetNewDialog();

/*  low-memory globals  */
extern ProcPtr ResumeProc : 0xA8C;

#endif _DialogMgr_