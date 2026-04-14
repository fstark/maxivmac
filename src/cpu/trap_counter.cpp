/*
	trap_counter.cpp — A-line trap call counter implementation

	Architecture:
	  - Flat uint32_t array indexed by trap number (OS 0-255, Toolbox 256-1279)
		incremented unconditionally from DoCodeA().  Zero runtime overhead beyond
		one array write per trap.
	  - Static dictionary of 681 trap entries for name lookup / autocomplete.
	  - Watchlist: small vector of trap words the UI cares about.
*/

#include "cpu/trap_counter.h"
#include "cpu/trap_tracer.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>

/* ── Counter array ────────────────────────────────── */

/*
   Index layout:
	 0..255   = OS trap numbers   (trap word & 0x00FF)
	 256..1279 = Toolbox trap numbers (trap word & 0x03FF) + 256
*/
static constexpr int kCounterSize = 1280;
static std::atomic<uint32_t> s_counters[kCounterSize];

static inline int TrapIndex(uint16_t trapWord)
{
	if (trapWord & 0x0800)
		return 256 + (trapWord & 0x03FF);
	else
		return trapWord & 0x00FF;
}

void trap_counter_record(uint16_t trapWord)
{
	int idx = TrapIndex(trapWord);
	s_counters[idx].fetch_add(1, std::memory_order_relaxed);
}

void trap_counter_reset()
{
	for (int i = 0; i < kCounterSize; ++i)
		s_counters[i].store(0, std::memory_order_relaxed);
}

uint32_t trap_counter_get(uint16_t trapWord)
{
	return s_counters[TrapIndex(trapWord)].load(std::memory_order_relaxed);
}

/* ── Console tracing ──────────────────────────────── */

static std::atomic<int> s_traceDepth{0};

void BeginTraceTraps()
{
	int prev = s_traceDepth.fetch_add(1, std::memory_order_relaxed);
	if (prev == 0) g_tracer.enable(true);
}

void EndTraceTraps()
{
	int prev = s_traceDepth.fetch_sub(1, std::memory_order_relaxed);
	if (prev <= 1)
	{
		s_traceDepth.store(0, std::memory_order_relaxed);
		g_tracer.enable(false);
	}
}

void trap_trace_log(uint16_t trapWord)
{
	if (s_traceDepth.load(std::memory_order_relaxed) <= 0) return;
	const char *name = trap_dict_name(trapWord);
	if (name)
		fprintf(stdout, "[TRAP] $%04X %s\n", trapWord, name);
	else
		fprintf(stdout, "[TRAP] $%04X\n", trapWord);
}

/* ── Full trap dictionary ─────────────────────────── */

