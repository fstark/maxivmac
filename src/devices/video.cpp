/*
	VIDeo card EMulated DEVice

	Emulation of video card for Macintosh II.

	Written referring to:
		Sample firmware code in "Designing Cards and Drivers
		for Macintosh II and Macintosh SE", Apple Computer,
		page 8-20.

		Basilisk II source code, especially slot_rom.cpp
*/

#include "core/common.h"

#include "devices/video.h"
#include "devices/slot_rom.h"
#include "core/wire_bus.h"
#include "core/machine_obj.h"
#include "devices/via2.h"

#include "cpu/m68k.h"
#include "devices/sony.h"
#include "core/abnormal_ids.h"

#include <cstring>

/*
	ReportAbnormalID unused 0x0A08 - 0x0AFF
*/

#define VID_dolog 1

/* Maximum number of modes (depth 0..5) */
#define kMaxModes 6

/* Maximum number of resolutions */
#define kMaxResolutions 8

struct ResolutionEntry {
	uint32_t displayModeID;
	uint16_t width;
	uint16_t height;
};

/* Resolution table — built during init() */
static ResolutionEntry s_resolutions[kMaxResolutions];
static int s_numResolutions = 0;

/* Current depth and configured max depth (set during init) */
static int s_currentDepth = 0;
static int s_maxDepth     = 0;
static int s_preferredDepth = -1; /* -1 = use configured depth */
static uint32_t s_currentDisplayModeID = 1;
static int s_preferredDisplayModeID = -1; /* -1 = use boot res */
static uint16_t s_currentWidth = 640;
static uint16_t s_currentHeight = 480;

/* Flag for host to detect resolution change */
static bool s_resolutionChanged = false;

/* Host desktop size — set by platform layer before init() */
static uint16_t s_hostDesktopW = 0;
static uint16_t s_hostDesktopH = 0;

static const uint8_t VidDrvr_contents[] = {
0x4C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x2A, 0x00, 0x00, 0x00, 0xE2, 0x00, 0xEC,
0x00, 0xB6, 0x15, 0x2E, 0x44, 0x69, 0x73, 0x70,
0x6C, 0x61, 0x79, 0x5F, 0x56, 0x69, 0x64, 0x65,
0x6F, 0x5F, 0x53, 0x61, 0x6D, 0x70, 0x6C, 0x65,
0x00, 0x00, 0x24, 0x48, 0x26, 0x49, 0x70, 0x04,
0xA4, 0x40, 0x70, 0x04, 0xA7, 0x22, 0x66, 0x00,
0x00, 0x50, 0x27, 0x48, 0x00, 0x14, 0xA0, 0x29,
0x49, 0xFA, 0x00, 0x4A, 0x70, 0x10, 0xA7, 0x1E,
0x66, 0x00, 0x00, 0x3E, 0x31, 0x7C, 0x00, 0x06,
0x00, 0x04, 0x21, 0x4C, 0x00, 0x08, 0x21, 0x4B,
0x00, 0x0C, 0x70, 0x00, 0x10, 0x2B, 0x00, 0x28,
0xA0, 0x75, 0x66, 0x24, 0x22, 0x6B, 0x00, 0x14,
0x22, 0x51, 0x22, 0x88, 0x3F, 0x3C, 0x00, 0x01,
0x55, 0x4F, 0x3F, 0x3C, 0x00, 0x03, 0x41, 0xFA,
0x00, 0x9C, 0x2F, 0x18, 0x20, 0x50, 0x20, 0x8F,
0xDE, 0xFC, 0x00, 0x0A, 0x70, 0x00, 0x60, 0x02,
0x70, 0xE9, 0x4E, 0x75, 0x2F, 0x08, 0x55, 0x4F,
0x3F, 0x3C, 0x00, 0x04, 0x41, 0xFA, 0x00, 0x7E,
0x2F, 0x18, 0x20, 0x50, 0x20, 0x8F, 0x50, 0x4F,
0x20, 0x29, 0x00, 0x2A, 0xE1, 0x98, 0x02, 0x40,
0x00, 0x0F, 0x20, 0x78, 0x0D, 0x28, 0x4E, 0x90,
0x20, 0x5F, 0x70, 0x01, 0x4E, 0x75, 0x2F, 0x0B,
0x26, 0x69, 0x00, 0x14, 0x42, 0x67, 0x55, 0x4F,
0x3F, 0x3C, 0x00, 0x03, 0x41, 0xFA, 0x00, 0x4E,
0x2F, 0x18, 0x20, 0x50, 0x20, 0x8F, 0xDE, 0xFC,
0x00, 0x0A, 0x20, 0x53, 0x20, 0x50, 0xA0, 0x76,
0x20, 0x4B, 0xA0, 0x23, 0x70, 0x00, 0x26, 0x5F,
0x4E, 0x75, 0x2F, 0x08, 0x55, 0x4F, 0x3F, 0x3C,
0x00, 0x06, 0x60, 0x08, 0x2F, 0x08, 0x55, 0x4F,
0x3F, 0x3C, 0x00, 0x05, 0x41, 0xFA, 0x00, 0x1E,
0x2F, 0x18, 0x20, 0x50, 0x20, 0x8F, 0x5C, 0x4F,
0x30, 0x1F, 0x20, 0x5F, 0x08, 0x28, 0x00, 0x09,
0x00, 0x06, 0x67, 0x02, 0x4E, 0x75, 0x20, 0x78,
0x08, 0xFC, 0x4E, 0xD0
};

static void ChecksumSlotROM()
{
	/* Calculate CRC */
	/* assuming check sum field initialized to zero */
	int i;
	uint8_t * p = g_vidROM;
	uint32_t crc = 0;

	const auto& cfg = g_machine->config();
	for (i = cfg.vidROMSize; --i >= 0; ) {
		crc = ((crc << 1) | (crc >> 31)) + *p++;
	}
	do_put_mem_long(p - 12, crc);
}

