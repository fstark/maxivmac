
/*
 *  IntlPkg.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */


#ifndef	_IntlPkg_
#define _IntlPkg_

#ifndef	_MacTypes_
#include "MacTypes.h"
#endif

#define	currSymLead		16
#define	currNegSym		32
#define	currTrailingZ	64
#define	currLeadingZ	128

enum { mdy, dmy, ymd, myd, dym, ydm };

#define	century		128
#define	mntLdingZ	64
#define	dayLdingZ	32

#define	hrLeadingZ	128
#define	minLeadingZ	64
#define	secLeadingZ	32

#define zeroCycle	1

enum { longDay, longWeek, longMonth, longYear };

#define supDay		1
#define supWeek		2
#define supMonth	4
#define supYear		8

enum {
	verUS,
	verFrance,
	verBritain,
	verGermany,
	verItaly,
	verNetherlands,
	verBelgiumLux,
	verSweden,
	verSpain,
	verDenmark,
	verPortugal,
	verFrCanada,
	verNorway,
	verIsrael,
	verJapan,
	verAustralia,
	verArabia,
	verFinland,
	verFrSwiss,
	verGrSwiss,
	verGreece,
	verIceland,
	verMalta,
	verCyprus,
	verTurkey,
	verYugoslavia
};


typedef	struct	Intl0Rec
	{
	char	decimalPt;
	char	thousSep;
	char	listSep;
	char	currSym1;
	char	currSym2;
	char	currSym3;
	Byte	currFmt	;
	Byte	dateOrder;
	Byte	shrtDateFmt;
	char	dateSep	;
	Byte	timeCycle;
	Byte	timeFmt	;
	char	mornStr[4];
	char	eveStr[4];
	char	timeSep;
	char	time1Suff;
	char	time2Suff;
	char	time3Suff;
	char	time4Suff;
	char	time5Suff;
	char	time6Suff;
	char	time7Suff;
	char	time8Suff;
	Byte	metricSys;
	int		Intl0Vers;
	} Intl0Rec,* Intl0Ptr,** Intl0Hndl ;

typedef	struct	Intl1Rec
	{
	char	days[7][16];
	char	months[12][16];
	Byte	suppressDay;
	Byte	lngDateFmt;
	Byte	dayleading0;
	Byte	abbrLen;
	char	st0[4];
	char	st1[4];
	char	st2[4];
	char	st3[4];
	char	st4[4];
	int		intl1Vers;
	int		localRtn;
	} Intl1Rec,* Intl1Ptr,** Intl1Hndl ;


typedef enum { shortDate, longDate, abbrevDate } DateForm;


/*  functions returning non-integral values  */
pascal Handle IUGetIntl();


#endif _IntlPkg_