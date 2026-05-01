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
#include "core/main.h"

/* Forward declarations to avoid pulling in the full osglu_common.h
   include chain (which depends on emulator config macros). */
extern void InitKeyCodes();
extern bool g_requestMacOff;
extern bool g_speedStopped;

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
#include <array>
#include <vector>

#include "platform/clipboard_image.h"
#include "stb_image_write.h"

/* ── Shortcut table ──────────────────────────────────── */

struct ShortcutEntry
{
	SDL_Scancode scancode;
	UIAction action;
};

static constexpr std::array kShortcuts = {
	ShortcutEntry{SDL_SCANCODE_F, UIAction::ToggleFullscreen},
	ShortcutEntry{SDL_SCANCODE_M, UIAction::ToggleScaling},
	ShortcutEntry{SDL_SCANCODE_Z, UIAction::Zoom},
	ShortcutEntry{SDL_SCANCODE_S, UIAction::Screenshot},
	ShortcutEntry{SDL_SCANCODE_RIGHT, UIAction::SpeedUp},
	ShortcutEntry{SDL_SCANCODE_LEFT, UIAction::SpeedDown},
	ShortcutEntry{SDL_SCANCODE_0, UIAction::SpeedReset},
	ShortcutEntry{SDL_SCANCODE_P, UIAction::TogglePaused},
	ShortcutEntry{SDL_SCANCODE_I, UIAction::InsertDisk},
	ShortcutEntry{SDL_SCANCODE_R, UIAction::Reboot},
};

/* ── Speed presets ───────────────────────────────────── */

static constexpr uint8_t kSpeedPresets[] = {1, 2, 4, 8, 16, 32, 0};
static constexpr int kSpeedPresetCount = 7;

/* ── init / shutdown ─────────────────────────────────── */

bool ImGuiBackend::init(EmulatorShell *shell)
{
	shell_ = shell;

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
	{
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return false;
	}

	/* Request OpenGL 3.2 Core (minimum for ImGui) */
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	InitKeyCodes();
	return true;
}

