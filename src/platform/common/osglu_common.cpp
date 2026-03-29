/*
	COMmon code for Operating System GLUe — implementation
*/

#include "platform/common/osglu_ui.h"
#include "platform/common/osglu_ud.h"
#include "platform/common/osglu_common.h"

/* --- backend-provided debug log primitives (extern) --- */

#if dbglog_HAVE
extern bool dbglog_open0();
extern void dbglog_write0(char *s, uint32_t L);
extern void dbglog_close0();
#endif

/* --- global variables --- */

uint8_t * g_rom = nullptr;
bool g_romLoaded = false;

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

/* Runtime screen dimensions — initialized from MachineConfig */
uint16_t g_screenWidth  = 640;
uint16_t g_screenHeight = 480;
uint8_t  g_screenDepth  = 3;

bool g_useColorMode = false;
bool g_colorModeWorks = false;

bool g_colorMappingChanged = false;

uint16_t CLUT_reds[CLUT_size];
uint16_t CLUT_greens[CLUT_size];
uint16_t CLUT_blues[CLUT_size];

bool g_requestMacOff = false;

bool g_forceMacOff = false;

bool g_wantMacInterrupt = false;

bool g_wantMacReset = false;

uint8_t g_speedValue = 4;

bool g_SkipThrottle = false;

bool g_wantNotAutoSlow = false;

uint16_t g_curMouseV = 0;
uint16_t g_curMouseH = 0;

#if EnableFSMouseMotion
bool g_haveMouseMotion = false;
#endif

uint32_t g_quietTime = 0;
uint32_t g_quietSubTicks = 0;

#if EmLocalTalk

uint8_t g_ltNodeHint = 0;

#if LT_MayHaveEcho
bool g_certainlyNotMyPacket = false;
#endif

uint8_t * g_ltTxBuffer = nullptr;

/* Transmit state */
uint16_t g_ltTxBuffSz = 0;

/* Receive state */
uint8_t * g_ltRxBuffer = nullptr;
uint32_t g_ltRxBuffSz = 0;

#endif

bool g_emVideoDisable = false;
int8_t g_emLagTime = 0;

uint32_t g_onTrueTime = 0;

/* --- Pbuf support --- */

uint32_t g_pbufAllocatedMask;
uint32_t PbufSize[NumPbufs];

bool FirstFreePbuf(PbufIndex *r)
{
	PbufIndex i;

	for (i = 0; i < NumPbufs; ++i) {
		if (! PbufIsAllocated(i)) {
			*r = i;
			return true;
		}
	}
	return false;
}

void PbufNewNotify(PbufIndex Pbuf_No, uint32_t count)
{
	PbufSize[Pbuf_No] = count;
	g_pbufAllocatedMask |= ((uint32_t)1 << Pbuf_No);
}

void PbufDisposeNotify(PbufIndex Pbuf_No)
{
	g_pbufAllocatedMask &= ~ ((uint32_t)1 << Pbuf_No);
}

tMacErr CheckPbuf(PbufIndex Pbuf_No)
{
	tMacErr result;

	if (Pbuf_No >= NumPbufs) {
		result = tMacErr::nsDrvErr;
	} else if (! PbufIsAllocated(Pbuf_No)) {
		result = tMacErr::offLinErr;
	} else {
		result = tMacErr::noErr;
	}

	return result;
}

tMacErr PbufGetSize(PbufIndex Pbuf_No, uint32_t *Count)
{
	tMacErr result = CheckPbuf(Pbuf_No);

	if (tMacErr::noErr == result) {
		*Count = PbufSize[Pbuf_No];
	}

	return result;
}

/* --- Disk support --- */

