> **Note (2026-04):** The ImGui LowMemTool was removed with developer mode.
> This document is kept as historical reference.

# Low-Memory Globals Viewer — Implementation Plan

Design: `docs/features/LOW_MEMORY_DESIGN.md`

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Data model: enums, struct, static table (~160 entries) | Done |
| 2 | Snapshot helpers and value formatting | Done |
| 3 | ImGui tool panel (draw, filter, Mark/Clear) | Done |
| 4 | Registration, build integration, smoke test | Done |

Build gate: `cmake --build bld/macos-imgui && cmake --build bld/macos-headless`
Test gate:  `cd test && ./verify.sh`

---

## Phase 1 — Data model

Create `src/platform/lomem_globals.h` and `src/platform/lomem_globals.cpp`
with the type/category enums, the `LMGlobal` descriptor struct, and the
complete static table of low-memory globals.

### 1.1 — Create `src/platform/lomem_globals.h`

New file with:

```cpp
/*
    lomem_globals.h — Macintosh low-memory global variable descriptors
*/

#pragma once

#include <cstdint>

enum LMType {
    LM_BYTE,       /*  1 byte  */
    LM_WORD,       /*  2 bytes */
    LM_LONG,       /*  4 bytes */
    LM_POINTER,    /*  4 bytes, displayed as address */
    LM_HANDLE,     /*  4 bytes, displayed as handle */
    LM_PSTRING,    /* length-prefixed Pascal string */
    LM_BYTES,      /* raw byte array */
    LM_RECT,       /*  8 bytes: top, left, bottom, right */
    LM_PATTERN,    /*  8 bytes pattern */
    LM_OSType,     /*  4 bytes four-char code */
};

enum LMCategory {
    LM_CAT_SYSTEM,
    LM_CAT_HARDWARE,
    LM_CAT_TIMING,
    LM_CAT_INTERRUPT,
    LM_CAT_IO,
    LM_CAT_EVENTS,
    LM_CAT_WINDOW,
    LM_CAT_MENU,
    LM_CAT_TEXTEDIT,
    LM_CAT_RESOURCE,
    LM_CAT_SCRAP,
    LM_CAT_PRINTING,
    LM_CAT_SOUND,
    LM_CAT_FONT,
    LM_CAT_APP,
    LM_CAT_MISC,
    LM_CAT_COUNT
};

struct LMGlobal {
    const char *name;
    uint32_t    addr;
    uint16_t    size;
    LMType      type;
    LMCategory  category;
    const char *brief;
};

/* Full table (sorted by address). */
extern const LMGlobal kLowMemGlobals[];
extern const int      kLowMemCount;

/* Human-readable category labels (LM_CAT_COUNT entries). */
extern const char * const kLMCategoryLabels[];
```

### 1.2 — Create `src/platform/lomem_globals.cpp`

New file containing the static table.  Every entry comes from
`macdocs/ref/GLOBAL_VARS.md`.  Each gets a manually assigned `LMType`
and `LMCategory` based on the "Contents" column.

Type assignment rules (from the description text):
- "word" → `LM_WORD`
- "byte" → `LM_BYTE`
- "long" → `LM_LONG`
- "Address of …" / "Pointer to …" / "… base address" → `LM_POINTER`, size 4
- "Handle to …" → `LM_HANDLE`, size 4
- "length byte + up to N chars" → `LM_PSTRING`, size = 1 + max chars
- "N-byte scratch/table" → `LM_BYTES`, size = N
- "Rectangle …" / 8 bytes rect → `LM_RECT`, size 8
- "Pattern …" / 8 bytes pattern → `LM_PATTERN`, size 8
- Otherwise default: if no size hint, infer `LM_LONG` size 4

Category assignment rules:
- Heap/zone/stack/memory → `LM_CAT_SYSTEM`
- VIA/SCC/IWM/SCSI/Screen/ROM addresses → `LM_CAT_HARDWARE`
- Ticks/Time/DBRA calibration → `LM_CAT_TIMING`
- Lvl*DT/VBL/interrupt vectors → `LM_CAT_INTERRUPT`
- File system (FCB/VCB/Drv/FS) → `LM_CAT_IO`
- Event queue/journal/keyboard/mouse → `LM_CAT_EVENTS`
- Window* / WMgr / paint / GrayRgn → `LM_CAT_WINDOW`
- Menu* / MBar* → `LM_CAT_MENU`
- TE* / TextEdit → `LM_CAT_TEXTEDIT`
- Res*/Map/SysMap → `LM_CAT_RESOURCE`
- Scrap* → `LM_CAT_SCRAP`
- Print* → `LM_CAT_PRINTING`
- Sound*/Sd*/Beeper → `LM_CAT_SOUND`
- Font*/FOND/Width* → `LM_CAT_FONT`
- CurAp*/CurrentA5/ApplZone/AppParm → `LM_CAT_APP`
- Everything else → `LM_CAT_MISC`