void ImGuiBackend::shutdown()
{
	if (emuTextureId_)
	{
		glDeleteTextures(1, &emuTextureId_);
		emuTextureId_ = 0;
	}
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();

	if (glContext_)
	{
		SDL_GL_DestroyContext(glContext_);
		glContext_ = nullptr;
	}
	if (window_)
	{
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
	auto wantQuit = [this]() -> bool
	{
		if (shell_->isMachineInited()) return shell_->shouldQuit();
		return g_requestMacOff;
	};

	while (!wantQuit())
	{
		/* PeekPending → Peek timer transition */
		if (overlayMode_ == OverlayMode::PeekPending &&
			(SDL_GetTicks() - ctrlDownTick_) >= kPeekThresholdMs)
		{
			overlayMode_ = OverlayMode::Peek;
		}

		/* 1. Poll SDL events — feed to ImGui first */
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL3_ProcessEvent(&event);

			/* In ModelSelector state, only handle quit events */
			if (uiState_ == UIState::ModelSelector)
			{
				if (event.type == SDL_EVENT_QUIT) g_requestMacOff = true;
				continue;
			}

			/* --- Ctrl key for overlay state machine --- */
			if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
				(event.key.scancode == SDL_SCANCODE_LCTRL ||
				 event.key.scancode == SDL_SCANCODE_RCTRL))
			{
				switch (overlayMode_)
				{
					case OverlayMode::Hidden:
						overlayMode_ = OverlayMode::PeekPending;
						ctrlDownTick_ = SDL_GetTicks();
						shell_->forceShowCursor();
						break;
					case OverlayMode::Sticky:
						overlayMode_ = OverlayMode::Hidden;
						break;
					default:
						break;
				}
				continue;
			}
			if (event.type == SDL_EVENT_KEY_UP && (event.key.scancode == SDL_SCANCODE_LCTRL ||
												   event.key.scancode == SDL_SCANCODE_RCTRL))
			{
				switch (overlayMode_)
				{
					case OverlayMode::PeekPending:
						overlayMode_ = OverlayMode::Sticky; // tap → sticky
						break;
					case OverlayMode::Peek:
						overlayMode_ = OverlayMode::Hidden; // hold released → dismiss
						break;
					default:
						break;
				}
				continue;
			}
			/* Escape dismisses all overlay modes */
			if (overlayMode_ != OverlayMode::Hidden && event.type == SDL_EVENT_KEY_DOWN &&
				event.key.scancode == SDL_SCANCODE_ESCAPE)
			{
				overlayMode_ = OverlayMode::Hidden;
				continue;
			}
			/* Suppress Ctrl+Click → right-click while overlay visible */
			if (overlayMode_ != OverlayMode::Hidden && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
				event.button.button == SDL_BUTTON_RIGHT)
			{
				continue;
			}

			/* Shortcut dispatch while overlay is visible.
			   Works with or without Ctrl held (so bare keys work in sticky mode). */
			if (overlayMode_ != OverlayMode::Hidden && event.type == SDL_EVENT_KEY_DOWN &&
				!event.key.repeat && event.key.scancode != SDL_SCANCODE_LCTRL &&
				event.key.scancode != SDL_SCANCODE_RCTRL)
			{
				UIAction action = UIAction::None;
				for (const auto &s : kShortcuts)
				{
					if (event.key.scancode == s.scancode)
					{
						action = s.action;
						break;
					}
				}
				if (action != UIAction::None)
				{
					executeAction(action);
					continue;
				}
			}

			/* Integer-snap resize logic (skip when OS-maximized) */
			if (event.type == SDL_EVENT_WINDOW_RESIZED && !snapping_ &&
				!(SDL_GetWindowFlags(window_) & SDL_WINDOW_MAXIMIZED))
			{
				if (uiState_ == UIState::Windowed && scalingMode_ == ScalingMode::PixelPerfect)
				{
					int newW = event.window.data1;
					int newH = event.window.data2;
					int scaleX = std::max(1, (newW + emuTexW_ / 2) / emuTexW_);
					int scaleY = std::max(1, (newH + emuTexH_ / 2) / emuTexH_);
					int scale = std::min(scaleX, scaleY);
					int snapW = emuTexW_ * scale;
					int snapH = emuTexH_ * scale;
					if (snapW != newW || snapH != newH)
					{
						snapping_ = true;
						SDL_SetWindowSize(window_, snapW, snapH);
						snapping_ = false;
					}
					currentScale_ = scale;
				}
				continue;
			}

			if (!imGuiConsumedEvent(event))
			{
				/* When overlay is visible, don't forward to emulator */
				if (overlayMode_ != OverlayMode::Hidden) continue;

				PlatformEvent pe = translateSdlEvent(event);
				if (pe.type != PlatformEvent::Type::None) shell_->dispatchEvent(pe);
			}
		}

		/* Branch on UI state */
		switch (uiState_)
		{
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
					glClearColor(0.78f, 0.78f, 0.78f, 1.0f);
					glClear(GL_COLOR_BUFFER_BIT);
					ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
					SDL_GL_SwapWindow(window_);
				}

				/* Now it's safe to tear down ImGui and boot */
				if (pendingBoot_)
				{
					pendingBoot_ = false;
					bootFromSelector(pendingBootConfig_);
				}
				else
				{
					SDL_Delay(16); /* ~60 fps for UI */
				}
				break;
			}

			case UIState::Windowed:
			case UIState::Fullscreen:
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
	if (overlayMode_ != OverlayMode::Hidden) shell_->forceShowCursor();
	if (shell_->shouldQuit()) return;

	/* Handle speed-stopped state */
	if (shell_->isSpeedStopped())
	{
		SDL_Event waitEvt;
		if (SDL_WaitEvent(&waitEvt))
		{
			ImGui_ImplSDL3_ProcessEvent(&waitEvt);
			if (!imGuiConsumedEvent(waitEvt))
			{
				PlatformEvent pe = translateSdlEvent(waitEvt);
				if (pe.type != PlatformEvent::Type::None) shell_->dispatchEvent(pe);
			}
		}
		return;
	}

	/* Run emulation ticks */
	if (!shell_->tickIsDue())
	{
		SDL_Delay(shell_->getDelayMs());
		return;
	}
	if (shell_->tickIsDue() && !shell_->shouldQuit()) shell_->runOneTick();

	/* Upload emulator framebuffer to GL texture */
	uploadFramebuffer();

	/* ImGui frame */
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	drawEmulatorViewport();

	/* Control overlay */
	if (overlayMode_ != OverlayMode::Hidden)
	{
		UIState requested = uiState_;
		overlay_.draw(uiState_, shell_, this, requested);
		if (requested != uiState_)
		{
			switch (requested)
			{
				case UIState::Windowed:
					enterWindowed();
					break;
				case UIState::Fullscreen:
					enterFullscreen();
					break;
				default:
					break;
			}
			overlayMode_ = OverlayMode::Hidden;
		}
	}

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

