/*
	sdl_backend.cpp — SDL3 implementation of PlatformBackend
*/

#include "platform/sdl_backend.h"
#include "platform/sdl_keyboard.h"
#include "platform/sdl_sound.h"
#include "platform/platform.h"

/* From osglu_common.h — forward declare what we need to avoid
   pulling in the full platform include chain. */
extern void InitKeyCodes();

#include <cstdio>

/* --- Lifecycle --- */

bool SdlBackend::init(EmulatorShell* shell)
{
	shell_ = shell;

	InitKeyCodes();

	if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
		fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
		return false;
	}

	return true;
}

void SdlBackend::shutdown()
{
	SDL_QuitSubSystem(SDL_INIT_AUDIO | SDL_INIT_VIDEO);
	SDL_Quit();
}

void SdlBackend::runLoop()
{
	/* Stub — will be implemented in Phase 3. */
}

/* --- Window --- */

bool SdlBackend::createWindow(const char* title,
	int width, int height, bool fullscreen)
{
	Uint32 flags = SDL_WINDOW_RESIZABLE;

	if (nullptr == (window_ = SDL_CreateWindow(
		title, width, height, flags)))
	{
		fprintf(stderr, "SDL_CreateWindow fails: %s\n",
			SDL_GetError());
		return false;
	}

	if (nullptr == (renderer_ = SDL_CreateRenderer(window_, 0))) {
		fprintf(stderr, "SDL_CreateRenderer fails: %s\n",
			SDL_GetError());
		return false;
	}

	if (!SDL_SetRenderLogicalPresentation(
		renderer_,
		vMacScreenWidth,
		vMacScreenHeight,
		SDL_LOGICAL_PRESENTATION_INTEGER_SCALE))
	{
		fprintf(stderr, "SDL_SetRenderLogicalPresentation fails: %s\n",
			SDL_GetError());
		return false;
	}

	if (nullptr == (texture_ = SDL_CreateTexture(
		renderer_,
		SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		vMacScreenWidth, vMacScreenHeight)))
	{
		fprintf(stderr, "SDL_CreateTexture fails: %s\n",
			SDL_GetError());
		return false;
	}

	if (!SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST)) {
		fprintf(stderr, "SDL_SetTextureScaleMode fails: %s\n",
			SDL_GetError());
		return false;
	}

	if (nullptr == (format_ = SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_ARGB8888))) {
		fprintf(stderr, "SDL_GetPixelFormatDetails fails: %s\n",
			SDL_GetError());
		return false;
	}

	SDL_RenderClear(renderer_);

	if (fullscreen) {
		SDL_SetWindowFullscreen(window_, true);
	}

	g_colorModeWorks = true;
	return true;
}

void SdlBackend::destroyWindow()
{
	format_ = nullptr;

	if (nullptr != texture_) {
		SDL_DestroyTexture(texture_);
		texture_ = nullptr;
	}

	if (nullptr != renderer_) {
		SDL_DestroyRenderer(renderer_);
		renderer_ = nullptr;
	}

	if (nullptr != window_) {
		SDL_DestroyWindow(window_);
		window_ = nullptr;
	}
}

bool SdlBackend::recreateWindow(const char* title,
	int width, int height, bool fullscreen)
{
	/* This is a simple destroy+create. The caller (sdl.cpp) handles
	   the state save/restore, cursor show/grab logic. */
	destroyWindow();
	return createWindow(title, width, height, fullscreen);
}

void SdlBackend::getWindowSize(int* w, int* h)
{
	SDL_GetWindowSizeInPixels(window_, w, h);
}

void SdlBackend::getWindowPosition(int* x, int* y)
{
	SDL_GetWindowPosition(window_, x, y);
}

void SdlBackend::setWindowPosition(int x, int y)
{
	SDL_SetWindowPosition(window_, x, y);
}

void SdlBackend::setFullscreen(bool fullscreen)
{
	SDL_SetWindowFullscreen(window_, fullscreen);
}

void SdlBackend::clearScreen()
{
	SDL_RenderClear(renderer_);
}

/* --- Cursor --- */

void SdlBackend::showCursor()
{
	SDL_ShowCursor();
}

void SdlBackend::hideCursor()
{
	SDL_HideCursor();
}

bool SdlBackend::warpCursor(int x, int y)
{
	SDL_WarpMouseInWindow(window_, x, y);
	return true;
}

void SdlBackend::setMouseGrab(bool grab)
{
	SDL_SetWindowMouseGrab(window_, grab);
}

/* --- Audio --- */

bool SdlBackend::audioInit()
{
	return Sound_Init();
}

void SdlBackend::audioStart()
{
	Sound_Start();
}

void SdlBackend::audioStop()
{
	Sound_Stop();
}

void SdlBackend::audioShutdown()
{
	Sound_UnInit();
}

/* --- Keyboard --- */

void SdlBackend::disableKeyRepeat()
{
	DisableKeyRepeat();
}

void SdlBackend::restoreKeyRepeat()
{
	RestoreKeyRepeat();
}

/* --- Dialog --- */

void SdlBackend::showMessageBox(const char* title, const char* message)
{
	if (0 != SDL_ShowSimpleMessageBox(
		SDL_MESSAGEBOX_ERROR,
		title, message, window_))
	{
		fprintf(stderr, "%s\n", title);
		fprintf(stderr, "%s\n", message);
	}
}

/* --- Query --- */

bool SdlBackend::getDisplayBounds(PlatformDisplayBounds* bounds)
{
	SDL_Rect r;
	if (0 == SDL_GetDisplayBounds(0, &r)) {
		bounds->x = r.x;
		bounds->y = r.y;
		bounds->w = r.w;
		bounds->h = r.h;
		return true;
	}
	return false;
}

/* --- Window state management --- */

void SdlBackend::zapWindowState()
{
	window_ = nullptr;
	renderer_ = nullptr;
	texture_ = nullptr;
	format_ = nullptr;
}

void SdlBackend::getWindowState(SdlWindowState* state)
{
	state->window = window_;
	state->renderer = renderer_;
	state->texture = texture_;
	state->format = format_;
}

void SdlBackend::setWindowState(const SdlWindowState* state)
{
	window_ = state->window;
	renderer_ = state->renderer;
	texture_ = state->texture;
	format_ = state->format;
}