/* Return the CLUT size for a given depth (0 for direct modes) */
static int clutSizeForDepth(int depth)
{
	if (depth <= 0) return 2;
	if (depth >= 4) return 0; /* direct mode, no CLUT */
	return 1 << (1 << depth);
}

/* Look up a resolution by displayModeID.  Returns nullptr if not found. */
static const ResolutionEntry* findResolution(uint32_t displayModeID)
{
	for (int i = 0; i < s_numResolutions; i++) {
		if (s_resolutions[i].displayModeID == displayModeID)
			return &s_resolutions[i];
	}
	return nullptr;
}

/* Return the next resolution after prevID, or nullptr if exhausted. */
static const ResolutionEntry* nextResolution(uint32_t prevID)
{
	if (prevID == 0)
		return (s_numResolutions > 0) ? &s_resolutions[0] : nullptr;
	for (int i = 0; i < s_numResolutions; i++) {
		if (s_resolutions[i].displayModeID == prevID) {
			if (i + 1 < s_numResolutions)
				return &s_resolutions[i + 1];
			return nullptr;
		}
	}
	return nullptr;
}

/* Maximum depth mode that fits in VRAM for a given resolution. */
static int maxDepthForResolution(uint16_t w, uint16_t h, uint32_t vidMemSize)
{
	int best = 0;
	for (int d = 0; d <= 5; d++) {
		uint32_t bytes = (uint32_t)w * h * (1 << d) / 8;
		if (bytes <= vidMemSize)
			best = d;
	}
	return best;
}

/* Build the classic resolution table.  Called during init().
   If host desktop dimensions were set via Vid_SetHostDesktop(),
   append host-derived resolutions (IDs 100, 101) provided they
   differ from the classic set and fit within 15 MB VRAM. */
static void buildResolutionTable()
{
	static const struct { uint32_t id; uint16_t w, h; } kClassic[] = {
		{ 1,  512,  342 },
		{ 2,  512,  384 },
		{ 3,  640,  480 },
		{ 4,  832,  624 },
		{ 5, 1024,  768 },
		{ 6, 1152,  870 },
	};
	s_numResolutions = 0;
	for (int i = 0; i < (int)(sizeof(kClassic) / sizeof(kClassic[0])); i++) {
		if (s_numResolutions >= kMaxResolutions) break;
		s_resolutions[s_numResolutions].displayModeID = kClassic[i].id;
		s_resolutions[s_numResolutions].width  = kClassic[i].w;
		s_resolutions[s_numResolutions].height = kClassic[i].h;
		s_numResolutions++;
	}

	/* Host-derived resolutions: full desktop (ID 100) and half (ID 101).
	   Only added if they differ from every classic entry (±8 px tolerance)
	   and their framebuffer at 32 bpp fits in 15 MB (NuBus super-slot
	   limit before ROM space). */
	static const uint32_t kMaxVRAM = 15u * 1024 * 1024;
	struct { uint32_t id; uint16_t w, h; } hostRes[2];
	int hostCount = 0;

	if (s_hostDesktopW > 0 && s_hostDesktopH > 0) {
		hostRes[0] = { 100, s_hostDesktopW, s_hostDesktopH };
		hostRes[1] = { 101, (uint16_t)(s_hostDesktopW / 2),
		               (uint16_t)(s_hostDesktopH / 2) };
		hostCount = 2;
	}

	for (int hi = 0; hi < hostCount; hi++) {
		uint16_t hw = hostRes[hi].w, hh = hostRes[hi].h;
		if (hw < 512 || hh < 342) continue; /* too small */

		/* Check VRAM cap at 32 bpp */
		uint32_t fb32 = (uint32_t)hw * hh * 4;
		if (fb32 > kMaxVRAM) continue;

		/* Check for near-duplicate of a classic entry */
		bool dup = false;
		for (int ci = 0; ci < s_numResolutions; ci++) {
			int dw = (int)hw - (int)s_resolutions[ci].width;
			int dh = (int)hh - (int)s_resolutions[ci].height;
			if (dw >= -8 && dw <= 8 && dh >= -8 && dh <= 8) {
				dup = true;
				break;
			}
		}
		if (dup) continue;

		if (s_numResolutions >= kMaxResolutions) break;
		s_resolutions[s_numResolutions].displayModeID = hostRes[hi].id;
		s_resolutions[s_numResolutions].width  = hw;
		s_resolutions[s_numResolutions].height = hh;
		s_numResolutions++;
	}
}

