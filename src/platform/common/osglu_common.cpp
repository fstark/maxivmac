/*
	osglu_common.cpp

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
	COMmon code for Operating System GLUe — implementation
*/

#include "platform/common/osglu_ui.h"
#include "platform/common/osglu_ud.h"
#include "platform/common/osglu_common.h"

/* --- backend-provided debug log primitives (extern) --- */

#if dbglog_HAVE
extern bool dbglog_open0(void);
extern void dbglog_write0(char *s, uint32_t L);
extern void dbglog_close0(void);
#endif

/* --- global variables --- */

uint8_t * ROM = nullptr;
bool ROM_loaded = false;

uint32_t vSonyWritableMask = 0;
uint32_t vSonyInsertedMask = 0;

#if IncludeSonyRawMode
bool vSonyRawMode = false;
#endif

#if IncludeSonyNew
bool vSonyNewDiskWanted = false;
uint32_t vSonyNewDiskSize;
#endif

#if IncludeSonyNameNew
tPbuf vSonyNewDiskName = NotAPbuf;
#endif

uint32_t CurMacDateInSeconds = 0;
#if AutoLocation
uint32_t CurMacLatitude = 0;
uint32_t CurMacLongitude = 0;
#endif
#if AutoTimeZone
uint32_t CurMacDelta = 0;
#endif

/* Runtime screen dimensions — initialized from MachineConfig */
uint16_t g_screenWidth  = 640;
uint16_t g_screenHeight = 480;
uint8_t  g_screenDepth  = 3;

bool UseColorMode = false;
bool ColorModeWorks = false;

bool ColorMappingChanged = false;

uint16_t CLUT_reds[CLUT_size];
uint16_t CLUT_greens[CLUT_size];
uint16_t CLUT_blues[CLUT_size];

bool RequestMacOff = false;

bool ForceMacOff = false;

bool WantMacInterrupt = false;

bool WantMacReset = false;

uint8_t SpeedValue = WantInitSpeedValue;

#if EnableAutoSlow
bool WantNotAutoSlow = (WantInitNotAutoSlow != 0);
#endif

uint16_t CurMouseV = 0;
uint16_t CurMouseH = 0;

#if EnableFSMouseMotion
bool HaveMouseMotion = false;
#endif

#if EnableAutoSlow
uint32_t QuietTime = 0;
uint32_t QuietSubTicks = 0;
#endif

#if EmLocalTalk

uint8_t LT_NodeHint = 0;

#if LT_MayHaveEcho
bool CertainlyNotMyPacket = false;
#endif

uint8_t * LT_TxBuffer = NULL;

/* Transmit state */
uint16_t LT_TxBuffSz = 0;

/* Receive state */
uint8_t * LT_RxBuffer = NULL;
uint32_t LT_RxBuffSz = 0;

#endif

bool EmVideoDisable = false;
int8_t EmLagTime = 0;

uint32_t OnTrueTime = 0;

/* --- Pbuf support --- */

#if IncludePbufs
uint32_t PbufAllocatedMask;
uint32_t PbufSize[NumPbufs];
#endif

#if IncludePbufs
bool FirstFreePbuf(tPbuf *r)
{
	tPbuf i;

	for (i = 0; i < NumPbufs; ++i) {
		if (! PbufIsAllocated(i)) {
			*r = i;
			return true;
		}
	}
	return false;
}
#endif

#if IncludePbufs
void PbufNewNotify(tPbuf Pbuf_No, uint32_t count)
{
	PbufSize[Pbuf_No] = count;
	PbufAllocatedMask |= ((uint32_t)1 << Pbuf_No);
}
#endif

#if IncludePbufs
void PbufDisposeNotify(tPbuf Pbuf_No)
{
	PbufAllocatedMask &= ~ ((uint32_t)1 << Pbuf_No);
}
#endif

#if IncludePbufs
tMacErr CheckPbuf(tPbuf Pbuf_No)
{
	tMacErr result;

	if (Pbuf_No >= NumPbufs) {
		result = mnvm_nsDrvErr;
	} else if (! PbufIsAllocated(Pbuf_No)) {
		result = mnvm_offLinErr;
	} else {
		result = mnvm_noErr;
	}

	return result;
}
#endif

#if IncludePbufs
tMacErr PbufGetSize(tPbuf Pbuf_No, uint32_t *Count)
{
	tMacErr result = CheckPbuf(Pbuf_No);

	if (mnvm_noErr == result) {
		*Count = PbufSize[Pbuf_No];
	}

	return result;
}
#endif

