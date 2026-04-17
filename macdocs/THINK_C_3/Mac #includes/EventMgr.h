
/*
 *  EventMgr.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef	_EventMgr_
#define _EventMgr_
	
#ifndef	_MacTypes_
#include "MacTypes.h"
#endif

/*  event codes  */
enum {
	nullEvent,
	mouseDown,
	mouseUp,
	keyDown,
	keyUp,
	autoKey,
	updateEvt,
	diskEvt,
	activateEvt,
	/* event 9 is no longer used */
	networkEvt = 10,
	driverEvt,
	app1Evt,
	app2Evt,
	app3Evt,
	app4Evt
};

/*  masks for keyboard event message  */
#define charCodeMask 	0x000000FFL
#define keyCodeMask		0x0000FF00L
#define adbAddrMask		0x00FF0000L

/*  event masks  */
#define mDownMask		0x2 
#define mUpMask			0x4 
#define keyDownMask 	0x8
#define keyUpMask 		0x10 
#define autoKeyMask 	0x20 
#define updateMask 		0x40 
#define diskMask 		0x80 
#define activMask 		0x100 
/* event 9 is no longer used */
#define networkMask 	0x400 
#define driverMask 		0x800 
#define app1Mask 		0x1000 
#define app2Mask 		0x2000 
#define app3Mask 		0x4000 
#define app4Mask 		0x8000 
#define everyEvent 		0xFFFF

/*  modifiers  */
#define activeFlag		0x0001
#define changeFlag		0x0002
#define btnState 		0x0080
#define cmdKey 			0x0100
#define shiftKey 		0x0200
#define alphaLock 		0x0400
#define optionKey 		0x0800
#define controlKey		0x1000


/* results returned by PostEvent	*/
/*	#define	noErr		0		*/
#define EvtNotEnb		1


typedef struct EventRecord 
	{
	int		what;
	long	message;
	long	when;
	Point	where;
	int		modifiers;
	}EventRecord;

typedef struct KeyMap
	{
	long	Key[4];
	}KeyMap;
	
typedef	struct	EvQEl
	{
	struct QElem	*qLink;
	int				qType;
	int				evtQWhat;
	long			evtQMessage;
	long			evtQWhen;
	Point			evtQWhere;
	int				evtQModifiers;
	} EvQEl, *EvQElPtr ;


/*  functions returning non-integral values  */
pascal struct QHdr *GetEvQHdr();

/*  low-memory globals  */
extern int SysEvtMask : 0x144;
extern struct QHdr EventQueue : 0x14A;
extern char SEvtEnb : 0x15C;
extern long Ticks : 0x16A;
extern int KeyThresh : 0x18E;
extern int KeyRepThresh : 0x190;
extern ProcPtr JGNEFilter : 0x29A;
extern ProcPtr Key1Trans : 0x29E;
extern ProcPtr Key2Trans : 0x2A2;
extern long DoubleTime : 0x2F0;
extern long CaretTime : 0x2F4;
extern char ScrDmpEnb : 0x2F8;


#endif _EventMgr_