/*
	platform_backend.h — abstract platform backend interface

	Defines the PlatformBackend base class that abstracts all
	platform-specific operations (window, cursor, audio, input).
*/

#ifndef PLATFORM_BACKEND_H
#define PLATFORM_BACKEND_H

#include <cstdint>

class EmulatorShell;

struct PlatformEvent {
	enum class Type {
		None,
		Quit,
		FocusGained,
		FocusLost,
		MouseEnter,
		MouseLeave,
		WindowResized,
		MouseMove,
		MouseButtonDown,
		MouseButtonUp,
		KeyDown,
		KeyUp,
		MouseWheel,
		FileDrop
	};
	Type type = Type::None;
	uint8_t macKeyCode = 0;
	bool keyDown = false;
	float x = 0, y = 0;
	float wheelX = 0, wheelY = 0;
	const char* filePath = nullptr;
};

struct PlatformDisplayBounds {
	int x, y, w, h;
};

class PlatformBackend {
public:
	virtual ~PlatformBackend() = default;

	/* Lifecycle */
	virtual bool init(EmulatorShell* shell) = 0;
	virtual void shutdown() = 0;
	virtual void runLoop() = 0;

	/* Window */
	virtual bool createWindow(const char* title,
		int width, int height, bool fullscreen) = 0;
	virtual void destroyWindow() = 0;
	virtual bool recreateWindow(const char* title,
		int width, int height, bool fullscreen) = 0;
	virtual void getWindowSize(int* w, int* h) = 0;
	virtual void getWindowPosition(int* x, int* y) = 0;
	virtual void setWindowPosition(int x, int y) = 0;
	virtual void setFullscreen(bool fullscreen) = 0;
	virtual void clearScreen() = 0;

	/* Cursor */
	virtual void showCursor() = 0;
	virtual void hideCursor() = 0;
	virtual bool warpCursor(int x, int y) = 0;
	virtual void setMouseGrab(bool grab) = 0;

	/* Audio */
	virtual bool audioInit() = 0;
	virtual void audioStart() = 0;
	virtual void audioStop() = 0;
	virtual void audioShutdown() = 0;

	/* Keyboard */
	virtual void disableKeyRepeat() = 0;
	virtual void restoreKeyRepeat() = 0;

	/* Dialog */
	virtual void showMessageBox(const char* title, const char* message) = 0;

	/* Query */
	virtual bool getDisplayBounds(PlatformDisplayBounds* bounds) = 0;

	/* Paths — platform-specific application path resolution.
	   Returned strings are owned by the backend; caller must not free.
	   getAppParent(): directory containing the executable.
	   getPrefDir(): writable directory for user preferences.
	   freePath(): release a path returned by getPrefDir(). */
	virtual const char* getAppParent() = 0;
	virtual char* getPrefDir(const char* org, const char* app) = 0;
	virtual void freePath(void* path) = 0;
};

#endif /* PLATFORM_BACKEND_H */
