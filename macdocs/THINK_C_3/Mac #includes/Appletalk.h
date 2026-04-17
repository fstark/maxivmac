
/*
 *  Appletalk.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef _Appletalk_
#define _Appletalk_

#ifndef _MacTypes_
#include "MacTypes.h"
#endif


/* result codes */
enum {
	sktClosedErr = -3109,
	recNotFnd,
	atpBadRsp,
	atpLenErr,
	readQErr,
	extractErr,
	cksumErr,
	noMPPError,
	buf2SmallErr,
	reqAborted = -1105,
	noDataArea,
	noSendResp,
	cbNotFound,
	noRelErr,
	badBuffNum,
	badATPSkt,
	tooManySkts,
	tooManyReqs,
	reqFailed,
	nbpNISErr = -1029,
	nbpNotFound,
	nbpDuplicate,
	nbpConfDiff,
	nbpNoConfirm,
	nbpBuffOvr,
	portNotCf = -98,
	portInUse,
	excessCollsns = -95,
	lapProtErr,
	noBridgeErr,
	ddpLenErr,
	ddpSktErr
};


typedef enum {
	tLAPRead,
	tLAPWrite,
	tDDPRead,
	tDDPWrite,
	tNBPLookup,
	tNBPConfirm,
	tNBPRegister,
	tATPSndRequest,
	tATPGetRequest,
	tATPSdRsp,
	tATPAddRsp,
	tATPRequest,
	tATPResponse
} ABCallType;

typedef Byte ABByte;

typedef struct LAPAdrBlock {
	Byte			dstNodeID;
	Byte			srcNodeID;
	ABByte			lapProtType;
} LAPAdrBlock;

typedef struct AddrBlock {
	int				aNet;
	Byte			aNode;
	Byte			aSocket;
} AddrBlock;

typedef unsigned char Str32[34];

typedef struct EntityName {
	Str32			objStr;
	Str32			typeStr;
	Str32			zoneStr;
} EntityName, *EntityPtr;

typedef struct RetransType {
	Byte			retransInterval;
	Byte			retransCount;
} RetransType;

typedef struct BDSElement {
	int				buffSize;
	Ptr				buffPtr;
	int				dataSize;
	long			userBytes;
} BDSElement, BDSType[8];
typedef BDSType *BDSPtr;

typedef char BitMapType;

typedef struct lapProto {
	ABCallType		abOpcode;
	int				abResult;
	long			abUserReference;
	LAPAdrBlock		lapAddress;
	int				lapReqCount;
	int				lapActCount;
	Ptr				lapDataPtr;
} lapProto, LAPProto;
#define lapSize		sizeof(lapProto)

typedef struct ddpProto {
	ABCallType		abOpcode;
	int				abResult;
	long			abUserReference;
	unsigned		ddpType;
	unsigned		ddpSocket;
	AddrBlock		ddpAddress;
	int				ddpReqCount;
	int				ddpActCount;
	Ptr				ddpDataPtr;
	unsigned		ddpNodeID;
} ddpProto, DDPProto;
#define ddpSize		sizeof(ddpProto)

typedef struct nbpProto {
	ABCallType		abOpcode;
	int				abResult;
	long			abUserReference;
	EntityPtr		nbpEntityPtr;
	Ptr				nbpBufPtr;
	int				nbpBufSize;
	int				nbpDataField;
	AddrBlock		nbpAddress;
	RetransType		nbpRetransmitInfo;
} nbpProto, NBPProto;
#define nbpSize		sizeof(nbpProto)

typedef struct atpProto {
	ABCallType		abOpcode;
	int				abResult;
	long			abUserReference;
	unsigned		atpSocket;
	AddrBlock		atpAddress;
	int				atpReqCount;
	Ptr				atpDataPtr;
	BDSPtr			atpRspBDSPtr;
	BitMapType		atpBitMap;
	int				atpTransID;
	int				atpActCount;
	long			atpUserData;
	Boolean			atpXO;
	Boolean			atpEOM;
	unsigned		atpTimeOut;
	unsigned		atpRetries;
	unsigned		atpNumBufs;
	unsigned		atpNumRsp;
	unsigned		atpBDSSize;
	long			atpRspUData;
	Ptr				atpRspBuf;
	int				atpRspSize;
} atpProto, ATPProto;
#define atpSize		sizeof(atpProto)

typedef union {
	lapProto		lapProto;
	ddpProto		ddpProto;
	nbpProto		nbpProto;
	atpProto		atpProto;
} ABusRecord, *ABRecPtr, **ABRecHandle;


#endif _Appletalk_