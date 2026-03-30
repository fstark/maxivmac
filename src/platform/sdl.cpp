/*
	SDL3 platform backend

	Window management, audio, input, file I/O, screen rendering,
	and event loop for macOS / Linux / Windows via SDL3.
*/

#include "platform/common/osglu_ui.h"
#include "platform/common/osglu_ud.h"
#include "core/machine_obj.h"
#include "core/main.h"

#include <sys/stat.h>


/* --- some simple utilities --- */

void MoveBytes(uint8_t * srcPtr, uint8_t * destPtr, int32_t byteCount)
{
	(void) memcpy(reinterpret_cast<char *>(destPtr), reinterpret_cast<char *>(srcPtr), byteCount);
}

/* --- control mode and internationalization --- */

#define DBGLOG_OSG_INIT 0

#include "platform/common/intl_chars.h"


static char *d_arg = nullptr;
static char *n_arg = nullptr;

char *app_parent = nullptr;
static char *pref_dir = nullptr;

/* --- information about the environment --- */

#include "platform/common/osglu_common.h"

#include "platform/common/param_buffers.h"

#include "platform/common/control_mode.h"

#include "platform/common/mac_roman.h"
#include "platform/common/path_utils.h"
#include "platform/common/dbglog_platform.h"
#include "platform/common/disk_io.h"
#include "platform/common/rom_loader.h"

/* --- text translation --- */

static void NativeStrFromCStr(char *r, const char *s)
{
	uint8_t ps[ClStrMaxLength];
	int i;
	int L;

	ClStrFromSubstCStr(&L, ps, s);

	for (i = 0; i < L; ++i) {
		r[i] = Cell2PlainAsciiMap[ps[i]];
	}

	r[L] = 0;
}

/* --- drives --- */









/* Register an open file as an inserted disk image.
   Finds a free drive slot and marks it inserted. */

// Open a disk image (read-write if possible, else read-only).




/* --- disk insertion helpers --- */

static bool Sony_Insert1a(char *drivepath, bool silentfail)
{
	bool v;

	if (! g_romLoaded) {
		v = (tMacErr::noErr == LoadMacRomFrom(drivepath));
	} else {
		v = Sony_Insert1(drivepath, silentfail);
	}

	return v;
}

static bool Sony_Insert2(char *s)
{
	char *d =
		(nullptr == d_arg) ? app_parent :
		d_arg;
	bool IsOk = false;

	if (nullptr == d) {
		IsOk = Sony_Insert1(s, true);
	} else
	{
		char *t = nullptr;

		if (tMacErr::noErr == ChildPath(d, s, &t)) {
			IsOk = Sony_Insert1(t, true);
		}

		free(t);
	}

	return IsOk;
}

static bool Sony_InsertIth(int i)
{
	bool v;

	if ((i > 9) || ! FirstFreeDisk(nullptr)) {
		v = false;
	} else {
		char s[] = "disk?.dsk";

		s[4] = '0' + i;

		v = Sony_Insert2(s);
	}

	return v;
}

static bool LoadInitialImages()
{
	if (! AnyDiskInserted()) {
		int i;

		for (i = 1; Sony_InsertIth(i); ++i) {
			/* stop on first error (including file not found) */
		}
	}

	return true;
}

/* --- video out --- */

static int hOffset;
static int vOffset;

static bool s_useFullScreen = false;

static bool s_useMagnify = true;


static bool s_backgroundFlag = false;
static bool s_trueBackgroundFlag = false;
static bool s_curSpeedStopped = true;

static int s_windowScale = 1;

#include "platform/sdl_backend.h"

static SdlBackend* s_backend = nullptr;

#include "platform/screen_convert.h"


/* Convert a dirty rectangle of the emulated framebuffer into
   host pixels and present it via the SDL texture/surface. */
