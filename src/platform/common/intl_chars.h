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

	kNumCells
};

/* --- Data table declarations --- */

extern const uint8_t CellData[];

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

/* --- State variables --- */

extern bool g_speedStopped;
extern bool g_runInBackground;

extern bool g_wantFullScreen;

extern bool g_wantMagnify;

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

extern bool g_requestInsertDisk;

#ifndef NeedRequestIthDisk
#define NeedRequestIthDisk 1
#endif

extern uint8_t g_requestIthDisk;

extern bool g_controlKeyPressed;

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

const char * GetSubstitutionStr(char x);
int ClStrSizeSubstCStr(const char *s);
void ClStrAppendChar(int *L0, uint8_t *r, uint8_t c);
void ClStrAppendSubstCStr(int *L, uint8_t *r, const char *s);
void ClStrFromSubstCStr(int *L, uint8_t *r, const char *s);
