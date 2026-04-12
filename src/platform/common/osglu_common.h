/*
	COMmon code for Operating System GLUe — declarations
*/

#pragma once

/* --- power-of-2 helper macros --- */

#define POW_OF_2(p) ((uint32_t)1 << (p))
#define POW2_MASK(p) (POW_OF_2(p) - 1)
#define MOD_POW2(i, p) ((i) & POW2_MASK(p))
#define FLOOR_DIV_POW2(i, p) ((i) >> (p))
#define FLOOR_POW2_MULT(i, p) ((i) & (~POW2_MASK(p)))
#define CEIL_POW2_MULT(i, p) FLOOR_POW2_MULT((i) + POW2_MASK(p), (p))

/* --- keep-mask constants for DisconnectKeyCodes --- */

#define kKeepMaskControl (1 << 0)
#define kKeepMaskCapsLock (1 << 1)
#define kKeepMaskCommand (1 << 2)
#define kKeepMaskOption (1 << 3)
#define kKeepMaskShift (1 << 4)

/* --- variables (extern declarations) --- */
/* NOTE: Variables already declared extern in platform/platform.h are
   intentionally NOT duplicated here. Include platform.h (via osglu_ud.h)
   for those. */

extern bool g_requestMacOff;

extern uint16_t g_viewHSize;
extern uint16_t g_viewVSize;
extern uint16_t g_viewHStart;
extern uint16_t g_viewVStart;
extern int16_t g_savedMouseH;
extern int16_t g_savedMouseV;

extern bool g_haveMouseMotion;

extern uint32_t g_pbufAllocatedMask;
extern uint32_t g_pbufSize[NumPbufs];
#define PbufIsAllocated(i) ((g_pbufAllocatedMask & ((uint32_t)1 << (i))) != 0)

extern uint32_t g_theKeys[4];
extern bool g_mouseButtonState;

/* event queue */
#define MyEvtQLg2Sz 4
#define MyEvtQSz (1 << MyEvtQLg2Sz)
#define MyEvtQIMask (MyEvtQSz - 1)

extern EvtQEl g_evtQA[MyEvtQSz];
extern uint16_t g_evtQIn;
extern uint16_t g_evtQOut;
extern bool g_evtQNeedRecover;

#if EmLocalTalk
extern uint32_t g_entropyPool[2];
extern uint32_t g_ltMyStamp;
#endif

/* --- function prototypes --- */
/* NOTE: Functions already declared in platform/platform.h are NOT
   duplicated here. */

void ScreenChangedAll();

void AutoScrollScreen();

bool FirstFreeDisk(DriveIndex *driveNo);
void DiskInsertNotify(DriveIndex driveNo, bool locked);
void DiskEjectedNotify(DriveIndex driveNo);

bool FirstFreePbuf(PbufIndex *r);
void PbufNewNotify(PbufIndex pbufNo, uint32_t count);
void PbufDisposeNotify(PbufIndex pbufNo);

void Keyboard_UpdateKeyMap(uint8_t key, bool down);
void MyMouseButtonSet(bool down);
void MyMousePositionSetDelta(uint16_t dh, uint16_t dv);
void MyMousePositionSet(uint16_t h, uint16_t v);

void InitKeyCodes();
void DisconnectKeyCodes(uint32_t keepMask);
void EvtQTryRecoverFromFull();

EvtQEl *EvtQElPreviousIn();
EvtQEl *EvtQElAlloc();

void MacMsg(const char *briefMsg, const char *longMsg, bool fatal);

bool dbglog_ReserveAlloc();

#ifndef dbglog_buflnsz
/* unbuffered mode — map directly to the platform _open0/_close0/_write0 */
#define dbglog_close dbglog_close0
#define dbglog_open dbglog_open0
#define dbglog_write dbglog_write0
#endif

#if EmLocalTalk
void EntropyPoolAddPtr(uint8_t *p, uint32_t n);
void LT_PickStampNodeHint();
#endif
