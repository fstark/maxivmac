/*
	COMmon code for Operating System GLUe — implementation
*/

#include "platform/common/osglu_ui.h"
#include "platform/common/osglu_ud.h"
#include "platform/common/osglu_common.h"

#include <cstring>

#include "platform/emulator_shell.h"

/* --- backend-provided debug log primitives (extern) --- */

extern bool dbglog_open0(const char *appParent);
extern void dbglog_write0(char *s, uint32_t L);
extern void dbglog_close0();

/* --- global variables --- */

uint8_t *g_rom = nullptr;

uint32_t g_sonyWritableMask = 0;
uint32_t g_sonyInsertedMask = 0;

bool g_sonyRawMode = false;

bool g_sonyNewDiskWanted = false;
uint32_t g_sonyNewDiskSize;

PbufIndex g_sonyNewDiskName = NOT_A_PBUF;

uint32_t g_curMacDateInSeconds = 0;
uint32_t g_curMacLatitude = 0;
uint32_t g_curMacLongitude = 0;
uint32_t g_curMacDelta = 0;

bool g_requestMacOff = false;

bool g_forceMacOff = false;

bool g_wantMacInterrupt = false;

bool g_wantMacReset = false;

uint8_t g_speedValue = 4;

bool g_SkipThrottle = false;

bool g_wantNotAutoSlow = false;

uint16_t g_curMouseV = 0;
uint16_t g_curMouseH = 0;

bool g_haveMouseMotion = false;

uint32_t g_quietTime = 0;
uint32_t g_quietSubTicks = 0;

#if EmLocalTalk

uint8_t g_ltNodeHint = 0;

#if LT_MayHaveEcho
bool g_certainlyNotMyPacket = false;
#endif

uint8_t *g_ltTxBuffer = nullptr;

/* Transmit state */
uint16_t g_ltTxBuffSz = 0;

/* Receive state */
uint8_t *g_ltRxBuffer = nullptr;
uint32_t g_ltRxBuffSz = 0;

#endif

uint32_t g_onTrueTime = 0;

/* --- Pbuf support --- */

uint32_t g_pbufAllocatedMask;
uint32_t PbufSize[NumPbufs];

bool FirstFreePbuf(PbufIndex *r)
{
	PbufIndex i;

	for (i = 0; i < NumPbufs; ++i)
	{
		if (!PbufIsAllocated(i))
		{
			*r = i;
			return true;
		}
	}
	return false;
}

void PbufNewNotify(PbufIndex pbufNo, uint32_t count)
{
	PbufSize[pbufNo] = count;
	g_pbufAllocatedMask |= ((uint32_t)1 << pbufNo);
}

void PbufDisposeNotify(PbufIndex pbufNo)
{
	g_pbufAllocatedMask &= ~((uint32_t)1 << pbufNo);
}

tMacErr CheckPbuf(PbufIndex pbufNo)
{
	tMacErr result;

	if (pbufNo >= NumPbufs)
	{
		result = tMacErr::nsDrvErr;
	}
	else if (!PbufIsAllocated(pbufNo))
	{
		result = tMacErr::offLinErr;
	}
	else
	{
		result = tMacErr::noErr;
	}

	return result;
}

tMacErr PbufGetSize(PbufIndex pbufNo, uint32_t *count)
{
	tMacErr result = CheckPbuf(pbufNo);

	if (tMacErr::noErr == result)
	{
		*count = PbufSize[pbufNo];
	}

	return result;
}

/* --- Disk support --- */

bool FirstFreeDisk(DriveIndex *driveNo)
{
	DriveIndex i;

	for (i = 0; i < NumDrives; ++i)
	{
		if (!vSonyIsInserted(i))
		{
			if (nullptr != driveNo)
			{
				*driveNo = i;
			}
			return true;
		}
	}
	return false;
}

bool AnyDiskInserted()
{
	return 0 != g_sonyInsertedMask;
}

void DiskRevokeWritable(DriveIndex driveNo)
{
	g_sonyWritableMask &= ~((uint32_t)1 << driveNo);
}