/*
	Build the Mac II slot ROM image in g_vidROM.
	Constructs sResource directory, board/video type entries,
	video driver code, and mode parameter blocks for all
	depths from 0 (1 bpp) through maxDepth.
*/
bool VideoDevice::init()
{
	const auto& cfg = g_machine->config();
	int maxDepth = cfg.screenDepth;
	uint16_t bootWidth = cfg.screenWidth;
	uint16_t bootHeight = cfg.screenHeight;

	s_maxDepth = maxDepth;

	/* Build resolution table (classic resolutions only for now) */
	buildResolutionTable();

	/* Find the boot resolution — pick the classic entry matching
	   the configured screen size, or default to 640×480 (ID 3). */
	const ResolutionEntry* bootRes = nullptr;
	int bootIndex = -1;
	for (int i = 0; i < s_numResolutions; i++) {
		if (s_resolutions[i].width == bootWidth &&
			s_resolutions[i].height == bootHeight) {
			bootRes = &s_resolutions[i];
			bootIndex = i;
			break;
		}
	}
	if (!bootRes) {
		/* Configured resolution not in table; use 640×480 */
		bootRes = findResolution(3);
		if (!bootRes) bootRes = &s_resolutions[0];
		for (int i = 0; i < s_numResolutions; i++)
			if (&s_resolutions[i] == bootRes) { bootIndex = i; break; }
	}

	/* Move boot resolution to front of table so its sResource
	   (ID 0x80) is the first video functional sResource in the
	   directory.  The Slot Manager boots from the first video
	   sResource when PRAM has no saved preference. */
	if (bootIndex > 0) {
		ResolutionEntry tmp = s_resolutions[0];
		s_resolutions[0] = s_resolutions[bootIndex];
		s_resolutions[bootIndex] = tmp;
		bootRes = &s_resolutions[0];
	}

	s_currentWidth = bootRes->width;
	s_currentHeight = bootRes->height;
	s_currentDisplayModeID = bootRes->displayModeID;

	/*
		Boot at the highest indexed (CLUT) depth, not a direct-color
		mode.  Direct modes (16 bpp / 32 bpp) require QuickDraw 32-bit
		support that the System only enables after the Slot Manager has
		probed the card.  Booting straight into a direct mode hangs.
		The user can switch to Thousands / Millions via Monitors once
		the desktop is up.
	*/
	int bootDepth = (maxDepth >= 4) ? 3 : maxDepth;
	s_currentDepth = bootDepth;
	g_screenDepth = bootDepth;
	g_screenWidth = bootRes->width;
	g_screenHeight = bootRes->height;
	g_useColorMode = (bootDepth > 0);
	if (s_preferredDepth < 0)
		s_preferredDepth = bootDepth;
	if (s_preferredDisplayModeID < 0)
		s_preferredDisplayModeID = (int)bootRes->displayModeID;

#if VID_dolog
	dbglog_writelnNum("VideoDevice::init maxDepth", maxDepth);
	dbglog_writelnNum("  bootRes width", bootRes->width);
	dbglog_writelnNum("  bootRes height", bootRes->height);
	dbglog_writelnNum("  bootRes displayModeID", bootRes->displayModeID);
	dbglog_writelnNum("  numResolutions", s_numResolutions);
	dbglog_writelnNum("  vidROMSize", cfg.vidROMSize);
	dbglog_writelnNum("  vidMemSize", cfg.vidMemSize);
#endif

	SlotROMWriter w(g_vidROM, cfg.vidROMSize);

	/* --- sResource directory ---
	   One board sResource + ONE video functional sResource.
	   Multiple resolutions are advertised by the DM2 driver protocol
	   (GetNextResolution / GetVideoParameters / SwitchMode), not by
	   multiple video sResources.  Having multiple video sResources
	   makes the System think there are multiple physical displays,
	   which causes a freeze during boot. */
	size_t sRsrcDir = w.pos();
	auto rBoard = w.reserve();
	auto rVideo = w.reserve();
	w.writeEndOfList();

	/* --- Board sResource --- */
	w.patchOffset(rBoard, 0x01);
	auto rBoardType = w.reserve();
	auto rBoardName = w.reserve();
	w.writeDataEntry(0x20, 0x00764D);  /* BoardId: 'vM' */
	auto rVendorInfo = w.reserve();
	w.writeEndOfList();

	w.patchOffset(rBoardType, 0x01);
	w.writeWord(0x0001);  /* catDisplay */
	w.writeWord(0x0000);
	w.writeWord(0x0000);
	w.writeWord(0x0000);

	w.patchOffset(rBoardName, 0x02);
	w.writeString("maxivmac video card");

	/* --- Vendor info --- */
	w.patchOffset(rVendorInfo, 0x24);
	auto rVendorID = w.reserve();
	auto rRevLevel = w.reserve();
	auto rPartNum  = w.reserve();
	w.writeEndOfList();

	w.patchOffset(rVendorID, 0x01);
	w.writeString("maxivmac");

	w.patchOffset(rRevLevel, 0x03);
	w.writeString("3.0");

	w.patchOffset(rPartNum, 0x04);
	w.writeString("MVMv-2");

	/* --- Single video sResource (ID 0x80) ---
	   Contains depth mode entries for the boot resolution.
	   Other resolutions are exposed via GetNextResolution. */
	{
		int bootMaxDepth = maxDepthForResolution(
			bootRes->width, bootRes->height, cfg.vidMemSize);
		if (bootMaxDepth > maxDepth)
			bootMaxDepth = maxDepth;

		w.patchOffset(rVideo, 0x80);

		auto rVideoType = w.reserve();
		auto rVideoName = w.reserve();
		auto rVidDrvrDir = w.reserve();
		w.writeDataEntry(0x08, 0x00000001);  /* sRsrcHWDevId */
		auto rMinorBase   = w.reserve();
		auto rMinorLength = w.reserve();

		/* Reserve one entry per depth mode */
		size_t rModes[kMaxModes];
		for (int d = 0; d <= bootMaxDepth; d++)
			rModes[d] = w.reserve();
		w.writeEndOfList();

		w.patchOffset(rVideoType, 0x01);
		w.writeWord(0x0003);  /* catDisplay */
		w.writeWord(0x0001);  /* typVideo */
		w.writeWord(0x0001);  /* drSwApple */
		w.writeWord(0x0001);  /* drHwTFB */

		w.patchOffset(rVideoName, 0x02);
		w.writeString("Display_Video_Apple_TFB");

		w.patchOffset(rMinorBase, 0x0A);
		w.writeLong(0x00000000);

		w.patchOffset(rMinorLength, 0x0B);
		w.writeLong(cfg.vidMemSize);

		/* Driver directory */
		w.patchOffset(rVidDrvrDir, 0x04);
		auto rDriverEntry = w.reserve();
		w.writeEndOfList();

		w.patchOffset(rDriverEntry, 0x02);  /* sMacOS68020 */
		w.writeLong(4 + sizeof(VidDrvr_contents) + 8);
		w.writeBytes(VidDrvr_contents, sizeof(VidDrvr_contents));
		w.writeWord(kcom_callcheck);
		w.writeWord(kExtnVideo);
		w.writeLong(cfg.extnBlockBase);

		/* Mode entries and VPBlocks for the boot resolution */
		for (int d = 0; d <= bootMaxDepth; d++) {
			w.patchOffset(rModes[d], 0x80 + d);
			auto rVP = w.reserve();
			w.writeDataEntry(0x03, 0x00000001);  /* mPageCnt = 1 */
			w.writeDataEntry(0x04, (d < 4) ? 0x00000000 : 0x00000002);
			w.writeEndOfList();

			w.patchOffset(rVP, 0x01);  /* mVidParams */
			VPBlock::forMode(d, bootRes->width, bootRes->height).writeTo(w);
		}
	}

	/* --- Pad + trailer --- */
	uint32_t usedSoFar = (uint32_t)w.pos() + 20;
#if VID_dolog
	dbglog_writelnNum("  ROM used bytes", (long)usedSoFar);
	dbglog_writelnNum("  ROM resolutions", s_numResolutions);
#endif
	if (usedSoFar > cfg.vidROMSize) {
		ReportAbnormalID(AbnormalID::kVIDEO_vidROMSize_too_small,
			"vidROMSize too small");
		return false;
	}

	size_t padBytes = cfg.vidROMSize - usedSoFar;
	for (size_t i = 0; i < padBytes; i++)
		w.writeByte(0);

	/* ROM trailer (last 20 bytes) */
	w.writeLong((uint32_t)(sRsrcDir - (cfg.vidROMSize - 20)) & 0x00FFFFFF);
	w.writeLong(cfg.vidROMSize);
	w.writeLong(0x00000000);  /* CRC placeholder */
	w.writeByte(0x01);  /* revision level */
	w.writeByte(0x01);  /* format */
	w.writeLong(0x5A932BC7);  /* test pattern */
	w.writeByte(0x00);  /* reserved */
	w.writeByte(0x0F);  /* byte lanes */

	ChecksumSlotROM();

	/* Initialize CLUT for the boot depth (indexed modes only) */
	if (bootDepth > 0 && bootDepth < 4) {
		CLUT_reds[0] = 0xFFFF;
		CLUT_greens[0] = 0xFFFF;
		CLUT_blues[0] = 0xFFFF;
		int clutEnd = clutSizeForDepth(bootDepth) - 1;
		CLUT_reds[clutEnd] = 0;
		CLUT_greens[clutEnd] = 0;
		CLUT_blues[clutEnd] = 0;
	}

	return true;
}