bool FirstFreeDisk(DriveIndex *Drive_No)
{
	DriveIndex i;

	for (i = 0; i < NumDrives; ++i) {
		if (! vSonyIsInserted(i)) {
			if (nullptr != Drive_No) {
				*Drive_No = i;
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

void DiskRevokeWritable(DriveIndex Drive_No)
{
	g_sonyWritableMask &= ~ ((uint32_t)1 << Drive_No);
}

void DiskInsertNotify(DriveIndex Drive_No, bool locked)
{
	fprintf(stderr, "DISK_INSERT drive=%d locked=%d\n", (int)Drive_No, (int)locked);
	g_sonyInsertedMask |= ((uint32_t)1 << Drive_No);
	if (! locked) {
		g_sonyWritableMask |= ((uint32_t)1 << Drive_No);
	}

	QuietEnds();
}

void DiskEjectedNotify(DriveIndex Drive_No)
{
	g_sonyWritableMask &= ~ ((uint32_t)1 << Drive_No);
	g_sonyInsertedMask &= ~ ((uint32_t)1 << Drive_No);
}

/* --- Screen change detection --- */

/*
	block type - for operating on multiple uint8_t elements
		at a time.
*/

#if LittleEndianUnaligned || BigEndianUnaligned

#define uibb uint32_t
#define uibr uint32_t
#define ln2uiblockn 2

#else

#define uibb uint8_t
#define uibr uint8_t
#define ln2uiblockn 0

#endif

#define uiblockn (1 << ln2uiblockn)
#define ln2uiblockbitsn (3 + ln2uiblockn)
#define uiblockbitsn (8 * uiblockn)

static bool FindFirstChangeInLVecs(uibb *ptr1, uibb *ptr2,
					uint32_t L, uint32_t *j)
{
	uibb *p1 = ptr1;
	uibb *p2 = ptr2;
	uint32_t i;

	for (i = L; i != 0; --i) {
		if (*p1++ != *p2++) {
			--p1;
			*j = p1 - ptr1;
			return true;
		}
	}
	return false;
}

static void FindLastChangeInLVecs(uibb *ptr1, uibb *ptr2,
					uint32_t L, uint32_t *j)
{
	uibb *p1 = ptr1 + L;
	uibb *p2 = ptr2 + L;

	while (*--p1 == *--p2) {
	}
	*j = p1 - ptr1;
}

/* Scan left/right within changed rows to find the narrowest
   bounding box of differing pixels per scanline. */
static void FindLeftRightChangeInLMat(uibb *ptr1, uibb *ptr2,
	uint32_t width, uint32_t top, uint32_t bottom,
	uint32_t *LeftMin0, uibr *LeftMask0,
	uint32_t *RightMax0, uibr *RightMask0)
{
	uint32_t i;
	uint32_t j;
	uibb *p1;
	uibb *p2;
	uibr x;
	uint32_t offset = top * width;
	uibb *p10 = (uibb *)ptr1 + offset;
	uibb *p20 = (uibb *)ptr2 + offset;
	uint32_t LeftMin = *LeftMin0;
	uint32_t RightMax = *RightMax0;
	uibr LeftMask = 0;
	uibr RightMask = 0;
	for (i = top; i < bottom; ++i) {
		p1 = p10;
		p2 = p20;
		for (j = 0; j < LeftMin; ++j) {
			x = *p1++ ^ *p2++;
			if (0 != x) {
				LeftMin = j;
				LeftMask = x;
				goto Label_3;
			}
		}
		LeftMask |= (*p1 ^ *p2);
Label_3:
		p1 = p10 + RightMax;
		p2 = p20 + RightMax;
		RightMask |= (*p1++ ^ *p2++);
		for (j = RightMax + 1; j < width; ++j) {
			x = *p1++ ^ *p2++;
			if (0 != x) {
				RightMax = j;
				RightMask = x;
			}
		}

		p10 += width;
		p20 += width;
	}
	*LeftMin0 = LeftMin;
	*RightMax0 = RightMax;
	*LeftMask0 = LeftMask;
	*RightMask0 = RightMask;
}

uint8_t * g_screenCompareBuff = nullptr;

static uint32_t s_nextDrawRow = 0;


#if BigEndianUnaligned

#define FlipCheckMonoBits (uiblockbitsn - 1)

#else

#define FlipCheckMonoBits 7

#endif

#define FlipCheckBits (FlipCheckMonoBits >> vMacScreenDepth)

#if WantColorTransValid
bool g_colorTransValid = false;
#endif

/* Compare current and previous screen buffers row-by-row to
   find the dirty rectangle, limiting work per tick for smooth
   animation.  Copies changed rows to the shadow buffer. */
static bool ScreenFindChanges(uint8_t * screencurrentbuff,
	int8_t TimeAdjust, int16_t *top, int16_t *left, int16_t *bottom, int16_t *right)
{
	uint32_t j0;
	uint32_t j1;
	uint32_t j0h;
	uint32_t j1h;
	uint32_t j0v;
	uint32_t j1v;
	uint32_t copysize;
	uint32_t copyoffset;
	uint32_t copyrows;
	uint32_t LimitDrawRow;
	uint32_t MaxRowsDrawnPerTick;
	uint32_t LeftMin;
	uint32_t RightMax;
	uibr LeftMask;
	uibr RightMask;
	int j;

	if (TimeAdjust < 4) {
		MaxRowsDrawnPerTick = vMacScreenHeight;
	} else if (TimeAdjust < 6) {
		MaxRowsDrawnPerTick = vMacScreenHeight / 2;
	} else {
		MaxRowsDrawnPerTick = vMacScreenHeight / 4;
	}

	if (vMacScreenDepth != 0 && g_useColorMode) {
		if (g_colorMappingChanged) {
			g_colorMappingChanged = false;
			j0h = 0;
			j1h = vMacScreenWidth;
			j0v = 0;
			j1v = vMacScreenHeight;
#if WantColorTransValid
			g_colorTransValid = false;
#endif
		} else {
			if (! FindFirstChangeInLVecs(
				(uibb *)screencurrentbuff
					+ s_nextDrawRow * (vMacScreenBitWidth / uiblockbitsn),
				(uibb *)g_screenCompareBuff
					+ s_nextDrawRow * (vMacScreenBitWidth / uiblockbitsn),
				((uint32_t)(vMacScreenHeight - s_nextDrawRow)
					* (uint32_t)vMacScreenBitWidth) / uiblockbitsn,
				&j0))
			{
				s_nextDrawRow = 0;
				return false;
			}
			j0v = j0 / (vMacScreenBitWidth / uiblockbitsn);
			j0h = j0 - j0v * (vMacScreenBitWidth / uiblockbitsn);
			j0v += s_nextDrawRow;
			LimitDrawRow = j0v + MaxRowsDrawnPerTick;
			if (LimitDrawRow >= vMacScreenHeight) {
				LimitDrawRow = vMacScreenHeight;
				s_nextDrawRow = 0;
			} else {
				s_nextDrawRow = LimitDrawRow;
			}
			FindLastChangeInLVecs((uibb *)screencurrentbuff,
				(uibb *)g_screenCompareBuff,
				((uint32_t)LimitDrawRow
					* (uint32_t)vMacScreenBitWidth) / uiblockbitsn,
				&j1);
			j1v = j1 / (vMacScreenBitWidth / uiblockbitsn);
			j1h = j1 - j1v * (vMacScreenBitWidth / uiblockbitsn);
			j1v++;

			if (j0h < j1h) {
				LeftMin = j0h;
				RightMax = j1h;
			} else {
				LeftMin = j1h;
				RightMax = j0h;
			}

			FindLeftRightChangeInLMat((uibb *)screencurrentbuff,
				(uibb *)g_screenCompareBuff,
				(vMacScreenBitWidth / uiblockbitsn),
				j0v, j1v, &LeftMin, &LeftMask, &RightMax, &RightMask);

			if (vMacScreenDepth > ln2uiblockbitsn) {
				j0h =  (LeftMin >> (vMacScreenDepth - ln2uiblockbitsn));
			} else if (ln2uiblockbitsn > vMacScreenDepth) {
				for (j = 0; j < (1 << (ln2uiblockbitsn - vMacScreenDepth));
					++j)
				{
					if (0 != (LeftMask
						& (((((uibr)1) << (1 << vMacScreenDepth)) - 1)
							<< ((j ^ FlipCheckBits) << vMacScreenDepth))))
					{
						goto Label_1c;
					}
				}
Label_1c:
				j0h =  (LeftMin << (ln2uiblockbitsn - vMacScreenDepth)) + j;
			} else {
				j0h =  LeftMin;
			}

			if (vMacScreenDepth > ln2uiblockbitsn) {
				j1h = (RightMax >> (vMacScreenDepth - ln2uiblockbitsn)) + 1;
			} else if (ln2uiblockbitsn > vMacScreenDepth) {
				for (j = (uiblockbitsn >> vMacScreenDepth); --j >= 0; ) {
					if (0 != (RightMask
						& (((((uibr)1) << (1 << vMacScreenDepth)) - 1)
							<< ((j ^ FlipCheckBits) << vMacScreenDepth))))
					{
						goto Label_2c;
					}
				}
Label_2c:
				j1h = (RightMax << (ln2uiblockbitsn - vMacScreenDepth))
					+ j + 1;
			} else {
				j1h = RightMax + 1;
			}
		}

		copyrows = j1v - j0v;
		copyoffset = j0v * vMacScreenByteWidth;
		copysize = copyrows * vMacScreenByteWidth;
	} else {
		if (vMacScreenDepth != 0 && g_colorMappingChanged) {
			g_colorMappingChanged = false;
			j0h = 0;
			j1h = vMacScreenWidth;
			j0v = 0;
			j1v = vMacScreenHeight;
#if WantColorTransValid
			g_colorTransValid = false;
#endif
		} else {
			if (! FindFirstChangeInLVecs(
				(uibb *)screencurrentbuff
					+ s_nextDrawRow * (vMacScreenWidth / uiblockbitsn),
				(uibb *)g_screenCompareBuff
					+ s_nextDrawRow * (vMacScreenWidth / uiblockbitsn),
				((uint32_t)(vMacScreenHeight - s_nextDrawRow)
					* (uint32_t)vMacScreenWidth) / uiblockbitsn,
				&j0))
			{
				s_nextDrawRow = 0;
				return false;
			}
			j0v = j0 / (vMacScreenWidth / uiblockbitsn);
			j0h = j0 - j0v * (vMacScreenWidth / uiblockbitsn);
			j0v += s_nextDrawRow;
			LimitDrawRow = j0v + MaxRowsDrawnPerTick;
			if (LimitDrawRow >= vMacScreenHeight) {
				LimitDrawRow = vMacScreenHeight;
				s_nextDrawRow = 0;
			} else {
				s_nextDrawRow = LimitDrawRow;
			}
			FindLastChangeInLVecs((uibb *)screencurrentbuff,
				(uibb *)g_screenCompareBuff,
				((uint32_t)LimitDrawRow
					* (uint32_t)vMacScreenWidth) / uiblockbitsn,
				&j1);
			j1v = j1 / (vMacScreenWidth / uiblockbitsn);
			j1h = j1 - j1v * (vMacScreenWidth / uiblockbitsn);
			j1v++;

			if (j0h < j1h) {
				LeftMin = j0h;
				RightMax = j1h;
			} else {
				LeftMin = j1h;
				RightMax = j0h;
			}

			FindLeftRightChangeInLMat((uibb *)screencurrentbuff,
				(uibb *)g_screenCompareBuff,
				(vMacScreenWidth / uiblockbitsn),
				j0v, j1v, &LeftMin, &LeftMask, &RightMax, &RightMask);

			for (j = 0; j < uiblockbitsn; ++j) {
				if (0 != (LeftMask
					& (((uibr)1) << (j ^ FlipCheckMonoBits))))
				{
					goto Label_1;
				}
			}
Label_1:
			j0h = LeftMin * uiblockbitsn + j;

			for (j = uiblockbitsn; --j >= 0; ) {
				if (0 != (RightMask
					& (((uibr)1) << (j ^ FlipCheckMonoBits))))
				{
					goto Label_2;
				}
			}
Label_2:
			j1h = RightMax * uiblockbitsn + j + 1;
		}

		copyrows = j1v - j0v;
		copyoffset = j0v * vMacScreenMonoByteWidth;
		copysize = copyrows * vMacScreenMonoByteWidth;
	}

	MoveBytes(screencurrentbuff + copyoffset,
		g_screenCompareBuff + copyoffset,
		copysize);

	*top = j0v;
	*left = j0h;
	*bottom = j1v;
	*right = j1h;

	return true;
}

/* --- Screen frame output --- */

int16_t g_screenChangedTop;
int16_t g_screenChangedLeft;
int16_t g_screenChangedBottom;
int16_t g_screenChangedRight;

void ScreenClearChanges()
{
	g_screenChangedTop = 0;
	g_screenChangedBottom = vMacScreenHeight;
	g_screenChangedLeft = 0;
	g_screenChangedRight = vMacScreenWidth;
}

void ScreenChangedAll()
{
	g_screenChangedTop = 0;
	g_screenChangedBottom = vMacScreenHeight;
	g_screenChangedLeft = 0;
	g_screenChangedRight = vMacScreenWidth;
}

int16_t g_screenChangedQuietTop = vMacScreenHeight;
int16_t g_screenChangedQuietLeft = vMacScreenWidth;
int16_t g_screenChangedQuietBottom = 0;
int16_t g_screenChangedQuietRight = 0;

/* Find the changed region since last frame and notify the
   platform layer (HaveChangedScreenBuff) for display update. */
void Screen_OutputFrame(uint8_t * screencurrentbuff)
{
	int16_t top;
	int16_t left;
	int16_t bottom;
	int16_t right;

	if (! g_emVideoDisable) {
		if (ScreenFindChanges(screencurrentbuff, g_emLagTime,
			&top, &left, &bottom, &right))
		{
			if (top < g_screenChangedTop) {
				g_screenChangedTop = top;
			}
			if (bottom > g_screenChangedBottom) {
				g_screenChangedBottom = bottom;
			}
			if (left < g_screenChangedLeft) {
				g_screenChangedLeft = left;
			}
			if (right > g_screenChangedRight) {
				g_screenChangedRight = right;
			}

			if (top < g_screenChangedQuietTop) {
				g_screenChangedQuietTop = top;
			}
			if (bottom > g_screenChangedQuietBottom) {
				g_screenChangedQuietBottom = bottom;
			}
			if (left < g_screenChangedQuietLeft) {
				g_screenChangedQuietLeft = left;
			}
			if (right > g_screenChangedQuietRight) {
				g_screenChangedQuietRight = right;
			}

			if (((g_screenChangedQuietRight - g_screenChangedQuietLeft) > 1)
				|| ((g_screenChangedQuietBottom
					- g_screenChangedQuietTop) > 32))
			{
				g_screenChangedQuietTop = vMacScreenHeight;
				g_screenChangedQuietLeft = vMacScreenWidth;
				g_screenChangedQuietBottom = 0;
				g_screenChangedQuietRight = 0;

				QuietEnds();
			}
		}
	}
}

/* --- Full screen view support --- */

uint16_t g_viewHSize;
uint16_t g_viewVSize;
uint16_t g_viewHStart = 0;
uint16_t g_viewVStart = 0;
#if EnableFSMouseMotion
int16_t g_savedMouseH;
int16_t g_savedMouseV;
#endif

#ifndef WantAutoScrollBorder
#define WantAutoScrollBorder 0
#endif

#if EnableFSMouseMotion
void AutoScrollScreen()
{
	int16_t Shift;
	int16_t Limit;

	if (vMacScreenWidth != g_viewHSize) {
		Shift = 0;
		Limit = g_viewHStart
#if WantAutoScrollBorder
			+ (g_viewHSize / 16)
#endif
			;
		if (g_curMouseH < Limit) {
			Shift = (Limit - g_curMouseH + 1) & (~ 1);
			Limit = g_viewHStart;
			if (Shift >= Limit) {
				Shift = Limit;
			}
			Shift = - Shift;
		} else {
			Limit = g_viewHStart + g_viewHSize
#if WantAutoScrollBorder
				- (g_viewHSize / 16)
#endif
				;
			if (g_curMouseH > Limit) {
				Shift = (g_curMouseH - Limit + 1) & (~ 1);
				Limit = vMacScreenWidth - g_viewHSize - g_viewHStart;
				if (Shift >= Limit) {
					Shift = Limit;
				}
			}
		}

		if (Shift != 0) {
			g_viewHStart += Shift;
			g_savedMouseH += Shift;
			ScreenChangedAll();
		}
	}

	if (vMacScreenHeight != g_viewVSize) {
		Shift = 0;
		Limit = g_viewVStart
#if WantAutoScrollBorder
			+ (g_viewVSize / 16)
#endif
			;
		if (g_curMouseV < Limit) {
			Shift = (Limit - g_curMouseV + 1) & (~ 1);
			Limit = g_viewVStart;
			if (Shift >= Limit) {
				Shift = Limit;
			}
			Shift = - Shift;
		} else {
			Limit = g_viewVStart + g_viewVSize
#if WantAutoScrollBorder
				- (g_viewVSize / 16)
#endif
				;
			if (g_curMouseV > Limit) {
				Shift = (g_curMouseV - Limit + 1) & (~ 1);
				Limit = vMacScreenHeight - g_viewVSize - g_viewVStart;
				if (Shift >= Limit) {
					Shift = Limit;
				}
			}
		}

		if (Shift != 0) {
			g_viewVStart += Shift;
			g_savedMouseV += Shift;
			ScreenChangedAll();
		}
	}
}
#endif

/* --- Memory allocation --- */

static void SetLongs(uint32_t *p, long n)
{
	long i;

	for (i = n; --i >= 0; ) {
		*p++ = (uint32_t) -1;
	}
}

bool AllocBlock(uint8_t **p, uint32_t n, bool FillOnes)
{
	*p = static_cast<uint8_t *>(calloc(1, n));
	if (*p == nullptr) {
		return false;
	}
	if (FillOnes) {
		SetLongs(reinterpret_cast<uint32_t *>(*p), n / 4);
	}
	return true;
}

/* --- sending debugging info to file --- */

#if dbglog_HAVE

#ifndef dbglog_buflnsz

bool dbglog_ReserveAlloc()
{
	/* nothing to do in unbuffered mode */
	return true;
}

/* dbglog_close/open/write macros already in osglu_common.h */

#else

#define dbglog_bufsz PowOf2(dbglog_buflnsz)
static uint32_t dbglog_bufpos = 0;

static char *dbglog_bufp = nullptr;

bool dbglog_ReserveAlloc()
{
	return AllocBlock((uint8_t * *)&dbglog_bufp, dbglog_bufsz,
		false);
}

#define dbglog_open dbglog_open0

static void dbglog_close()
{
	uint32_t n = ModPow2(dbglog_bufpos, dbglog_buflnsz);
	if (n != 0) {
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
	uint32_t newbufdiv = FloorDivPow2(newbufpos, dbglog_buflnsz);

	for (;;) {
		curbufdiv = FloorDivPow2(dbglog_bufpos, dbglog_buflnsz);
		bufposmod = ModPow2(dbglog_bufpos, dbglog_buflnsz);
		if (newbufdiv == curbufdiv) {
			break;
		}
		r = dbglog_bufsz - bufposmod;
		MoveBytes(reinterpret_cast<uint8_t *>(p), reinterpret_cast<uint8_t *>(dbglog_bufp + bufposmod), r);
		dbglog_write0(dbglog_bufp, dbglog_bufsz);
		L -= r;
		p += r;
		dbglog_bufpos += r;
	}
	MoveBytes(reinterpret_cast<uint8_t *>(p), reinterpret_cast<uint8_t *>(dbglog_bufp + bufposmod), L);
	dbglog_bufpos = newbufpos;
}

#endif /* dbglog_buflnsz defined */

static uint32_t CStrLength(char *s)
{
	char *p = s;

	while (*p++ != 0) {
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

	do {
		v = x & 0x0F;
		if (v < 10) {
			*--p = '0' + v;
		} else {
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

	do {
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

	if ((x > 32) && (x < 127)) {
		s = x;
	} else {
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

#endif /* dbglog_HAVE */


/* --- event queue --- */

EvtQEl EvtQA[MyEvtQSz];
uint16_t EvtQIn = 0;
uint16_t EvtQOut = 0;

EvtQEl * EvtQOutP()
{
	EvtQEl *p = nullptr;
	if (EvtQIn != EvtQOut) {
		p = &EvtQA[EvtQOut & MyEvtQIMask];
	}
	return p;
}

void EvtQOutDone()
{
	++EvtQOut;
}

bool EvtQNeedRecover = false;

EvtQEl * EvtQElPreviousIn()
{
	EvtQEl *p = nullptr;
	if (EvtQIn - EvtQOut != 0) {
		p = &EvtQA[(EvtQIn - 1) & MyEvtQIMask];
	}

	return p;
}

EvtQEl * EvtQElAlloc()
{
	EvtQEl *p = nullptr;
	if (EvtQIn - EvtQOut >= MyEvtQSz) {
		EvtQNeedRecover = true;
	} else {
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
	if (CurDown != down) {
		EvtQEl *p = EvtQElAlloc();
		if (nullptr != p) {
			p->kind = EvtQElKind::Key;
			p->u.press.key = k;
			p->u.press.down = down;

			if (down) {
				*kpi |= bit;
			} else {
				*kpi &= ~ bit;
			}
		}

		QuietEnds();
	}
}

bool g_mouseButtonState = false;

void MyMouseButtonSet(bool down)
{
	if (g_mouseButtonState != down) {
		EvtQEl *p = EvtQElAlloc();
		if (nullptr != p) {
			p->kind = EvtQElKind::MouseButton;
			p->u.press.down = down;

			g_mouseButtonState = down;
		}

		QuietEnds();
	}
}

#if EnableFSMouseMotion
void MyMousePositionSetDelta(uint16_t dh, uint16_t dv)
{
	if ((dh != 0) || (dv != 0)) {
		EvtQEl *p = EvtQElPreviousIn();
		if ((nullptr != p) && (EvtQElKind::MouseDelta == p->kind)) {
			p->u.pos.h += dh;
			p->u.pos.v += dv;
		} else {
			p = EvtQElAlloc();
			if (nullptr != p) {
				p->kind = EvtQElKind::MouseDelta;
				p->u.pos.h = dh;
				p->u.pos.v = dv;
			}
		}

		QuietEnds();
	}
}
#endif

uint16_t g_mousePosCurV = 0;
uint16_t g_mousePosCurH = 0;

void MyMousePositionSet(uint16_t h, uint16_t v)
{
	if ((h != g_mousePosCurH) || (v != g_mousePosCurV)) {
		EvtQEl *p = EvtQElPreviousIn();
		if ((nullptr == p) || (EvtQElKind::MousePos != p->kind)) {
			p = EvtQElAlloc();
		}
		if (nullptr != p) {
			p->kind = EvtQElKind::MousePos;
			p->u.pos.h = h;
			p->u.pos.v = v;

			g_mousePosCurH = h;
			g_mousePosCurV = v;
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

void DisconnectKeyCodes(uint32_t KeepMask)
{
	int j;
	int b;
	int key;
	uint32_t m;

	for (j = 0; j < 16; ++j) {
		uint8_t k1 = (reinterpret_cast<uint8_t *>(theKeys))[j];
		if (0 != k1) {
			uint8_t bit = 1;
			for (b = 0; b < 8; ++b) {
				if (0 != (k1 & bit)) {
					key = j * 8 + b;
					switch (key) {
						case MKC_Control: m = kKeepMaskControl; break;
						case MKC_CapsLock: m = kKeepMaskCapsLock; break;
						case MKC_Command: m = kKeepMaskCommand; break;
						case MKC_Option: m = kKeepMaskOption; break;
						case MKC_Shift: m = kKeepMaskShift; break;
						default: m = 0; break;
					}
					if (0 == (KeepMask & m)) {
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

const char *SavedBriefMsg = nullptr;
const char *SavedLongMsg;
#if WantAbnormalReports
uint16_t g_savedIDMsg = 0;
#endif
bool g_savedFatalMsg;

void MacMsg(const char *briefMsg, const char *longMsg, bool fatal)
{
	if (nullptr != SavedBriefMsg) {
		/*
			ignore the new message, only display the
			first error.
		*/
	} else {
		SavedBriefMsg = briefMsg;
		SavedLongMsg = longMsg;
		g_savedFatalMsg = fatal;
	}
}

#if WantAbnormalReports
void WarnMsgAbnormalID(uint16_t id)
{
	MacMsg(Localize(kStrReportAbnormalTitle),
		Localize(kStrReportAbnormalMessage), false);

	if (0 != g_savedIDMsg) {
		/*
			ignore the new message, only display the
			first error.
		*/
	} else {
		g_savedIDMsg = id;
	}
}
#endif

/* --- LocalTalk entropy pool --- */

#if EmLocalTalk

uint32_t e_p[2] = {
	0, 0
	};

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

void EntropyPoolAddPtr(uint8_t * p, uint32_t n)
{
	uint32_t i;

	for (i = n + 1; 0 != --i; ) {
		EntropyPoolAddByte(*p++);
	}

#if dbglog_HAVE
	dbglog_writeCStr("ep: ");
	dbglog_writeHex(e_p[0]);
	dbglog_writeCStr(" ");
	dbglog_writeHex(e_p[1]);
	dbglog_writeReturn();
#endif
}



uint32_t g_ltMyStamp = 0;

void LT_PickStampNodeHint()
{
	g_ltMyStamp = e_p[0];

#if dbglog_HAVE && 1
	dbglog_writelnNum("LT_MyStamp ", g_ltMyStamp);
#endif

	{
		int i = 8 + 1;

		for (;;) {
			/* user node should be in 1-127 */

			g_ltNodeHint = e_p[1] & 0x7F;

#if dbglog_HAVE && 1
			dbglog_writelnNum("LT_NodeHint ", g_ltNodeHint);
#endif

			if (0 != g_ltNodeHint) {
				break;
			}
			if (0 == --i) {
				/* just maybe, randomness is broken */
				g_ltNodeHint = 4;
				break;
			}
			EntropyPoolStir();
		}
	}
}

#endif /* EmLocalTalk */
