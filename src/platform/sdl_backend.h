/*
	sdl_backend.h — SDL3 implementation of PlatformBackend

	Wraps all SDL3 calls behind the PlatformBackend interface.
	During the Phase 2 transition, sdl.cpp still orchestrates
	the event loop and exposes transitional getters for SDL objects.
*/

#ifndef SDL_BACKEND_H
#define SDL_BACKEND_H

#include "platform/platform_backend.h"
#include <SDL3/SDL.h>

struct SdlWindowState {
	uint16_t viewHSize;
	uint16_t viewVSize;
	uint16_t viewHStart;
	uint16_t viewVStart;
	int hOffset;
	int vOffset;
	bool useFullScreen;
	bool useMagnify;
	int curWinIndx;
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;
	const SDL_PixelFormatDetails* format;
};

class SdlBackend : public PlatformBackend {
public:
	SdlBackend() = default;
	~SdlBackend() override = default;

	/* PlatformBackend interface */
	bool init(EmulatorShell* shell) override;
	void shutdown() override;
	void runLoop() override;

	bool createWindow(const char* title,
		int width, int height, bool fullscreen) override;
	void destroyWindow() override;
	bool recreateWindow(const char* title,
		int width, int height, bool fullscreen) override;
	void getWindowSize(int* w, int* h) override;
	void getWindowPosition(int* x, int* y) override;
	void setWindowPosition(int x, int y) override;
	void setFullscreen(bool fullscreen) override;
	void clearScreen() override;

	void showCursor() override;
	void hideCursor() override;
	bool warpCursor(int x, int y) override;
	void setMouseGrab(bool grab) override;

	bool audioInit() override;
	void audioStart() override;
	void audioStop() override;
	void audioShutdown() override;

	void disableKeyRepeat() override;
	void restoreKeyRepeat() override;

	void showMessageBox(const char* title, const char* message) override;

	bool getDisplayBounds(PlatformDisplayBounds* bounds) override;

	/* Transitional getters — sdl.cpp still needs direct access during
	   Phase 2 migration. These will be removed in Phase 3. */
	SDL_Window* window() const { return window_; }
	SDL_Renderer* renderer() const { return renderer_; }
	SDL_Texture* texture() const { return texture_; }
	const SDL_PixelFormatDetails* format() const { return format_; }

	/* Window state save/restore for recreateWindow */
	void zapWindowState();
	void getWindowState(SdlWindowState* state);
	void setWindowState(const SdlWindowState* state);

private:
	EmulatorShell* shell_ = nullptr;
	SDL_Window* window_ = nullptr;
	SDL_Renderer* renderer_ = nullptr;
	SDL_Texture* texture_ = nullptr;
	const SDL_PixelFormatDetails* format_ = nullptr;

	PlatformEvent translateSdlEvent(SDL_Event& event);
	void presentIfDirty();
};

#endif /* SDL_BACKEND_H */
