/*
	intl_chars.h

	Copyright (C) 2010 Paul C. Pratt

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
	InterNaTionAL CHARacters — declarations
*/

#pragma once

/* --- Cell enum (font glyph indices) --- */

enum {
	kCellUpA,
	kCellUpB,
	kCellUpC,
	kCellUpD,
	kCellUpE,
	kCellUpF,
	kCellUpG,
	kCellUpH,
	kCellUpI,
	kCellUpJ,
	kCellUpK,
	kCellUpL,
	kCellUpM,
	kCellUpN,
	kCellUpO,
	kCellUpP,
	kCellUpQ,
	kCellUpR,
	kCellUpS,
	kCellUpT,
	kCellUpU,
	kCellUpV,
	kCellUpW,
	kCellUpX,
	kCellUpY,
	kCellUpZ,
	kCellLoA,
	kCellLoB,
	kCellLoC,
	kCellLoD,
	kCellLoE,
	kCellLoF,
	kCellLoG,
	kCellLoH,
	kCellLoI,
	kCellLoJ,
	kCellLoK,
	kCellLoL,
	kCellLoM,
	kCellLoN,
	kCellLoO,
	kCellLoP,
	kCellLoQ,
	kCellLoR,
	kCellLoS,
	kCellLoT,
	kCellLoU,
	kCellLoV,
	kCellLoW,
	kCellLoX,
	kCellLoY,
	kCellLoZ,
	kCellDigit0,
	kCellDigit1,
	kCellDigit2,
	kCellDigit3,
	kCellDigit4,
	kCellDigit5,
	kCellDigit6,
	kCellDigit7,
	kCellDigit8,
	kCellDigit9,
	kCellExclamation,
	kCellAmpersand,
	kCellApostrophe,
	kCellLeftParen,
	kCellRightParen,
	kCellComma,
	kCellHyphen,
	kCellPeriod,
	kCellSlash,
	kCellColon,
	kCellSemicolon,
	kCellQuestion,
	kCellEllipsis,
	kCellUnderscore,
	kCellLeftDQuote,
	kCellRightDQuote,
	kCellLeftSQuote,
	kCellRightSQuote,
	kCellCopyright,
	kCellSpace,

#if NeedIntlChars
	kCellUpADiaeresis,
	kCellUpARing,
	kCellUpCCedilla,
	kCellUpEAcute,
	kCellUpNTilde,
	kCellUpODiaeresis,
	kCellUpUDiaeresis,
	kCellLoAAcute,
	kCellLoAGrave,
	kCellLoACircumflex,
	kCellLoADiaeresis,
	kCellLoATilde,
	kCellLoARing,
	kCellLoCCedilla,
	kCellLoEAcute,
	kCellLoEGrave,
	kCellLoECircumflex,
	kCellLoEDiaeresis,
	kCellLoIAcute,
	kCellLoIGrave,
	kCellLoICircumflex,
	kCellLoIDiaeresis,
	kCellLoNTilde,
	kCellLoOAcute,
	kCellLoOGrave,
	kCellLoOCircumflex,
	kCellLoODiaeresis,
	kCellLoOTilde,
	kCellLoUAcute,
	kCellLoUGrave,
	kCellLoUCircumflex,
	kCellLoUDiaeresis,

	kCellUpAE,
	kCellUpOStroke,

	kCellLoAE,
	kCellLoOStroke,
	kCellInvQuestion,
	kCellInvExclam,

	kCellUpAGrave,
	kCellUpATilde,
	kCellUpOTilde,
	kCellUpLigatureOE,
	kCellLoLigatureOE,

	kCellLoYDiaeresis,
	kCellUpYDiaeresis,

	kCellUpACircumflex,
	kCellUpECircumflex,
	kCellUpAAcute,
	kCellUpEDiaeresis,
	kCellUpEGrave,
	kCellUpIAcute,
	kCellUpICircumflex,
	kCellUpIDiaeresis,
	kCellUpIGrave,
	kCellUpOAcute,
	kCellUpOCircumflex,

	kCellUpOGrave,
	kCellUpUAcute,
	kCellUpUCircumflex,
	kCellUpUGrave,
	kCellSharpS,