/* --- Disk support --- */

bool FirstFreeDisk(tDrive *Drive_No)
{
	tDrive i;

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

bool AnyDiskInserted(void)
{
	return 0 != vSonyInsertedMask;
}

void DiskRevokeWritable(tDrive Drive_No)
{
	vSonyWritableMask &= ~ ((uint32_t)1 << Drive_No);
}

void DiskInsertNotify(tDrive Drive_No, bool locked)
{
	vSonyInsertedMask |= ((uint32_t)1 << Drive_No);
	if (! locked) {
		vSonyWritableMask |= ((uint32_t)1 << Drive_No);
	}

	QuietEnds();
}

void DiskEjectedNotify(tDrive Drive_No)
{
	vSonyWritableMask &= ~ ((uint32_t)1 << Drive_No);
	vSonyInsertedMask &= ~ ((uint32_t)1 << Drive_No);
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

uint8_t * screencomparebuff = nullptr;

static uint32_t NextDrawRow = 0;


#if BigEndianUnaligned

#define FlipCheckMonoBits (uiblockbitsn - 1)

#else

#define FlipCheckMonoBits 7

#endif

#define FlipCheckBits (FlipCheckMonoBits >> vMacScreenDepth)

#if WantColorTransValid
bool ColorTransValid = false;
#endif

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

	if (vMacScreenDepth != 0 && UseColorMode) {
		if (ColorMappingChanged) {
			ColorMappingChanged = false;
			j0h = 0;
			j1h = vMacScreenWidth;
			j0v = 0;
			j1v = vMacScreenHeight;
#if WantColorTransValid
			ColorTransValid = false;
#endif
		} else {
			if (! FindFirstChangeInLVecs(
				(uibb *)screencurrentbuff
					+ NextDrawRow * (vMacScreenBitWidth / uiblockbitsn),
				(uibb *)screencomparebuff
					+ NextDrawRow * (vMacScreenBitWidth / uiblockbitsn),
				((uint32_t)(vMacScreenHeight - NextDrawRow)
					* (uint32_t)vMacScreenBitWidth) / uiblockbitsn,
				&j0))
			{
				NextDrawRow = 0;
				return false;
			}
			j0v = j0 / (vMacScreenBitWidth / uiblockbitsn);
			j0h = j0 - j0v * (vMacScreenBitWidth / uiblockbitsn);
			j0v += NextDrawRow;
			LimitDrawRow = j0v + MaxRowsDrawnPerTick;
			if (LimitDrawRow >= vMacScreenHeight) {
				LimitDrawRow = vMacScreenHeight;
				NextDrawRow = 0;
			} else {
				NextDrawRow = LimitDrawRow;
			}
			FindLastChangeInLVecs((uibb *)screencurrentbuff,
				(uibb *)screencomparebuff,
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
				(uibb *)screencomparebuff,
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
		if (vMacScreenDepth != 0 && ColorMappingChanged) {
			ColorMappingChanged = false;
			j0h = 0;
			j1h = vMacScreenWidth;
			j0v = 0;
			j1v = vMacScreenHeight;
#if WantColorTransValid
			ColorTransValid = false;
#endif
		} else {
			if (! FindFirstChangeInLVecs(
				(uibb *)screencurrentbuff
					+ NextDrawRow * (vMacScreenWidth / uiblockbitsn),
				(uibb *)screencomparebuff
					+ NextDrawRow * (vMacScreenWidth / uiblockbitsn),
				((uint32_t)(vMacScreenHeight - NextDrawRow)
					* (uint32_t)vMacScreenWidth) / uiblockbitsn,
				&j0))
			{
				NextDrawRow = 0;
				return false;
			}
			j0v = j0 / (vMacScreenWidth / uiblockbitsn);
			j0h = j0 - j0v * (vMacScreenWidth / uiblockbitsn);
			j0v += NextDrawRow;
			LimitDrawRow = j0v + MaxRowsDrawnPerTick;
			if (LimitDrawRow >= vMacScreenHeight) {
				LimitDrawRow = vMacScreenHeight;
				NextDrawRow = 0;
			} else {
				NextDrawRow = LimitDrawRow;
			}
			FindLastChangeInLVecs((uibb *)screencurrentbuff,
				(uibb *)screencomparebuff,
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
				(uibb *)screencomparebuff,
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

	MyMoveBytes((uint8_t *)screencurrentbuff + copyoffset,
		(uint8_t *)screencomparebuff + copyoffset,
		copysize);

	*top = j0v;
	*left = j0h;
	*bottom = j1v;
	*right = j1h;

	return true;
}

/* --- Screen frame output --- */

int16_t ScreenChangedTop;
int16_t ScreenChangedLeft;
int16_t ScreenChangedBottom;
int16_t ScreenChangedRight;

void ScreenClearChanges(void)
{
	ScreenChangedTop = 0;
	ScreenChangedBottom = vMacScreenHeight;
	ScreenChangedLeft = 0;
	ScreenChangedRight = vMacScreenWidth;
}

void ScreenChangedAll(void)
{
	ScreenChangedTop = 0;
	ScreenChangedBottom = vMacScreenHeight;
	ScreenChangedLeft = 0;
	ScreenChangedRight = vMacScreenWidth;
}

#if EnableAutoSlow
int16_t ScreenChangedQuietTop = vMacScreenHeight;
int16_t ScreenChangedQuietLeft = vMacScreenWidth;
int16_t ScreenChangedQuietBottom = 0;
int16_t ScreenChangedQuietRight = 0;
#endif

void Screen_OutputFrame(uint8_t * screencurrentbuff)
{
	int16_t top;
	int16_t left;
	int16_t bottom;
	int16_t right;

	if (! EmVideoDisable) {
		if (ScreenFindChanges(screencurrentbuff, EmLagTime,
			&top, &left, &bottom, &right))
		{
			if (top < ScreenChangedTop) {
				ScreenChangedTop = top;
			}
			if (bottom > ScreenChangedBottom) {
				ScreenChangedBottom = bottom;
			}
			if (left < ScreenChangedLeft) {
				ScreenChangedLeft = left;
			}
			if (right > ScreenChangedRight) {
				ScreenChangedRight = right;
			}

#if EnableAutoSlow
			if (top < ScreenChangedQuietTop) {
				ScreenChangedQuietTop = top;
			}
			if (bottom > ScreenChangedQuietBottom) {
				ScreenChangedQuietBottom = bottom;
			}
			if (left < ScreenChangedQuietLeft) {
				ScreenChangedQuietLeft = left;
			}
			if (right > ScreenChangedQuietRight) {
				ScreenChangedQuietRight = right;
			}

			if (((ScreenChangedQuietRight - ScreenChangedQuietLeft) > 1)
				|| ((ScreenChangedQuietBottom
					- ScreenChangedQuietTop) > 32))
			{
				ScreenChangedQuietTop = vMacScreenHeight;
				ScreenChangedQuietLeft = vMacScreenWidth;
				ScreenChangedQuietBottom = 0;
				ScreenChangedQuietRight = 0;

				QuietEnds();
			}
#endif
		}
	}
}

/* --- Full screen view support --- */

#if MayFullScreen
uint16_t ViewHSize;
uint16_t ViewVSize;
uint16_t ViewHStart = 0;
uint16_t ViewVStart = 0;
#if EnableFSMouseMotion
int16_t SavedMouseH;
int16_t SavedMouseV;
#endif
#endif

#ifndef WantAutoScrollBorder
#define WantAutoScrollBorder 0
#endif

#if EnableFSMouseMotion
void AutoScrollScreen(void)
{
	int16_t Shift;
	int16_t Limit;

	if (vMacScreenWidth != ViewHSize) {
		Shift = 0;
		Limit = ViewHStart
#if WantAutoScrollBorder
			+ (ViewHSize / 16)
#endif
			;
		if (CurMouseH < Limit) {
			Shift = (Limit - CurMouseH + 1) & (~ 1);
			Limit = ViewHStart;
			if (Shift >= Limit) {
				Shift = Limit;
			}
			Shift = - Shift;
		} else {
			Limit = ViewHStart + ViewHSize
#if WantAutoScrollBorder
				- (ViewHSize / 16)
#endif
				;
			if (CurMouseH > Limit) {
				Shift = (CurMouseH - Limit + 1) & (~ 1);
				Limit = vMacScreenWidth - ViewHSize - ViewHStart;
				if (Shift >= Limit) {
					Shift = Limit;
				}
			}
		}

		if (Shift != 0) {
			ViewHStart += Shift;
			SavedMouseH += Shift;
			ScreenChangedAll();
		}
	}

	if (vMacScreenHeight != ViewVSize) {
		Shift = 0;
		Limit = ViewVStart
#if WantAutoScrollBorder
			+ (ViewVSize / 16)
#endif
			;
		if (CurMouseV < Limit) {
			Shift = (Limit - CurMouseV + 1) & (~ 1);
			Limit = ViewVStart;
			if (Shift >= Limit) {
				Shift = Limit;
			}
			Shift = - Shift;
		} else {
			Limit = ViewVStart + ViewVSize
#if WantAutoScrollBorder
				- (ViewVSize / 16)
#endif
				;
			if (CurMouseV > Limit) {
				Shift = (CurMouseV - Limit + 1) & (~ 1);
				Limit = vMacScreenHeight - ViewVSize - ViewVStart;
				if (Shift >= Limit) {
					Shift = Limit;
				}
			}
		}

		if (Shift != 0) {
			ViewVStart += Shift;
			SavedMouseV += Shift;
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

uint32_t ReserveAllocOffset;
uint8_t * ReserveAllocBigBlock = nullptr;

void ReserveAllocOneBlock(uint8_t * *p, uint32_t n,
	uint8_t align, bool FillOnes)
{
	ReserveAllocOffset = CeilPow2Mult(ReserveAllocOffset, align);
	if (nullptr == ReserveAllocBigBlock) {
		*p = nullptr;
	} else {
		*p = ReserveAllocBigBlock + ReserveAllocOffset;
		if (FillOnes) {
			SetLongs((uint32_t *)*p, n / 4);
		}
	}
	ReserveAllocOffset += n;
}

/* --- sending debugging info to file --- */

#if dbglog_HAVE

#ifndef dbglog_buflnsz

void dbglog_ReserveAlloc(void)
{
	/* nothing to do in unbuffered mode */
}

#define dbglog_close dbglog_close0
#define dbglog_open dbglog_open0
#define dbglog_write dbglog_write0

#else

#define dbglog_bufsz PowOf2(dbglog_buflnsz)
static uint32_t dbglog_bufpos = 0;

static char *dbglog_bufp = nullptr;

void dbglog_ReserveAlloc(void)
{
	ReserveAllocOneBlock((uint8_t * *)&dbglog_bufp, dbglog_bufsz,
		5, false);
}

#define dbglog_open dbglog_open0

static void dbglog_close(void)
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

label_retry:
	curbufdiv = FloorDivPow2(dbglog_bufpos, dbglog_buflnsz);
	bufposmod = ModPow2(dbglog_bufpos, dbglog_buflnsz);
	if (newbufdiv != curbufdiv) {
		r = dbglog_bufsz - bufposmod;
		MyMoveBytes((uint8_t *)p, (uint8_t *)(dbglog_bufp + bufposmod), r);
		dbglog_write0(dbglog_bufp, dbglog_bufsz);
		L -= r;
		p += r;
		dbglog_bufpos += r;
		goto label_retry;
	}
	MyMoveBytes((uint8_t *)p, (uint8_t *)dbglog_bufp + bufposmod, L);
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

void dbglog_writeReturn(void)
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

static void dbglog_writeSpace(void)
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

MyEvtQEl MyEvtQA[MyEvtQSz];
uint16_t MyEvtQIn = 0;
uint16_t MyEvtQOut = 0;

MyEvtQEl * MyEvtQOutP(void)
{
	MyEvtQEl *p = nullptr;
	if (MyEvtQIn != MyEvtQOut) {
		p = &MyEvtQA[MyEvtQOut & MyEvtQIMask];
	}
	return p;
}

void MyEvtQOutDone(void)
{
	++MyEvtQOut;
}

bool MyEvtQNeedRecover = false;

MyEvtQEl * MyEvtQElPreviousIn(void)
{
	MyEvtQEl *p = NULL;
	if (MyEvtQIn - MyEvtQOut != 0) {
		p = &MyEvtQA[(MyEvtQIn - 1) & MyEvtQIMask];
	}

	return p;
}

MyEvtQEl * MyEvtQElAlloc(void)
{
	MyEvtQEl *p = NULL;
	if (MyEvtQIn - MyEvtQOut >= MyEvtQSz) {
		MyEvtQNeedRecover = true;
	} else {
		p = &MyEvtQA[MyEvtQIn & MyEvtQIMask];

		++MyEvtQIn;
	}

	return p;
}

/* --- keyboard and mouse --- */

uint32_t theKeys[4];

void Keyboard_UpdateKeyMap(uint8_t key, bool down)
{
	uint8_t k = key & 127;
	uint8_t bit = 1 << (k & 7);
	uint8_t *kp = (uint8_t *)theKeys;
	uint8_t *kpi = &kp[k / 8];
	bool CurDown = ((*kpi & bit) != 0);
	if (CurDown != down) {
		MyEvtQEl *p = MyEvtQElAlloc();
		if (NULL != p) {
			p->kind = MyEvtQElKindKey;
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

bool MyMouseButtonState = false;

void MyMouseButtonSet(bool down)
{
	if (MyMouseButtonState != down) {
		MyEvtQEl *p = MyEvtQElAlloc();
		if (NULL != p) {
			p->kind = MyEvtQElKindMouseButton;
			p->u.press.down = down;

			MyMouseButtonState = down;
		}

		QuietEnds();
	}
}

#if EnableFSMouseMotion
void MyMousePositionSetDelta(uint16_t dh, uint16_t dv)
{
	if ((dh != 0) || (dv != 0)) {
		MyEvtQEl *p = MyEvtQElPreviousIn();
		if ((NULL != p) && (MyEvtQElKindMouseDelta == p->kind)) {
			p->u.pos.h += dh;
			p->u.pos.v += dv;
		} else {
			p = MyEvtQElAlloc();
			if (NULL != p) {
				p->kind = MyEvtQElKindMouseDelta;
				p->u.pos.h = dh;
				p->u.pos.v = dv;
			}
		}

		QuietEnds();
	}
}
#endif

uint16_t MyMousePosCurV = 0;
uint16_t MyMousePosCurH = 0;

void MyMousePositionSet(uint16_t h, uint16_t v)
{
	if ((h != MyMousePosCurH) || (v != MyMousePosCurV)) {
		MyEvtQEl *p = MyEvtQElPreviousIn();
		if ((NULL == p) || (MyEvtQElKindMousePos != p->kind)) {
			p = MyEvtQElAlloc();
		}
		if (NULL != p) {
			p->kind = MyEvtQElKindMousePos;
			p->u.pos.h = h;
			p->u.pos.v = v;

			MyMousePosCurH = h;
			MyMousePosCurV = v;
		}

		QuietEnds();
	}
}

void InitKeyCodes(void)
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
		uint8_t k1 = ((uint8_t *)theKeys)[j];
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

void MyEvtQTryRecoverFromFull(void)
{
	MyMouseButtonSet(false);
	DisconnectKeyCodes(0);
}

/* --- MacMsg --- */

char *SavedBriefMsg = nullptr;
char *SavedLongMsg;
#if WantAbnormalReports
uint16_t SavedIDMsg = 0;
#endif
bool SavedFatalMsg;

void MacMsg(char *briefMsg, char *longMsg, bool fatal)
{
	if (nullptr != SavedBriefMsg) {
		/*
			ignore the new message, only display the
			first error.
		*/
	} else {
		SavedBriefMsg = briefMsg;
		SavedLongMsg = longMsg;
		SavedFatalMsg = fatal;
	}
}

#if WantAbnormalReports
void WarnMsgAbnormalID(uint16_t id)
{
	MacMsg(kStrReportAbnormalTitle,
		kStrReportAbnormalMessage, false);

	if (0 != SavedIDMsg) {
		/*
			ignore the new message, only display the
			first error.
		*/
	} else {
		SavedIDMsg = id;
	}
}
#endif

/* --- LocalTalk entropy pool --- */

#if EmLocalTalk

uint32_t e_p[2] = {
	0, 0
	};

static void EntropyPoolStir(void)
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



uint32_t LT_MyStamp = 0;

void LT_PickStampNodeHint(void)
{
	LT_MyStamp = e_p[0];

#if dbglog_HAVE && 1
	dbglog_writelnNum("LT_MyStamp ", LT_MyStamp);
#endif

#if 0
	LT_NodeHint = 1; /* for testing collision handling */
#else
	{
		int i = 8 + 1;

label_retry:
		/* user node should be in 1-127 */

		LT_NodeHint = e_p[1] & 0x7F;

#if dbglog_HAVE && 1
		dbglog_writelnNum("LT_NodeHint ", LT_NodeHint);
#endif

		if (0 != LT_NodeHint) {
			/* ok */
		} else
		if (0 == --i) {
			/* just maybe, randomness is broken */
			LT_NodeHint = 4;
		} else
		{
			EntropyPoolStir();

			goto label_retry;
		}
	}
#endif
}

#endif /* EmLocalTalk */
