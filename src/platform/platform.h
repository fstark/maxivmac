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

extern void MoveBytes(uint8_t * srcPtr, uint8_t * destPtr, int32_t byteCount);

extern bool AllocBlock(uint8_t **p, uint32_t n, bool fillOnes);

extern uint8_t * g_rom;

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

using PbufIndex = uint16_t;

#define NOT_A_PBUF ((PbufIndex)0xFFFF)

extern tMacErr CheckPbuf(PbufIndex pbufNo);
extern tMacErr PbufGetSize(PbufIndex pbufNo, uint32_t *count);

extern tMacErr PbufNew(uint32_t count, PbufIndex *r);
extern void PbufDispose(PbufIndex i);
extern void PbufTransfer(uint8_t * buffer,
	PbufIndex i, uint32_t offset, uint32_t count, bool isWrite);


using DriveIndex = uint16_t;

extern uint32_t g_sonyWritableMask;
extern uint32_t g_sonyInsertedMask;

#define vSonyIsInserted(driveNo) \
	((g_sonyInsertedMask & ((uint32_t)1 << (driveNo))) != 0)

extern tMacErr vSonyTransfer(bool isWrite, uint8_t * buffer,
	DriveIndex driveNo, uint32_t sonyStart, uint32_t sonyCount,
	uint32_t *sonyActCount);
extern tMacErr vSonyEject(DriveIndex driveNo);
extern tMacErr vSonyGetSize(DriveIndex driveNo, uint32_t *sonyCount);

extern bool AnyDiskInserted();
extern void DiskRevokeWritable(DriveIndex driveNo);

extern bool g_sonyRawMode;

extern bool g_sonyNewDiskWanted;
extern uint32_t g_sonyNewDiskSize;
extern tMacErr vSonyEjectDelete(DriveIndex driveNo);

extern PbufIndex g_sonyNewDiskName;

extern tMacErr vSonyGetName(DriveIndex driveNo, PbufIndex *r);

extern tMacErr HTCEexport(PbufIndex i);
extern tMacErr HTCEimport(PbufIndex *r);

extern uint32_t g_onTrueTime;

extern uint32_t g_curMacDateInSeconds;
extern uint32_t g_curMacLatitude;
extern uint32_t g_curMacLongitude;
extern uint32_t g_curMacDelta;
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

extern bool g_useColorMode;
extern bool g_colorModeWorks;

extern bool g_colorMappingChanged;

#define CLUT_size 256

extern uint16_t CLUT_reds[CLUT_size];
extern uint16_t CLUT_greens[CLUT_size];
extern uint16_t CLUT_blues[CLUT_size];

extern bool g_emVideoDisable;
extern int8_t g_emLagTime;

extern void Screen_OutputFrame(uint8_t * screencurrentbuff);
extern void DoneWithDrawingForTick();

extern bool g_forceMacOff;

extern bool g_wantMacInterrupt;

extern bool g_wantMacReset;

extern bool ExtraTimeNotOver();

extern uint8_t g_speedValue;

extern bool g_SkipThrottle;

extern bool g_wantNotAutoSlow;

/* where emulated machine thinks mouse is */
extern uint16_t g_curMouseV;
extern uint16_t g_curMouseH;

extern uint32_t g_quietTime;
extern uint32_t g_quietSubTicks;

#define QuietEnds() \
{ \
	g_quietTime = 0; \
	g_quietSubTicks = 0; \
}

using RawSoundSample = uint16_t;
using BufferedSoundSample = uint16_t;
using SoundSamplePtr = uint16_t *;
#define kCenterSound 0x8000


extern SoundSamplePtr Sound_BeginWrite(uint16_t n, uint16_t *actL);
extern void Sound_EndWrite(uint16_t actL);

/* 370 samples per tick = 22,254.54 per second */

#if EmLocalTalk

extern uint8_t g_ltNodeHint;

#if LT_MayHaveEcho
extern bool g_certainlyNotMyPacket;
#endif

#define LT_TxBfMxSz 1024
extern uint8_t * g_ltTxBuffer;
extern uint16_t g_ltTxBuffSz;

extern void LT_TransmitPacket();

extern uint8_t * g_ltRxBuffer;
extern uint32_t g_ltRxBuffSz;

extern void LT_ReceivePacket();

#endif

extern void WaitForNextTick();

enum class EvtQElKind : uint8_t {
	Key        = 0,
	MouseButton = 1,
	MousePos   = 2,
	MouseDelta = 3,
};

struct EvtQEl {
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

extern EvtQEl * EvtQOutP();
extern void EvtQOutDone();

#include "keycodes.h"