	kCellUpACedille,
	kCellLoACedille,
	kCellUpCAcute,
	kCellLoCAcute,
	kCellUpECedille,
	kCellLoECedille,
	kCellUpLBar,
	kCellLoLBar,
	kCellUpNAcute,
	kCellLoNAcute,
	kCellUpSAcute,
	kCellLoSAcute,
	kCellUpZAcute,
	kCellLoZAcute,
	kCellUpZDot,
	kCellLoZDot,
	kCellMidDot,
	kCellUpCCaron,
	kCellLoCCaron,
	kCellLoECaron,
	kCellLoRCaron,
	kCellLoSCaron,
	kCellLoTCaron,
	kCellLoZCaron,
	kCellUpYAcute,
	kCellLoYAcute,
	kCellLoUDblac,
	kCellLoURing,
	kCellUpDStroke,
	kCellLoDStroke,
#endif

	kCellUpperLeft,
	kCellUpperMiddle,
	kCellUpperRight,
	kCellMiddleLeft,
	kCellMiddleRight,
	kCellLowerLeft,
	kCellLowerMiddle,
	kCellLowerRight,
	kCellGraySep,
	kCellIcon00,
	kCellIcon01,
	kCellIcon02,
	kCellIcon03,
	kCellIcon10,
	kCellIcon11,
	kCellIcon12,
	kCellIcon13,
	kCellIcon20,
	kCellIcon21,
	kCellIcon22,
	kCellIcon23,
#if EnableAltKeysMode
	kInsertText00,
	kInsertText01,
	kInsertText02,
	kInsertText03,
	kInsertText04,
#endif
#if EnableDemoMsg
	kCellDemo0,
	kCellDemo1,
	kCellDemo2,
	kCellDemo3,
	kCellDemo4,
	kCellDemo5,
	kCellDemo6,
	kCellDemo7,
#endif

	kNumCells
};

/* --- Data table declarations --- */

extern const uint8_t CellData[];

#if UseActvCode && 0
#define UseActvFile 1
#else
#define UseActvFile 0
#endif

#ifndef NeedCell2MacAsciiMap
#if 1
#define NeedCell2MacAsciiMap 1
#else
#define NeedCell2MacAsciiMap 0
#endif
#endif

#if NeedCell2MacAsciiMap
extern const char Cell2MacAsciiMap[];
#endif

/* Always compile all mapping tables for cross-TU availability */
#ifndef NeedCell2PlainAsciiMap
#define NeedCell2PlainAsciiMap 1
#endif

#if NeedCell2PlainAsciiMap
extern const char Cell2PlainAsciiMap[];
#endif

#ifndef NeedCell2UnicodeMap
#define NeedCell2UnicodeMap 1
#endif

#if NeedCell2UnicodeMap
extern const uint16_t Cell2UnicodeMap[];
#endif

#ifndef NeedCell2WinAsciiMap
#define NeedCell2WinAsciiMap 0
#endif

#if NeedCell2WinAsciiMap
extern const char Cell2WinAsciiMap[];
#endif

/* --- State variables --- */

extern bool SpeedStopped;
extern bool RunInBackground;

#if VarFullScreen
extern bool WantFullScreen;
#endif

#if EnableMagnify
extern bool WantMagnify;
#endif

/* Force all Need* to 1 — these variables are always compiled in intl_chars.cpp */
#ifndef NeedRequestInsertDisk
#define NeedRequestInsertDisk 1
#endif

#ifndef NeedDoMoreCommandsMsg
#define NeedDoMoreCommandsMsg 1
#endif

#ifndef NeedDoAboutMsg
#define NeedDoAboutMsg 1
#endif

extern bool RequestInsertDisk;

#ifndef NeedRequestIthDisk
#define NeedRequestIthDisk 1
#endif

extern uint8_t RequestIthDisk;

#if UseControlKeys
extern bool ControlKeyPressed;
#endif

#ifndef kStrCntrlKyName
#define kStrCntrlKyName "control"
#endif

#ifndef kControlModeKey
#define kControlModeKey kStrCntrlKyName
#endif

#ifndef kUnMappedKey
#define kUnMappedKey kStrCntrlKyName
#endif

/* --- String substitution functions --- */

#define ClStrMaxLength 512

char * GetSubstitutionStr(char x);
int ClStrSizeSubstCStr(char *s);
void ClStrAppendChar(int *L0, uint8_t *r, uint8_t c);
void ClStrAppendSubstCStr(int *L, uint8_t *r, char *s);
void ClStrFromSubstCStr(int *L, uint8_t *r, char *s);
