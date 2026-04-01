/*
	imgui_backend.cpp — ImGui + SDL3 + OpenGL3 backend implementation

	Renders the emulator's ARGB8888 framebuffer as a GL texture inside
	an ImGui window.  SDL3 handles windowing, input, and audio; Dear
	ImGui provides the UI chrome (menu bar, viewport, future debug
	windows).
*/

#include "platform/imgui_backend.h"
#include "platform/emulator_shell.h"
#include "platform/sdl_keyboard.h"
#include "platform/sdl_sound.h"
#include "platform/platform.h"
#include "platform/imgui_debug_windows.h"
#include "core/main.h"

/* Forward declarations to avoid pulling in the full osglu_common.h
   include chain (which depends on emulator config macros). */
extern void InitKeyCodes();
extern bool g_requestMacOff;

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

/* ── init / shutdown ─────────────────────────────────── */

bool ImGuiBackend::init(EmulatorShell* shell)
{
	shell_ = shell;

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return false;
	}

	/* Request OpenGL 3.2 Core (minimum for ImGui) */
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
		SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	InitKeyCodes();
	return true;
}

void ImGuiBackend::shutdown()
{
	if (emuTextureId_) {
		glDeleteTextures(1, &emuTextureId_);
		emuTextureId_ = 0;
	}
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();

	if (glContext_) {
		SDL_GL_DestroyContext(glContext_);
		glContext_ = nullptr;
	}
	if (window_) {
		SDL_DestroyWindow(window_);
		window_ = nullptr;
	}
	SDL_Quit();
}

/* ── run loop ────────────────────────────────────────── */

void ImGuiBackend::runLoop()
{
	if (!shell_) return;

	/* In ModelSelector state, shouldQuit() would crash (no machine).
	   Use g_forceMacOff directly when machine isn't inited. */
	auto wantQuit = [this]() -> bool {
		if (shell_->isMachineInited())
			return shell_->shouldQuit();
		return g_requestMacOff;
	};

	while (!wantQuit()) {
		/* 1. Poll SDL events — feed to ImGui first */
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL3_ProcessEvent(&event);

			/* In ModelSelector state, only handle quit events */
			if (uiState_ == UIState::ModelSelector) {
				if (event.type == SDL_EVENT_QUIT)
					g_requestMacOff = true;
				continue;
			}

			if (!imGuiConsumedEvent(event)) {
				/* Intercept Ctrl key for overlay */
				if (event.type == SDL_EVENT_KEY_DOWN &&
					(event.key.scancode == SDL_SCANCODE_LCTRL ||
					 event.key.scancode == SDL_SCANCODE_RCTRL)) {
					overlayVisible_ = true;
					continue; /* don't forward to emulator */
				}
				if (event.type == SDL_EVENT_KEY_UP &&
					(event.key.scancode == SDL_SCANCODE_LCTRL ||
					 event.key.scancode == SDL_SCANCODE_RCTRL)) {
					overlayVisible_ = false;
					continue;
				}
				/* When overlay is visible, don't forward to emulator */
				if (overlayVisible_)
					continue;

				PlatformEvent pe = translateSdlEvent(event);
				if (pe.type != PlatformEvent::Type::None)
					shell_->dispatchEvent(pe);
			}
		}

		/* Branch on UI state */
		switch (uiState_) {
		case UIState::ModelSelector:
		{
			/* No emulation ticks — just draw the selector UI */
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplSDL3_NewFrame();
			ImGui::NewFrame();

			/* drawModelSelector() may set pendingBoot_ — we must
			   finish the current ImGui frame before tearing down
			   the context, so defer the actual boot. */
			drawModelSelector();

			ImGui::Render();
			{
				int displayW, displayH;
				SDL_GetWindowSizeInPixels(window_, &displayW, &displayH);
				glViewport(0, 0, displayW, displayH);
				glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
				glClear(GL_COLOR_BUFFER_BIT);
				ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
				SDL_GL_SwapWindow(window_);
			}

			/* Now it's safe to tear down ImGui and boot */
			if (pendingBoot_) {
				pendingBoot_ = false;
				bootFromSelector(pendingBootConfig_);
			} else {
				SDL_Delay(16); /* ~60 fps for UI */
			}
			break;
		}

		case UIState::Windowed:
		case UIState::Fullscreen:
		case UIState::Developer:
			drawWindowedState();
			break;
		}
	}
}

/* ── Per-state draw paths ────────────────────────────── */