void DiskInsertNotify(DriveIndex driveNo, bool locked)
{
	fprintf(stderr, "DISK_INSERT drive=%d locked=%d\n", (int)driveNo, (int)locked);
	g_sonyInsertedMask |= ((uint32_t)1 << driveNo);
	if (!locked)
	{
		g_sonyWritableMask |= ((uint32_t)1 << driveNo);
	}

	QuietEnds();
}

void DiskEjectedNotify(DriveIndex driveNo)
{
	g_sonyWritableMask &= ~((uint32_t)1 << driveNo);
	g_sonyInsertedMask &= ~((uint32_t)1 << driveNo);
}

/* --- Screen change detection --- */

/* Compare current screen buffer against shadow buffer.
   If anything changed (or the color palette was modified),
   snapshot the current buffer and set g_screenChanged. */
static bool ScreenFindChanges(uint8_t *screencurrentbuff)
{
	uint32_t bufSize;

	if (g_colorMappingChanged)
	{
		g_colorMappingChanged = false;
		g_colorTransValid = false;
		bufSize = (uint32_t)vMacScreenHeight * vMacScreenByteWidth;
		MoveBytes(screencurrentbuff, g_screenCompareBuff, bufSize);
		return true;
	}

	if (vMacScreenDepth != 0 && g_useColorMode)
	{
		bufSize = (uint32_t)vMacScreenHeight * vMacScreenByteWidth;
	}
	else
	{
		bufSize = (uint32_t)vMacScreenHeight * vMacScreenMonoByteWidth;
	}

	if (0 != memcmp(screencurrentbuff, g_screenCompareBuff, bufSize))
	{
		MoveBytes(screencurrentbuff, g_screenCompareBuff, bufSize);
		return true;
	}

	return false;
}

/* --- Screen frame output --- */

void ScreenChangedAll()
{
	g_screenChanged = true;
}

/* Detect whether the screen changed since last frame and
   flag it for the platform layer to redraw. */
void Screen_OutputFrame(uint8_t *screencurrentbuff)
{
	if (ScreenFindChanges(screencurrentbuff))
	{
		g_screenChanged = true;
		QuietEnds();
	}
}

/* --- Full screen view support --- */

uint16_t g_viewHSize;
uint16_t g_viewVSize;
uint16_t g_viewHStart = 0;
uint16_t g_viewVStart = 0;
int16_t g_savedMouseH;
int16_t g_savedMouseV;

void AutoScrollScreen()
{
	int16_t Shift;
	int16_t Limit;

	if (vMacScreenWidth != g_viewHSize)
	{
		Shift = 0;
		Limit = g_viewHStart + (g_viewHSize / 16);
		if (g_curMouseH < Limit)
		{
			Shift = (Limit - g_curMouseH + 1) & (~1);
			Limit = g_viewHStart;
			if (Shift >= Limit)
			{
				Shift = Limit;
			}
			Shift = -Shift;
		}
		else
		{
			Limit = g_viewHStart + g_viewHSize - (g_viewHSize / 16);
			if (g_curMouseH > Limit)
			{
				Shift = (g_curMouseH - Limit + 1) & (~1);
				Limit = vMacScreenWidth - g_viewHSize - g_viewHStart;
				if (Shift >= Limit)
				{
					Shift = Limit;
				}
			}
		}

		if (Shift != 0)
		{
			g_viewHStart += Shift;
			g_savedMouseH += Shift;
			ScreenChangedAll();
		}
	}

	if (vMacScreenHeight != g_viewVSize)
	{
		Shift = 0;
		Limit = g_viewVStart + (g_viewVSize / 16);
		if (g_curMouseV < Limit)
		{
			Shift = (Limit - g_curMouseV + 1) & (~1);
			Limit = g_viewVStart;
			if (Shift >= Limit)
			{
				Shift = Limit;
			}
			Shift = -Shift;
		}
		else
		{
			Limit = g_viewVStart + g_viewVSize - (g_viewVSize / 16);
			if (g_curMouseV > Limit)
			{
				Shift = (g_curMouseV - Limit + 1) & (~1);
				Limit = vMacScreenHeight - g_viewVSize - g_viewVStart;
				if (Shift >= Limit)
				{
					Shift = Limit;
				}
			}
		}

		if (Shift != 0)
		{
			g_viewVStart += Shift;
			g_savedMouseV += Shift;
			ScreenChangedAll();
		}
	}
}

