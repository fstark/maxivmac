/*
	lomem_globals.cpp — Low-memory global table and helpers

	Complete table of ~160 Macintosh low-memory globals sorted by
	address, plus snapshot/diff and value-formatting utilities.
*/

#include "platform/lomem_globals.h"
#include "core/machine.h"
#include "platform/common/mac_roman.h"

#include <cstdio>
#include <cstring>

/* --- Category labels --- */

const char * const kLMCategoryLabels[] = {
	"System",
	"Hardware",
	"Timing",
	"Interrupt",
	"I/O",
	"Events",
	"Window",
	"Menu",
	"TextEdit",
	"Resource",
	"Scrap",
	"Printing",
	"Sound",
	"Font",
	"App",
	"Misc",
};
static_assert(sizeof(kLMCategoryLabels) / sizeof(kLMCategoryLabels[0]) == LM_CAT_COUNT,
	"kLMCategoryLabels must match LM_CAT_COUNT");

/* --- Global table (sorted by address) --- */

const LMGlobal kLowMemGlobals[] = {
	{ "ScrVRes",        0x0102, 2,  LM_WORD,    LM_CAT_HARDWARE,  "Pixels per inch vertically" },
	{ "ScrHRes",        0x0104, 2,  LM_WORD,    LM_CAT_HARDWARE,  "Pixels per inch horizontally" },
	{ "MemTop",         0x0108, 4,  LM_POINTER, LM_CAT_SYSTEM,    "Address of end of RAM" },
	{ "BufPtr",         0x010C, 4,  LM_POINTER, LM_CAT_SYSTEM,    "Address of end of jump table" },
	{ "HeapEnd",        0x0114, 4,  LM_POINTER, LM_CAT_SYSTEM,    "End of application heap zone" },
	{ "TheZone",        0x0118, 4,  LM_POINTER, LM_CAT_SYSTEM,    "Address of current heap zone" },
	{ "UTableBase",     0x011C, 4,  LM_POINTER, LM_CAT_IO,        "Base address of unit table" },
	{ "CPUFlag",        0x012F, 2,  LM_WORD,    LM_CAT_HARDWARE,  "Microprocessor in use" },
	{ "ApplLimit",      0x0130, 4,  LM_POINTER, LM_CAT_SYSTEM,    "Application heap limit" },
	{ "SysEvtMask",     0x0144, 2,  LM_WORD,    LM_CAT_EVENTS,    "System event mask" },
	{ "EventQueue",     0x014A, 10, LM_BYTES,   LM_CAT_EVENTS,    "Event queue header" },
	{ "RndSeed",        0x0156, 4,  LM_LONG,    LM_CAT_MISC,      "Random number seed" },
	{ "SEvtEnb",        0x015C, 1,  LM_BYTE,    LM_CAT_EVENTS,    "0 if SystemEvent returns FALSE" },
	{ "VBLQueue",       0x0160, 10, LM_BYTES,   LM_CAT_INTERRUPT,  "Vertical retrace queue header" },
	{ "Ticks",          0x016A, 4,  LM_LONG,    LM_CAT_TIMING,    "Tick count since startup" },
	{ "KeyThresh",      0x018E, 2,  LM_WORD,    LM_CAT_EVENTS,    "Auto-key threshold" },
	{ "KeyRepThresh",   0x0190, 2,  LM_WORD,    LM_CAT_EVENTS,    "Auto-key repeat rate" },
	{ "Lvl1DT",         0x0192, 32, LM_BYTES,   LM_CAT_INTERRUPT,  "Level-1 secondary interrupt vector table" },
	{ "Lvl2DT",         0x01B2, 32, LM_BYTES,   LM_CAT_INTERRUPT,  "Level-2 secondary interrupt vector table" },
	{ "SCCRd",          0x01D8, 4,  LM_POINTER, LM_CAT_HARDWARE,  "SCC read base address" },
	{ "VIA",            0x01DA, 4,  LM_POINTER, LM_CAT_HARDWARE,  "VIA base address" },
	{ "SCCWr",          0x01DC, 4,  LM_POINTER, LM_CAT_HARDWARE,  "SCC write base address" },
	{ "Scratch20",      0x01E4, 20, LM_BYTES,   LM_CAT_MISC,      "20-byte scratch area" },
	{ "SysParam",       0x01F8, 20, LM_BYTES,   LM_CAT_MISC,      "Low-memory copy of parameter RAM" },
	{ "SPValid",        0x01F8, 1,  LM_BYTE,    LM_CAT_MISC,      "Validity status" },
	{ "SPATalkA",       0x01F9, 1,  LM_BYTE,    LM_CAT_MISC,      "AppleTalk node ID hint for modem port" },
	{ "SPATalkB",       0x01FA, 1,  LM_BYTE,    LM_CAT_MISC,      "AppleTalk node ID hint for printer port" },
	{ "SPConfig",       0x01FB, 1,  LM_BYTE,    LM_CAT_MISC,      "Use types for serial ports" },
	{ "SPPortA",        0x01FC, 2,  LM_WORD,    LM_CAT_MISC,      "Modem port configuration" },
	{ "SPPortB",        0x01FE, 2,  LM_WORD,    LM_CAT_MISC,      "Printer port configuration" },
	{ "SPAlarm",        0x0200, 4,  LM_LONG,    LM_CAT_MISC,      "Alarm setting" },
	{ "SPFont",         0x0204, 2,  LM_WORD,    LM_CAT_FONT,      "Application font number minus 1" },
	{ "SPKbd",          0x0206, 1,  LM_BYTE,    LM_CAT_EVENTS,    "Auto-key threshold and rate" },
	{ "SPPrint",        0x0207, 1,  LM_BYTE,    LM_CAT_PRINTING,  "Printer connection" },
	{ "SPVolCtl",       0x0208, 1,  LM_BYTE,    LM_CAT_SOUND,     "Speaker volume in parameter RAM" },
	{ "SPClikCaret",    0x0209, 1,  LM_BYTE,    LM_CAT_MISC,      "Double-click and caret-blink times" },
	{ "SPMisc2",        0x020B, 1,  LM_BYTE,    LM_CAT_MISC,      "Mouse scaling, startup disk, menu blink" },
	{ "Time",           0x020C, 4,  LM_LONG,    LM_CAT_TIMING,    "Seconds since midnight, January 1, 1904" },
	{ "BootDrive",      0x0210, 2,  LM_WORD,    LM_CAT_IO,        "Working directory refnum for startup volume" },
	{ "SFSaveDisk",     0x0214, 2,  LM_WORD,    LM_CAT_IO,        "Negative of volume refnum for Standard File" },
	{ "KbdLast",        0x0218, 1,  LM_BYTE,    LM_CAT_EVENTS,    "ADB address of keyboard last used" },
	{ "KbdType",        0x021E, 1,  LM_BYTE,    LM_CAT_EVENTS,    "Keyboard type of keyboard last used" },
	{ "MemErr",         0x0220, 2,  LM_WORD,    LM_CAT_SYSTEM,    "Current value of MemError" },
	{ "SdVolume",       0x0260, 1,  LM_BYTE,    LM_CAT_SOUND,     "Current speaker volume (low 3 bits)" },
	{ "SoundPtr",       0x0262, 4,  LM_POINTER, LM_CAT_SOUND,     "Pointer to four-tone record" },
	{ "SoundBase",      0x0266, 4,  LM_POINTER, LM_CAT_SOUND,     "Pointer to free-form synthesizer buffer" },
	{ "SoundLevel",     0x027F, 1,  LM_BYTE,    LM_CAT_SOUND,     "Amplitude in 740-byte buffer" },
	{ "CurPitch",       0x0280, 2,  LM_WORD,    LM_CAT_SOUND,     "Count value in square-wave synthesizer buffer" },
	{ "ROM85",          0x028E, 2,  LM_WORD,    LM_CAT_HARDWARE,  "Version number of ROM" },
	{ "PortBUse",       0x0291, 1,  LM_BYTE,    LM_CAT_IO,        "Current availability of serial port B" },
	{ "SysZone",        0x02A6, 4,  LM_POINTER, LM_CAT_SYSTEM,    "Address of system heap zone" },
	{ "ApplZone",       0x02AA, 4,  LM_POINTER, LM_CAT_APP,       "Address of application heap zone" },
	{ "ROMBase",        0x02AE, 4,  LM_POINTER, LM_CAT_HARDWARE,  "Base address of ROM" },
	{ "RAMBase",        0x02B2, 4,  LM_POINTER, LM_CAT_HARDWARE,  "Trap dispatch table base for RAM routines" },
	{ "DSAlertTab",     0x02BA, 4,  LM_POINTER, LM_CAT_SYSTEM,    "Pointer to system error alert table" },
	{ "ExtStsDT",       0x02BE, 16, LM_BYTES,   LM_CAT_INTERRUPT,  "External/status interrupt vector table" },
	{ "ABusVars",       0x02D8, 4,  LM_POINTER, LM_CAT_IO,        "Pointer to AppleTalk variables" },
	{ "FinderName",     0x02E0, 16, LM_PSTRING, LM_CAT_APP,       "Name of Finder" },
	{ "DoubleTime",     0x02F0, 4,  LM_LONG,    LM_CAT_TIMING,    "Double-click interval in ticks" },
	{ "CaretTime",      0x02F4, 4,  LM_LONG,    LM_CAT_TIMING,    "Caret-blink interval in ticks" },
	{ "ScrDmpEnb",      0x02F8, 1,  LM_BYTE,    LM_CAT_EVENTS,    "0 if GetNextEvent ignores Cmd-Shift-number" },
	{ "BufTgFNum",      0x02FC, 4,  LM_LONG,    LM_CAT_IO,        "File tags: file number" },
	{ "BufTgFFlg",      0x0300, 2,  LM_WORD,    LM_CAT_IO,        "File tags: flags" },
	{ "BufTgFBkNum",    0x0302, 2,  LM_WORD,    LM_CAT_IO,        "File tags: logical block number" },
	{ "BufTgDate",      0x0304, 4,  LM_LONG,    LM_CAT_IO,        "File tags: date/time of last modification" },
	{ "DrvQHdr",        0x0308, 10, LM_BYTES,   LM_CAT_IO,        "Drive queue header" },
	{ "Lo3Bytes",       0x031A, 4,  LM_LONG,    LM_CAT_MISC,      "$00FFFFFF constant" },
	{ "MinStack",       0x031E, 4,  LM_LONG,    LM_CAT_SYSTEM,    "Minimum space allotment for stack" },
	{ "DefltStack",     0x0322, 4,  LM_LONG,    LM_CAT_SYSTEM,    "Default space allotment for stack" },
	{ "GZRootHnd",      0x0328, 4,  LM_HANDLE,  LM_CAT_SYSTEM,    "Handle to relocatable block not moved by grow zone" },
	{ "FCBSPtr",        0x034E, 4,  LM_POINTER, LM_CAT_IO,        "Pointer to file-control-block buffer" },
	{ "DefVCBPtr",      0x0352, 4,  LM_POINTER, LM_CAT_IO,        "Pointer to default volume control block" },
	{ "VCBQHdr",        0x0356, 10, LM_BYTES,   LM_CAT_IO,        "Volume-control-block queue header" },
	{ "FSQHdr",         0x0360, 10, LM_BYTES,   LM_CAT_IO,        "File I/O queue header" },
	{ "CurDirStore",    0x0398, 4,  LM_LONG,    LM_CAT_IO,        "Directory ID of directory last opened" },
	{ "ToExtFS",        0x03F2, 4,  LM_POINTER, LM_CAT_IO,        "Pointer to external file system" },
	{ "FSFCBLen",       0x03F6, 2,  LM_WORD,    LM_CAT_IO,        "Size of file control block" },
	{ "DSAlertRect",    0x03F8, 8,  LM_RECT,    LM_CAT_SYSTEM,    "Rectangle enclosing system error alert" },
	{ "JADBProc",       0x06B8, 4,  LM_POINTER, LM_CAT_INTERRUPT,  "Pointer to ADBReInit pre/post-processing" },
	{ "ScrnBase",       0x0824, 4,  LM_POINTER, LM_CAT_HARDWARE,  "Address of main screen buffer" },
	{ "MainDevice",     0x08A4, 4,  LM_HANDLE,  LM_CAT_HARDWARE,  "Handle to current main device" },
	{ "DeviceList",     0x08A8, 4,  LM_HANDLE,  LM_CAT_HARDWARE,  "Handle to first element in device list" },
	{ "QDColors",       0x08B0, 4,  LM_POINTER, LM_CAT_MISC,      "Default QuickDraw colors" },
	{ "JournalFlag",    0x08DE, 2,  LM_WORD,    LM_CAT_EVENTS,    "Journaling mode" },
	{ "WidthListHand",  0x08E4, 4,  LM_HANDLE,  LM_CAT_FONT,      "Handle to list of handles to width tables" },
	{ "JournalRef",     0x08E8, 2,  LM_WORD,    LM_CAT_EVENTS,    "Refnum of journaling device driver" },
	{ "CrsrThresh",     0x08EC, 2,  LM_WORD,    LM_CAT_EVENTS,    "Mouse-scaling threshold" },
	{ "JFetch",         0x08F4, 4,  LM_POINTER, LM_CAT_INTERRUPT,  "Jump vector for Fetch function" },
	{ "JStash",         0x08F8, 4,  LM_POINTER, LM_CAT_INTERRUPT,  "Jump vector for Stash function" },
	{ "JIODone",        0x08FC, 4,  LM_POINTER, LM_CAT_INTERRUPT,  "Jump vector for IODone function" },
	{ "CurApRefNum",    0x0900, 2,  LM_WORD,    LM_CAT_APP,       "Refnum of current application's resource file" },
	{ "CurrentA5",      0x0904, 4,  LM_POINTER, LM_CAT_APP,       "Boundary between app globals and app params" },
	{ "CurStackBase",   0x0908, 4,  LM_POINTER, LM_CAT_APP,       "Address of base of stack; start of app globals" },
	{ "CurApName",      0x0910, 32, LM_PSTRING, LM_CAT_APP,       "Name of current application" },
	{ "CurJTOffset",    0x0934, 2,  LM_WORD,    LM_CAT_APP,       "Offset to jump table from A5" },
	{ "CurPageOption",  0x0936, 2,  LM_WORD,    LM_CAT_APP,       "Sound/screen buffer config for Chain/Launch" },
	{ "HiliteMode",     0x0938, 2,  LM_WORD,    LM_CAT_MISC,      "Set if highlighting is on" },
	{ "PrintErr",       0x0944, 2,  LM_WORD,    LM_CAT_PRINTING,  "Result code from last Printing Manager routine" },
	{ "ScrapSize",      0x0960, 4,  LM_LONG,    LM_CAT_SCRAP,     "Size in bytes of desk scrap" },
	{ "ScrapHandle",    0x0964, 4,  LM_HANDLE,  LM_CAT_SCRAP,     "Handle to desk scrap in memory" },
	{ "ScrapCount",     0x0968, 2,  LM_WORD,    LM_CAT_SCRAP,     "Count changed by ZeroScrap" },
	{ "ScrapState",     0x096A, 2,  LM_WORD,    LM_CAT_SCRAP,     "Where desk scrap is" },
	{ "ScrapName",      0x096C, 4,  LM_POINTER, LM_CAT_SCRAP,     "Pointer to scrap file name" },
	{ "ROMFont0",       0x0980, 4,  LM_HANDLE,  LM_CAT_FONT,      "Handle to font record for system font" },
	{ "ApFontID",       0x0984, 2,  LM_WORD,    LM_CAT_FONT,      "Font number of application font" },
	{ "ToolScratch",    0x09CE, 8,  LM_BYTES,   LM_CAT_MISC,      "8-byte scratch area" },
	{ "SaveUpdate",     0x09DA, 2,  LM_WORD,    LM_CAT_WINDOW,    "Flag: generate update events" },
	{ "WindowList",     0x09D6, 4,  LM_POINTER, LM_CAT_WINDOW,    "Pointer to first window in window list" },
	{ "PaintWhite",     0x09DC, 2,  LM_WORD,    LM_CAT_WINDOW,    "Flag: paint window white before update event" },
	{ "WMgrPort",       0x09DE, 4,  LM_POINTER, LM_CAT_WINDOW,    "Pointer to Window Manager port" },
	{ "OldStructure",   0x09E6, 4,  LM_HANDLE,  LM_CAT_WINDOW,    "Handle to saved structure region" },
	{ "OldContent",     0x09EA, 4,  LM_HANDLE,  LM_CAT_WINDOW,    "Handle to saved content region" },
	{ "GrayRgn",        0x09EE, 4,  LM_HANDLE,  LM_CAT_WINDOW,    "Handle to region drawn as desktop" },
	{ "SaveVisRgn",     0x09F2, 4,  LM_HANDLE,  LM_CAT_WINDOW,    "Handle to saved visRgn" },
	{ "DragHook",       0x09F6, 4,  LM_POINTER, LM_CAT_WINDOW,    "Address of procedure during drag/track ops" },
	{ "Scratch8",       0x09FA, 8,  LM_BYTES,   LM_CAT_MISC,      "8-byte scratch area" },
	{ "OneOne",         0x0A02, 4,  LM_LONG,    LM_CAT_MISC,      "$00010001 constant" },
	{ "MinusOne",       0x0A06, 4,  LM_LONG,    LM_CAT_MISC,      "$FFFFFFFF constant" },
	{ "TopMenuItem",    0x0A0A, 2,  LM_WORD,    LM_CAT_MENU,      "Pixel value of top of scrollable menu" },
	{ "AtMenuBottom",   0x0A0C, 2,  LM_WORD,    LM_CAT_MENU,      "Flag for menu scrolling" },
	{ "MenuList",       0x0A1C, 4,  LM_HANDLE,  LM_CAT_MENU,      "Handle to current menu list" },
	{ "MBarEnable",     0x0A20, 2,  LM_WORD,    LM_CAT_MENU,      "Unique menu ID for active desk accessory" },
	{ "MenuFlash",      0x0A24, 2,  LM_WORD,    LM_CAT_MENU,      "Count for menu item blink duration" },
	{ "TheMenu",        0x0A26, 2,  LM_WORD,    LM_CAT_MENU,      "Menu ID of currently highlighted menu" },
	{ "MBarHook",       0x0A2C, 4,  LM_POINTER, LM_CAT_MENU,      "Routine called by MenuSelect before draw" },
	{ "MenuHook",       0x0A30, 4,  LM_POINTER, LM_CAT_MENU,      "Routine called during MenuSelect" },
	{ "DragPattern",    0x0A34, 8,  LM_PATTERN, LM_CAT_WINDOW,    "Pattern of dragged region outline" },
	{ "DeskPattern",    0x0A3C, 8,  LM_PATTERN, LM_CAT_WINDOW,    "Pattern for desktop painting" },
	{ "TopMapHndl",     0x0A50, 4,  LM_HANDLE,  LM_CAT_RESOURCE,  "Handle to resource map of most recently opened file" },
	{ "SysMapHndl",     0x0A54, 4,  LM_HANDLE,  LM_CAT_RESOURCE,  "Handle to map of system resource file" },
	{ "SysMap",         0x0A58, 2,  LM_WORD,    LM_CAT_RESOURCE,  "Refnum of system resource file" },
	{ "CurMap",         0x0A5A, 2,  LM_WORD,    LM_CAT_RESOURCE,  "Refnum of current resource file" },
	{ "ResLoad",        0x0A5E, 2,  LM_WORD,    LM_CAT_RESOURCE,  "Current SetResLoad state" },
	{ "ResErr",         0x0A60, 2,  LM_WORD,    LM_CAT_RESOURCE,  "Current value of ResError" },
	{ "FScaleDisable",  0x0A63, 1,  LM_BYTE,    LM_CAT_FONT,      "Nonzero to disable font scaling" },
	{ "CurActivate",    0x0A64, 4,  LM_POINTER, LM_CAT_WINDOW,    "Pointer to window to receive activate event" },
	{ "CurDeactive",    0x0A68, 4,  LM_POINTER, LM_CAT_WINDOW,    "Pointer to window to receive deactivate event" },
	{ "DeskHook",       0x0A6C, 4,  LM_POINTER, LM_CAT_WINDOW,    "Procedure for painting/clicking desktop" },
	{ "TEDoText",       0x0A70, 4,  LM_POINTER, LM_CAT_TEXTEDIT,  "Address of TextEdit multi-purpose routine" },
	{ "TERecal",        0x0A74, 4,  LM_POINTER, LM_CAT_TEXTEDIT,  "Routine to recalculate TE line starts" },
	{ "ApplScratch",    0x0A78, 12, LM_BYTES,   LM_CAT_APP,       "12-byte scratch area for applications" },
	{ "GhostWindow",    0x0A84, 4,  LM_POINTER, LM_CAT_WINDOW,    "Window never considered frontmost" },
	{ "ResumeProc",     0x0A8C, 4,  LM_POINTER, LM_CAT_SYSTEM,    "Address of resume procedure" },
	{ "ANumber",        0x0A98, 2,  LM_WORD,    LM_CAT_MISC,      "Resource ID of last alert" },
	{ "ACount",         0x0A9A, 2,  LM_WORD,    LM_CAT_MISC,      "Stage number (0-3) of last alert" },
	{ "DABeeper",       0x0A9C, 4,  LM_POINTER, LM_CAT_SOUND,     "Address of current sound procedure" },
	{ "DAStrings",      0x0AA0, 16, LM_BYTES,   LM_CAT_MISC,      "Handles to ParamText strings" },
	{ "TEScrpLength",   0x0AB0, 4,  LM_LONG,    LM_CAT_TEXTEDIT,  "Size in bytes of TextEdit scrap" },
	{ "TEScrpHandle",   0x0AB4, 4,  LM_HANDLE,  LM_CAT_TEXTEDIT,  "Handle to TextEdit scrap" },
	{ "SysResName",     0x0AD8, 20, LM_PSTRING, LM_CAT_RESOURCE,  "Name of system resource file" },
	{ "AppParmHandle",  0x0AEC, 4,  LM_HANDLE,  LM_CAT_APP,       "Handle to Finder information" },
	{ "DSErrCode",      0x0AF0, 2,  LM_WORD,    LM_CAT_SYSTEM,    "Current system error ID" },
	{ "ResErrProc",     0x0AF2, 4,  LM_POINTER, LM_CAT_RESOURCE,  "Address of resource error procedure" },
	{ "DlgFont",        0x0AFA, 2,  LM_WORD,    LM_CAT_FONT,      "Font number for dialogs and alerts" },
	{ "WidthPtr",       0x0B10, 4,  LM_POINTER, LM_CAT_FONT,      "Pointer to global width table" },
	{ "WidthTabHandle", 0x0B2A, 4,  LM_HANDLE,  LM_CAT_FONT,      "Handle to global width table" },
	{ "MenuDisable",    0x0B54, 4,  LM_LONG,    LM_CAT_MENU,      "Menu ID and item for selected disabled item" },
	{ "RomMapInsert",   0x0B9E, 1,  LM_BYTE,    LM_CAT_RESOURCE,  "Flag: insert map to ROM resources" },
	{ "TmpResLoad",     0x0B9F, 1,  LM_BYTE,    LM_CAT_RESOURCE,  "Temporary SetResLoad state" },
	{ "IntlSpec",       0x0BA0, 4,  LM_LONG,    LM_CAT_MISC,      "International software installed if != -1" },
	{ "SysFontFam",     0x0BA6, 2,  LM_WORD,    LM_CAT_FONT,      "Font number for system font (if nonzero)" },
	{ "SysFontSize",    0x0BA8, 2,  LM_WORD,    LM_CAT_FONT,      "Size of system font (if nonzero)" },
	{ "MBarHeight",     0x0BAA, 2,  LM_WORD,    LM_CAT_MENU,      "Height of menu bar" },
	{ "LastFOND",       0x0BC2, 4,  LM_HANDLE,  LM_CAT_FONT,      "Handle to last family record used" },
	{ "FractEnable",    0x0BF4, 1,  LM_BYTE,    LM_CAT_FONT,      "Nonzero to enable fractional widths" },
	{ "MMU32Bit",       0x0CB2, 1,  LM_BYTE,    LM_CAT_SYSTEM,    "Current address mode" },
	{ "TheGDevice",     0x0CC8, 4,  LM_HANDLE,  LM_CAT_HARDWARE,  "Handle to current active device" },
	{ "AuxWinHead",     0x0CD0, 4,  LM_LONG,    LM_CAT_WINDOW,    "Auxiliary window list header" },
	{ "TimeDBRA",       0x0D00, 2,  LM_WORD,    LM_CAT_TIMING,    "DBRA instructions per millisecond" },
	{ "TimeSCCDB",      0x0D02, 2,  LM_WORD,    LM_CAT_TIMING,    "SCC accesses per millisecond" },
	{ "JVBLTask",       0x0D28, 4,  LM_POINTER, LM_CAT_INTERRUPT,  "Jump vector for DoVBLTask routine" },
	{ "SynListHandle",  0x0D32, 4,  LM_HANDLE,  LM_CAT_FONT,      "Handle to synthetic font list" },
	{ "MenuCInfo",      0x0D50, 4,  LM_POINTER, LM_CAT_MENU,      "Header for menu color information table" },
	{ "DTQueue",        0x0D92, 10, LM_BYTES,   LM_CAT_INTERRUPT,  "Deferred task queue header" },
	{ "JDTInstall",     0x0D9C, 4,  LM_POINTER, LM_CAT_INTERRUPT,  "Jump vector for DTInstall routine" },
	{ "HiliteRGB",      0x0DA0, 6,  LM_BYTES,   LM_CAT_MISC,      "Default highlight color for system" },
	{ "TimeSCSIDB",     0x0DA6, 2,  LM_WORD,    LM_CAT_TIMING,    "SCSI accesses per millisecond" },
};

