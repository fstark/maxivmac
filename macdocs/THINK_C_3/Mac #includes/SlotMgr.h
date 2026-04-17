
/*
 *  SlotMgr.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef _SlotMgr_
#define _SlotMgr_

#ifndef _MacTypes_
#include "MacTypes.h"
#endif

#ifndef _OSUtil_
#include "OSUtil.h"
#endif


				/* StatusFlags constants */
#define fCardIsChanged	1
#define fCkForSame		0
#define fCkForNext		1
#define fWarmStart		2

				/* State constants */
enum {
	stateNil,
	stateSDMInit,
	statePRAMInit,
	statePInit,
	stateSInit
};


				/* DCE extension for slots */
				
typedef struct AuxDCE {
	Ptr				dCtlDriver;				/*  original DCE  */
	short			dCtlFlags;
	QHdr			dCtlQHdr;
	long			dCtlPosition;
	Handle			dCtlStorage;
	short			dCtlRefNum;
	long			dCtlCurTicks;
	struct GrafPort	*dCtlWindow;
	short			dCtlDelay;
	short			dCtlEMask;
	short			dCtlMenu;
	unsigned char	dCtlSlot;				/*  new fields  */
	unsigned char	dCtlSlotId;
	long			dCtlDevBase;
	Ptr				dCtlOwner;
	unsigned char	dCtlExtDev;
	unsigned char	fillByte;
} AuxDCE,*AuxDCEPtr,**AuxDCEHandle;


				/*  PBs for opening slot devices  */

typedef struct SlotDevParam {
	struct QElem	*qLink;
	short			qType;
	short			ioTrap;
	Ptr				ioCmdAddr;
	ProcPtr			ioCompletion;
	OsErr			ioResult;
	StringPtr		ioNamePtr;
	short			ioVRefNum;
	short			ioRefNum;
	char			ioVersNum;
	char			ioPermssn;
	Ptr				ioMix;
	short			ioFlags;
	char			ioSlot;
	char			ioID;
} SlotDevParam;


typedef struct MultiDevParam {
	struct QElem	*qLink;
	short			qType;
	short			ioTrap;
	Ptr				ioCmdAddr;
	ProcPtr			ioCompletion;
	OsErr			ioResult;
	StringPtr		ioNamePtr;
	short			ioVRefNum;
	short			ioRefNum;
	char			ioVersNum;
	char			ioPermssn;
	Ptr				ioMix;
	short			ioFlags;
	Ptr				ioSEBlkPtr;
} MultiDevParam;


				/* Device Manager Slot Support */

typedef struct SlotIntQElement{
	Ptr				sqLink;
	short			sqType;
	short			sqPrio;
	ProcPtr			sqAddr;
	long			sqParm;
} SlotIntQElement, *SQElemPtr;
				  

				/* Slot Declaration Manager */

typedef struct SpBlock{
	long			spResult;
	Ptr				spsPointer;
	long			spSize;
	long			spOffsetData;
	Ptr				spIOFileName;
	Ptr				spsExecPBlk;
	Ptr				spStackPtr;
	long			spMisc;
	long			spReserved;
	short			spIOReserved;
	short			spRefNum;
	short			spCategory;
	short			spCType;
	short			spDrvrSW;
	short			spDrvrHW;
	char			spTBMask;
	char	  		spSlot;
	char			spID;
	char			spExtDev;
	char			spHwDev;
	char			spByteLanes;
	char			spFlags;
	char			spKey;
} SpBlock, *SpBlockPtr;

typedef struct SInfoRecord{
	Ptr				siDirPtr;
	short			siInitStatusA;
	short			siInitStatusV;
	char			siState;
	char			siCPUByteLanes;
	char			siTopOfROM;
	char			siStatusFlags;
	short			siTOConst;
	char			siReserved[2];
} SInfoRecord, *SInfoRecPtr;


typedef struct SDMRecord{
	ProcPtr			sdBEVSave;
	ProcPtr			sdBusErrProc;
	ProcPtr			sdErrorEntry;
	long			sdReserved;
} SDMRecord;
			  

typedef struct FHeaderRec{
	long			fhDirOffset;
	long			fhLength;
	long			fhCRC;
	char			fhROMRev;
	char			fhFormat;
	long			fhTstPat;
	short			fhReserved;
	char			fhByteLanes;
} FHeaderRec, *FHeaderRecPtr;


typedef struct SEBlock{
	char			seSlot;
	char			sesRsrcId;
	short			seStatus;
	char			seFlags;
	char			seFiller0;
	char			seFiller1;
	char			seFiller2;
	long			seResult;
	long			seIOFileName;
	char			seDevice;
	char			sePartition;
	char			seOSType;
	char			seReserved;
	char			seRefNum;
	char			seNumDevices;
	char			seBootState;
} SEBlock;

	
/*  error codes  */

enum {
	slotNumErr = -360,
	smRecNotFnd = -351,
	smSRTOvrFlErr,
	smNoGoodOpens,
	smOffsetErr,
	smByteLanesErr,
	smBadsPtrErr,
	smsGetDrvrErr,
	smNoMoresRsrcs,
	smDisDrvrNamErr,
	smGetDrvrNamErr,
	smCkStatusErr,
	smBlkMoveErr,
	smNewPErr,
	smSelOOBErr,
	smSlotOOBErr,
	smNilsBlockErr,
	smsPointerNil,
	smCPUErr,
	smCodeRevErr,
	smReservedErr,
	smBadsList,
	smBadRefId,
	smBusErrTO = -320,
	smBadBoardId,
	smNoJmpTbl,
	smIntTblVErr,
	smIntStatVErr,
	smNoBoardId,
	smGetPRErr,
	smNoBoardsRsrc,
	smDisposePErr,
	smFHBlkDispErr,
	smFHBlockRdErr,
	smBLFieldBad,
	smUnExBusErr,
	smResrvErr,
	smNosInfoArray,
	smLWTstBad,
	smNoDir,
	smRevisionErr,
	smFormatErr,
	smCRCFail,
	smEmptySlot,
	smPriInitErr = -293,
	smPRAMInitErr,
	smSRTInitErr,
	smSDMInitErr
};


#endif _SlotMgr_