/*
	imgui_overlay.h — Control overlay (Ctrl key)

	Semi-transparent panel drawn over the emulator viewport
	when the Ctrl key is held. Provides machine actions,
	display settings, and speed controls.
*/

#pragma once

class EmulatorShell;
class ImGuiBackend;
enum class UIState;

class ControlOverlay {
public:
	/* Draw the overlay. Returns true if a UIState change was requested. */
	bool draw(UIState currentState, EmulatorShell* shell,
		ImGuiBackend* backend, UIState& requestedState);

private:
	void drawMachineTab(EmulatorShell* shell);
	void drawDisplayTab(UIState currentState, ImGuiBackend* backend,
		UIState& requestedState);
	void drawSpeedTab();
	void drawAdvancedTab(UIState currentState, UIState& requestedState);
};