void VideoDevice::update()
{
	if (! Vid_VBLintunenbl) {
		g_wires.set(Wire_VBLinterrupt, 0);
		if (auto* via2 = machine_->findDevice<VIA2Device>())
			via2->iCA1_PulseNtfy();
	}
}

static uint16_t Vid_GetMode()
{
	return 0x80 + s_currentDepth;
}

static tMacErr Vid_SetMode(uint16_t modeID)
{
	int newDepth = modeID - 0x80;
	if (newDepth < 0 || newDepth > s_maxDepth)
		return tMacErr::paramErr;
	if (newDepth == s_currentDepth)
		return tMacErr::noErr;

	s_currentDepth = newDepth;
	g_screenDepth = newDepth;
	g_useColorMode = (newDepth > 0);
	g_colorMappingChanged = true;

	/* Re-initialize CLUT for indexed modes */
	if (newDepth > 0 && newDepth < 4) {
		int cs = clutSizeForDepth(newDepth);
		CLUT_reds[0] = 0xFFFF;
		CLUT_greens[0] = 0xFFFF;
		CLUT_blues[0] = 0xFFFF;
		CLUT_reds[cs - 1] = 0;
		CLUT_greens[cs - 1] = 0;
		CLUT_blues[cs - 1] = 0;
	}

	return tMacErr::noErr;
}

uint16_t VideoDevice::vidReset()
{
	int defDepth = (s_preferredDepth >= 0) ? s_preferredDepth : 0;
	if (defDepth > s_maxDepth)
		defDepth = s_maxDepth;

	s_currentDepth = defDepth;
	g_screenDepth = defDepth;
	g_useColorMode = (defDepth > 0);

	/* Restore preferred resolution (pre-DM2 reboot path).
	   On older Systems the user selects a resolution in Monitors,
	   then reboots; the System calls GetPreferredConfiguration
	   on startup but the video card must also boot into the
	   preferred resolution. */
	if (s_preferredDisplayModeID >= 0) {
		const ResolutionEntry* res =
			findResolution((uint32_t)s_preferredDisplayModeID);
		if (res) {
			uint32_t needBytes = (uint32_t)res->width * res->height
				* (1 << defDepth) / 8;
			if (needBytes <= g_machine->config().vidMemSize) {
				s_currentWidth = res->width;
				s_currentHeight = res->height;
				s_currentDisplayModeID = (uint32_t)s_preferredDisplayModeID;
				g_screenWidth = res->width;
				g_screenHeight = res->height;
				s_resolutionChanged = true;
			}
		}
	}

	return 0x80 + defDepth;
}

#define kCmndVideoFeatures 1
static constexpr int kCmndVideoGetIntEnbl = 2;
static constexpr int kCmndVideoSetIntEnbl = 3;
static constexpr int kCmndVideoClearInt = 4;
static constexpr int kCmndVideoStatus = 5;
static constexpr int kCmndVideoControl = 6;

#define CntrlParam_csCode 0x1A /* control/status code [word] */
#define CntrlParam_csParam 0x1C /* operation-defined parameters */

#define VDPageInfo_csMode 0
#define VDPageInfo_csData 2
#define VDPageInfo_csPage 6
#define VDPageInfo_csBaseAddr 8

#define VDSetEntryRecord_csTable 0
#define VDSetEntryRecord_csStart 4
#define VDSetEntryRecord_csCount 6

#define VDGammaRecord_csGTable 0

/* VDSwitchInfoRec offsets (used by GetCurrentMode) */
#define VDSwitchInfo_csMode 0
#define VDSwitchInfo_csData 2
#define VDSwitchInfo_csPage 6
#define VDSwitchInfo_csBaseAddr 8

/* VDVideoParametersInfoRec offsets */
#define VDVidParams_csDisplayModeID 0
#define VDVidParams_csDepthMode 4
#define VDVidParams_csVPBlockPtr 6
#define VDVidParams_csPageCount 10

