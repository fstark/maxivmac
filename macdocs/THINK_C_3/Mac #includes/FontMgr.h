
/*
 *  FontMgr.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef	_FontMgr_
#define _FontMgr_
	
#ifndef	_MacTypes_
#include "MacTypes.h"
#endif

/*  font numbers  */
enum {
	systemFont,
	applFont,
	newYork,
	geneva,
	monaco,
	venice,
	london,
	athens,
	sanFran,
	toronto,
	cairo = 11,
	losAngeles,
	times = 20,
	helvetica,
	courier,
	symbol,
	taliesin
};
					
#define propFont	0x9000
#define prpFntH			0x9001
#define prpFntW			0x9002
#define prpFntHW		0x9003
#define fixedFont	0xB000
#define fxdFntH			0xB001
#define fxdFntW			0xB002
#define fxdFntHW		0xB003
#define fontWid		0xACB0

#define fMgrCtl1	8


typedef struct FMInput
	{
	int			family ;
	int			size ;
	char		face ;
	char		needBits ;
	int			device ;
	Point		numer ;
	Point		denom ;
	} FMInput ;

typedef struct	FMOutput
	{
	int			errNum ;
	Handle		fontHandle ;
	Byte		bold ;
	Byte		italic ;
	Byte 		ulOffset ;
	Byte		ulShadow ;
	Byte		ulThick ;
	Byte		shadow ;
	SignedByte	extra ;
	Byte		ascent ;
	Byte		descent ;
	Byte		widMax ;
	SignedByte	leading ;
	Byte		unused ;
	Point		numer ;
	Point		denom ;
	} FMOutput ;
typedef	FMOutput *	FMOutPtr ;

typedef	struct	FontRec
	{
	int			fontType ;
	int			firstChar ;
	int			lastChar ;
	int			widMax ;
	int			kernMax ;
	int			nDescent ;
	int			fRectWidth ;
	int			fRectHeight ;
	int			owTLoc	;
	int			ascent ;
	int			descent ;
	int			leading ;
	int			rowWords ;
/*	int			bitImage[rowWords][chHeight];	*/
/*	int			locTable[];			*/
/*	int			owTable[];			*/
	} FontRec ;

typedef struct FMetricRec {
	Fixed		ascent;
	Fixed		descent;
	Fixed		leading;
	Fixed		widMax;
	Handle		wTabHandle;
} FMetricRec;

typedef struct FamRec {
	int			ffFlags;
	int			ffFamID;
	int			ffFirstChar;
	int			ffLastChar;
	int			ffAscent;
	int			ffDescent;
	int			ffLeading;
	int			ffWidMax;
	long		ffWTabOff;
	long		ffKernOff;
	long		ffStylOff;
	int			ffProperty[9];
	int			ffIntl[2];
	int			ffVersion;
/*	FontAssoc	ffAssoc;		*/
/*	WidTable	ffWidthTab;		*/
/*	StyleTable	ffStyTab;		*/
/*	KernTable	ffKernTab;		*/
} FamRec;

typedef struct WidthTable {
	Fixed		tabData[256];
	Handle		tabFont;
	long		sExtra;
	long		style;
	int			fID;
	int			fSize;
	int			face;
	int			device;
	Point		inNumer;
	Point		inDenom;
	int			aFID;
	Handle		fHand;
	Boolean		usedFam;
	Byte		aFace;
	int			vOutput;
	int			hOutput;
	int			vFactor;
	int			hFactor;
	int			aSize;
	int			tabSize;
} WidthTable;


/*  functions returning non-integral values  */
pascal FMOutPtr SwapFont();
pascal FMOutPtr FMSwapFont();

/*  low-memory globals  */
extern int ApFontID : 0x984;
extern char FScaleDisable : 0xA63;
extern char FractEnable : 0xBF4;
extern WidthTable **WidthTabHandle : 0xB2A;
extern FamRec **LastFOND : 0xBC2;


#endif _FontMgr_