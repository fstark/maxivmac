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

void MyMoveBytes(uint8_t * srcPtr, uint8_t * destPtr, int32_t byteCount)
{
	(void) memcpy(reinterpret_cast<char *>(destPtr), reinterpret_cast<char *>(srcPtr), byteCount);
}

/* --- control mode and internationalization --- */

#define dbglog_OSGInit (0 && dbglog_HAVE)

#include "platform/common/intl_chars.h"


static char *d_arg = nullptr;
static char *n_arg = nullptr;

char *app_parent = nullptr;
static char *pref_dir = nullptr;

static SDL_AudioStream *stream = nullptr;

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

	if (! ROM_loaded) {
		v = (mnvm_noErr == LoadMacRomFrom(drivepath));
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

		if (mnvm_noErr == ChildPath(d, s, &t)) {
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

static bool UseFullScreen = false;

static bool UseMagnify = true;


static bool gBackgroundFlag = false;
static bool gTrueBackgroundFlag = false;
static bool CurSpeedStopped = true;

static int WindowScale = 2;


static SDL_Window *my_main_wind = nullptr;
static SDL_Renderer *my_renderer = nullptr;
static SDL_Texture *my_texture = nullptr;
static
const SDL_PixelFormatDetails
*my_format = nullptr;

static uint8_t * ScalingBuff = nullptr;

static uint8_t * CLUT_final;

#define CLUT_finalsz (256 * 8 * 4)
	/*
		256 possible values of one byte
		8 pixels per byte maximum (when black and white)
		4 bytes per destination pixel maximum
			multiplied by WindowScale when magnified
	*/

#define ScrnMapr_DoMap UpdateBWDepth3Copy
#define ScrnMapr_SrcDepth 0
#define ScrnMapr_DstDepth 3
#include "platform/screen_map_inst.h"

#define ScrnMapr_DoMap UpdateBWDepth4Copy
#define ScrnMapr_SrcDepth 0
#define ScrnMapr_DstDepth 4
#include "platform/screen_map_inst.h"

#define ScrnMapr_DoMap UpdateBWDepth5Copy
#define ScrnMapr_SrcDepth 0
#define ScrnMapr_DstDepth 5
#include "platform/screen_map_inst.h"


/* Color copy functions for runtime depths 1, 2, 3 (CLUT-indexed).
   Each src depth needs its own instantiation since ScrnMapr_SrcDepth
   must be a compile-time constant.
   Note: screen_map.h #undefines Src/Dst/Map after each inclusion,
   so they must be redefined for every instantiation. */

#define ScrnMapr_DoMap UpdateColorSrc1Dst3Copy
#define ScrnMapr_SrcDepth 1
#define ScrnMapr_DstDepth 3
#include "platform/screen_map_inst.h"

#define ScrnMapr_DoMap UpdateColorSrc1Dst4Copy
#define ScrnMapr_SrcDepth 1
#define ScrnMapr_DstDepth 4
#include "platform/screen_map_inst.h"

#define ScrnMapr_DoMap UpdateColorSrc1Dst5Copy
#define ScrnMapr_SrcDepth 1
#define ScrnMapr_DstDepth 5
#include "platform/screen_map_inst.h"

#define ScrnMapr_DoMap UpdateColorSrc2Dst3Copy
#define ScrnMapr_SrcDepth 2
#define ScrnMapr_DstDepth 3
#include "platform/screen_map_inst.h"

#define ScrnMapr_DoMap UpdateColorSrc2Dst4Copy
#define ScrnMapr_SrcDepth 2
#define ScrnMapr_DstDepth 4
#include "platform/screen_map_inst.h"

#define ScrnMapr_DoMap UpdateColorSrc2Dst5Copy
#define ScrnMapr_SrcDepth 2
#define ScrnMapr_DstDepth 5
#include "platform/screen_map_inst.h"

#define ScrnMapr_DoMap UpdateColorSrc3Dst3Copy
#define ScrnMapr_SrcDepth 3
#define ScrnMapr_DstDepth 3
#include "platform/screen_map_inst.h"

#define ScrnMapr_DoMap UpdateColorSrc3Dst4Copy
#define ScrnMapr_SrcDepth 3
#define ScrnMapr_DstDepth 4
#include "platform/screen_map_inst.h"

#define ScrnMapr_DoMap UpdateColorSrc3Dst5Copy
#define ScrnMapr_SrcDepth 3
#define ScrnMapr_DstDepth 5
#include "platform/screen_map_inst.h"


/* Convert a dirty rectangle of the emulated framebuffer into
   host pixels and present it via the SDL texture/surface. */
static void HaveChangedScreenBuff(uint16_t top, uint16_t left,
	uint16_t bottom, uint16_t right)
{
	int i;
	int j;
	uint8_t *p;
	Uint32 pixel;
	Uint32 CLUT_pixel[CLUT_size];
	Uint32 BWLUT_pixel[2];
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

	if (UseFullScreen)
	{
		if (top < ViewVStart) {
			top = ViewVStart;
		}
		if (left < ViewHStart) {
			left = ViewHStart;
		}
		if (bottom > ViewVStart + ViewVSize) {
			bottom = ViewVStart + ViewVSize;
		}
		if (right > ViewHStart + ViewHSize) {
			right = ViewHStart + ViewHSize;
		}

		if ((top >= bottom) || (left >= right)) {
			return;
		}
	}

	XDest = left;
	YDest = top;
	DestWidth = (right - left);
	DestHeight = (bottom - top);

	if (UseFullScreen)
	{
		XDest -= ViewHStart;
		YDest -= ViewVStart;
	}

	if (UseMagnify) {
		XDest *= WindowScale;
		YDest *= WindowScale;
		DestWidth *= WindowScale;
		DestHeight *= WindowScale;
	}

	if (UseFullScreen)
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
		SDL_LockTexture(my_texture, nullptr, &pixels, &pitch)
	) {
		return;
	}

	{

	int bpp = my_format->bytes_per_pixel;
	uint32_t ExpectedPitch = vMacScreenWidth * bpp;


	if (UseColorMode && vMacScreenDepth > 0 && vMacScreenDepth < 4) {
		for (i = 0; i < CLUT_size; ++i) {
			CLUT_pixel[i] = SDL_MapRGB(my_format,
				nullptr,
				CLUT_reds[i] >> 8,
				CLUT_greens[i] >> 8,
				CLUT_blues[i] >> 8);
		}
	} else {
		BWLUT_pixel[1] = SDL_MapRGB(
			my_format,
			nullptr,
			0, 0, 0
		);
			/* black */
		BWLUT_pixel[0] = SDL_MapRGB(
			my_format,
			nullptr,
			255, 255, 255
		);
			/* white */
	}

	if ((0 == ((bpp - 1) & bpp)) /* a power of 2 */
		&& ((uint32_t)pitch == ExpectedPitch)
		&& (vMacScreenDepth <= 3 || ! UseColorMode)
		)
	{
		int k;
		Uint32 v;
		int PixPerByte =
			(UseColorMode && vMacScreenDepth > 0 && vMacScreenDepth < 4)
			? (1 << (3 - vMacScreenDepth)) : 8;
		Uint8 *p4 = CLUT_final;

		for (i = 0; i < 256; ++i) {
			for (k = PixPerByte; --k >= 0; ) {

				if (UseColorMode && vMacScreenDepth > 0 && vMacScreenDepth < 4) {
					v = CLUT_pixel[
						(vMacScreenDepth == 3) ? i :
						((i >> (k << vMacScreenDepth)) & (CLUT_size - 1))
					];
				} else {
					v = BWLUT_pixel[(i >> k) & 1];
				}

				{
					switch (bpp) {
						case 1: /* Assuming 8-bpp */
							*p4++ = v;
							break;
						case 2: /* Probably 15-bpp or 16-bpp */
							*(Uint16 *)p4 = v;
							p4 += 2;
							break;
						case 4: /* Probably 32-bpp */
							*(Uint32 *)p4 = v;
							p4 += 4;
							break;
					}
				}
			}
		}

		ScalingBuff = static_cast<uint8_t *>(pixels);

		if (UseColorMode && vMacScreenDepth > 0 && vMacScreenDepth < 4) {
			{
				switch (vMacScreenDepth) {
					case 1: switch (bpp) { case 1: UpdateColorSrc1Dst3Copy(top,left,bottom,right); break; case 2: UpdateColorSrc1Dst4Copy(top,left,bottom,right); break; case 4: UpdateColorSrc1Dst5Copy(top,left,bottom,right); break; } break;
					case 2: switch (bpp) { case 1: UpdateColorSrc2Dst3Copy(top,left,bottom,right); break; case 2: UpdateColorSrc2Dst4Copy(top,left,bottom,right); break; case 4: UpdateColorSrc2Dst5Copy(top,left,bottom,right); break; } break;
					case 3: switch (bpp) { case 1: UpdateColorSrc3Dst3Copy(top,left,bottom,right); break; case 2: UpdateColorSrc3Dst4Copy(top,left,bottom,right); break; case 4: UpdateColorSrc3Dst5Copy(top,left,bottom,right); break; } break;
				}
			}
		} else {
			{
				switch (bpp) {
					case 1:
						UpdateBWDepth3Copy(top, left, bottom, right);
						break;
					case 2:
						UpdateBWDepth4Copy(top, left, bottom, right);
						break;
					case 4:
						UpdateBWDepth5Copy(top, left, bottom, right);
						break;
				}
			}
		}

	} else {
		uint8_t *the_data = GetCurDrawBuff();

		/* adapted from putpixel in SDL documentation */

		for (i = top2; i < (int)bottom2; ++i) {
			for (j = left2; j < (int)right2; ++j) {
				int i0 = i;
				int j0 = j;
				Uint8 *bufp = static_cast<Uint8 *>(pixels)
					+ i * pitch + j * bpp;


				if (UseColorMode && vMacScreenDepth > 0) {
					if (vMacScreenDepth < 4) {
						p = the_data + ((i0 * vMacScreenWidth + j0)
							>> (3 - vMacScreenDepth));
						{
							uint8_t k = (*p >> (((~ j0)
									& ((1 << (3 - vMacScreenDepth)) - 1))
								<< vMacScreenDepth))
								& (CLUT_size - 1);
							pixel = CLUT_pixel[k];
						}
					} else if (vMacScreenDepth == 4) {
						p = the_data + ((i0 * vMacScreenWidth + j0) << 1);
						{
							uint16_t t0 = do_get_mem_word(p);
							pixel = SDL_MapRGB(my_format,
								nullptr,
								((t0 & 0x7C00) >> 7)
									| ((t0 & 0x7000) >> 12),
								((t0 & 0x03E0) >> 2)
									| ((t0 & 0x0380) >> 7),
								((t0 & 0x001F) << 3)
									| ((t0 & 0x001C) >> 2));
						}
					} else { /* depth == 5 */
						p = the_data + ((i0 * vMacScreenWidth + j0) << 2);
						pixel = SDL_MapRGB(my_format,
							nullptr,
							p[1],
							p[2],
							p[3]);
					}
				} else {
					p = the_data + ((i0 * vMacScreenWidth + j0) / 8);
					pixel = BWLUT_pixel[(*p >> ((~ j0) & 0x7)) & 1];
				}

				switch (bpp) {
					case 1: /* Assuming 8-bpp */
						*bufp = pixel;
						break;
					case 2: /* Probably 15-bpp or 16-bpp */
						*(Uint16 *)bufp = pixel;
						break;
					case 3:
						/* Slow 24-bpp mode, usually not used */
						if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
							bufp[0] = (pixel >> 16) & 0xff;
							bufp[1] = (pixel >> 8) & 0xff;
							bufp[2] = pixel & 0xff;
						} else {
							bufp[0] = pixel & 0xff;
							bufp[1] = (pixel >> 8) & 0xff;
							bufp[2] = (pixel >> 16) & 0xff;
						}
						break;
					case 4: /* Probably 32-bpp */
						*(Uint32 *)bufp = pixel;
						break;
				}
			}
		}
	}

	}

	SDL_UnlockTexture(my_texture);

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
	(my_renderer, my_texture, &src_rect, &dst_rect);
	
	SDL_RenderPresent(my_renderer);

}