static void HaveChangedScreenBuff(uint16_t top, uint16_t left,
	uint16_t bottom, uint16_t right)
{
	uint32_t top2;
	uint32_t left2;
	uint32_t bottom2;
	uint32_t right2;
	void *pixels;
	int pitch;

	SDL_FRect
	src_rect, dst_rect;
	int XDest;
	int YDest;
	int DestWidth;
	int DestHeight;

	if (s_useFullScreen)
	{
		if (top < g_viewVStart) {
			top = g_viewVStart;
		}
		if (left < g_viewHStart) {
			left = g_viewHStart;
		}
		if (bottom > g_viewVStart + g_viewVSize) {
			bottom = g_viewVStart + g_viewVSize;
		}
		if (right > g_viewHStart + g_viewHSize) {
			right = g_viewHStart + g_viewHSize;
		}

		if ((top >= bottom) || (left >= right)) {
			return;
		}
	}

	XDest = left;
	YDest = top;
	DestWidth = (right - left);
	DestHeight = (bottom - top);

	if (s_useFullScreen)
	{
		XDest -= g_viewHStart;
		YDest -= g_viewVStart;
	}

	/* SDL3 logical presentation handles magnification —
	   dst_rect is in logical (unmagnified) coordinates. */

	if (s_useFullScreen)
	{
		XDest += hOffset;
		YDest += vOffset;
	}


	top2 = top;
	left2 = left;
	bottom2 = bottom;
	right2 = right;


	if (
		!
		SDL_LockTexture(s_backend->texture(), nullptr, &pixels, &pitch)
	) {
		return;
	}

	{

	int bpp = s_backend->format()->bytes_per_pixel;
	uint32_t ExpectedPitch = vMacScreenWidth * bpp;

	if ((0 == ((bpp - 1) & bpp)) /* a power of 2 */
		&& ((uint32_t)pitch == ExpectedPitch)
		&& (vMacScreenDepth <= 3 || ! g_useColorMode)
		)
	{
		ScalingBuff = static_cast<uint8_t *>(pixels);
		BuildClutTable(bpp);
		ConvertRect(bpp, top, left, bottom, right);
	} else {
		ConvertRectSlow(static_cast<uint8_t *>(pixels), pitch, bpp,
			top2, left2, bottom2, right2);
	}

	}

	SDL_UnlockTexture(s_backend->texture());

	src_rect.x = left2;
	src_rect.y = top2;
	src_rect.w = right2 - left2;
	src_rect.h = bottom2 - top2;

	dst_rect.x = XDest;
	dst_rect.y = YDest;
	dst_rect.w = DestWidth;
	dst_rect.h = DestHeight;

	/* SDL_RenderClear(my_renderer); */
	SDL_RenderTexture
	(s_backend->renderer(), s_backend->texture(), &src_rect, &dst_rect);
	
	SDL_RenderPresent(s_backend->renderer());

}

static void DrawChangesAndClear()
{
	if (g_screenChangedBottom > g_screenChangedTop) {
		HaveChangedScreenBuff(g_screenChangedTop, g_screenChangedLeft,
			g_screenChangedBottom, g_screenChangedRight);
		ScreenClearChanges();
	}
}

void DoneWithDrawingForTick()
{
	if (g_haveMouseMotion) {
		AutoScrollScreen();
	}
	DrawChangesAndClear();
}

/* --- mouse --- */

/* cursor hiding */

static bool s_haveCursorHidden = false;
static bool s_wantCursorHidden = false;

static void ForceShowCursor()
{
	if (s_haveCursorHidden) {
		s_haveCursorHidden = false;
		s_backend->showCursor();
	}
}

/* cursor moving */



static bool MoveMouse(int16_t h, int16_t v)
{
	/*
		OSGLUxxx common:
		Move the cursor to the point h, v on the emulated screen.
		If detect that this fails return false,
			otherwise return true.
		(On some platforms it is possible to move the curser,
			but there is no way to detect failure.)
	*/

	if (s_useFullScreen)
	{
		h -= g_viewHStart;
		v -= g_viewVStart;
	}

	if (s_useMagnify) {
		h *= s_windowScale;
		v *= s_windowScale;
	}

	if (s_useFullScreen)
	{
		h += hOffset;
		v += vOffset;
	}

	SDL_WarpMouseInWindow(s_backend->window(), h, v);

	return true;
}

/* cursor state */