The table must be sorted by address (ascending).  Here is the complete
entry list — all ~160 globals from GLOBAL_VARS.md:

```cpp
const LMGlobal kLowMemGlobals[] = {
    // --- SYSTEM ---
    { "ScrVRes",       0x0102, 2, LM_WORD,    LM_CAT_HARDWARE, "Pixels per inch vertically" },
    { "ScrHRes",       0x0104, 2, LM_WORD,    LM_CAT_HARDWARE, "Pixels per inch horizontally" },
    { "MemTop",        0x0108, 4, LM_POINTER, LM_CAT_SYSTEM,   "Address of end of RAM" },
    { "BufPtr",        0x010C, 4, LM_POINTER, LM_CAT_SYSTEM,   "Address of end of jump table" },
    { "HeapEnd",       0x0114, 4, LM_POINTER, LM_CAT_SYSTEM,   "End of application heap zone" },
    { "TheZone",       0x0118, 4, LM_POINTER, LM_CAT_SYSTEM,   "Address of current heap zone" },
    { "UTableBase",    0x011C, 4, LM_POINTER, LM_CAT_IO,       "Base address of unit table" },
    { "CPUFlag",       0x012F, 2, LM_WORD,    LM_CAT_HARDWARE, "Microprocessor in use" },
    { "ApplLimit",     0x0130, 4, LM_POINTER, LM_CAT_SYSTEM,   "Application heap limit" },
    { "SysEvtMask",    0x0144, 2, LM_WORD,    LM_CAT_EVENTS,   "System event mask" },
    { "EventQueue",    0x014A, 10,LM_BYTES,   LM_CAT_EVENTS,   "Event queue header" },
    { "RndSeed",       0x0156, 4, LM_LONG,    LM_CAT_MISC,     "Random number seed" },
    { "SEvtEnb",       0x015C, 1, LM_BYTE,    LM_CAT_EVENTS,   "0 if SystemEvent returns FALSE" },
    { "VBLQueue",      0x0160, 10,LM_BYTES,   LM_CAT_INTERRUPT, "Vertical retrace queue header" },
    { "Ticks",         0x016A, 4, LM_LONG,    LM_CAT_TIMING,   "Tick count since startup" },
    { "KeyThresh",     0x018E, 2, LM_WORD,    LM_CAT_EVENTS,   "Auto-key threshold" },
    { "KeyRepThresh",  0x0190, 2, LM_WORD,    LM_CAT_EVENTS,   "Auto-key repeat rate" },
    { "Lvl1DT",        0x0192, 32,LM_BYTES,   LM_CAT_INTERRUPT, "Level-1 secondary interrupt vector table" },
    { "Lvl2DT",        0x01B2, 32,LM_BYTES,   LM_CAT_INTERRUPT, "Level-2 secondary interrupt vector table" },
    { "SCCRd",         0x01D8, 4, LM_POINTER, LM_CAT_HARDWARE, "SCC read base address" },
    { "VIA",           0x01DA, 4, LM_POINTER, LM_CAT_HARDWARE, "VIA base address" },
    { "SCCWr",         0x01DC, 4, LM_POINTER, LM_CAT_HARDWARE, "SCC write base address" },
    { "Scratch20",     0x01E4, 20,LM_BYTES,   LM_CAT_MISC,     "20-byte scratch area" },
    { "SPValid",       0x01F8, 1, LM_BYTE,    LM_CAT_MISC,     "Validity status" },
    { "SPATalkA",      0x01F9, 1, LM_BYTE,    LM_CAT_MISC,     "AppleTalk node ID hint for modem port" },
    { "SPATalkB",      0x01FA, 1, LM_BYTE,    LM_CAT_MISC,     "AppleTalk node ID hint for printer port" },
    { "SPConfig",      0x01FB, 1, LM_BYTE,    LM_CAT_MISC,     "Use types for serial ports" },
    { "SPPortA",       0x01FC, 2, LM_WORD,    LM_CAT_MISC,     "Modem port configuration" },
    { "SPPortB",       0x01FE, 2, LM_WORD,    LM_CAT_MISC,     "Printer port configuration" },
    { "SPAlarm",       0x0200, 4, LM_LONG,    LM_CAT_MISC,     "Alarm setting" },
    { "SPFont",        0x0204, 2, LM_WORD,    LM_CAT_FONT,     "Application font number minus 1" },
    { "SPKbd",         0x0206, 1, LM_BYTE,    LM_CAT_EVENTS,   "Auto-key threshold and rate" },
    { "SPPrint",       0x0207, 1, LM_BYTE,    LM_CAT_PRINTING, "Printer connection" },
    { "SPVolCtl",      0x0208, 1, LM_BYTE,    LM_CAT_SOUND,    "Speaker volume in parameter RAM" },
    { "SPClikCaret",   0x0209, 1, LM_BYTE,    LM_CAT_MISC,     "Double-click and caret-blink times" },
    { "SPMisc2",       0x020B, 1, LM_BYTE,    LM_CAT_MISC,     "Mouse scaling, startup disk, menu blink" },
    { "Time",          0x020C, 4, LM_LONG,    LM_CAT_TIMING,   "Seconds since midnight, January 1, 1904" },
    { "BootDrive",     0x0210, 2, LM_WORD,    LM_CAT_IO,       "Working directory refnum for startup volume" },
    { "SFSaveDisk",    0x0214, 2, LM_WORD,    LM_CAT_IO,       "Negative of volume refnum for Standard File" },
    { "KbdLast",       0x0218, 1, LM_BYTE,    LM_CAT_EVENTS,   "ADB address of keyboard last used" },
    { "KbdType",       0x021E, 1, LM_BYTE,    LM_CAT_EVENTS,   "Keyboard type of keyboard last used" },
    { "MemErr",        0x0220, 2, LM_WORD,    LM_CAT_SYSTEM,   "Current value of MemError" },
    { "SdVolume",      0x0260, 1, LM_BYTE,    LM_CAT_SOUND,    "Current speaker volume (low 3 bits)" },
    { "SoundPtr",      0x0262, 4, LM_POINTER, LM_CAT_SOUND,    "Pointer to four-tone record" },
    { "SoundBase",     0x0266, 4, LM_POINTER, LM_CAT_SOUND,    "Pointer to free-form synthesizer buffer" },
    { "SoundLevel",    0x027F, 1, LM_BYTE,    LM_CAT_SOUND,    "Amplitude in 740-byte buffer" },
    { "CurPitch",      0x0280, 2, LM_WORD,    LM_CAT_SOUND,    "Count value in square-wave synthesizer buffer" },
    { "ROM85",         0x028E, 2, LM_WORD,    LM_CAT_HARDWARE, "Version number of ROM" },
    { "PortBUse",      0x0291, 1, LM_BYTE,    LM_CAT_IO,       "Current availability of serial port B" },
    { "SysZone",       0x02A6, 4, LM_POINTER, LM_CAT_SYSTEM,   "Address of system heap zone" },
    { "ApplZone",      0x02AA, 4, LM_POINTER, LM_CAT_APP,      "Address of application heap zone" },
    { "ROMBase",       0x02AE, 4, LM_POINTER, LM_CAT_HARDWARE, "Base address of ROM" },
    { "RAMBase",       0x02B2, 4, LM_POINTER, LM_CAT_HARDWARE, "Trap dispatch table base for RAM routines" },
    { "DSAlertTab",    0x02BA, 4, LM_POINTER, LM_CAT_SYSTEM,   "Pointer to system error alert table" },
    { "ExtStsDT",      0x02BE, 16,LM_BYTES,   LM_CAT_INTERRUPT, "External/status interrupt vector table" },
    { "ABusVars",      0x02D8, 4, LM_POINTER, LM_CAT_IO,       "Pointer to AppleTalk variables" },
    { "FinderName",    0x02E0, 16,LM_PSTRING, LM_CAT_APP,      "Name of Finder" },
    { "DoubleTime",    0x02F0, 4, LM_LONG,    LM_CAT_TIMING,   "Double-click interval in ticks" },
    { "CaretTime",     0x02F4, 4, LM_LONG,    LM_CAT_TIMING,   "Caret-blink interval in ticks" },
    { "ScrDmpEnb",     0x02F8, 1, LM_BYTE,    LM_CAT_EVENTS,   "0 if GetNextEvent ignores Cmd-Shift-number" },
    { "BufTgFNum",     0x02FC, 4, LM_LONG,    LM_CAT_IO,       "File tags: file number" },
    { "BufTgFFlg",     0x0300, 2, LM_WORD,    LM_CAT_IO,       "File tags: flags" },
    { "BufTgFBkNum",   0x0302, 2, LM_WORD,    LM_CAT_IO,       "File tags: logical block number" },
    { "BufTgDate",     0x0304, 4, LM_LONG,    LM_CAT_IO,       "File tags: date/time of last modification" },
    { "DrvQHdr",       0x0308, 10,LM_BYTES,   LM_CAT_IO,       "Drive queue header" },
    { "Lo3Bytes",      0x031A, 4, LM_LONG,    LM_CAT_MISC,     "$00FFFFFF constant" },
    { "MinStack",      0x031E, 4, LM_LONG,    LM_CAT_SYSTEM,   "Minimum space allotment for stack" },
    { "DefltStack",    0x0322, 4, LM_LONG,    LM_CAT_SYSTEM,   "Default space allotment for stack" },
    { "GZRootHnd",     0x0328, 4, LM_HANDLE,  LM_CAT_SYSTEM,   "Handle to relocatable block not moved by grow zone" },
    { "FCBSPtr",       0x034E, 4, LM_POINTER, LM_CAT_IO,       "Pointer to file-control-block buffer" },
    { "DefVCBPtr",     0x0352, 4, LM_POINTER, LM_CAT_IO,       "Pointer to default volume control block" },
    { "VCBQHdr",       0x0356, 10,LM_BYTES,   LM_CAT_IO,       "Volume-control-block queue header" },
    { "FSQHdr",        0x0360, 10,LM_BYTES,   LM_CAT_IO,       "File I/O queue header" },
    { "ToExtFS",       0x03F2, 4, LM_POINTER, LM_CAT_IO,       "Pointer to external file system" },
    { "FSFCBLen",      0x03F6, 2, LM_WORD,    LM_CAT_IO,       "Size of file control block" },
    { "DSAlertRect",   0x03F8, 8, LM_RECT,    LM_CAT_SYSTEM,   "Rectangle enclosing system error alert" },
    { "CurDirStore",   0x0398, 4, LM_LONG,    LM_CAT_IO,       "Directory ID of directory last opened" },
    { "JADBProc",      0x06B8, 4, LM_POINTER, LM_CAT_INTERRUPT, "Pointer to ADBReInit pre/post-processing" },
    { "ScrnBase",      0x0824, 4, LM_POINTER, LM_CAT_HARDWARE, "Address of main screen buffer" },
    { "MainDevice",    0x08A4, 4, LM_HANDLE,  LM_CAT_HARDWARE, "Handle to current main device" },
    { "DeviceList",    0x08A8, 4, LM_HANDLE,  LM_CAT_HARDWARE, "Handle to first element in device list" },
    { "QDColors",      0x08B0, 4, LM_POINTER, LM_CAT_MISC,     "Default QuickDraw colors" },
    { "JournalFlag",   0x08DE, 2, LM_WORD,    LM_CAT_EVENTS,   "Journaling mode" },
    { "WidthListHand", 0x08E4, 4, LM_HANDLE,  LM_CAT_FONT,     "Handle to list of handles to width tables" },
    { "JournalRef",    0x08E8, 2, LM_WORD,    LM_CAT_EVENTS,   "Refnum of journaling device driver" },
    { "CrsrThresh",    0x08EC, 2, LM_WORD,    LM_CAT_EVENTS,   "Mouse-scaling threshold" },
    { "JFetch",        0x08F4, 4, LM_POINTER, LM_CAT_INTERRUPT, "Jump vector for Fetch function" },
    { "JStash",        0x08F8, 4, LM_POINTER, LM_CAT_INTERRUPT, "Jump vector for Stash function" },
    { "JIODone",       0x08FC, 4, LM_POINTER, LM_CAT_INTERRUPT, "Jump vector for IODone function" },
    { "CurApRefNum",   0x0900, 2, LM_WORD,    LM_CAT_APP,      "Refnum of current application's resource file" },
    { "CurrentA5",     0x0904, 4, LM_POINTER, LM_CAT_APP,      "Boundary between app globals and app params" },
    { "CurStackBase",  0x0908, 4, LM_POINTER, LM_CAT_APP,      "Address of base of stack; start of app globals" },
    { "CurApName",     0x0910, 32,LM_PSTRING, LM_CAT_APP,      "Name of current application" },
    { "CurJTOffset",   0x0934, 2, LM_WORD,    LM_CAT_APP,      "Offset to jump table from A5" },
    { "CurPageOption", 0x0936, 2, LM_WORD,    LM_CAT_APP,      "Sound/screen buffer config for Chain/Launch" },
    { "HiliteMode",    0x0938, 2, LM_WORD,    LM_CAT_MISC,     "Set if highlighting is on" },
    { "PrintErr",      0x0944, 2, LM_WORD,    LM_CAT_PRINTING, "Result code from last Printing Manager routine" },
    { "ScrapSize",     0x0960, 4, LM_LONG,    LM_CAT_SCRAP,    "Size in bytes of desk scrap" },
    { "ScrapHandle",   0x0964, 4, LM_HANDLE,  LM_CAT_SCRAP,    "Handle to desk scrap in memory" },
    { "ScrapCount",    0x0968, 2, LM_WORD,    LM_CAT_SCRAP,    "Count changed by ZeroScrap" },
    { "ScrapState",    0x096A, 2, LM_WORD,    LM_CAT_SCRAP,    "Where desk scrap is" },
    { "ScrapName",     0x096C, 4, LM_POINTER, LM_CAT_SCRAP,    "Pointer to scrap file name" },
    { "ROMFont0",      0x0980, 4, LM_HANDLE,  LM_CAT_FONT,     "Handle to font record for system font" },
    { "ApFontID",      0x0984, 2, LM_WORD,    LM_CAT_FONT,     "Font number of application font" },
    { "SaveUpdate",    0x09DA, 2, LM_WORD,    LM_CAT_WINDOW,   "Flag: generate update events" },
    { "PaintWhite",    0x09DC, 2, LM_WORD,    LM_CAT_WINDOW,   "Flag: paint window white before update event" },
    { "WindowList",    0x09D6, 4, LM_POINTER, LM_CAT_WINDOW,   "Pointer to first window in window list" },
    { "WMgrPort",      0x09DE, 4, LM_POINTER, LM_CAT_WINDOW,   "Pointer to Window Manager port" },
    { "OldStructure",  0x09E6, 4, LM_HANDLE,  LM_CAT_WINDOW,   "Handle to saved structure region" },
    { "OldContent",    0x09EA, 4, LM_HANDLE,  LM_CAT_WINDOW,   "Handle to saved content region" },
    { "GrayRgn",       0x09EE, 4, LM_HANDLE,  LM_CAT_WINDOW,   "Handle to region drawn as desktop" },
    { "SaveVisRgn",    0x09F2, 4, LM_HANDLE,  LM_CAT_WINDOW,   "Handle to saved visRgn" },
    { "DragHook",      0x09F6, 4, LM_POINTER, LM_CAT_WINDOW,   "Address of procedure during drag/track ops" },
    { "Scratch8",      0x09FA, 8, LM_BYTES,   LM_CAT_MISC,     "8-byte scratch area" },
    { "ToolScratch",   0x09CE, 8, LM_BYTES,   LM_CAT_MISC,     "8-byte scratch area" },
    { "OneOne",        0x0A02, 4, LM_LONG,    LM_CAT_MISC,     "$00010001 constant" },
    { "MinusOne",      0x0A06, 4, LM_LONG,    LM_CAT_MISC,     "$FFFFFFFF constant" },
    { "TopMenuItem",   0x0A0A, 2, LM_WORD,    LM_CAT_MENU,     "Pixel value of top of scrollable menu" },
    { "AtMenuBottom",  0x0A0C, 2, LM_WORD,    LM_CAT_MENU,     "Flag for menu scrolling" },
    { "MenuList",      0x0A1C, 4, LM_HANDLE,  LM_CAT_MENU,     "Handle to current menu list" },
    { "MBarEnable",    0x0A20, 2, LM_WORD,    LM_CAT_MENU,     "Unique menu ID for active desk accessory" },
    { "MenuFlash",     0x0A24, 2, LM_WORD,    LM_CAT_MENU,     "Count for menu item blink duration" },
    { "TheMenu",       0x0A26, 2, LM_WORD,    LM_CAT_MENU,     "Menu ID of currently highlighted menu" },
    { "MBarHook",      0x0A2C, 4, LM_POINTER, LM_CAT_MENU,     "Routine called by MenuSelect before draw" },
    { "MenuHook",      0x0A30, 4, LM_POINTER, LM_CAT_MENU,     "Routine called during MenuSelect" },
    { "DragPattern",   0x0A34, 8, LM_PATTERN, LM_CAT_WINDOW,   "Pattern of dragged region outline" },
    { "DeskPattern",   0x0A3C, 8, LM_PATTERN, LM_CAT_WINDOW,   "Pattern for desktop painting" },
    { "TopMapHndl",    0x0A50, 4, LM_HANDLE,  LM_CAT_RESOURCE, "Handle to resource map of most recently opened file" },
    { "SysMapHndl",    0x0A54, 4, LM_HANDLE,  LM_CAT_RESOURCE, "Handle to map of system resource file" },
    { "SysMap",        0x0A58, 2, LM_WORD,    LM_CAT_RESOURCE, "Refnum of system resource file" },
    { "CurMap",        0x0A5A, 2, LM_WORD,    LM_CAT_RESOURCE, "Refnum of current resource file" },
    { "ResLoad",       0x0A5E, 2, LM_WORD,    LM_CAT_RESOURCE, "Current SetResLoad state" },
    { "ResErr",        0x0A60, 2, LM_WORD,    LM_CAT_RESOURCE, "Current value of ResError" },
    { "FScaleDisable", 0x0A63, 1, LM_BYTE,    LM_CAT_FONT,     "Nonzero to disable font scaling" },
    { "CurActivate",   0x0A64, 4, LM_POINTER, LM_CAT_WINDOW,   "Pointer to window to receive activate event" },
    { "CurDeactive",   0x0A68, 4, LM_POINTER, LM_CAT_WINDOW,   "Pointer to window to receive deactivate event" },
    { "DeskHook",      0x0A6C, 4, LM_POINTER, LM_CAT_WINDOW,   "Procedure for painting/clicking desktop" },
    { "TEDoText",      0x0A70, 4, LM_POINTER, LM_CAT_TEXTEDIT, "Address of TextEdit multi-purpose routine" },
    { "TERecal",       0x0A74, 4, LM_POINTER, LM_CAT_TEXTEDIT, "Routine to recalculate TE line starts" },
    { "ApplScratch",   0x0A78, 12,LM_BYTES,   LM_CAT_APP,      "12-byte scratch area for applications" },
    { "GhostWindow",   0x0A84, 4, LM_POINTER, LM_CAT_WINDOW,   "Window never considered frontmost" },
    { "ResumeProc",    0x0A8C, 4, LM_POINTER, LM_CAT_SYSTEM,   "Address of resume procedure" },
    { "ANumber",       0x0A98, 2, LM_WORD,    LM_CAT_MISC,     "Resource ID of last alert" },
    { "ACount",        0x0A9A, 2, LM_WORD,    LM_CAT_MISC,     "Stage number (0-3) of last alert" },
    { "DABeeper",      0x0A9C, 4, LM_POINTER, LM_CAT_SOUND,    "Address of current sound procedure" },
    { "DAStrings",     0x0AA0, 16,LM_BYTES,   LM_CAT_MISC,     "Handles to ParamText strings" },
    { "TEScrpLength",  0x0AB0, 4, LM_LONG,    LM_CAT_TEXTEDIT, "Size in bytes of TextEdit scrap" },
    { "TEScrpHandle",  0x0AB4, 4, LM_HANDLE,  LM_CAT_TEXTEDIT, "Handle to TextEdit scrap" },
    { "SysResName",    0x0AD8, 20,LM_PSTRING, LM_CAT_RESOURCE, "Name of system resource file" },
    { "AppParmHandle", 0x0AEC, 4, LM_HANDLE,  LM_CAT_APP,      "Handle to Finder information" },
    { "DSErrCode",     0x0AF0, 2, LM_WORD,    LM_CAT_SYSTEM,   "Current system error ID" },
    { "ResErrProc",    0x0AF2, 4, LM_POINTER, LM_CAT_RESOURCE, "Address of resource error procedure" },
    { "DlgFont",       0x0AFA, 2, LM_WORD,    LM_CAT_FONT,     "Font number for dialogs and alerts" },
    { "WidthPtr",      0x0B10, 4, LM_POINTER, LM_CAT_FONT,     "Pointer to global width table" },
    { "WidthTabHandle",0x0B2A, 4, LM_HANDLE,  LM_CAT_FONT,     "Handle to global width table" },
    { "MenuDisable",   0x0B54, 4, LM_LONG,    LM_CAT_MENU,     "Menu ID and item for selected disabled item" },
    { "RomMapInsert",  0x0B9E, 1, LM_BYTE,    LM_CAT_RESOURCE, "Flag: insert map to ROM resources" },
    { "TmpResLoad",    0x0B9F, 1, LM_BYTE,    LM_CAT_RESOURCE, "Temporary SetResLoad state" },
    { "IntlSpec",      0x0BA0, 4, LM_LONG,    LM_CAT_MISC,     "International software installed if != -1" },
    { "SysFontFam",    0x0BA6, 2, LM_WORD,    LM_CAT_FONT,     "Font number for system font (if nonzero)" },
    { "SysFontSize",   0x0BA8, 2, LM_WORD,    LM_CAT_FONT,     "Size of system font (if nonzero)" },
    { "MBarHeight",    0x0BAA, 2, LM_WORD,    LM_CAT_MENU,     "Height of menu bar" },
    { "LastFOND",      0x0BC2, 4, LM_HANDLE,  LM_CAT_FONT,     "Handle to last family record used" },
    { "FractEnable",   0x0BF4, 1, LM_BYTE,    LM_CAT_FONT,     "Nonzero to enable fractional widths" },
    { "MMU32Bit",      0x0CB2, 1, LM_BYTE,    LM_CAT_SYSTEM,   "Current address mode" },
    { "TheGDevice",    0x0CC8, 4, LM_HANDLE,  LM_CAT_HARDWARE, "Handle to current active device" },
    { "AuxWinHead",    0x0CD0, 4, LM_LONG,    LM_CAT_WINDOW,   "Auxiliary window list header" },
    { "TimeDBRA",      0x0D00, 2, LM_WORD,    LM_CAT_TIMING,   "DBRA instructions per millisecond" },
    { "TimeSCCDB",     0x0D02, 2, LM_WORD,    LM_CAT_TIMING,   "SCC accesses per millisecond" },
    { "JVBLTask",      0x0D28, 4, LM_POINTER, LM_CAT_INTERRUPT, "Jump vector for DoVBLTask routine" },
    { "SynListHandle", 0x0D32, 4, LM_HANDLE,  LM_CAT_FONT,     "Handle to synthetic font list" },
    { "MenuCInfo",     0x0D50, 4, LM_POINTER, LM_CAT_MENU,     "Header for menu color information table" },
    { "DTQueue",       0x0D92, 10,LM_BYTES,   LM_CAT_INTERRUPT, "Deferred task queue header" },
    { "JDTInstall",    0x0D9C, 4, LM_POINTER, LM_CAT_INTERRUPT, "Jump vector for DTInstall routine" },
    { "HiliteRGB",     0x0DA0, 6, LM_BYTES,   LM_CAT_MISC,     "Default highlight color for system" },
    { "TimeSCSIDB",    0x0DA6, 2, LM_WORD,    LM_CAT_TIMING,   "SCSI accesses per millisecond" },
    { "SysParam",      0x01F8, 20,LM_BYTES,   LM_CAT_MISC,     "Low-memory copy of parameter RAM" },
};
```