static void MyDrawChangesAndClear()
{
	if (ScreenChangedBottom > ScreenChangedTop) {
		HaveChangedScreenBuff(ScreenChangedTop, ScreenChangedLeft,
			ScreenChangedBottom, ScreenChangedRight);
		ScreenClearChanges();
	}
}

void DoneWithDrawingForTick()
{
#if EnableFSMouseMotion
	if (HaveMouseMotion) {
		AutoScrollScreen();
	}
#endif
	MyDrawChangesAndClear();
}

/* --- mouse --- */

/* cursor hiding */

static bool HaveCursorHidden = false;
static bool WantCursorHidden = false;

static void ForceShowCursor()
{
	if (HaveCursorHidden) {
		HaveCursorHidden = false;
		SDL_ShowCursor();
	}
}

/* cursor moving */


#ifndef HaveWorkingWarp
#define HaveWorkingWarp 1
#endif

#if EnableMoveMouse && HaveWorkingWarp
static bool MyMoveMouse(int16_t h, int16_t v)
{
	/*
		OSGLUxxx common:
		Move the cursor to the point h, v on the emulated screen.
		If detect that this fails return false,
			otherwise return true.
		(On some platforms it is possible to move the curser,
			but there is no way to detect failure.)
	*/

	if (UseFullScreen)
	{
		h -= ViewHStart;
		v -= ViewVStart;
	}

	if (UseMagnify) {
		h *= WindowScale;
		v *= WindowScale;
	}

	if (UseFullScreen)
	{
		h += hOffset;
		v += vOffset;
	}

	SDL_WarpMouseInWindow(my_main_wind, h, v);

	return true;
}
#endif

/* cursor state */

static void MousePositionNotify(int NewMousePosh, int NewMousePosv)
{
	bool ShouldHaveCursorHidden = true;

	if (UseFullScreen)
	{
		NewMousePosh -= hOffset;
		NewMousePosv -= vOffset;
	}

	if (UseMagnify) {
		NewMousePosh /= WindowScale;
		NewMousePosv /= WindowScale;
	}

	if (UseFullScreen)
	{
		NewMousePosh += ViewHStart;
		NewMousePosv += ViewVStart;
	}

#if EnableFSMouseMotion
	if (HaveMouseMotion) {
		MyMousePositionSetDelta(NewMousePosh - SavedMouseH,
			NewMousePosv - SavedMouseV);
		SavedMouseH = NewMousePosh;
		SavedMouseV = NewMousePosv;
	} else
#endif
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

		if (UseFullScreen)
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

	WantCursorHidden = ShouldHaveCursorHidden;
}

#if EnableFSMouseMotion && ! HaveWorkingWarp
static void MousePositionNotifyRelative(int deltah, int deltav)
{
	bool ShouldHaveCursorHidden = true;

	if (UseMagnify) {
		/*
			This is not really right. If only move one pixel
			each time, emulated mouse doesn't move at all.
		*/
		deltah /= WindowScale;
		deltav /= WindowScale;
	}

	MyMousePositionSetDelta(deltah,
		deltav);

	WantCursorHidden = ShouldHaveCursorHidden;
}
#endif

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