void ImGuiBackend::drawWindowedState()
{
	/* Process saved tasks (disk inserts etc.) */
	shell_->processSavedTasks();
	if (shell_->shouldQuit()) return;

	/* Handle speed-stopped state */
	if (shell_->isSpeedStopped()) {
		SDL_Event waitEvt;
		if (SDL_WaitEvent(&waitEvt)) {
			ImGui_ImplSDL3_ProcessEvent(&waitEvt);
			if (!imGuiConsumedEvent(waitEvt)) {
				PlatformEvent pe = translateSdlEvent(waitEvt);
				if (pe.type != PlatformEvent::Type::None)
					shell_->dispatchEvent(pe);
			}
		}
		return;
	}

	/* Run emulation ticks */
	if (!shell_->tickIsDue()) {
		SDL_Delay(shell_->getDelayMs());
		return;
	}
	while (shell_->tickIsDue() && !shell_->shouldQuit())
		shell_->runOneTick();

	/* Upload emulator framebuffer to GL texture */
	uploadFramebuffer();

	/* ImGui frame */
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	/* Only show menu bar in Developer state */
	if (uiState_ == UIState::Developer)
		drawMenuBar();

	drawEmulatorViewport();

	/* Control overlay (Ctrl key held) */
	if (overlayVisible_) {
		UIState requested = uiState_;
		overlay_.draw(uiState_, shell_, requested);
		if (requested != uiState_) {
			switch (requested) {
			case UIState::Windowed:   enterWindowed(); break;
			case UIState::Fullscreen: enterFullscreen(); break;
			case UIState::Developer:  enterDeveloper(); break;
			default: break;
			}
			overlayVisible_ = false;
		}
	}

	/* Debug tools only in Developer mode */
	if (uiState_ == UIState::Developer)
		toolRegistry_.drawAllVisible();

	ImGui::Render();

	/* Render */
	int displayW, displayH;
	SDL_GetWindowSizeInPixels(window_, &displayW, &displayH);
	glViewport(0, 0, displayW, displayH);
	if (uiState_ == UIState::Fullscreen)
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	else
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	SDL_GL_SwapWindow(window_);
}

void ImGuiBackend::drawFullscreenState()
{
	/* Placeholder — will be implemented in Phase 4 */
	drawWindowedState();
}

void ImGuiBackend::drawDeveloperState()
{
	/* Developer mode: same emulation as windowed but with larger
	   window, menu bar, and visible debug panels.
	   (Full DockSpace requires imgui docking branch — using floating
	    windows for now, which still provides a good developer UX.) */
	drawWindowedState();
}

/* ── Model selector ──────────────────────────────────── */

void ImGuiBackend::drawModelSelector()
{
	ModelSelectorResult result = modelSelector_.draw();
	if (result.accepted && shell_) {
		pendingBoot_ = true;
		pendingBootConfig_ = result.config;
	}
}

void ImGuiBackend::bootFromSelector(const LaunchConfig& config)
{
	/* Update the global config with user's choices */
	SetLaunchConfig(config);

	/* Tear down the selector window — initMachine will create
	   the properly-sized emulation window. */
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
	if (glContext_) {
		SDL_GL_DestroyContext(glContext_);
		glContext_ = nullptr;
	}
	if (window_) {
		SDL_DestroyWindow(window_);
		window_ = nullptr;
	}

	/* Now do the full machine init (ROM, RAM, devices, window) */
	if (!shell_->initMachine()) {
		fprintf(stderr, "Machine init failed for %s\n",
			ModelToString(config.model));
		g_requestMacOff = true;
		return;
	}

	/* Register debug tools now that the machine is initialized */
	RegisterDebugTools(toolRegistry_);

	uiState_ = UIState::Windowed;
}

/* ── State transitions ───────────────────────────────── */

void ImGuiBackend::enterWindowed()
{
	if (uiState_ == UIState::Fullscreen) {
		SDL_SetWindowFullscreen(window_, false);
		if (savedWinW_ > 0 && savedWinH_ > 0) {
			SDL_SetWindowSize(window_, savedWinW_, savedWinH_);
			SDL_SetWindowPosition(window_, savedWinX_, savedWinY_);
		}
	}
	uiState_ = UIState::Windowed;
}

void ImGuiBackend::enterFullscreen()
{
	if (uiState_ != UIState::Fullscreen) {
		SDL_GetWindowPosition(window_, &savedWinX_, &savedWinY_);
		SDL_GetWindowSize(window_, &savedWinW_, &savedWinH_);
		SDL_SetWindowFullscreen(window_, true);
	}
	uiState_ = UIState::Fullscreen;
}