/* VDResolutionInfoRec offsets */
#define VDResInfo_csPreviousDisplayModeID 0
#define VDResInfo_csRIDisplayModeID 4
#define VDResInfo_csHorizontalPixels 8
#define VDResInfo_csVerticalLines 12
#define VDResInfo_csRefreshRate 16
#define VDResInfo_csMaxDepthMode 20

/* VDDisplayConnectInfoRec offsets */
#define VDConnectInfo_csDisplayType 0
#define VDConnectInfo_csConnectTaggedType 2
#define VDConnectInfo_csConnectTaggedData 3
#define VDConnectInfo_csConnectFlags 4
#define VDConnectInfo_csDisplayComponent 8
#define VDConnectInfo_csConnectReserved 12

#define VidBaseAddr 0xF9900000

static bool s_useGrayTones = false;

static void FillScreenWithGrayPattern()
{
	int i;
	int j;
	uint32_t *p1 = reinterpret_cast<uint32_t *>(g_vidMem);
	int depth = s_currentDepth;

	if (depth > 0) {
		uint32_t pat;
		switch (depth) {
			case 1: pat = 0xCCCCCCCC; break;
			case 2: pat = 0xF0F0F0F0; break;
			case 3: pat = 0xFF00FF00; break;
			case 4: pat = 0x00007FFF; break;
			case 5: pat = 0x00000000; break;
			default: pat = 0xAAAAAAAA; break;
		}
		int byteWidth = (int)((uint32_t)vMacScreenWidth * (1 << depth) / 8);
		for (i = vMacScreenHeight; --i >= 0; ) {
			for (j = byteWidth >> 2; --j >= 0; ) {
				*p1++ = pat;
				if (depth == 5) {
					pat = (~ pat) & 0x00FFFFFF;
				}
			}
			pat = (~ pat);
			if (depth == 4) pat &= 0x7FFF7FFF;
			else if (depth == 5) pat &= 0x00FFFFFF;
		}
	} else {
		uint32_t pat = 0xAAAAAAAA;

		for (i = vMacScreenHeight; --i >= 0; ) {
			for (j = vMacScreenMonoByteWidth >> 2; --j >= 0; ) {
				*p1++ = pat;
			}
			pat = ~ pat;
		}
	}
}

void VideoDevice::reset()
{
	vidReset();
}

/*
	Write a VPBlock to guest memory at guestPtr.
	Note: physBlockSize is a ROM sBlock header, NOT part of the
	VPBlock struct.  The guest buffer starts at vpBaseOffset.
*/
static void writeVPBlockToGuest(int depth, uint16_t width,
	uint16_t height, uint32_t guestPtr)
{
	VPBlock vp = VPBlock::forMode(depth, width, height);

	put_vm_long(guestPtr +  0, vp.baseOffset);
	put_vm_word(guestPtr +  4, vp.rowBytes);
	put_vm_word(guestPtr +  6, vp.boundsTop);
	put_vm_word(guestPtr +  8, vp.boundsLeft);
	put_vm_word(guestPtr + 10, vp.boundsBottom);
	put_vm_word(guestPtr + 12, vp.boundsRight);
	put_vm_word(guestPtr + 14, vp.version);
	put_vm_word(guestPtr + 16, vp.packType);
	put_vm_long(guestPtr + 18, vp.packSize);
	put_vm_long(guestPtr + 22, vp.hRes);
	put_vm_long(guestPtr + 26, vp.vRes);
	put_vm_word(guestPtr + 30, vp.pixelType);
	put_vm_word(guestPtr + 32, vp.pixelSize);
	put_vm_word(guestPtr + 34, vp.cmpCount);
	put_vm_word(guestPtr + 36, vp.cmpSize);
	put_vm_long(guestPtr + 38, vp.planeBytes);
}

