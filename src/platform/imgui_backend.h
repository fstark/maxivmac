/*
	imgui_backend.h — ImGui + SDL3 + OpenGL3 implementation of PlatformBackend

	Renders the emulator viewport as a GL texture in an ImGui window,
	with a menu bar and (eventually) debug windows. Uses SDL3 for
	windowing/input and OpenGL3 for rendering.
*/

#ifndef IMGUI_BACKEND_H
#define IMGUI_BACKEND_H

#include "platform/platform_backend.h"
#include "platform/imgui_model_selector.h"
#include "platform/imgui_overlay.h"
#include "platform/imgui_tool_registry.h"
#include <SDL3/SDL.h>

typedef unsigned int GLuint;

/* UI state machine — determines what the backend draws each frame. */
enum class UIState {
	ModelSelector,   // Pre-boot: user picks a model + config
	Windowed,        // Running emulation in a window (no chrome)
	Fullscreen,      // Running emulation fullscreen
	Developer,       // Running emulation + dockable debug tools
};

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

	/* UI state transitions */
	void setUIState(UIState state) { uiState_ = state; }
	UIState getUIState() const { return uiState_; }
	void enterWindowed();
	void enterFullscreen();
	void enterDeveloper();

	/* Pre-boot window: create a window suitable for the model selector
	   (no emulation texture, no emulator shell dependency). */
	bool createSelectorWindow();

	/* Tool registry for developer mode */
	ToolRegistry& getToolRegistry() { return toolRegistry_; }

private:
	EmulatorShell* shell_ = nullptr;
	SDL_Window* window_ = nullptr;
	SDL_GLContext glContext_ = nullptr;
	GLuint emuTextureId_ = 0;
	int emuTexW_ = 0;
	int emuTexH_ = 0;
	float emuViewOriginX_ = 0;
	float emuViewOriginY_ = 0;
	bool relativeMouseMode_ = false;
	bool emuViewportHovered_ = false;

	/* UI state */
	UIState uiState_ = UIState::ModelSelector;
	bool    overlayVisible_ = false;
	bool    pendingBoot_ = false;
	LaunchConfig pendingBootConfig_;

	/* Overlay */
	ControlOverlay overlay_;

	/* Saved window geometry for returning from fullscreen/developer */
	int savedWinX_ = 0, savedWinY_ = 0;
	int savedWinW_ = 0, savedWinH_ = 0;

	PlatformEvent translateSdlEvent(SDL_Event& event);
	bool imGuiConsumedEvent(const SDL_Event& event) const;
	void uploadFramebuffer();
	void drawMenuBar();
	void drawEmulatorViewport();
	void drawViewportWindowed();
	void drawViewportFullscreen();
	void drawViewportDeveloper();
	void displayEmulatorImage(float w, float h);

	/* Model selector (pre-boot) */
	ModelSelector modelSelector_;
	void drawModelSelector();
	void bootFromSelector(const LaunchConfig& config);

	/* Tool registry for developer mode */
	ToolRegistry toolRegistry_;

	/* Per-state draw dispatchers */
	void drawWindowedState();
	void drawFullscreenState();
	void drawDeveloperState();
};

#endif /* IMGUI_BACKEND_H */
