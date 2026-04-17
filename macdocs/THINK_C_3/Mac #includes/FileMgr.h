
/*
 *  FileMgr.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef	_FileMgr_
#define _FileMgr_
	
#ifndef	_MacTypes_
#include "MacTypes.h"
#endif


/*  finder constants  */
#define	fHasBundle	8192
#define	fInvisible	16384
#define	fTrash		(-3)
#define	fDeskTop	(-2)
#define	fDisk		0
#define fOnDesk		1


/*  ioPosMode values  */
enum {
	fsAtMark,
	fsFromStart,
	fsFromLEOF,
	fsFromMark
};
#define rdVerify	64


/*  ioPermssn values  */
enum {
	fsCurPerm,
	fsRdPerm,
	fsWrPerm,
	fsRdWrPerm,
	fsRdWrShPerm
};


/* result codes */
enum {
	fsDSIntErr = -127,
	volGoneErr = -124,
	wrgVolTypErr,
	badMovErr,
	tmwdoErr,
	dirNFErr,
	wrPermErr = -61,
	badMDBErr,
	fsRnErr,
	extFSErr,
	noMacDskErr,
	nsDrvErr,
	volOnLinErr,
	permErr,
	volOffLinErr,
	gfpErr,
	rfNumErr,
	paramErr,
	opWrErr,
	dupFNErr,
	fBsyErr,
	vLckdErr,
	fLckdErr,
	wPrErr,
	fnfErr,
	tmfoErr,
	mFulErr,
	posErr,
	eofErr,
	fnOpnErr,
	bdNamErr,
	ioErr,
	nsvErr,
	dskFulErr,
	dirFulErr
};


/*  standard header used in all PB records  */
#define STANDARD_PBHEADER			\
	struct QElem *	qLink;			\
	int				qType;			\
	int				ioTrap;			\
	Ptr				ioCmdAddr;		\
	ProcPtr			ioCompletion;	\
	OsErr			ioResult;		\
	StringPtr		ioNamePtr;		\
	int				ioVRefNum;


typedef	struct
	{
	OsType		fdType;
	OsType		fdCreator;
	int			fdFlags;
	Point		fdLocation;
	int			fdFldr;
	} FInfo ;


typedef struct
	{
	STANDARD_PBHEADER
	int			ioRefNum;
	SignedByte	ioVersNum;
	SignedByte	ioPermssn;
	Ptr			ioMisc;
	Ptr			ioBuffer;
	long		ioReqCount;
	long		ioActCount;
	int			ioPosMode;
	long		ioPosOffset;
	} ioParam, IOParam ;

/*  file parameter block  */
typedef struct
	{
	STANDARD_PBHEADER
	int			ioFRefNum;
	SignedByte	ioFVersNum;
	SignedByte	filler1;
	int			ioFDirIndex;
	SignedByte	ioFlAttrib;
	SignedByte	ioFlVersNum;
	FInfo		ioFlFndrInfo;
	long		ioFlNum;
	int			ioFlStBlk;
	long 		ioFlLgLen;
	long		ioFlPyLen;
	int			ioFlRStBlk;
	long		ioFlRLgLen;
	long		ioFlRPyLen;
	long		ioFlCrDat;
	long		ioFlMdDat;
	} fileParam, FileParam ;

/*  volume parameter block  */
typedef struct
	{
	STANDARD_PBHEADER
	long		filler2;
	int			ioVolIndex;
	long		ioVCrDate;
	long		ioVLsBkUp;
	int			ioVAtrb;
	int			ioVNmFls;
	int			ioVDirSt;
	int			ioVBlLn;
	int			ioVNmAlBlks;
	long		ioVAlBlkSiz;
	long		ioVClpSiz;
	int			ioAlBlSt;
	long		ioVNxtFNum;
	int			ioVFrBlk;
	} volumeParam, VolumeParam ;


typedef union {
	ioParam		ioParam;
	fileParam	fileParam;
	volumeParam	volumeParam;
} ParamBlockRec, *ParmBlkPtr;


/*  volume control block  */
typedef struct
	{
	struct QElem *qLink;
	int			qType;
	int			vcbFlags;
	int			vcbSigWord;
	long		vcbCrDate;
	long		vcbLsBkUp;
	int			vcbAtrb;
	int			vcbNmFls;
	int			vcbDirSt;
	int			vcbBlLn;
	int			vcbNmBlks;
	long		vcbAlBlkSiz;
	long		vcbClpSiz;
	int			vcbAlBlSt;
	long		vcbNxtFNum;
	int			vcbFreeBks;
	char		vcbVN[28];
	int			vcbDrvNum;
	int			vcbDRefNum;
	int			vcbFSID;
	int			vcbVRefNum;
	Ptr			vcbMAdr;
	Ptr			vcbBufAdr;
	int			vcbMLen;
	int			vcbDirIndex;
	int			vcbDirBlk;
	} VCB ;


/*  drive queue element  */

typedef	struct	
	{
/*	long		flags;		*/
	struct QElem *qLink; 
	int			qType;
	int			dQDrive;
	int			dQRefNum;
	int			dQFSID;
	int			dQDrvSize;
	} DrvQEl,*DrvQElPtr ;


/*  functions returning non-integral values  */
pascal struct QHdr *GetFSQHdr();
pascal struct QHdr *GetDrvQHdr();
pascal struct QHdr *GetVCBQHdr();

/*  low-memory globals  */
extern int BootDrive : 0x210;
extern struct QHdr DrvQHdr : 0x308;
extern ProcPtr EjectNotify : 0x338;
extern Ptr FCBSPtr : 0x34E;
extern VCB *DefVCBPtr : 0x352;
extern struct QHdr VCBQHdr : 0x356;
extern struct QHdr FSQHdr : 0x360;


#endif _FileMgr_