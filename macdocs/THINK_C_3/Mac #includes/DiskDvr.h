
/*
 *  DiskDvr.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef	_DiskDvr_
#define _DiskDvr_
	
/*  positioning  */
#define	currPos		0		/*  fsAtMark  */
#define	absPos		1		/*  fsFromStart  */
#define	relPos		3		/*  fsFromMark  */

typedef	struct DrvSts {
	int				track;
	char			writeProt;
	char			diskInPlace;
	char			installed;
	char			sides;
	struct QElem 	*qLink;
	int				qType;
	int				dQDrive;
	int				dQRefNum;
	int				dQFSID;
	char			twoSideFmt;
	char			needsFlush;
	int				diskErrs;
} DrvSts;

typedef	struct DrvSts2 {	/*  HD-20  */
	int				track;
	char			writeProt;
	char			diskInPlace;
	char			installed;
	char			sides;
	struct QElem 	*qLink;
	int				qType;
	int				dQDrive;
	int				dQRefNum;
	int				dQFSID;
	int				driveSize;
	int				driveS1;
	int				driveType;
	int				driveManf;
	char			driveChar;
	char			driveMisc;
} DrvSts2;

/*  result codes  */
#define firstDskErr		(-84)
enum {
	verErr = -84,
	fmt2Err,
	fmt1Err,
	sectNFErr,
	seekErr,
	spdAdjErr,
	twoSideErr,
	initIWMErr,
	tk0BadErr,
	cantStepErr,
	wrUnderrun,
	badDBtSlp,
	badDCksum,
	noDtaMkErr,
	badBtSlpErr,
	badCksmErr,
	dataVerErr,
	noAdrMkErr,
	noNybErr,
	offLinErr,
	noDriveErr
};
#define lastDskErr		(-64)


#endif _DiskDvr_