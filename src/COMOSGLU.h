/*
	COMOSGLU.h

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
	COMmon code for Operating System GLUe
*/

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

GLOBALVAR uint8_t * ROM = nullpr;
LOCALVAR blnr ROM_loaded = falseblnr;

GLOBALVAR uint32_t vSonyWritableMask = 0;
GLOBALVAR uint32_t vSonyInsertedMask = 0;

#if IncludeSonyRawMode
GLOBALVAR blnr vSonyRawMode = falseblnr;
#endif

#if IncludeSonyNew
GLOBALVAR blnr vSonyNewDiskWanted = falseblnr;
GLOBALVAR uint32_t vSonyNewDiskSize;
#endif

#if IncludeSonyNameNew
GLOBALVAR tPbuf vSonyNewDiskName = NotAPbuf;
#endif

GLOBALVAR uint32_t CurMacDateInSeconds = 0;
#if AutoLocation
GLOBALVAR uint32_t CurMacLatitude = 0;
GLOBALVAR uint32_t CurMacLongitude = 0;
#endif
#if AutoTimeZone
GLOBALVAR uint32_t CurMacDelta = 0;
#endif

#if 0 != vMacScreenDepth
GLOBALVAR blnr UseColorMode = falseblnr;
GLOBALVAR blnr ColorModeWorks = falseblnr;
#endif

#if 0 != vMacScreenDepth
GLOBALVAR blnr ColorMappingChanged = falseblnr;
#endif

#if (0 != vMacScreenDepth) && (vMacScreenDepth < 4)
GLOBALVAR uint16_t CLUT_reds[CLUT_size];
GLOBALVAR uint16_t CLUT_greens[CLUT_size];
GLOBALVAR uint16_t CLUT_blues[CLUT_size];
#endif

LOCALVAR blnr RequestMacOff = falseblnr;

GLOBALVAR blnr ForceMacOff = falseblnr;

GLOBALVAR blnr WantMacInterrupt = falseblnr;

GLOBALVAR blnr WantMacReset = falseblnr;

GLOBALVAR uint8_t SpeedValue = WantInitSpeedValue;

#if EnableAutoSlow
GLOBALVAR blnr WantNotAutoSlow = (WantInitNotAutoSlow != 0);
#endif

GLOBALVAR uint16_t CurMouseV = 0;
GLOBALVAR uint16_t CurMouseH = 0;

#if EnableFSMouseMotion
LOCALVAR blnr HaveMouseMotion = falseblnr;
#endif

#if EnableAutoSlow
GLOBALVAR uint32_t QuietTime = 0;
GLOBALVAR uint32_t QuietSubTicks = 0;
#endif

#if EmLocalTalk

GLOBALVAR uint8_t LT_NodeHint = 0;

#if LT_MayHaveEcho
GLOBALVAR blnr CertainlyNotMyPacket = falseblnr;
#endif

GLOBALVAR uint8_t * LT_TxBuffer = NULL;

/* Transmit state */
GLOBALVAR uint16_t LT_TxBuffSz = 0;

/* Receive state */
GLOBALVAR uint8_t * LT_RxBuffer = NULL;
	/* When data pending, this is used */
GLOBALVAR uint32_t LT_RxBuffSz = 0;
	/* When data pending, this is used */

#endif

#ifndef GrabKeysFullScreen
#define GrabKeysFullScreen 1
#endif

#ifndef GrabKeysMaxFullScreen
#define GrabKeysMaxFullScreen 0
#endif

#if IncludePbufs
LOCALVAR uint32_t PbufAllocatedMask;
LOCALVAR uint32_t PbufSize[NumPbufs];
#endif

#if IncludePbufs
#define PbufIsAllocated(i) ((PbufAllocatedMask & ((uint32_t)1 << (i))) != 0)
#endif

#if IncludePbufs
LOCALFUNC blnr FirstFreePbuf(tPbuf *r)
{
	tPbuf i;

	for (i = 0; i < NumPbufs; ++i) {
		if (! PbufIsAllocated(i)) {
			*r = i;
			return trueblnr;
		}
	}
	return falseblnr;
}
#endif