/*
	Handle video extension trap from the guest driver.
	Dispatches control (SetVidMode, SetEntries, SetGamma,
	GrayScreen) and status (GetMode, GetPages, GetGray,
	GetCurrentMode, GetConnection, GetVideoParameters,
	GetNextResolution, etc.) commands.
*/
void VideoDevice::extnVideoAccess(uint32_t p)
{
	tMacErr result = tMacErr::controlErr;

	switch (get_vm_word(p + ExtnDat_commnd)) {
		case kCmndVersion:
#if VID_dolog
			dbglog_WriteNote("Video_Access kCmndVersion");
#endif
			put_vm_word(p + ExtnDat_version, 1);
			result = tMacErr::noErr;
			break;
		case kCmndVideoGetIntEnbl:
#if VID_dolog
			dbglog_WriteNote("Video_Access kCmndVideoGetIntEnbl");
#endif
			put_vm_word(p + 8,
				Vid_VBLintunenbl ? 0 : 1);
			result = tMacErr::noErr;
			break;
		case kCmndVideoSetIntEnbl:
#if VID_dolog
			dbglog_WriteNote("Video_Access kCmndVideoSetIntEnbl");
#endif
			g_wires.set(Wire_VBLintunenbl,
				(0 == get_vm_word(p + 8))
					? 1 : 0);
			result = tMacErr::noErr;
			break;
		case kCmndVideoClearInt:
#if VID_dolog && 0 /* frequent */
			dbglog_WriteNote("Video_Access kCmndVideoClearInt");
#endif
			g_wires.set(Wire_VBLinterrupt, 1);
			result = tMacErr::noErr;
			break;
		case kCmndVideoControl:
			{
				uint32_t CntrlParams = get_vm_long(p + 8);
				uint32_t csParam =
					get_vm_long(CntrlParams + CntrlParam_csParam);
				uint16_t csCode =
					get_vm_word(CntrlParams + CntrlParam_csCode);

				switch (csCode) {
					case 0: /* VidReset */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoControl, VidReset");
						dbglog_writelnNum("  returning mode", Vid_GetMode());
#endif
						put_vm_word(csParam + VDPageInfo_csMode,
							Vid_GetMode());
						put_vm_word(csParam + VDPageInfo_csPage, 0);
						put_vm_long(csParam + VDPageInfo_csBaseAddr,
							VidBaseAddr);
						result = tMacErr::noErr;
						break;
					case 1: /* KillIO */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoControl, KillIO");
#endif
						result = tMacErr::noErr;
						break;
					case 2: /* SetVidMode */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoControl, "
							"SetVidMode");
						dbglog_writelnNum("  requested mode",
							get_vm_word(csParam + VDPageInfo_csMode));
#endif
						if (0 != get_vm_word(
							csParam + VDPageInfo_csPage))
						{
							ReportAbnormalID(AbnormalID::kVIDEO_SetVidMode_not_page_0,
								"SetVidMode not page 0");
						} else {
							result = Vid_SetMode(get_vm_word(
								csParam + VDPageInfo_csMode));
							put_vm_long(csParam + VDPageInfo_csBaseAddr,
								VidBaseAddr);
						}
						break;
					case 3: /* SetEntries */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoControl, "
							"SetEntries");
#endif
						{
							int cs = clutSizeForDepth(s_currentDepth);
							if (s_currentDepth > 0 && s_currentDepth < 4) {
								uint32_t csTable = get_vm_long(
									csParam + VDSetEntryRecord_csTable);
								uint16_t csStart = get_vm_word(
									csParam + VDSetEntryRecord_csStart);
								uint16_t csCount = 1 + get_vm_word(
									csParam + VDSetEntryRecord_csCount);

								if (((uint16_t) 0xFFFF) == csStart) {
									int i;

									result = tMacErr::noErr;
									for (i = 0; i < csCount; ++i) {
										uint16_t j = get_vm_word(csTable + 0);
										if (j == 0) {
											/* ignore input, leave white */
										} else
										if (j == (uint16_t)(cs - 1)) {
											/* ignore input, leave black */
										} else
										if (j >= cs) {
											result = tMacErr::paramErr;
										} else
										{
											uint16_t r =
												get_vm_word(csTable + 2);
											uint16_t g =
												get_vm_word(csTable + 4);
											uint16_t b =
												get_vm_word(csTable + 6);
											CLUT_reds[j] = r;
											CLUT_greens[j] = g;
											CLUT_blues[j] = b;
										}
										csTable += 8;
									}
									g_colorMappingChanged = true;
								} else
								if (csStart + csCount < csStart) {
									result = tMacErr::paramErr;
								} else
								if (csStart + csCount > cs) {
									result = tMacErr::paramErr;
								} else
								{
									int i;

									for (i = 0; i < csCount; ++i) {
										int j = i + csStart;

										if (j == 0) {
											/* ignore input, leave white */
										} else
										if (j == cs - 1) {
											/* ignore input, leave black */
										} else
										{
											uint16_t r =
												get_vm_word(csTable + 2);
											uint16_t g =
												get_vm_word(csTable + 4);
											uint16_t b =
												get_vm_word(csTable + 6);
											CLUT_reds[j] = r;
											CLUT_greens[j] = g;
											CLUT_blues[j] = b;
										}
										csTable += 8;
									}
									g_colorMappingChanged = true;
									result = tMacErr::noErr;
								}
							}
						}
						break;
					case 4: /* SetGamma */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoControl, SetGamma");
#endif
						result = tMacErr::noErr;
						break;
					case 5: /* GrayScreen */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoControl, "
							"GrayScreen");
#endif
						{
							FillScreenWithGrayPattern();
							result = tMacErr::noErr;
						}
						break;
					case 6: /* SetGray */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoControl, SetGray");
#endif
						{
							uint8_t csMode = get_vm_byte(
								csParam + VDPageInfo_csMode);

							s_useGrayTones = (csMode != 0);
							result = tMacErr::noErr;
						}
						break;
					case 9: /* SetDefaultMode */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoControl, "
							"SetDefaultMode");
#endif
						{
							uint16_t mode = get_vm_word(
								csParam + VDSwitchInfo_csMode);
							int depth = mode - 0x80;
							if (depth >= 0 && depth <= s_maxDepth)
								s_preferredDepth = depth;
							result = tMacErr::noErr;
						}
						break;
					case 10: /* SwitchMode (Display Manager 2.0) */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoControl, "
							"SwitchMode");
#endif
						{
							uint16_t mode = get_vm_word(
								csParam + VDSwitchInfo_csMode);
							uint32_t modeID = get_vm_long(
								csParam + VDSwitchInfo_csData);
							uint16_t page = get_vm_word(
								csParam + VDSwitchInfo_csPage);
#if VID_dolog
							dbglog_writelnNum("  mode", mode);
							dbglog_writelnNum("  displayModeID", modeID);
							dbglog_writelnNum("  page", page);
#endif
							/* Write current base addr in case we fail */
							put_vm_long(csParam + VDSwitchInfo_csBaseAddr,
								VidBaseAddr);

							if (page != 0) {
								result = tMacErr::paramErr;
								break;
							}

							const ResolutionEntry* res = findResolution(modeID);
							if (!res) {
								result = tMacErr::paramErr;
								break;
							}

							int newDepth = mode - 0x80;
							if (newDepth < 0 || newDepth > s_maxDepth) {
								result = tMacErr::paramErr;
								break;
							}

							/* Validate VRAM fits */
							uint32_t needBytes = (uint32_t)res->width
								* res->height * (1 << newDepth) / 8;
							if (needBytes > g_machine->config().vidMemSize) {
								result = tMacErr::paramErr;
								break;
							}

							bool resChanged =
								(res->width != s_currentWidth ||
								 res->height != s_currentHeight);

							/* Update video state */
							s_currentDepth = newDepth;
							s_currentWidth = res->width;
							s_currentHeight = res->height;
							s_currentDisplayModeID = modeID;

							/* Write through to DisplayState */
							g_screenWidth = res->width;
							g_screenHeight = res->height;
							g_screenDepth = newDepth;
							g_useColorMode = (newDepth > 0);
							g_colorMappingChanged = true;

							/* Reinit CLUT for indexed modes */
							if (newDepth > 0 && newDepth < 4) {
								int cs = clutSizeForDepth(newDepth);
								CLUT_reds[0] = 0xFFFF;
								CLUT_greens[0] = 0xFFFF;
								CLUT_blues[0] = 0xFFFF;
								CLUT_reds[cs - 1] = 0;
								CLUT_greens[cs - 1] = 0;
								CLUT_blues[cs - 1] = 0;
							}

							if (resChanged) {
								s_resolutionChanged = true;
								/* Reset screen compare buffer to
								   force full redraw */
								if (g_screenCompareBuff) {
									uint32_t bufSz = (uint32_t)res->width
										* res->height * (1 << newDepth) / 8;
									std::memset(g_screenCompareBuff, 0xFF, bufSz);
								}
							}

							FillScreenWithGrayPattern();

							put_vm_long(csParam + VDSwitchInfo_csBaseAddr,
								VidBaseAddr);
							result = tMacErr::noErr;
						}
						break;
					case 16: /* SavePreferredConfiguration */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoControl, "
							"SavePreferredConfiguration");
