/*
	emulator_shell.cpp — Platform-independent emulation orchestration

	All logic previously in sdl.cpp, minus SDL-specific I/O.
*/

#include "platform/emulator_shell.h"
#include "platform/common/osglu_ui.h"
#include "platform/common/osglu_ud.h"
#include "core/machine_obj.h"
#include "core/main.h"
#include "platform/common/osglu_common.h"
#include "platform/common/param_buffers.h"
#include "platform/common/keyboard_map.h"
#include "platform/common/mac_roman.h"
#include "platform/common/path_utils.h"
#include "platform/common/dbglog_platform.h"
#include "platform/common/disk_io.h"
#include "platform/common/rom_loader.h"
#include "platform/common/tick_timer.h"
#include "platform/screen_convert.h"
#include "platform/platform.h"
#include "devices/video.h"

#include <sys/stat.h>
#include <cstdio>
#include <cstring>
#include <string>

/* Forward declarations for platform functions we call directly. */
extern void Sound_SecondNotify();
extern bool Sound_AllocBuffer();
extern void Sound_FreeBuffer();
extern void ReconnectKeyCodes3();
extern void DisconnectKeyCodes3();

/* Global shell pointer for free-function wrappers. */
EmulatorShell* g_shell = nullptr;

/* Early DisplayState for code that runs before g_shell is set. */
static DisplayState s_earlyDisplay;

DisplayState& GetDisplayState()
{
	if (g_shell) return g_shell->display();
	return s_earlyDisplay;
}

/* appParent_ is set in initWhereAmI(). */

/* --- Free-function wrappers (called from core) --- */

void DoneWithDrawingForTick()
{
	if (g_shell) g_shell->doneWithDrawingForTick();
}

void ToggleWantFullScreen()
{
	if (g_shell) g_shell->toggleWantFullScreen();
}

void WaitForNextTick()
{
	/* No longer used — the backend drives the loop.
	   Stub retained for the extern declaration in platform.h. */
}

void MoveBytes(uint8_t * srcPtr, uint8_t * destPtr, int32_t byteCount)
{
	(void) memcpy(reinterpret_cast<char *>(destPtr),
		reinterpret_cast<char *>(srcPtr), byteCount);
}

/* --- Disk insertion helpers --- */

static bool Sony_Insert1a_impl(char *drivepath, bool silentfail)
{
	if (! g_shell->isRomLoaded()) {
		return (tMacErr::noErr == LoadMacRomFrom(drivepath));
	} else {
		return Sony_Insert1(drivepath, silentfail);
	}
}

static char* s_d_arg = nullptr; /* set by shell init */

static bool Sony_Insert2_impl(char *s)
{
	char *d = (nullptr == s_d_arg) ? g_shell->getAppParent() : s_d_arg;
	bool IsOk = false;

	if (nullptr == d) {
		IsOk = Sony_Insert1(s, true);
	} else {
		char *t = nullptr;
		if (tMacErr::noErr == ChildPath(d, s, &t)) {
			IsOk = Sony_Insert1(t, true);
		}
		free(t);
	}

	return IsOk;
}

static bool Sony_InsertIth_impl(int i)
{
	if ((i > 9) || ! FirstFreeDisk(nullptr)) {
		return false;
	}
	char s[] = "disk?.dsk";
	s[4] = '0' + i;
	return Sony_Insert2_impl(s);
}

/* --- EmulatorShell implementation --- */

EmulatorShell::EmulatorShell(PlatformBackend* backend)
	: backend_(backend)
{
}

EmulatorShell::~EmulatorShell()
{
	free(argbBuffer_);
	argbBuffer_ = nullptr;
}

bool EmulatorShell::init(int argc, char** argv)
{
	if (!initPlatform(argc, argv)) return false;
	if (!initMachine()) return false;
	return true;
}