static uint8_t SDLScan2MacKeyCode(SDL_Scancode i)
{
	uint8_t v = MKC_None;

	switch (i) {
		case SDL_SCANCODE_BACKSPACE: v = MKC_BackSpace; break;
		case SDL_SCANCODE_TAB: v = MKC_Tab; break;
		case SDL_SCANCODE_CLEAR: v = MKC_Clear; break;
		case SDL_SCANCODE_RETURN: v = MKC_Return; break;
		case SDL_SCANCODE_PAUSE: v = MKC_Pause; break;
		case SDL_SCANCODE_ESCAPE: v = MKC_formac_Escape; break;
		case SDL_SCANCODE_SPACE: v = MKC_Space; break;
		case SDL_SCANCODE_APOSTROPHE: v = MKC_SingleQuote; break;
		case SDL_SCANCODE_COMMA: v = MKC_Comma; break;
		case SDL_SCANCODE_MINUS: v = MKC_Minus; break;
		case SDL_SCANCODE_PERIOD: v = MKC_Period; break;
		case SDL_SCANCODE_SLASH: v = MKC_formac_Slash; break;
		case SDL_SCANCODE_0: v = MKC_0; break;
		case SDL_SCANCODE_1: v = MKC_1; break;
		case SDL_SCANCODE_2: v = MKC_2; break;
		case SDL_SCANCODE_3: v = MKC_3; break;
		case SDL_SCANCODE_4: v = MKC_4; break;
		case SDL_SCANCODE_5: v = MKC_5; break;
		case SDL_SCANCODE_6: v = MKC_6; break;
		case SDL_SCANCODE_7: v = MKC_7; break;
		case SDL_SCANCODE_8: v = MKC_8; break;
		case SDL_SCANCODE_9: v = MKC_9; break;
		case SDL_SCANCODE_SEMICOLON: v = MKC_SemiColon; break;
		case SDL_SCANCODE_EQUALS: v = MKC_Equal; break;

		case SDL_SCANCODE_LEFTBRACKET: v = MKC_LeftBracket; break;
		case SDL_SCANCODE_BACKSLASH: v = MKC_formac_BackSlash; break;
		case SDL_SCANCODE_RIGHTBRACKET: v = MKC_RightBracket; break;
		case SDL_SCANCODE_GRAVE: v = MKC_formac_Grave; break;

		case SDL_SCANCODE_A: v = MKC_A; break;
		case SDL_SCANCODE_B: v = MKC_B; break;
		case SDL_SCANCODE_C: v = MKC_C; break;
		case SDL_SCANCODE_D: v = MKC_D; break;
		case SDL_SCANCODE_E: v = MKC_E; break;
		case SDL_SCANCODE_F: v = MKC_F; break;
		case SDL_SCANCODE_G: v = MKC_G; break;
		case SDL_SCANCODE_H: v = MKC_H; break;
		case SDL_SCANCODE_I: v = MKC_I; break;
		case SDL_SCANCODE_J: v = MKC_J; break;
		case SDL_SCANCODE_K: v = MKC_K; break;
		case SDL_SCANCODE_L: v = MKC_L; break;
		case SDL_SCANCODE_M: v = MKC_M; break;
		case SDL_SCANCODE_N: v = MKC_N; break;
		case SDL_SCANCODE_O: v = MKC_O; break;
		case SDL_SCANCODE_P: v = MKC_P; break;
		case SDL_SCANCODE_Q: v = MKC_Q; break;
		case SDL_SCANCODE_R: v = MKC_R; break;
		case SDL_SCANCODE_S: v = MKC_S; break;
		case SDL_SCANCODE_T: v = MKC_T; break;
		case SDL_SCANCODE_U: v = MKC_U; break;
		case SDL_SCANCODE_V: v = MKC_V; break;
		case SDL_SCANCODE_W: v = MKC_W; break;
		case SDL_SCANCODE_X: v = MKC_X; break;
		case SDL_SCANCODE_Y: v = MKC_Y; break;
		case SDL_SCANCODE_Z: v = MKC_Z; break;

		case SDL_SCANCODE_KP_0: v = MKC_KP0; break;
		case SDL_SCANCODE_KP_1: v = MKC_KP1; break;
		case SDL_SCANCODE_KP_2: v = MKC_KP2; break;
		case SDL_SCANCODE_KP_3: v = MKC_KP3; break;
		case SDL_SCANCODE_KP_4: v = MKC_KP4; break;
		case SDL_SCANCODE_KP_5: v = MKC_KP5; break;
		case SDL_SCANCODE_KP_6: v = MKC_KP6; break;
		case SDL_SCANCODE_KP_7: v = MKC_KP7; break;
		case SDL_SCANCODE_KP_8: v = MKC_KP8; break;
		case SDL_SCANCODE_KP_9: v = MKC_KP9; break;

		case SDL_SCANCODE_KP_PERIOD: v = MKC_Decimal; break;
		case SDL_SCANCODE_KP_DIVIDE: v = MKC_KPDevide; break;
		case SDL_SCANCODE_KP_MULTIPLY: v = MKC_KPMultiply; break;
		case SDL_SCANCODE_KP_MINUS: v = MKC_KPSubtract; break;
		case SDL_SCANCODE_KP_PLUS: v = MKC_KPAdd; break;
		case SDL_SCANCODE_KP_ENTER: v = MKC_formac_Enter; break;
		case SDL_SCANCODE_KP_EQUALS: v = MKC_KPEqual; break;

		case SDL_SCANCODE_UP: v = MKC_Up; break;
		case SDL_SCANCODE_DOWN: v = MKC_Down; break;
		case SDL_SCANCODE_RIGHT: v = MKC_Right; break;
		case SDL_SCANCODE_LEFT: v = MKC_Left; break;
		case SDL_SCANCODE_INSERT: v = MKC_formac_Help; break;
		case SDL_SCANCODE_HOME: v = MKC_formac_Home; break;
		case SDL_SCANCODE_END: v = MKC_formac_End; break;
		case SDL_SCANCODE_PAGEUP: v = MKC_formac_PageUp; break;
		case SDL_SCANCODE_PAGEDOWN: v = MKC_formac_PageDown; break;

		case SDL_SCANCODE_F1: v = MKC_formac_F1; break;
		case SDL_SCANCODE_F2: v = MKC_formac_F2; break;
		case SDL_SCANCODE_F3: v = MKC_formac_F3; break;
		case SDL_SCANCODE_F4: v = MKC_formac_F4; break;
		case SDL_SCANCODE_F5: v = MKC_formac_F5; break;
		case SDL_SCANCODE_F6: v = MKC_F6; break;
		case SDL_SCANCODE_F7: v = MKC_F7; break;
		case SDL_SCANCODE_F8: v = MKC_F8; break;
		case SDL_SCANCODE_F9: v = MKC_F9; break;
		case SDL_SCANCODE_F10: v = MKC_F10; break;
		case SDL_SCANCODE_F11: v = MKC_F11; break;
		case SDL_SCANCODE_F12: v = MKC_F12; break;

		case SDL_SCANCODE_NUMLOCKCLEAR:
			v = MKC_formac_ForwardDel; break;
		case SDL_SCANCODE_CAPSLOCK: v = MKC_formac_CapsLock; break;
		case SDL_SCANCODE_SCROLLLOCK: v = MKC_ScrollLock; break;
		case SDL_SCANCODE_RSHIFT: v = MKC_formac_RShift; break;
		case SDL_SCANCODE_LSHIFT: v = MKC_formac_Shift; break;
		case SDL_SCANCODE_RCTRL: v = MKC_formac_RControl; break;
		case SDL_SCANCODE_LCTRL: v = MKC_formac_Control; break;
		case SDL_SCANCODE_RALT: v = MKC_formac_ROption; break;
		case SDL_SCANCODE_LALT: v = MKC_formac_Option; break;
		case SDL_SCANCODE_RGUI: v = MKC_formac_RCommand; break;
		case SDL_SCANCODE_LGUI: v = MKC_formac_Command; break;
		/* case SDLK_LSUPER: v = MKC_formac_Option; break; */
		/* case SDLK_RSUPER: v = MKC_formac_ROption; break; */

		case SDL_SCANCODE_HELP: v = MKC_formac_Help; break;
		case SDL_SCANCODE_PRINTSCREEN: v = MKC_Print; break;

		case SDL_SCANCODE_UNDO: v = MKC_formac_F1; break;
		case SDL_SCANCODE_CUT: v = MKC_formac_F2; break;
		case SDL_SCANCODE_COPY: v = MKC_formac_F3; break;
		case SDL_SCANCODE_PASTE: v = MKC_formac_F4; break;

		case SDL_SCANCODE_AC_HOME: v = MKC_formac_Home; break;

		case SDL_SCANCODE_KP_A: v = MKC_A; break;
		case SDL_SCANCODE_KP_B: v = MKC_B; break;
		case SDL_SCANCODE_KP_C: v = MKC_C; break;
		case SDL_SCANCODE_KP_D: v = MKC_D; break;
		case SDL_SCANCODE_KP_E: v = MKC_E; break;
		case SDL_SCANCODE_KP_F: v = MKC_F; break;

		case SDL_SCANCODE_KP_BACKSPACE: v = MKC_BackSpace; break;
		case SDL_SCANCODE_KP_CLEAR: v = MKC_Clear; break;
		case SDL_SCANCODE_KP_COMMA: v = MKC_Comma; break;
		case SDL_SCANCODE_KP_DECIMAL: v = MKC_Decimal; break;

		default:
			break;
	}

	return v;
}

