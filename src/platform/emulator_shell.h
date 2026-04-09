/*
	emulator_shell.h — Platform-independent emulation orchestration

	Owns all state and logic that was previously in sdl.cpp, minus
	the SDL-specific I/O. Communicates with the platform through
	the PlatformBackend interface.
*/

#ifndef EMULATOR_SHELL_H
#define EMULATOR_SHELL_H

#include "platform/platform_backend.h"
#include "platform/display_state.h"
#include <cstdint>

class EmulatorShell {
public:
	explicit EmulatorShell(PlatformBackend* backend);
	~EmulatorShell();

	/* --- Lifecycle --- */
	bool init(int argc, char** argv);

	/* Two-phase init for the model selector flow:
	   initPlatform() sets up backend, window, paths — no emulation.
	   initMachine() does everything else (ROM, RAM, devices, boot).
	   The old init() is equivalent to initPlatform() + initMachine(). */
	bool initPlatform(int argc, char** argv);
	bool initMachine();

	bool isMachineInited() const { return machineInited_; }
	bool isRomLoaded() const { return romLoaded_; }
	void setRomLoaded(bool v) { romLoaded_ = v; }
	char* getAppParent() const { return appParent_; }
	DisplayState& display() { return display_; }

	void queueMessage(const char* brief, const char* longMsg, bool fatal);
	bool hasQueuedMessage() const { return savedBriefMsg_ != nullptr; }
	const char* getBriefMsg() const { return savedBriefMsg_; }
	const char* getLongMsg() const { return savedLongMsg_; }
	void clearQueuedMessage() { savedBriefMsg_ = nullptr; }

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

	/* --- Cursor --- */
	void forceShowCursor();

	/* --- Fullscreen hint (for backends that manage their own fullscreen) --- */
	void setFullscreenHint(bool fs) { fullscreenHint_ = fs; }

	/* --- Window title --- */
	const char* windowTitle() const;

private:
	/* --- Helpers --- */
	void drawChangesAndClear();
	void convertFramebuffer(uint16_t top, uint16_t left,
		uint16_t bottom, uint16_t right);
	void mousePositionNotify(int newH, int newV);
	void grabMachine();
	void ungrabMachine();
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
	bool mouseInWindow_ = false;
	bool grabMachine_ = false;
	bool fullscreenHint_ = false;

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
	char* romPath_ = nullptr;
	char* appParent_ = nullptr;
	int argc_ = 0;
	char** argv_ = nullptr;

	/* --- Framebuffer --- */
	uint8_t* argbBuffer_ = nullptr;
	bool framebufferDirty_ = false;

	/* --- Init state --- */
	bool machineInited_ = false;
	bool romLoaded_ = false;

	/* --- Consolidated display state --- */
	DisplayState display_;

	/* --- Queued error message (single-slot) --- */
	const char* savedBriefMsg_ = nullptr;
	const char* savedLongMsg_ = nullptr;
	bool savedFatalMsg_ = false;
};

/* Global shell pointer for free-function wrappers. */
extern EmulatorShell* g_shell;

#endif /* EMULATOR_SHELL_H */
