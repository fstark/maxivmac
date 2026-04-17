
/*
 *  SerialDvr.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef	_SerialDvr_
#define _SerialDvr_

/* baud rate constants */
#define	baud300		380
#define	baud600		189
#define	baud1200	94
#define	baud1800	62
#define	baud2400	46
#define	baud3600	30
#define	baud4800	22
#define	baud7200	14
#define	baud9600	10
#define	baud19200	4
#define	baud57600	0

/* SCC channel config word */
#define	stop10		16384
#define	stop15		((int) -32768)
#define	stop20		(-16384)
#define	noParity	0
#define	oddParity	4096
#define	evenParity	12288
#define	data5		0
#define	data6		2048
#define	data7		1024
#define	data8		3072

/* serial driver error masks */
#define	swOverrunErr	1
#define	parityErr		16
#define	hwOverrunErr	32
#define	framingErr		64

#define	xOffWasSent	0x80

#define dtrNegated	0x40

#define	ctsEvent	32
#define	breakEvent	128

typedef enum { sPortA, sPortB } SPortSel;

/* refNums for the serial ports */
#define	AinRefNum	-6
#define	AoutRefNum	-7
#define	BinRefNum	-8
#define	BoutRefNum	-9

/* errors */
#define rcvrErr 	 -89
#define breakRecd	 -90


typedef	struct		
	{
	char	fXOn;
	char	fCTS;
	char	xOn;
	char	xOff;
	char	errs;
	char	evts;
	char	fInX;
	char	fDTR;
	} SerShk;


typedef	struct 	 
	{
	char	cumErrs;
	char	xOffSent;
	char	rdPend;
	char	wrPend;
	char	ctsHold;
	char	xOffHold;
	} SerStaRec ;
	

#endif _SerialDvr_