/* ── Model selector ──────────────────────────────────── */

void ImGuiBackend::drawModelSelector()
{
	ModelSelectorResult result = modelSelector_.draw();
	if (result.accepted && shell_)
	{
		pendingBoot_ = true;
		pendingBootConfig_ = result.config;
	}
}

void ImGuiBackend::bootFromSelector(const LaunchConfig &config)
{
	/* Update the global config with user's choices */
	SetLaunchConfig(config);

	/* Tear down the selector window — initMachine will create
	   the properly-sized emulation window. */
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
	if (glContext_)
	{
		SDL_GL_DestroyContext(glContext_);
		glContext_ = nullptr;
	}
	if (window_)
	{
		SDL_DestroyWindow(window_);
		window_ = nullptr;
	}

	/* Now do the full machine init (ROM, RAM, devices, window) */
	if (!shell_->initMachine())
	{
		fprintf(stderr, "Rig init failed for %s\n", ModelToString(config.model));
		g_requestMacOff = true;
		return;
	}

	uiState_ = UIState::Windowed;
}

/* ── State transitions ───────────────────────────────── */

void ImGuiBackend::enterWindowed()
{
	if (uiState_ == UIState::Fullscreen)
	{
		SDL_SetWindowFullscreen(window_, false);
		if (savedWinW_ > 0 && savedWinH_ > 0)
		{
			SDL_SetWindowSize(window_, savedWinW_, savedWinH_);
			SDL_SetWindowPosition(window_, savedWinX_, savedWinY_);
		}
	}
	if (shell_) shell_->setFullscreenHint(false);
	uiState_ = UIState::Windowed;
}

void ImGuiBackend::enterFullscreen()
{
	if (uiState_ != UIState::Fullscreen)
	{
		SDL_GetWindowPosition(window_, &savedWinX_, &savedWinY_);
		SDL_GetWindowSize(window_, &savedWinW_, &savedWinH_);
		SDL_SetWindowFullscreen(window_, true);
	}
	if (shell_) shell_->setFullscreenHint(true);
	uiState_ = UIState::Fullscreen;
}

/* ── Selector window (pre-boot) ──────────────────────── */

bool ImGuiBackend::createSelectorWindow()
{
	/* Create a modest window for the model selector, no emulation
	   texture needed yet. */
	Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY;

	window_ = SDL_CreateWindow("Maxi vMac", 700, 500, flags);
	if (!window_)
	{
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		return false;
	}

	glContext_ = SDL_GL_CreateContext(window_);
	if (!glContext_)
	{
		fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
		return false;
	}
	SDL_GL_MakeCurrent(window_, glContext_);
	SDL_GL_SetSwapInterval(1);

	/* Initialize ImGui */
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigWindowsMoveFromTitleBarOnly = true;
	ImGui::StyleColorsDark();

	ImGui_ImplSDL3_InitForOpenGL(window_, glContext_);
	ImGui_ImplOpenGL3_Init("#version 150");

	/* Initialize model selector with ROM discovery */
	const LaunchConfig &lc = GetLaunchConfig();
	modelSelector_.init(lc.romDir);

	return true;
}

/* ── event translation ───────────────────────────────── */

bool ImGuiBackend::imGuiConsumedEvent(const SDL_Event &event) const
{
	switch (event.type)
	{
		case SDL_EVENT_MOUSE_MOTION:
			/* Mouse position is always forwarded to the emulator so
			   the guest cursor tracks the host even when an ImGui
			   window is on top.  translateSdlEvent sets positionOnly
			   when the viewport is not hovered. */
			return false;
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
		case SDL_EVENT_MOUSE_WHEEL:
			/* Forward to the emulator only when the emulator viewport
			   is hovered (from the previous frame).  In Windowed the
			   viewport fills the window, so this is always true.
			   The overlay is the only UI that can appear on top. */
			return !emuViewportHovered_;
		default:
			return false;
	}
}

