
/*
 *  LoMem.h - definitions of lo-mem globals
 *
 *  Copyright (c) 1991 Symantec Corporation.  All rights reserved.
 *
 *  These interfaces are based on material copyrighted
 *  by Apple Computer, Inc., 1985-1991.
 *
 */

#ifndef __LOMEM__
#define __LOMEM__

#ifndef __TYPES__
#include <Types.h>
#endif

#ifndef __CONTROLS__
#include <Controls.h>
#endif

#ifndef __DEVICES__
#include <Devices.h>
#endif

#ifndef __DISKINIT__
#include <DiskInit.h>
#endif

#ifndef __FONTS__
#include <Fonts.h>
#endif

#ifndef __MEMORY__
#include <Memory.h>
#endif

#ifndef __MENUS__
#include <Menus.h>
#endif

#ifndef __QUICKDRAW__
#include <Quickdraw.h>
#endif

#ifndef __SCRAP__
#include <Scrap.h>
#endif

#ifndef __WINDOWS__
#include <Windows.h>
#endif


short ApFontID : 0x984;
Ptr ApplLimit : 0x130;
THz ApplZone : 0x2AA;
Handle AppPacks[] : 0xAB8;
Handle AppParmHandle : 0xAEC;
short AtMenuBottom : 0xA0C;
AuxCtlHandle AuxCtlHead : 0xCD4;
AuxWinHandle AuxWinHead : 0xCD0;

short BootDrive : 0x210;
Ptr BufPtr : 0x10C;

long CaretTime : 0x2F4;
char CPUFlag : 0x12F;
Rect CrsrPin : 0x834;
WindowPtr CurActivate : 0xA64;
Str31 CurApName : 0x910;
short CurApRefNum : 0x900;
WindowPtr CurDeactive : 0xA68;
long CurDirStore : 0x398;
short CurJTOffset : 0x934;
short CurMap : 0xA5A;
short CurPageOption : 0x936;
Ptr CurrentA5 : 0x904;
Ptr CurStackBase : 0x908;

StringHandle DAStrings[4] : 0xAA0;
long DefltStack : 0x322;
VCB *DefVCBPtr : 0x352;
ProcPtr DeskHook : 0xA6C;
Pattern DeskPattern : 0xA3C;
GDHandle DeviceList : 0x8A8;
long DoubleTime : 0x2F0;
ProcPtr DragHook : 0x9F6;
Pattern DragPattern : 0xA34;
QHdr DrvQHdr : 0x308;
short DSErrCode : 0xAF0;

ProcPtr EjectNotify : 0x338;
QHdr EventQueue : 0x14A;
ProcPtr ExtStsDT[] : 0x2BE;

Ptr FCBSPtr : 0x34E;
Str15 FinderName : 0x2E0;
HFSDefaults *FmtDefaults : 0x39E;
char FractEnable : 0xBF4;
char FScaleDisable : 0xA63;
short FSFCBLen : 0x3F6;
QHdr FSQHdr : 0x360;

WindowPtr GhostWindow : 0xA84;
RgnHandle GrayRgn : 0x9EE;

Ptr HeapEnd : 0x114;
char HiliteMode : 0x938;
RGBColor HiliteRGB : 0xDA0;
short HWCfgFlags : 0xB22;

ProcPtr IAZNotify : 0x33C;
Ptr IWM : 0x1E0;

ProcPtr JGNEFilter : 0x29A;

short KeyRepThresh : 0x190;
short KeyThresh : 0x18E;
ProcPtr Key1Trans : 0x29E;
ProcPtr Key2Trans : 0x2A2;

FamRec **LastFOND : 0xBC2;
long Lo3Bytes : 0x31A;
ProcPtr Lvl1DT[] : 0x192;
ProcPtr Lvl2DT[] : 0x1B2;

GDHandle MainDevice : 0x8A4;
short MBarEnable : 0xA20;
short MBarHeight : 0xBAA;
ProcPtr MBarHook : 0xA2C;
short MemErr : 0x220;
Ptr MemTop : 0x108;
MCTableHandle MenuCInfo : 0xD50;
long MenuDisable : 0xB54;
short MenuFlash : 0xA24;
Handle MenuList : 0xA1C;
ProcPtr MenuHook : 0xA30;
long MinStack : 0x31E;

short PaintWhite : 0x9DC;
short PrintErr : 0x944;

char QDExist : 0x8F3;

Ptr RAMBase : 0x2B2;
short ResErr : 0xA60;
ProcPtr ResErrProc : 0xAF2;
Boolean ResLoad : 0xA5E;
ProcPtr ResumeProc : 0xA8C;
long RndSeed : 0x156;
Ptr ROMBase : 0x2AE;
short RomMapInsert : 0xB9E;
short ROM85 : 0x28E;

short SaveUpdate : 0x9DA;
RgnHandle SaveVisRgn : 0x9F2;
Ptr SCCRd : 0x1D8;
Ptr SCCWr : 0x1DC;
ScrapStuff ScrapInfo : 0x960;
char ScrDmpEnb : 0x2F8;
short ScreenRow : 0x106;
short ScrHRes : 0x104;
Ptr ScrnBase : 0x824;
short ScrVRes : 0x102;
char SdVolume : 0x260;
char SEvtEnb : 0x15C;
short SFSaveDisk : 0x214;
short SysEvtMask : 0x144;
short SysMap : 0xA58;
Handle SysMapHndl : 0xA54;
SysParmType SysParam : 0x1F8;
unsigned char SysResName[20] : 0xAD8;
short SysVersion : 0x15A;
THz SysZone : 0x2A6;

Handle TEScrpHandle : 0xAB4;
short TEScrpLength : 0xAB0;
GDHandle TheGDevice : 0xCC8;
short TheMenu : 0xA26;
THz TheZone : 0x118;
volatile long Ticks : 0x16A;
volatile long Time : 0x20C;
volatile long TimeLM : 0x20C;
Handle TopMapHndl : 0xA50;
short TopMenuItem : 0xA0A;

short UnitNtryCnt : 0x1D2;
DCtlHandle *UTableBase : 0x11C;

volatile QHdr VBLQueue : 0x160;
QHdr VCBQHdr : 0x356;
Ptr VIA : 0x1D4;

WidthTable **WidthTabHandle : 0xB2A;
WindowPeek WindowList : 0x9D6;
CGrafPtr WMgrCPort : 0xD2C;
GrafPtr WMgrPort : 0x9DE;
char WWExist : 0x8F2;


#endif // __LOMEM__
