/*
	OSGLUNDS.c

	Copyright (C) 2012 Lazyone, Paul C. Pratt

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
	Operating System GLUe for Nintendo DS
*/

#include "OSGCOMUI.h"
#include "OSGCOMUD.h"

#ifdef WantOSGLUNDS

#include "FB1BPP2I.h"

#define CONSOLE_TRACE() \
	fprintf(stderr, "%s() at line %d\n", __FUNCTION__, __LINE__)

/* --- some simple utilities --- */

void MyMoveBytes(uint8_t * srcPtr, uint8_t * destPtr, int32_t byteCount)
{
	(void) memcpy((char *)destPtr, (char *)srcPtr, byteCount);
}

/*
	Nintendo DS port globals
*/
#define DS_ScreenWidth 256
#define DS_ScreenHeight 192

static volatile int VBlankCounter = 0;
static volatile int HBlankCounter = 0;
static volatile unsigned int TimerBaseMSec = 0;
static Keyboard* DSKeyboard = NULL;
static volatile int LastKeyboardKey = NOKEY;
static volatile int KeyboardKey = NOKEY;
static volatile int KeysHeld = 0;
static volatile int CursorX = 0;
static volatile int CursorY = 0;
static int Display_bg2_Main = 0;

/* --- control mode and internationalization --- */

#define NeedCell2PlainAsciiMap 1

#include "INTLCHAR.h"

/* --- sending debugging info to file --- */

#if dbglog_HAVE

#define dbglog_ToStdErr 1

#if ! dbglog_ToStdErr
static FILE *dbglog_File = NULL;
#endif

static bool dbglog_open0(void)
{
#if dbglog_ToStdErr
	return true;
#else
	dbglog_File = fopen("dbglog.txt", "w");
	return (NULL != dbglog_File);
#endif
}

static void dbglog_write0(char *s, uint32_t L)
{
#if dbglog_ToStdErr
	(void) fwrite(s, 1, L, stderr);
#else
	if (dbglog_File != NULL) {
		(void) fwrite(s, 1, L, dbglog_File);
	}
#endif
}

static void dbglog_close0(void)
{
#if ! dbglog_ToStdErr
	if (dbglog_File != NULL) {
		fclose(dbglog_File);
		dbglog_File = NULL;
	}
#endif
}

#endif

/* --- debug settings and utilities --- */

#if ! dbglog_HAVE
#define WriteExtraErr(s)
#else
static void WriteExtraErr(char *s)
{
	dbglog_writeCStr("*** error: ");
	dbglog_writeCStr(s);
	dbglog_writeReturn();
}
#endif

/* --- information about the environment --- */

#define WantColorTransValid 0

#include "COMOSGLU.h"
#include "CONTROLM.h"

static void NativeStrFromCStr(char *r, char *s)
{
	uint8_t ps[ClStrMaxLength];
	int i;
	int L;

	ClStrFromSubstCStr(&L, ps, s);

	for (i = 0; i < L; ++i) {
		r[i] = Cell2PlainAsciiMap[ps[i]];
	}

	r[L] = 0;
}

/* --- drives --- */

#define NotAfileRef NULL

static FILE *Drives[NumDrives]; /* open disk image files */
#if IncludeSonyGetName || IncludeSonyNew
static char *DriveNames[NumDrives];
#endif

static void InitDrives(void)
{
	/*
		This isn't really needed, Drives[i] and DriveNames[i]
		need not have valid values when not vSonyIsInserted[i].
	*/
	tDrive i;

	for (i = 0; i < NumDrives; ++i) {
		Drives[i] = NotAfileRef;
#if IncludeSonyGetName || IncludeSonyNew
		DriveNames[i] = NULL;
#endif
	}
}

 tMacErr vSonyTransfer(bool IsWrite, uint8_t * Buffer,
	tDrive Drive_No, uint32_t Sony_Start, uint32_t Sony_Count,
	uint32_t *Sony_ActCount)
{
	tMacErr err = mnvm_miscErr;
	FILE *refnum = Drives[Drive_No];
	uint32_t NewSony_Count = 0;

	if (0 == fseek(refnum, Sony_Start, SEEK_SET)) {
		if (IsWrite) {
			NewSony_Count = fwrite(Buffer, 1, Sony_Count, refnum);
		} else {
			NewSony_Count = fread(Buffer, 1, Sony_Count, refnum);
		}

		if (NewSony_Count == Sony_Count) {
			err = mnvm_noErr;
		}
	}

	if (nullptr != Sony_ActCount) {
		*Sony_ActCount = NewSony_Count;
	}

	return err; /*& figure out what really to return &*/
}

 tMacErr vSonyGetSize(tDrive Drive_No, uint32_t *Sony_Count)
{
	tMacErr err = mnvm_miscErr;
	FILE *refnum = Drives[Drive_No];
	long v;

	if (0 == fseek(refnum, 0, SEEK_END)) {
		v = ftell(refnum);
		if (v >= 0) {
			*Sony_Count = v;
			err = mnvm_noErr;
		}
	}

	return err; /*& figure out what really to return &*/
}

