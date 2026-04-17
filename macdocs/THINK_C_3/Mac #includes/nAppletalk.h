
/*
 *  nAppletalk.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef _nAppletalk_
#define _nAppletalk_

#ifndef _Appletalk_
#include "AppleTalk.h"
#endif


#define xppLoadedBit		5
#define xppUnitNum			40
#define xppRefNum			-41

#define scbMemSize			0xC0

#define xppFlagClr			0x00
#define xppFlagSet			0x80

#define atpXOvalue			32
#define atpEOMvalue			16
#define atpSTSvalue			8
#define atpTIDValidvalue	2
#define atpSendChkvalue		1


/*  AFP command codes  */

enum {
	afpByteRangeLock = 1,
	afpVolClose,
	afpDirClose,
	afpForkClose,
	afpCopyFile,
	afpDirCreate,
	afpFileCreate,
	afpDelete,
	afpEnumerate,
	afpFlush,
	afpForkFlush,
	afpGetDirParms,
	afpGetFileParms,
	afpGetForkParms,
	afpGetSInfo,
	afpGetSParms,
	afpGetVolParms,
	afpLogin,
	afpContLogin,
	afpLogout,
	afpMapID,
	afpMapName,
	afpMove,
	afpOpenVol,
	afpOpenDir,
	afpOpenFork,
	afpRead,
	afpRename,
	afpSetDirParms,
	afpSetFileParms,
	afpSetForkParms,
	afpSetVolParms,
	afpWrite,
	afpGetFlDrParms,
	afpSetFlDrParms,
	afpDTOpen = 48,
	afpDTClose,
	afpGetIcon = 51,
	afpGtIcnInfo,
	afpAddAPPL,
	afpRmvAPPL,
	afpGetAPPL,
	afpAddCmt,
	afpRmvCmt,
	afpGetCmt,
	afpPAddIcon = 192
};


/*  ASP error codes  */

enum {
	aspNoAck = -1075,
	aspTooMany,
	aspSizeErr,
	aspSessClosed,
	aspServerBusy,
	aspParamErr,
	aspNoServers,
	aspNoMoreSess,
	aspBufTooSmall,
	aspBadVersNum
};


/*  AFP error codes  */

enum {
	afpIconTypeError = -5030,
	afpDirNotFound,
	afpCantRename,
	afpServerGoingDown,
	afpTooManyFilesOpen,
	afpObjectTypeErr,
	afpCallNotSupported,
	afpUserNotAuth,
	afpSessClosed,
	afpRangeOverlap,
	afpRangeNotLocked,
	afpParmErr,
	afpObjectNotFound,
	afpObjectExists,
	afpNoServer,
	afpNoMoreLocks,
	afpMiscErr,
	afpLockErr,
	afpItemNotFound,
	afpFlatVol,
	afpFileBusy,
	afpEofError,
	afpDiskFull,
	afpDirNotEmpty,
	afpDenyConflict,
	afpCantMove,
	afpBitmapErr,
	afpBadVersNum,
	afpBadUAM,
	afpAuthContinue,
	afpAccessDenied
};


/* ---------- MPP parameter block ---------- */


