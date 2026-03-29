/*
	COMmon code for Operating System GLUe — declarations
*/

#pragma once

/* --- feature computation macros --- */

#define EnableFSMouseMotion 1
#define EnableRecreateW 1
#define EnableMoveMouse 1

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

extern bool g_romLoaded;
extern bool g_requestMacOff;

extern uint8_t * g_screenCompareBuff;

extern int16_t g_screenChangedTop;
extern int16_t g_screenChangedLeft;
extern int16_t g_screenChangedBottom;
extern int16_t g_screenChangedRight;

extern int16_t g_screenChangedQuietTop;
extern int16_t g_screenChangedQuietLeft;
extern int16_t g_screenChangedQuietBottom;
extern int16_t g_screenChangedQuietRight;

extern uint16_t g_viewHSize;
extern uint16_t g_viewVSize;
extern uint16_t g_viewHStart;
extern uint16_t g_viewVStart;
#if EnableFSMouseMotion
extern int16_t g_savedMouseH;
extern int16_t g_savedMouseV;
#endif

#if EnableFSMouseMotion
extern bool g_haveMouseMotion;
#endif

extern uint32_t g_pbufAllocatedMask;
extern uint32_t PbufSize[NumPbufs];
#define PbufIsAllocated(i) ((g_pbufAllocatedMask & ((uint32_t)1 << (i))) != 0)

extern uint32_t theKeys[4];
extern bool g_mouseButtonState;

extern uint16_t g_mousePosCurV;
extern uint16_t g_mousePosCurH;

/* event queue */
#define MyEvtQLg2Sz 4
#define MyEvtQSz (1 << MyEvtQLg2Sz)
#define MyEvtQIMask (MyEvtQSz - 1)

extern EvtQEl EvtQA[MyEvtQSz];
extern uint16_t EvtQIn;
extern uint16_t EvtQOut;
extern bool EvtQNeedRecover;

extern const char *SavedBriefMsg;
extern const char *SavedLongMsg;
#if WantAbnormalReports
extern uint16_t g_savedIDMsg;
#endif
extern bool g_savedFatalMsg;

#define WantColorTransValid 1
#if WantColorTransValid
extern bool g_colorTransValid;
#endif

#if EmLocalTalk
extern uint32_t e_p[2];
extern uint32_t g_ltMyStamp;
#endif

/* --- function prototypes --- */
/* NOTE: Functions already declared in platform/platform.h are NOT
   duplicated here. */

void ScreenClearChanges();
void ScreenChangedAll();

#if EnableFSMouseMotion
void AutoScrollScreen();
#endif

bool FirstFreeDisk(DriveIndex *Drive_No);
void DiskInsertNotify(DriveIndex Drive_No, bool locked);
void DiskEjectedNotify(DriveIndex Drive_No);

bool FirstFreePbuf(PbufIndex *r);
void PbufNewNotify(PbufIndex Pbuf_No, uint32_t count);
void PbufDisposeNotify(PbufIndex Pbuf_No);

void Keyboard_UpdateKeyMap(uint8_t key, bool down);
void MyMouseButtonSet(bool down);
#if EnableFSMouseMotion
void MyMousePositionSetDelta(uint16_t dh, uint16_t dv);
#endif
void MyMousePositionSet(uint16_t h, uint16_t v);

void InitKeyCodes();
void DisconnectKeyCodes(uint32_t KeepMask);
void EvtQTryRecoverFromFull();

EvtQEl * EvtQElPreviousIn();
EvtQEl * EvtQElAlloc();

void MacMsg(const char *briefMsg, const char *longMsg, bool fatal);

#if dbglog_HAVE
bool dbglog_ReserveAlloc();

#ifndef dbglog_buflnsz
/* unbuffered mode — map directly to the platform _open0/_close0/_write0 */
#define dbglog_close dbglog_close0
#define dbglog_open  dbglog_open0
#define dbglog_write dbglog_write0
#endif
#endif

#if EmLocalTalk
void EntropyPoolAddPtr(uint8_t * p, uint32_t n);
void LT_PickStampNodeHint();
#endif
