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

/* Forward declarations to avoid pulling in the full osglu_common.h
   include chain (which depends on emulator config macros). */
extern void InitKeyCodes();
extern bool g_requestMacOff;

/* Debug window visibility toggles (defined in imgui_debug_windows.cpp) */
extern bool g_showRegisters;
extern bool g_showDisassembly;
extern bool g_showMemory;
extern bool g_showVIA;

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

	while (!shell_->shouldQuit()) {
		/* 1. Poll SDL events — feed to ImGui first */
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL3_ProcessEvent(&event);
			if (!imGuiConsumedEvent(event)) {
				PlatformEvent pe = translateSdlEvent(event);
				if (pe.type != PlatformEvent::Type::None)
					shell_->dispatchEvent(pe);
			}
		}

		/* 2. Process saved tasks (disk inserts etc.) */
		shell_->processSavedTasks();
		if (shell_->shouldQuit()) break;

		/* 3. Handle speed-stopped state */
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
			continue;
		}

		/* 4. Run emulation ticks */
		if (!shell_->tickIsDue()) {
			SDL_Delay(shell_->getDelayMs());
			continue;
		}
		while (shell_->tickIsDue() && !shell_->shouldQuit())
			shell_->runOneTick();

		/* 5. Upload emulator framebuffer to GL texture */
		uploadFramebuffer();

		/* 6. ImGui frame */
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();

		drawMenuBar();
		drawEmulatorViewport();
		DrawDebugWindows();

		ImGui::Render();

		/* 7. Render */
		int displayW, displayH;
		SDL_GetWindowSizeInPixels(window_, &displayW, &displayH);
		glViewport(0, 0, displayW, displayH);
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(window_);
	}
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
			return io.WantCaptureMouse;
		default:
			return false;
	}
}

PlatformEvent ImGuiBackend::translateSdlEvent(SDL_Event& event)
{
	PlatformEvent pEvt;

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
			pEvt.x = event.motion.x;
			pEvt.y = event.motion.y;
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			pEvt.type = PlatformEvent::Type::MouseButtonDown;
			pEvt.x = event.button.x;
			pEvt.y = event.button.y;
			break;
		case SDL_EVENT_MOUSE_BUTTON_UP:
			pEvt.type = PlatformEvent::Type::MouseButtonUp;
			pEvt.x = event.button.x;
			pEvt.y = event.button.y;
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
			if (ImGui::MenuItem("Fullscreen"))
				shell_->toggleWantFullScreen();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Debug")) {
			ImGui::MenuItem("Registers",   nullptr, &g_showRegisters);
			ImGui::MenuItem("Disassembly", nullptr, &g_showDisassembly);
			ImGui::MenuItem("Memory",      nullptr, &g_showMemory);
			ImGui::MenuItem("VIA State",   nullptr, &g_showVIA);
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}

void ImGuiBackend::drawEmulatorViewport()
{
	ImGui::SetNextWindowSize(
		ImVec2((float)emuTexW_ + 16, (float)emuTexH_ + 36),
		ImGuiCond_FirstUseEver);

	if (ImGui::Begin("Macintosh")) {
		ImVec2 avail = ImGui::GetContentRegionAvail();
		float scale = std::min(
			avail.x / (float)emuTexW_,
			avail.y / (float)emuTexH_);
		if (scale < 0.1f) scale = 0.1f;
		ImVec2 size((float)emuTexW_ * scale, (float)emuTexH_ * scale);
		ImGui::Image((ImTextureID)(intptr_t)emuTextureId_, size);
	}
	ImGui::End();
}

/* ── Window ──────────────────────────────────────────── */

bool ImGuiBackend::createWindow(const char* title,
	int width, int height, bool fullscreen)
{
	/* Create window larger than emulator resolution for ImGui chrome */
	int winW = width * 2;
	int winH = height * 2;

	Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
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
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
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

bool ImGuiBackend::warpCursor(int x, int y)
{
	if (!window_) return false;
	SDL_WarpMouseInWindow(window_, (float)x, (float)y);
	return true;
}

void ImGuiBackend::setMouseGrab(bool grab)
{
	if (window_)
		SDL_SetWindowMouseGrab(window_, grab);
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
