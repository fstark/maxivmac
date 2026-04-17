
/*
 *  Quickdraw.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef	_Quickdraw_
#define _Quickdraw_
	
#ifndef	_MacTypes_
#include "MacTypes.h"
#endif


/*  transfer modes */
enum {
	srcCopy,
	srcOr,
	srcXor,
	srcBic,
	notSrcCopy,
	notSrcOr,
	notSrcXor,
	notSrcBic,
	patCopy,
	patOr,
	patXor,
	patBic,
	notPatCopy,
	notPatOr,
	notPatXor,
	notPatBic
};

/* colors */
#define	blackColor		33
#define	whiteColor		30
#define	redColor		205
#define	greenColor		341
#define	blueColor		409
#define	cyanColor		273
#define	magentaColor	137
#define	yellowColor		69

/* standard picture comments */
enum { picLParen, picRParen };

/* color mapping */
enum {
	normalBit,
	inverseBit,
	blueBit,
	greenBit,
	redBit,
	blackBit,
	yellowBit,
	magentaBit,
	cyanBit
};

typedef	char QDByte, *QDPtr, **QDHandle;

typedef enum {
	bold = 1,
	italic = 2,
	underline = 4,
	outline = 8,
	shadow = 16,
	condense = 32,
	extend = 64
} Style;

typedef	enum { frame, paint, erase, invert, fill } GrafVerb;

typedef unsigned char Pattern[8];
typedef Pattern *PatPtr, **PatHandle;

typedef int	Bits16[16];

typedef	struct	
	{
	int		ascent;
	int		descent;
	int		widMax;
	int		leading;
	} FontInfo;
	
typedef	struct	
	{
	QDPtr	baseAddr;
	int		rowBytes;
	Rect	bounds;
	} BitMap;
	
typedef	struct
	{
	Bits16	data;
	Bits16	mask;
	Point	hotSpot;
	} Cursor, *CursPtr, **CursHandle;
	
typedef	struct	
	{
	Point	pnLoc;
	Point	pnSize;
	int		pnMode;
	Pattern	pnPat;
	} PenState;

typedef	struct
	{
	int		polySize;
	Rect	polyBBox;
	Point	polyPoints[];
	} Polygon, *PolyPtr, **PolyHandle;

typedef	struct	
	{
	int		rgnSize;
	Rect	rgnBBox;
	} Region,* RgnPtr,** RgnHandle;


typedef	struct	
	{
	int		picSize;
	Rect	picFrame;
	} Picture, *PicPtr, **PicHandle;

typedef	struct	
	{
	QDPtr	textProc;
	QDPtr	lineProc;
	QDPtr	rectProc;
	QDPtr	rRectProc;
	QDPtr	ovalProc;
	QDPtr	arcProc;
	QDPtr	polyProc;
	QDPtr	rgnProc;
	QDPtr	bitsProc;
	QDPtr	commentProc;
	QDPtr	txMeasProc;
	QDPtr	getPicProc;
	QDPtr	putPicProc;
	} QDProcs,* QDProcsPtr;

typedef	struct	GrafPort
	{
	int			device;
	BitMap		portBits;
	Rect		portRect;
	RgnHandle	visRgn;
	RgnHandle	clipRgn;
	Pattern		bkPat;
	Pattern		fillPat;
	Point		pnLoc;
	Point		pnSize;
	int			pnMode;
	Pattern		pnPat;
	int			pnVis;
	int			txFont;
	Style		txFace;
	int			txMode;
	int			txSize;
	long		spExtra;
	long		fgColor;
	long		bkColor;
	int			colrBit;
	int			patStretch;
	QDHandle 	picSave;
	QDHandle 	rgnSave;
	QDHandle 	polySave;
	QDProcsPtr 	grafProcs;
	} GrafPort, * GrafPtr;

/*  Quickdraw global variables - defined in the MacTraps library  */
extern GrafPtr	thePort;
extern Pattern	white;
extern Pattern	black;
extern Pattern	gray;
extern Pattern	ltGray;
extern Pattern	dkGray;
extern Cursor	arrow;
extern BitMap	screenBits;
extern long		randSeed;

/*  functions returning non-integral values  */
pascal RgnHandle NewRgn();
pascal PicHandle OpenPicture();
pascal PolyHandle OpenPoly();

/*  low-memory globals  */
extern int ScrVRes : 0x102;
extern int ScrHRes : 0x104;
extern int ScreenRow : 0x106;
extern long RndSeed : 0x156;
extern Ptr ScrnBase : 0x824;
extern Rect CrsrPin : 0x834;


#endif _Quickdraw_