static void DoKeyCode(
	SDL_KeyboardEvent
	*r, bool down)
{
	uint8_t v = SDLScan2MacKeyCode(r->scancode);
	if (MKC_None != v) {
		Keyboard_UpdateKeyMap2(v, down);
	}
}

static void DisableKeyRepeat()
{
	/*
		OSGLUxxx common:
		If possible and useful, disable keyboard autorepeat.
	*/
}

static void RestoreKeyRepeat()
{
	/*
		OSGLUxxx common:
		Undo any effects of DisableKeyRepeat.
	*/
}

static void ReconnectKeyCodes3()
{
}

static void DisconnectKeyCodes3()
{
	DisconnectKeyCodes2();
	MyMouseButtonSet(false);
}

/* --- time, date, location --- */

#include "platform/common/tick_timer.h"

/* --- sound --- */


#define kLn2SoundBuffers 4 /* kSoundBuffers must be a power of two */
#define kSoundBuffers (1 << kLn2SoundBuffers)
#define kSoundBuffMask (kSoundBuffers - 1)

#define DesiredMinFilledSoundBuffs 3
	/*
		if too big then sound lags behind emulation.
		if too small then sound will have pauses.
	*/

#define kLnOneBuffLen 9
#define kLnAllBuffLen (kLn2SoundBuffers + kLnOneBuffLen)
#define kOneBuffLen (1UL << kLnOneBuffLen)
#define kAllBuffLen (1UL << kLnAllBuffLen)
#define kLnOneBuffSz (kLnOneBuffLen + 1)
#define kLnAllBuffSz (kLnAllBuffLen + 1)
#define kOneBuffSz (1UL << kLnOneBuffSz)
#define kAllBuffSz (1UL << kLnAllBuffSz)
#define kOneBuffMask (kOneBuffLen - 1)
#define kAllBuffMask (kAllBuffLen - 1)
#define dbhBufferSize (kAllBuffSz + kOneBuffSz)

#define dbglog_SoundStuff (0 && dbglog_HAVE)
#define dbglog_SoundBuffStats (0 && dbglog_HAVE)

static tpSoundSamp TheSoundBuffer = nullptr;
volatile static uint16_t ThePlayOffset;
volatile static uint16_t TheFillOffset;
volatile static uint16_t MinFilledSoundBuffs;
#if dbglog_SoundBuffStats
static uint16_t MaxFilledSoundBuffs;
#endif
static uint16_t TheWriteOffset;

static void MySound_Init0()
{
	ThePlayOffset = 0;
	TheFillOffset = 0;
	TheWriteOffset = 0;
}

static void MySound_Start0()
{
	/* Reset variables */
	MinFilledSoundBuffs = kSoundBuffers + 1;
#if dbglog_SoundBuffStats
	MaxFilledSoundBuffs = 0;
#endif
}

 tpSoundSamp MySound_BeginWrite(uint16_t n, uint16_t *actL)
{
	uint16_t ToFillLen = kAllBuffLen - (TheWriteOffset - ThePlayOffset);
	uint16_t WriteBuffContig =
		kOneBuffLen - (TheWriteOffset & kOneBuffMask);

	if (WriteBuffContig < n) {
		n = WriteBuffContig;
	}
	if (ToFillLen < n) {
		/* overwrite previous buffer */
#if dbglog_SoundStuff
		dbglog_writeln("sound buffer over flow");
#endif
		TheWriteOffset -= kOneBuffLen;
	}

	*actL = n;
	return TheSoundBuffer + (TheWriteOffset & kAllBuffMask);
}

static void ConvertSoundBlockToNative(tpSoundSamp p)
{
	int i;

	for (i = kOneBuffLen; --i >= 0; ) {
		*p++ -= 0x8000;
	}
}

static void MySound_WroteABlock()
{
	uint16_t PrevWriteOffset = TheWriteOffset - kOneBuffLen;
	tpSoundSamp p = TheSoundBuffer + (PrevWriteOffset & kAllBuffMask);

#if dbglog_SoundStuff
	dbglog_writeln("enter MySound_WroteABlock");
#endif

	ConvertSoundBlockToNative(p);

	TheFillOffset = TheWriteOffset;

#if dbglog_SoundBuffStats
	{
		uint16_t ToPlayLen = TheFillOffset
			- ThePlayOffset;
		uint16_t ToPlayBuffs = ToPlayLen >> kLnOneBuffLen;

		if (ToPlayBuffs > MaxFilledSoundBuffs) {
			MaxFilledSoundBuffs = ToPlayBuffs;
		}
	}
#endif
}

static bool MySound_EndWrite0(uint16_t actL)
{
	bool v;

	TheWriteOffset += actL;

	if (0 != (TheWriteOffset & kOneBuffMask)) {
		v = false;
	} else {
		/* just finished a block */

		MySound_WroteABlock();

		v = true;
	}

	return v;
}

static void MySound_SecondNotify0()
{
	if (MinFilledSoundBuffs <= kSoundBuffers) {
		if (MinFilledSoundBuffs > DesiredMinFilledSoundBuffs) {
#if dbglog_SoundStuff
			dbglog_writeln("MinFilledSoundBuffs too high");
#endif
			IncrNextTime();
		} else if (MinFilledSoundBuffs < DesiredMinFilledSoundBuffs) {
#if dbglog_SoundStuff
			dbglog_writeln("MinFilledSoundBuffs too low");
#endif
			++TrueEmulatedTime;
		}
#if dbglog_SoundBuffStats
		dbglog_writelnNum("MinFilledSoundBuffs",
			MinFilledSoundBuffs);
		dbglog_writelnNum("MaxFilledSoundBuffs",
			MaxFilledSoundBuffs);
		MaxFilledSoundBuffs = 0;
#endif
		MinFilledSoundBuffs = kSoundBuffers + 1;
	}
}

typedef uint16_t trSoundTemp;

#define kCenterTempSound 0x8000

#define AudioStepVal 0x0040

#define ConvertTempSoundSampleFromNative(v) ((v) + kCenterSound)

#define ConvertTempSoundSampleToNative(v) ((v) - kCenterSound)

static void SoundRampTo(trSoundTemp *last_val, trSoundTemp dst_val,
	tpSoundSamp *stream, int *len)
{
	trSoundTemp diff;
	tpSoundSamp p = *stream;
	int n = *len;
	trSoundTemp v1 = *last_val;

	while ((v1 != dst_val) && (0 != n)) {
		if (v1 > dst_val) {
			diff = v1 - dst_val;
			if (diff > AudioStepVal) {
				v1 -= AudioStepVal;
			} else {
				v1 = dst_val;
			}
		} else {
			diff = dst_val - v1;
			if (diff > AudioStepVal) {
				v1 += AudioStepVal;
			} else {
				v1 = dst_val;
			}
		}

		--n;
		*p++ = ConvertTempSoundSampleToNative(v1);
	}

	*stream = p;
	*len = n;
	*last_val = v1;
}