bool EmulatorShell::initPlatform(int argc, char** argv)
{
	argc_ = argc;
	argv_ = argv;
	g_shell = this;

	/* Adopt any display state written before g_shell was set. */
	display_ = s_earlyDisplay;

	windowScale_ = GetEmulatorConfig().windowScale;
	g_speedValue = GetEmulatorConfig().speed;

	const LaunchConfig& lc = GetLaunchConfig();

	/* Seed options from launch config */
	static std::string s_title;
	static std::string s_romDir;
	if (!lc.title.empty()) {
		s_title = lc.title;
		n_arg_ = const_cast<char*>(s_title.c_str());
	}
	if (!lc.romDir.empty()) {
		s_romDir = lc.romDir;
		d_arg_ = const_cast<char*>(s_romDir.c_str());
	}
	s_d_arg = d_arg_;

	/* Initialize paths and drives */
	zapWinStateVars();
	InitDrives();

	if (!backend_->init(this)) return false;
	if (!initWhereAmI()) return false;
	dbglog_open(appParent_);

	return true;
}

bool EmulatorShell::initMachine()
{
	const LaunchConfig& lc = GetLaunchConfig();

	/* Resolve ROM path from the current LaunchConfig. */
	static std::string s_machineRom;
	s_machineRom = ResolveRomPath(lc.romPath, lc.model, lc.romDir);
	if (!s_machineRom.empty())
		romPath_ = const_cast<char*>(s_machineRom.c_str());

	/* Query host desktop size (SDL is initialized by initPlatform)
	   and inform the video subsystem so it can add host-derived
	   resolutions.  Then bump VRAM if the host desktop at 32 bpp
	   exceeds the current allocation. */
	if (g_machine->config().emVidCard) {
		PlatformDisplayBounds db;
		if (backend_->getDisplayBounds(&db) && db.w > 0 && db.h > 0) {
			Vid_SetHostDesktop((uint16_t)db.w, (uint16_t)db.h);
		}
		uint32_t maxW, maxH;
		Vid_MaxResolutionSize(&maxW, &maxH);
		uint32_t fbSize = maxW * maxH * 4;
		/* Round up to next power of two */
		fbSize--;
		fbSize |= fbSize >> 1;
		fbSize |= fbSize >> 2;
		fbSize |= fbSize >> 4;
		fbSize |= fbSize >> 8;
		fbSize |= fbSize >> 16;
		fbSize++;
		if (fbSize > g_machine->config().vidMemSize)
			g_machine->configMut().vidMemSize = fbSize;
	}

	if (!allocMyMemory()) return false;
	if (!scanCommandLine()) return false;
	if (!LoadMacRom(romPath_, d_arg_, appParent_, pref_dir_)) return false;
	if (!loadInitialImages()) return false;
	if (!InitLocationDat()) return false;
	if (!backend_->audioInit()) return false;
	if (!createMainWindow()) return false;

	/* Insert disk images from command line */
	for (const auto& diskPath : lc.diskPaths) {
		(void) Sony_Insert1(const_cast<char*>(diskPath.c_str()), false);
	}

	if (!ProgramMain()) return false;

	/* Allocate ARGB framebuffer for screen conversion.
	   Size for the largest resolution (classic + host-derived) at
	   32 bpp so resolution switches don't need reallocation. */
	{
		uint32_t maxW, maxH;
		Vid_MaxResolutionSize(&maxW, &maxH);
		uint32_t allocW = (vMacScreenWidth > (long)maxW)
			? (uint32_t)vMacScreenWidth : maxW;
		uint32_t allocH = (vMacScreenHeight > (long)maxH)
			? (uint32_t)vMacScreenHeight : maxH;
		argbBuffer_ = static_cast<uint8_t*>(
			calloc(allocW * allocH * 4, 1));
	}
	if (!argbBuffer_) return false;

	/* Build initial palette (B&W default for non-colour models) */
	BuildPalette();

	machineInited_ = true;
	return true;
}

void EmulatorShell::queueMessage(const char* brief, const char* longMsg, bool fatal)
{
	if (savedBriefMsg_ == nullptr) {
		savedBriefMsg_ = brief;
		savedLongMsg_ = longMsg;
		savedFatalMsg_ = fatal;
	}
}

