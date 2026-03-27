/*
	Operating System GLUe AAA

	header file for operating system dependent code (OSGLUxxx).
	the same header is used for all platforms.
	(The "AAA" is just to put it alphabetically in front
	of the OSGLUxxx files.)

	This code is descended from Richard F. Bannister's Macintosh
	port of vMac, by Philip Cummins.
*/

#pragma once


#if WantAbnormalReports
extern void WarnMsgAbnormalID(uint16_t id);
#endif

extern void WarnMsgUnsupportedDisk();

#if dbglog_HAVE
extern void dbglog_writeCStr(char *s);
extern void dbglog_writeReturn();
extern void dbglog_writeHex(uint32_t x);
extern void dbglog_writeNum(uint32_t x);
extern void dbglog_writeMacChar(uint8_t x);
extern void dbglog_writeln(char *s);
extern void dbglog_writelnHex(char *s, uint32_t x);
extern void dbglog_writelnNum(char *s, int32_t v);
#endif

#if dbglog_HAVE
extern void MacMsgDebugAlert(char *s);
#endif

extern void MyMoveBytes(uint8_t * srcPtr, uint8_t * destPtr, int32_t byteCount);

extern bool AllocBlock(uint8_t **p, uint32_t n, bool FillOnes);

extern uint8_t * ROM;

/*
	error codes returned by Mini vMac extensions
	(passed back to the emulated 68k code).
*/

enum class tMacErr : uint16_t {
	noErr            = 0x0000,  /*    0 - No Error */
	miscErr          = 0xFFFF,  /*   -1 - Should probably replace these */
	controlErr       = 0xFFEF,  /*  -17 - I/O System Errors */
	statusErr        = 0xFFEE,  /*  -18 - Driver can't respond to Status call */
	closErr          = 0xFFE8,  /*  -24 - I/O System Errors */
	eofErr           = 0xFFD9,  /*  -39 - End of file */
	tmfoErr          = 0xFFD6,  /*  -42 - too many files open */
	fnfErr           = 0xFFD5,  /*  -43 - File not found */
	wPrErr           = 0xFFD4,  /*  -44 - diskette is write protected */
	vLckdErr         = 0xFFD2,  /*  -46 - volume is locked */
	dupFNErr         = 0xFFD0,  /*  -48 - duplicate filename */
	opWrErr          = 0xFFCF,  /*  -49 - file already open with write permission */
	paramErr         = 0xFFCE,  /*  -50 - error in parameter list */
	permErr          = 0xFFCA,  /*  -54 - permissions error (on file open) */
	nsDrvErr         = 0xFFC8,  /*  -56 - No Such Drive */
	wrPermErr        = 0xFFC3,  /*  -61 - write permissions error */
	offLinErr        = 0xFFBF,  /*  -65 - off-line drive */
	dirNFErr         = 0xFF88,  /* -120 - directory not found */
	afpAccessDenied  = 0xEC78,  /* -5000 - Insufficient access privileges */
};

/* Backward-compat aliases — allow incremental migration. */
constexpr tMacErr mnvm_noErr            = tMacErr::noErr;
constexpr tMacErr mnvm_miscErr          = tMacErr::miscErr;
constexpr tMacErr mnvm_controlErr       = tMacErr::controlErr;
constexpr tMacErr mnvm_statusErr        = tMacErr::statusErr;
constexpr tMacErr mnvm_closErr          = tMacErr::closErr;
constexpr tMacErr mnvm_eofErr           = tMacErr::eofErr;
constexpr tMacErr mnvm_tmfoErr          = tMacErr::tmfoErr;
constexpr tMacErr mnvm_fnfErr           = tMacErr::fnfErr;
constexpr tMacErr mnvm_wPrErr           = tMacErr::wPrErr;
constexpr tMacErr mnvm_vLckdErr         = tMacErr::vLckdErr;
constexpr tMacErr mnvm_dupFNErr         = tMacErr::dupFNErr;
constexpr tMacErr mnvm_opWrErr          = tMacErr::opWrErr;
constexpr tMacErr mnvm_paramErr         = tMacErr::paramErr;
constexpr tMacErr mnvm_permErr          = tMacErr::permErr;
constexpr tMacErr mnvm_nsDrvErr         = tMacErr::nsDrvErr;
constexpr tMacErr mnvm_wrPermErr        = tMacErr::wrPermErr;
constexpr tMacErr mnvm_offLinErr        = tMacErr::offLinErr;
constexpr tMacErr mnvm_dirNFErr         = tMacErr::dirNFErr;
constexpr tMacErr mnvm_afpAccessDenied  = tMacErr::afpAccessDenied;


using tPbuf = uint16_t;

#define NotAPbuf ((tPbuf)0xFFFF)

extern tMacErr CheckPbuf(tPbuf Pbuf_No);
extern tMacErr PbufGetSize(tPbuf Pbuf_No, uint32_t *Count);

extern tMacErr PbufNew(uint32_t count, tPbuf *r);
extern void PbufDispose(tPbuf i);
extern void PbufTransfer(uint8_t * Buffer,
	tPbuf i, uint32_t offset, uint32_t count, bool IsWrite);


