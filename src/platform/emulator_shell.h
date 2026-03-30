/*
	emulator_shell.h — Platform-independent emulation orchestration

	Owns all state and logic that was previously in sdl.cpp, minus
	the SDL-specific I/O. Communicates with the platform through
	the PlatformBackend interface.
*/

#ifndef EMULATOR_SHELL_H
#define EMULATOR_SHELL_H

#include "platform/platform_backend.h"
#include <cstdint>

class EmulatorShell {
public:
	explicit EmulatorShell(PlatformBackend* backend);
	~EmulatorShell();

	/* --- Lifecycle --- */
	bool init(int argc, char** argv);
	void shutdown();

	/* --- Event dispatch (from backend) --- */
	void dispatchEvent(const PlatformEvent& evt);

	/* --- State machine (called once per loop iteration) --- */
	void processSavedTasks();

	/* --- Timing --- */
	bool tickIsDue();
	void runOneTick();
	uint32_t getDelayMs();

	/* --- Query --- */
	bool shouldQuit() const;
	bool isSpeedStopped() const;

	/* --- Framebuffer --- */
	bool isFramebufferDirty() const { return framebufferDirty_; }
	const uint8_t* getFramebuffer() const { return argbBuffer_; }
	void clearDirtyFlag() { framebufferDirty_ = false; }
	void doneWithDrawingForTick();

	/* --- Disk --- */
	bool insertDiskOrRom(const char* path, bool silent);

	/* --- Fullscreen toggle (called from control_mode.cpp) --- */
	void toggleWantFullScreen();

	/* --- Mouse (internal, but used by wrapper) --- */
	void checkMouseState();

	/* --- Extra time (for ExtraTimeNotOver wrapper) --- */
	bool extraTimeNotOver();

	/* --- Window title --- */
	const char* windowTitle() const;

private:
	/* --- Helpers --- */
	void drawChangesAndClear();
	void convertFramebuffer(uint16_t top, uint16_t left,
		uint16_t bottom, uint16_t right);
	void mousePositionNotify(int newH, int newV);
	bool moveMouse(int16_t h, int16_t v);
	void mouseConstrain();
	void grabMachine();
	void ungrabMachine();
	void forceShowCursor();
	void leaveBackground();
	void enterBackground();
	void leaveSpeedStopped();
	void enterSpeedStopped();
	bool createMainWindow();
	bool reCreateMainWindow();
	void closeMainWindow();
	void zapWinStateVars();
	bool scanCommandLine();
	bool allocMyMemory();
	void unallocMyMemory();
	bool initWhereAmI();
	void uninitWhereAmI();
	bool loadInitialImages();

	PlatformBackend* backend_;

	/* --- Video state --- */
	int hOffset_ = 0;
	int vOffset_ = 0;
	bool useFullScreen_ = false;
	bool useMagnify_ = true;
	int windowScale_ = 1;

	/* --- Background/speed state --- */
	bool backgroundFlag_ = false;
	bool trueBackgroundFlag_ = false;
	bool curSpeedStopped_ = true;

	/* --- Cursor state --- */
	bool haveCursorHidden_ = false;
	bool wantCursorHidden_ = false;
	bool caughtMouse_ = false;
	bool grabMachine_ = false;

	/* --- Window position tracking --- */
	enum { kMagStateNormal = 0, kMagStateMagnifgy = 1, kNumMagStates = 2 };
	enum { kWinStateWindowed = 0, kWinStateFullScreen = 1, kNumWinStates = 2 };
	static constexpr int kMagStateAuto = kNumMagStates;

	int curWinIndx_ = 0;
	bool havePositionWins_[kNumMagStates] = {};
	int winPositionsX_[kNumMagStates] = {};
	int winPositionsY_[kNumMagStates] = {};
	int winMagStates_[kNumWinStates] = {};

	/* --- Command line / paths --- */
	char* d_arg_ = nullptr;
	char* n_arg_ = nullptr;
	char* pref_dir_ = nullptr;
	int argc_ = 0;
	char** argv_ = nullptr;

	/* --- Framebuffer --- */
	uint8_t* argbBuffer_ = nullptr;
	bool framebufferDirty_ = false;
};

/* Global shell pointer for free-function wrappers. */
extern EmulatorShell* g_shell;

#endif /* EMULATOR_SHELL_H */