void EmulatorShell::shutdown()
{
	backend_->restoreKeyRepeat();
	ungrabMachine();
	backend_->audioStop();
	backend_->audioShutdown();
	UnInitPbufs();
	UnInitDrives();

	forceShowCursor();

	dbglog_close();

	/* Show any pending error message */
	if (hasQueuedMessage()) {
		backend_->showMessageBox(getBriefMsg(), getLongMsg());
		clearQueuedMessage();
	}

	closeMainWindow();
	uninitWhereAmI();
	unallocMyMemory();

	g_shell = nullptr;
}

/* --- Event dispatch --- */

void EmulatorShell::dispatchEvent(const PlatformEvent& evt)
{
	/* When backgrounded or speed-stopped, block mouse input to the
	   guest.  Housekeeping events (focus, quit) still get through. */
	if (curSpeedStopped_ || backgroundFlag_) {
		switch (evt.type) {
			case PlatformEvent::Type::MouseMove:
			case PlatformEvent::Type::MouseButtonDown:
			case PlatformEvent::Type::MouseButtonUp:
			case PlatformEvent::Type::MouseWheel:
				return;
			default:
				break;
		}
	}

	switch (evt.type) {
		case PlatformEvent::Type::Quit:
			g_requestMacOff = true;
			break;
		case PlatformEvent::Type::FocusGained:
			trueBackgroundFlag_ = false;
			break;
		case PlatformEvent::Type::FocusLost:
			trueBackgroundFlag_ = true;
			break;
		case PlatformEvent::Type::MouseEnter:
			mouseInWindow_ = true;
			break;
		case PlatformEvent::Type::MouseLeave:
			mouseInWindow_ = false;
			break;
		case PlatformEvent::Type::WindowResized:
			backend_->clearScreen();
			break;
		case PlatformEvent::Type::MouseMove:
			if (evt.isRelative) {
				MyMousePositionSetDelta(evt.dx, evt.dy);
				wantCursorHidden_ = true;
			} else {
				mousePositionNotify(evt.x, evt.y);
				if (evt.positionOnly)
					wantCursorHidden_ = false;
			}
			break;
		case PlatformEvent::Type::MouseButtonDown:
			if (!evt.isRelative)
				mousePositionNotify(evt.x, evt.y);
			MyMouseButtonSet(true);
			break;
		case PlatformEvent::Type::MouseButtonUp:
			if (!evt.isRelative)
				mousePositionNotify(evt.x, evt.y);
			MyMouseButtonSet(false);
			break;
		case PlatformEvent::Type::KeyDown:
			Keyboard_updateKeyMap2(evt.macKeyCode, true);
			break;
		case PlatformEvent::Type::KeyUp:
			Keyboard_updateKeyMap2(evt.macKeyCode, false);
			break;
		case PlatformEvent::Type::MouseWheel:
			if (evt.wheelX < 0) {
				Keyboard_updateKeyMap2(MKC_Left, true);
				Keyboard_updateKeyMap2(MKC_Left, false);
			} else if (evt.wheelX > 0) {
				Keyboard_updateKeyMap2(MKC_Right, true);
				Keyboard_updateKeyMap2(MKC_Right, false);
			}
			if (evt.wheelY < 0) {
				Keyboard_updateKeyMap2(MKC_Down, true);
				Keyboard_updateKeyMap2(MKC_Down, false);
			} else if (evt.wheelY > 0) {
				Keyboard_updateKeyMap2(MKC_Up, true);
				Keyboard_updateKeyMap2(MKC_Up, false);
			}
			break;
		case PlatformEvent::Type::FileDrop:
			if (evt.filePath) {
				(void) Sony_Insert1a_impl(const_cast<char*>(evt.filePath), false);
			}
			break;
		default:
			break;
	}
}

/* --- State machine --- */