static tMacErr vSonyEject0(tDrive Drive_No, bool deleteit)
{
	FILE *refnum = Drives[Drive_No];

	DiskEjectedNotify(Drive_No);

	fclose(refnum);
	Drives[Drive_No] = NotAfileRef; /* not really needed */

#if IncludeSonyGetName || IncludeSonyNew
	{
		char *s = DriveNames[Drive_No];
		if (NULL != s) {
			if (deleteit) {
				remove(s);
			}
			free(s);
			DriveNames[Drive_No] = NULL; /* not really needed */
		}
	}
#endif

	return mnvm_noErr;
}

 tMacErr vSonyEject(tDrive Drive_No)
{
	return vSonyEject0(Drive_No, false);
}

#if IncludeSonyNew
 tMacErr vSonyEjectDelete(tDrive Drive_No)
{
	return vSonyEject0(Drive_No, true);
}
#endif

static void UnInitDrives(void)
{
	tDrive i;

	for (i = 0; i < NumDrives; ++i) {
		if (vSonyIsInserted(i)) {
			(void) vSonyEject(i);
		}
	}
}

#if IncludeSonyGetName
 tMacErr vSonyGetName(tDrive Drive_No, tPbuf *r)
{
	char *drivepath = DriveNames[Drive_No];
	if (NULL == drivepath) {
		return mnvm_miscErr;
	} else {
		char *s = strrchr(drivepath, '/');
		if (NULL == s) {
			s = drivepath;
		} else {
			++s;
		}
		return NativeTextToMacRomanPbuf(s, r);
	}
}
#endif

static bool Sony_Insert0(FILE *refnum, bool locked,
	char *drivepath)
{
	tDrive Drive_No;
	bool IsOk = false;

	if (! FirstFreeDisk(&Drive_No)) {
		MacMsg(kStrTooManyImagesTitle, kStrTooManyImagesMessage,
			false);
	} else {
		/* printf("Sony_Insert0 %d\n", (int)Drive_No); */

		{
			Drives[Drive_No] = refnum;
			DiskInsertNotify(Drive_No, locked);

#if IncludeSonyGetName || IncludeSonyNew
			{
				uint32_t L = strlen(drivepath);
				char *p = malloc(L + 1);
				if (p != NULL) {
					(void) memcpy(p, drivepath, L + 1);
				}
				DriveNames[Drive_No] = p;
			}
#endif

			IsOk = true;
		}
	}

	if (! IsOk) {
		fclose(refnum);
	}

	return IsOk;
}

static bool Sony_Insert1(char *drivepath, bool silentfail)
{
	bool locked = false;
	/* printf("Sony_Insert1 %s\n", drivepath); */
	FILE *refnum = fopen(drivepath, "rb+");
	if (NULL == refnum) {
		locked = true;
		refnum = fopen(drivepath, "rb");
		CONSOLE_TRACE();
	}
	if (NULL == refnum) {
		if (! silentfail) {
			MacMsg(kStrOpenFailTitle, kStrOpenFailMessage, false);
			CONSOLE_TRACE();
		}
	} else {
		CONSOLE_TRACE();
		return Sony_Insert0(refnum, locked, drivepath);
	}

	return false;
}

#define Sony_Insert2(s) Sony_Insert1(s, true)

static bool Sony_InsertIth(int i)
{
	bool v;

	if ((i > 9) || ! FirstFreeDisk(nullptr)) {
		v = false;
	} else {
		char s[] = "disk?.dsk";

		s[4] = '0' + i;

		v = Sony_Insert2(s);
	}

	return v;
}

static bool LoadInitialImages(void)
{
	int i;

	CONSOLE_TRACE();

	for (i = 1; Sony_InsertIth(i); ++i) {
		/* stop on first error (including file not found) */
	}

	return true;
}

#if IncludeSonyNew
static bool WriteZero(FILE *refnum, uint32_t L)
{
#define ZeroBufferSize 2048
	uint32_t i;
	uint8_t buffer[ZeroBufferSize];

	memset(&buffer, 0, ZeroBufferSize);

	while (L > 0) {
		i = (L > ZeroBufferSize) ? ZeroBufferSize : L;
		if (fwrite(buffer, 1, i, refnum) != i) {
			return false;
		}
		L -= i;
	}
	return true;
}
#endif