static void MousePositionNotify(int NewMousePosh, int NewMousePosv)
{
	bool ShouldHaveCursorHidden = true;

	/* SDL_ConvertEventToRenderCoordinates already maps to
	   logical coordinates, so no manual scaling needed. */

	if (s_useFullScreen)
	{
		NewMousePosh -= hOffset;
		NewMousePosv -= vOffset;
	}

	if (s_useFullScreen)
	{
		NewMousePosh += g_viewHStart;
		NewMousePosv += g_viewVStart;
	}

	if (g_haveMouseMotion) {
		MyMousePositionSetDelta(NewMousePosh - g_savedMouseH,
			NewMousePosv - g_savedMouseV);
		g_savedMouseH = NewMousePosh;
		g_savedMouseV = NewMousePosv;
	} else
	{
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

		if (s_useFullScreen)
		{
			ShouldHaveCursorHidden = true;
		}

		/* if (ShouldHaveCursorHidden || CurMouseButton) */
		/*
			for a game like arkanoid, would like mouse to still
			move even when outside window in one direction
		*/
		MyMousePositionSet(NewMousePosh, NewMousePosv);
	}

	s_wantCursorHidden = ShouldHaveCursorHidden;
}


static void CheckMouseState()
{
	/*
		this doesn't work as desired, doesn't get mouse movements
		when outside of our window.
	*/
	float
	x, y;

	(void) SDL_GetMouseState(&x, &y);
	MousePositionNotify(x, y);
}

/* --- keyboard input --- */

#include "platform/sdl_keyboard.h"

/* --- time, date, location --- */

#include "platform/common/tick_timer.h"

/* --- sound --- */

#include "platform/sdl_sound.h"


/* --- basic dialogs --- */

static void CheckSavedMacMsg()
{
	if (nullptr != SavedBriefMsg) {
		char briefMsg0[ClStrMaxLength + 1];
		char longMsg0[ClStrMaxLength + 1];

		NativeStrFromCStr(briefMsg0, SavedBriefMsg);
		NativeStrFromCStr(longMsg0, SavedLongMsg);

		s_backend->showMessageBox(SavedBriefMsg, SavedLongMsg);

		SavedBriefMsg = nullptr;
	}
}


/* --- event handling for main window --- */


static bool s_caughtMouse = false;

/* Dispatch an SDL event: quit, keyboard, mouse, window resize,
   drag-and-drop, etc. */
static void HandleTheEvent(SDL_Event *event)
{
	if (!s_useFullScreen) {
		SDL_ConvertEventToRenderCoordinates(
			s_backend->renderer(),
			event
		);
	}

	switch (event->type) {
		case
			SDL_EVENT_QUIT
			:
			g_requestMacOff = true;
			break;
				case
					SDL_EVENT_WINDOW_FOCUS_GAINED
					:
					s_trueBackgroundFlag = 0;
					break;
				case
					SDL_EVENT_WINDOW_FOCUS_LOST
					:
					s_trueBackgroundFlag = 1;
					break;
				case
					SDL_EVENT_WINDOW_MOUSE_ENTER
					:
					s_caughtMouse = 1;
					break;
				case
					SDL_EVENT_WINDOW_MOUSE_LEAVE
					:
					s_caughtMouse = 0;
					break;
				case SDL_EVENT_WINDOW_RESIZED:
					SDL_RenderClear(s_backend->renderer());
					break;
		case
			SDL_EVENT_MOUSE_MOTION
			:
			{
				MousePositionNotify(
					event->motion.x, event->motion.y);
			}
			break;
		case
			SDL_EVENT_MOUSE_BUTTON_DOWN
			:
			/* any mouse button, we don't care which */
			{
				MousePositionNotify(
					event->button.x, event->button.y);
			}
			MyMouseButtonSet(true);
			break;
		case
			SDL_EVENT_MOUSE_BUTTON_UP
			:
			{
				MousePositionNotify(
					event->button.x, event->button.y);
			}
			MyMouseButtonSet(false);
			break;
		case
			SDL_EVENT_KEY_DOWN
			:
			DoKeyCode(&event->key, true);
			break;
		case
			SDL_EVENT_KEY_UP
			:
			DoKeyCode(&event->key, false);
			break;
		case
			SDL_EVENT_MOUSE_WHEEL
			:
			if (event->wheel.x < 0) {
				Keyboard_updateKeyMap2(MKC_Left, true);
				Keyboard_updateKeyMap2(MKC_Left, false);
			} else if (event->wheel.x > 0) {
				Keyboard_updateKeyMap2(MKC_Right, true);
				Keyboard_updateKeyMap2(MKC_Right, false);
			}
			if (event->wheel.y < 0) {
				Keyboard_updateKeyMap2(MKC_Down, true);
				Keyboard_updateKeyMap2(MKC_Down, false);
			} else if(event->wheel.y > 0) {
				Keyboard_updateKeyMap2(MKC_Up, true);
				Keyboard_updateKeyMap2(MKC_Up, false);
			}
			break;
		case
			SDL_EVENT_DROP_FILE
			:
			{
				char *s = (char *)event->drop.
				data
				;

				(void) Sony_Insert1a(s, false);
				SDL_RaiseWindow(s_backend->window());
			}
			break;
	}
}