void EmulatorShell::processSavedTasks()
{
	if (EvtQNeedRecover) {
		EvtQNeedRecover = false;
		EvtQTryRecoverFromFull();
	}

	if (g_requestMacOff) {
		g_requestMacOff = false;
		if (AnyDiskInserted()) {
			fprintf(stderr, "Warning: Quitting with disks still mounted. "
				"Shut down the guest Mac first to avoid data loss.\n");
		}
		g_forceMacOff = true;
	}

	if (g_forceMacOff) {
		return;
	}

	if (trueBackgroundFlag_ != backgroundFlag_) {
		backgroundFlag_ = trueBackgroundFlag_;
		if (trueBackgroundFlag_) {
			enterBackground();
		} else {
			leaveBackground();
		}
	}

	if (curSpeedStopped_ != (g_speedStopped ||
		(backgroundFlag_ && ! g_runInBackground)))
	{
		curSpeedStopped_ = ! curSpeedStopped_;
		if (curSpeedStopped_) {
			enterSpeedStopped();
		} else {
			leaveSpeedStopped();
		}
	}

	if ((useMagnify_ != g_wantMagnify)
		|| (useFullScreen_ != g_wantFullScreen))
	{
		(void) reCreateMainWindow();
	}

	if (grabMachine_ != (
		useFullScreen_ &&
		! (trueBackgroundFlag_ || curSpeedStopped_)))
	{
		grabMachine_ = ! grabMachine_;
		if (grabMachine_) {
			grabMachine();
		} else {
			ungrabMachine();
		}
	}

	if (0 != g_requestIthDisk) {
		Sony_InsertIth_impl(g_requestIthDisk);
		g_requestIthDisk = 0;
	}

	if (g_sonyNewDiskWanted) {
		if (g_sonyNewDiskName != NOT_A_PBUF) {
			uint8_t *p = static_cast<uint8_t *>(PbufDat[g_sonyNewDiskName]);
			uint32_t L = PbufSize[g_sonyNewDiskName];
			char drivename[256];
			uint32_t j = 0;
			for (uint32_t i = 0; i < L && j < sizeof(drivename) - 1; ++i) {
				uint8_t x = p[i];
				if (x < 32) {
					x = '-';
				} else {
					switch (x) {
						case '/': case '<': case '>':
						case '|': case ':':
							x = '-';
						default:
							break;
					}
				}
				drivename[j++] = x;
			}
			drivename[j] = 0;
			if (j > 0 && drivename[0] == '.') {
				drivename[0] = '-';
			}
			MakeNewDisk(g_sonyNewDiskSize, drivename);
			PbufDispose(g_sonyNewDiskName);
			g_sonyNewDiskName = NOT_A_PBUF;
		} else {
			char defaultName[] = "untitled.dsk";
			MakeNewDisk(g_sonyNewDiskSize, defaultName);
		}
		g_sonyNewDiskWanted = false;
	}

	if (haveCursorHidden_ != (wantCursorHidden_
		&& ! (trueBackgroundFlag_ || curSpeedStopped_)))
	{
		haveCursorHidden_ = ! haveCursorHidden_;
		haveCursorHidden_ ? backend_->hideCursor() : backend_->showCursor();
	}
}

/* --- Timing --- */

bool EmulatorShell::tickIsDue()
{
	if (g_SkipThrottle) {
		return true;
	}
	UpdateTrueEmulatedTime();
	return g_trueEmulatedTime != g_onTrueTime;
}

void EmulatorShell::runOneTick()
{
	if (!g_SkipThrottle) {
		if (CheckDateTime()) {
			Sound_SecondNotify();
		}

	} else {
		drawChangesAndClear();
	}

	++g_onTrueTime;
	RunEmulatedTicksToTrueTime();
}

uint32_t EmulatorShell::getDelayMs()
{
	return GetTimerDelay();
}

bool EmulatorShell::shouldQuit() const
{
	return g_forceMacOff;
}

bool EmulatorShell::isSpeedStopped() const
{
	return curSpeedStopped_;
}

/* --- Framebuffer --- */