/* --- Memory allocation --- */

static void SetLongs(uint32_t *p, long n)
{
	long i;

	for (i = n; --i >= 0;)
	{
		*p++ = (uint32_t)-1;
	}
}

bool AllocBlock(uint8_t **p, uint32_t n, bool fillOnes)
{
	*p = static_cast<uint8_t *>(calloc(1, n));
	if (*p == nullptr)
	{
		return false;
	}
	if (fillOnes)
	{
		SetLongs(reinterpret_cast<uint32_t *>(*p), n / 4);
	}
	return true;
}

/* --- sending debugging info to file --- */


#ifndef dbglog_buflnsz

bool dbglog_ReserveAlloc()
{
	/* nothing to do in unbuffered mode */
	return true;
}

/* dbglog_close/open/write macros already in osglu_common.h */

#else

#define dbglog_bufsz POW_OF_2(dbglog_buflnsz)
static uint32_t dbglog_bufpos = 0;

static char *dbglog_bufp = nullptr;

bool dbglog_ReserveAlloc()
{
	return AllocBlock((uint8_t **)&dbglog_bufp, dbglog_bufsz, false);
}

#define dbglog_open dbglog_open0

static void dbglog_close()
{
	uint32_t n = MOD_POW2(dbglog_bufpos, dbglog_buflnsz);
	if (n != 0)
	{
		dbglog_write0(dbglog_bufp, n);
	}

	dbglog_close0();
}

static void dbglog_write(char *p, uint32_t L)
{
	uint32_t r;
	uint32_t bufposmod;
	uint32_t curbufdiv;
	uint32_t newbufpos = dbglog_bufpos + L;
	uint32_t newbufdiv = FLOOR_DIV_POW2(newbufpos, dbglog_buflnsz);

	for (;;)
	{
		curbufdiv = FLOOR_DIV_POW2(dbglog_bufpos, dbglog_buflnsz);
		bufposmod = MOD_POW2(dbglog_bufpos, dbglog_buflnsz);
		if (newbufdiv == curbufdiv)
		{
			break;
		}
		r = dbglog_bufsz - bufposmod;
		MoveBytes(reinterpret_cast<uint8_t *>(p),
				  reinterpret_cast<uint8_t *>(dbglog_bufp + bufposmod), r);
		dbglog_write0(dbglog_bufp, dbglog_bufsz);
		L -= r;
		p += r;
		dbglog_bufpos += r;
	}
	MoveBytes(reinterpret_cast<uint8_t *>(p), reinterpret_cast<uint8_t *>(dbglog_bufp + bufposmod),
			  L);
	dbglog_bufpos = newbufpos;
}

#endif /* dbglog_buflnsz defined */

static uint32_t CStrLength(char *s)
{
	char *p = s;

	while (*p++ != 0)
	{
	}
	return p - s - 1;
}

void dbglog_writeCStr(char *s)
{
	dbglog_write(s, CStrLength(s));
}

void dbglog_writeReturn()
{
	dbglog_writeCStr("\n");
}

void dbglog_writeHex(uint32_t x)
{
	uint8_t v;
	char s[16];
	char *p = s + 16;
	uint32_t n = 0;

	do
	{
		v = x & 0x0F;
		if (v < 10)
		{
			*--p = '0' + v;
		}
		else
		{
			*--p = 'A' + v - 10;
		}
		x >>= 4;
		++n;
	} while (x != 0);

	dbglog_write(p, n);
}

void dbglog_writeNum(uint32_t x)
{
	uint32_t newx;
	char s[16];
	char *p = s + 16;
	uint32_t n = 0;

	do
	{
		newx = x / (uint32_t)10;
		*--p = '0' + (x - newx * 10);
		x = newx;
		++n;
	} while (x != 0);

	dbglog_write(p, n);
}

