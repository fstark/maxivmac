/*
	imgui_backend.h — ImGui + SDL3 + OpenGL3 implementation of PlatformBackend

	Renders the emulator viewport as a GL texture in an ImGui window,
	with a menu bar and (eventually) debug windows. Uses SDL3 for
	windowing/input and OpenGL3 for rendering.
*/

#ifndef IMGUI_BACKEND_H
#define IMGUI_BACKEND_H

#include "platform/platform_backend.h"
#include "platform/imgui_launcher.h"
#include "platform/imgui_overlay.h"
#include "core/config_loader.h"
#include <SDL3/SDL.h>

typedef unsigned int GLuint;

/* UI state machine — determines what the backend draws each frame. */
enum class UIState
{
	Launcher,	// Pre-boot: .mac file launcher cards
	Windowed,	// Running emulation in a window (no chrome)
	Fullscreen, // Running emulation fullscreen
};

/* Scaling mode for the emulator viewport. */
enum class ScalingMode : uint8_t
{
	PixelPerfect,
	Stretched
};

/* Actions that can be triggered by shortcuts or overlay buttons. */
enum class UIAction : uint8_t
{
	None,
	ToggleFullscreen,
	ToggleScaling,
	Zoom,
	Screenshot,
	SpeedUp,
	SpeedDown,
	SpeedReset,
	TogglePaused,
	InsertDisk,
	Reboot,
};

/* GL texture filter for the emulator viewport. */
enum class TextureFilter
{
	Nearest,
	Linear
};

class ImGuiBackend : public PlatformBackend
{
public:
	ImGuiBackend() = default;
	~ImGuiBackend() override = default;

	bool init(EmulatorShell *shell) override;
	void shutdown() override;
	void runLoop() override;

	bool createWindow(const char *title, int width, int height, bool fullscreen) override;
	void destroyWindow() override;
	bool recreateWindow(const char *title, int width, int height, bool fullscreen) override;
	void getWindowSize(int *w, int *h) override;
	void getWindowPosition(int *x, int *y) override;
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

	void showMessageBox(const char *title, const char *message) override;
	bool getDisplayBounds(PlatformDisplayBounds *bounds) override;
	void onResolutionChanged(uint16_t newW, uint16_t newH) override;

	const char *getAppParent() override;
	char *getPrefDir(const char *org, const char *app) override;
	void freePath(void *path) override;

	/* UI state transitions */
	void setUIState(UIState state) { uiState_ = state; }
	UIState getUIState() const { return uiState_; }
	void enterWindowed();
	void enterFullscreen();

	/* Create the Launcher UI from .mac file entries. */
	bool createLauncher(std::vector<MacFileEntry> entries);

	/* GL texture filter */
	void setTextureFilter(TextureFilter f);
	TextureFilter textureFilter() const { return textureFilter_; }

	/* Scaling mode */
	ScalingMode scalingMode() const { return scalingMode_; }
	void setScalingMode(ScalingMode m);

	/* Action dispatch (used by overlay and shortcuts) */
	void executeAction(UIAction action);

	/* Shell accessor (for file dialog callback) */
	EmulatorShell *shell() { return shell_; }

private:
	EmulatorShell *shell_ = nullptr;
	SDL_Window *window_ = nullptr;
	SDL_GLContext glContext_ = nullptr;
	GLuint emuTextureId_ = 0;
	int emuTexW_ = 0;
	int emuTexH_ = 0;
	float emuViewOriginX_ = 0;
	float emuViewOriginY_ = 0;
	float emuViewW_ = 0;
	float emuViewH_ = 0;
	bool relativeMouseMode_ = false;
	bool emuViewportHovered_ = false;
	bool cursorHidden_ = false;
	TextureFilter textureFilter_ = TextureFilter::Linear;
	ScalingMode scalingMode_ = ScalingMode::PixelPerfect;
	bool snapping_ = false;
	int currentScale_ = 2;

	/* UI state */
	UIState uiState_ = UIState::Launcher;
	enum class OverlayMode : uint8_t
	{
		Hidden,
		PeekPending,
		Peek,
		Sticky
	};
	OverlayMode overlayMode_ = OverlayMode::Hidden;
	uint64_t ctrlDownTick_ = 0;
	static constexpr uint64_t kPeekThresholdMs = 250;
	bool pendingBoot_ = false;
	LaunchConfig pendingBootConfig_;

	/* Overlay */
	ControlOverlay overlay_;

	/* Saved window geometry for returning from fullscreen */
	int savedWinX_ = 0, savedWinY_ = 0;
	int savedWinW_ = 0, savedWinH_ = 0;

	/* Zoom (our own maximize: largest Pixel Perfect, centered) */
	bool zoomed_ = false;
	int preZoomX_ = 0, preZoomY_ = 0;
	int preZoomW_ = 0, preZoomH_ = 0;

	PlatformEvent translateSdlEvent(SDL_Event &event);
	bool imGuiConsumedEvent(const SDL_Event &event) const;
	void uploadFramebuffer();
	void drawEmulatorViewport();
	void drawViewportWindowed();
	void drawViewportFullscreen();
	void displayEmulatorImage(float w, float h);

	/* Launcher (pre-boot) */
	Launcher launcher_;
	std::string launcherDataDir_;
	void drawLauncher();
	void bootFromLauncher(const MacFileEntry &entry);
	void bootFromLauncherConfig(const LaunchConfig &config);

	/* Per-state draw dispatchers */
	void drawWindowedState();
	void drawFullscreenState();

	/* Actions / shortcuts */
	void adjustSpeed(int delta);
	void setSpeed(int idx);
	void captureScreenshot();
	void openFileDialog();
	void toggleZoom();
};

#endif /* IMGUI_BACKEND_H */