void EmulatorShell::doneWithDrawingForTick()
{
	/* Check if the guest switched resolution via SwitchMode */
	if (Vid_ResolutionChanged()) {
		Vid_ClearResolutionChanged();
		uint16_t newW = Vid_CurrentWidth();
		uint16_t newH = Vid_CurrentHeight();
		backend_->onResolutionChanged(newW, newH);
		display_.screenChanged = true;
	}

	if (g_haveMouseMotion) {
		AutoScrollScreen();
	}
	drawChangesAndClear();
}

void EmulatorShell::drawChangesAndClear()
{
	if (display_.screenChanged) {
		int depth = (display_.useColorMode && vMacScreenDepth > 0)
			? vMacScreenDepth : 0;

		if (depth < 4)
			BuildPalette();

		const uint32_t* pal = (depth < 4) ? display_.clut32 : nullptr;

		ConvertScreen(g_screenCompareBuff,
			reinterpret_cast<uint32_t*>(argbBuffer_),
			pal, depth, vMacScreenWidth, vMacScreenHeight);

		display_.screenChanged = false;
		framebufferDirty_ = true;
	}
}

void EmulatorShell::convertFramebuffer()
{
	int depth = (display_.useColorMode && vMacScreenDepth > 0)
		? vMacScreenDepth : 0;
	const uint32_t* pal = (depth < 4) ? display_.clut32 : nullptr;

	ConvertScreen(g_screenCompareBuff,
		reinterpret_cast<uint32_t*>(argbBuffer_),
		pal, depth, vMacScreenWidth, vMacScreenHeight);
}

/* --- Mouse --- */

void EmulatorShell::mousePositionNotify(int NewMousePosh, int NewMousePosv)
{
	bool ShouldHaveCursorHidden = true;

	if (useFullScreen_) {
		NewMousePosh += g_viewHStart - hOffset_;
		NewMousePosv += g_viewVStart - vOffset_;
	}

	if (NewMousePosh < 0) {
		NewMousePosh = 0;
		ShouldHaveCursorHidden = false;
	} else if (NewMousePosh >= vMacScreenWidth) {
		NewMousePosh = vMacScreenWidth - 1;
		ShouldHaveCursorHidden = false;
	}
	if (NewMousePosv < 0) {
		NewMousePosv = 0;
		ShouldHaveCursorHidden = false;
	} else if (NewMousePosv >= vMacScreenHeight) {
		NewMousePosv = vMacScreenHeight - 1;
		ShouldHaveCursorHidden = false;
	}

	if (useFullScreen_ || fullscreenHint_) {
		ShouldHaveCursorHidden = true;
	}

	MyMousePositionSet(NewMousePosh, NewMousePosv);

	wantCursorHidden_ = ShouldHaveCursorHidden;
}



/* --- Grab/ungrab --- */

void EmulatorShell::grabMachine()
{
	backend_->setMouseGrab(true);
	g_haveMouseMotion = true;
}

void EmulatorShell::ungrabMachine()
{
	g_haveMouseMotion = false;
	backend_->setMouseGrab(false);
}

/* --- Background/speed --- */

void EmulatorShell::forceShowCursor()
{
	if (haveCursorHidden_) {
		haveCursorHidden_ = false;
		backend_->showCursor();
	}
}

void EmulatorShell::leaveBackground()
{
	ReconnectKeyCodes3();
	backend_->disableKeyRepeat();
}

void EmulatorShell::enterBackground()
{
	backend_->restoreKeyRepeat();
	DisconnectKeyCodes3();
	forceShowCursor();
}

void EmulatorShell::leaveSpeedStopped()
{
	backend_->audioStart();
	StartUpTimeAdjust();
}

void EmulatorShell::enterSpeedStopped()
{
	backend_->audioStop();
}

/* --- Window management --- */