PlatformEvent ImGuiBackend::translateSdlEvent(SDL_Event &event)
{
	PlatformEvent pEvt;

	/* Helper: test if window-space coordinates fall inside the
	   emulator viewport and compute emulator-pixel coords.
	   When the image is scaled (e.g. 2× in fullscreen), map from
	   display-pixel space back to emulator-pixel space so the
	   shell receives coordinates in [0, emuTexW_) × [0, emuTexH_). */
	auto mouseInEmuView = [&](float wx, float wy, float &ex, float &ey) -> bool
	{
		float relX = wx - emuViewOriginX_;
		float relY = wy - emuViewOriginY_;
		if (emuViewW_ > 0 && emuViewH_ > 0)
		{
			ex = relX * emuTexW_ / emuViewW_;
			ey = relY * emuTexH_ / emuViewH_;
		}
		else
		{
			ex = relX;
			ey = relY;
		}
		return ex >= 0 && ey >= 0 && ex < emuTexW_ && ey < emuTexH_;
	};

	switch (event.type)
	{
		case SDL_EVENT_QUIT:
			/* Window close button quits. Cmd+Q is not routed here
			   (intercepted by macOS menu handling). */
			g_requestMacOff = true;
			break;
		case SDL_EVENT_WINDOW_FOCUS_GAINED:
			pEvt.type = PlatformEvent::Type::FocusGained;
			break;
		case SDL_EVENT_WINDOW_FOCUS_LOST:
			SDL_CaptureMouse(false);
			pEvt.type = PlatformEvent::Type::FocusLost;
			break;
		case SDL_EVENT_WINDOW_MOUSE_ENTER:
			pEvt.type = PlatformEvent::Type::MouseEnter;
			break;
		case SDL_EVENT_WINDOW_MOUSE_LEAVE:
			pEvt.type = PlatformEvent::Type::MouseLeave;
			showCursor();
			break;
		case SDL_EVENT_WINDOW_RESIZED:
			pEvt.type = PlatformEvent::Type::WindowResized;
			break;
		case SDL_EVENT_MOUSE_MOTION:
		{
			if (relativeMouseMode_)
			{
				pEvt.type = PlatformEvent::Type::MouseMove;
				pEvt.isRelative = true;
				pEvt.dx = event.motion.xrel;
				pEvt.dy = event.motion.yrel;
			}
			else if (overlayMode_ == OverlayMode::Hidden)
			{
				bool inView = mouseInEmuView(event.motion.x, event.motion.y, pEvt.x, pEvt.y);
				if (emuViewportHovered_)
				{
					/* Viewport is topmost — normal cursor-hiding path. */
					pEvt.type = PlatformEvent::Type::MouseMove;
				}
				else if (inView)
				{
					/* Mouse is over the guest area but an ImGui window
					   is on top: forward position so the guest cursor
					   tracks the host, but keep the host cursor visible
					   for the overlapping UI. */
					pEvt.type = PlatformEvent::Type::MouseMove;
					pEvt.positionOnly = true;
				}
				/* Otherwise mouse is outside the guest area entirely
				   (e.g. in a debug panel) — don't update the guest. */
			}
			break;
		}
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			SDL_CaptureMouse(true);
			if (relativeMouseMode_)
			{
				pEvt.type = PlatformEvent::Type::MouseButtonDown;
				pEvt.isRelative = true;
			}
			else if (mouseInEmuView(event.button.x, event.button.y, pEvt.x, pEvt.y))
			{
				pEvt.type = PlatformEvent::Type::MouseButtonDown;
			}
			break;
		case SDL_EVENT_MOUSE_BUTTON_UP:
			SDL_CaptureMouse(false);
			if (relativeMouseMode_)
			{
				pEvt.type = PlatformEvent::Type::MouseButtonUp;
				pEvt.isRelative = true;
			}
			else if (mouseInEmuView(event.button.x, event.button.y, pEvt.x, pEvt.y))
			{
				pEvt.type = PlatformEvent::Type::MouseButtonUp;
			}
			break;
		case SDL_EVENT_KEY_DOWN:
		{
			uint8_t mkc = SDLScan2MacKeyCode(event.key.scancode);
			if (mkc != 0xFF)
			{
				pEvt.type = PlatformEvent::Type::KeyDown;
				pEvt.macKeyCode = mkc;
			}
			break;
		}
		case SDL_EVENT_KEY_UP:
		{
			uint8_t mkc = SDLScan2MacKeyCode(event.key.scancode);
			if (mkc != 0xFF)
			{
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

void ImGuiBackend::setTextureFilter(TextureFilter f)
{
	if (textureFilter_ == f) return;
	textureFilter_ = f;
	if (emuTextureId_)
	{
		GLenum glFilter = (f == TextureFilter::Nearest) ? GL_NEAREST : GL_LINEAR;
		glBindTexture(GL_TEXTURE_2D, emuTextureId_);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glFilter);
	}
}

void ImGuiBackend::setScalingMode(ScalingMode m)
{
	if (scalingMode_ == m) return;
	scalingMode_ = m;
	if (m == ScalingMode::PixelPerfect && uiState_ == UIState::Windowed)
	{
		int w, h;
		SDL_GetWindowSize(window_, &w, &h);
		int scaleX = std::max(1, (w + emuTexW_ / 2) / emuTexW_);
		int scaleY = std::max(1, (h + emuTexH_ / 2) / emuTexH_);
		int scale = std::min(scaleX, scaleY);
		snapping_ = true;
		SDL_SetWindowSize(window_, emuTexW_ * scale, emuTexH_ * scale);
		snapping_ = false;
		currentScale_ = scale;
	}
}

/* ── Action dispatch ─────────────────────────────────── */

void ImGuiBackend::executeAction(UIAction action)
{
	switch (action)
	{
		case UIAction::ToggleFullscreen:
			if (uiState_ == UIState::Fullscreen)
				enterWindowed();
			else
				enterFullscreen();
			overlayMode_ = OverlayMode::Hidden;
			break;
		case UIAction::ToggleScaling:
			setScalingMode(scalingMode_ == ScalingMode::PixelPerfect ? ScalingMode::Stretched
																	 : ScalingMode::PixelPerfect);
			break;
		case UIAction::Zoom:
			toggleZoom();
			break;
		case UIAction::Screenshot:
			captureScreenshot();
			break;
		case UIAction::SpeedUp:
			adjustSpeed(+1);
			break;
		case UIAction::SpeedDown:
			adjustSpeed(-1);
			break;
		case UIAction::SpeedReset:
			setSpeed(0);
			break;
		case UIAction::TogglePaused:
			g_speedStopped = !g_speedStopped;
			break;
		case UIAction::InsertDisk:
			openFileDialog();
			overlayMode_ = OverlayMode::Hidden;
			break;
		case UIAction::Reboot:
			g_wantMacReset = true;
			overlayMode_ = OverlayMode::Hidden;
			break;
		default:
			break;
	}
}

void ImGuiBackend::adjustSpeed(int delta)
{
	int idx = 0;
	for (int i = 0; i < kSpeedPresetCount; ++i)
	{
		if (kSpeedPresets[i] == g_speedValue)
		{
			idx = i;
			break;
		}
	}
	idx = std::clamp(idx + delta, 0, kSpeedPresetCount - 1);
	g_speedValue = kSpeedPresets[idx];
}

void ImGuiBackend::setSpeed(int idx)
{
	idx = std::clamp(idx, 0, kSpeedPresetCount - 1);
	g_speedValue = kSpeedPresets[idx];
}

static void pngWriteCallback(void *context, void *data, int size)
{
	auto *buf = static_cast<std::vector<uint8_t> *>(context);
	auto *bytes = static_cast<const uint8_t *>(data);
	buf->insert(buf->end(), bytes, bytes + size);
}

void ImGuiBackend::captureScreenshot()
{
	if (!shell_ || !shell_->getFramebuffer()) return;

	int w = emuTexW_;
	int h = emuTexH_;
	const uint32_t *src = reinterpret_cast<const uint32_t *>(shell_->getFramebuffer());

	/* BGRA → RGBA swizzle */
	std::vector<uint8_t> rgba(w * h * 4);
	for (int i = 0; i < w * h; ++i)
	{
		uint32_t px = src[i];
		rgba[i * 4 + 0] = (px >> 16) & 0xFF; // R
		rgba[i * 4 + 1] = (px >> 8) & 0xFF;	 // G
		rgba[i * 4 + 2] = (px >> 0) & 0xFF;	 // B
		rgba[i * 4 + 3] = 0xFF;				 // A
	}

	std::vector<uint8_t> pngBuf;
	stbi_write_png_to_func(pngWriteCallback, &pngBuf, w, h, 4, rgba.data(), w * 4);

	if (!pngBuf.empty())
	{
		HostClipSetImage(pngBuf.data(), pngBuf.size());
	}
}

static void fileDialogCallback(void *userdata, const char *const *filelist, int filter)
{
	(void)filter;
	auto *backend = static_cast<ImGuiBackend *>(userdata);
	if (filelist && filelist[0])
	{
		backend->shell()->insertDiskOrRom(filelist[0], false);
	}
}

void ImGuiBackend::openFileDialog()
{
	static const SDL_DialogFileFilter filters[] = {
		{"Disk Images", "dsk;img;hfs;dmg;iso;image;dc42"},
		{"All Files", "*"},
	};
	SDL_ShowOpenFileDialog(fileDialogCallback, this, window_, filters, 2, nullptr, false);
}

void ImGuiBackend::uploadFramebuffer()
{
	if (!shell_ || !shell_->isFramebufferDirty()) return;

	glBindTexture(GL_TEXTURE_2D, emuTextureId_);
	GLenum glFilter = (textureFilter_ == TextureFilter::Nearest) ? GL_NEAREST : GL_LINEAR;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glFilter);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, emuTexW_, emuTexH_, GL_BGRA,
					GL_UNSIGNED_INT_8_8_8_8_REV, shell_->getFramebuffer());
	shell_->clearDirtyFlag();
}

/* ── ImGui drawing ───────────────────────────────────── */

void ImGuiBackend::displayEmulatorImage(float w, float h)
{
	ImVec2 pos = ImGui::GetCursorScreenPos();
	emuViewOriginX_ = pos.x;
	emuViewOriginY_ = pos.y;
	emuViewW_ = w;
	emuViewH_ = h;
	ImGui::Image((ImTextureID)(intptr_t)emuTextureId_, ImVec2(w, h));
}

void ImGuiBackend::drawViewportWindowed()
{
	ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(displaySize);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
							 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
							 ImGuiWindowFlags_NoBringToFrontOnFocus |
							 ImGuiWindowFlags_NoSavedSettings;
	emuViewportHovered_ = false;

	/* Both modes need black background when window exceeds content */
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

	if (ImGui::Begin("Macintosh", nullptr, flags))
	{
		emuViewportHovered_ = ImGui::IsWindowHovered();
		if (scalingMode_ == ScalingMode::PixelPerfect)
		{
			/* Largest integer scale that fits the window */
			int scaleX = std::max(1, static_cast<int>(displaySize.x) / emuTexW_);
			int scaleY = std::max(1, static_cast<int>(displaySize.y) / emuTexH_);
			int scale = std::min(scaleX, scaleY);
			float viewW = static_cast<float>(emuTexW_ * scale);
			float viewH = static_cast<float>(emuTexH_ * scale);
			float offsetX = (displaySize.x - viewW) * 0.5f;
			float offsetY = (displaySize.y - viewH) * 0.5f;
			ImGui::SetCursorPos(ImVec2(offsetX, offsetY));
			displayEmulatorImage(viewW, viewH);
		}
		else
		{
			float emuAspect = static_cast<float>(emuTexW_) / emuTexH_;
			float winAspect = displaySize.x / displaySize.y;
			float viewW, viewH;
			if (emuAspect > winAspect)
			{
				viewW = displaySize.x;
				viewH = displaySize.x / emuAspect;
			}
			else
			{
				viewH = displaySize.y;
				viewW = displaySize.y * emuAspect;
			}
			float offsetX = (displaySize.x - viewW) * 0.5f;
			float offsetY = (displaySize.y - viewH) * 0.5f;
			ImGui::SetCursorPos(ImVec2(offsetX, offsetY));
			displayEmulatorImage(viewW, viewH);
		}
	}
	ImGui::End();
	ImGui::PopStyleColor();
}

void ImGuiBackend::drawViewportFullscreen()
{
	ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(displaySize);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
							 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
							 ImGuiWindowFlags_NoSavedSettings |
							 ImGuiWindowFlags_NoBringToFrontOnFocus;
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.102f, 0.102f, 0.102f, 1.0f));
	emuViewportHovered_ = false;
	if (ImGui::Begin("##FullscreenViewport", nullptr, flags))
	{
		emuViewportHovered_ = ImGui::IsWindowHovered();
		float emuAspect = (float)emuTexW_ / (float)emuTexH_;
		float dispAspect = displaySize.x / displaySize.y;
		float scaledW, scaledH;
		if (emuAspect > dispAspect)
		{
			scaledW = displaySize.x;
			scaledH = displaySize.x / emuAspect;
		}
		else
		{
			scaledH = displaySize.y;
			scaledW = displaySize.y * emuAspect;
		}

		if (scalingMode_ == ScalingMode::PixelPerfect)
		{
			int intScale = static_cast<int>(scaledW / emuTexW_);
			if (intScale >= 1)
			{
				float intW = emuTexW_ * intScale;
				float intH = emuTexH_ * intScale;
				if (intW <= displaySize.x && intH <= displaySize.y)
				{
					scaledW = intW;
					scaledH = intH;
				}
			}
		}

		float offsetX = (displaySize.x - scaledW) * 0.5f;
		float offsetY = (displaySize.y - scaledH) * 0.5f;
		ImGui::SetCursorPos(ImVec2(offsetX, offsetY));
		displayEmulatorImage(scaledW, scaledH);
	}
	ImGui::End();
	ImGui::PopStyleColor();
}

