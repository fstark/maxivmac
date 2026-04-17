
/*
 *  MacTypes.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef	_MacTypes_
#define _MacTypes_
	
#define	noErr	0

typedef	int	OsErr, OSErr;
typedef long OsType, OSType;
typedef char SignedByte ;
typedef unsigned char Byte ;
typedef char * Ptr ;
typedef char ** Handle ;
typedef int (*ProcPtr)() ;
typedef long Fixed, Fract;
typedef	long Size;
typedef enum { false, true, FALSE = 0, TRUE } Boolean;
typedef unsigned char Str255[256];
typedef unsigned char * StringPtr,** StringHandle ;
typedef	long ResType ;


typedef struct 
	{
	int	v,h;
	} Point ;
	
typedef struct
	{
	int	top,left,bottom,right ;
	} Rect ;
#define topLeft(r)	(((Point *) &(r))[0])
#define botRight(r)	(((Point *) &(r))[1])


/*  functions returning non-integral values  */
pascal Handle NewHandle();
pascal Ptr NewPtr();
pascal Handle GetResource();
pascal Handle GetIndResource();
pascal Handle GetNamedResource();

/*  low-memory globals  */
extern int ROM85 : 0x28E;


#endif _MacTypes_