bool EmulatorShell::createMainWindow()
{
	int NewWindowX;
	int NewWindowY;
	int NewWindowHeight = vMacScreenHeight;
	int NewWindowWidth = vMacScreenWidth;

	if (useMagnify_) {
		NewWindowHeight *= windowScale_;
		NewWindowWidth *= windowScale_;
	}

	if (useFullScreen_) {
		NewWindowX = 0x1FFF0000; /* SDL_WINDOWPOS_UNDEFINED */
		NewWindowY = 0x1FFF0000;
	} else {
		int WinIndx = useMagnify_ ? kMagStateMagnifgy : kMagStateNormal;

		if (! havePositionWins_[WinIndx]) {
			NewWindowX = 0x2FFF0000; /* SDL_WINDOWPOS_CENTERED */
			NewWindowY = 0x2FFF0000;
		} else {
			NewWindowX = winPositionsX_[WinIndx];
			NewWindowY = winPositionsY_[WinIndx];
		}

		curWinIndx_ = WinIndx;
	}

	if (!backend_->createWindow(windowTitle(),
		NewWindowWidth, NewWindowHeight, useFullScreen_))
	{
		return false;
	}

	backend_->setWindowPosition(NewWindowX, NewWindowY);

	if (useFullScreen_) {
		int wr, hr;
		backend_->getWindowSize(&wr, &hr);

		g_viewHSize = wr;
		g_viewVSize = hr;
		if (useMagnify_) {
			g_viewHSize /= windowScale_;
			g_viewVSize /= windowScale_;
		}
		if (g_viewHSize >= vMacScreenWidth) {
			g_viewHStart = 0;
			g_viewHSize = vMacScreenWidth;
		} else {
			g_viewHSize &= ~ 1;
		}
		if (g_viewVSize >= vMacScreenHeight) {
			g_viewVStart = 0;
			g_viewVSize = vMacScreenHeight;
		} else {
			g_viewVSize &= ~ 1;
		}

		if (wr > NewWindowWidth) {
			hOffset_ = (wr - NewWindowWidth) / 2;
		} else {
			hOffset_ = 0;
		}
		if (hr > NewWindowHeight) {
			vOffset_ = (hr - NewWindowHeight) / 2;
		} else {
			vOffset_ = 0;
		}
	}

	return true;
}

void EmulatorShell::closeMainWindow()
{
	backend_->destroyWindow();
}

bool EmulatorShell::reCreateMainWindow()
{
	bool HadCursorHidden = haveCursorHidden_;
	int OldWinState = useFullScreen_ ? kWinStateFullScreen : kWinStateWindowed;
	int OldMagState = useMagnify_ ? kMagStateMagnifgy : kMagStateNormal;

	winMagStates_[OldWinState] = OldMagState;

	if (! useFullScreen_) {
		backend_->getWindowPosition(
			&winPositionsX_[curWinIndx_],
			&winPositionsY_[curWinIndx_]);
		havePositionWins_[curWinIndx_] = true;
	}

	forceShowCursor();

	if (grabMachine_) {
		grabMachine_ = false;
		ungrabMachine();
	}

	/* Save state, destroy, recreate */
	closeMainWindow();

	useMagnify_ = g_wantMagnify;
	useFullScreen_ = g_wantFullScreen;

	if (! createMainWindow()) {
		/* Restore old state if creation fails */
		g_wantFullScreen = useFullScreen_;
		g_wantMagnify = useMagnify_;
		return false;
	}

	if (HadCursorHidden) {
		/* Cursor was hidden before recreate — re-hide it.
		   Position will update from the next mouse event. */
		wantCursorHidden_ = true;
	}

	return true;
}

void EmulatorShell::zapWinStateVars()
{
	for (int i = 0; i < kNumMagStates; ++i) {
		havePositionWins_[i] = false;
	}
	for (int i = 0; i < kNumWinStates; ++i) {
		winMagStates_[i] = kMagStateAuto;
	}
}

/* --- Fullscreen toggle --- */

