/*
	imgui_backend.h — ImGui + SDL3 + OpenGL3 implementation of PlatformBackend

	Renders the emulator viewport as a GL texture in an ImGui window,
	with a menu bar and (eventually) debug windows. Uses SDL3 for
	windowing/input and OpenGL3 for rendering.
*/

#ifndef IMGUI_BACKEND_H
#define IMGUI_BACKEND_H

#include "platform/platform_backend.h"
#include <SDL3/SDL.h>

typedef unsigned int GLuint;

class ImGuiBackend : public PlatformBackend {
public:
	ImGuiBackend() = default;
	~ImGuiBackend() override = default;

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

	const char* getAppParent() override;
	char* getPrefDir(const char* org, const char* app) override;
	void freePath(void* path) override;

private:
	EmulatorShell* shell_ = nullptr;
	SDL_Window* window_ = nullptr;
	SDL_GLContext glContext_ = nullptr;
	GLuint emuTextureId_ = 0;
	int emuTexW_ = 0;
	int emuTexH_ = 0;

	PlatformEvent translateSdlEvent(SDL_Event& event);
	bool imGuiConsumedEvent(const SDL_Event& event) const;
	void uploadFramebuffer();
	void drawMenuBar();
	void drawEmulatorViewport();
};

#endif /* IMGUI_BACKEND_H */