#if IncludeSonyNew
static void MakeNewDisk(uint32_t L, char *drivepath)
{
	bool IsOk = false;
	FILE *refnum = fopen(drivepath, "wb+");
	if (NULL == refnum) {
		MacMsg(kStrOpenFailTitle, kStrOpenFailMessage, false);
	} else {
		if (WriteZero(refnum, L)) {
			IsOk = Sony_Insert0(refnum, false, drivepath);
			refnum = NULL;
		}
		if (refnum != NULL) {
			fclose(refnum);
		}
		if (! IsOk) {
			(void) remove(drivepath);
		}
	}
}
#endif

#if IncludeSonyNew
static void MakeNewDiskAtDefault(uint32_t L)
{
	char s[ClStrMaxLength + 1];

	NativeStrFromCStr(s, "untitled.dsk");
	MakeNewDisk(L, s);
}
#endif

/* --- ROM --- */

static tMacErr LoadMacRomFrom(char *path)
{
	tMacErr err;
	FILE *ROM_File;
	int File_Size;

	ROM_File = fopen(path, "rb");
	if (NULL == ROM_File) {
		err = mnvm_fnfErr;
	} else {
		File_Size = fread(ROM, 1, kROM_Size, ROM_File);
		if (kROM_Size != File_Size) {
			if (feof(ROM_File)) {
				MacMsgOverride(kStrShortROMTitle,
					kStrShortROMMessage);
				err = mnvm_eofErr;
			} else {
				MacMsgOverride(kStrNoReadROMTitle,
					kStrNoReadROMMessage);
				err = mnvm_miscErr;
			}
		} else {
			err = ROM_IsValid();
		}
		fclose(ROM_File);
	}

	return err;
}

static bool LoadMacRom(void)
{
	tMacErr err;

	if (mnvm_fnfErr == (err = LoadMacRomFrom(RomFileName)))
	{
	}

	return true; /* keep launching Mini vMac, regardless */
}

/* --- video out --- */

#if MayFullScreen
static short hOffset;
static short vOffset;
#endif

#if VarFullScreen
static bool UseFullScreen = (WantInitFullScreen != 0);
#endif

#if EnableMagnify
static bool UseMagnify = (WantInitMagnify != 0);
#endif

static bool CurSpeedStopped = true;

#if EnableMagnify
#define MaxScale MyWindowScale
#else
#define MaxScale 1
#endif

static void HaveChangedScreenBuff(uint16_t top, uint16_t left,
	uint16_t bottom, uint16_t right)
{
	/*
		Oh god, clean this up.
	*/
	u8 *octpix = NULL;
	u32 *vram = NULL;

	octpix = (u8 *)GetCurDrawBuff();
	vram = (u32 *)BG_BMP_RAM(0);

	octpix += ((top * vMacScreenWidth ) >> 3);
	vram += ((top * vMacScreenWidth ) >> 2);

	FB1BPPtoIndexed(vram, octpix,
		((bottom - top) * vMacScreenWidth) >> 3);
}

static void MyDrawChangesAndClear(void)
{
	if (ScreenChangedBottom > ScreenChangedTop) {
		HaveChangedScreenBuff(ScreenChangedTop, ScreenChangedLeft,
			ScreenChangedBottom, ScreenChangedRight);
		ScreenClearChanges();
	}
}

void DoneWithDrawingForTick(void)
{
#if 0 && EnableFSMouseMotion
	if (HaveMouseMotion) {
		AutoScrollScreen();
	}
#endif
	MyDrawChangesAndClear();
}

/* --- mouse --- */

/* cursor state */

static void CheckMouseState(void)
{
	int32_t MotionX;
	int32_t MotionY;

	/*
		TODO:

		- Don't hardcode motion values
		- Acceleration?
		- Allow key remapping
		- Handle touchscreen input (non-mouse motion)
		- Handle touchscreen input (trackpad style mouse motion)
	*/

	if (0 != (KeysHeld & KEY_LEFT)) {
		MotionX = -4;
	} else if (0 != (KeysHeld & KEY_RIGHT)) {
		MotionX = 4;
	}

	if (0 != (KeysHeld & KEY_UP)) {
		MotionY = -4;
	} else if (0 != (KeysHeld & KEY_DOWN)) {
		MotionY = 4;
	}

	HaveMouseMotion = true;

	MyMousePositionSetDelta(MotionX, MotionY);
	MyMouseButtonSet(0 != (KeysHeld & KEY_A));
}

/* --- keyboard input --- */

static uint8_t KC2MKC[256];