typedef struct MPPParamBlock {
	struct QElem	*qLink;
	short			qType;
	short			ioTrap;
	Ptr				ioCmdAddr;
	ProcPtr			ioCompletion;
	OSErr			ioResult;
	StringPtr		ioNamePtr;
	short			ioVRefNum;
	short			ioRefNum;
	short			csCode;

	union {
	
		struct {
			unsigned char	protType;
			union {
				Ptr				wdsPointer;
				Ptr				handler;
			} LAP1;
		} LAP;
#define MPPprotType			MPP.LAP.protType
#define MPPwdsPointer		MPP.LAP.LAP1.wdsPointer
#define MPPhandler			MPP.LAP.LAP1.handler

		struct {
			unsigned char	socket;
			unsigned char	checksumFlag;
			Ptr				listener;
		} DDP;
#define MPPsocket			MPP.DDP.socket
#define MPPchecksumFlag		MPP.DDP.checksumFlag
#define MPPlistener			MPP.DDP.listener

		struct {
			unsigned char	interval;
			unsigned char	count;
			Ptr				entityPtr;
			union {
				unsigned char	verifyFlag;
				struct {
					Ptr				retBuffPtr;
					short			retBuffSize;
					short			maxToGet;
					short			numGotten;
				} NBP2;
				struct {
					AddrBlock		confirmAddr;
					unsigned char	newSocket;
				} NBP3;
			} NBP1;
		} NBP;
#define MPPinterval			MPP.NBP.interval
#define MPPcount			MPP.NBP.count
#define MPPentityPtr		MPP.NBP.entityPtr
#define MPPverifyFlag		MPP.NBP.NBP1.verifyFlag
#define MPPretBuffPtr		MPP.NBP.NBP1.NBP2.retBuffPtr
#define MPPretBuffSize		MPP.NBP.NBP1.NBP2.retBuffSize
#define MPPmaxToGet			MPP.NBP.NBP1.NBP2.maxToGet
#define MPPnumGotten		MPP.NBP.NBP1.NBP2.numGotten
#define MPPconfirmAddr		MPP.NBP.NBP1.NBP3.confirmAddr
#define MPPnewSocket		MPP.NBP.NBP1.NBP3.newSocket

		struct {
			unsigned char	newSelfFlag;
			unsigned char	oldSelfFlag;
		} SSS;
#define MPPnewSelfFlag		MPP.SSS.newSelfFlag
#define MPPoldSelfFlag		MPP.SSS.oldSelfFlag
		
		Ptr				nKillQEl;
#define MPPnKillQEl			MPP.nKillQEl

	} MPP;
} MPPParamBlock, *MPPPBptr;


/*  for MPW C compatibility  */
#define MPPioCompletion		ioCompletion		
#define MPPioResult			ioResult	
#define MPPioRefNum			ioRefNum		
#define MPPcsCode			csCode
#define LAPprotType			MPPprotType
#define LAPwdsPointer		MPPwdsPointer
#define LAPhandler			MPPhandler
#define DDPsocket			MPPsocket
#define DDPchecksumFlag		MPPchecksumFlag
#define DDPwdsPointer		MPPwdsPointer
#define DDPlistener			MPPlistener
#define	NBPinterval 		MPPinterval
#define NBPcount 			MPPcount
#define	NBPntQElPtr 		MPPentityPtr
#define NBPentityPtr 		MPPentityPtr
#define NBPverifyFlag 		MPPverifyFlag
#define NBPretBuffPtr 		MPPretBuffPtr
#define NBPretBuffSize 		MPPretBuffSize
#define NBPmaxToGet 		MPPmaxToGet
#define NBPnumGotten 		MPPnumGotten
#define NBPconfirmAddr 		MPPconfirmAddr
#define NBPnewSocket 		MPPnewSocket
#define NBPnKillQEl			MPPnKillQEl


typedef struct WDSElement
{
	short			entrylength;
	Ptr				entryPtr;
} WDSElement;

typedef struct NamesTableEntry
{
	Ptr				qNext;
	AddrBlock		nteAddress;
	char			filler;
	char			entityData[99];
} NamesTableEntry;


/* ---------- ATP parameter block ---------- */


typedef struct ATPParamBlock {
	struct QElem	*qLink;
	short			qType;
	short			ioTrap;
	Ptr				ioCmdAddr;
	ProcPtr			ioCompletion;
	OSErr			ioResult;
	long			userData;
	short			reqTID;
	short			ioRefNum;
	short			csCode;

	unsigned char	atpSocket;
	unsigned char	atpFlags;
	AddrBlock		addrBlock;
	short			reqLength;
	Ptr				reqPointer;
	Ptr				bdsPointer;

	union {
	
		struct {
			unsigned char	numOfBuffs;
			unsigned char	timeOutVal;
			unsigned char	numOfResps;
			unsigned char	retryCount;
			short			intBuff;
		} ATP1;
#define ATPnumOfBuffs		ATP.ATP1.numOfBuffs
#define ATPtimeOutVal		ATP.ATP1.timeOutVal
#define ATPnumOfResps		ATP.ATP1.numOfResps
#define ATPretryCount		ATP.ATP1.retryCount
#define ATPintBuff			ATP.ATP1.intBuff

		struct {
			unsigned char	filler;
			unsigned char	bdsSize;
			short			transID;
		} ATP2;
#define ATPbdsSize			ATP.ATP2.bdsSize
#define ATPtransID			ATP.ATP2.transID
		
		unsigned char	bitMap;
#define ATPbitMap			ATP.bitMap

		unsigned char	rspNum;
#define ATPrspNum			ATP.rspNum

		Ptr				aKillQEl;
#define ATPaKillQEl			ATP.aKillQEl

	} ATP;
} ATPParamBlock, *ATPPBptr;


