/*
	imgui_overlay.h — Control overlay (Ctrl key)

	Semi-transparent panel drawn over the emulator viewport
	when the Ctrl key is held. Provides machine actions,
	display settings, and speed controls.
*/

#pragma once

#include <cstdint>

class EmulatorShell;
class ImGuiBackend;
enum class UIState;

class ControlOverlay
{
public:
	/* Draw the overlay. Returns true if a UIState change was requested. */
	bool draw(UIState currentState, EmulatorShell *shell, ImGuiBackend *backend,
			  UIState &requestedState);

private:
	void drawPrimaryControls(UIState currentState, EmulatorShell *shell, ImGuiBackend *backend,
							 UIState &requestedState);
	void drawAdvancedControls(ImGuiBackend *backend);
	void drawAbout();

	/* Flash feedback */
	const char *flashMsg_ = nullptr;
	uint64_t flashExpiry_ = 0;
};