/* --- main window creation and disposal --- */

static int my_argc;
static char **my_argv;

static bool Screen_Init()
{
	return s_backend->init(nullptr);
}

static bool s_grabMachine = false;

static void GrabTheMachine()
{
	s_backend->setMouseGrab(true);


	/*
		if magnification changes, need to reset,
		even if g_haveMouseMotion already true
	*/
	if (MoveMouse(g_viewHStart + (g_viewHSize / 2),
		g_viewVStart + (g_viewVSize / 2)))
	{
		g_savedMouseH = g_viewHStart + (g_viewHSize / 2);
		g_savedMouseV = g_viewVStart + (g_viewVSize / 2);
		g_haveMouseMotion = true;
	}

}

static void UngrabMachine()
{

	if (g_haveMouseMotion) {
		(void) MoveMouse(g_curMouseH, g_curMouseV);

		g_haveMouseMotion = false;
	}

	s_backend->setMouseGrab(false);
}

static void MouseConstrain()
{
	int16_t shiftdh;
	int16_t shiftdv;

	if (g_savedMouseH < g_viewHStart + (g_viewHSize / 4)) {
		shiftdh = g_viewHSize / 2;
	} else if (g_savedMouseH > g_viewHStart + g_viewHSize - (g_viewHSize / 4)) {
		shiftdh = - g_viewHSize / 2;
	} else {
		shiftdh = 0;
	}
	if (g_savedMouseV < g_viewVStart + (g_viewVSize / 4)) {
		shiftdv = g_viewVSize / 2;
	} else if (g_savedMouseV > g_viewVStart + g_viewVSize - (g_viewVSize / 4)) {
		shiftdv = - g_viewVSize / 2;
	} else {
		shiftdv = 0;
	}
	if ((shiftdh != 0) || (shiftdv != 0)) {
		g_savedMouseH += shiftdh;
		g_savedMouseV += shiftdv;
		if (! MoveMouse(g_savedMouseH, g_savedMouseV)) {
			g_haveMouseMotion = false;
		}
	}
}


enum {
	kMagStateNormal,
	kMagStateMagnifgy,
	kNumMagStates
};

#define kMagStateAuto kNumMagStates

static int s_curWinIndx;
static bool HavePositionWins[kNumMagStates];
static int WinPositionsX[kNumMagStates];
static int WinPositionsY[kNumMagStates];

static bool CreateMainWindow()
{
	int NewWindowX;
	int NewWindowY;
	int NewWindowHeight = vMacScreenHeight;
	int NewWindowWidth = vMacScreenWidth;
	bool v = false;

	if (s_useMagnify) {
		NewWindowHeight *= s_windowScale;
		NewWindowWidth *= s_windowScale;
	}

	if (s_useFullScreen)
	{
		NewWindowX = SDL_WINDOWPOS_UNDEFINED;
		NewWindowY = SDL_WINDOWPOS_UNDEFINED;
	}
	else
	{
		int WinIndx;

		if (s_useMagnify) {
			WinIndx = kMagStateMagnifgy;
		} else
		{
			WinIndx = kMagStateNormal;
		}

		if (! HavePositionWins[WinIndx]) {
			NewWindowX = SDL_WINDOWPOS_CENTERED;
			NewWindowY = SDL_WINDOWPOS_CENTERED;
		} else {
			NewWindowX = WinPositionsX[WinIndx];
			NewWindowY = WinPositionsY[WinIndx];
		}

		s_curWinIndx = WinIndx;
	}

	if (!s_backend->createWindow(
		(nullptr != n_arg) ? n_arg : kStrAppName,
		NewWindowWidth, NewWindowHeight, s_useFullScreen))
	{
		return false;
	}

	if (!SDL_SetWindowPosition(s_backend->window(), NewWindowX, NewWindowY)) {
		fprintf(stderr, "SDL_SetWindowPosition fails: %s\n",
			SDL_GetError());
		return false;
	}

	if (s_useFullScreen)
	{
		int wr;
		int hr;

		SDL_GetWindowSizeInPixels(s_backend->window(), &wr, &hr);

		g_viewHSize = wr;
		g_viewVSize = hr;
		if (s_useMagnify) {
			g_viewHSize /= s_windowScale;
			g_viewVSize /= s_windowScale;
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
			hOffset = (wr - NewWindowWidth) / 2;
		} else {
			hOffset = 0;
		}
		if (hr > NewWindowHeight) {
			vOffset = (hr - NewWindowHeight) / 2;
		} else {
			vOffset = 0;
		}
	}

	v = true;
	return v;
}