/*
	AHA!
	GCC Was turning this into a macro of some sort which of course
	broke horribly with libnds's keyboard having some negative values.
*/
static void AssignKeyToMKC(int UKey, int LKey, uint8_t MKC)
{
	if (UKey != NOKEY) {
		KC2MKC[UKey] = MKC;
	}

	if (LKey != NOKEY) {
		KC2MKC[LKey] = MKC;
	}
}

static bool KC2MKCInit(void)
{
	int i;

	for (i = 0; i < 256; ++i) {
		KC2MKC[i] = MKC_None;
	}

	AssignKeyToMKC('A', 'a', MKC_A);
	AssignKeyToMKC('B', 'b', MKC_B);
	AssignKeyToMKC('C', 'c', MKC_C);
	AssignKeyToMKC('D', 'd', MKC_D);
	AssignKeyToMKC('E', 'e', MKC_E);
	AssignKeyToMKC('F', 'f', MKC_F);
	AssignKeyToMKC('G', 'g', MKC_G);
	AssignKeyToMKC('H', 'h', MKC_H);
	AssignKeyToMKC('I', 'i', MKC_I);
	AssignKeyToMKC('J', 'j', MKC_J);
	AssignKeyToMKC('K', 'k', MKC_K);
	AssignKeyToMKC('L', 'l', MKC_L);
	AssignKeyToMKC('M', 'm', MKC_M);
	AssignKeyToMKC('N', 'n', MKC_N);
	AssignKeyToMKC('O', 'o', MKC_O);
	AssignKeyToMKC('P', 'p', MKC_P);
	AssignKeyToMKC('Q', 'q', MKC_Q);
	AssignKeyToMKC('R', 'r', MKC_R);
	AssignKeyToMKC('S', 's', MKC_S);
	AssignKeyToMKC('T', 't', MKC_T);
	AssignKeyToMKC('U', 'u', MKC_U);
	AssignKeyToMKC('V', 'v', MKC_V);
	AssignKeyToMKC('W', 'w', MKC_W);
	AssignKeyToMKC('X', 'x', MKC_X);
	AssignKeyToMKC('Y', 'y', MKC_Y);
	AssignKeyToMKC('Z', 'z', MKC_Z);

	AssignKeyToMKC(')', '0', MKC_0);
	AssignKeyToMKC('!', '1', MKC_1);
	AssignKeyToMKC('@', '2', MKC_2);
	AssignKeyToMKC('#', '3', MKC_3);
	AssignKeyToMKC('$', '4', MKC_4);
	AssignKeyToMKC('%', '5', MKC_5);
	AssignKeyToMKC('^', '6', MKC_6);
	AssignKeyToMKC('&', '7', MKC_7);
	AssignKeyToMKC('*', '8', MKC_8);
	AssignKeyToMKC('(', '9', MKC_9);

	AssignKeyToMKC('~', '`', MKC_formac_Grave);
	AssignKeyToMKC('_', '-', MKC_Minus);
	AssignKeyToMKC('+', '=', MKC_Equal);
	AssignKeyToMKC(':', ';', MKC_SemiColon);
	AssignKeyToMKC('\"', '\'', MKC_SingleQuote);
	AssignKeyToMKC('{', '[', MKC_LeftBracket);
	AssignKeyToMKC('}', ']', MKC_RightBracket);
	AssignKeyToMKC('|', '\\', MKC_formac_BackSlash);
	AssignKeyToMKC('<', ',', MKC_Comma);
	AssignKeyToMKC('>', '.', MKC_Period);
	AssignKeyToMKC('?', '/', MKC_formac_Slash);

	AssignKeyToMKC(NOKEY, DVK_SPACE, MKC_Space);
	AssignKeyToMKC(NOKEY, DVK_BACKSPACE, MKC_BackSpace);
	AssignKeyToMKC(NOKEY, DVK_ENTER, MKC_formac_Enter);
	AssignKeyToMKC(NOKEY, DVK_TAB, MKC_Tab);

	InitKeyCodes();

	return true;
}

static void DoKeyCode0(int i, bool down)
{
	uint8_t key = KC2MKC[i];
	if (MKC_None != key) {
		fprintf(stderr, "%s() :: %c (%d) == %d\n",
			__FUNCTION__, (char) i, key, down);
		Keyboard_UpdateKeyMap2(key, down);
	}
}

static void DoKeyCode(int i, bool down)
{
	if ((i >= 0) && (i < 256)) {
		DoKeyCode0(i, down);
	}
}

/*
	TODO:

	Rethink keyboard input...
	Especially shift and capslock, the libnds keyboard
	is weird about those.
*/

static bool DS_Keystate_Menu = false;
static bool DS_Keystate_Shift = false;