void dbglog_writeMacChar(uint8_t x)
{
	char s;

	if ((x > 32) && (x < 127))
	{
		s = x;
	}
	else
	{
		s = '?';
	}

	dbglog_write(&s, 1);
}

static void dbglog_writeSpace()
{
	dbglog_writeCStr(" ");
}

void dbglog_writeln(char *s)
{
	dbglog_writeCStr(s);
	dbglog_writeReturn();
}

void dbglog_writelnHex(char *s, uint32_t x)
{
	dbglog_writeCStr(s);
	dbglog_writeSpace();
	dbglog_writeHex(x);
	dbglog_writeReturn();
}

void dbglog_writelnNum(char *s, int32_t v)
{
	dbglog_writeCStr(s);
	dbglog_writeSpace();
	dbglog_writeNum(v);
	dbglog_writeReturn();
}


/* --- event queue --- */

EvtQEl EvtQA[MyEvtQSz];
uint16_t EvtQIn = 0;
uint16_t EvtQOut = 0;

EvtQEl *EvtQOutP()
{
	EvtQEl *p = nullptr;
	if (EvtQIn != EvtQOut)
	{
		p = &EvtQA[EvtQOut & MyEvtQIMask];
	}
	return p;
}

void EvtQOutDone()
{
	++EvtQOut;
}

bool EvtQNeedRecover = false;

EvtQEl *EvtQElPreviousIn()
{
	EvtQEl *p = nullptr;
	if (EvtQIn - EvtQOut != 0)
	{
		p = &EvtQA[(EvtQIn - 1) & MyEvtQIMask];
	}

	return p;
}

EvtQEl *EvtQElAlloc()
{
	EvtQEl *p = nullptr;
	if (EvtQIn - EvtQOut >= MyEvtQSz)
	{
		EvtQNeedRecover = true;
	}
	else
	{
		p = &EvtQA[EvtQIn & MyEvtQIMask];

		++EvtQIn;
	}

	return p;
}

/* --- keyboard and mouse --- */

uint32_t theKeys[4];

void Keyboard_UpdateKeyMap(uint8_t key, bool down)
{
	uint8_t k = key & 127;
	uint8_t bit = 1 << (k & 7);
	uint8_t *kp = reinterpret_cast<uint8_t *>(theKeys);
	uint8_t *kpi = &kp[k / 8];
	bool CurDown = ((*kpi & bit) != 0);
	if (CurDown != down)
	{
		EvtQEl *p = EvtQElAlloc();
		if (nullptr != p)
		{
			p->kind = EvtQElKind::Key;
			p->u.press.key = k;
			p->u.press.down = down;

			if (down)
			{
				*kpi |= bit;
			}
			else
			{
				*kpi &= ~bit;
			}
		}

		QuietEnds();
	}
}

bool g_mouseButtonState = false;

void MyMouseButtonSet(bool down)
{
	if (g_mouseButtonState != down)
	{
		EvtQEl *p = EvtQElAlloc();
		if (nullptr != p)
		{
			p->kind = EvtQElKind::MouseButton;
			p->u.press.down = down;

			g_mouseButtonState = down;
		}

		QuietEnds();
	}
}

void MyMousePositionSetDelta(uint16_t dh, uint16_t dv)
{
	if ((dh != 0) || (dv != 0))
	{
		EvtQEl *p = EvtQElPreviousIn();
		if ((nullptr != p) && (EvtQElKind::MouseDelta == p->kind))
		{
			p->u.pos.h += dh;
			p->u.pos.v += dv;
		}
		else
		{
			p = EvtQElAlloc();
			if (nullptr != p)
			{
				p->kind = EvtQElKind::MouseDelta;
				p->u.pos.h = dh;
				p->u.pos.v = dv;
			}
		}

		QuietEnds();
	}
}

void MyMousePositionSet(uint16_t h, uint16_t v)
{
	if ((h != g_curMouseH) || (v != g_curMouseV))
	{
		EvtQEl *p = EvtQElPreviousIn();
		if ((nullptr == p) || (EvtQElKind::MousePos != p->kind))
		{
			p = EvtQElAlloc();
		}
		if (nullptr != p)
		{
			p->kind = EvtQElKind::MousePos;
			p->u.pos.h = h;
			p->u.pos.v = v;
		}

		QuietEnds();
	}
}