const int kLowMemCount = sizeof(kLowMemGlobals) / sizeof(kLowMemGlobals[0]);

/* --- Snapshot helpers --- */

static const uint32_t kSnapshotSize = 4096;

void lomem_snapshot_take(uint8_t *snapshot)
{
	if (!g_ram) return;
	memcpy(snapshot, g_ram, kSnapshotSize);
}

bool lomem_snapshot_changed(const uint8_t *snapshot, uint32_t addr, uint16_t size)
{
	if (addr + size > kSnapshotSize) return false;
	return memcmp(snapshot + addr, g_ram + addr, size) != 0;
}

/* --- Value formatting --- */

const char *lomem_type_label(LMType t)
{
	switch (t) {
	case LM_BYTE:    return "byte";
	case LM_WORD:    return "word";
	case LM_LONG:    return "long";
	case LM_POINTER: return "ptr";
	case LM_HANDLE:  return "hdl";
	case LM_PSTRING: return "str";
	case LM_BYTES:   return "bytes";
	case LM_RECT:    return "rect";
	case LM_PATTERN: return "pat";
	case LM_OSType:  return "ostp";
	}
	return "?";
}

static uint8_t rd8(uint32_t a)
{
	return g_ram[a];
}

static uint16_t rd16(uint32_t a)
{
	return ((uint16_t)g_ram[a] << 8) | g_ram[a + 1];
}

