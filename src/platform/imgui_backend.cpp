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
#include "config/mac_file.h"
#include "core/config_loader.h"
#include "core/extn_extfs.h"
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
#include <OpenGL/gl3.h> /* full GL 3.x — superset of gl.h */
extern "C" long platformPasteboardChangeCount();
extern "C" void platformRemoveMenuKeyEquivalents();
#elif defined(_WIN32)
#include <GL/gl.h>
#include <GL/glext.h>
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif

/* ── Non-Apple GL 2.x/3.x function pointers ─────────────────────────────────
   On Apple the symbols are direct exports from OpenGL.framework (via gl3.h).
   On other platforms we load them lazily via SDL_GL_GetProcAddress so the
   binary does not hard-link against symbols absent from the GL 1.1 import
   library on Windows.                                                        */
#ifndef __APPLE__
static PFNGLACTIVETEXTUREPROC           s_glActiveTexture           = nullptr;
static PFNGLCREATESHADERPROC            s_glCreateShader            = nullptr;
static PFNGLSHADERSOURCEPROC            s_glShaderSource            = nullptr;
static PFNGLCOMPILESHADERPROC           s_glCompileShader           = nullptr;
static PFNGLGETSHADERIVPROC             s_glGetShaderiv             = nullptr;
static PFNGLGETSHADERINFOLOGPROC        s_glGetShaderInfoLog        = nullptr;
static PFNGLCREATEPROGRAMPROC           s_glCreateProgram           = nullptr;
static PFNGLATTACHSHADERPROC            s_glAttachShader            = nullptr;
static PFNGLLINKPROGRAMPROC             s_glLinkProgram             = nullptr;
static PFNGLGETPROGRAMIVPROC            s_glGetProgramiv            = nullptr;
static PFNGLGETPROGRAMINFOLOGPROC       s_glGetProgramInfoLog       = nullptr;
static PFNGLDELETESHADERPROC            s_glDeleteShader            = nullptr;
static PFNGLDELETEPROGRAMPROC           s_glDeleteProgram           = nullptr;
static PFNGLUSEPROGRAMPROC              s_glUseProgram              = nullptr;
static PFNGLGETUNIFORMLOCATIONPROC      s_glGetUniformLocation      = nullptr;
static PFNGLUNIFORM1IPROC               s_glUniform1i               = nullptr;
static PFNGLUNIFORM2FPROC               s_glUniform2f               = nullptr;
static PFNGLGETATTRIBLOCATIONPROC       s_glGetAttribLocation       = nullptr;
static PFNGLVERTEXATTRIBPOINTERPROC     s_glVertexAttribPointer     = nullptr;
static PFNGLENABLEVERTEXATTRIBARRAYPROC s_glEnableVertexAttribArray = nullptr;
static PFNGLGENVERTEXARRAYSPROC         s_glGenVertexArrays         = nullptr;
static PFNGLBINDVERTEXARRAYPROC         s_glBindVertexArray         = nullptr;
static PFNGLDELETEVERTEXARRAYSPROC      s_glDeleteVertexArrays      = nullptr;
static PFNGLGENBUFFERSPROC              s_glGenBuffers              = nullptr;
static PFNGLBINDBUFFERPROC              s_glBindBuffer              = nullptr;
static PFNGLBUFFERDATAPROC              s_glBufferData              = nullptr;
static PFNGLDELETEBUFFERSPROC           s_glDeleteBuffers           = nullptr;
static PFNGLGENFRAMEBUFFERSPROC         s_glGenFramebuffers         = nullptr;
static PFNGLBINDFRAMEBUFFERPROC         s_glBindFramebuffer         = nullptr;
static PFNGLFRAMEBUFFERTEXTURE2DPROC    s_glFramebufferTexture2D    = nullptr;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC  s_glCheckFramebufferStatus  = nullptr;
static PFNGLDELETEFRAMEBUFFERSPROC      s_glDeleteFramebuffers      = nullptr;

