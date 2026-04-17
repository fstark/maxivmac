
/*
 *  Color.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef _Color_
#define _Color_

#ifndef _Quickdraw_
#include "Quickdraw.h"
#endif


#define invalColReq		-1			/* invalid color table request */

/* VALUES FOR GDType */

#define clutType		0
#define fixedType		1
#define directType		2

/*  BIT ASSIGNMENTS FOR GDFlags */

#define gdDevType		0
#define ramInit			10
#define mainScrn		11
#define allInit			12
#define screenDevice	13
#define noDriver		14
#define scrnActive		15

#define hiliteBit		7

#define defQDColors     127


typedef struct RGBColor{
	unsigned short	red;
	unsigned short	green;
	unsigned short	blue;
} RGBColor;

typedef struct ColorSpec{
	short			value;
	RGBColor		rgb;
} ColorSpec;

typedef ColorSpec CSpecArray[1];		/* array [0..0] of ColorSpec */

typedef struct ColorTable{
	long			ctSeed;
	short			transIndex;
	short			ctSize;
	CSpecArray		ctTable;
} ColorTable, *CTabPtr, **CTabHandle;

typedef struct MatchRec{
	unsigned short	red;
	unsigned short	green;
	unsigned short	blue;
	long			matchData;
} MatchRec;

typedef struct PixMap{
	Ptr				baseAddr;
	short			rowBytes;
	Rect			bounds;
	short			pmVersion;
	short			packType;
	long			packSize;
	Fixed			hRes;
	Fixed			vRes;
	short			pixelType;
	short			pixelSize;
	short			cmpCount;
	short			cmpSize;
	long			planeBytes;
	CTabHandle		pmTable;
	long			pmReserved;
} PixMap, *PixMapPtr, **PixMapHandle;

typedef struct PixPat{
	short			patType;
	PixMapHandle	patMap;
	Handle			patData;
	Handle			patXData;
	short			patXValid;
	Handle			patXMap;
	Pattern			pat1Data;
} PixPat, *PixPatPtr, **PixPatHandle;

typedef struct CCrsr{
	short			crsrType;
	PixMapHandle	crsrMap;
	Handle			crsrData;
	Handle			crsrXData;
	short			crsrXValid;
	Handle			crsrXHandle;
	Bits16			crsr1Data;
	Bits16			crsrMask;
	Point			crsrHotSpot;
	long			crsrXTable;
	long			crsrID;
} CCrsr, *CCrsrPtr, **CCrsrHandle;

typedef struct CIcon{
	PixMap			iconPMap;
	BitMap			iconMask;
	BitMap			iconBMap;
	Handle			iconData;
	short			iconMaskData[1];
} CIcon, *CIconPtr, **CIconHandle;

typedef struct GammaTbl{
	short			gVersion;
	short			gType;
	short			gFormulaSize;
	short			gChanCnt;
	short			gDataCnt;
	short			gDataWidth;
	short			gFormulaData[1];
} GammaTbl, *GammaTblPtr, **GammaTblHandle;

typedef struct ITab{
	long			iTabSeed;
	short			iTabRes;
	char			iTTable[1];
} ITab, *ITabPtr, **ITabHandle;

typedef struct SProcRec{
	struct SProcRec	**nxtSrch;
	ProcPtr			srchProc;
} SProcRec, *SProcPtr, **SProcHndl;

typedef struct CProcRec{
	struct CProcRec	**nxtComp;
	ProcPtr			compProc;
} CProcRec, *CProcPtr, **CProcHndl;

typedef struct GDevice{
	short			gdRefNum;
	short			gdID;
	short			gdType;
	ITabHandle		gdITable;
	short			gdResPref;
	SProcHndl		gdSearchProc;
	CProcHndl		gdCompProc;
	short			gdFlags;
	PixMapHandle	gdPMap;
	long			gdRefCon;
	struct GDevice	**gdNextGD;
	Rect			gdRect;
	long			gdMode;
	short			gdCCBytes;
	short			gdCCDepth;
	Handle			gdCCXData;
	Handle			gdCCXMask;
	long			gdReserved;
} GDevice, *GDPtr, **GDHandle;