struct MySoundR {
	tpSoundSamp fTheSoundBuffer;
	volatile uint16_t (*fPlayOffset);
	volatile uint16_t (*fFillOffset);
	volatile uint16_t (*fMinFilledSoundBuffs);

	volatile trSoundTemp lastv;

	bool wantplaying;
	bool HaveStartedPlaying;
};

static void my_audio_callback(void *udata, Uint8 *stream, int len)
{
	uint16_t ToPlayLen;
	uint16_t FilledSoundBuffs;
	int i;
	MySoundR *datp = (MySoundR *)udata;
	tpSoundSamp CurSoundBuffer = datp->fTheSoundBuffer;
	uint16_t CurPlayOffset = *datp->fPlayOffset;
	trSoundTemp v0 = datp->lastv;
	trSoundTemp v1 = v0;
	tpSoundSamp dst = (tpSoundSamp)stream;

	len >>= 1; /* convert byte length to sample count (16-bit samples) */

#if dbglog_SoundStuff
	dbglog_writeln("Enter my_audio_callback");
	dbglog_writelnNum("len", len);
#endif

	while (len > 0) {
		ToPlayLen = *datp->fFillOffset - CurPlayOffset;
		FilledSoundBuffs = ToPlayLen >> kLnOneBuffLen;

		if (! datp->wantplaying) {
#if dbglog_SoundStuff
			dbglog_writeln("playing end transistion");
#endif

			SoundRampTo(&v1, kCenterTempSound, &dst, &len);

			ToPlayLen = 0;
		} else if (! datp->HaveStartedPlaying) {
#if dbglog_SoundStuff
			dbglog_writeln("playing start block");
#endif

			if ((ToPlayLen >> kLnOneBuffLen) < 8) {
				ToPlayLen = 0;
			} else {
				tpSoundSamp p = datp->fTheSoundBuffer
					+ (CurPlayOffset & kAllBuffMask);
				trSoundTemp v2 = ConvertTempSoundSampleFromNative(*p);

#if dbglog_SoundStuff
				dbglog_writeln("have enough samples to start");
#endif

				SoundRampTo(&v1, v2, &dst, &len);

				if (v1 == v2) {
#if dbglog_SoundStuff
					dbglog_writeln("finished start transition");
#endif

					datp->HaveStartedPlaying = true;
				}
			}
		}

		if (0 == len) {
			/* done */

			if (FilledSoundBuffs < *datp->fMinFilledSoundBuffs) {
				*datp->fMinFilledSoundBuffs = FilledSoundBuffs;
			}
		} else if (0 == ToPlayLen) {

#if dbglog_SoundStuff
			dbglog_writeln("under run");
#endif

			for (i = 0; i < len; ++i) {
				*dst++ = ConvertTempSoundSampleToNative(v1);
			}
			*datp->fMinFilledSoundBuffs = 0;
			break;
		} else {
			uint16_t PlayBuffContig = kAllBuffLen
				- (CurPlayOffset & kAllBuffMask);
			tpSoundSamp p = CurSoundBuffer
				+ (CurPlayOffset & kAllBuffMask);

			if (ToPlayLen > PlayBuffContig) {
				ToPlayLen = PlayBuffContig;
			}
			if (ToPlayLen > len) {
				ToPlayLen = len;
			}

			for (i = 0; i < ToPlayLen; ++i) {
				*dst++ = *p++;
			}
			v1 = ConvertTempSoundSampleFromNative(p[-1]);

			CurPlayOffset += ToPlayLen;
			len -= ToPlayLen;

			*datp->fPlayOffset = CurPlayOffset;
		}
	}

	datp->lastv = v1;
}
static void SDLCALL sdl3_audio_callback(void *udata, SDL_AudioStream *stream, int addAmount, int /*amount*/) {
	/* https://github.com/libsdl-org/sdl/blob/main/docs/README-migration.md#sdl_audioh */
	if (addAmount > 0) {
		Uint8 *data = SDL_stack_alloc(Uint8, addAmount);
		if (data) {
			my_audio_callback(udata, data, addAmount);
			SDL_PutAudioStreamData(stream, data, addAmount);
			SDL_stack_free(data);
		}
	}
}

static MySoundR cur_audio;

static bool HaveSoundOut = false;

static void MySound_Stop()
{
#if dbglog_SoundStuff
	dbglog_writeln("enter MySound_Stop");
#endif

	if (cur_audio.wantplaying && HaveSoundOut) {
		uint16_t retry_limit = 50; /* half of a second */

		cur_audio.wantplaying = false;

		while (kCenterTempSound != cur_audio.lastv
			&& --retry_limit != 0)
		{
			/*
				give time back, particularly important
				if got here on a suspend event.
			*/

#if dbglog_SoundStuff
			dbglog_writeln("busy, so sleep");
#endif

			(void) SDL_Delay(10);
		}

#if dbglog_SoundStuff
		if (kCenterTempSound == cur_audio.lastv) {
			dbglog_writeln("reached kCenterTempSound");
		} else {
			dbglog_writeln("retry limit reached");
		}
#endif

		SDL_PauseAudioDevice(
			SDL_GetAudioStreamDevice(stream)
		);
	}

#if dbglog_SoundStuff
	dbglog_writeln("leave MySound_Stop");
#endif
}

static void MySound_Start()
{
	if ((! cur_audio.wantplaying) && HaveSoundOut) {
		MySound_Start0();
		cur_audio.lastv = kCenterTempSound;
		cur_audio.HaveStartedPlaying = false;
		cur_audio.wantplaying = true;

		SDL_ResumeAudioDevice(
			SDL_GetAudioStreamDevice(stream)
		);
	}
}

static void MySound_UnInit()
{
	if (HaveSoundOut) {
		SDL_DestroyAudioStream(stream);
	}
}

#define SOUND_SAMPLERATE 22255 /* = round(7833600 * 2 / 704) */

static bool MySound_Init()
{
#if dbglog_OSGInit
	dbglog_writeln("enter MySound_Init");
#endif

	SDL_AudioSpec desired;

	MySound_Init0();

	cur_audio.fTheSoundBuffer = TheSoundBuffer;
	cur_audio.fPlayOffset = &ThePlayOffset;
	cur_audio.fFillOffset = &TheFillOffset;
	cur_audio.fMinFilledSoundBuffs = &MinFilledSoundBuffs;
	cur_audio.wantplaying = false;

	desired.freq = SOUND_SAMPLERATE;
	desired.format =
	SDL_AUDIO_S16;

	desired.channels = 1;
	stream = SDL_OpenAudioDeviceStream(
		SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
		&desired,
		(SDL_AudioStreamCallback)sdl3_audio_callback,
		(void *)&cur_audio
	);

	/* Open the audio device */
	if (
		stream == nullptr
	) {
		fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
	} else {
		HaveSoundOut = true;

		MySound_Start();
			/*
				This should be taken care of by LeaveSpeedStopped,
				but since takes a while to get going properly,
				start early.
			*/
	}

	return true; /* keep going, even if no sound */
}

void MySound_EndWrite(uint16_t actL)
{
	if (MySound_EndWrite0(actL)) {
	}
}

static void MySound_SecondNotify()
{
	/*
		OSGLUxxx common:
		called once a second.
		can be used to check if sound output it
		lagging or gaining, and if so
		adjust emulated time by a tick.
	*/

	if (HaveSoundOut) {
		MySound_SecondNotify0();
	}
}


/* --- basic dialogs --- */

static void CheckSavedMacMsg()
{
	/*
		OSGLUxxx common:
		This is currently only used in the
		rare case where there is a message
		still pending as the program quits.
	*/

	if (nullptr != SavedBriefMsg) {
		char briefMsg0[ClStrMaxLength + 1];
		char longMsg0[ClStrMaxLength + 1];

		NativeStrFromCStr(briefMsg0, SavedBriefMsg);
		NativeStrFromCStr(longMsg0, SavedLongMsg);

		if (0 != SDL_ShowSimpleMessageBox(
			SDL_MESSAGEBOX_ERROR,
			SavedBriefMsg,
			SavedLongMsg,
			my_main_wind
			))
		{
			fprintf(stderr, "%s\n", briefMsg0);
			fprintf(stderr, "%s\n", longMsg0);
		}

		SavedBriefMsg = nullptr;
	}
}


