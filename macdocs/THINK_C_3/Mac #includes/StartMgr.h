
/*
 *  StartMgr.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef _StartMgr_
#define _StartMgr_


typedef struct SlotDev{
	char	sdExtDevID;
	char	sdPartition;
	char	sdSlotNum;
	char	sdSRsrcID;
} SlotDev;

typedef struct SCSIDev{
	char	sdReserved1;
	char	sdReserved2;
	short	sdRefNum;
} SCSIDev;

typedef union DefStartRec {
	SlotDev	slotDev;
	SCSIDev	scsiDev;
} DefStartRec, *DefStartPtr;


typedef struct DefVideoRec{
	char			sdSlot;
	char			sdSResource;
} DefVideoRec, *DefVideoPtr;

typedef struct DefOSRec{
	char			sdReserved;
	char			sdOSType;
} DefOSRec, *DefOSPtr;


#endif _StartMgr_