void ImGuiBackend::drawEmulatorViewport()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

	switch (uiState_)
	{
		case UIState::Fullscreen:
			drawViewportFullscreen();
			break;
		default:
			drawViewportWindowed();
			break;
	}

	ImGui::PopStyleVar(2);
}

/* ── Window ──────────────────────────────────────────── */

bool ImGuiBackend::createWindow(const char *title, int width, int height, bool fullscreen)
{
	int winW = width;
	int winH = height;

	Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE;
	if (fullscreen) flags |= SDL_WINDOW_FULLSCREEN;

	window_ = SDL_CreateWindow(title, winW, winH, flags);
	if (!window_)
	{
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		return false;
	}

	glContext_ = SDL_GL_CreateContext(window_);
	if (!glContext_)
	{
		fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
		return false;
	}
	SDL_GL_MakeCurrent(window_, glContext_);
	SDL_GL_SetSwapInterval(1);

	/* Initialize ImGui */
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
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
	{
		GLenum glFilter = (textureFilter_ == TextureFilter::Nearest) ? GL_NEAREST : GL_LINEAR;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glFilter);
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, emuTexW_, emuTexH_, 0, GL_BGRA,
				 GL_UNSIGNED_INT_8_8_8_8_REV, nullptr);

	return true;
}

void ImGuiBackend::destroyWindow()
{
	if (emuTextureId_)
	{
		glDeleteTextures(1, &emuTextureId_);
		emuTextureId_ = 0;
	}
	if (glContext_)
	{
		SDL_GL_DestroyContext(glContext_);
		glContext_ = nullptr;
	}
	if (window_)
	{
		SDL_DestroyWindow(window_);
		window_ = nullptr;
	}
}