static void DS_HandleKey(int32_t Key, bool Down)
{
	if (Key == NOKEY) {
		return;
	}

	switch (Key) {
		case DVK_UP:
			Keyboard_UpdateKeyMap2(MKC_Up, Down);
			break;

		case DVK_DOWN:
			Keyboard_UpdateKeyMap2(MKC_Down, Down);
			break;

		case DVK_LEFT:
			Keyboard_UpdateKeyMap2(MKC_Left, Down);
			break;

		case DVK_RIGHT:
			Keyboard_UpdateKeyMap2(MKC_Right, Down);
			break;

		case DVK_SHIFT:
			Keyboard_UpdateKeyMap2(MKC_formac_Shift, true);
			break;

		default:
			if (Key > 0) {
				DoKeyCode(Key, Down);
				Keyboard_UpdateKeyMap2(MKC_formac_Shift, false);
			}
			break;
	}
}

static void DS_HandleKeyboard(void)
{
	LastKeyboardKey = KeyboardKey;
	KeyboardKey = keyboardUpdate();

	if ((KeyboardKey == NOKEY) && (LastKeyboardKey != NOKEY)) {
		DS_HandleKey(LastKeyboardKey, false);
		LastKeyboardKey = NOKEY;
	} else {
		DS_HandleKey(KeyboardKey, true);
		LastKeyboardKey = KeyboardKey;
	}
}

/* --- time, date, location --- */

static uint32_t TrueEmulatedTime = 0;

#include "DATE2SEC.h"

#define TicksPerSecond 1000000
/* #define TicksPerSecond  1000 */

static bool HaveTimeDelta = false;
static uint32_t TimeDelta;

static uint32_t NewMacDateInSeconds;

static uint32_t LastTimeSec;
static uint32_t LastTimeUsec;

static void GetCurrentTicks(void)
{
	struct timeval t;

	gettimeofday(&t, NULL);

	/*
		HACKHACKHACK
	*/
	t.tv_usec = TimerBaseMSec + TIMER1_DATA;
	t.tv_usec = t.tv_usec * 1000;

	if (! HaveTimeDelta) {
		time_t Current_Time;
		struct tm *s;

		(void) time(&Current_Time);
		s = localtime(&Current_Time);
		TimeDelta = Date2MacSeconds(s->tm_sec, s->tm_min, s->tm_hour,
			s->tm_mday, 1 + s->tm_mon, 1900 + s->tm_year) - t.tv_sec;
#if 0 && AutoTimeZone /* how portable is this ? */
		CurMacDelta = ((uint32_t)(s->tm_gmtoff) & 0x00FFFFFF)
			| ((s->tm_isdst ? 0x80 : 0) << 24);
#endif
		HaveTimeDelta = true;
	}

	NewMacDateInSeconds = t.tv_sec + TimeDelta;
	LastTimeSec = (uint32_t)t.tv_sec;
	LastTimeUsec = (uint32_t)t.tv_usec;
}

/* #define MyInvTimeStep 16626 */ /* TicksPerSecond / 60.14742 */
#define MyInvTimeStep 17

static uint32_t NextTimeSec;
static uint32_t NextTimeUsec;

static void IncrNextTime(void)
{
	NextTimeUsec += MyInvTimeStep;
	if (NextTimeUsec >= TicksPerSecond) {
		NextTimeUsec -= TicksPerSecond;
		NextTimeSec += 1;
	}
}

static void InitNextTime(void)
{
	NextTimeSec = LastTimeSec;
	NextTimeUsec = LastTimeUsec;
	IncrNextTime();
}

static void StartUpTimeAdjust(void)
{
	GetCurrentTicks();
	InitNextTime();
}

static int32_t GetTimeDiff(void)
{
	return ((int32_t)(LastTimeSec - NextTimeSec)) * TicksPerSecond
		+ ((int32_t)(LastTimeUsec - NextTimeUsec));
}

static void UpdateTrueEmulatedTime(void)
{
	int32_t TimeDiff;

	GetCurrentTicks();

	TimeDiff = GetTimeDiff();
	if (TimeDiff >= 0) {
		if (TimeDiff > 4 * MyInvTimeStep) {
			/* emulation interrupted, forget it */
			++TrueEmulatedTime;
			InitNextTime();
		} else {
			do {
				++TrueEmulatedTime;
				IncrNextTime();
				TimeDiff -= TicksPerSecond;
			} while (TimeDiff >= 0);
		}
	} else if (TimeDiff < - 2 * MyInvTimeStep) {
		/* clock goofed if ever get here, reset */
		InitNextTime();
	}
}

static bool CheckDateTime(void)
{
	if (CurMacDateInSeconds != NewMacDateInSeconds) {
		CurMacDateInSeconds = NewMacDateInSeconds;
		return true;
	} else {
		return false;
	}
}

static bool InitLocationDat(void)
{
	GetCurrentTicks();
	CurMacDateInSeconds = NewMacDateInSeconds;

	return true;
}