#if IncludePbufs
LOCALPROC PbufNewNotify(tPbuf Pbuf_No, uint32_t count)
{
	PbufSize[Pbuf_No] = count;
	PbufAllocatedMask |= ((uint32_t)1 << Pbuf_No);
}
#endif

#if IncludePbufs
LOCALPROC PbufDisposeNotify(tPbuf Pbuf_No)
{
	PbufAllocatedMask &= ~ ((uint32_t)1 << Pbuf_No);
}
#endif

#if IncludePbufs
GLOBALOSGLUFUNC tMacErr CheckPbuf(tPbuf Pbuf_No)
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
GLOBALOSGLUFUNC tMacErr PbufGetSize(tPbuf Pbuf_No, uint32_t *Count)
{
	tMacErr result = CheckPbuf(Pbuf_No);

	if (mnvm_noErr == result) {
		*Count = PbufSize[Pbuf_No];
	}

	return result;
}
#endif

LOCALFUNC blnr FirstFreeDisk(tDrive *Drive_No)
{
	tDrive i;

	for (i = 0; i < NumDrives; ++i) {
		if (! vSonyIsInserted(i)) {
			if (nullpr != Drive_No) {
				*Drive_No = i;
			}
			return trueblnr;
		}
	}
	return falseblnr;
}

GLOBALOSGLUFUNC blnr AnyDiskInserted(void)
{
#if 0
	tDrive i;

	for (i = 0; i < NumDrives; ++i) {
		if (vSonyIsInserted(i)) {
			return trueblnr;
		}
	}
	return falseblnr;
#endif
	return 0 != vSonyInsertedMask;
}

GLOBALOSGLUPROC DiskRevokeWritable(tDrive Drive_No)
{
	vSonyWritableMask &= ~ ((uint32_t)1 << Drive_No);
}

LOCALPROC DiskInsertNotify(tDrive Drive_No, blnr locked)
{
	vSonyInsertedMask |= ((uint32_t)1 << Drive_No);
	if (! locked) {
		vSonyWritableMask |= ((uint32_t)1 << Drive_No);
	}

	QuietEnds();
}

LOCALPROC DiskEjectedNotify(tDrive Drive_No)
{
	vSonyWritableMask &= ~ ((uint32_t)1 << Drive_No);
	vSonyInsertedMask &= ~ ((uint32_t)1 << Drive_No);
}

/*
	block type - for operating on multiple uint8_t elements
		at a time.
*/

#if LittleEndianUnaligned || BigEndianUnaligned

#define uibb uint32_t
#define uibr uint32_t
#define ln2uiblockn 2

#if 0
#define uibb long long
#define uibr long long
#define ln2uiblockn 3
#endif

#else

#define uibb uint8_t
#define uibr uint8_t
#define ln2uiblockn 0

#endif

#define uiblockn (1 << ln2uiblockn)
#define ln2uiblockbitsn (3 + ln2uiblockn)
#define uiblockbitsn (8 * uiblockn)

LOCALFUNC blnr FindFirstChangeInLVecs(uibb *ptr1, uibb *ptr2,
					uint32_t L, uint32_t *j)
{
/*
	find index of first difference
*/
	uibb *p1 = ptr1;
	uibb *p2 = ptr2;
	uint32_t i;

	for (i = L; i != 0; --i) {
		if (*p1++ != *p2++) {
			--p1;
			*j = p1 - ptr1;
			return trueblnr;
		}
	}
	return falseblnr;
}

LOCALPROC FindLastChangeInLVecs(uibb *ptr1, uibb *ptr2,
					uint32_t L, uint32_t *j)
{
/*
	find index of last difference, assuming there is one
*/
	uibb *p1 = ptr1 + L;
	uibb *p2 = ptr2 + L;

	while (*--p1 == *--p2) {
	}
	*j = p1 - ptr1;
}

