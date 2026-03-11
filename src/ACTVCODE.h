/*
	ACTVCODE.h

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
	ACTiVation CODE
*/

LOCALFUNC uint32_t KeyFun0(uint32_t x, uint32_t y, uint32_t m)
{
	uint32_t r = x + y;

	if ((r >= m) || (r < x)) {
		r -= m;
	}

	return r;
}

LOCALFUNC uint32_t KeyFun1(uint32_t x, uint32_t y, uint32_t m)
{
	uint32_t r = 0;
	uint32_t t = x;
	uint32_t s = y;

	while (s > 0) {
		if (0 != (s & 1)) {
			r = KeyFun0(r, t, m);
		}
		t = KeyFun0(t, t, m);
		s >>= 1;
	}

	return r;
}

LOCALFUNC uint32_t KeyFun2(uint32_t x, uint32_t y, uint32_t m)
{
	uint32_t r = 1;
	uint32_t t = x;
	uint32_t s = y;

	while (s > 0) {
		if (0 != (s & 1)) {
			r = KeyFun1(r, t, m);
		}
		t = KeyFun1(t, t, m);
		s >>= 1;
	}

	return r;
}

LOCALFUNC blnr CheckActvCode(uint8_t * p, blnr *Trial)
{
	blnr IsOk = falseblnr;
	uint32_t v0 = do_get_mem_long(p);
	uint32_t v1 = do_get_mem_long(p + 4);

	if (v0 > KeyCon2) {
		/* v0 too big */
	} else if (v1 > KeyCon4) {
		/* v1 too big */
	} else {
		uint32_t t0 = KeyFun0(v0, KeyCon0, KeyCon2);
		uint32_t t1 = KeyFun2(KeyCon1, t0, KeyCon2);
		uint32_t t2 = KeyFun2(v1, KeyCon3, KeyCon4);
		uint32_t t3 = KeyFun0(t2, KeyCon4 - t1, KeyCon4);
		uint32_t t4 = KeyFun0(t3, KeyCon4 - KeyCon5, KeyCon4);
		if ((0 == (t4 >> 8)) && (t4 >= KeyCon6)) {
			*Trial = falseblnr;
			IsOk = trueblnr;
		} else if (0 == t4) {
			*Trial = trueblnr;
			IsOk = trueblnr;
		}
	}

	return IsOk;
}

/* user interface */

LOCALFUNC blnr Key2Digit(uint8_t key, uint8_t *r)
{
	uint8_t v;

	switch (key) {
		case MKC_0:
		case MKC_KP0:
			v = 0;
			break;
		case MKC_1:
		case MKC_KP1:
			v = 1;
			break;
		case MKC_2:
		case MKC_KP2:
			v = 2;
			break;
		case MKC_3:
		case MKC_KP3:
			v = 3;
			break;
		case MKC_4:
		case MKC_KP4:
			v = 4;
			break;
		case MKC_5:
		case MKC_KP5:
			v = 5;
			break;
		case MKC_6:
		case MKC_KP6:
			v = 6;
			break;
		case MKC_7:
		case MKC_KP7:
			v = 7;
			break;
		case MKC_8:
		case MKC_KP8:
			v = 8;
			break;
		case MKC_9:
		case MKC_KP9:
			v = 9;
			break;
		default:
			return falseblnr;
			break;
	}

	*r = v;
	return trueblnr;
}

#define ActvCodeMaxLen 20
LOCALVAR uint16_t ActvCodeLen = 0;
LOCALVAR uint8_t ActvCodeDigits[ActvCodeMaxLen];

#define ActvCodeFileLen 8

#if UseActvFile
FORWARDFUNC tMacErr ActvCodeFileSave(uint8_t * p);
FORWARDFUNC tMacErr ActvCodeFileLoad(uint8_t * p);
#endif

LOCALVAR uint8_t CurActvCode[ActvCodeFileLen];

LOCALPROC DoActvCodeModeKey(uint8_t key)
{
	uint8_t digit;
	uint8_t L;
	int i;
	blnr Trial;

	if (MKC_BackSpace == key) {
		if (ActvCodeLen > 0) {
			--ActvCodeLen;
			NeedWholeScreenDraw = trueblnr;
		}
	} else if (Key2Digit(key, &digit)) {
		if (ActvCodeLen < (ActvCodeMaxLen - 1)) {
			ActvCodeDigits[ActvCodeLen] = digit;
			++ActvCodeLen;
			NeedWholeScreenDraw = trueblnr;
			L = ActvCodeDigits[0] + (1 + 9);
			if (ActvCodeLen == L) {
				uint32_t v0 = 0;
				uint32_t v1 = 0;

				for (i = 1; i < (ActvCodeDigits[0] + 1); ++i) {
					v0 = v0 * 10 + ActvCodeDigits[i];
				}
				for (; i < ActvCodeLen; ++i) {
					v1 = v1 * 10 + ActvCodeDigits[i];
				}

				do_put_mem_long(&CurActvCode[0], v0);
				do_put_mem_long(&CurActvCode[4], v1);

				if (CheckActvCode(CurActvCode, &Trial)) {
					SpecialModeClr(SpclModeActvCode);
					NeedWholeScreenDraw = trueblnr;
#if UseActvFile
					if (Trial) {
						MacMsg(
							"Using temporary code.",
							"Thank you for trying Mini vMac!",
							falseblnr);
					} else {
						if (mnvm_noErr != ActvCodeFileSave(CurActvCode))
						{
							MacMsg("Oops",
								"I could not save the activation code"
								" to disk.",
								falseblnr);
						} else {
							MacMsg("Activation succeeded.",
								"Thank you!", falseblnr);
						}
					}
#else
					MacMsg(
						"Thank you for trying Mini vMac!",
						"sample variation : ^v",
						falseblnr);
#endif
				}
			} else if (ActvCodeLen > L) {
				--ActvCodeLen;
			}
		}
	}
}