/* --- basic dialogs --- */

static void CheckSavedMacMsg(void)
{
	if (nullptr != SavedBriefMsg) {
		char briefMsg0[ClStrMaxLength + 1];
		char longMsg0[ClStrMaxLength + 1];

		NativeStrFromCStr(briefMsg0, SavedBriefMsg);
		NativeStrFromCStr(longMsg0, SavedLongMsg);

		fprintf(stderr, "%s\n", briefMsg0);
		fprintf(stderr, "%s\n", longMsg0);

		SavedBriefMsg = nullptr;
	}
}

/* --- main window creation and disposal --- */

/*
	Screen_Init

	Mode 5 gives us 2 text backgrounds 0-1 (tiled mode) and
	2 extended rotation backgrounds 2-3. (linear fb)

	Also we need to map 2 banks of vram so we have enough space for
	our 512x512 surface.
*/
static bool Screen_Init(void)
{
	videoSetMode(MODE_5_2D);
	vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
	vramSetBankB(VRAM_B_MAIN_BG_0x06020000);

	Display_bg2_Main = bgInit(2, BgType_Bmp8, BgSize_B8_512x512, 0, 0);

	BG_PALETTE[0] = RGB15(31, 31, 31);
	BG_PALETTE[1] = RGB15(0, 0, 0);

	return true;
}

#if VarFullScreen
static void ToggleWantFullScreen(void)
{
	WantFullScreen = ! WantFullScreen;
}
#endif

/* --- SavedTasks --- */

static void LeaveSpeedStopped(void)
{
#if MySoundEnabled
	MySound_Start();
#endif

	StartUpTimeAdjust();
}

static void EnterSpeedStopped(void)
{
#if MySoundEnabled
	MySound_Stop();
#endif
}

static void CheckForSavedTasks(void)
{
	if (MyEvtQNeedRecover) {
		MyEvtQNeedRecover = false;

		/* attempt cleanup, MyEvtQNeedRecover may get set again */
		MyEvtQTryRecoverFromFull();
	}

	if (RequestMacOff) {
		RequestMacOff = false;
		if (AnyDiskInserted()) {
			MacMsgOverride(kStrQuitWarningTitle,
				kStrQuitWarningMessage);
		} else {
			ForceMacOff = true;
		}
	}

	if (ForceMacOff) {
		return;
	}

	if (CurSpeedStopped != SpeedStopped) {
		CurSpeedStopped = ! CurSpeedStopped;
		if (CurSpeedStopped) {
			EnterSpeedStopped();
		} else {
			LeaveSpeedStopped();
		}
	}

#if IncludeSonyNew
	if (vSonyNewDiskWanted) {
#if IncludeSonyNameNew
		if (vSonyNewDiskName != NotAPbuf) {
			uint8_t * NewDiskNameDat;
			if (MacRomanTextToNativePtr(vSonyNewDiskName, true,
				&NewDiskNameDat))
			{
				MakeNewDisk(vSonyNewDiskSize, (char *)NewDiskNameDat);
				free(NewDiskNameDat);
			}
			PbufDispose(vSonyNewDiskName);
			vSonyNewDiskName = NotAPbuf;
		} else
#endif
		{
			MakeNewDiskAtDefault(vSonyNewDiskSize);
		}
		vSonyNewDiskWanted = false;
			/* must be done after may have gotten disk */
	}
#endif

	if ((nullptr != SavedBriefMsg) & ! MacMsgDisplayed) {
		MacMsgDisplayOn();
	}

	if (NeedWholeScreenDraw) {
		NeedWholeScreenDraw = false;
		ScreenChangedAll();
	}

#if NeedRequestIthDisk
	if (0 != RequestIthDisk) {
		Sony_InsertIth(RequestIthDisk);
		RequestIthDisk = 0;
	}
#endif
}

/* --- main program flow --- */

 bool ExtraTimeNotOver(void)
{
	UpdateTrueEmulatedTime();
	return TrueEmulatedTime == OnTrueTime;
}

static void WaitForTheNextEvent(void)
{
}

static void CheckForSystemEvents(void)
{
	DS_HandleKeyboard();
}

void WaitForNextTick(void)
{
label_retry:
	CheckForSystemEvents();
	CheckForSavedTasks();
	if (ForceMacOff) {
		return;
	}

	if (CurSpeedStopped) {
		MyDrawChangesAndClear();
		WaitForTheNextEvent();
		goto label_retry;
	}

	if (ExtraTimeNotOver()) {
		int32_t TimeDiff = GetTimeDiff();
		if (TimeDiff < 0) {
			/*
				FIXME:

				Implement this?

				struct timespec rqt;
				struct timespec rmt;

				rqt.tv_sec = 0;
				rqt.tv_nsec = (- TimeDiff) * 1000;

				(void) nanosleep(&rqt, &rmt);
			*/
		}
		goto label_retry;
	}

	if (CheckDateTime()) {
#if MySoundEnabled
		MySound_SecondNotify();
#endif
#if EnableDemoMsg
		DemoModeSecondNotify();
#endif
	}

	CheckMouseState();

	OnTrueTime = TrueEmulatedTime;
}

