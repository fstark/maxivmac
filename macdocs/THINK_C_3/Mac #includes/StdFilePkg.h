
/*
 *  StdFilePkg.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef	_StdFilePkg_
#define _StdFilePkg_

#define	putDlgID	-3999

enum {
	putSave = 1,
	putCancel,
	putEject = 5,
	putDrive,
	putName
};

#define	getDlgID	-4000

enum {
	getOpen = 1,
	getCancel = 3,
	getEject = 5,
	getDrive,
	getNmList,
	getScroll
};


typedef	struct	SFReply	
	{
	char			good;
	char			copy;
	long			fType;		/* array[1..4] of char; */
	int				vRefNum;
	int				version;
	unsigned char	fName[64];
	}SFReply;

typedef	long	SFTypeList[4];	/* array[0..3] of OSType; */


/*  low-memory globals  */
extern int SFSaveDisk : 0x214;
extern long CurDirStore : 0x398;


#endif _StdFilePkg_