/* --- event handling for main window --- */

#define UseMotionEvents 1

#if UseMotionEvents
static bool CaughtMouse = false;
#endif

/* Dispatch an SDL event: quit, keyboard, mouse, window resize,
   drag-and-drop, etc. */
static void HandleTheEvent(SDL_Event *event)
{
	if (!UseFullScreen) {
		SDL_ConvertEventToRenderCoordinates(
			my_renderer,
			event
		);
	}

	switch (event->type) {
		case
			SDL_EVENT_QUIT
			:
			RequestMacOff = true;
			break;
				case
					SDL_EVENT_WINDOW_FOCUS_GAINED
					:
					gTrueBackgroundFlag = 0;
					break;
				case
					SDL_EVENT_WINDOW_FOCUS_LOST
					:
					gTrueBackgroundFlag = 1;
					break;
				case
					SDL_EVENT_WINDOW_MOUSE_ENTER
					:
					CaughtMouse = 1;
					break;
				case
					SDL_EVENT_WINDOW_MOUSE_LEAVE
					:
					CaughtMouse = 0;
					break;
				case SDL_EVENT_WINDOW_RESIZED:
					SDL_RenderClear(my_renderer);
					break;
		case
			SDL_EVENT_MOUSE_MOTION
			:
#if EnableFSMouseMotion && ! HaveWorkingWarp
			if (HaveMouseMotion) {
				MousePositionNotifyRelative(
					event->motion.xrel, event->motion.yrel);
			} else
#endif
			{
				MousePositionNotify(
					event->motion.x, event->motion.y);
			}
			break;
		case
			SDL_EVENT_MOUSE_BUTTON_DOWN
			:
			/* any mouse button, we don't care which */
#if EnableFSMouseMotion && ! HaveWorkingWarp
			if (HaveMouseMotion) {
				/* ignore position */
			} else
#endif
			{
				MousePositionNotify(
					event->button.x, event->button.y);
			}
			MyMouseButtonSet(true);
			break;
		case
			SDL_EVENT_MOUSE_BUTTON_UP
			:
#if EnableFSMouseMotion && ! HaveWorkingWarp
			if (HaveMouseMotion) {
				/* ignore position */
			} else
#endif
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
				Keyboard_UpdateKeyMap2(MKC_Left, true);
				Keyboard_UpdateKeyMap2(MKC_Left, false);
			} else if (event->wheel.x > 0) {
				Keyboard_UpdateKeyMap2(MKC_Right, true);
				Keyboard_UpdateKeyMap2(MKC_Right, false);
			}
			if (event->wheel.y < 0) {
				Keyboard_UpdateKeyMap2(MKC_Down, true);
				Keyboard_UpdateKeyMap2(MKC_Down, false);
			} else if(event->wheel.y > 0) {
				Keyboard_UpdateKeyMap2(MKC_Up, true);
				Keyboard_UpdateKeyMap2(MKC_Up, false);
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
				SDL_RaiseWindow(my_main_wind);
			}
			break;
	}
}

/* --- main window creation and disposal --- */

static int my_argc;
static char **my_argv;

static bool Screen_Init()
{
	bool v = false;

#if dbglog_OSGInit
	dbglog_writeln("enter Screen_Init");
#endif

	InitKeyCodes();

	if (
		!
		SDL_Init(
			SDL_INIT_AUDIO |
			SDL_INIT_VIDEO
		)
	) {
		fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
	} else
	{

		v = true;
	}

	return v;
}

static bool GrabMachine = false;

static void GrabTheMachine()
{
#if GrabKeysFullScreen
	SDL_SetWindowMouseGrab(my_main_wind, true);
#endif

#if EnableFSMouseMotion

#if HaveWorkingWarp
	/*
		if magnification changes, need to reset,
		even if HaveMouseMotion already true
	*/
	if (MyMoveMouse(ViewHStart + (ViewHSize / 2),
		ViewVStart + (ViewVSize / 2)))
	{
		SavedMouseH = ViewHStart + (ViewHSize / 2);
		SavedMouseV = ViewVStart + (ViewVSize / 2);
		HaveMouseMotion = true;
	}
#endif

#endif /* EnableFSMouseMotion */
}

static void UngrabMachine()
{
#if EnableFSMouseMotion

	if (HaveMouseMotion) {
#if HaveWorkingWarp
		(void) MyMoveMouse(CurMouseH, CurMouseV);
#endif

		HaveMouseMotion = false;
	}

#endif /* EnableFSMouseMotion */

#if GrabKeysFullScreen
	SDL_SetWindowMouseGrab(my_main_wind, false);
#endif
}

#if EnableFSMouseMotion && HaveWorkingWarp
static void MyMouseConstrain()
{
	int16_t shiftdh;
	int16_t shiftdv;

	if (SavedMouseH < ViewHStart + (ViewHSize / 4)) {
		shiftdh = ViewHSize / 2;
	} else if (SavedMouseH > ViewHStart + ViewHSize - (ViewHSize / 4)) {
		shiftdh = - ViewHSize / 2;
	} else {
		shiftdh = 0;
	}
	if (SavedMouseV < ViewVStart + (ViewVSize / 4)) {
		shiftdv = ViewVSize / 2;
	} else if (SavedMouseV > ViewVStart + ViewVSize - (ViewVSize / 4)) {
		shiftdv = - ViewVSize / 2;
	} else {
		shiftdv = 0;
	}
	if ((shiftdh != 0) || (shiftdv != 0)) {
		SavedMouseH += shiftdh;
		SavedMouseV += shiftdv;
		if (! MyMoveMouse(SavedMouseH, SavedMouseV)) {
			HaveMouseMotion = false;
		}
	}
}
#endif


enum {
	kMagStateNormal,
	kMagStateMagnifgy,
	kNumMagStates
};

#define kMagStateAuto kNumMagStates

static int CurWinIndx;
static bool HavePositionWins[kNumMagStates];
static int WinPositionsX[kNumMagStates];
static int WinPositionsY[kNumMagStates];