static void CloseMainWindow()
{
	s_backend->destroyWindow();
}

static void ZapMyWState()
{
	s_backend->zapWindowState();
}

struct MyWState {
	uint16_t f_ViewHSize;
	uint16_t f_ViewVSize;
	uint16_t f_ViewHStart;
	uint16_t f_ViewVStart;
	int f_hOffset;
	int f_vOffset;
	bool f_UseFullScreen;
	bool f_UseMagnify;
	int f_CurWinIndx;
	SdlWindowState f_sdlState;
};

static void GetMyWState(MyWState *r)
{
	r->f_ViewHSize = g_viewHSize;
	r->f_ViewVSize = g_viewVSize;
	r->f_ViewHStart = g_viewHStart;
	r->f_ViewVStart = g_viewVStart;
	r->f_hOffset = hOffset;
	r->f_vOffset = vOffset;
	r->f_UseFullScreen = s_useFullScreen;
	r->f_UseMagnify = s_useMagnify;
	r->f_CurWinIndx = s_curWinIndx;
	s_backend->getWindowState(&r->f_sdlState);
}

static void SetMyWState(MyWState *r)
{
	g_viewHSize = r->f_ViewHSize;
	g_viewVSize = r->f_ViewVSize;
	g_viewHStart = r->f_ViewHStart;
	g_viewVStart = r->f_ViewVStart;
	hOffset = r->f_hOffset;
	vOffset = r->f_vOffset;
	s_useFullScreen = r->f_UseFullScreen;
	s_useMagnify = r->f_UseMagnify;
	s_curWinIndx = r->f_CurWinIndx;
	s_backend->setWindowState(&r->f_sdlState);
}

enum {
	kWinStateWindowed,
	kWinStateFullScreen,
	kNumWinStates
};

static int WinMagStates[kNumWinStates];

static bool ReCreateMainWindow()
{
	/*
		OSGLUxxx common:
		Like CreateMainWindow (which it calls), except may be
		called when already have window, without CloseMainWindow
		being called first. (Usually with different
		values of g_wantMagnify and g_wantFullScreen than
		on the previous call.)

		If there is existing window, and fail to create
		the new one, then existing window must be left alone,
		in valid state. (and return false. otherwise,
		if succeed, return true)

		i.e. can allocate the new one before disposing
		of the old one.
	*/

	MyWState old_state;
	MyWState new_state;
	bool HadCursorHidden = s_haveCursorHidden;
	int OldWinState =
		s_useFullScreen ? kWinStateFullScreen : kWinStateWindowed;
	int OldMagState =
		s_useMagnify ? kMagStateMagnifgy : kMagStateNormal;

	WinMagStates[OldWinState] =
		OldMagState;

	if (! s_useFullScreen)
	{
		s_backend->getWindowPosition(
			&WinPositionsX[s_curWinIndx],
			&WinPositionsY[s_curWinIndx]);
		HavePositionWins[s_curWinIndx] = true;
	}

	ForceShowCursor(); /* hide/show cursor api is per window */

	if (s_grabMachine) {
		s_grabMachine = false;
		UngrabMachine();
	}

	GetMyWState(&old_state);

	ZapMyWState();

	s_useMagnify = g_wantMagnify;
	s_useFullScreen = g_wantFullScreen;

	if (! CreateMainWindow()) {
		CloseMainWindow();
		SetMyWState(&old_state);

		/* avoid retry */
		g_wantFullScreen = s_useFullScreen;
		g_wantMagnify = s_useMagnify;

	} else {
		GetMyWState(&new_state);
		SetMyWState(&old_state);
		CloseMainWindow();
		SetMyWState(&new_state);

		if (HadCursorHidden) {
			(void) MoveMouse(g_curMouseH, g_curMouseV);
		}
	}

	return true;
}