#endif
						{
							uint16_t mode = get_vm_word(
								csParam + VDSwitchInfo_csMode);
							uint32_t modeID = get_vm_long(
								csParam + VDSwitchInfo_csData);
							int depth = mode - 0x80;
							if (depth >= 0 && depth <= s_maxDepth)
								s_preferredDepth = depth;
							if (findResolution(modeID))
								s_preferredDisplayModeID = (int)modeID;
							result = tMacErr::noErr;
						}
						break;
					default:
						ReportAbnormalID(AbnormalID::kVIDEO_kCmndVideoControl_unknown_csCode,
							"kCmndVideoControl, unknown csCode");
						dbglog_writelnNum("csCode", csCode);
						break;
				}
			}
			break;
		case kCmndVideoStatus:
			{
				uint32_t CntrlParams = get_vm_long(p + 8);
				uint32_t csParam = get_vm_long(
					CntrlParams + CntrlParam_csParam);
				uint16_t csCode = get_vm_word(
					CntrlParams + CntrlParam_csCode);

				result = tMacErr::statusErr;
				switch (csCode) {
					case 2: /* GetMode */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoStatus, GetMode");
#endif
						put_vm_word(csParam + VDPageInfo_csMode,
							Vid_GetMode());
						put_vm_word(csParam + VDPageInfo_csPage, 0);
						put_vm_long(csParam + VDPageInfo_csBaseAddr,
							VidBaseAddr);
						result = tMacErr::noErr;
						break;
					case 3: /* GetEntries */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoStatus, "
							"GetEntries");
#endif
						{
							ReportAbnormalID(AbnormalID::kVIDEO_GetEntries_not_implemented,
								"GetEntries not implemented");
						}
						break;
					case 4: /* GetPages */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoStatus, GetPages");
#endif
						put_vm_word(csParam + VDPageInfo_csPage, 1);
						result = tMacErr::noErr;
						break;
					case 5: /* GetPageAddr */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoStatus,"
							" GetPageAddr");
#endif
						{
							uint16_t csPage = get_vm_word(
								csParam + VDPageInfo_csPage);
							if (0 != csPage) {
								/*
									return tMacErr::statusErr,
									page must be 0
								*/
							} else {
								put_vm_long(
									csParam + VDPageInfo_csBaseAddr,
									VidBaseAddr);
								result = tMacErr::noErr;
							}
						}
						break;
					case 6: /* GetGray */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoStatus, GetGray");
#endif
						put_vm_word(csParam + VDPageInfo_csMode,
							s_useGrayTones ? 0x0100 : 0);
						result = tMacErr::noErr;
						break;
					case 8: /* GetGamma */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoStatus, "
							"GetGamma");
#endif
						/* stub — log if requested */
						result = tMacErr::statusErr;
						break;
					case 9: /* GetDefaultMode */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoStatus, "
							"GetDefaultMode");
#endif
						{
							int defDepth = (s_preferredDepth >= 0)
								? s_preferredDepth : s_maxDepth;
							put_vm_word(csParam + VDSwitchInfo_csMode,
								0x80 + defDepth);
							result = tMacErr::noErr;
						}
						break;
					case 10: /* GetCurrentMode */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoStatus, "
							"GetCurrentMode");
#endif
						put_vm_word(csParam + VDSwitchInfo_csMode,
							0x80 + s_currentDepth);
						put_vm_long(csParam + VDSwitchInfo_csData,
							s_currentDisplayModeID);
						put_vm_word(csParam + VDSwitchInfo_csPage, 0);
						put_vm_long(csParam + VDSwitchInfo_csBaseAddr,
							VidBaseAddr);
						result = tMacErr::noErr;
						break;
					case 12: /* GetConnection */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoStatus, "
							"GetConnection");
#endif
						put_vm_word(csParam + VDConnectInfo_csDisplayType,
							6); /* kVGAConnect */
						put_vm_byte(csParam + VDConnectInfo_csConnectTaggedType, 0);
						put_vm_byte(csParam + VDConnectInfo_csConnectTaggedData, 0);
						put_vm_long(csParam + VDConnectInfo_csConnectFlags,
							0x000E); /* kAllModes | kAllFlags */
						put_vm_long(csParam + VDConnectInfo_csDisplayComponent, 0);
						put_vm_long(csParam + VDConnectInfo_csConnectReserved, 0);
						result = tMacErr::noErr;
						break;
					case 13: /* GetModeTiming */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoStatus, "
							"GetModeTiming");
