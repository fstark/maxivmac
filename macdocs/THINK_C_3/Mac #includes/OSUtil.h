
/*
 *  OSUtil.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef	_OSUtil_
#define _OSUtil_

#ifndef	_MacTypes_
#include "MacTypes.h"
#endif

/*  result codes  */
enum {
	nmTypErr = -299,
	prInitErr = -88,
	prWrErr,
	clkWrErr,
	clkRdErr,
	seNoDB = -8,
	unimpErr = -4,
	corErr,
	vTypErr,
	qErr
};

/* queue types */
enum {
	vType = 1,
	ioQType,
	drvQType,
	evType,
	fsQType,
	siQType,
	dtQType,
	nmType
};

/* machine types */
enum { macXLMachine, macMachine };

/* trap types */
enum { OSTrap, ToolTrap };

/* serial port use */
enum { useFree, useATalk, useASync };

typedef	struct	
	{
	Byte		valid;
	Byte		aTalkA;
	Byte		aTalkB;
	Byte		config;
	int			portA;
	int			portB;
	long		alarm;
	int			font;
	int			kbdPrint;
	int			volClik;
	int			misc;
	} SysParmType,* SysPPtr ;

typedef	struct QElem
	{
	struct QElem *qLink;
	int			qType;
	char		qData[];
	} QElem, *QElemPtr;

typedef	struct QHdr
    {		
    int			qFlags;
    QElemPtr	qHead;
    QElemPtr	qTail;
    } QHdr,* QHdrPtr ;
	
typedef	struct	
	{
	int			year;
	int			month;
	int			day;
	int			hour;
	int			minute;
	int			second;
	int			dayOfWeek;
	} DateTimeRec ;


enum { false32b, true32b };


/*  access A5 from interrupt level  */
#define SetUpA5()		asm {	move.l	a5,-(sp)	\
								move.l	0x904,a5	}
#define RestoreA5()		asm {	move.l	(sp)+,a5	}


/* ---------- SysEnvirons ---------- */


typedef struct SysEnvRec {
	short			environsVersion;
	short			machineType;
	short			systemVersion;
	short			processor;
	Boolean			hasFPU;
	Boolean			hasColorQD;
	short			keyBoardType;
	short			atDrvrVersNum;
	short			sysVRefNum;
} SysEnvRec;

/*  machine types  */
enum {
	envXL = -2,
	envMac,
	envMachUnknown,
	env512KE,
	envMacPlus,
	envSE,
	envMacII
};

/*  CPU types  */
enum {
	envCPUUnknown,
	env68000,
	env68010,
	env68020
};

/*  keyboard types  */
enum {
	envUnknownKbd,
	envMacKbd,
	envMacAndPad,
	envMacPlusKbd,
	envAExtendKbd,
	envStandADBKbd
};

/*  error codes  */
enum {
	envSelTooBig = -5502,
	envBadSel,
	envNotPresent
};


/* ---------- Deferred Task Manager ---------- */


typedef struct DeferredTask {
	QElemPtr			qLink;
	short				qType;
	short				dtFlags;
	ProcPtr				dtAddr;
	long				dtParm;
	long				dtReserved;
} DeferredTask;


/* ---------- ShutDown Manager ---------- */


#define	sdOnPowerOff		1
#define	sdOnRestart			2
#define	sdOnUnmount			4
#define	sdOnDrivers			8
#define	sdRestartOrPower	(sdOnRestart+sdOnPowerOff)


/* ---------- Notification Manager ---------- */


typedef struct NMRec {
	QElemPtr			qLink;
	short				qType;
	short				nmFlags;
	long				nmPrivate;
	short				nmReserved;
	short				nmMark;
	Handle				nmSIcon;
	Handle				nmSound;
	StringPtr			nmStr;
	ProcPtr				nmResp;
	long				nmRefCon;
} NMRec;


/* ---------- */


/*  functions returning non-integral values  */
pascal SysPPtr GetSysPPtr();

/*  low-memory globals  */
extern int SysVersion : 0x15A;
extern SysParmType SysParam : 0x1F8;
extern long Time : 0x20C;


#endif _OSUtil_