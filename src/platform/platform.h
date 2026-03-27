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

#define MyEvtQElKindKey 0
#define MyEvtQElKindMouseButton 1
#define MyEvtQElKindMousePos 2
#define MyEvtQElKindMouseDelta 3

struct MyEvtQEl {
	/* expected size : 8 bytes */
	uint8_t kind;
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

#define MKC_A 0x00
#define MKC_B 0x0B
#define MKC_C 0x08
#define MKC_D 0x02
#define MKC_E 0x0E
#define MKC_F 0x03
#define MKC_G 0x05
#define MKC_H 0x04
#define MKC_I 0x22
#define MKC_J 0x26
#define MKC_K 0x28
#define MKC_L 0x25
#define MKC_M 0x2E
#define MKC_N 0x2D
#define MKC_O 0x1F
#define MKC_P 0x23
#define MKC_Q 0x0C
#define MKC_R 0x0F
#define MKC_S 0x01
#define MKC_T 0x11
#define MKC_U 0x20
#define MKC_V 0x09
#define MKC_W 0x0D
#define MKC_X 0x07
#define MKC_Y 0x10
#define MKC_Z 0x06

#define MKC_1 0x12
#define MKC_2 0x13
#define MKC_3 0x14
#define MKC_4 0x15
#define MKC_5 0x17
#define MKC_6 0x16
#define MKC_7 0x1A
#define MKC_8 0x1C
#define MKC_9 0x19
#define MKC_0 0x1D

#define MKC_Command 0x37
#define MKC_Shift 0x38
#define MKC_CapsLock 0x39
#define MKC_Option 0x3A

#define MKC_Space 0x31
#define MKC_Return 0x24
#define MKC_BackSpace 0x33
#define MKC_Tab 0x30

#define MKC_Left /* 0x46 */ 0x7B
#define MKC_Right /* 0x42 */ 0x7C
#define MKC_Down /* 0x48 */ 0x7D
#define MKC_Up /* 0x4D */ 0x7E

#define MKC_Minus 0x1B
#define MKC_Equal 0x18
#define MKC_BackSlash 0x2A
#define MKC_Comma 0x2B
#define MKC_Period 0x2F
#define MKC_Slash 0x2C
#define MKC_SemiColon 0x29
#define MKC_SingleQuote 0x27
#define MKC_LeftBracket 0x21
#define MKC_RightBracket 0x1E
#define MKC_Grave 0x32
#define MKC_Clear 0x47
#define MKC_KPEqual 0x51
#define MKC_KPDevide 0x4B
#define MKC_KPMultiply 0x43
#define MKC_KPSubtract 0x4E
#define MKC_KPAdd 0x45
#define MKC_Enter 0x4C

#define MKC_KP1 0x53
#define MKC_KP2 0x54
#define MKC_KP3 0x55
#define MKC_KP4 0x56
#define MKC_KP5 0x57
#define MKC_KP6 0x58
#define MKC_KP7 0x59
#define MKC_KP8 0x5B
#define MKC_KP9 0x5C
#define MKC_KP0 0x52
#define MKC_Decimal 0x41

/* these aren't on the Mac Plus keyboard */

#define MKC_Control 0x3B
#define MKC_Escape 0x35
#define MKC_F1 0x7a
#define MKC_F2 0x78
#define MKC_F3 0x63
#define MKC_F4 0x76
#define MKC_F5 0x60
#define MKC_F6 0x61
#define MKC_F7 0x62
#define MKC_F8 0x64
#define MKC_F9 0x65
#define MKC_F10 0x6d
#define MKC_F11 0x67
#define MKC_F12 0x6f

#define MKC_Home 0x73
#define MKC_End 0x77
#define MKC_PageUp 0x74
#define MKC_PageDown 0x79
#define MKC_Help 0x72 /* = Insert */
#define MKC_ForwardDel 0x75
#define MKC_Print 0x69
#define MKC_ScrollLock 0x6B
#define MKC_Pause 0x71

#define MKC_AngleBracket 0x0A /* found on german keyboard */

/*
	Additional codes found in Apple headers

	#define MKC_RightShift 0x3C
	#define MKC_RightOption 0x3D
	#define MKC_RightControl 0x3E
	#define MKC_Function 0x3F

	#define MKC_VolumeUp 0x48
	#define MKC_VolumeDown 0x49
	#define MKC_Mute 0x4A

	#define MKC_F16 0x6A
	#define MKC_F17 0x40
	#define MKC_F18 0x4F
	#define MKC_F19 0x50
	#define MKC_F20 0x5A

	#define MKC_F13 MKC_Print
	#define MKC_F14 MKC_ScrollLock
	#define MKC_F15 MKC_Pause
*/

/* not Apple key codes, only for Mini vMac */

#define MKC_CM 0x80
#define MKC_real_CapsLock 0x81
	/*
		for use in platform specific code
		when CapsLocks need special handling.
	*/
#define MKC_None 0xFF
