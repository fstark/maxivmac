
/*
 *  ScriptMgr.h
 *
 *  Copyright (c) 1988 Symantec Corporation.
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985, 1986, 1987, 1988.
 *
 */

#ifndef _ScriptMgr_
#define _ScriptMgr_


#define smgrVers		0x0155


/* Script interface identification numbers */

enum {
	smRoman,
	smJapanese,
	smChinese,
	smKorean,
	smArabic,
	smHebrew,
	smGreek,
	smRussian,
	smRSymbol,
	smDevanagari,
	smGurmukhi,
	smGujarati,
	smOriya,
	smBengali,
	smTamil,
	smTelugu,
	smKannada,
	smMalayalam,
	smSinhalese,
	smBurmese,
	smKhmer,
	smThai,
	smLaotian,
	smGeorgian,
	smArmenian,
	smMaldivian,
	smTibetan,
	smMongolian,
	smAmharic,
	smSlavic,
	smVietnamese,
	smSindhi,
	smReserved2
};

/* Language Codes */

enum {
	langEnglish,
	langFrench,
	langGerman,
	langItalian,
	langDutch,
	langSwedish,
	langSpanish,
	langDanish,
	langPortugese,
	langNorwegian,
	langHebrew,
	langJapanese,
	langArabic,
	langFinish,
	langGreek,
	langIcelandic,
	langMalta,
	langTurkish,
	langYugoslavian,
	langChinese,
	langUrdu,
	langHindi,
	langThai
};

/* Calendar Codes */

enum {
	calGregorian,
	calArabicCivil,
	calArabicLunar,
	calJapanese,
	calJewish,
	calCoptic
};

/* Integer Format Codes */

enum {
	intWestern,
	intArabic,
	intRoman,
	intJapanese
};

/* CharByte return values. */

enum {
	smFirstByte = -1,
	smSingleByte,
	smLastByte,
	smMiddleByte
};

/* CharType field masks */

#define smcTypeMask			0x000F
#define smcReserved			0x00F0
#define smcClassMask		0x0F00
#define smcReserved12		0x1000
#define smcRightMask		0x2000
#define smcUpperMask		0x4000
#define smcDoubleMask		0x8000

/* CharType character types. */

enum {
	smCharPunct,
	smCharAscii,
	smCharEuro = 7
};

/* CharType punctuation types. */

#define smPunctNormal        0x0000
#define smPunctNumber        0x0100
#define smPunctSymbol        0x0200
#define smPunctBlank		 0x0300

/* CharType case modifers. */

#define smCharLower          0x0000
#define smCharUpper          0x4000

/* CharType character size modifiers (1 or 2 bytes). */
	
#define smChar1byte          0x0000
#define smChar2byte          0x8000


/* Char2Pixel directions. */
enum {
	smRightCaret = -1,
	smLeftCaret,
	smHilite
};
   

/* Transliterate target types. */

enum {
	smTransAscii,
	smTransNative
};

/* Transliterate target modifiers. */

#define smTransLower         0x4000
#define smTransUpper         0x8000

/* Transliterate source masks. */

#define smMaskAscii          0x0001
#define smMaskNative         0x0002

	
/* Result values from GetEnvirons, SetEnvirons, GetScript and SetScript calls. */

enum {
	smBadScript = -2,
	smBadVerb,
	smNotInstalled
};

/* GetEnvirons/SetEnvirons verbs. */

enum {
	smVersion = 0,
	smMunged = 2,
	smEnabled = 4,
	smBidirect = 6,
	smFontForce = 8,
	smIntlForce = 10,
	smForced = 12,
	smDefault = 14,
	smPrint = 16,
	smSysScript = 18,
	smLastScript = 20,
	smKeyScript = 22,
	smSysRef = 24,
	smKeyCache = 26,
	smKeySwap = 28
};
	

/* GetScript/SetScript verbs. */

enum {
	smScriptVersion = 0,
	smScriptMunged = 2,
	smScriptEnabled = 4,
	smScriptRight = 6,
	smScriptJust = 8,
	smScriptRedraw = 10,
	smScriptSysFond = 12,
	smScriptAppFond = 14,
	smScriptBundle = 16,
	smScriptNumber = 16,
	smScriptDate = 18,
	smScriptSort = 20,
	smScriptRsvd1 = 22,
	smScriptRsvd2 = 24,
	smScriptRsvd3 = 26,
	smScriptRsvd4 = 28,
	smScriptRsvd5 = 30,
	smScriptKeys = 32,
	smScriptIcon = 34,
	smScriptPrint = 36,
	smScriptTrap = 38,
	smScriptCreator = 40,
	smScriptFile = 42,
	smScriptName = 44
};
	
/* Bits in the smScriptFlags word */

enum {
	smsfIntellCP,
	smsfSingByte,
	smsfNatCase,
	smsfContext
};

/* Roman script constants */

#define romanVers			1
#define romanSysFond		0x3FFF
#define romanAppFond		3

/* Script Manager font equates. */

#define smFondStart			0x4000
#define smFondEnd			0xC000

	

typedef short OffsetTable[6];

typedef struct BreakTable     {
	char		charTypes[256];
	short		tripleLength;
	short		triples[1];
	} BreakTable, *BreakTablePtr;


/* Bundle declarations */

typedef struct ItlcRecord {
	short		itlcSystem;
	short		itlcReserved;
	char		itlcFontForce;
	char		itlcIntlForce;
	} ItlcRecord;

typedef struct ItlbRecord {
	short		itlbNumber;
	short		itlbDate;
	short		itlbSort;
	short		itlbReserved1;
	short		itlbReserved2;
	short		itlbReserved3;
	short		itlbLang;
	char		itlbNumRep;
	char		itlbDateRep;
	short		itlbKeys;
	short		itlbIcon;
	} ItlbRecord;


#endif _ScriptMgr_