static const TrapInfo s_dict[] = {
	{0xA07C, "ADBOp"},
	{0xA07B, "ADBReInit"},
	{0xAA94, "ActivatePalette"},
	{0xAA3B, "AddComp"},
	{0xA04E, "AddDrive"},
	{0xA87E, "AddPt"},
	{0xA94D, "AddResMenu"},
	{0xA9AB, "AddResource"},
	{0xAA3A, "AddSearch"},
	{0xA985, "Alert"},
	{0xAA1D, "AllocCursor"},
	{0xA010, "Allocate"},
	{0xA8C4, "AngleFromSlope"},
	{0xAA99, "AnimateEntry"},
	{0xAA9A, "AnimatePalette"},
	{0xA933, "AppendMenu"},
	{0xA071, "AttachVBL"},
	{0xA863, "BackColor"},
	{0xA87C, "BackPat"},
	{0xAA0B, "BackPixPat"},
	{0xA922, "BeginUpdate"},
	{0xA858, "BitAnd"},
	{0xA85F, "BitClr"},
	{0xA85A, "BitNot"},
	{0xA85B, "BitOr"},
	{0xA85E, "BitSet"},
	{0xA85C, "BitShift"},
	{0xA85D, "BitTst"},
	{0xA859, "BitXor"},
	{0xA02E, "BlockMove"},
	{0xA920, "BringToFront"},
	{0xA974, "Button"},
	{0xAA9F, "CTab2Palette"},
	{0xAA4F, "CalcCMask"},
	{0xA838, "CalcMask"},
	{0xA948, "CalcMenuSize"},
	{0xA90A, "CalcVBehind"},
	{0xA909, "CalcVis"},
	{0xA988, "CautionAlert"},
	{0xA9F3, "Chain"},
	{0xA9AA, "ChangedResource"},
	{0xAA23, "CharExtra"},
	{0xA88D, "CharWidth"},
	{0xA945, "CheckItem"},
	{0xA911, "CheckUpdate"},
	{0xA934, "ClearMenuBar"},
	{0xA90B, "ClipAbove"},
	{0xA87B, "ClipRect"},
	{0xA001, "Close"},
	{0xA87D, "CloseCPort"},
	{0xA9B7, "CloseDeskAcc"},
	{0xA982, "CloseDialog"},
	{0xA8CC, "ClosePgon"},
	{0xA8F4, "ClosePicture"},
	{0xA99A, "CloseResFile"},
	{0xA8DB, "CloseRgn"},
	{0xA92D, "CloseWindow"},
	{0xA03C, "CmpString"},
	{0xAA33, "Color2Index"},
	{0xA864, "ColorBit"},
	{0xA04C, "CompactMem"},
	{0xA004, "Control"},
	{0xA8EC, "CopyBits"},
	{0xA817, "CopyMask"},
	{0xAA05, "CopyPixMap"},
	{0xAA09, "CopyPixPat"},
	{0xA8DC, "CopyRgn"},
	{0xA989, "CouldAlert"},
	{0xA979, "CouldDialog"},
	{0xA80D, "Count1Resources"},
	{0xA81C, "Count1Types"},
	{0xA077, "CountADBs"},
	{0xA950, "CountMItems"},
	{0xA99C, "CountResources"},
	{0xA99E, "CountTypes"},
	{0xA008, "Create"},
	{0xA9B1, "CreateResFile"},
	{0xA994, "CurResFile"},
	{0xA082, "DTInstall"},
	{0xA9C7, "Date2Secs"},
	{0xAA4D, "DelComp"},
	{0xAA60, "DelMCEntries"},
	{0xA952, "DelMenuItem"},
	{0xAA4C, "DelSearch"},
	{0xA03B, "Delay"},
	{0xA009, "Delete"},
	{0xA936, "DeleteMenu"},
	{0xA94F, "DeltaPoint"},
	{0xA96E, "Dequeue"},
	{0xA992, "DetachResource"},
	{0xA980, "DialogSelect"},
	{0xA8E6, "DiffRgn"},
	{0xA93A, "DisableItem"},
	{0xAA63, "DispMCInfo"},
	{0xAA26, "DisposCCursor"},
	{0xAA25, "DisposCIcon"},
	{0xAA24, "DisposCTable"},
	{0xA955, "DisposControl"},
	{0xA983, "DisposDialog"},
	{0xAA30, "DisposGDevice"},
	{0xA023, "DisposHandle"},
	{0xA932, "DisposMenu"},
	{0xAA04, "DisposPixMap"},
	{0xAA08, "DisposPixPat"},
	{0xA01F, "DisposPtr"},
	{0xA8D9, "DisposRgn"},
	{0xA914, "DisposWindow"},
	{0xAA93, "DisposePalette"},
	{0xA072, "DoVBLTask"},
	{0xA967, "DragControl"},
	{0xA905, "DragGrayRgn"},
	{0xA926, "DragTheRgn"},
	{0xA925, "DragWindow"},
	{0xA96D, "Draw1Control"},
	{0xA883, "DrawChar"},
	{0xA969, "DrawControls"},
	{0xA981, "DrawDialog"},
	{0xA904, "DrawGrowIcon"},
	{0xA937, "DrawMenuBar"},
	{0xA90F, "DrawNew"},
	{0xA8F6, "DrawPicture"},
	{0xA884, "DrawString"},
	{0xA885, "DrawText"},
	{0xA03D, "DrvrInstall"},
	{0xA03E, "DrvrRemove"},
	{0xA017, "Eject"},
	{0xA9EC, "Elems68K"},
	{0xA02B, "EmptyHandle"},
	{0xA8AE, "EmptyRect"},
	{0xA8E2, "EmptyRgn"},
	{0xA939, "EnableItem"},
	{0xA923, "EndUpdate"},
	{0xA96F, "Enqueue"},
	{0xA881, "EqualPt"},
	{0xA8A6, "EqualRect"},
	{0xA8E3, "EqualRgn"},
	{0xA8C0, "EraseArc"},
	{0xA8B9, "EraseOval"},
	{0xA8C8, "ErasePoly"},
	{0xA8A3, "EraseRect"},
	{0xA8D4, "EraseRgn"},
	{0xA8B2, "EraseRoundRect"},
	{0xA98C, "ErrorSound"},
	{0xA971, "EventAvail"},
	{0xA9F4, "ExitToShell"},
	{0xA901, "FMSwapFont"},
	{0xA9EB, "FP68K"},
	{0xA8C2, "FillArc"},
	{0xAA11, "FillCArc"},
	{0xAA0F, "FillCOval"},
	{0xAA13, "FillCPoly"},
	{0xAA0E, "FillCRect"},
	{0xAA12, "FillCRgn"},
	{0xAA10, "FillCRoundRect"},
	{0xA8BB, "FillOval"},
	{0xA8CA, "FillPoly"},
	{0xA8A5, "FillRect"},
	{0xA8D6, "FillRgn"},
	{0xA8B4, "FillRoundRect"},
	{0xA96C, "FindControl"},
	{0xA984, "FindDItem"},
	{0xA92C, "FindWindow"},
	{0xA841, "Fix2Frac"},
	{0xA840, "Fix2Long"},
	{0xA843, "Fix2X"},
	{0xA818, "FixAtan2"},
	{0xA84D, "FixDiv"},
	{0xA868, "FixMul"},
	{0xA869, "FixRatio"},
	{0xA86C, "FixRound"},
	{0xA94C, "FlashMenuBar"},
	{0xA032, "FlushEvents"},
	{0xA045, "FlushFile"},
	{0xA013, "FlushVol"},
	{0xA835, "FontMetrics"},
	{0xA862, "ForeColor"},
	{0xA842, "Frac2Fix"},
	{0xA845, "Frac2X"},
	{0xA847, "FracCos"},
	{0xA84B, "FracDiv"},
	{0xA84A, "FracMul"},
	{0xA848, "FracSin"},
	{0xA849, "FracSqrt"},
	{0xA8BE, "FrameArc"},
	{0xA8B7, "FrameOval"},
	{0xA8C6, "FramePoly"},
	{0xA8A1, "FrameRect"},
	{0xA8D2, "FrameRgn"},
	{0xA8B0, "FrameRoundRect"},
	{0xA98A, "FreeAlert"},
	{0xA97A, "FreeDialog"},
	{0xA01C, "FreeMem"},
	{0xA924, "FrontWindow"},
	{0xA80E, "Get1IxResource"},
	{0xA80F, "Get1IxType"},
	{0xA820, "Get1NamedResource"},
	{0xA81F, "Get1Resource"},
	{0xA079, "GetADBInfo"},
	{0xA9F5, "GetAppParms"},
	{0xAA44, "GetAuxCtl"},
	{0xAA42, "GetAuxWin"},
	{0xAA1A, "GetBackColor"},
	{0xAA1B, "GetCCursor"},
	{0xAA1E, "GetCIcon"},
	{0xAA17, "GetCPixel"},
	{0xA95A, "GetCRefCon"},
	{0xAA18, "GetCTable"},
	{0xA95E, "GetCTitle"},
	{0xA809, "GetCVariant"},
	{0xAA48, "GetCWMgrPort"},
	{0xA87A, "GetClip"},
	{0xA96A, "GetCtlAction"},
	{0xA960, "GetCtlValue"},
	{0xAA28, "GetCTSeed"},
	{0xA9B9, "GetCursor"},
	{0xA07D, "GetDefaultStartup"},
	{0xAA29, "GetDeviceList"},
	{0xA98D, "GetDItem"},
	{0xA011, "GetEOF"},
	{0xAA9B, "GetEntryColor"},
	{0xAA9D, "GetEntryUsage"},
	{0xA8FF, "GetFName"},
	{0xA900, "GetFNum"},
	{0xA018, "GetFPos"},
	{0xA00C, "GetFileInfo"},
	{0xA88B, "GetFontInfo"},
	{0xAA19, "GetForeColor"},
	{0xAA32, "GetGDevice"},
	{0xA025, "GetHandleSize"},
	{0xA990, "GetIText"},
	{0xA9BB, "GetIcon"},
	{0xA078, "GetIndADB"},
	{0xA99D, "GetIndResource"},
	{0xA99F, "GetIndType"},
	{0xA946, "GetItem"},
	{0xA84E, "GetItemCmd"},
	{0xA93F, "GetItmIcon"},
	{0xA943, "GetItmMark"},
	{0xA941, "GetItmStyle"},
	{0xA976, "GetKeys"},
	{0xAA64, "GetMCEntry"},
	{0xAA61, "GetMCInfo"},
	{0xA949, "GetMHandle"},
	{0xAA2A, "GetMainDevice"},
	{0xA962, "GetMaxCtl"},
	{0xAA27, "GetMaxDevice"},
	{0xA93B, "GetMenuBar"},
	{0xA961, "GetMinCtl"},
	{0xA972, "GetMouse"},
	{0xA9A1, "GetNamedResource"},
	{0xAA46, "GetNewCWindow"},
	{0xA9BE, "GetNewControl"},
	{0xA97C, "GetNewDialog"},
	{0xA9C0, "GetNewMBar"},
	{0xAA92, "GetNewPalette"},
	{0xA9BD, "GetNewWindow"},
	{0xAA2B, "GetNextDevice"},
	{0xA970, "GetNextEvent"},
	{0xA084, "GetOSDefault"},
	{0xA031, "GetOSEvent"},
	{0xAA96, "GetPalette"},
	{0xA9B8, "GetPattern"},
	{0xA89A, "GetPen"},
	{0xA898, "GetPenState"},
	{0xA9BC, "GetPicture"},
	{0xAA0C, "GetPixPat"},
	{0xA865, "GetPixel"},
	{0xA874, "GetPort"},
	{0xA021, "GetPtrSize"},
	{0xA9BF, "GetRMenu"},
	{0xA9A6, "GetResAttrs"},
	{0xA9F6, "GetResFileAttrs"},
	{0xA9A8, "GetResInfo"},
	{0xA9A0, "GetResource"},
	{0xA9FD, "GetScrap"},
	{0xA9BA, "GetString"},
	{0xAA37, "GetSubTable"},
	{0xA146, "GetTrapAddress"},
	{0xA080, "GetVideoDefault"},
	{0xA014, "GetVol"},
	{0xA007, "GetVolInfo"},
	{0xA910, "GetWMgrPort"},
	{0xA917, "GetWRefCon"},
	{0xA919, "GetWTitle"},
	{0xA80A, "GetWVariant"},
	{0xA92F, "GetWindowPic"},
	{0xA11A, "GetZone"},
	{0xA871, "GlobalToLocal"},
	{0xA872, "GrafDevice"},
	{0xA92B, "GrowWindow"},
	{0xA068, "HClrRBit"},
	{0xA069, "HGetState"},
	{0xA029, "HLock"},
	{0xA04A, "HNoPurge"},
	{0xA049, "HPurge"},
	{0xA067, "HSetRBit"},
	{0xA06A, "HSetState"},
	{0xA02A, "HUnlock"},
	{0xA9E4, "HandAndHand"},
	{0xA9E1, "HandToHand"},
	{0xA126, "HandleZone"},
	{0xA86A, "HiWord"},
	{0xA958, "HideControl"},
	{0xA852, "HideCursor"},
	{0xA827, "HideDItem"},
	{0xA896, "HidePen"},
	{0xA916, "HideWindow"},
	{0xAA22, "HiliteColor"},
	{0xA95D, "HiliteControl"},
	{0xA938, "HiliteMenu"},
	{0xA91C, "HiliteWindow"},
	{0xA9A4, "HomeResFile"},
	{0xAA34, "Index2Color"},
	{0xA9F9, "InfoScrap"},
	{0xA9E6, "InitAllPacks"},
	{0xA02C, "InitApplZone"},
	{0xAA01, "InitCport"},
	{0xA850, "InitCursor"},
	{0xA97B, "InitDialogs"},
	{0xA8FE, "InitFonts"},
	{0xAA2E, "InitGDevice"},
	{0xA86E, "InitGraf"},
	{0xA930, "InitMenus"},
	{0xA9E5, "InitPack"},
	{0xAA90, "InitPalettes"},
	{0xA86D, "InitPort"},
	{0xA808, "InitProcMenu"},
	{0xA016, "InitQueue"},
	{0xA995, "InitResources"},
	{0xA03F, "InitUtil"},
	{0xA912, "InitWindows"},
	{0xA019, "InitZone"},
	{0xA826, "InsMenuItem"},
	{0xA935, "InsertMenu"},
	{0xA951, "InsertResMenu"},
	{0xA8A9, "InsetRect"},
	{0xA8E1, "InsetRgn"},
	{0xA07F, "InternalWait"},
	{0xA928, "InvalRect"},
	{0xA927, "InvalRgn"},
	{0xA8A4, "InverRect"},
	{0xA8D5, "InverRgn"},
	{0xA8B3, "InverRoundRect"},
	{0xA8C1, "InvertArc"},
	{0xAA35, "InvertColor"},
	{0xA8BA, "InvertOval"},
	{0xA8C9, "InvertPoly"},
	{0xA97F, "IsDialogEvent"},
	{0xA9C3, "KeyTrans"},
	{0xA956, "KillControls"},
	{0xA006, "KillIO"},
	{0xA8F5, "KillPicture"},
	{0xA8CD, "KillPoly"},
	{0xA9F2, "Launch"},
	{0xA892, "Line"},
	{0xA891, "LineTo"},
	{0xA86B, "LoWord"},
	{0xA9A2, "LoadResource"},
	{0xA9F0, "LoadSeg"},
	{0xA870, "LocalToGlobal"},
	{0xA9FB, "LodeScrap"},
	{0xA83F, "Long2Fix"},
	{0xA867, "LongMul"},
	{0xAA39, "MakeITable"},
	{0xAA0D, "MakeRGBPat"},
	{0xA8FC, "MapPoly"},
	{0xA8F9, "MapPt"},
	{0xA8FA, "MapRect"},
	{0xA8FB, "MapRgn"},
	{0xA063, "MaxApplZone"},
	{0xA061, "MaxBlock"},
	{0xA11D, "MaxMem"},
	{0xA821, "MaxSizeRsrc"},
	{0xA837, "MeasureText"},
	{0xAA66, "MenuChoice"},
	{0xA93E, "MenuKey"},
	{0xA93D, "MenuSelect"},
	{0xA991, "ModalDialog"},
	{0xA036, "MoreMasters"},
	{0xA00F, "MountVol"},
	{0xA894, "Move"},
	{0xA959, "MoveControl"},
	{0xA064, "MoveHHi"},
	{0xA877, "MovePortTo"},
	{0xA893, "MoveTo"},
	{0xA91B, "MoveWindow"},
	{0xA9E0, "Munger"},
	{0xAA4B, "NewCDialog"},
	{0xAA45, "NewCWindow"},
	{0xA954, "NewControl"},
	{0xA97D, "NewDialog"},
	{0xA066, "NewEmptyHandle"},
	{0xAA2F, "NewGDevice"},
	{0xA122, "NewHandle"},
	{0xA931, "NewMenu"},
	{0xAA91, "NewPalette"},
	{0xAA03, "NewPixMap"},
	{0xAA07, "NewPixPat"},
	{0xA11E, "NewPtr"},
	{0xA8D8, "NewRgn"},
	{0xA906, "NewString"},
	{0xA913, "NewWindow"},
	{0xA987, "NoteAlert"},
	{0xA030, "OSEventAvail"},
	{0xA856, "ObscureCursor"},
	{0xA035, "Offline"},
	{0xA8CE, "OffsetPoly"},
	{0xA8A8, "OffsetRect"},
	{0xA8E0, "OfsetRgn"},
	{0xAA21, "OpColor"},
	{0xA000, "Open"},
	{0xAA00, "OpenCport"},
	{0xA9B6, "OpenDeskAcc"},
	{0xA8F3, "OpenPicture"},
	{0xA8CB, "OpenPoly"},
	{0xA86F, "OpenPort"},
	{0xA00A, "OpenRF"},
	{0xA9C4, "OpenRFPerm"},
	{0xA997, "OpenResFile"},
	{0xA8DA, "OpenRgn"},
	{0xA12F, "PPostEvent"},
	{0xA8CF, "PackBits"},
	{0xA8BF, "PaintArc"},
	{0xA90D, "PaintBehind"},
	{0xA90C, "PaintOne"},
	{0xA8B8, "PaintOval"},
	{0xA8C7, "PaintPoly"},
	{0xA8A2, "PaintRect"},
	{0xA8D3, "PaintRgn"},
	{0xA8B1, "PaintRoundRect"},
	{0xAAA0, "Palette2CTab"},
	{0xA98B, "ParamText"},
	{0xA89C, "PenMode"},
	{0xA89E, "PenNormal"},
	{0xA89D, "PenPat"},
	{0xAA0A, "PenPixPat"},
	{0xA89B, "PenSize"},
	{0xA8F2, "PicComment"},
	{0xA94E, "PinRect"},
	{0xAA1F, "PlotCIcon"},
	{0xA94B, "PlotIcon"},
	{0xAA98, "PmBackColor"},
	{0xAA97, "PmForeColor"},
	{0xA80B, "PopUpMenuSelect"},
	{0xA876, "PortSize"},
	{0xA02F, "PostEvent"},
	{0xA8FD, "PrGlue"},
	{0xAA3D, "ProtectEntry"},
	{0xA8AC, "Pt2Rect"},
	{0xA8AD, "PtInRect"},
	{0xA8E8, "PtInRgn"},
	{0xA8C3, "PtToAngle"},
	{0xA9EF, "PtrAndHand"},
	{0xA9E3, "PtrToHand"},
	{0xA9E2, "PtrToXHand"},
	{0xA148, "PtrZone"},
	{0xA04D, "PurgeMem"},
	{0xA062, "PurgeSpace"},
	{0xA9FE, "PutScrap"},
	{0xAA40, "QDError"},
	{0xA04F, "RDrvrInstall"},
	{0xAA15, "RGBBackColor"},
	{0xAA14, "RGBForeColor"},
	{0xA80C, "RGetResource"},
	{0xA861, "Random"},
	{0xA002, "Read"},
	{0xA039, "ReadDateTime"},
	{0xAA36, "RealColor"},
	{0xA902, "RealFont"},
	{0xA027, "ReallocHandle"},
	{0xA128, "RecoverHandle"},
	{0xA8E9, "RectInRgn"},
	{0xA8DF, "RectRgn"},
	{0xA050, "RelString"},
	{0xA9A3, "ReleaseResource"},
	{0xA00B, "Rename"},
	{0xA9AF, "ResError"},
	{0xAA3E, "ReserveEntry"},
	{0xA040, "ResrvMem"},
	{0xAA4A, "RestoreEntries"},
	{0xA9AD, "RmveResource"},
	{0xA9C5, "RsrcMapEntry"},
	{0xA996, "RsrcZoneInit"},
	{0xA042, "RstFilLock"},
	{0xAA49, "SaveEntries"},
	{0xA90E, "SaveOld"},
	{0xA8F8, "ScalePt"},
	{0xA8EF, "ScrollRect"},
	{0xA9C6, "Secs2Date"},
	{0xA8AA, "SectRect"},
	{0xA8E4, "SectRgn"},
	{0xAA50, "SeedCFill"},
	{0xA839, "SeedFill"},
	{0xA97E, "SelIText"},
	{0xA91F, "SelectWindow"},
	{0xA921, "SendBehind"},
	{0xA07A, "SetADBInfo"},
	{0xA057, "SetAppBase"},
	{0xA02D, "SetApplLimit"},
	{0xAA1C, "SetCCursor"},
	{0xAA16, "SetCPixel"},
	{0xAA06, "SetCPortPix"},
	{0xA95B, "SetCRefCon"},
	{0xA95F, "SetCTitle"},
	{0xAA3C, "SetClientID"},
	{0xA879, "SetClip"},
	{0xA96B, "SetCtlAction"},
	{0xAA43, "SetCtlColor"},
	{0xA963, "SetCtlValue"},
	{0xA851, "SetCursor"},
	{0xA98E, "SetDItem"},
	{0xA03A, "SetDateTime"},
	{0xA07E, "SetDefaultStartup"},
	{0xAA47, "SetDeskCPat"},
	{0xAA2D, "SetDeviceAttribute"},
	{0xA012, "SetEOF"},
	{0xA8DD, "SetEmptyRgn"},
	{0xAA3F, "SetEntries"},
	{0xAA9C, "SetEntryColor"},
	{0xAA9E, "SetEntryUsage"},
	{0xA044, "SetFPos"},
	{0xA834, "SetFScaleDisable"},
	{0xA041, "SetFilLock"},
	{0xA043, "SetFilType"},
	{0xA00D, "SetFileInfo"},
	{0xA903, "SetFontLock"},
	{0xAA31, "SetGDevice"},
	{0xA04B, "SetGrowZone"},
	{0xA024, "SetHandleSize"},
	{0xA98F, "SetIText"},
	{0xA947, "SetItem"},
	{0xA84F, "SetItemCmd"},
	{0xA940, "SetItmIcon"},
	{0xA944, "SetItmMark"},
	{0xA942, "SetItmStyle"},
	{0xAA65, "SetMCEntries"},
	{0xAA62, "SetMCInfo"},
	{0xA94A, "SetMFlash"},
	{0xA965, "SetMaxCtl"},
	{0xA93C, "SetMenuBar"},
	{0xA964, "SetMinCtl"},
	{0xA083, "SetOSDefault"},
	{0xA878, "SetOrigin"},
	{0xA875, "SetPBits"},
	{0xAA95, "SetPalette"},
	{0xA899, "SetPenState"},
	{0xA873, "SetPort"},
	{0xA880, "SetPt"},
	{0xA020, "SetPtrSize"},
	{0xA8DE, "SetRecRgn"},
	{0xA8A7, "SetRect"},
	{0xA9A7, "SetResAttrs"},
	{0xA9F7, "SetResFileAttrs"},
	{0xA9A9, "SetResInfo"},
	{0xA99B, "SetResLoad"},
	{0xA993, "SetResPurge"},
	{0xAA4E, "SetStdCProcs"},
	{0xA8EA, "SetStdProcs"},
	{0xA907, "SetString"},
	{0xA047, "SetTrapAddress"},
	{0xA081, "SetVideoDefault"},
	{0xA015, "SetVol"},
	{0xA918, "SetWRefCon"},
	{0xA91A, "SetWTitle"},
	{0xAA41, "SetWinColor"},
	{0xA92E, "SetWindowPic"},
	{0xA01B, "SetZone"},
	{0xA855, "ShieldCursor"},
	{0xA957, "ShowControl"},
	{0xA853, "ShowCursor"},
	{0xA828, "ShowDItem"},
	{0xA908, "ShowHide"},
	{0xA897, "ShowPen"},
	{0xA915, "ShowWindow"},
	{0xA95C, "SizeControl"},
	{0xA9A5, "SizeRsrc"},
	{0xA91D, "SizeWindow"},
	{0xA8BC, "SlopeFromAngle"},
	{0xA06F, "SlotVInstall"},
	{0xA070, "SlotVRemove"},
	{0xA802, "SndAddModifier"},
	{0xA806, "SndControl"},
	{0xA801, "SndDisposeChannel"},
	{0xA803, "SndDoCommand"},
	{0xA804, "SndDoImmediate"},
	{0xA807, "SndNewChannel"},
	{0xA805, "SndPlay"},
	{0xA88E, "SpaceExtra"},
	{0xA065, "StackSpace"},
	{0xA005, "Status"},
	{0xA8BD, "StdArc"},
	{0xA8EB, "StdBits"},
	{0xA8F1, "StdComment"},
	{0xA8EE, "StdGetPic"},
	{0xA890, "StdLine"},
	{0xA8B6, "StdOval"},
	{0xA8C5, "StdPoly"},
	{0xA8F0, "StdPutPic"},
	{0xA8AF, "StdRRect"},
	{0xA8A0, "StdRect"},
	{0xA8D1, "StdRgn"},
	{0xA882, "StdText"},
	{0xA8ED, "StdTxMeas"},
	{0xA973, "StillDown"},
	{0xA986, "StopAlert"},
	{0xA88C, "StringWidth"},
	{0xA055, "StripAddress"},
	{0xA866, "StuffHex"},
	{0xA87F, "SubPt"},
	{0xA05D, "SwapMMUMode"},
	{0xA9C8, "SysBeep"},
	{0xA9C2, "SysEdit"},
	{0xA090, "SysEnvirons"},
	{0xA9C9, "SysError"},
	{0xA9B3, "SystemClick"},
	{0xA9B2, "SystemEvent"},
	{0xA9B5, "SystemMenu"},
	{0xA9B4, "SystemTask"},
	{0xA9D8, "TEActivate"},
	{0xA813, "TEAutoView"},
	{0xA9D0, "TECalText"},
	{0xA9D4, "TEClick"},
	{0xA9D5, "TECopy"},
	{0xA9D6, "TECut"},
	{0xA9D9, "TEDeactivate"},
	{0xA9D7, "TEDelete"},
	{0xA9CD, "TEDispose"},
	{0xA83C, "TEGetOffset"},
	{0xA9CB, "TEGetText"},
	{0xA9DA, "TEIdle"},
	{0xA9CC, "TEInit"},
	{0xA9DE, "TEInsert"},
	{0xA9DC, "TEKey"},
	{0xA9D2, "TENew"},
	{0xA9DB, "TEPaste"},
	{0xA812, "TEPinScroll"},
	{0xA9DD, "TEScroll"},
	{0xA811, "TESelView"},
	{0xA9DF, "TESetJust"},
	{0xA9D1, "TESetSelect"},
	{0xA9CF, "TESetText"},
	{0xA83E, "TEStyleNew"},
	{0xA9D3, "TEUpdate"},
	{0xA966, "TestControl"},
	{0xAA2C, "TestDeviceAttribute"},
	{0xA9CE, "TextBox"},
	{0xA888, "TextFace"},
	{0xA887, "TextFont"},
	{0xA889, "TextMode"},
	{0xA88A, "TextSize"},
	{0xA886, "TextWidth"},
	{0xA975, "TickCount"},
	{0xA83B, "TrackBox"},
	{0xA968, "TrackControl"},
	{0xA91E, "TrackGoAway"},
	{0xA8AB, "UnionRect"},
	{0xA8E5, "UnionRgn"},
	{0xA810, "Unique1ID"},
	{0xA9C1, "UniqueID"},
	{0xA9F1, "UnloadSeg"},
	{0xA9FA, "UnlodeScrap"},
	{0xA00E, "UnmountVol"},
	{0xA8D0, "UnpackBits"},
	{0xA999, "UpdateResFile"},
	{0xA953, "UpdtControl"},
	{0xA978, "UpdtDialog"},
	{0xA054, "UprString"},
	{0xA998, "UseResFile"},
	{0xA033, "VInstall"},
	{0xA034, "VRemove"},
	{0xA92A, "ValidRect"},
	{0xA929, "ValidRgn"},
	{0xA977, "WaitMouseUp"},
	{0xA003, "Write"},
	{0xA038, "WriteParam"},
	{0xA9B0, "WriteResource"},
	{0xA844, "X2Fix"},
	{0xA846, "X2Frac"},
	{0xA8E7, "XorRgn"},
	{0xA9FC, "ZeroScrap"},
	{0xA83A, "ZoomWindow"},
	/* Dispatch traps (package entry points) */
	{0xA9E7, "Pack0"},
	{0xA9E9, "Pack2"},
	{0xA9EA, "Pack3"},
	{0xA9ED, "Pack6"},
	{0xA9EE, "Pack7"},
	{0xA82E, "Pack12"},
	{0xA260, "HFSDispatch"},
	{0xA815, "SCSIDispatch"},
	{0xA8B5, "ScriptUtil"},
	{0xA06E, "SlotManager"},
	{0xA895, "Shutdown"},
	{0xA83D, "TEDispatch"},
};

