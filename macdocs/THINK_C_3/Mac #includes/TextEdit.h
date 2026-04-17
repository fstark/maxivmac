
/*
 *  TextEdit.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef	_TextEdit_
#define _TextEdit_

#ifndef	_Quickdraw_
#include "Quickdraw.h"
#endif

/* justifications */
enum { teForceLeft = -2, teJustRight, teJustLeft, teJustCenter };

typedef	char Chars[1], *CharsPtr, **CharsHandle;

typedef	struct
	{
	Rect	destRect ;
	Rect	viewRect ;
	Rect	selRect ;
	int		lineHeight ;
	int		fontAscent ;
	Point	selPoint ;
	int		selStart ;
	int		selEnd ;
	int		active ;
	ProcPtr	wordBreak ;
	ProcPtr	clikLoop ;
	long	clickTime ;
	int		clickLoc ;
	long	caretTime ;
	int		caretState ;
	int		just ;
	int		teLength ;
	Handle	hText ;
	int		recalBack ;
	int		recalLines;
	int		clikStuff ;
	int		crOnly ;
	int		txFont ;
	char	txFace ;
	int		txMode ;
	int		txSize ;
	GrafPtr	inPort ;
	ProcPtr	highHook ;
	ProcPtr caretHook ;
	int		nLines ;
	int		lineStarts[];
	} TERec, *TEPtr, **TEHandle ;


/* ---------- new TE stuff ("with style") ---------- */


#define doFont			1
#define doFace			2
#define doSize			4
#define doColor			8
#define doAll			15
#define addSize			16
#define doToggle		32

typedef enum {
	intEOLHook, intDrawHook, intWidthHook, intHitTestHook
} TEHook;


/*  avoid having to bring in all of Color Quickdraw  */ 
typedef struct _RGBColor{
	unsigned short	red;
	unsigned short	green;
	unsigned short	blue;
} _RGBColor;
		

typedef struct StyleRun{
	short			startChar;
	short			styleIndex;
} StyleRun;

typedef struct STElement{
	short			stCount;
	short			stHeight;
	short			stAscent;
	short			stFont;
	Style			stFace;
	short			stSize;
	_RGBColor		stColor;
} STElement;

typedef STElement TEStyleTable[1777], *STPtr, **STHandle;
		
typedef struct LHElement{
	short			lhHeight;
	short			lhAscent;
} LHElement;
						  
typedef LHElement LHTable[8001], *LHPtr, **LHHandle;

typedef struct ScrpSTElement{
	long			scrpStartChar;
	short			scrpHeight;
	short			scrpAscent;
	short			scrpFont;
	Style			scrpFace;
	short			scrpSize;
	_RGBColor		scrpColor;
} ScrpSTElement;
		
typedef ScrpSTElement ScrpSTTable[1601];

typedef struct StScrpRec{
	short			scrpNStyles;
	ScrpSTTable		scrpStyleTab;
} StScrpRec, *StScrpPtr, **StScrpHandle;
	  
typedef struct NullStRec{
	long			teReserved;
	StScrpHandle	nullScrap;
} NullStRec, *NullStPtr, **NullStHandle;

typedef struct TEStyleRec{
	short			nRuns;
	short			nStyles;
	STHandle		styleTab;
	LHHandle		lhTab;
	long			teRefCon;
	NullStHandle	nullStyle;
	StyleRun		runs[8001];
} TEStyleRec, *TEStylePtr, **TEStyleHandle;

typedef struct TextStyle{
	short			tsFont;
	Style			tsFace;
	short			tsSize;
	_RGBColor		tsColor;
} TextStyle;


/* ---------- */


/*  functions returning non-integral values  */
pascal TEHandle TENew();
pascal CharsHandle TEGetText();
pascal Handle TEScrapHandle();
pascal TEHandle TEStylNew();
pascal TEStyleHandle GetStylHandle();
pascal StScrpHandle GetStylScrap();
/*pascal Point TEGetPoint();  -- actually returns a long  */

/*  low-memory globals  */
extern int TEScrpLength : 0xAB0;
extern Handle TEScrpHandle : 0xAB4;


#endif _TextEdit_