static bool loadCRTGLFunctions()
{
#define LOAD(type, sym)                                                        \
	do {                                                                       \
		s_##sym = (type)SDL_GL_GetProcAddress(#sym);                           \
		if (!s_##sym) { fprintf(stderr, "CRT: missing %s\n", #sym); return false; } \
	} while (0)
	LOAD(PFNGLACTIVETEXTUREPROC,           glActiveTexture);
	LOAD(PFNGLCREATESHADERPROC,            glCreateShader);
	LOAD(PFNGLSHADERSOURCEPROC,            glShaderSource);
	LOAD(PFNGLCOMPILESHADERPROC,           glCompileShader);
	LOAD(PFNGLGETSHADERIVPROC,             glGetShaderiv);
	LOAD(PFNGLGETSHADERINFOLOGPROC,        glGetShaderInfoLog);
	LOAD(PFNGLCREATEPROGRAMPROC,           glCreateProgram);
	LOAD(PFNGLATTACHSHADERPROC,            glAttachShader);
	LOAD(PFNGLLINKPROGRAMPROC,             glLinkProgram);
	LOAD(PFNGLGETPROGRAMIVPROC,            glGetProgramiv);
	LOAD(PFNGLGETPROGRAMINFOLOGPROC,       glGetProgramInfoLog);
	LOAD(PFNGLDELETESHADERPROC,            glDeleteShader);
	LOAD(PFNGLDELETEPROGRAMPROC,           glDeleteProgram);
	LOAD(PFNGLUSEPROGRAMPROC,              glUseProgram);
	LOAD(PFNGLGETUNIFORMLOCATIONPROC,      glGetUniformLocation);
	LOAD(PFNGLUNIFORM1IPROC,               glUniform1i);
	LOAD(PFNGLUNIFORM2FPROC,               glUniform2f);
	LOAD(PFNGLGETATTRIBLOCATIONPROC,       glGetAttribLocation);
	LOAD(PFNGLVERTEXATTRIBPOINTERPROC,     glVertexAttribPointer);
	LOAD(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray);
	LOAD(PFNGLGENVERTEXARRAYSPROC,         glGenVertexArrays);
	LOAD(PFNGLBINDVERTEXARRAYPROC,         glBindVertexArray);
	LOAD(PFNGLDELETEVERTEXARRAYSPROC,      glDeleteVertexArrays);
	LOAD(PFNGLGENBUFFERSPROC,              glGenBuffers);
	LOAD(PFNGLBINDBUFFERPROC,              glBindBuffer);
	LOAD(PFNGLBUFFERDATAPROC,              glBufferData);
	LOAD(PFNGLDELETEBUFFERSPROC,           glDeleteBuffers);
	LOAD(PFNGLGENFRAMEBUFFERSPROC,         glGenFramebuffers);
	LOAD(PFNGLBINDFRAMEBUFFERPROC,         glBindFramebuffer);
	LOAD(PFNGLFRAMEBUFFERTEXTURE2DPROC,    glFramebufferTexture2D);
	LOAD(PFNGLCHECKFRAMEBUFFERSTATUSPROC,  glCheckFramebufferStatus);
	LOAD(PFNGLDELETEFRAMEBUFFERSPROC,      glDeleteFramebuffers);
#undef LOAD
	return true;
}

/* Route standard GL names to the loaded pointers for the CRT section. */
#define glActiveTexture            s_glActiveTexture
#define glCreateShader             s_glCreateShader
#define glShaderSource             s_glShaderSource
#define glCompileShader            s_glCompileShader
#define glGetShaderiv              s_glGetShaderiv
#define glGetShaderInfoLog         s_glGetShaderInfoLog
#define glCreateProgram            s_glCreateProgram
#define glAttachShader             s_glAttachShader
#define glLinkProgram              s_glLinkProgram
#define glGetProgramiv             s_glGetProgramiv
#define glGetProgramInfoLog        s_glGetProgramInfoLog
#define glDeleteShader             s_glDeleteShader
#define glDeleteProgram            s_glDeleteProgram
#define glUseProgram               s_glUseProgram
#define glGetUniformLocation       s_glGetUniformLocation
#define glUniform1i                s_glUniform1i
#define glUniform2f                s_glUniform2f
#define glGetAttribLocation        s_glGetAttribLocation
#define glVertexAttribPointer      s_glVertexAttribPointer
#define glEnableVertexAttribArray  s_glEnableVertexAttribArray
#define glGenVertexArrays          s_glGenVertexArrays
#define glBindVertexArray          s_glBindVertexArray
#define glDeleteVertexArrays       s_glDeleteVertexArrays
#define glGenBuffers               s_glGenBuffers
#define glBindBuffer               s_glBindBuffer
#define glBufferData               s_glBufferData
#define glDeleteBuffers            s_glDeleteBuffers
#define glGenFramebuffers          s_glGenFramebuffers
#define glBindFramebuffer          s_glBindFramebuffer
#define glFramebufferTexture2D     s_glFramebufferTexture2D
#define glCheckFramebufferStatus   s_glCheckFramebufferStatus
#define glDeleteFramebuffers       s_glDeleteFramebuffers
#endif /* !__APPLE__ */

#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <array>
#include <vector>

#include "platform/host_pasteboard.h"
#include "stb_image_write.h"

/* ── Shortcut table ──────────────────────────────────── */

struct ShortcutEntry
{
	SDL_Scancode scancode;
	UIAction action;
};

static constexpr std::array kShortcuts = {
	ShortcutEntry{SDL_SCANCODE_F, UIAction::ToggleFullscreen},
	ShortcutEntry{SDL_SCANCODE_P, UIAction::ToggleScaling},
	ShortcutEntry{SDL_SCANCODE_Z, UIAction::Zoom},
	ShortcutEntry{SDL_SCANCODE_S, UIAction::Screenshot},
	ShortcutEntry{SDL_SCANCODE_RIGHT, UIAction::SpeedUp},
	ShortcutEntry{SDL_SCANCODE_LEFT, UIAction::SpeedDown},
	ShortcutEntry{SDL_SCANCODE_I, UIAction::InsertDisk},
	ShortcutEntry{SDL_SCANCODE_R, UIAction::Reboot},
	ShortcutEntry{SDL_SCANCODE_N, UIAction::Interrupt},
	ShortcutEntry{SDL_SCANCODE_Q, UIAction::PowerOff},
};

/* ── Speed presets ───────────────────────────────────── */

static constexpr uint8_t kSpeedPresets[] = {0, 1, 2, 3, 4, 5, (uint8_t)-1};
static constexpr int kSpeedPresetCount = 7;

/* ── init / shutdown ─────────────────────────────────── */

bool ImGuiBackend::init(EmulatorShell *shell)
{
	shell_ = shell;

#if defined(__APPLE__)
	/* Disable SDL's Ctrl+Click → right-click emulation so that Ctrl+Click
	   while the overlay is open reaches ImGui as a plain left-click.
	   Must be set before SDL_Init. */
	SDL_SetHint(SDL_HINT_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK, "0");
#endif

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
	{
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return false;
	}

#if defined(__APPLE__)
	/* Strip Cmd+Q and Cmd+W from the macOS menu bar so they flow
	   through SDL as normal key events and reach the guest. */
	platformRemoveMenuKeyEquivalents();
#endif

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
	shutdownCRTResources();
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

	/* In Launcher state, shouldQuit() would crash (no machine).
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

		/* Clipboard change detection — use native changeCount on
		   macOS (zero-cost integer compare), fall back to SDL event
		   on other platforms. */
#if defined(__APPLE__)
		{
			static long s_lastChangeCount = 0;
			long cc = platformPasteboardChangeCount();
			if (cc != s_lastChangeCount)
			{
				s_lastChangeCount = cc;
				GetHostPasteboard().onClipboardUpdate();
			}
		}
#endif

		/* 1. Poll SDL events — feed to ImGui first */
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL3_ProcessEvent(&event);

#if !defined(__APPLE__)
			if (event.type == SDL_EVENT_CLIPBOARD_UPDATE)
			{
				GetHostPasteboard().onClipboardUpdate();
				continue;
			}
#endif

			/* In Launcher state, only handle quit events */
			if (uiState_ == UIState::Launcher)
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
			/* Shortcut dispatch while overlay is visible.
			   Works with or without Ctrl held (so bare keys work in sticky mode). */
			if (overlayMode_ != OverlayMode::Hidden && event.type == SDL_EVENT_KEY_DOWN &&
				!event.key.repeat && event.key.scancode != SDL_SCANCODE_LCTRL &&
				event.key.scancode != SDL_SCANCODE_RCTRL)
			{
				UIAction action = UIAction::None;
				if ((event.key.mod & (SDL_KMOD_LSHIFT | SDL_KMOD_RSHIFT)) &&
					event.key.scancode == SDL_SCANCODE_S)
				{
					action = UIAction::SaveScreenshot;
				}
				else
				{
					/* Digit keys: 0=pause toggle, 1-6=speed presets, 9=Max */
					int digit = -1;
					if (event.key.scancode == SDL_SCANCODE_0)
						digit = 0;
					else if (event.key.scancode >= SDL_SCANCODE_1 &&
					         event.key.scancode <= SDL_SCANCODE_9)
						digit = event.key.scancode - SDL_SCANCODE_1 + 1;

					if (digit >= 0)
					{
						if (digit == 0)
							g_speedStopped = !g_speedStopped;
						else if (digit >= 1 && digit <= 6)
							g_speedValue = (uint8_t)(digit - 1);
						else if (digit == 9)
							g_speedValue = (uint8_t)-1;
						continue;
					}
					for (const auto &s : kShortcuts)
					{
						if (event.key.scancode == s.scancode)
						{
							action = s.action;
							break;
						}
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
				if (overlayMode_ != OverlayMode::Hidden)
				{
					/* A click outside the overlay panel dismisses it; the click
					   is forwarded to the emulator naturally below.
					   Any other event while the overlay is visible is discarded. */
					if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
						overlayMode_ = OverlayMode::Hidden;
					else
						continue;
				}

				PlatformEvent pe = translateSdlEvent(event);
				if (pe.type != PlatformEvent::Type::None) shell_->dispatchEvent(pe);
			}
		}

		/* Branch on UI state */
		switch (uiState_)
		{
			case UIState::Launcher:
			{
				ImGui_ImplOpenGL3_NewFrame();
				ImGui_ImplSDL3_NewFrame();
				ImGui::NewFrame();

				drawLauncher();

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

				if (pendingBoot_)
				{
					pendingBoot_ = false;
					bootFromLauncherConfig(pendingBootConfig_);
				}
				else
				{
					SDL_Delay(16);
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

	/* Handle speed-stopped state: throttle frame rate but keep rendering
	   so the overlay remains responsive. */
	if (shell_->isSpeedStopped())
	{
		SDL_Delay(16);
		/* fall through to render path */
	}
	else
	{
		/* Run emulation ticks */
		if (!shell_->tickIsDue())
		{
			SDL_Delay(shell_->getDelayMs());
			return;
		}
		if (shell_->tickIsDue() && !shell_->shouldQuit()) shell_->runOneTick();
	}

	/* Periodic shared-drive catalog refresh (~every 2 seconds at 60 fps).
	   Host-driven, not guest-driven — keeps the door open for future
	   filesystem-notification triggers. */
	{
		static int s_refreshCounter = 0;
		if (++s_refreshCounter >= 120)
		{
			s_refreshCounter = 0;
			ExtFS_RefreshCatalogs();
		}
	}

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

void ImGuiBackend::bootFromLauncherConfig(const LaunchConfig &config)
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

/* ── Launcher ────────────────────────────────────────── */

void ImGuiBackend::drawLauncher()
{
	launcher_.draw();
	const MacFileEntry *selected = launcher_.selectedMac();
	if (selected && shell_)
	{
		bootFromLauncher(*selected);
	}
}

void ImGuiBackend::bootFromLauncher(const MacFileEntry &entry)
{
	LaunchConfig lc = LaunchConfigFromMacEntry(entry, launcherDataDir_);
	pendingBoot_ = true;
	pendingBootConfig_ = lc;
}

bool ImGuiBackend::createLauncher(std::vector<MacFileEntry> entries)
{
	launcherDataDir_ = "data"; // default; caller can set properly

	/* Create the Launcher window */
	Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY;
	window_ = SDL_CreateWindow("maxivmac", 700, 500, flags);
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

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigWindowsMoveFromTitleBarOnly = true;
#if defined(__APPLE__)
	/* Disable ImGui's built-in Ctrl+Left-click → Right-click conversion.
	   We use Ctrl as the overlay activation key, so Ctrl+Click must reach
	   overlay buttons as a plain left-click. */
	io.ConfigMacOSXBehaviors = false;
#endif
	ImGui::StyleColorsDark();

	ImGui_ImplSDL3_InitForOpenGL(window_, glContext_);
	ImGui_ImplOpenGL3_Init("#version 150");

	launcher_.init(std::move(entries));
	launcher_.loadTextures();
	return true;
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
		case UIAction::SaveScreenshot:
			captureScreenshotToFile();
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
			/* Peek mode: dismiss immediately (Ctrl is held; overlay goes regardless).
			   Sticky mode: keep overlay open until dialog resolves. */
			if (overlayMode_ == OverlayMode::Peek || overlayMode_ == OverlayMode::PeekPending)
				overlayMode_ = OverlayMode::Hidden;
			openFileDialog();
			break;
		case UIAction::Reboot:
			g_wantMacReset = true;
			overlayMode_ = OverlayMode::Hidden;
			break;
		case UIAction::PowerOff:
			g_requestMacOff = true;
			break;
		case UIAction::Interrupt:
			g_wantMacInterrupt = true;
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

static std::vector<uint8_t> buildScreenshotPng(const uint32_t *src, int w, int h)
{
	std::vector<uint8_t> rgba(w * h * 4);
	for (int i = 0; i < w * h; ++i)
	{
		uint32_t px = src[i];
		rgba[i * 4 + 0] = (px >> 16) & 0xFF;
		rgba[i * 4 + 1] = (px >> 8) & 0xFF;
		rgba[i * 4 + 2] = px & 0xFF;
		rgba[i * 4 + 3] = 0xFF;
	}
	std::vector<uint8_t> pngBuf;
	stbi_write_png_to_func(pngWriteCallback, &pngBuf, w, h, 4, rgba.data(), w * 4);
	return pngBuf;
}

void ImGuiBackend::captureScreenshot()
{
	if (!shell_ || !shell_->getFramebuffer()) return;
	auto png = buildScreenshotPng(
		reinterpret_cast<const uint32_t *>(shell_->getFramebuffer()), emuTexW_, emuTexH_);
	if (!png.empty())
	{
		GetHostPasteboard().setImage(png.data(), png.size());
		overlay_.flash("Screenshot copied to clipboard", 2000);
	}
}

struct ScreenshotSaveContext
{
	ImGuiBackend *backend;
	std::vector<uint8_t> png;
};

static void screenshotSaveCallback(void *userdata, const char *const *filelist, int /*filter*/)
{
	auto *ctx = static_cast<ScreenshotSaveContext *>(userdata);
	if (filelist && filelist[0])
	{
		FILE *f = fopen(filelist[0], "wb");
		if (f)
		{
			fwrite(ctx->png.data(), 1, ctx->png.size(), f);
			fclose(f);
			ctx->backend->flashOverlay("Screenshot saved", 2000);
		}
	}
	delete ctx;
}

void ImGuiBackend::captureScreenshotToFile()
{
	if (!shell_ || !shell_->getFramebuffer()) return;
	auto *ctx = new ScreenshotSaveContext;
	ctx->backend = this;
	ctx->png = buildScreenshotPng(
		reinterpret_cast<const uint32_t *>(shell_->getFramebuffer()), emuTexW_, emuTexH_);
	if (ctx->png.empty()) { delete ctx; return; }
	static const SDL_DialogFileFilter filters[] = {
		{"PNG Image", "png"}, {"All Files", "*"}
	};
	SDL_ShowSaveFileDialog(screenshotSaveCallback, ctx, window_, filters, 2, "screenshot.png");
}

static void fileDialogCallback(void *userdata, const char *const *filelist, int filter)
{
	(void)filter;
	auto *backend = static_cast<ImGuiBackend *>(userdata);
	if (filelist && filelist[0])
	{
		backend->shell()->insertDiskOrRom(filelist[0], false);
		backend->hideOverlay(); // disk inserted → dismiss overlay
	}
	// cancelled → overlay stays as-is
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

/* ── CRT shader ──────────────────────────────────────── */

static const char *kCRTVertSrc = R"GLSL(
#version 150
in  vec2 aPos;
in  vec2 aUV;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)GLSL";

static const char *kCRTFragSrc = R"GLSL(
#version 150
in  vec2      vUV;
out vec4      fragColor;
uniform sampler2D uTex;
uniform vec2      uTexSize;   /* emulator texture pixel dimensions */

/* Barrel distortion — push UV outward from centre */
vec2 barrel(vec2 uv) {
    vec2  cc   = uv - 0.5;
    float dist = dot(cc, cc);
    return uv + cc * (dist * 0.10);
}

void main() {
    vec2 uv = barrel(vUV);

    /* Out-of-bounds → black border */
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec4 col = texture(uTex, uv);

    /* Scanlines: darken alternate rows at emulator-pixel resolution
       so the gaps scale correctly with zoom level.                   */
    float row = floor(uv.y * uTexSize.y);
    col.rgb  *= mix(0.60, 1.0, mod(row, 2.0));

    /* Vignette: darken towards the edges */
    vec2  vig      = uv * (1.0 - uv);
    float vignette = clamp(pow(vig.x * vig.y * 18.0, 0.35), 0.0, 1.0);
    col.rgb *= vignette;

    fragColor = col;
}
)GLSL";

/* Compile one shader stage; returns 0 and logs on failure. */
static GLuint compileShader(GLenum type, const char *src)
{
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, nullptr);
	glCompileShader(s);
	GLint ok = 0;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok)
	{
		char log[512];
		glGetShaderInfoLog(s, sizeof(log), nullptr, log);
		fprintf(stderr, "CRT shader compile error: %s\n", log);
		glDeleteShader(s);
		return 0;
	}
	return s;
}

void ImGuiBackend::initCRTResources()
{
#ifndef __APPLE__
	if (!loadCRTGLFunctions()) return;
#endif

	GLuint vert = compileShader(GL_VERTEX_SHADER, kCRTVertSrc);
	if (!vert) return;
	GLuint frag = compileShader(GL_FRAGMENT_SHADER, kCRTFragSrc);
	if (!frag) { glDeleteShader(vert); return; }

	GLuint prog = glCreateProgram();
	glAttachShader(prog, vert);
	glAttachShader(prog, frag);
	glLinkProgram(prog);
	glDeleteShader(vert);
	glDeleteShader(frag);

	GLint ok = 0;
	glGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if (!ok)
	{
		char log[512];
		glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
		fprintf(stderr, "CRT shader link error: %s\n", log);
		glDeleteProgram(prog);
		return;
	}

	/* Fullscreen quad: two triangles covering NDC [-1,1]×[-1,1].
	   UV (0,0) = OpenGL bottom-left = Mac top row (raster order).   */
	static const float kQuad[] = {
		/* x      y     u     v  */
		-1.0f, -1.0f,  0.0f, 0.0f,
		 1.0f, -1.0f,  1.0f, 0.0f,
		-1.0f,  1.0f,  0.0f, 1.0f,
		 1.0f,  1.0f,  1.0f, 1.0f,
	};

	GLuint vao = 0, vbo = 0;
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(kQuad), kQuad, GL_STATIC_DRAW);

	GLint aPos = glGetAttribLocation(prog, "aPos");
	GLint aUV  = glGetAttribLocation(prog, "aUV");
	if (aPos >= 0)
	{
		glEnableVertexAttribArray((GLuint)aPos);
		glVertexAttribPointer((GLuint)aPos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
	}
	if (aUV >= 0)
	{
		glEnableVertexAttribArray((GLuint)aUV);
		glVertexAttribPointer((GLuint)aUV, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
							  (void *)(2 * sizeof(float)));
	}
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	crtProgram_         = prog;
	crtVao_             = vao;
	crtVbo_             = vbo;
	crtUniformTex_      = glGetUniformLocation(prog, "uTex");
	crtUniformTexSize_  = glGetUniformLocation(prog, "uTexSize");
}

void ImGuiBackend::shutdownCRTResources()
{
	if (crtFboId_)    { glDeleteFramebuffers(1, &crtFboId_);  crtFboId_ = 0; }
	if (crtFboTexId_) { glDeleteTextures(1, &crtFboTexId_);   crtFboTexId_ = 0; }
	if (crtVbo_)      { glDeleteBuffers(1, &crtVbo_);         crtVbo_ = 0; }
	if (crtVao_)      { glDeleteVertexArrays(1, &crtVao_);    crtVao_ = 0; }
	if (crtProgram_)  { glDeleteProgram(crtProgram_);         crtProgram_ = 0; }
	crtFboW_ = crtFboH_ = 0;
}

void ImGuiBackend::ensureCRTFBO(int w, int h)
{
	if (crtFboW_ == w && crtFboH_ == h) return;

	if (crtFboTexId_) glDeleteTextures(1, &crtFboTexId_);
	if (crtFboId_)    glDeleteFramebuffers(1, &crtFboId_);

	glGenTextures(1, &crtFboTexId_);
	glBindTexture(GL_TEXTURE_2D, crtFboTexId_);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	glGenFramebuffers(1, &crtFboId_);
	glBindFramebuffer(GL_FRAMEBUFFER, crtFboId_);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, crtFboTexId_, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		fprintf(stderr, "CRT FBO incomplete (%d×%d)\n", w, h);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	crtFboW_ = w;
	crtFboH_ = h;
}

void ImGuiBackend::renderCRTPass(int viewW, int viewH)
{
	if (!crtProgram_) return;
	ensureCRTFBO(viewW, viewH);

	/* Save GL state we touch */
	GLint prevFBO      = 0; glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFBO);
	GLint prevVP[4]    = {}; glGetIntegerv(GL_VIEWPORT, prevVP);
	GLint prevProg     = 0; glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
	GLint prevActiveTex = 0; glGetIntegerv(GL_ACTIVE_TEXTURE, &prevActiveTex);
	GLint prevTex0     = 0;
	glActiveTexture(GL_TEXTURE0);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex0);
	GLint prevVAO      = 0; glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);

	/* Render through CRT shader into the FBO */
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, crtFboId_);
	glViewport(0, 0, viewW, viewH);

	glUseProgram(crtProgram_);
	glUniform1i(crtUniformTex_, 0);
	glUniform2f(crtUniformTexSize_, (float)emuTexW_, (float)emuTexH_);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, emuTextureId_);

	glBindVertexArray(crtVao_);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray((GLuint)prevVAO);

	/* Restore GL state */
	glBindTexture(GL_TEXTURE_2D, (GLuint)prevTex0);
	glActiveTexture((GLenum)prevActiveTex);
	glUseProgram((GLuint)prevProg);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)prevFBO);
	glViewport(prevVP[0], prevVP[1], prevVP[2], prevVP[3]);
}