The actual file will have these entries sorted by address and will
include `kLowMemCount` and `kLMCategoryLabels[]`.

**Build gate**: compile only — `cmake --build bld/macos-imgui` must succeed
(no tool panel yet, just data).

---

## Phase 2 — Snapshot helpers and value formatting

Add utility functions to `lomem_globals.cpp` for reading guest memory,
taking a snapshot, comparing against a snapshot, and formatting a value
for display.

### 2.1 — Snapshot functions

Add to `lomem_globals.h`:

```cpp
/* Take a 4 KB snapshot of low memory ($000–$FFF) from g_ram. */
void lomem_snapshot_take(uint8_t *snapshot);

/* Returns true if live RAM at [addr, addr+size) differs from snapshot. */
bool lomem_snapshot_changed(const uint8_t *snapshot, uint32_t addr, uint16_t size);
```

Implement in `lomem_globals.cpp`:
- `lomem_snapshot_take`: `memcpy(snapshot, g_ram, 4096)`, guarded by
  `if (!g_ram) return;` and clamped to `g_rig->ramSize()` if < 4096
  (theoretical — every Mac has ≥ 128 KB).
- `lomem_snapshot_changed`: `memcmp(snapshot + addr, g_ram + addr, size) != 0`,
  with bounds checks.

### 2.2 — Value formatting