void InitKeyCodes()
{
	theKeys[0] = 0;
	theKeys[1] = 0;
	theKeys[2] = 0;
	theKeys[3] = 0;
}

void DisconnectKeyCodes(uint32_t keepMask)
{
	int j;
	int b;
	int key;
	uint32_t m;

	for (j = 0; j < 16; ++j)
	{
		uint8_t k1 = (reinterpret_cast<uint8_t *>(theKeys))[j];
		if (0 != k1)
		{
			uint8_t bit = 1;
			for (b = 0; b < 8; ++b)
			{
				if (0 != (k1 & bit))
				{
					key = j * 8 + b;
					switch (key)
					{
						case MKC_Control:
							m = kKeepMaskControl;
							break;
						case MKC_CapsLock:
							m = kKeepMaskCapsLock;
							break;
						case MKC_Command:
							m = kKeepMaskCommand;
							break;
						case MKC_Option:
							m = kKeepMaskOption;
							break;
						case MKC_Shift:
							m = kKeepMaskShift;
							break;
						default:
							m = 0;
							break;
					}
					if (0 == (keepMask & m))
					{
						Keyboard_UpdateKeyMap(key, false);
					}
				}
				bit <<= 1;
			}
		}
	}
}

void EvtQTryRecoverFromFull()
{
	MyMouseButtonSet(false);
	DisconnectKeyCodes(0);
}

/* --- MacMsg --- */

void MacMsg(const char *briefMsg, const char *longMsg, bool fatal)
{
	if (g_shell)
	{
		g_shell->queueMessage(briefMsg, longMsg, fatal);
	}
}

#if WantAbnormalReports
void WarnMsgAbnormalID(uint16_t id)
{
	/* Abnormal IDs are already logged via dbglog in DoReportAbnormalID.
	   No user-facing overlay needed. */
	(void)id;
}
#endif

/* --- LocalTalk entropy pool --- */

#if EmLocalTalk

uint32_t e_p[2] = {0, 0};

static void EntropyPoolStir()
{
	uint32_t t0a = e_p[0];
	uint32_t t1a = e_p[1];

	uint32_t t0b = t0a * 0xAE3CC725 + 0xD860D735;
	uint32_t t1b = t1a * 0x9FE72885 + 0x641AD0A9;

	uint32_t t0c = (t0b << 8) + (t1b >> 24);
	uint32_t t1c = (t1b << 8) + (t0b >> 24);

	e_p[0] = t0c;
	e_p[1] = t1c;
}

static void EntropyPoolAddByte(uint8_t v)
{
	e_p[0] += v;
	e_p[1] += v;

	EntropyPoolStir();
}

void EntropyPoolAddPtr(uint8_t *p, uint32_t n)
{
	uint32_t i;

	for (i = n + 1; 0 != --i;)
	{
		EntropyPoolAddByte(*p++);
	}

	dbglog_writeCStr("ep: ");
	dbglog_writeHex(e_p[0]);
	dbglog_writeCStr(" ");
	dbglog_writeHex(e_p[1]);
	dbglog_writeReturn();
}


uint32_t g_ltMyStamp = 0;

void LT_PickStampNodeHint()
{
	g_ltMyStamp = e_p[0];

	dbglog_writelnNum("LT_MyStamp ", g_ltMyStamp);

	{
		int i = 8 + 1;

		for (;;)
		{
			/* user node should be in 1-127 */

			g_ltNodeHint = e_p[1] & 0x7F;

			dbglog_writelnNum("LT_NodeHint ", g_ltNodeHint);

			if (0 != g_ltNodeHint)
			{
				break;
			}
			if (0 == --i)
			{
				/* just maybe, randomness is broken */
				g_ltNodeHint = 4;
				break;
			}
			EntropyPoolStir();
		}
	}
}

#endif /* EmLocalTalk */