bool ImGuiBackend::recreateWindow(const char *title, int width, int height, bool fullscreen)
{
	destroyWindow();
	return createWindow(title, width, height, fullscreen);
}

void ImGuiBackend::getWindowSize(int *w, int *h)
{
	if (window_)
		SDL_GetWindowSize(window_, w, h);
	else
	{
		*w = 0;
		*h = 0;
	}
}

void ImGuiBackend::getWindowPosition(int *x, int *y)
{
	if (window_)
		SDL_GetWindowPosition(window_, x, y);
	else
	{
		*x = 0;
		*y = 0;
	}
}

void ImGuiBackend::setWindowPosition(int x, int y)
{
	if (window_) SDL_SetWindowPosition(window_, x, y);
}

void ImGuiBackend::setFullscreen(bool fullscreen)
{
	if (window_) SDL_SetWindowFullscreen(window_, fullscreen);
}

void ImGuiBackend::clearScreen()
{
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	if (window_) SDL_GL_SwapWindow(window_);
}

/* ── Cursor ──────────────────────────────────────────── */

void ImGuiBackend::showCursor()
{
	if (cursorHidden_)
	{
		SDL_ShowCursor();
		cursorHidden_ = false;
	}
}
void ImGuiBackend::hideCursor()
{
	if (!cursorHidden_)
	{
		SDL_HideCursor();
		cursorHidden_ = true;
	}
}