using tDrive = uint16_t;

extern uint32_t vSonyWritableMask;
extern uint32_t vSonyInsertedMask;

#define vSonyIsInserted(Drive_No) \
	((vSonyInsertedMask & ((uint32_t)1 << (Drive_No))) != 0)

extern tMacErr vSonyTransfer(bool IsWrite, uint8_t * Buffer,
	tDrive Drive_No, uint32_t Sony_Start, uint32_t Sony_Count,
	uint32_t *Sony_ActCount);
extern tMacErr vSonyEject(tDrive Drive_No);
extern tMacErr vSonyGetSize(tDrive Drive_No, uint32_t *Sony_Count);

extern bool AnyDiskInserted();
extern void DiskRevokeWritable(tDrive Drive_No);

extern bool vSonyRawMode;

extern bool vSonyNewDiskWanted;
extern uint32_t vSonyNewDiskSize;
extern tMacErr vSonyEjectDelete(tDrive Drive_No);

extern tPbuf vSonyNewDiskName;

extern tMacErr vSonyGetName(tDrive Drive_No, tPbuf *r);

extern tMacErr HTCEexport(tPbuf i);
extern tMacErr HTCEimport(tPbuf *r);

extern uint32_t OnTrueTime;

extern uint32_t CurMacDateInSeconds;
extern uint32_t CurMacLatitude;
extern uint32_t CurMacLongitude;
extern uint32_t CurMacDelta;
	/* (dlsDelta << 24) | (gmtDelta & 0x00FFFFFF) */


/* --- Runtime screen dimensions (set from MachineConfig at init) --- */
extern uint16_t g_screenWidth;
extern uint16_t g_screenHeight;
extern uint8_t  g_screenDepth;

#define vMacScreenWidth  ((long)g_screenWidth)
#define vMacScreenHeight ((long)g_screenHeight)
#define vMacScreenDepth  ((int)g_screenDepth)

#define vMacScreenNumPixels \
	((long)vMacScreenHeight * (long)vMacScreenWidth)
#define vMacScreenNumBits (vMacScreenNumPixels << vMacScreenDepth)
#define vMacScreenNumBytes (vMacScreenNumBits / 8)
#define vMacScreenBitWidth ((long)vMacScreenWidth << vMacScreenDepth)
#define vMacScreenByteWidth (vMacScreenBitWidth / 8)

#define vMacScreenMonoNumBytes (vMacScreenNumPixels / 8)
#define vMacScreenMonoByteWidth ((long)vMacScreenWidth / 8)

extern bool UseColorMode;
extern bool ColorModeWorks;

extern bool ColorMappingChanged;

#define CLUT_size 256

extern uint16_t CLUT_reds[CLUT_size];
extern uint16_t CLUT_greens[CLUT_size];
extern uint16_t CLUT_blues[CLUT_size];

extern bool EmVideoDisable;
extern int8_t EmLagTime;

extern void Screen_OutputFrame(uint8_t * screencurrentbuff);
extern void DoneWithDrawingForTick();

extern bool ForceMacOff;

extern bool WantMacInterrupt;

extern bool WantMacReset;

extern bool ExtraTimeNotOver();

extern uint8_t SpeedValue;

extern bool g_SkipThrottle;

extern bool WantNotAutoSlow;

/* where emulated machine thinks mouse is */
extern uint16_t CurMouseV;
extern uint16_t CurMouseH;

extern uint32_t QuietTime;
extern uint32_t QuietSubTicks;

#define QuietEnds() \
{ \
	QuietTime = 0; \
	QuietSubTicks = 0; \
}

using trSoundSamp = uint16_t;
using tbSoundSamp = uint16_t;
using tpSoundSamp = uint16_t *;
#define kCenterSound 0x8000


extern tpSoundSamp MySound_BeginWrite(uint16_t n, uint16_t *actL);
extern void MySound_EndWrite(uint16_t actL);

/* 370 samples per tick = 22,254.54 per second */

#if EmLocalTalk

extern uint8_t LT_NodeHint;

#if LT_MayHaveEcho
extern bool CertainlyNotMyPacket;
#endif

#define LT_TxBfMxSz 1024
extern uint8_t * LT_TxBuffer;
extern uint16_t LT_TxBuffSz;

extern void LT_TransmitPacket();

extern uint8_t * LT_RxBuffer;
extern uint32_t LT_RxBuffSz;

extern void LT_ReceivePacket();

#endif

extern void WaitForNextTick();

enum class EvtQElKind : uint8_t {
	Key        = 0,
	MouseButton = 1,
	MousePos   = 2,
	MouseDelta = 3,
};

struct MyEvtQEl {
	/* expected size : 8 bytes */
	EvtQElKind kind;
	uint8_t pad[3];
	union {
		struct {
			uint8_t down;
			uint8_t key;
		} press;
		struct {
			uint16_t h;
			uint16_t v;
		} pos;
	} u;
};
typedef struct MyEvtQEl MyEvtQEl;

extern MyEvtQEl * MyEvtQOutP();
extern void MyEvtQOutDone();

#include "keycodes.h"