static void ZapWinStateVars()
{
	{
		int i;

		for (i = 0; i < kNumMagStates; ++i) {
			HavePositionWins[i] = false;
		}
	}
	{
		int i;

		for (i = 0; i < kNumWinStates; ++i) {
			WinMagStates[i] = kMagStateAuto;
		}
	}
}

void ToggleWantFullScreen()
{
	g_wantFullScreen = ! g_wantFullScreen;

	{
		int OldWinState =
			s_useFullScreen ? kWinStateFullScreen : kWinStateWindowed;
		int OldMagState =
			s_useMagnify ? kMagStateMagnifgy : kMagStateNormal;
		int NewWinState =
			g_wantFullScreen ? kWinStateFullScreen : kWinStateWindowed;
		int NewMagState = WinMagStates[NewWinState];

		WinMagStates[OldWinState] = OldMagState;
		if (kMagStateAuto != NewMagState) {
			g_wantMagnify = (kMagStateMagnifgy == NewMagState);
		} else {
			g_wantMagnify = false;
			if (g_wantFullScreen) {
				PlatformDisplayBounds r;

				if (s_backend->getDisplayBounds(&r)) {
					if ((r.w >= vMacScreenWidth * s_windowScale)
						&& (r.h >= vMacScreenHeight * s_windowScale)
						)
					{
						g_wantMagnify = true;
					}
				}
			}
		}
	}
}

/* --- SavedTasks --- */

static void LeaveBackground()
{
	ReconnectKeyCodes3();
	s_backend->disableKeyRepeat();
}

static void EnterBackground()
{
	s_backend->restoreKeyRepeat();
	DisconnectKeyCodes3();

	ForceShowCursor();
}

static void LeaveSpeedStopped()
{
	s_backend->audioStart();

	StartUpTimeAdjust();
}

static void EnterSpeedStopped()
{
	s_backend->audioStop();
}

static void CheckForSavedTasks()
{
	if (EvtQNeedRecover) {
		EvtQNeedRecover = false;

		/* attempt cleanup, EvtQNeedRecover may get set again */
		EvtQTryRecoverFromFull();
	}

	if (g_haveMouseMotion) {
		MouseConstrain();
	}

	if (g_requestMacOff) {
		g_requestMacOff = false;
		if (AnyDiskInserted()) {
			MacMsgOverride(Localize(kStrQuitWarningTitle),
				Localize(kStrQuitWarningMessage));
		} else {
			g_forceMacOff = true;
		}
	}

	if (g_forceMacOff) {
		return;
	}

	if (s_trueBackgroundFlag != s_backgroundFlag) {
		s_backgroundFlag = s_trueBackgroundFlag;
		if (s_trueBackgroundFlag) {
			EnterBackground();
		} else {
			LeaveBackground();
		}
	}

	if (s_curSpeedStopped != (g_speedStopped ||
		(s_backgroundFlag && ! g_runInBackground
		)))
	{
		s_curSpeedStopped = ! s_curSpeedStopped;
		if (s_curSpeedStopped) {
			EnterSpeedStopped();
		} else {
			LeaveSpeedStopped();
		}
	}

	if ((nullptr != SavedBriefMsg) & ! MacMsgDisplayed) {
		MacMsgDisplayOn();
	}

	if (0
		|| (s_useMagnify != g_wantMagnify)
		|| (s_useFullScreen != g_wantFullScreen)
		)
	{
		(void) ReCreateMainWindow();
	}

	if (s_grabMachine != (
		s_useFullScreen &&
		! (s_trueBackgroundFlag || s_curSpeedStopped)))
	{
		s_grabMachine = ! s_grabMachine;
		if (s_grabMachine) {
			GrabTheMachine();
		} else {
			UngrabMachine();
		}
	}

	if (g_needWholeScreenDraw) {
		g_needWholeScreenDraw = false;
		ScreenChangedAll();
	}

	if (0 != g_requestIthDisk) {
		Sony_InsertIth(g_requestIthDisk);
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
		} else
		{
			char defaultName[] = "untitled.dsk";
			MakeNewDisk(g_sonyNewDiskSize, defaultName);
		}
		g_sonyNewDiskWanted = false;
	}

	if (s_haveCursorHidden != (s_wantCursorHidden
		&& ! (s_trueBackgroundFlag || s_curSpeedStopped)))
	{
		s_haveCursorHidden = ! s_haveCursorHidden;
		s_haveCursorHidden ? s_backend->hideCursor() : s_backend->showCursor();
	}
}