Add to `lomem_globals.h`:

```cpp
/* Format a low-memory global's value into buf (null-terminated).
   Returns buf for convenience.  bufSize should be >= 64. */
char *lomem_format_value(const LMGlobal *g, char *buf, int bufSize);
```

Implement in `lomem_globals.cpp`.  Reads from `g_ram` directly.
Formatting per type:
- `LM_BYTE`: `snprintf(buf, sz, "$%02X", v)`
- `LM_WORD`: `snprintf(buf, sz, "$%04X", v)`
- `LM_LONG`: `snprintf(buf, sz, "$%08X", v)`
- `LM_POINTER`: `snprintf(buf, sz, "$%08X", v)`
- `LM_HANDLE`: `snprintf(buf, sz, "$%08X (H)", v)`
- `LM_PSTRING`: read length byte, then up to 31 chars; convert using
  `MacRoman2UniCodeData`, wrap in quotes.
- `LM_BYTES`: `"XX XX XX … "` — first 16 bytes, then `…` if longer.
- `LM_RECT`: read 4 words (top, left, bottom, right), format as
  `"(%d,%d)-(%d,%d)"` using signed int16.
- `LM_PATTERN`: 8 bytes as `"XX XX XX XX XX XX XX XX"`.
- `LM_OSType`: read 4 bytes, format as `"'ABCD'"`.

