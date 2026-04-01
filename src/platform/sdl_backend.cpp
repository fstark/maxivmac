/*
	sdl_backend.cpp — SDL3 implementation of PlatformBackend
*/

#include "platform/sdl_backend.h"
#include "platform/emulator_shell.h"
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
	if (!shell_) return;

	while (!shell_->shouldQuit()) {
		/* Poll events */
		SDL_Event event;
		int evtCount = 10;
		while ((--evtCount >= 0) && SDL_PollEvent(&event)) {
			PlatformEvent pEvt = translateSdlEvent(event);
			if (pEvt.type != PlatformEvent::Type::None) {
				shell_->dispatchEvent(pEvt);
			}
		}

		shell_->processSavedTasks();
		if (shell_->shouldQuit()) break;

		if (shell_->isSpeedStopped()) {
			presentIfDirty();
			SDL_Event waitEvt;
			if (SDL_WaitEvent(&waitEvt)) {
				PlatformEvent pEvt = translateSdlEvent(waitEvt);
				if (pEvt.type != PlatformEvent::Type::None) {
					shell_->dispatchEvent(pEvt);
				}
			}
			continue;
		}

		if (!shell_->tickIsDue()) {
			(void) SDL_Delay(shell_->getDelayMs());
			continue;
		}

		while (shell_->tickIsDue() && !shell_->shouldQuit()) {
			shell_->runOneTick();
		}

		presentIfDirty();
	}
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

void SdlBackend::setMouseGrab(bool grab)
{
	SDL_SetWindowMouseGrab(window_, grab);
	SDL_SetWindowRelativeMouseMode(window_, grab);
	relativeMouseMode_ = grab;
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

/* --- Paths --- */

const char* SdlBackend::getAppParent()
{
	return SDL_GetBasePath();
}

char* SdlBackend::getPrefDir(const char* org, const char* app)
{
	return SDL_GetPrefPath(org, app);
}

void SdlBackend::freePath(void* path)
{
	SDL_free(path);
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

/* --- Event translation --- */

PlatformEvent SdlBackend::translateSdlEvent(SDL_Event& event)
{
	PlatformEvent pEvt;

	/* Convert coordinates for non-fullscreen rendering */
	SDL_ConvertEventToRenderCoordinates(renderer_, &event);

	switch (event.type) {
		case SDL_EVENT_QUIT:
			pEvt.type = PlatformEvent::Type::Quit;
			break;
		case SDL_EVENT_WINDOW_FOCUS_GAINED:
			pEvt.type = PlatformEvent::Type::FocusGained;
			break;
		case SDL_EVENT_WINDOW_FOCUS_LOST:
			pEvt.type = PlatformEvent::Type::FocusLost;
			break;
		case SDL_EVENT_WINDOW_MOUSE_ENTER:
			pEvt.type = PlatformEvent::Type::MouseEnter;
			break;
		case SDL_EVENT_WINDOW_MOUSE_LEAVE:
			pEvt.type = PlatformEvent::Type::MouseLeave;
			break;
		case SDL_EVENT_WINDOW_RESIZED:
			pEvt.type = PlatformEvent::Type::WindowResized;
			break;
		case SDL_EVENT_MOUSE_MOTION:
			pEvt.type = PlatformEvent::Type::MouseMove;
			if (relativeMouseMode_) {
				pEvt.isRelative = true;
				pEvt.dx = event.motion.xrel;
				pEvt.dy = event.motion.yrel;
			} else {
				pEvt.x = event.motion.x;
				pEvt.y = event.motion.y;
			}
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			pEvt.type = PlatformEvent::Type::MouseButtonDown;
			if (relativeMouseMode_) {
				pEvt.isRelative = true;
			} else {
				pEvt.x = event.button.x;
				pEvt.y = event.button.y;
			}
			break;
		case SDL_EVENT_MOUSE_BUTTON_UP:
			pEvt.type = PlatformEvent::Type::MouseButtonUp;
			if (relativeMouseMode_) {
				pEvt.isRelative = true;
			} else {
				pEvt.x = event.button.x;
				pEvt.y = event.button.y;
			}
			break;
		case SDL_EVENT_KEY_DOWN: {
			uint8_t mkc = SDLScan2MacKeyCode(event.key.scancode);
			if (mkc != 0xFF) { /* MKC_None */
				pEvt.type = PlatformEvent::Type::KeyDown;
				pEvt.macKeyCode = mkc;
			}
			break;
		}
		case SDL_EVENT_KEY_UP: {
			uint8_t mkc = SDLScan2MacKeyCode(event.key.scancode);
			if (mkc != 0xFF) {
				pEvt.type = PlatformEvent::Type::KeyUp;
				pEvt.macKeyCode = mkc;
			}
			break;
		}
		case SDL_EVENT_MOUSE_WHEEL:
			pEvt.type = PlatformEvent::Type::MouseWheel;
			pEvt.wheelX = event.wheel.x;
			pEvt.wheelY = event.wheel.y;
			break;
		case SDL_EVENT_DROP_FILE:
			pEvt.type = PlatformEvent::Type::FileDrop;
			pEvt.filePath = event.drop.data;
			break;
		default:
			break;
	}

	return pEvt;
}

/* --- Present framebuffer to screen --- */

void SdlBackend::presentIfDirty()
{
	if (!shell_ || !shell_->isFramebufferDirty()) return;

	void* pixels;
	int pitch;

	if (!SDL_LockTexture(texture_, nullptr, &pixels, &pitch)) {
		shell_->clearDirtyFlag();
		return;
	}

	const uint8_t* fb = shell_->getFramebuffer();
	int fbPitch = vMacScreenWidth * 4;

	if (pitch == fbPitch) {
		memcpy(pixels, fb, fbPitch * vMacScreenHeight);
	} else {
		/* Row-by-row copy if pitches differ */
		uint8_t* dst = static_cast<uint8_t*>(pixels);
		for (int row = 0; row < vMacScreenHeight; ++row) {
			memcpy(dst, fb + row * fbPitch, fbPitch);
			dst += pitch;
		}
	}

	SDL_UnlockTexture(texture_);
	SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);
	SDL_RenderPresent(renderer_);

	shell_->clearDirtyFlag();
}