void ImGuiBackend::setMouseGrab(bool grab)
{
	if (window_)
	{
		SDL_SetWindowMouseGrab(window_, grab);
		SDL_SetWindowRelativeMouseMode(window_, grab);
		relativeMouseMode_ = grab;
	}
}

/* ── Audio ───────────────────────────────────────────── */

bool ImGuiBackend::audioInit()
{
	return GetEmulatorConfig().soundEnabled ? SoundInit() : true;
}
void ImGuiBackend::audioStart()
{
	if (GetEmulatorConfig().soundEnabled) SoundStart();
}
void ImGuiBackend::audioStop()
{
	if (GetEmulatorConfig().soundEnabled) SoundStop();
}
void ImGuiBackend::audioShutdown()
{
	if (GetEmulatorConfig().soundEnabled) SoundUnInit();
}

/* ── Keyboard ────────────────────────────────────────── */

void ImGuiBackend::disableKeyRepeat()
{
	DisableKeyRepeat();
}
void ImGuiBackend::restoreKeyRepeat()
{
	RestoreKeyRepeat();
}

/* ── Dialog ──────────────────────────────────────────── */

void ImGuiBackend::showMessageBox(const char *title, const char *message)
{
	SDL_ShowSimpleMessageBox(0, title, message, window_);
}