/* --- command line parsing --- */

static bool ScanCommandLine()
{
	char *pa;
	int i = 1;

#if DBGLOG_OSG_INIT
	dbglog_writeln("enter ScanCommandLine"); /*^*/
#endif

	while (i < my_argc) {
		pa = my_argv[i++];
		if ('-' == pa[0]) {
			if (0 == strcmp(pa, "--rom"))
			{
				if (i < my_argc) {
					rom_path = my_argv[i++];
				}
			} else
			if (0 == strncmp(pa, "--rom=", 6))
			{
				/* --rom=path form (also handled by ProgramEarlyInit) */
				rom_path = pa + 6;
			} else
			{
				/* ignore unrecognized options (e.g. --model, --help
				   already consumed by ProgramEarlyInit, or OS X -psn_*) */
				dbglog_writeln("ignoring command line argument");
				dbglog_writeln(pa);
				/* long option (--foo) without '=': next argv is its value;
				   skip it so it is not mistaken for a disk image path */
				if ('-' == pa[1] && nullptr == strchr(pa, '=')) {
					if (i < my_argc && '-' != my_argv[i][0]) {
						++i; /* skip the value */
					}
				}
			}
		}
		/* else: positional disk paths are handled via lc.diskPaths in main() */
	}

	return true;
}

/* --- main program flow --- */

static void WaitForTheNextEvent()
{
	SDL_Event event;

	if (SDL_WaitEvent(&event)) {
		HandleTheEvent(&event);
	}
}

static void CheckForSystemEvents()
{
	/*
		OSGLUxxx common:
		Handle any events that are waiting for us.
		Return immediately when no more events
		are waiting, don't wait for more.
	*/

	SDL_Event event;
	int i = 10;

	while ((--i >= 0) && SDL_PollEvent(&event)) {
		HandleTheEvent(&event);
	}
}


 bool ExtraTimeNotOver()
{
	UpdateTrueEmulatedTime();
	return g_trueEmulatedTime == g_onTrueTime;
}

void WaitForNextTick()
{
	/* Deterministic path: no wall-clock gating, advance exactly one
	   tick per call.  Used for --record / --verify golden files. */
	if (g_SkipThrottle) {
		CheckForSystemEvents();
		DoneWithDrawingForTick();
		++g_onTrueTime;
		return;
	}

	for (;;) {
		CheckForSystemEvents();
		CheckForSavedTasks();

		if (g_forceMacOff) {
			return;
		}

		if (s_curSpeedStopped) {
			DoneWithDrawingForTick();
			WaitForTheNextEvent();
			continue;
		}


		if (ExtraTimeNotOver()) {
			(void) SDL_Delay(GetTimerDelay());
			continue;
		}

		break;
	}

	if (CheckDateTime()) {
		Sound_SecondNotify();
	}

	if ((! s_backgroundFlag)
		&& (! s_caughtMouse)
		)
	{
		CheckMouseState();
	}

	g_onTrueTime = g_trueEmulatedTime;
}

/* --- platform independent code can be thought of as going here --- */

#include "core/main.h"

static void ZapOSGLUVars()
{
	/*
		OSGLUxxx common:
		Set initial values of variables for
		platform dependent code, where not
		done using c initializers. (such
		as for arrays.)
	*/

	InitDrives();
	ZapWinStateVars();
}