static uint32_t rd32(uint32_t a)
{
	return ((uint32_t)g_ram[a] << 24)
	     | ((uint32_t)g_ram[a + 1] << 16)
	     | ((uint32_t)g_ram[a + 2] << 8)
	     |  (uint32_t)g_ram[a + 3];
}

char *lomem_format_value(const LMGlobal *g, char *buf, int bufSize)
{
	if (!g_ram) {
		snprintf(buf, bufSize, "—");
		return buf;
	}

	uint32_t a = g->addr;

	switch (g->type) {
	case LM_BYTE:
		snprintf(buf, bufSize, "$%02X", rd8(a));
		break;
	case LM_WORD:
		snprintf(buf, bufSize, "$%04X", rd16(a));
		break;
	case LM_LONG:
		snprintf(buf, bufSize, "$%08X", rd32(a));
		break;
	case LM_POINTER:
		snprintf(buf, bufSize, "$%08X", rd32(a));
		break;
	case LM_HANDLE:
		snprintf(buf, bufSize, "$%08X (H)", rd32(a));
		break;
	case LM_OSType: {
		uint8_t c[4];
		for (int i = 0; i < 4; i++) c[i] = rd8(a + i);
		snprintf(buf, bufSize, "'%c%c%c%c'", c[0], c[1], c[2], c[3]);
		break;
	}
	case LM_PSTRING: {
		uint8_t len = rd8(a);
		if (len > 31) len = 31;
		/* Read MacRoman bytes then convert to UTF-8 */
		uint8_t raw[32];
		for (int i = 0; i < len; i++) raw[i] = rd8(a + 1 + i);
		uint32_t uLen = MacRoman2UniCodeSize(raw, len);
		if ((int)uLen + 3 > bufSize) uLen = bufSize - 3;
		buf[0] = '"';
		MacRoman2UniCodeData(raw, len, buf + 1);
		buf[1 + uLen] = '"';
		buf[2 + uLen] = '\0';
		break;
	}
	case LM_BYTES: {
		int pos = 0;
		int show = g->size > 16 ? 16 : g->size;
		for (int i = 0; i < show && pos + 4 < bufSize; i++) {
			if (i > 0) buf[pos++] = ' ';
			pos += snprintf(buf + pos, bufSize - pos, "%02X", rd8(a + i));
		}
		if (g->size > 16 && pos + 2 < bufSize) {
			buf[pos++] = ' ';
			buf[pos++] = '\xE2'; buf[pos++] = '\x80'; buf[pos++] = '\xA6'; /* … */
		}
		buf[pos] = '\0';
		break;
	}
	case LM_RECT: {
		int16_t top    = (int16_t)rd16(a);
		int16_t left   = (int16_t)rd16(a + 2);
		int16_t bottom = (int16_t)rd16(a + 4);
		int16_t right  = (int16_t)rd16(a + 6);
		snprintf(buf, bufSize, "(%d,%d)-(%d,%d)", top, left, bottom, right);
		break;
	}
	case LM_PATTERN: {
		int pos = 0;
		for (int i = 0; i < 8 && pos + 4 < bufSize; i++) {
			if (i > 0) buf[pos++] = ' ';
			pos += snprintf(buf + pos, bufSize - pos, "%02X", rd8(a + i));
		}
		buf[pos] = '\0';
		break;
	}
	}

	return buf;
}
