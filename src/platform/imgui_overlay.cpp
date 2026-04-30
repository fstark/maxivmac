/*
	imgui_overlay.cpp — Control overlay (Ctrl key)

	Single flat panel with primary controls always visible
	and an advanced section collapsed by default.
*/

#include "platform/imgui_overlay.h"
#include "platform/imgui_backend.h"
#include "platform/emulator_shell.h"
#include "platform/platform.h"
#include <imgui.h>
#include <SDL3/SDL.h>

/* Globals from various platform headers */
extern bool g_requestMacOff;
extern bool g_speedStopped;
extern bool g_runInBackground;
extern bool g_wantNotAutoSlow;

bool ControlOverlay::draw(UIState currentState, EmulatorShell *shell, ImGuiBackend *backend,
						  UIState &requestedState)
{
	bool stateChanged = false;
	requestedState = currentState;

	/* Full-viewport scrim */
	ImVec2 ds = ImGui::GetIO().DisplaySize;
	ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), ds, IM_COL32(0, 0, 0, 160));

	/* Centered panel */
	float panelW = 400, panelH = 320;
	ImVec2 panelPos((ds.x - panelW) * 0.5f, (ds.y - panelH) * 0.5f);
	ImGui::SetNextWindowPos(panelPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(panelW, panelH), ImGuiCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.16f, 0.95f));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
							 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

	if (ImGui::Begin("##OverlayPanel", nullptr, flags))
	{
		drawPrimaryControls(currentState, shell, backend, requestedState);
		if (requestedState != currentState) stateChanged = true;
		ImGui::Separator();
		drawAdvancedControls(backend);

		/* Flash feedback */
		if (flashMsg_ && SDL_GetTicks() < flashExpiry_)
		{
			ImGui::Spacing();
			ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", flashMsg_);
		}
		else
		{
			flashMsg_ = nullptr;
		}
	}
	ImGui::End();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();

	return stateChanged;
}

/* ── Primary controls ────────────────────────────────── */

void ControlOverlay::drawPrimaryControls(UIState currentState, EmulatorShell *shell,
										 ImGuiBackend *backend, UIState &requestedState)
{
	float btnW = 110, btnH = 32, sp = 8;

	/* Row 1: Insert Disk, Fullscreen, Scaling */
	if (ImGui::Button("Insert Disk", ImVec2(btnW, btnH)))
		backend->executeAction(UIAction::InsertDisk);
	ImGui::SameLine(0, sp);
	bool isFS = (currentState == UIState::Fullscreen);
	if (ImGui::Button(isFS ? "Windowed" : "Fullscreen", ImVec2(btnW, btnH)))
		requestedState = isFS ? UIState::Windowed : UIState::Fullscreen;
	ImGui::SameLine(0, sp);
	const char *scaleLabel =
		(backend->scalingMode() == ScalingMode::Integer) ? "Integer" : "Stretched";
	if (ImGui::Button(scaleLabel, ImVec2(btnW, btnH)))
		backend->executeAction(UIAction::ToggleScaling);

	ImGui::Spacing();

	/* Row 2: Speed */
	ImGui::Text("Speed:");
	ImGui::SameLine();
	static constexpr uint8_t kPresets[] = {1, 2, 4, 8, 16, 32, 0};
	static constexpr const char *kLabels[] = {"1x", "2x", "4x", "8x", "16x", "32x", "\xe2\x88\x9e"};
	for (int i = 0; i < 7; ++i)
	{
		if (i > 0) ImGui::SameLine(0, 4);
		bool selected = (g_speedValue == kPresets[i]);
		if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
		if (ImGui::SmallButton(kLabels[i])) g_speedValue = kPresets[i];
		if (selected) ImGui::PopStyleColor();
	}

	ImGui::Spacing();

	/* Row 3: Screenshot, Reboot, Power Off */
	if (ImGui::Button("Screenshot", ImVec2(btnW, btnH)))
		backend->executeAction(UIAction::Screenshot);
	ImGui::SameLine(0, sp);
	if (ImGui::Button("Reboot", ImVec2(btnW, btnH))) backend->executeAction(UIAction::Reboot);
	ImGui::SameLine(0, sp);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
	if (ImGui::Button("Power Off", ImVec2(btnW, btnH))) g_requestMacOff = true;
	ImGui::PopStyleColor();

	(void)shell;
}

/* ── Advanced (collapsible) ──────────────────────────── */

void ControlOverlay::drawAdvancedControls(ImGuiBackend *backend)
{
	if (ImGui::CollapsingHeader("Advanced"))
	{
		float btnW = 100, btnH = 28, sp = 8;

		/* Row: Interrupt, Filter, Stopped */
		if (ImGui::Button("Interrupt", ImVec2(btnW, btnH))) g_wantMacInterrupt = true;
		ImGui::SameLine(0, sp);

		bool isNearest = (backend->textureFilter() == TextureFilter::Nearest);
		const char *fLabel = isNearest ? "Filter: Near" : "Filter: Linear";
		if (ImGui::Button(fLabel, ImVec2(btnW, btnH)))
			backend->setTextureFilter(isNearest ? TextureFilter::Linear : TextureFilter::Nearest);
		ImGui::SameLine(0, sp);
		ImGui::Checkbox("Stopped", &g_speedStopped);

		/* Row: Background, AutoSlow */
		ImGui::Checkbox("Run in Background", &g_runInBackground);
		ImGui::SameLine(0, sp);
		bool autoSlow = !g_wantNotAutoSlow;
		if (ImGui::Checkbox("AutoSlow", &autoSlow)) g_wantNotAutoSlow = !autoSlow;

		ImGui::Spacing();
		drawAbout();
	}
}

/* ── About ───────────────────────────────────────────── */

void ControlOverlay::drawAbout()
{
	ImGui::Separator();
	ImGui::TextDisabled("maxivmac — Classic Macintosh Emulator");
	ImGui::TextDisabled("Licensed under GNU GPL v2");
	ImGui::TextLinkOpenURL("github.com/InvisibleUp/minivmac",
						   "https://github.com/InvisibleUp/minivmac");
}
