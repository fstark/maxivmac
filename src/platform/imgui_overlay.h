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

	/* Show a brief feedback message in the info area. */
	void flash(const char *msg, uint32_t ms);

	/* Set the GL texture ID for the tool disk icon (call once after GL init). */
	void setToolDiskIcon(uint32_t texId) { toolDiskIconTex_ = texId; }

private:
	void drawPrimaryControls(UIState currentState, EmulatorShell *shell, ImGuiBackend *backend,
							 UIState &requestedState);
	void drawAdvancedControls(ImGuiBackend *backend);
	void drawAbout(ImGuiBackend *backend);

	/* Flash feedback */
	const char *flashMsg_ = nullptr;
	uint64_t flashExpiry_ = 0;

	/* Rendering state */
	bool crtEnabled_ = false;
	uint32_t toolDiskIconTex_ = 0;

	/* Hover help (set each frame by drawPrimaryControls / drawAdvancedControls) */
	const char *hoverHelp_ = nullptr;
};