LOCALPROC FindLeftRightChangeInLMat(uibb *ptr1, uibb *ptr2,
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

LOCALVAR uint8_t * screencomparebuff = nullpr;

LOCALVAR uint32_t NextDrawRow = 0;


#if BigEndianUnaligned

#define FlipCheckMonoBits (uiblockbitsn - 1)

#else

#define FlipCheckMonoBits 7

#endif

#define FlipCheckBits (FlipCheckMonoBits >> vMacScreenDepth)

#ifndef WantColorTransValid
#define WantColorTransValid 0
#endif

#if WantColorTransValid
LOCALVAR blnr ColorTransValid = falseblnr;
#endif

LOCALFUNC blnr ScreenFindChanges(uint8_t * screencurrentbuff,
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

#if 0 != vMacScreenDepth
	if (UseColorMode) {
		if (ColorMappingChanged) {
			ColorMappingChanged = falseblnr;
			j0h = 0;
			j1h = vMacScreenWidth;
			j0v = 0;
			j1v = vMacScreenHeight;
#if WantColorTransValid
			ColorTransValid = falseblnr;
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
				return falseblnr;
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

#if vMacScreenDepth > ln2uiblockbitsn
			j0h =  (LeftMin >> (vMacScreenDepth - ln2uiblockbitsn));
#elif ln2uiblockbitsn > vMacScreenDepth
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
#else
			j0h =  LeftMin;
#endif

#if vMacScreenDepth > ln2uiblockbitsn
			j1h = (RightMax >> (vMacScreenDepth - ln2uiblockbitsn)) + 1;
#elif ln2uiblockbitsn > vMacScreenDepth
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
#else
			j1h = RightMax + 1;
#endif
		}

		copyrows = j1v - j0v;
		copyoffset = j0v * vMacScreenByteWidth;
		copysize = copyrows * vMacScreenByteWidth;
	} else
#endif
	{
#if 0 != vMacScreenDepth
		if (ColorMappingChanged) {
			ColorMappingChanged = falseblnr;
			j0h = 0;
			j1h = vMacScreenWidth;
			j0v = 0;
			j1v = vMacScreenHeight;
#if WantColorTransValid
			ColorTransValid = falseblnr;
#endif
		} else
#endif
		{
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
				return falseblnr;
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

	MyMoveBytes((anyp)screencurrentbuff + copyoffset,
		(anyp)screencomparebuff + copyoffset,
		copysize);

	*top = j0v;
	*left = j0h;
	*bottom = j1v;
	*right = j1h;

	return trueblnr;
}

GLOBALVAR blnr EmVideoDisable = falseblnr;
GLOBALVAR int8_t EmLagTime = 0;

GLOBALVAR uint32_t OnTrueTime = 0;
	/*
		The time slice we are currently dealing
		with, in the same units as TrueEmulatedTime.
	*/

LOCALVAR int16_t ScreenChangedTop;
LOCALVAR int16_t ScreenChangedLeft;
LOCALVAR int16_t ScreenChangedBottom;
LOCALVAR int16_t ScreenChangedRight;

LOCALPROC ScreenClearChanges(void)
{
	ScreenChangedTop = 0;
	ScreenChangedBottom = vMacScreenHeight;
	ScreenChangedLeft = 0;
	ScreenChangedRight = vMacScreenWidth;
}

LOCALPROC ScreenChangedAll(void)
{
	ScreenChangedTop = 0;
	ScreenChangedBottom = vMacScreenHeight;
	ScreenChangedLeft = 0;
	ScreenChangedRight = vMacScreenWidth;
}

#if EnableAutoSlow
LOCALVAR int16_t ScreenChangedQuietTop = vMacScreenHeight;
LOCALVAR int16_t ScreenChangedQuietLeft = vMacScreenWidth;
LOCALVAR int16_t ScreenChangedQuietBottom = 0;
LOCALVAR int16_t ScreenChangedQuietRight = 0;
#endif

GLOBALOSGLUPROC Screen_OutputFrame(uint8_t * screencurrentbuff)
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

#if MayFullScreen
LOCALVAR uint16_t ViewHSize;
LOCALVAR uint16_t ViewVSize;
LOCALVAR uint16_t ViewHStart = 0;
LOCALVAR uint16_t ViewVStart = 0;
#if EnableFSMouseMotion
LOCALVAR int16_t SavedMouseH;
LOCALVAR int16_t SavedMouseV;
#endif
#endif

#ifndef WantAutoScrollBorder
#define WantAutoScrollBorder 0
#endif

#if EnableFSMouseMotion
LOCALPROC AutoScrollScreen(void)
{
	int16_t Shift;
	int16_t Limit;

	/*
		Scroll in multiples of two pixels, so as to
		work better with the common gray pattern.
		ViewHSize and ViewVSize are constrained
		to a multiple of two.

		Mac OS (some versions at least) constrains
		the mouse position to be less than the screen
		height and width, not allowing equal to it.
		Can still scroll to see last pixel because
		scroll in multiples of two pixels.
	*/

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

LOCALPROC SetLongs(uint32_t *p, long n)
{
	long i;

	for (i = n; --i >= 0; ) {
		*p++ = (uint32_t) -1;
	}
}

LOCALVAR uint32_t ReserveAllocOffset;
LOCALVAR uint8_t * ReserveAllocBigBlock = nullpr;

#define PowOf2(p) ((uint32_t)1 << (p))
#define Pow2Mask(p) (PowOf2(p) - 1)
#define ModPow2(i, p) ((i) & Pow2Mask(p))
#define FloorDivPow2(i, p) ((i) >> (p))
#define FloorPow2Mult(i, p) ((i) & (~ Pow2Mask(p)))
#define CeilPow2Mult(i, p) FloorPow2Mult((i) + Pow2Mask(p), (p))
	/* warning - CeilPow2Mult evaluates p twice */

GLOBALOSGLUPROC ReserveAllocOneBlock(uint8_t * *p, uint32_t n,
	uint8_t align, blnr FillOnes)
{
	ReserveAllocOffset = CeilPow2Mult(ReserveAllocOffset, align);
	if (nullpr == ReserveAllocBigBlock) {
		*p = nullpr;
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

#define dbglog_ReserveAlloc()
#define dbglog_close dbglog_close0
#define dbglog_open dbglog_open0
#define dbglog_write dbglog_write0

#else

#define dbglog_bufsz PowOf2(dbglog_buflnsz)
LOCALVAR uint32_t dbglog_bufpos = 0;

LOCALVAR char *dbglog_bufp = nullpr;

LOCALPROC dbglog_ReserveAlloc(void)
{
	ReserveAllocOneBlock((uint8_t * *)&dbglog_bufp, dbglog_bufsz,
		5, falseblnr);
}

#define dbglog_open dbglog_open0

LOCALPROC dbglog_close(void)
{
	uint32_t n = ModPow2(dbglog_bufpos, dbglog_buflnsz);
	if (n != 0) {
		dbglog_write0(dbglog_bufp, n);
	}

	dbglog_close0();
}

LOCALPROC dbglog_write(char *p, uint32_t L)
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
		MyMoveBytes((anyp)p, (anyp)(dbglog_bufp + bufposmod), r);
		dbglog_write0(dbglog_bufp, dbglog_bufsz);
		L -= r;
		p += r;
		dbglog_bufpos += r;
		goto label_retry;
	}
	MyMoveBytes((anyp)p, (anyp)dbglog_bufp + bufposmod, L);
	dbglog_bufpos = newbufpos;
}

#endif /* dbglog_buflnsz defined */

LOCALFUNC uint32_t CStrLength(char *s)
{
	char *p = s;

	while (*p++ != 0) {
	}
	return p - s - 1;
}

GLOBALOSGLUPROC dbglog_writeCStr(char *s)
{
	/* fprintf(DumpFile, "%s", s); */
	dbglog_write(s, CStrLength(s));
}

GLOBALOSGLUPROC dbglog_writeReturn(void)
{
	dbglog_writeCStr("\n");
	/* fprintf(DumpFile, "\n"); */
}

GLOBALOSGLUPROC dbglog_writeHex(uint32_t x)
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
	/* fprintf(DumpFile, "%d", (int)x); */
}

GLOBALOSGLUPROC dbglog_writeNum(uint32_t x)
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
	/* fprintf(DumpFile, "%d", (int)x); */
}

GLOBALOSGLUPROC dbglog_writeMacChar(uint8_t x)
{
	char s;

	if ((x > 32) && (x < 127)) {
		s = x;
	} else {
		s = '?';
	}

	dbglog_write(&s, 1);
}

LOCALPROC dbglog_writeSpace(void)
{
	dbglog_writeCStr(" ");
}

GLOBALOSGLUPROC dbglog_writeln(char *s)
{
	dbglog_writeCStr(s);
	dbglog_writeReturn();
}

GLOBALOSGLUPROC dbglog_writelnHex(char *s, uint32_t x)
{
	dbglog_writeCStr(s);
	dbglog_writeSpace();
	dbglog_writeHex(x);
	dbglog_writeReturn();
}

GLOBALOSGLUPROC dbglog_writelnNum(char *s, int32_t v)
{
	dbglog_writeCStr(s);
	dbglog_writeSpace();
	dbglog_writeNum(v);
	dbglog_writeReturn();
}

#endif /* dbglog_HAVE */


/* my event queue */

#define MyEvtQLg2Sz 4
#define MyEvtQSz (1 << MyEvtQLg2Sz)
#define MyEvtQIMask (MyEvtQSz - 1)

LOCALVAR MyEvtQEl MyEvtQA[MyEvtQSz];
LOCALVAR uint16_t MyEvtQIn = 0;
LOCALVAR uint16_t MyEvtQOut = 0;

GLOBALOSGLUFUNC MyEvtQEl * MyEvtQOutP(void)
{
	MyEvtQEl *p = nullpr;
	if (MyEvtQIn != MyEvtQOut) {
		p = &MyEvtQA[MyEvtQOut & MyEvtQIMask];
	}
	return p;
}

GLOBALOSGLUPROC MyEvtQOutDone(void)
{
	++MyEvtQOut;
}

LOCALVAR blnr MyEvtQNeedRecover = falseblnr;
	/* events lost because of full queue */

LOCALFUNC MyEvtQEl * MyEvtQElPreviousIn(void)
{
	MyEvtQEl *p = NULL;
	if (MyEvtQIn - MyEvtQOut != 0) {
		p = &MyEvtQA[(MyEvtQIn - 1) & MyEvtQIMask];
	}

	return p;
}

LOCALFUNC MyEvtQEl * MyEvtQElAlloc(void)
{
	MyEvtQEl *p = NULL;
	if (MyEvtQIn - MyEvtQOut >= MyEvtQSz) {
		MyEvtQNeedRecover = trueblnr;
	} else {
		p = &MyEvtQA[MyEvtQIn & MyEvtQIMask];

		++MyEvtQIn;
	}

	return p;
}

LOCALVAR uint32_t theKeys[4];

LOCALPROC Keyboard_UpdateKeyMap(uint8_t key, blnr down)
{
	uint8_t k = key & 127; /* just for safety */
	uint8_t bit = 1 << (k & 7);
	uint8_t *kp = (uint8_t *)theKeys;
	uint8_t *kpi = &kp[k / 8];
	blnr CurDown = ((*kpi & bit) != 0);
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

LOCALVAR blnr MyMouseButtonState = falseblnr;

LOCALPROC MyMouseButtonSet(blnr down)
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
LOCALPROC MyMousePositionSetDelta(uint16_t dh, uint16_t dv)
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

LOCALVAR uint16_t MyMousePosCurV = 0;
LOCALVAR uint16_t MyMousePosCurH = 0;

LOCALPROC MyMousePositionSet(uint16_t h, uint16_t v)
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

#if 0
#define Keyboard_TestKeyMap(key) \
	((((uint8_t *)theKeys)[(key) / 8] & (1 << ((key) & 7))) != 0)
#endif

LOCALPROC InitKeyCodes(void)
{
	theKeys[0] = 0;
	theKeys[1] = 0;
	theKeys[2] = 0;
	theKeys[3] = 0;
}

#define kKeepMaskControl  (1 << 0)
#define kKeepMaskCapsLock (1 << 1)
#define kKeepMaskCommand  (1 << 2)
#define kKeepMaskOption   (1 << 3)
#define kKeepMaskShift    (1 << 4)

LOCALPROC DisconnectKeyCodes(uint32_t KeepMask)
{
	/*
		Called when may miss key ups,
		so act is if all pressed keys have been released,
		except maybe for control, caps lock, command,
		option and shift.
	*/

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
						Keyboard_UpdateKeyMap(key, falseblnr);
					}
				}
				bit <<= 1;
			}
		}
	}
}