void ImGuiBackend::enterDeveloper()
{
	if (uiState_ != UIState::Developer) {
		SDL_GetWindowPosition(window_, &savedWinX_, &savedWinY_);
		SDL_GetWindowSize(window_, &savedWinW_, &savedWinH_);
		/* Expand to a larger window for developer tools */
		PlatformDisplayBounds bounds;
		int devW = 1400, devH = 900;
		if (getDisplayBounds(&bounds)) {
			devW = (int)(bounds.w * 0.8f);
			devH = (int)(bounds.h * 0.8f);
		}
		SDL_SetWindowSize(window_, devW, devH);
		SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED);
	}
	uiState_ = UIState::Developer;
}

/* ── Selector window (pre-boot) ──────────────────────── */

bool ImGuiBackend::createSelectorWindow()
{
	/* Create a modest window for the model selector, no emulation
	   texture needed yet. */
	Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
		| SDL_WINDOW_HIGH_PIXEL_DENSITY;

	window_ = SDL_CreateWindow("Maxi vMac", 700, 500, flags);
	if (!window_) {
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		return false;
	}

	glContext_ = SDL_GL_CreateContext(window_);
	if (!glContext_) {
		fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
		return false;
	}
	SDL_GL_MakeCurrent(window_, glContext_);
	SDL_GL_SetSwapInterval(1);

	/* Initialize ImGui */
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigWindowsMoveFromTitleBarOnly = true;
	ImGui::StyleColorsDark();

	ImGui_ImplSDL3_InitForOpenGL(window_, glContext_);
	ImGui_ImplOpenGL3_Init("#version 150");

	/* Initialize model selector with ROM discovery */
	const LaunchConfig& lc = GetLaunchConfig();
	modelSelector_.init(lc.romDir);

	return true;
}

/* ── event translation ───────────────────────────────── */

bool ImGuiBackend::imGuiConsumedEvent(const SDL_Event& event) const
{
	ImGuiIO& io = ImGui::GetIO();
	switch (event.type) {
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
			return io.WantCaptureKeyboard;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
		case SDL_EVENT_MOUSE_MOTION:
		case SDL_EVENT_MOUSE_WHEEL:
			/* Always forward mouse events to the emulator.
			   Coordinates are offset by emuViewOrigin so clicks
			   outside the viewport clamp harmlessly.  ImGui still
			   gets the event via ImGui_ImplSDL3_ProcessEvent(). */
			return false;
		default:
			return false;
	}
}

