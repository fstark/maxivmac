
/*
 *  SCSIMgr.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef	_SCSIMgr_
#define _SCSIMgr_


/*  transfer instruction op codes  */
enum {
	scInc = 1,
	scNoInc,
	scAdd,
	scMove,
	scLoop,
	scNop,
	scStop,
	scComp
};

/*  result codes  */
enum {
	scCommErr = 2,
	scArbNBErr,
	scBadParmsErr,
	scPhaseErr,
	scCompareErr,
	scMgrBusyErr,
	scSequenceErr,
	scBusTOErr,
	scComplPhaseErr
};

#define sbSIGWord		0x4552
#define pMapSIG			0x504D


typedef struct SCSIInstr {
	int			scOpcode;
	long		scParam1;
	long		scParam2;
} SCSIInstr;


typedef struct Block0 {
	unsigned short	sbSig;
	unsigned short	sbBlkSize;
	unsigned long	sbBlkCount;
	unsigned short	sbDevType;
	unsigned short	sbDevId;
	unsigned long	sbData;
	unsigned short	sbDrvrCount;
	unsigned long	ddBlock;
	unsigned short	ddSize;
	unsigned short	ddType;
	unsigned short	ddPad[243];		/* to make size be 512 */
}Block0;

typedef struct Partition {
	unsigned short	pmSig;
	unsigned short	pmSigPad;
	unsigned long	pmMapBlkCnt;
	unsigned long	pmPyPartStart;
	unsigned long	pmPartBlkCnt;
	unsigned char	pmPartName[32];
	unsigned char	pmParType[32];
	unsigned long	pmLgDataStart;
	unsigned long	pmDataCnt;
	unsigned long	pmPartStatus;
	unsigned long	pmLgBootStart;
	unsigned long	pmBootSize;
	unsigned long	pmBootAddr;
	unsigned long	pmBootAddr2;
	unsigned long	pmBootEntry;
	unsigned long	pmBootEntry2;
	unsigned long	pmBootCksum;
	unsigned char	pmProcessor[16];
	unsigned short	pmPad[188];			/* to make size be 512 */
}Partition;


#endif _SCSIMgr_