Also add a small helper:

```cpp
/* Short type label for display column. */
const char *lomem_type_label(LMType t);
```

Returns `"byte"`, `"word"`, `"long"`, `"ptr"`, `"hdl"`, `"str"`,
`"bytes"`, `"rect"`, `"pat"`, `"ostp"`.

**Build gate**: `cmake --build bld/macos-imgui`

---

## Phase 3 — ImGui tool panel

Create `src/platform/imgui_lomem_tool.h` and
`src/platform/imgui_lomem_tool.cpp` with the `LowMemTool` class.

### 3.1 — Create `src/platform/imgui_lomem_tool.h`

```cpp
#pragma once
#include "platform/imgui_tool.h"
#include <cstdint>

class LowMemTool : public ToolPanel {
public:
    const char* name() const override { return "Low Memory Globals"; }
    void draw() override;
private:
    char     filterBuf_[64] = {};
    int      categoryFilter_ = 0;   /* 0 = All */
    bool     snapshotValid_ = false;
    uint8_t  snapshot_[4096] = {};
};
```

### 3.2 — Create `src/platform/imgui_lomem_tool.cpp`

Implement `LowMemTool::draw()`:

1. **Early exit**: `if (!ImGui::Begin(name(), &visible)) { ImGui::End(); return; }`
2. **No machine guard**: `if (!g_ram) { ImGui::Text("No machine loaded"); ImGui::End(); return; }`
3. **Controls bar**:
   - `ImGui::InputText("Filter", filterBuf_, sizeof(filterBuf_))` with width 200.
   - `ImGui::SameLine()` + category combo: items are `"All"` + `kLMCategoryLabels[0..LM_CAT_COUNT-1]`.
   - `ImGui::SameLine()` + `ImGui::Button("Mark")` → `lomem_snapshot_take(snapshot_); snapshotValid_ = true;`
   - `ImGui::SameLine()` + `ImGui::Button("Clear")` → `snapshotValid_ = false;`