/*  for MPW C compatibility  */
#define ATPioCompletion		ioCompletion		
#define ATPioResult			ioResult		
#define ATPuserData			userData		
#define ATPreqTID			reqTID		
#define ATPioRefNum			ioRefNum		
#define ATPcsCode			csCode		
#define ATPatpSocket		atpSocket		
#define ATPatpFlags			atpFlags		
#define ATPaddrBlock		addrBlock		
#define ATPreqLength		reqLength		
#define ATPreqPointer		reqPointer		
#define ATPbdsPointer		bdsPointer	


/* ---------- XPP parameter block ---------- */


typedef struct XPPParamBlock {
	struct QElem	*qLink;
	short			qType;
	short			ioTrap;
	Ptr				ioCmdAddr;
	ProcPtr			ioCompletion;
	OSErr			ioResult;
	long			cmdResult;
	short			ioVRefNum;
	short			ioRefNum;
	short			csCode;

	union {
	
		Ptr				abortSCBPtr;
#define XPPabortSCBPtr		XPP.abortSCBPtr
		
		struct {
			short			aspMaxCmdSize;
			short			aspQuantumSize;
			short			numSesss;
		} XPP1;
#define XPPaspMaxCmdSize	XPP.XPP1.aspMaxCmdSize
#define XPPaspQuantumSize	XPP.XPP1.aspQuantumSize
#define XPPnumSesss			XPP.XPP1.numSesss

		struct {
			short			sessRefnum;
			unsigned char	aspTimeout;
			unsigned char	aspRetry;
			union {
				struct {
					AddrBlock		serverAddr;
					Ptr				scbPointer;
					Ptr				attnRoutine;
				} XPP2;
				struct {
					short			cbSize;
					Ptr				cbPtr;
					short			rbSize;
					Ptr				rbPtr;
					union {
						struct {
							AddrBlock		afpAddrBlock;
							Ptr				afpSCBPtr;
							Ptr				afpAttnRoutine;
						} XPP3;
						struct {
							short			wdSize;
							Ptr				wdPtr;
							unsigned char	ccbStart[296];
						} XPP4;
					} XPP5;
				} XPP6;
			} XPP7;
		} XPP8;
#define XPPsessRefnum		XPP.XPP8.sessRefnum
#define XPPaspTimeout		XPP.XPP8.aspTimeout
#define XPPaspRetry			XPP.XPP8.aspRetry
#define XPPserverAddr		XPP.XPP8.XPP7.XPP2.serverAddr
#define XPPscbPointer		XPP.XPP8.XPP7.XPP2.scbPointer
#define XPPattnRoutine		XPP.XPP8.XPP7.XPP2.attnRoutine
#define XPPcbSize			XPP.XPP8.XPP7.XPP6.cbSize
#define XPPcbPtr			XPP.XPP8.XPP7.XPP6.cbPtr
#define XPPrbSize			XPP.XPP8.XPP7.XPP6.rbSize
#define XPPrbPtr			XPP.XPP8.XPP7.XPP6.rbPtr
#define XPPafpAddrBlock		XPP.XPP8.XPP7.XPP6.XPP5.XPP3.afpAddrBlock
#define XPPafpSCBPtr		XPP.XPP8.XPP7.XPP6.XPP5.XPP3.afpSCBPtr
#define XPPafpAttnRoutine	XPP.XPP8.XPP7.XPP6.XPP5.XPP3.afpAttnRoutine
#define XPPwdSize			XPP.XPP8.XPP7.XPP6.XPP5.XPP4.wdSize
#define XPPwdPtr			XPP.XPP8.XPP7.XPP6.XPP5.XPP4.wdPtr
#define XPPccbStart			XPP.XPP8.XPP7.XPP6.XPP5.XPP4.ccbStart

	} XPP;
} XPPParamBlock, *XPPParmBlkPtr;


/* ---------- AFP command block ---------- */


typedef struct AFPCommandBlock {
	unsigned char	cmdByte;
	unsigned char	startEndFlag;
	short			forkRefNum;
	long			rwOffset;
	long			reqCount;
	unsigned char	newLineFlag;
	char			newLineChar;
} AFPCommandBlock;


#endif _nAppletalk_