PlatformEvent ImGuiBackend::translateSdlEvent(SDL_Event& event)
{
	PlatformEvent pEvt;

	/* Helper: test if window-space coordinates fall inside the
	   emulator viewport and compute emulator-relative coords. */
	auto mouseInEmuView = [&](float wx, float wy, float &ex, float &ey) -> bool {
		ex = wx - emuViewOriginX_;
		ey = wy - emuViewOriginY_;
		return ex >= 0 && ey >= 0 && ex < emuTexW_ && ey < emuTexH_;
	};

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
		case SDL_EVENT_MOUSE_MOTION: {
			if (relativeMouseMode_) {
				pEvt.type = PlatformEvent::Type::MouseMove;
				pEvt.isRelative = true;
				pEvt.dx = event.motion.xrel;
				pEvt.dy = event.motion.yrel;
			} else {
				bool inside = mouseInEmuView(event.motion.x, event.motion.y,
					pEvt.x, pEvt.y);
				if (inside) {
					pEvt.type = PlatformEvent::Type::MouseMove;
					SDL_HideCursor();
				} else {
					SDL_ShowCursor();
				}
			}
			break;
		}
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			if (relativeMouseMode_) {
				pEvt.type = PlatformEvent::Type::MouseButtonDown;
				pEvt.isRelative = true;
			} else if (mouseInEmuView(event.button.x, event.button.y,
					pEvt.x, pEvt.y)) {
				pEvt.type = PlatformEvent::Type::MouseButtonDown;
			}
			break;
		case SDL_EVENT_MOUSE_BUTTON_UP:
			if (relativeMouseMode_) {
				pEvt.type = PlatformEvent::Type::MouseButtonUp;
				pEvt.isRelative = true;
			} else if (mouseInEmuView(event.button.x, event.button.y,
					pEvt.x, pEvt.y)) {
				pEvt.type = PlatformEvent::Type::MouseButtonUp;
			}
			break;
		case SDL_EVENT_KEY_DOWN: {
			uint8_t mkc = SDLScan2MacKeyCode(event.key.scancode);
			if (mkc != 0xFF) {
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

/* ── GL texture upload ───────────────────────────────── */

void ImGuiBackend::uploadFramebuffer()
{
	if (!shell_ || !shell_->isFramebufferDirty()) return;

	glBindTexture(GL_TEXTURE_2D, emuTextureId_);
	/* GL_LINEAR, not GL_NEAREST.  On macOS Retina the emulator texture
	   is displayed at 1× logical size but the GL framebuffer is 2×
	   physical pixels (SDL_WINDOW_HIGH_PIXEL_DENSITY).  With GL_NEAREST,
	   alternating black/white checkerboard pixels (the Mac Plus desktop
	   pattern) produce a visible moiré that is anchored to the physical
	   display — moving the window doesn't move the pattern.  The exact
	   cause is unclear: possibly the macOS compositor or the OpenGL→Metal
	   translation layer resamples the backing store at sub-pixel offsets.
	   GL_LINEAR blends adjacent texels into uniform gray, matching the
	   SDL backend's appearance.  The trade-off is slightly softer edges
	   on fine pixel-art details. */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
		emuTexW_, emuTexH_,
		GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
		shell_->getFramebuffer());
	shell_->clearDirtyFlag();
}

/* ── ImGui drawing ───────────────────────────────────── */

void ImGuiBackend::drawMenuBar()
{
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Quit", "Ctrl+Q"))
				g_requestMacOff = true;
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Machine")) {
			if (ImGui::MenuItem("Windowed"))
				enterWindowed();
			if (ImGui::MenuItem("Fullscreen"))
				enterFullscreen();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Debug")) {
			toolRegistry_.drawToolMenu();
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}

void ImGuiBackend::drawEmulatorViewport()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

	if (uiState_ == UIState::Fullscreen) {
		/* Fullscreen: fill the display, centered, aspect-preserving */
		ImVec2 displaySize = ImGui::GetIO().DisplaySize;
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(displaySize);
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
			| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings
			| ImGuiWindowFlags_NoBringToFrontOnFocus;
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1));
		if (ImGui::Begin("##FullscreenViewport", nullptr, flags)) {
			/* Compute scaled size preserving aspect ratio */
			float emuAspect = (float)emuTexW_ / (float)emuTexH_;
			float dispAspect = displaySize.x / displaySize.y;
			float scaledW, scaledH;
			if (emuAspect > dispAspect) {
				scaledW = displaySize.x;
				scaledH = displaySize.x / emuAspect;
			} else {
				scaledH = displaySize.y;
				scaledW = displaySize.y * emuAspect;
			}
			/* Try integer scaling if close enough */
			int intScale = (int)(scaledW / emuTexW_);
			if (intScale >= 1) {
				float intW = emuTexW_ * intScale;
				float intH = emuTexH_ * intScale;
				if (intW <= displaySize.x && intH <= displaySize.y) {
					scaledW = intW;
					scaledH = intH;
				}
			}
			/* Center */
			float offsetX = (displaySize.x - scaledW) * 0.5f;
			float offsetY = (displaySize.y - scaledH) * 0.5f;
			ImGui::SetCursorPos(ImVec2(offsetX, offsetY));
			ImVec2 pos = ImGui::GetCursorScreenPos();
			emuViewOriginX_ = pos.x;
			emuViewOriginY_ = pos.y;
			ImGui::Image((ImTextureID)(intptr_t)emuTextureId_,
				ImVec2(scaledW, scaledH));
		}
		ImGui::End();
		ImGui::PopStyleColor();
	} else {
		/* Windowed / Developer: fixed-size viewport */
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_AlwaysAutoResize
			| ImGuiWindowFlags_NoScrollbar;
		if (uiState_ == UIState::Windowed) {
			/* In windowed mode, no title bar either */
			flags |= ImGuiWindowFlags_NoTitleBar;
		}
		if (ImGui::Begin("Macintosh", nullptr, flags)) {
			/* Snap the image origin to physical pixel boundaries */
			ImVec2 pos = ImGui::GetCursorScreenPos();
			float scale = ImGui::GetIO().DisplayFramebufferScale.x;
			pos.x = floorf(pos.x * scale) / scale;
			pos.y = floorf(pos.y * scale) / scale;
			ImGui::SetCursorScreenPos(pos);
			emuViewOriginX_ = pos.x;
			emuViewOriginY_ = pos.y;
			ImVec2 size((float)emuTexW_, (float)emuTexH_);
			ImGui::Image((ImTextureID)(intptr_t)emuTextureId_, size);
		}
		ImGui::End();
	}

	ImGui::PopStyleVar(2);
}

/* ── Window ──────────────────────────────────────────── */