4. **Separator**.
5. **Table**: `ImGui::BeginTable("lomem", 5, flags)` with:
   - flags = `Borders | RowBg | Sortable | ScrollY | SizingStretchProp | Resizable`
   - Columns: Name (stretch), Addr (fixed 70), Type (fixed 50), Value (stretch), Brief (stretch)
   - `ImGui::TableHeadersRow()`
6. **Sort handling**: support sorting by Name (strcmp), Addr (numeric),
   Count (not needed). Build an index array once, sort it, iterate in
   sorted order. Re-sort only when `ImGui::TableGetSortSpecs()->SpecsDirty`.
7. **Row loop**: for each entry (in sorted order):
   - **Filter**: skip if `categoryFilter_ > 0` and `entry.category != categoryFilter_ - 1`.
   - **Filter**: skip if `filterBuf_[0]` and neither `entry.name` nor `entry.brief`
     contains `filterBuf_` (case-insensitive `strcasestr` or manual loop).
   - **Name column**: `ImGui::TextUnformatted(entry.name)`
   - **Addr column**: `ImGui::Text("$%04X", entry.addr)`
   - **Type column**: `ImGui::TextUnformatted(lomem_type_label(entry.type))`
   - **Value column**:
     ```
     changed = snapshotValid_ && lomem_snapshot_changed(snapshot_, entry.addr, entry.size)
     if (changed) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.3f, 0.3f, 1))
     char vbuf[128];
     lomem_format_value(&entry, vbuf, sizeof(vbuf));
     ImGui::TextUnformatted(vbuf);
     if (changed) ImGui::PopStyleColor()
     ```
   - **Brief column**: `ImGui::TextUnformatted(entry.brief)` +
     `if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", entry.brief);`
