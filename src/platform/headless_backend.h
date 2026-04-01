/*
	headless_backend.h — headless (no-window, no-audio) backend

	All platform primitives are no-ops. The run loop drives emulation
	at maximum speed with no rendering or event polling. Suitable for
	CI golden-file testing via --verify.
*/

#ifndef HEADLESS_BACKEND_H
#define HEADLESS_BACKEND_H

#include "platform/platform_backend.h"
#include <cstdio>

class HeadlessBackend : public PlatformBackend {
public:
	bool init(EmulatorShell* shell) override;
	void shutdown() override;
	void runLoop() override;

	/* Window — all no-ops */
	bool createWindow(const char*, int, int, bool) override { return true; }
	void destroyWindow() override {}
	bool recreateWindow(const char*, int, int, bool) override { return true; }
	void getWindowSize(int* w, int* h) override { *w = 0; *h = 0; }
	void getWindowPosition(int* x, int* y) override { *x = 0; *y = 0; }
	void setWindowPosition(int, int) override {}
	void setFullscreen(bool) override {}
	void clearScreen() override {}

	/* Cursor — all no-ops */
	void showCursor() override {}
	void hideCursor() override {}
	void setMouseGrab(bool) override {}

	/* Audio — all no-ops */
	bool audioInit() override { return true; }
	void audioStart() override {}
	void audioStop() override {}
	void audioShutdown() override {}

	/* Keyboard — all no-ops */
	void disableKeyRepeat() override {}
	void restoreKeyRepeat() override {}

	/* Dialog — print to stderr */
	void showMessageBox(const char* title, const char* message) override {
		fprintf(stderr, "%s: %s\n", title, message);
	}

	/* Query — no display */
	bool getDisplayBounds(PlatformDisplayBounds*) override { return false; }

	/* Paths — use POSIX equivalents */
	const char* getAppParent() override;
	char* getPrefDir(const char* org, const char* app) override;
	void freePath(void* path) override;

private:
	EmulatorShell* shell_ = nullptr;
};

#endif /* HEADLESS_BACKEND_H */