#ifndef __APPLE__
/* ── Remove CRT GL name aliases ──────────────────────── */
#undef glActiveTexture
#undef glCreateShader
#undef glShaderSource
#undef glCompileShader
#undef glGetShaderiv
#undef glGetShaderInfoLog
#undef glCreateProgram
#undef glAttachShader
#undef glLinkProgram
#undef glGetProgramiv
#undef glGetProgramInfoLog
#undef glDeleteShader
#undef glDeleteProgram
#undef glUseProgram
#undef glGetUniformLocation
#undef glUniform1i
#undef glUniform2f
#undef glGetAttribLocation
#undef glVertexAttribPointer
#undef glEnableVertexAttribArray
#undef glGenVertexArrays
#undef glBindVertexArray
#undef glDeleteVertexArrays
#undef glGenBuffers
#undef glBindBuffer
#undef glBufferData
#undef glDeleteBuffers
#undef glGenFramebuffers
#undef glBindFramebuffer
#undef glFramebufferTexture2D
#undef glCheckFramebufferStatus
#undef glDeleteFramebuffers
#endif /* !__APPLE__ */

void ImGuiBackend::setRenderStyle(RenderStyle s)
{
	renderStyle_ = s;
}

/* ── ImGui drawing ───────────────────────────────────── */

