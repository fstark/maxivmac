
/*
 *  pascal.h
 *
 *  Copyright (c) 1991 Symantec Corporation.  All rights reserved.
 *
 */

#ifndef __pascal__
#define __pascal__

//  these functions modify their argument in place - don't use with constants
unsigned char *CtoPstr(char *);
char *PtoCstr(unsigned char *);
#define c2pstr(s) CtoPstr(s)
#define p2cstr(s) PtoCstr(s)
pascal unsigned char *C2PStr(char *);
pascal char *P2CStr(unsigned char *);

//  no longer needed - provided for backward compatibility
pascal void CallPascal (...) = { 0x205F, 0x4E90 };
pascal char CallPascalB(...) = { 0x205F, 0x4E90 };
pascal int  CallPascalW(...) = { 0x205F, 0x4E90 };
pascal long CallPascalL(...) = { 0x205F, 0x4E90 };

#endif // __pascal__