/*
	DS_ScrollBackground:

	Positions the screen as to center it over the emulated cursor.
*/
static void DS_ScrollBackground(void)
{
	int ScrollX = 0;
	int ScrollY = 0;
	int Scale = 0;

	/*
		TODO:
		Lots of magic numbers here.
	*/
#if EnableMagnify
	if (WantMagnify) {
		ScrollX = ((int) CurMouseH) - (DS_ScreenWidth / 4);
		ScrollY = ((int) CurMouseV) - (DS_ScreenHeight / 4);
		Scale = 128;

		ScrollX = ScrollX > vMacScreenWidth - (DS_ScreenWidth / 2)
			? vMacScreenWidth - (DS_ScreenWidth / 2)
			: ScrollX;
		ScrollY = ScrollY > vMacScreenHeight - (DS_ScreenHeight / 2)
			? vMacScreenHeight - (DS_ScreenHeight / 2)
			: ScrollY;
	} else
#endif
	{
		ScrollX = ((int) CurMouseH) - (DS_ScreenWidth / 2);
		ScrollY = ((int) CurMouseV) - (DS_ScreenHeight / 2);
		Scale = 256;

		ScrollX = ScrollX > vMacScreenWidth - DS_ScreenWidth
			? vMacScreenWidth - DS_ScreenWidth
			: ScrollX;
		ScrollY = ScrollY > vMacScreenHeight - DS_ScreenHeight
			? vMacScreenHeight - DS_ScreenHeight
			: ScrollY;
	}

	ScrollX = ScrollX < 0 ? 0 : ScrollX;
	ScrollY = ScrollY < 0 ? 0 : ScrollY;

	if (Display_bg2_Main) {
		bgSetScale(Display_bg2_Main, Scale, Scale);
		bgSetScroll(Display_bg2_Main, ScrollX, ScrollY);
	}
}

/*
	DS_Timer1_IRQ

	Called when TIMER0_DATA overflows.
*/
static void DS_Timer1_IRQ(void)
{
	TimerBaseMSec += 65536;
}

/*
	DS_VBlank_IRQ

	Vertical blank interrupt callback.
*/
static void DS_VBlank_IRQ(void)
{
	scanKeys();

	KeysHeld = keysHeld();

	if (++VBlankCounter == 60) {
		VBlankCounter = 0;
	}

	/*
		TODO:
		Rewrite this at some point, I'm not sure I like it.
	*/
	if (0 != (KeysHeld & KEY_LEFT)) {
		--CursorX;
	} else if (0 != (KeysHeld & KEY_RIGHT)) {
		++CursorX;
	}

	if (0 != (KeysHeld & KEY_UP)) {
		--CursorY;
	} else if (0 != (KeysHeld & KEY_DOWN)) {
		++CursorY;
	}

	CursorX = CursorX < 0 ? 0 : CursorX;
	CursorX = CursorX > vMacScreenWidth ? vMacScreenWidth : CursorX;

	CursorY = CursorY < 0 ? 0 : CursorY;
	CursorY = CursorY > vMacScreenHeight ? vMacScreenHeight : CursorY;

	DS_ScrollBackground();
	bgUpdate();
}

/*
	DS_HBlank_IRQ

	Called at the start of the horizontal blanking period.
	This is here mainly as a simple performance test.
*/
static void DS_HBlank_IRQ(void)
{
	++HBlankCounter;
}

/*
	DS_SysInit

	Initializes DS specific system hardware and interrupts.
*/
static void DS_SysInit(void)
{
	defaultExceptionHandler();
	powerOn(POWER_ALL_2D);
	lcdMainOnTop();

	irqSet(IRQ_VBLANK, DS_VBlank_IRQ);
	irqSet(IRQ_HBLANK, DS_HBlank_IRQ);
	irqSet(IRQ_TIMER1, DS_Timer1_IRQ);

	irqEnable(IRQ_VBLANK);
	irqEnable(IRQ_HBLANK);
	irqEnable(IRQ_TIMER1);

	/*
		This sets up 2 timers as a milisecond counter.
		TIMER0_DATA Will overflow roughly every 1 msec into TIMER1_DATA.
		When TIMER1_DATA overflows an interrupt will be generated
		and DS_Timer1_IRQ will be called.
	*/
	TIMER0_DATA = 32768;

	TIMER0_CR = TIMER_DIV_1 | TIMER_ENABLE;



	TIMER1_DATA = 0;

	TIMER1_CR = TIMER_ENABLE | TIMER_CASCADE | TIMER_IRQ_REQ;

	/*
		Testing.
	*/
	consoleDemoInit();
	consoleDebugInit(DebugDevice_NOCASH);

	/*
		Use the default keyboard until I design a (good) UI...
	*/
	DSKeyboard = keyboardDemoInit();
	keyboardShow();

	/*
		Drop back to a read only filesystem embedded in the
		Mini vMac binary if we cannot open a media device.
	*/
	if (! fatInitDefault()) {
		nitroFSInit();
	}
}