static bool AllocMyMemory()
{
	if (!dbglog_ReserveAlloc())
		goto fail;
	if (!AllocBlock(&g_rom, g_machine->config().romSize, false))
		goto fail;
	if (!AllocBlock(&g_screenCompareBuff, vMacScreenNumBytes, true))
		goto fail;
	if (!AllocBlock(&g_cntrlDisplayBuff, vMacScreenNumBytes, false))
		goto fail;
	if (!AllocBlock(&CLUT_final, CLUT_FINAL_SZ, false))
		goto fail;
	if (!Sound_AllocBuffer())
		goto fail;
	if (!EmulationReserveAlloc())
		goto fail;

	return true;
fail:
	MacMsg(Localize(kStrOutOfMemTitle), Localize(kStrOutOfMemMessage), true);
	return false;
}

static void UnallocMyMemory()
{
	free(g_rom); g_rom = nullptr;
	free(g_screenCompareBuff); g_screenCompareBuff = nullptr;
	free(g_cntrlDisplayBuff); g_cntrlDisplayBuff = nullptr;
	free(CLUT_final); CLUT_final = nullptr;
	Sound_FreeBuffer();
	EmulationFreeAlloc();
}

static bool InitWhereAmI()
{
	app_parent = const_cast<char *>(SDL_GetBasePath());

	pref_dir = SDL_GetPrefPath("gryphel", "maxivmac");

	return true; /* keep going regardless */
}

static void UninitWhereAmI()
{
	SDL_free(pref_dir);

}

/* Perform all platform initialisation in order: allocate memory,
   parse arguments, load g_rom and disks, create window and sound. */
static bool InitOSGLU()
{
#define INIT_STEP(name, expr) \
	if (!(expr)) { fprintf(stderr, "[SDL init] " name " FAILED\n"); return false; }

	INIT_STEP("AllocMyMemory", AllocMyMemory())
	INIT_STEP("InitWhereAmI", InitWhereAmI())
	/* dbglog_open is best-effort: fail to open log file is non-fatal */
	dbglog_open();
	INIT_STEP("ScanCommandLine", ScanCommandLine())
	INIT_STEP("LoadMacRom", LoadMacRom(d_arg, app_parent, pref_dir))
	INIT_STEP("LoadInitialImages", LoadInitialImages())
	INIT_STEP("InitLocationDat", InitLocationDat())
	INIT_STEP("Screen_Init", Screen_Init())
	INIT_STEP("Sound_Init", s_backend->audioInit())
	INIT_STEP("CreateMainWindow", CreateMainWindow())
	INIT_STEP("WaitForRom", WaitForRom())

#undef INIT_STEP
	return true;
}

static void UnInitOSGLU()
{
	if (MacMsgDisplayed) {
		MacMsgDisplayOff();
	}

	s_backend->restoreKeyRepeat();
	UngrabMachine();
	s_backend->audioStop();
	s_backend->audioShutdown();
	UnInitPbufs();
	UnInitDrives();

	ForceShowCursor();

	dbglog_close();

	UninitWhereAmI();
	UnallocMyMemory();

	CheckSavedMacMsg();

	CloseMainWindow();

	s_backend->shutdown();
}

int main(int argc, char **argv)
{
	my_argc = argc;
	my_argv = argv;

	ProgramEarlyInit(argc, argv);

	s_windowScale = GetEmulatorConfig().windowScale;
	g_speedValue = GetEmulatorConfig().speed;

	const LaunchConfig& lc = GetLaunchConfig();
	if (lc.help) {
		return 0;
	}

	/* Seed SDL-specific options from common launch config */
	static std::string s_title;
	static std::string s_romDir;
	static std::string s_resolvedRom;
	if (!lc.title.empty()) {
		s_title = lc.title;
		n_arg = const_cast<char*>(s_title.c_str());
	}
	if (!lc.romDir.empty()) {
		s_romDir = lc.romDir;
		d_arg = const_cast<char*>(s_romDir.c_str());
	}
	s_resolvedRom = ResolveRomPath(lc.romPath, lc.model, lc.romDir);
	if (!s_resolvedRom.empty()) {
		rom_path = const_cast<char*>(s_resolvedRom.c_str());
	}

	/* Insert disk images from command line */
	ZapOSGLUVars();

	s_backend = new SdlBackend();
	if (InitOSGLU()) {
		for (const auto& diskPath : lc.diskPaths) {
			(void) Sony_Insert1(const_cast<char*>(diskPath.c_str()), false);
		}
		ProgramMain();
	}
	UnInitOSGLU();
	delete s_backend;
	s_backend = nullptr;
	ProgramCleanup();

	return 0;
}