typedef struct CGrafPort{
	short			device;
	PixMapHandle	portPixMap;
	short			portVersion;
	Handle			grafVars;
	short			chExtra;
	short			pnLocHFrac;
	Rect			portRect;
	RgnHandle		visRgn;
	RgnHandle		clipRgn;
	PixPatHandle	bkPixPat;
	RGBColor		rgbFgColor;
	RGBColor		rgbBkColor;
	Point			pnLoc;
	Point			pnSize;
	short			pnMode;
	PixPatHandle	pnPixPat;
	PixPatHandle	fillPixPat;
	short			pnVis;
	short			txFont;
	Style			txFace;
	short			txMode;
	short			txSize;
	Fixed			spExtra;
	long			fgColor;
	long			bkColor;
	short			colrBit;
	short			patStretch;
	QDHandle		picSave;
	QDHandle		rgnSave;
	QDHandle		polySave;
	QDProcsPtr		grafProcs;
} CGrafPort, *CGrafPtr;

typedef struct GrafVars{
	RGBColor		rgbOpColor;
	RGBColor		rgbHiliteColor;
	Handle			pmFgColor;
	short			pmFgIndex;
	Handle			pmBkColor;
	short			pmBkIndex;
	short			pmFlags;
} GrafVars;


typedef struct CQDProcs {
	Ptr				textProc;
	Ptr				lineProc;
	Ptr				rectProc;
	Ptr				rRectProc;
	Ptr				ovalProc;
	Ptr				arcProc;
	Ptr				polyProc;
	Ptr				rgnProc;
	Ptr				bitsProc;
	Ptr				commentProc;
	Ptr				txMeasProc;
	Ptr				getPicProc;
	Ptr				putPicProc;
	Ptr				opcodeProc;			/* fields added to QDProcs */
	Ptr				newProc1;
	Ptr				newProc2;
	Ptr				newProc3;
	Ptr				newProc4;
	Ptr				newProc5;
	Ptr				newProc6;
} CQDProcs,*CQDProcsPtr;

typedef short QDErr;

typedef struct ReqListRec{
	short			reqLSize;
	short			reqLData[1];
} ReqListRec;


/*  Palette Manager  */

	/* Usage constants */
#define		pmCourteous		0
#define		pmTolerant		2
#define		pmAnimated		4
#define		pmExplicit		8

	/* CUpdates constants */
#define		pmAllCUpdates	0xC000
#define		pmBackCUpdates	0x8000
#define		pmFrontCUpdates	0x4000
			
typedef struct ColorInfo{
		RGBColor	ciRGB;
		short		ciUsage;
		short		ciTolerance;
		short		ciFlags;
		long		ciPrivate;
} ColorInfo;
			
typedef struct Palette{
		short			pmEntries;
		GrafPtr			pmWindow;
		short			pmPrivate;
		long			pmDevices;
		long			pmSeeds;
		ColorInfo		pmInfo[1];
} Palette, *PalettePtr, **PaletteHandle;


/*  Color Picker Package  */

#define MaxSmallFract	 0x0000FFFFL

typedef short SmallFract;		/* Unsigned fraction between 0 and 1 */


typedef struct HSVColor {
	SmallFract		hue;
	SmallFract		saturation;
	SmallFract		value;
} HSVColor;

typedef struct HSLColor {
	SmallFract		hue;
	SmallFract		saturation;
	SmallFract		lightness;
} HSLColor;

typedef struct CMYColor {
	SmallFract		cyan;
	SmallFract		magenta;
	SmallFract		yellow;
} CMYColor;


/*  error codes  */

enum {
	cResErr = -156,
	cDevErr,
	cProtectErr,
	cRangeErr,
	cNoMemErr,
	cTempMemErr,
	cMatchErr,
	updPixMemErr = -125,
	seNoMemErr = -21,
	seInvRequest,
	reRangeErr,
	gdBadDev,
	i2CRangeErr,
	seProtErr,
	seOutOfRange,
	noRoomErr,
	overRun,
	tblAllocErr,
	qAllocErr,
	noColMatch,
	iTabPurgErr
};


/*  functions returning non-integral values  */
pascal PixMapHandle NewPixMap();
pascal PixPatHandle NewPixPat();
pascal PixPatHandle GetPixPat();
pascal CTabHandle GetCTable();
pascal CCrsrHandle GetCCursor();
pascal CIconHandle GetCIcon();
pascal GDHandle GetMaxDevice();
pascal GDHandle GetDeviceList();
pascal GDHandle GetMainDevice();
pascal GDHandle GetNextDevice();
pascal GDHandle NewGDevice();
pascal GDHandle GetGDevice();
pascal PaletteHandle NewPalette();
pascal PaletteHandle GetNewPalette();
pascal PaletteHandle GetPalette();


/*  low-memory globals  */
extern char HiliteMode : 0x938;
extern GDHandle MainDevice : 0x8A4;
extern GDHandle DeviceList : 0x8A8;
extern GDHandle TheGDevice : 0xCC8;


#endif _Color_