/*
	DS_ClearVRAM:

	Make sure all of the video memory and background/object palettes
	are zeroed out just in-case the loader doesn't do it for us.
*/
static void DS_ClearVRAM(void)
{
	vramSetPrimaryBanks(VRAM_A_LCD, VRAM_B_LCD, VRAM_C_LCD, VRAM_D_LCD);

	dmaFillWords(0, (void *) VRAM_A, 128 * 1024 * 4);
	dmaFillWords(0, (void *) BG_PALETTE, 256 * 2);
	dmaFillWords(0, (void *) BG_PALETTE_SUB, 256 * 2);
	dmaFillWords(0, (void *) SPRITE_PALETTE, 256 * 2);
	dmaFillWords(0, (void *) SPRITE_PALETTE_SUB, 256 * 2);

	vramDefault();
}

/* --- platform independent code can be thought of as going here --- */

#include "PROGMAIN.h"

static void ReserveAllocAll(void)
{
#if dbglog_HAVE
	dbglog_ReserveAlloc();
#endif
	ReserveAllocOneBlock(&ROM, kROM_Size, 5, false);

	ReserveAllocOneBlock(&screencomparebuff,
		vMacScreenNumBytes, 5, true);
#if UseControlKeys
	ReserveAllocOneBlock(&CntrlDisplayBuff,
		vMacScreenNumBytes, 5, false);
#endif

#if MySoundEnabled
	ReserveAllocOneBlock((uint8_t * *)&TheSoundBuffer,
		dbhBufferSize, 5, false);
#endif

	EmulationReserveAlloc();
}

static bool AllocMyMemory(void)
{
	uint32_t n;
	bool IsOk = false;

	ReserveAllocOffset = 0;
	ReserveAllocBigBlock = nullptr;
	ReserveAllocAll();
	n = ReserveAllocOffset;
	ReserveAllocBigBlock = (uint8_t *)calloc(1, n);
	if (NULL == ReserveAllocBigBlock) {
		MacMsg(kStrOutOfMemTitle, kStrOutOfMemMessage, true);
	} else {
		ReserveAllocOffset = 0;
		ReserveAllocAll();
		if (n != ReserveAllocOffset) {
			/* oops, program error */
		} else {
			IsOk = true;
		}
	}

	return IsOk;
}

static void UnallocMyMemory(void)
{
	if (nullptr != ReserveAllocBigBlock) {
		free((char *)ReserveAllocBigBlock);
	}
}

static void ZapOSGLUVars(void)
{
	InitDrives();
	DS_ClearVRAM();
}

static bool InitOSGLU(void)
{
	DS_SysInit();

	if (AllocMyMemory())
#if dbglog_HAVE
	if (dbglog_open())
#endif
	if (LoadMacRom())
	if (LoadInitialImages())
	if (InitLocationDat())
#if MySoundEnabled
	if (MySound_Init())
#endif
	if (Screen_Init())
	if (KC2MKCInit())
	if (WaitForRom())
	{
		return true;
	}

	return false;
}

static void UnInitOSGLU(void)
{
	if (MacMsgDisplayed) {
		MacMsgDisplayOff();
	}

#if MySoundEnabled
	MySound_Stop();
#endif
#if MySoundEnabled
	MySound_UnInit();
#endif

	UnInitDrives();

#if dbglog_HAVE
	dbglog_close();
#endif

	UnallocMyMemory();
	CheckSavedMacMsg();
}

int main(int argc, char **argv)
{
	ZapOSGLUVars();

	if (InitOSGLU()) {
		iprintf("Entering ProgramMain...\n");

		ProgramMain();

		iprintf("Leaving ProgramMain...\n");
	}

	UnInitOSGLU();

	/*
		On some homebrew launchers this could return to
		the menu by default.
	*/
	exit(1);

	while (1) {
		swiWaitForVBlank();
	}

	return 0;
}

#endif /* WantOSGLUNDS */