bool ImGuiBackend::getDisplayBounds(PlatformDisplayBounds *bounds)
{
	SDL_DisplayID did = SDL_GetPrimaryDisplay();
	if (did == 0) return false;
	const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(did);
	if (!mode) return false;
	bounds->w = mode->w;
	bounds->h = mode->h;
	return true;
}

void ImGuiBackend::onResolutionChanged(uint16_t newW, uint16_t newH)
{
	/* Recreate GL texture at the new resolution */
	emuTexW_ = newW;
	emuTexH_ = newH;

	if (emuTextureId_)
	{
		glDeleteTextures(1, &emuTextureId_);
		emuTextureId_ = 0;
	}
	glGenTextures(1, &emuTextureId_);
	glBindTexture(GL_TEXTURE_2D, emuTextureId_);
	{
		GLenum glFilter = (textureFilter_ == TextureFilter::Nearest) ? GL_NEAREST : GL_LINEAR;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glFilter);
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, newW, newH, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
				 nullptr);

	/* Resize SDL window only in Windowed mode.  Fullscreen handles
	   the new resolution automatically via aspect-ratio scaling. */
	if (uiState_ == UIState::Windowed)
	{
		int scale = 2;
		PlatformDisplayBounds bounds;
		if (getDisplayBounds(&bounds))
		{
			if (newW * 2 > bounds.w || newH * 2 > bounds.h) scale = 1;
		}
		SDL_SetWindowSize(window_, newW * scale, newH * scale);
	}
}

/* ── Zoom (custom maximize: largest Pixel Perfect, centered) ── */

void ImGuiBackend::toggleZoom()
{
	if (!window_ || emuTexW_ == 0 || emuTexH_ == 0) return;
	if (uiState_ != UIState::Windowed) return;

	/* Compute max Pixel Perfect size */
	SDL_Rect usable;
	SDL_DisplayID did = SDL_GetDisplayForWindow(window_);
	if (!did || !SDL_GetDisplayUsableBounds(did, &usable)) return;

	int maxScaleX = std::max(1, usable.w / emuTexW_);
	int maxScaleY = std::max(1, usable.h / emuTexH_);
	int maxScale = std::min(maxScaleX, maxScaleY);
	int maxW = emuTexW_ * maxScale;
	int maxH = emuTexH_ * maxScale;

	int curW, curH;
	SDL_GetWindowSize(window_, &curW, &curH);

	if (zoomed_ || (curW == maxW && curH == maxH))
	{
		/* Unzoom: restore saved geometry */
		if (preZoomW_ > 0 && preZoomH_ > 0)
		{
			snapping_ = true;
			SDL_SetWindowSize(window_, preZoomW_, preZoomH_);
			SDL_SetWindowPosition(window_, preZoomX_, preZoomY_);
			snapping_ = false;
			currentScale_ = preZoomW_ / emuTexW_;
		}
		zoomed_ = false;
	}
	else
	{
		/* Zoom: save current geometry, go to max centered */
		SDL_GetWindowPosition(window_, &preZoomX_, &preZoomY_);
		preZoomW_ = curW;
		preZoomH_ = curH;

		int cx = usable.x + (usable.w - maxW) / 2;
		int cy = usable.y + (usable.h - maxH) / 2;
		snapping_ = true;
		SDL_SetWindowSize(window_, maxW, maxH);
		SDL_SetWindowPosition(window_, cx, cy);
		snapping_ = false;
		currentScale_ = maxScale;
		zoomed_ = true;
	}
}

/* ── Paths ───────────────────────────────────────────── */

const char *ImGuiBackend::getAppParent()
{
	return SDL_GetBasePath();
}

char *ImGuiBackend::getPrefDir(const char *org, const char *app)
{
	return SDL_GetPrefPath(org, app);
}

void ImGuiBackend::freePath(void *path)
{
	SDL_free(path);
}