LOCALPROC DrawCellsActvCodeModeBody(void)
{
#if UseActvFile
	DrawCellsOneLineStr("Please enter your activation code:");
	DrawCellsBlankLine();
#else
	DrawCellsOneLineStr(
		"To try this variation of ^p, please type these numbers:");
	DrawCellsBlankLine();
	DrawCellsOneLineStr("281 953 822 340");
	DrawCellsBlankLine();
#endif

	if (0 == ActvCodeLen) {
		DrawCellsOneLineStr("?");
	} else {
		int i;
		uint8_t L = ActvCodeDigits[0] + (1 + 9);

		DrawCellsBeginLine();
		for (i = 0; i < L; ++i) {
			if (0 == ((L - i) % 3)) {
				if (0 != i) {
					DrawCellAdvance(kCellSpace);
				}
			}
			if (i < ActvCodeLen) {
				DrawCellAdvance(kCellDigit0 + ActvCodeDigits[i]);
			} else if (i == ActvCodeLen) {
				DrawCellAdvance(kCellQuestion);
			} else {
				DrawCellAdvance(kCellHyphen);
			}
		}
		DrawCellsEndLine();
		if (L == ActvCodeLen) {
			DrawCellsBlankLine();
			DrawCellsOneLineStr(
				"Sorry, this is not a valid activation code.");
		}
	}

#if UseActvFile
	DrawCellsBlankLine();
	DrawCellsOneLineStr(
		"If you haven;}t obtained an activation code yet,"
		" here is a temporary one:");
	DrawCellsBlankLine();
	DrawCellsOneLineStr("281 953 822 340");
#else
	DrawCellsBlankLine();
	DrawCellsOneLineStr(kStrForMoreInfo);
	DrawCellsOneLineStr("http://www.gryphel.com/c/var/");
#endif
}

LOCALPROC DrawActvCodeMode(void)
{
	DrawSpclMode0(
#if UseActvFile
		"Activation Code",
#else
		"sample variation : ^v",
#endif
		DrawCellsActvCodeModeBody);
}

#if UseActvFile
LOCALPROC ClStrAppendHexLong(int *L0, uint8_t *r, uint32_t v)
{
	ClStrAppendHexWord(L0, r, (v >> 16) & 0xFFFF);
	ClStrAppendHexWord(L0, r, v & 0xFFFF);
}
#endif

LOCALPROC CopyRegistrationStr(void)
{
	uint8_t ps[ClStrMaxLength];
	int i;
	int L;
	tPbuf j;
#if UseActvFile
	int L0;
	uint32_t sum;

	ClStrFromSubstCStr(&L0, ps, "^v ");

	for (i = 0; i < L0; ++i) {
		ps[i] = Cell2MacAsciiMap[ps[i]];
	}
	L = L0;

	sum = 0;
	for (i = 0; i < L; ++i) {
		sum += ps[i];
		sum = (sum << 5) | ((sum >> (32 - 5)) & 0x1F);
		sum += (sum << 8);
	}

	sum &= 0x1FFFFFFF;

	sum = KeyFun0(sum, do_get_mem_long(&CurActvCode[0]), KeyCon4);

	ClStrAppendHexLong(&L, ps, sum);

	sum = KeyFun0(sum, do_get_mem_long(&CurActvCode[4]), KeyCon4);
	sum = KeyFun2(sum, KeyCon3, KeyCon4);

	ClStrAppendHexLong(&L, ps, sum);

	for (i = L0; i < L; ++i) {
		ps[i] = Cell2MacAsciiMap[ps[i]];
	}
#else
	ClStrFromSubstCStr(&L, ps, "^v");

	for (i = 0; i < L; ++i) {
		ps[i] = Cell2MacAsciiMap[ps[i]];
	}
#endif

	if (mnvm_noErr == PbufNew(L, &j)) {
		PbufTransfer(ps, j, 0, L, trueblnr);
		HTCEexport(j);
	}
}

LOCALFUNC blnr ActvCodeInit(void)
{
#if UseActvFile
	blnr Trial;

	if ((mnvm_noErr != ActvCodeFileLoad(CurActvCode))
		|| (! CheckActvCode(CurActvCode, &Trial))
		|| Trial
		)
#endif
	{
		SpecialModeSet(SpclModeActvCode);
		NeedWholeScreenDraw = trueblnr;
	}

	return trueblnr;
}