static constexpr int kDictSize = static_cast<int>(sizeof(s_dict) / sizeof(s_dict[0]));

/* Reverse-lookup index: counter-index → dictionary slot, built lazily */
static int16_t s_revIndex[kCounterSize]; /* -1 = no dict entry */
static bool s_revReady = false;

static void BuildRevIndex()
{
	for (int i = 0; i < kCounterSize; ++i)
		s_revIndex[i] = -1;
	for (int i = 0; i < kDictSize; ++i)
	{
		int idx = TrapIndex(s_dict[i].trapWord);
		s_revIndex[idx] = static_cast<int16_t>(i);
	}
	s_revReady = true;
}

int trap_dict_size()
{
	return kDictSize;
}

const TrapInfo &trap_dict_entry(int index)
{
	return s_dict[index];
}

const char *trap_dict_name(uint16_t trapWord)
{
	if (!s_revReady) BuildRevIndex();
	int idx = TrapIndex(trapWord);
	int16_t slot = s_revIndex[idx];
	return (slot >= 0) ? s_dict[slot].name : nullptr;
}

static inline bool ciStartsWith(const char *str, const char *prefix)
{
	for (; *prefix; ++str, ++prefix)
	{
		if (std::tolower(static_cast<unsigned char>(*str)) !=
			std::tolower(static_cast<unsigned char>(*prefix)))
			return false;
	}
	return true;
}

