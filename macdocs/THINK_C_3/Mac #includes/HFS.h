
/*
 *  HFS.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */


#ifndef	_HFS_
#define _HFS_

#ifndef	_FileMgr_
#include "FileMgr.h"
#endif


typedef struct {
	int				fdIconID;
	int				fdUnused[4];
	int				fdComment;
	long			fdPutAway;
} FXInfo;

typedef struct {
	Rect			frRect;
	int				frFlags;
	Point			frLocation;
	int				frView;
} DInfo;

typedef struct {
	Point			frScroll;
	long			frOpenChain;
	int				frUnused;
	int				frComment;
	long			fdPutAway;
} DXInfo;


typedef IOParam HIOParam;

typedef struct {
	STANDARD_PBHEADER
	int				ioFRefNum;
	SignedByte		ioFVersNum;
	SignedByte		filler1;
	int				ioFDirIndex;
	SignedByte		ioFlAttrib;
	SignedByte		ioFlVersNum;
	FInfo			ioFlFndrInfo;
	long			ioDirID;
	int				ioFlStBlk;
	long 			ioFlLgLen;
	long			ioFlPyLen;
	int				ioFlRStBlk;
	long			ioFlRLgLen;
	long			ioFlRPyLen;
	long			ioFlCrDat;
	long			ioFlMdDat;
} HFileParam ;

typedef struct {
	STANDARD_PBHEADER
	long			filler2;
	int				ioVolIndex;
	long			ioVCrDate;
	long			ioVLsMod;
	int				ioVAtrb;
	int				ioVNmFls;
	int				ioVBitMap;
	int				ioAllocPtr;
	int				ioVNmAlBlks;
	long			ioVAlBlkSiz;
	long			ioVClpSiz;
	int				ioAlBlSt;
	long			ioVNxtCNID;
	int				ioVFrBlk;
	int				ioVSigWord;
	int				ioVDrvInfo;
	int				ioVDRefNum;
	int				ioVFSID;
	long			ioVBkUp;
	int				ioVSeqNum;
	long			ioVWrCnt;
	long			ioVFilCnt;
	long			ioVDirCnt;
	long			ioVFndrInfo[8];
} HVolumeParam;

typedef union {
	HIOParam		ioParam;
	HFileParam		fileParam;
	HVolumeParam	volumeParam;
} HParamBlockRec, *HParmBlkPtr;


typedef struct {
	STANDARD_PBHEADER
	int				ioFRefNum;
	SignedByte		ioFVersNum;
	SignedByte		filler1;
	int				ioFDirIndex;
	SignedByte		ioFlAttrib;
	SignedByte		filler2;
	FInfo			ioFlFndrInfo;
	long			ioDirID;
	int				ioFlStBlk;
	long			ioFlLgLen;
	long			ioFlPyLen;
	int				ioFlRStBlk;
	long			ioFlRLgLen;
	long			ioFlRPyLen;
	long			ioFlCrDat;
	long			ioFlMdDat;
	long			ioFlBkDat;
	FXInfo			ioFlXFndrInfo;
	long			ioFlParID;
	long			ioFlClpSiz;
} HFileInfo;

typedef struct {
	STANDARD_PBHEADER
	int				ioFRefNum;
	SignedByte		ioFVersNum;
	SignedByte		filler1;
	int				ioFDirIndex;
	SignedByte		ioFlAttrib;
	SignedByte		filler2;
	DInfo			ioDrUsrWds;
	long			ioDrDirID;
	int				ioDrNmFls;
	int				filler3[9];
	long			ioDrCrDat;
	long			ioDrMdDat;
	long			ioDrBkDat;
	DXInfo			ioDrFndrInfo;
	long			ioDrParID;
} DirInfo;

typedef union {
	HFileInfo	hFileInfo;
	DirInfo		dirInfo;
} CInfoPBRec, *CInfoPBPtr;


typedef struct {
	STANDARD_PBHEADER
	long			filler1;
	StringPtr		ioNewName;
	long			filler2;
	long			ioNewDirID;
	long			filler3[2];
	long			ioDirID;
} CMovePBRec, *CMovePBPtr;


typedef struct {
	STANDARD_PBHEADER
	int				filler1;
	int				ioWDIndex;
	long			ioWDProcID;
	int				ioWDVRefNum;
	int				filler2[7];
	long			ioWDDirID;
} WDPBRec, *WDPBPtr;


typedef struct {
	STANDARD_PBHEADER
	int				ioRefNum;
	int				filler;
	int				ioFCBIndx;
	int				ioFCBFiller1;
	long			ioFCBFlNm;
	int				ioFCBFlags;
	int				ioFCBStBlk;
	long			ioFCBEOF;
	long			ioFCBPLen;
	long			ioFCBCrPs;
	int				ioFCBVRefNum;
	long			ioFCBClpSiz;
	long			ioFCBParID;
} FCBPBRec, *FCBPBPtr;


typedef struct {
	struct QElem *	qLink;
	int				qtype;
	int				vcbFlags;
	int				vcbSigWord;
	long			vcbCrDate;
	long			vcbLsMod;
	int				vcbAtrb;
	int				vcbNmFls;
	int				vcbVBMSt;
	int				vcbAllocPtr;
	int				vcbNmAlBlks;
	long			vcbAlBlkSiz;
	long			vcbClpSIz;
	int				vcbAlBlSt;
	long			vcbNxtCNID;
	int				vcbFreeBks;
	char			vcbVN[28];
	int				vcbDrvNum;
	int				vcbDRefNum;
	int				vcbFSID;
	int				vcbVRefNum;
	Ptr				vcbMAdr;
	Ptr				vcbBufAdr;
	int				vcbMLen;
	int				vcbDirIndex;
	int				vcbDirBlk;
	long			vcbVolBkUp;
	int				vcbVSeqNum;
	long			vcbWrCnt;
	long			vcbXTClpSiz;
	long			vcbCTClpSiz;
	int				vcbNmRtDirs;
	long			vcbFilCnt;
	long			vcbDirCnt;
	long			vcbFndrInfo[8];
	int				vcbVCSize;
	int				vcbVBMCSiz;
	int				vcbCtlCSiz;
	int				vcbXTAlBlks;
	int				vcbCTAlBlks;
	int				vcbXTRef;
	int				vcbCTRef;
	Ptr				vcbCtlBuf;
	long			vcbDirIDM;
	int				vcbOffsM;
} HVCB;


typedef	struct {
/*	long			flags;		*/
	struct QElem *	qLink; 
	int				qType;
	int				dQDrive;
	int				dQRefNum;
	int				dQFSID;
	int				dQDrvSz;
	int				dQDrvSz2;
} HDrvQEl, *HDrvQElPtr;


typedef struct {
	char			sigWord[2];
	long			abSize;
	long			clpSize;
	long			nxFreeFN;
	long			btClpSize;
	int				rsrv1;
	int				rsrv2;
	int				rsrv3;
} HFSDefaults;


/*  low-memory globals  */
extern int FSFCBLen : 0x3F6;
extern HFSDefaults *FmtDefaults : 0x39E;


#endif _HFS_