void ImGuiBackend::displayEmulatorImage(float w, float h)
{
	ImVec2 pos = ImGui::GetCursorScreenPos();
	emuViewOriginX_ = pos.x;
	emuViewOriginY_ = pos.y;
	emuViewW_ = w;
	emuViewH_ = h;

	if (renderStyle_ == RenderStyle::CRT && crtProgram_)
	{
		/* Size the FBO to physical pixels for maximum quality on HiDPI */
		const ImGuiIO &io = ImGui::GetIO();
		int physW = std::max(1, (int)(w * io.DisplayFramebufferScale.x));
		int physH = std::max(1, (int)(h * io.DisplayFramebufferScale.y));
		renderCRTPass(physW, physH);
		ImGui::Image((ImTextureID)(intptr_t)crtFboTexId_, ImVec2(w, h));
	}
	else
	{
		ImGui::Image((ImTextureID)(intptr_t)emuTextureId_, ImVec2(w, h));
	}
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
#if defined(__APPLE__)
	/* Disable ImGui's built-in Ctrl+Left-click → Right-click conversion.
	   We use Ctrl as the overlay activation key, so Ctrl+Click must reach
	   overlay buttons as a plain left-click. */
	io.ConfigMacOSXBehaviors = false;
#endif
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

	initCRTResources();

	return true;
}

void ImGuiBackend::destroyWindow()
{
	shutdownCRTResources();
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

	/* Max Pixel Perfect scale for the current display */
	SDL_Rect usable;
	SDL_DisplayID did = SDL_GetDisplayForWindow(window_);
	if (!did || !SDL_GetDisplayUsableBounds(did, &usable)) return;

	int maxScale = std::min(std::max(1, usable.w / emuTexW_),
						   std::max(1, usable.h / emuTexH_));

	/* Current effective integer scale (floor of window/texture ratio) */
	int curW, curH;
	SDL_GetWindowSize(window_, &curW, &curH);
	int curScale = std::min(std::max(1, curW / emuTexW_),
						   std::max(1, curH / emuTexH_));

	/* Cycle: 1 → 2 → … → max → 1 */
	int nextScale = (curScale % maxScale) + 1;

	int newW = emuTexW_ * nextScale;
	int newH = emuTexH_ * nextScale;
	int cx   = usable.x + (usable.w - newW) / 2;
	int cy   = usable.y + (usable.h - newH) / 2;
	snapping_ = true;
	SDL_SetWindowSize(window_, newW, newH);
	SDL_SetWindowPosition(window_, cx, cy);
	snapping_ = false;
	currentScale_ = nextScale;
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