LOCALPROC MyEvtQTryRecoverFromFull(void)
{
	MyMouseButtonSet(falseblnr);
	DisconnectKeyCodes(0);
}

/* MacMsg */

LOCALVAR char *SavedBriefMsg = nullpr;
LOCALVAR char *SavedLongMsg;
#if WantAbnormalReports
LOCALVAR uint16_t SavedIDMsg = 0;
#endif
LOCALVAR blnr SavedFatalMsg;

LOCALPROC MacMsg(char *briefMsg, char *longMsg, blnr fatal)
{
	if (nullpr != SavedBriefMsg) {
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
GLOBALOSGLUPROC WarnMsgAbnormalID(uint16_t id)
{
	MacMsg(kStrReportAbnormalTitle,
		kStrReportAbnormalMessage, falseblnr);

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

#if EmLocalTalk

/*
	small and simple entropy pool

	cryptographic rigor not claimed. or any rigor.

	there is code for a serious implementation of an entropy
	pool in the Mini vMac extra "MakeRand" (adapted from
	MacPGP), but it seems massive overkill for the purpose
	here - minimizing the chance that two instances of
	Mini vMac pick the same LT_NodeHint and LT_MyStamp.
*/

LOCALVAR uint32_t e_p[2] = {
	0, 0
	};

LOCALPROC EntropyPoolStir(void)
{
	/*
		mix up the bits in a way that doesn't lose information.
		that is, each of the 2**64 possible inputs leads to a
			different one of the 2**64 possible outputs.

		to do: verify it actually does scramble things sufficiently,
			or modify it til it does.
	*/

	uint32_t t0a = e_p[0];
	uint32_t t1a = e_p[1];

	uint32_t t0b = t0a * 0xAE3CC725 + 0xD860D735;
	uint32_t t1b = t1a * 0x9FE72885 + 0x641AD0A9;
		/*
			multiplication constants from
				"https://arxiv.org/abs/2001.05304",
				though not being used as intended. haven't
				really actually read the thing yet.
			addition constants are just random odd numbers.
		*/

	uint32_t t0c = (t0b << 8) + (t1b >> 24);
	uint32_t t1c = (t1b << 8) + (t0b >> 24);

	e_p[0] = t0c;
	e_p[1] = t1c;
}

LOCALPROC EntropyPoolAddByte(uint8_t v)
{
	e_p[0] += v;
	e_p[1] += v;

	EntropyPoolStir();
}

LOCALPROC EntropyPoolAddPtr(uint8_t * p, uint32_t n)
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



LOCALVAR uint32_t LT_MyStamp = 0;
	/*
		randomly chosen value included in sent packets.
		if received packets have different value, then know it
			is from somebody else. (but if it is same value, don't
			know for sure it isn't from somebody else.)
	*/

LOCALPROC LT_PickStampNodeHint(void)
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
			/*
				fall back to a different random number generater:
					https://xkcd.com/221/
			*/
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