#endif
						{
							/* VDTimingInfo: csTimingMode at +0 contains
							   the displayModeID.  We need to confirm it
							   exists and return timing flags. */
							uint32_t timingModeID = get_vm_long(
								csParam + 0);
							/* Offsets: +4 reserved, +8 csTimingFormat,
							   +12 csTimingData, +16 csTimingFlags */
							const ResolutionEntry* res =
								findResolution(timingModeID);
							if (!res) {
								result = tMacErr::paramErr;
							} else {
								put_vm_long(csParam + 8,
									0x6465636C); /* 'decl' */
								put_vm_long(csParam + 12, 0);
								/* Flags: mode valid (1), safe (2),
								   shown in Monitors (8).
								   Add default (4) if this is the
								   preferred mode. */
								uint32_t flags = 0x000B;
								if ((int)timingModeID ==
									s_preferredDisplayModeID)
									flags |= 0x0004;
								put_vm_long(csParam + 16, flags);
								result = tMacErr::noErr;
							}
						}
						break;
					case 14: /* GetModeBaseAddress */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoStatus, "
							"GetModeBaseAddress");
#endif
						put_vm_long(csParam + VDPageInfo_csBaseAddr,
							VidBaseAddr);
						result = tMacErr::noErr;
						break;
					case 16: /* GetPreferredConfiguration */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoStatus, "
							"GetPreferredConfiguration");
#endif
						{
							int defDepth = (s_preferredDepth >= 0)
								? s_preferredDepth : s_maxDepth;
							uint32_t prefMode = (s_preferredDisplayModeID >= 0)
								? (uint32_t)s_preferredDisplayModeID
								: s_currentDisplayModeID;
							put_vm_word(csParam + VDSwitchInfo_csMode,
								0x80 + defDepth);
							put_vm_long(csParam + VDSwitchInfo_csData,
								prefMode);
							result = tMacErr::noErr;
						}
						break;
					case 17: /* GetNextResolution */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoStatus, "
							"GetNextResolution");
#endif
						{
							uint32_t prevID = get_vm_long(
								csParam + VDResInfo_csPreviousDisplayModeID);
							const ResolutionEntry* next = nextResolution(prevID);
							if (next) {
								int resMax = maxDepthForResolution(
									next->width, next->height,
									g_machine->config().vidMemSize);
								if (resMax > s_maxDepth) resMax = s_maxDepth;
								put_vm_long(csParam + VDResInfo_csRIDisplayModeID,
									next->displayModeID);
								put_vm_long(csParam + VDResInfo_csHorizontalPixels,
									next->width);
								put_vm_long(csParam + VDResInfo_csVerticalLines,
									next->height);
								put_vm_long(csParam + VDResInfo_csRefreshRate,
									0x00420000); /* ~66.67 Hz fixed-point */
								put_vm_word(csParam + VDResInfo_csMaxDepthMode,
									0x80 + resMax);
								result = tMacErr::noErr;
							} else if (prevID != 0 && findResolution(prevID)) {
								/* prevID was the last — no more */
								put_vm_long(csParam + VDResInfo_csRIDisplayModeID,
									0xFFFFFFFE);
								result = tMacErr::noErr;
							} else {
								result = tMacErr::paramErr;
							}
						}
						break;
					case 18: /* GetVideoParameters */
#if VID_dolog
						dbglog_WriteNote(
							"Video_Access kCmndVideoStatus, "
							"GetVideoParameters");
#endif
						{
							uint32_t displayModeID = get_vm_long(
								csParam + VDVidParams_csDisplayModeID);
							uint16_t depthMode = get_vm_word(
								csParam + VDVidParams_csDepthMode);
							uint32_t vpPtr = get_vm_long(
								csParam + VDVidParams_csVPBlockPtr);
#if VID_dolog
							dbglog_writelnNum("  displayModeID", displayModeID);
							dbglog_writelnNum("  depthMode", depthMode);
							dbglog_writelnNum("  s_maxDepth", s_maxDepth);
#endif
							int depth = depthMode - 0x80;
							const ResolutionEntry* res =
								findResolution(displayModeID);
							if (!res || depth < 0 || depth > s_maxDepth)
							{
								result = tMacErr::paramErr;
							} else {
								writeVPBlockToGuest(depth,
									res->width, res->height, vpPtr);
								put_vm_long(csParam + VDVidParams_csPageCount, 1);
								result = tMacErr::noErr;
							}
						}
						break;
					default:
						ReportAbnormalID(AbnormalID::kVIDEO_Video_Access_kCmndVideoStatus,
							"Video_Access kCmndVideoStatus, "
								"unknown csCode");
						dbglog_writelnNum("csCode", csCode);
						break;
				}
			}
			break;
		default:
			ReportAbnormalID(AbnormalID::kVIDEO_Video_Access_unknown_commnd,
				"Video_Access, unknown commnd");
			break;
	}

	put_vm_word(p + ExtnDat_result, static_cast<uint16_t>(result));
}

/* --- Resolution-change flag accessors (used by host/emulator_shell) --- */

bool Vid_ResolutionChanged()
{
	return s_resolutionChanged;
}

void Vid_ClearResolutionChanged()
{
	s_resolutionChanged = false;
}

uint16_t Vid_CurrentWidth()
{
	return s_currentWidth;
}

uint16_t Vid_CurrentHeight()
{
	return s_currentHeight;
}

void Vid_SetHostDesktop(uint16_t w, uint16_t h)
{
	s_hostDesktopW = w;
	s_hostDesktopH = h;
}

void Vid_MaxResolutionSize(uint32_t* outW, uint32_t* outH)
{
	/* Classic maximums */
	uint32_t maxW = 1152, maxH = 870;

	/* Include host-derived if set */
	if (s_hostDesktopW > 0 && s_hostDesktopH > 0) {
		static const uint32_t kMaxVRAM = 15u * 1024 * 1024;
		if ((uint32_t)s_hostDesktopW * s_hostDesktopH * 4 <= kMaxVRAM) {
			if (s_hostDesktopW > maxW) maxW = s_hostDesktopW;
			if (s_hostDesktopH > maxH) maxH = s_hostDesktopH;
		}
	}

	*outW = maxW;
	*outH = maxH;
}
