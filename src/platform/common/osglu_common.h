/*
	osglu_common.h

	Copyright (C) 2009 Paul C. Pratt

	You can redistribute this file and/or modify it under the terms
	of version 2 of the GNU General Public License as published by
	the Free Software Foundation.  You should have received a copy
	of the license along with this file; see the file COPYING.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	license for more details.
*/

/*
	COMmon code for Operating System GLUe — declarations
*/

#pragma once

/* --- feature computation macros --- */

#if EnableMouseMotion && MayFullScreen
#define EnableFSMouseMotion 1
#else
#define EnableFSMouseMotion 0
#endif

#if EnableMagnify || VarFullScreen
#define EnableRecreateW 1
#else
#define EnableRecreateW 0
#endif

#if EnableRecreateW || EnableFSMouseMotion
#define EnableMoveMouse 1
#else
#define EnableMoveMouse 0
#endif

#ifndef GrabKeysFullScreen
#define GrabKeysFullScreen 1
#endif

#ifndef GrabKeysMaxFullScreen
#define GrabKeysMaxFullScreen 0
#endif

/* --- power-of-2 helper macros --- */

#define PowOf2(p) ((uint32_t)1 << (p))
#define Pow2Mask(p) (PowOf2(p) - 1)
#define ModPow2(i, p) ((i) & Pow2Mask(p))
#define FloorDivPow2(i, p) ((i) >> (p))
#define FloorPow2Mult(i, p) ((i) & (~ Pow2Mask(p)))
#define CeilPow2Mult(i, p) FloorPow2Mult((i) + Pow2Mask(p), (p))

/* --- keep-mask constants for DisconnectKeyCodes --- */

#define kKeepMaskControl  (1 << 0)
#define kKeepMaskCapsLock (1 << 1)
#define kKeepMaskCommand  (1 << 2)
#define kKeepMaskOption   (1 << 3)
#define kKeepMaskShift    (1 << 4)

/* --- variables (extern declarations) --- */
/* NOTE: Variables already declared extern in platform/platform.h are
   intentionally NOT duplicated here. Include platform.h (via osglu_ud.h)
   for those. */

extern bool ROM_loaded;
extern bool RequestMacOff;

extern uint8_t * screencomparebuff;

extern int16_t ScreenChangedTop;
extern int16_t ScreenChangedLeft;
extern int16_t ScreenChangedBottom;
extern int16_t ScreenChangedRight;

#if EnableAutoSlow
extern int16_t ScreenChangedQuietTop;
extern int16_t ScreenChangedQuietLeft;
extern int16_t ScreenChangedQuietBottom;
extern int16_t ScreenChangedQuietRight;
#endif

#if MayFullScreen
extern uint16_t ViewHSize;
extern uint16_t ViewVSize;
extern uint16_t ViewHStart;
extern uint16_t ViewVStart;
#if EnableFSMouseMotion
extern int16_t SavedMouseH;
extern int16_t SavedMouseV;
#endif
#endif

#if EnableFSMouseMotion
extern bool HaveMouseMotion;
#endif

extern uint32_t ReserveAllocOffset;
extern uint8_t * ReserveAllocBigBlock;

#if IncludePbufs
extern uint32_t PbufAllocatedMask;
extern uint32_t PbufSize[NumPbufs];
#define PbufIsAllocated(i) ((PbufAllocatedMask & ((uint32_t)1 << (i))) != 0)
#endif

extern uint32_t theKeys[4];
extern bool MyMouseButtonState;

extern uint16_t MyMousePosCurV;
extern uint16_t MyMousePosCurH;

/* event queue */
#define MyEvtQLg2Sz 4
#define MyEvtQSz (1 << MyEvtQLg2Sz)
#define MyEvtQIMask (MyEvtQSz - 1)

extern MyEvtQEl MyEvtQA[MyEvtQSz];
extern uint16_t MyEvtQIn;
extern uint16_t MyEvtQOut;
extern bool MyEvtQNeedRecover;

extern char *SavedBriefMsg;
extern char *SavedLongMsg;
#if WantAbnormalReports
extern uint16_t SavedIDMsg;
#endif
extern bool SavedFatalMsg;

#define WantColorTransValid 1
#if WantColorTransValid
extern bool ColorTransValid;
#endif

#if EmLocalTalk
extern uint32_t e_p[2];
extern uint32_t LT_MyStamp;
#endif

/* --- function prototypes --- */
/* NOTE: Functions already declared in platform/platform.h are NOT
   duplicated here. */

void ScreenClearChanges(void);
void ScreenChangedAll(void);

#if EnableFSMouseMotion
void AutoScrollScreen(void);
#endif

bool FirstFreeDisk(tDrive *Drive_No);
void DiskInsertNotify(tDrive Drive_No, bool locked);
void DiskEjectedNotify(tDrive Drive_No);

#if IncludePbufs
bool FirstFreePbuf(tPbuf *r);
void PbufNewNotify(tPbuf Pbuf_No, uint32_t count);
void PbufDisposeNotify(tPbuf Pbuf_No);
#endif

void Keyboard_UpdateKeyMap(uint8_t key, bool down);
void MyMouseButtonSet(bool down);
#if EnableFSMouseMotion
void MyMousePositionSetDelta(uint16_t dh, uint16_t dv);
#endif
void MyMousePositionSet(uint16_t h, uint16_t v);

void InitKeyCodes(void);
void DisconnectKeyCodes(uint32_t KeepMask);
void MyEvtQTryRecoverFromFull(void);

MyEvtQEl * MyEvtQElPreviousIn(void);
MyEvtQEl * MyEvtQElAlloc(void);

void MacMsg(char *briefMsg, char *longMsg, bool fatal);

#if dbglog_HAVE
void dbglog_ReserveAlloc(void);
#endif

#if EmLocalTalk
void EntropyPoolAddPtr(uint8_t * p, uint32_t n);
void LT_PickStampNodeHint(void);
#endif