void trap_dict_search(const char *prefix, std::vector<TrapInfo> &results, int maxResults)
{
	results.clear();
	if (!prefix || !prefix[0]) return;
	for (int i = 0; i < kDictSize && (int)results.size() < maxResults; ++i)
	{
		if (ciStartsWith(s_dict[i].name, prefix)) results.push_back(s_dict[i]);
	}
}

/* ── Watchlist ────────────────────────────────────── */

static std::vector<uint16_t> s_watchlist;

static const uint16_t kDefaults[] = {
	0xA000, 0xA001, 0xA002, 0xA003, /* Open Close Read Write */
	0xA122, 0xA023, 0xA11E, 0xA01F, /* NewHandle DisposHandle NewPtr DisposPtr */
	0xA029, 0xA02A, 0xA02E,			/* HLock HUnlock BlockMove */
	0xA9A0, 0xA9A2, 0xA9A3,			/* GetResource LoadResource ReleaseResource */
	0xA970, 0xA971, 0xA02F,			/* GetNextEvent EventAvail PostEvent */
	0xA92C, 0xA913, 0xA884,			/* FindWindow NewWindow DrawString */
};

void trap_watch_load_defaults()
{
	s_watchlist.clear();
	for (uint16_t tw : kDefaults)
		s_watchlist.push_back(tw);
}

void trap_watch_add(uint16_t trapWord)
{
	for (uint16_t tw : s_watchlist)
		if (tw == trapWord) return;
	s_watchlist.push_back(trapWord);
}

void trap_watch_remove(uint16_t trapWord)
{
	auto it = std::find(s_watchlist.begin(), s_watchlist.end(), trapWord);
	if (it != s_watchlist.end()) s_watchlist.erase(it);
}

std::vector<WatchEntry> trap_watch_snapshot()
{
	std::vector<WatchEntry> out;
	out.reserve(s_watchlist.size());
	for (uint16_t tw : s_watchlist)
	{
		WatchEntry e;
		e.trapWord = tw;
		e.name = trap_dict_name(tw);
		if (!e.name) e.name = "???";
		e.count = trap_counter_get(tw);
		out.push_back(e);
	}
	return out;
}