bool ImGuiBackend::createWindow(const char* title,
	int width, int height, bool fullscreen)
{
	/* Size the window to fit the emulator display plus ImGui chrome.
	   The incoming width/height may already be magnified (e.g. 2x),
	   so just add modest extra space rather than doubling. */
	int winW = width + 200;
	int winH = height + 200;

	Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
		| SDL_WINDOW_HIGH_PIXEL_DENSITY;
	if (fullscreen) flags |= SDL_WINDOW_FULLSCREEN;

	window_ = SDL_CreateWindow(title, winW, winH, flags);
	if (!window_) {
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		return false;
	}

	glContext_ = SDL_GL_CreateContext(window_);
	if (!glContext_) {
		fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
		return false;
	}
	SDL_GL_MakeCurrent(window_, glContext_);
	SDL_GL_SetSwapInterval(1);

	/* Initialize ImGui */
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigWindowsMoveFromTitleBarOnly = true;
	ImGui::StyleColorsDark();

	ImGui_ImplSDL3_InitForOpenGL(window_, glContext_);
	ImGui_ImplOpenGL3_Init("#version 150");

	/* Create GL texture for emulator framebuffer.
	   Use actual emulator resolution (g_screenWidth/Height), not the
	   window size which may include magnification scaling. */
	emuTexW_ = g_screenWidth;
	emuTexH_ = g_screenHeight;
	glGenTextures(1, &emuTextureId_);
	glBindTexture(GL_TEXTURE_2D, emuTextureId_);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, emuTexW_, emuTexH_, 0,
		GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, nullptr);

	return true;
}

void ImGuiBackend::destroyWindow()
{
	if (emuTextureId_) {
		glDeleteTextures(1, &emuTextureId_);
		emuTextureId_ = 0;
	}
	if (glContext_) {
		SDL_GL_DestroyContext(glContext_);
		glContext_ = nullptr;
	}
	if (window_) {
		SDL_DestroyWindow(window_);
		window_ = nullptr;
	}
}

bool ImGuiBackend::recreateWindow(const char* title,
	int width, int height, bool fullscreen)
{
	destroyWindow();
	return createWindow(title, width, height, fullscreen);
}

void ImGuiBackend::getWindowSize(int* w, int* h)
{
	if (window_) SDL_GetWindowSize(window_, w, h);
	else { *w = 0; *h = 0; }
}

void ImGuiBackend::getWindowPosition(int* x, int* y)
{
	if (window_) SDL_GetWindowPosition(window_, x, y);
	else { *x = 0; *y = 0; }
}

void ImGuiBackend::setWindowPosition(int x, int y)
{
	if (window_) SDL_SetWindowPosition(window_, x, y);
}

void ImGuiBackend::setFullscreen(bool fullscreen)
{
	if (window_)
		SDL_SetWindowFullscreen(window_, fullscreen);
}

void ImGuiBackend::clearScreen()
{
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	if (window_) SDL_GL_SwapWindow(window_);
}

/* ── Cursor ──────────────────────────────────────────── */

void ImGuiBackend::showCursor() { SDL_ShowCursor(); }
void ImGuiBackend::hideCursor() { SDL_HideCursor(); }

void ImGuiBackend::setMouseGrab(bool grab)
{
	if (window_) {
		SDL_SetWindowMouseGrab(window_, grab);
		SDL_SetWindowRelativeMouseMode(window_, grab);
		relativeMouseMode_ = grab;
	}
}

/* ── Audio ───────────────────────────────────────────── */

bool ImGuiBackend::audioInit()    { return Sound_Init(); }
void ImGuiBackend::audioStart()   { Sound_Start(); }
void ImGuiBackend::audioStop()    { Sound_Stop(); }
void ImGuiBackend::audioShutdown() { Sound_UnInit(); }

/* ── Keyboard ────────────────────────────────────────── */

void ImGuiBackend::disableKeyRepeat() { DisableKeyRepeat(); }
void ImGuiBackend::restoreKeyRepeat() { RestoreKeyRepeat(); }

/* ── Dialog ──────────────────────────────────────────── */

void ImGuiBackend::showMessageBox(const char* title, const char* message)
{
	SDL_ShowSimpleMessageBox(0, title, message, window_);
}

bool ImGuiBackend::getDisplayBounds(PlatformDisplayBounds* bounds)
{
	SDL_DisplayID did = SDL_GetPrimaryDisplay();
	if (did == 0) return false;
	const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(did);
	if (!mode) return false;
	bounds->w = mode->w;
	bounds->h = mode->h;
	return true;
}

/* ── Paths ───────────────────────────────────────────── */

const char* ImGuiBackend::getAppParent()
{
	return SDL_GetBasePath();
}

char* ImGuiBackend::getPrefDir(const char* org, const char* app)
{
	return SDL_GetPrefPath(org, app);
}

void ImGuiBackend::freePath(void* path)
{
	SDL_free(path);
}
