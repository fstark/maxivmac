
/*
 *  BDC.h - binary/decimal conversion
 *
 *  Copyright (c) 1991 Symantec Corporation.  All rights reserved.
 *
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985-1991.
 *
 *  This material has been extracted from <Packages.h>, so that it
 *  can be used without dragging in the International Utilities and
 *  Script Manager interfaces.
 *
 */

#ifndef __BDC__
#define __BDC__

#ifndef __TYPES__
#include <Types.h>
#endif

pascal void StringToNum(ConstStr255Param theString,long *theNum); 
void stringtonum(char *theString,long *theNum); 
pascal void NumToString(long theNum,Str255 theString); 
void numtostring(long theNum,char *theString); 

#endif // __BDC__