void EmulatorShell::toggleWantFullScreen()
{
	g_wantFullScreen = ! g_wantFullScreen;

	int OldWinState = useFullScreen_ ? kWinStateFullScreen : kWinStateWindowed;
	int OldMagState = useMagnify_ ? kMagStateMagnifgy : kMagStateNormal;
	int NewWinState = g_wantFullScreen ? kWinStateFullScreen : kWinStateWindowed;
	int NewMagState = winMagStates_[NewWinState];

	winMagStates_[OldWinState] = OldMagState;
	if (kMagStateAuto != NewMagState) {
		g_wantMagnify = (kMagStateMagnifgy == NewMagState);
	} else {
		g_wantMagnify = false;
		if (g_wantFullScreen) {
			PlatformDisplayBounds r;
			if (backend_->getDisplayBounds(&r)) {
				if ((r.w >= vMacScreenWidth * windowScale_)
					&& (r.h >= vMacScreenHeight * windowScale_))
				{
					g_wantMagnify = true;
				}
			}
		}
	}
}

/* --- Disk --- */

bool EmulatorShell::insertDiskOrRom(const char* path, bool silent)
{
	return Sony_Insert1a_impl(const_cast<char*>(path), silent);
}

/* --- Window title --- */

const char* EmulatorShell::windowTitle() const
{
	return (nullptr != n_arg_) ? n_arg_ : kStrAppName;
}

/* --- Command line parsing --- */

bool EmulatorShell::scanCommandLine()
{
	char *pa;
	int i = 1;

	while (i < argc_) {
		pa = argv_[i++];
		if ('-' == pa[0]) {
			if (0 == strcmp(pa, "--rom")) {
				if (i < argc_) {
					romPath_ = argv_[i++];
				}
			} else if (0 == strncmp(pa, "--rom=", 6)) {
				romPath_ = pa + 6;
			} else {
				dbglog_writeln("ignoring command line argument");
				dbglog_writeln(pa);
				if ('-' == pa[1] && nullptr == strchr(pa, '=')) {
					if (i < argc_ && '-' != argv_[i][0]) {
						++i;
					}
				}
			}
		}
	}

	return true;
}

/* --- Memory --- */

bool EmulatorShell::allocMyMemory()
{
	if (!dbglog_ReserveAlloc())
		goto fail;
	if (!AllocBlock(&g_rom, g_machine->config().romSize, false))
		goto fail;
	/* Allocate screen compare buffer for the largest resolution
	   at the deepest mode (32 bpp) so resolution switches don't
	   need reallocation. */
	{
		uint32_t maxW, maxH;
		Vid_MaxResolutionSize(&maxW, &maxH);
		uint32_t allocW = ((uint32_t)g_screenWidth > maxW)
			? (uint32_t)g_screenWidth : maxW;
		uint32_t allocH = ((uint32_t)g_screenHeight > maxH)
			? (uint32_t)g_screenHeight : maxH;
		uint32_t maxBytes = allocW * allocH * 4;
		if (!display_.allocBuffers(maxBytes))
			goto fail;
	}
	if (!Sound_AllocBuffer())
		goto fail;
	if (!EmulationReserveAlloc())
		goto fail;

	return true;
fail:
	MacMsg("Out of Memory", "Not enough memory is available", true);
	return false;
}

void EmulatorShell::unallocMyMemory()
{
	free(g_rom); g_rom = nullptr;
	display_.freeBuffers();
	Sound_FreeBuffer();
	EmulationFreeAlloc();
}

/* --- Where Am I --- */

bool EmulatorShell::initWhereAmI()
{
	appParent_ = const_cast<char *>(backend_->getAppParent());
	pref_dir_ = backend_->getPrefDir("gryphel", "maxivmac");

	return true;
}

void EmulatorShell::uninitWhereAmI()
{
	backend_->freePath(pref_dir_);
	pref_dir_ = nullptr;
}

/* --- Load initial images --- */

bool EmulatorShell::loadInitialImages()
{
	if (! AnyDiskInserted()) {
		for (int i = 1; Sony_InsertIth_impl(i); ++i) {
			/* stop on first error */
		}
	}
	return true;
}