8. **EndTable**, **End**.

**Build gate**: `cmake --build bld/macos-imgui`

---

## Phase 4 — Registration, build integration, smoke test

### 4.1 — Add to CMakeLists.txt

Add two lines to the `BACKEND_SOURCES` for the `imgui` backend:

```cmake
        src/platform/lomem_globals.cpp
        src/platform/imgui_lomem_tool.cpp
```

### 4.2 — Register the tool

**File:** `src/platform/imgui_debug_windows.cpp`

Add include at the top:
```cpp
#include "platform/imgui_lomem_tool.h"
```

Add to `RegisterDebugTools()`:
```cpp
registry.registerTool(std::make_unique<LowMemTool>());
```

### 4.3 — Update header

**File:** `src/platform/imgui_debug_windows.h`

No change needed — `LowMemTool` is declared in its own header.  The
`RegisterDebugTools` function signature is already in `imgui_debug_windows.h`
and doesn't need to know about the concrete type.

### 4.4 — Build and test

```sh
cmake --build bld/macos-imgui
cmake --build bld/macos-headless   # must still compile (no lomem in headless)
cd test && ./verify.sh             # regression suite must pass
```

### 4.5 — Manual smoke test

Launch the imgui build, open the "Low Memory Globals" window from the
Tools menu.  Verify:

- Table displays with all ~160 entries, sorted by address
- Filter narrows visible rows (type "Scrap" → only scrap globals)
- Category combo filters (select "Timing" → only Ticks, Time, etc.)
- Values update each frame (Ticks should change)
- Mark button: click it, wait, then verify Ticks/Time show in red
- Clear button: red highlighting goes away
- Pascal strings display correctly (CurApName shows "Finder" or similar)

### 4.6 — Commit

```sh
git add -A
git commit -m "feat: low-memory globals viewer (Beatrix debug tool)"
```