static bool CreateMainWindow()
{
	/*
		OSGLUxxx common:
		Set up somewhere for us to draw the emulated screen and
		receive mouse input. i.e. usually a window, as is the case
		for this port.

		The window should not be resizeable.

		Should look at the current value of UseMagnify and
		UseFullScreen.
	*/

	int NewWindowX;
	int NewWindowY;
	int NewWindowHeight = vMacScreenHeight;
	int NewWindowWidth = vMacScreenWidth;
	Uint32 flags =
	SDL_WINDOW_RESIZABLE
	;
	bool v = false;

#if 1
	if (UseMagnify) {
		NewWindowHeight *= WindowScale;
		NewWindowWidth *= WindowScale;
	}
#endif

	if (UseFullScreen)
	{
		/*
			We don't want physical screen mode to be changed in modern
			displays, so we pass this _DESKTOP flag.
		*/
		SDL_SetWindowFullscreen(my_main_wind, true);

		NewWindowX = SDL_WINDOWPOS_UNDEFINED;
		NewWindowY = SDL_WINDOWPOS_UNDEFINED;
	}
	else
	{
		int WinIndx;

		if (UseMagnify) {
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

		CurWinIndx = WinIndx;
	}


	if (nullptr == (my_main_wind = SDL_CreateWindow(
		(nullptr != n_arg) ? n_arg : kStrAppName,
		NewWindowWidth, NewWindowHeight,
		flags)))
	{
		fprintf(stderr, "SDL_CreateWindow fails: %s\n",
			SDL_GetError());
	} else
	if (nullptr == (my_renderer = SDL_CreateRenderer(
		my_main_wind,
		0 /* SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC */
			/*
				SDL_RENDERER_ACCELERATED not needed
				"no flags gives priority to available
				SDL_RENDERER_ACCELERATED renderers"
			*/
			/* would rather not require vsync */
		)))
	{
		fprintf(stderr, "SDL_CreateRenderer fails: %s\n",
			SDL_GetError());
	} else
	if (
		!SDL_SetWindowPosition(
			my_main_wind,
			NewWindowX,
			NewWindowY
		)
	) {
		fprintf(stderr, "SDL_SetWindowPosition fails: %s\n",
			SDL_GetError());
	} else
	if (
		!SDL_SetRenderLogicalPresentation(
			my_renderer,
			vMacScreenWidth,
			vMacScreenHeight,
			SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
		)
	) {
		fprintf(stderr, "SDL_SetRenderLogicalPresentation fails: %s\n",
			SDL_GetError());
	} else
	if (nullptr == (my_texture = SDL_CreateTexture(
		my_renderer,
		SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		vMacScreenWidth, vMacScreenHeight
		)))
	{
		fprintf(stderr, "SDL_CreateTexture fails: %s\n",
			SDL_GetError());
	} else
	if (
		!SDL_SetTextureScaleMode(
			my_texture,
			SDL_SCALEMODE_NEAREST
		)
	) {
		fprintf(stderr, "SDL_SetTextureScaleMode fails: %s\n",
			SDL_GetError());
	} else
	if (
		nullptr == (
			my_format =
			SDL_GetPixelFormatDetails
			(SDL_PIXELFORMAT_ARGB8888)
		)
	) {
		fprintf(stderr, "SDL_AllocFormat fails: %s\n",
			SDL_GetError());
	} else
	{
		/* SDL_ShowWindow(my_main_wind); */

		SDL_RenderClear(my_renderer);


		if (UseFullScreen)
		{
			int wr;
			int hr;

			SDL_SetWindowFullscreen(my_main_wind, true);
			SDL_GetWindowSizeInPixels
			(my_main_wind, &wr, &hr);

			ViewHSize = wr;
			ViewVSize = hr;
			if (UseMagnify) {
				ViewHSize /= WindowScale;
				ViewVSize /= WindowScale;
			}
			if (ViewHSize >= vMacScreenWidth) {
				ViewHStart = 0;
				ViewHSize = vMacScreenWidth;
			} else {
				ViewHSize &= ~ 1;
			}
			if (ViewVSize >= vMacScreenHeight) {
				ViewVStart = 0;
				ViewVSize = vMacScreenHeight;
			} else {
				ViewVSize &= ~ 1;
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

		ColorModeWorks = true;

		v = true;
	}

	return v;
}

static void CloseMainWindow()
{
	/*
		OSGLUxxx common:
		Dispose of anything set up by CreateMainWindow.
	*/

	if (nullptr != my_format) {
		my_format = nullptr;
	}

	if (nullptr != my_texture) {
		SDL_DestroyTexture(my_texture);
		my_texture = nullptr;
	}

	if (nullptr != my_renderer) {
		SDL_DestroyRenderer(my_renderer);
		my_renderer = nullptr;
	}

	if (nullptr != my_main_wind) {
		SDL_DestroyWindow(my_main_wind);
		my_main_wind = nullptr;
	}
}

#if EnableRecreateW
static void ZapMyWState()
{
	my_main_wind = nullptr;
	my_renderer = nullptr;
	my_texture = nullptr;
	my_format = nullptr;
}
#endif

#if EnableRecreateW
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
	SDL_Window *f_my_main_wind;
	SDL_Renderer *f_my_renderer;
	SDL_Texture *f_my_texture;
	const SDL_PixelFormatDetails
	*f_my_format;
};
#endif

#if EnableRecreateW
static void GetMyWState(MyWState *r)
{
	r->f_ViewHSize = ViewHSize;
	r->f_ViewVSize = ViewVSize;
	r->f_ViewHStart = ViewHStart;
	r->f_ViewVStart = ViewVStart;
	r->f_hOffset = hOffset;
	r->f_vOffset = vOffset;
	r->f_UseFullScreen = UseFullScreen;
	r->f_UseMagnify = UseMagnify;
	r->f_CurWinIndx = CurWinIndx;
	r->f_my_main_wind = my_main_wind;
	r->f_my_renderer = my_renderer;
	r->f_my_texture = my_texture;
	r->f_my_format = my_format;
}
#endif

#if EnableRecreateW
static void SetMyWState(MyWState *r)
{
	ViewHSize = r->f_ViewHSize;
	ViewVSize = r->f_ViewVSize;
	ViewHStart = r->f_ViewHStart;
	ViewVStart = r->f_ViewVStart;
	hOffset = r->f_hOffset;
	vOffset = r->f_vOffset;
	UseFullScreen = r->f_UseFullScreen;
	UseMagnify = r->f_UseMagnify;
	CurWinIndx = r->f_CurWinIndx;
	my_main_wind = r->f_my_main_wind;
	my_renderer = r->f_my_renderer;
	my_texture = r->f_my_texture;
	my_format = r->f_my_format;
}
#endif

enum {
	kWinStateWindowed,
	kWinStateFullScreen,
	kNumWinStates
};

static int WinMagStates[kNumWinStates];

#if EnableRecreateW
static bool ReCreateMainWindow()
{
	/*
		OSGLUxxx common:
		Like CreateMainWindow (which it calls), except may be
		called when already have window, without CloseMainWindow
		being called first. (Usually with different
		values of WantMagnify and WantFullScreen than
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
#if HaveWorkingWarp
	bool HadCursorHidden = HaveCursorHidden;
#endif
	int OldWinState =
		UseFullScreen ? kWinStateFullScreen : kWinStateWindowed;
	int OldMagState =
		UseMagnify ? kMagStateMagnifgy : kMagStateNormal;

	WinMagStates[OldWinState] =
		OldMagState;

	if (! UseFullScreen)
	{
		SDL_GetWindowPosition(my_main_wind,
			&WinPositionsX[CurWinIndx],
			&WinPositionsY[CurWinIndx]);
		HavePositionWins[CurWinIndx] = true;
	}

	ForceShowCursor(); /* hide/show cursor api is per window */

	if (GrabMachine) {
		GrabMachine = false;
		UngrabMachine();
	}

	GetMyWState(&old_state);

	ZapMyWState();

	UseMagnify = WantMagnify;
	UseFullScreen = WantFullScreen;

	if (! CreateMainWindow()) {
		CloseMainWindow();
		SetMyWState(&old_state);

		/* avoid retry */
		WantFullScreen = UseFullScreen;
		WantMagnify = UseMagnify;

	} else {
		GetMyWState(&new_state);
		SetMyWState(&old_state);
		CloseMainWindow();
		SetMyWState(&new_state);

#if HaveWorkingWarp
		if (HadCursorHidden) {
			(void) MyMoveMouse(CurMouseH, CurMouseV);
		}
#endif
	}

	return true;
}
#endif


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
	WantFullScreen = ! WantFullScreen;

	{
		int OldWinState =
			UseFullScreen ? kWinStateFullScreen : kWinStateWindowed;
		int OldMagState =
			UseMagnify ? kMagStateMagnifgy : kMagStateNormal;
		int NewWinState =
			WantFullScreen ? kWinStateFullScreen : kWinStateWindowed;
		int NewMagState = WinMagStates[NewWinState];

		WinMagStates[OldWinState] = OldMagState;
		if (kMagStateAuto != NewMagState) {
			WantMagnify = (kMagStateMagnifgy == NewMagState);
		} else {
			WantMagnify = false;
			if (WantFullScreen) {
				SDL_Rect r;

				if (0 == SDL_GetDisplayBounds(0, &r)) {
					if ((r.w >= vMacScreenWidth * WindowScale)
						&& (r.h >= vMacScreenHeight * WindowScale)
						)
					{
						WantMagnify = true;
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
	DisableKeyRepeat();
}

static void EnterBackground()
{
	RestoreKeyRepeat();
	DisconnectKeyCodes3();

	ForceShowCursor();
}

static void LeaveSpeedStopped()
{
	MySound_Start();

	StartUpTimeAdjust();
}

static void EnterSpeedStopped()
{
	MySound_Stop();
}

static void CheckForSavedTasks()
{
	if (MyEvtQNeedRecover) {
		MyEvtQNeedRecover = false;

		/* attempt cleanup, MyEvtQNeedRecover may get set again */
		MyEvtQTryRecoverFromFull();
	}

#if EnableFSMouseMotion && HaveWorkingWarp
	if (HaveMouseMotion) {
		MyMouseConstrain();
	}
#endif

	if (RequestMacOff) {
		RequestMacOff = false;
		if (AnyDiskInserted()) {
			MacMsgOverride(Localize(kStrQuitWarningTitle),
				Localize(kStrQuitWarningMessage));
		} else {
			ForceMacOff = true;
		}
	}

	if (ForceMacOff) {
		return;
	}

	if (gTrueBackgroundFlag != gBackgroundFlag) {
		gBackgroundFlag = gTrueBackgroundFlag;
		if (gTrueBackgroundFlag) {
			EnterBackground();
		} else {
			LeaveBackground();
		}
	}

	if (CurSpeedStopped != (SpeedStopped ||
		(gBackgroundFlag && ! RunInBackground
		)))
	{
		CurSpeedStopped = ! CurSpeedStopped;
		if (CurSpeedStopped) {
			EnterSpeedStopped();
		} else {
			LeaveSpeedStopped();
		}
	}

	if ((nullptr != SavedBriefMsg) & ! MacMsgDisplayed) {
		MacMsgDisplayOn();
	}

#if EnableRecreateW
	if (0
		|| (UseMagnify != WantMagnify)
		|| (UseFullScreen != WantFullScreen)
		)
	{
		(void) ReCreateMainWindow();
	}
#endif

	if (GrabMachine != (
		UseFullScreen &&
		! (gTrueBackgroundFlag || CurSpeedStopped)))
	{
		GrabMachine = ! GrabMachine;
		if (GrabMachine) {
			GrabTheMachine();
		} else {
			UngrabMachine();
		}
	}

	if (NeedWholeScreenDraw) {
		NeedWholeScreenDraw = false;
		ScreenChangedAll();
	}

#if NeedRequestIthDisk
	if (0 != RequestIthDisk) {
		Sony_InsertIth(RequestIthDisk);
		RequestIthDisk = 0;
	}
#endif

	if (vSonyNewDiskWanted) {
		if (vSonyNewDiskName != NotAPbuf) {
			uint8_t *p = static_cast<uint8_t *>(PbufDat[vSonyNewDiskName]);
			uint32_t L = PbufSize[vSonyNewDiskName];
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
			MakeNewDisk(vSonyNewDiskSize, drivename);
			PbufDispose(vSonyNewDiskName);
			vSonyNewDiskName = NotAPbuf;
		} else
		{
			char defaultName[] = "untitled.dsk";
			MakeNewDisk(vSonyNewDiskSize, defaultName);
		}
		vSonyNewDiskWanted = false;
	}

	if (HaveCursorHidden != (WantCursorHidden
		&& ! (gTrueBackgroundFlag || CurSpeedStopped)))
	{
		HaveCursorHidden = ! HaveCursorHidden;
		HaveCursorHidden ? SDL_HideCursor() : SDL_ShowCursor();
	}
}

/* --- command line parsing --- */

static bool ScanCommandLine()
{
	char *pa;
	int i = 1;

#if dbglog_OSGInit
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
#if dbglog_HAVE
				dbglog_writeln("ignoring command line argument");
				dbglog_writeln(pa);
#endif
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
	return TrueEmulatedTime == OnTrueTime;
}

void WaitForNextTick()
{
	/* Deterministic path: no wall-clock gating, advance exactly one
	   tick per call.  Used for --record / --verify golden files. */
	if (g_SkipThrottle) {
		CheckForSystemEvents();
		DoneWithDrawingForTick();
		++OnTrueTime;
		return;
	}

	for (;;) {
		CheckForSystemEvents();
		CheckForSavedTasks();

		if (ForceMacOff) {
			return;
		}

		if (CurSpeedStopped) {
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
		MySound_SecondNotify();
	}

	if ((! gBackgroundFlag)
#if UseMotionEvents
		&& (! CaughtMouse)
#endif
		)
	{
		CheckMouseState();
	}

	OnTrueTime = TrueEmulatedTime;
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
#if dbglog_HAVE
	if (!dbglog_ReserveAlloc())
		goto fail;
#endif
	if (!AllocBlock(&ROM, g_machine->config().romSize, false))
		goto fail;
	if (!AllocBlock(&screencomparebuff, vMacScreenNumBytes, true))
		goto fail;
#if UseControlKeys
	if (!AllocBlock(&CntrlDisplayBuff, vMacScreenNumBytes, false))
		goto fail;
#endif
	if (!AllocBlock(&CLUT_final, CLUT_finalsz, false))
		goto fail;
	if (!AllocBlock((uint8_t **)&TheSoundBuffer, dbhBufferSize, false))
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
	free(ROM); ROM = nullptr;
	free(screencomparebuff); screencomparebuff = nullptr;
#if UseControlKeys
	free(CntrlDisplayBuff); CntrlDisplayBuff = nullptr;
#endif
	free(CLUT_final); CLUT_final = nullptr;
	free(TheSoundBuffer); TheSoundBuffer = nullptr;
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
   parse arguments, load ROM and disks, create window and sound. */
static bool InitOSGLU()
{
#define INIT_STEP(name, expr) \
	if (!(expr)) { fprintf(stderr, "[SDL init] " name " FAILED\n"); return false; }

	INIT_STEP("AllocMyMemory", AllocMyMemory())
	INIT_STEP("InitWhereAmI", InitWhereAmI())
#if dbglog_HAVE
	/* dbglog_open is best-effort: fail to open log file is non-fatal */
	dbglog_open();
#endif
	INIT_STEP("ScanCommandLine", ScanCommandLine())
	INIT_STEP("LoadMacRom", LoadMacRom(d_arg, app_parent, pref_dir))
	INIT_STEP("LoadInitialImages", LoadInitialImages())
	INIT_STEP("InitLocationDat", InitLocationDat())
	INIT_STEP("Screen_Init", Screen_Init())
	INIT_STEP("MySound_Init", MySound_Init())
	INIT_STEP("CreateMainWindow", CreateMainWindow())
	INIT_STEP("WaitForRom", WaitForRom())

#undef INIT_STEP
	return true;
}

static void UnInitOSGLU()
{
	/*
		OSGLUxxx common:
		Do all clean ups needed before the program quits.
	*/

	if (MacMsgDisplayed) {
		MacMsgDisplayOff();
	}

	RestoreKeyRepeat();
	UngrabMachine();
	MySound_Stop();
	MySound_UnInit();
	UnInitPbufs();
	UnInitDrives();

	ForceShowCursor();

#if dbglog_HAVE
	dbglog_close();
#endif

	UninitWhereAmI();
	UnallocMyMemory();

	CheckSavedMacMsg();

	CloseMainWindow();

	SDL_QuitSubSystem(SDL_INIT_AUDIO | SDL_INIT_VIDEO);

	SDL_Quit();
}

int main(int argc, char **argv)
{
	my_argc = argc;
	my_argv = argv;

	ProgramEarlyInit(argc, argv);

	WindowScale = GetEmulatorConfig().windowScale;
	SpeedValue = GetEmulatorConfig().speed;

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
	if (InitOSGLU()) {
		for (const auto& diskPath : lc.diskPaths) {
			(void) Sony_Insert1(const_cast<char*>(diskPath.c_str()), false);
		}
		ProgramMain();
	}
	UnInitOSGLU();
	ProgramCleanup();

	return 0;
}

