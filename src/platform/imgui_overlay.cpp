/*
	imgui_overlay.cpp — Control overlay (Ctrl key)
*/

#include "platform/imgui_overlay.h"
#include "platform/imgui_backend.h"
#include "platform/emulator_shell.h"
#include "platform/platform.h"
#include <imgui.h>

/* Globals from various platform headers — declared here to avoid
   pulling in osglu_common.h which has heavy include dependencies. */
extern bool g_requestMacOff;
extern bool g_requestInsertDisk;
extern uint8_t g_requestIthDisk;
extern bool g_wantMagnify;
extern bool g_wantFullScreen;
extern bool g_speedStopped;
extern bool g_runInBackground;
extern bool g_wantNotAutoSlow;

bool ControlOverlay::draw(UIState currentState, EmulatorShell* shell,
	ImGuiBackend* backend, UIState& requestedState)
{
	bool stateChanged = false;
	requestedState = currentState;

	/* Semi-transparent full-viewport overlay background */
	ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(displaySize);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.55f));
	ImGuiWindowFlags bgFlags = ImGuiWindowFlags_NoDecoration
		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs;
	ImGui::Begin("##OverlayBg", nullptr, bgFlags);
	ImGui::End();
	ImGui::PopStyleColor();

	/* Centered overlay panel */
	float panelW = 400;
	float panelH = 320;
	ImVec2 panelPos((displaySize.x - panelW) * 0.5f,
	                (displaySize.y - panelH) * 0.5f);
	ImGui::SetNextWindowPos(panelPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(panelW, panelH), ImGuiCond_Always);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.16f, 0.95f));
	ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoDecoration
		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoSavedSettings;

	if (ImGui::Begin("##OverlayPanel", nullptr, panelFlags)) {
		if (ImGui::BeginTabBar("OverlayTabs")) {
			if (ImGui::BeginTabItem("Machine")) {
				drawMachineTab(shell);
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Display")) {
				drawDisplayTab(currentState, backend, requestedState);
				if (requestedState != currentState)
					stateChanged = true;
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Speed")) {
				drawSpeedTab();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Advanced")) {
				drawAdvancedTab(currentState, requestedState);
				if (requestedState != currentState)
					stateChanged = true;
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
	}
	ImGui::End();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();

	return stateChanged;
}

/* ── Machine tab ─────────────────────────────────────── */

void ControlOverlay::drawMachineTab(EmulatorShell* shell)
{
	ImGui::Spacing();

	float btnW = 100;
	float btnH = 40;
	float spacing = 12;

	/* Row 1: Insert Disk, Eject */
	if (ImGui::Button("Insert Disk", ImVec2(btnW, btnH))) {
		g_requestInsertDisk = true;
	}
	ImGui::SameLine(0, spacing);
	/* Eject all drives — individual per-drive only possible if we
	   expose which drives are loaded (future improvement) */
	if (ImGui::Button("Eject All", ImVec2(btnW, btnH))) {
		/* Request eject for drives 1-6 */
		for (int i = 1; i <= 6; ++i) {
			g_requestIthDisk = 0; /* TODO: per-drive eject */
		}
	}

	ImGui::Spacing();

	/* Row 2: Interrupt, Reboot, Power Off */
	if (ImGui::Button("Interrupt", ImVec2(btnW, btnH))) {
		g_wantMacInterrupt = true;
	}
	ImGui::SameLine(0, spacing);
	if (ImGui::Button("Reboot", ImVec2(btnW, btnH))) {
		g_wantMacReset = true;
	}
	ImGui::SameLine(0, spacing);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
	if (ImGui::Button("Power Off", ImVec2(btnW, btnH))) {
		g_requestMacOff = true;
	}
	ImGui::PopStyleColor();

	ImGui::Spacing();

	/* Row 3: Screenshot */
	if (ImGui::Button("Screenshot", ImVec2(btnW, btnH))) {
		/* TODO: capture framebuffer to clipboard */
		(void)shell;
	}
}

/* ── Display tab ─────────────────────────────────────── */

void ControlOverlay::drawDisplayTab(UIState currentState,
	ImGuiBackend* backend, UIState& requestedState)
{
	ImGui::Spacing();
	ImGui::Text("Zoom");
	ImGui::SameLine(80);

	/* Zoom: maps to g_wantMagnify. For now, simple 1x/2x toggle
	   since the backend uses windowScale_. */
	bool magnified = g_wantMagnify;
	if (ImGui::RadioButton("1x", !magnified)) {
		g_wantMagnify = false;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("2x", magnified)) {
		g_wantMagnify = true;
	}

	ImGui::Spacing();

	/* Filter toggle */
	ImGui::Text("Filter");
	ImGui::SameLine(80);
	bool isNearest = (backend->textureFilter() == TextureFilter::Nearest);
	if (ImGui::RadioButton("Nearest", isNearest)) {
		backend->setTextureFilter(TextureFilter::Nearest);
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Linear", !isNearest)) {
		backend->setTextureFilter(TextureFilter::Linear);
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	/* Fullscreen toggle */
	bool isFullscreen = (currentState == UIState::Fullscreen);
	if (ImGui::Button(isFullscreen ? "Windowed" : "Fullscreen",
		ImVec2(120, 36)))
	{
		requestedState = isFullscreen ? UIState::Windowed : UIState::Fullscreen;
	}
}

/* ── Speed tab ───────────────────────────────────────── */

void ControlOverlay::drawSpeedTab()
{
	ImGui::Spacing();
	ImGui::Text("Emulation Speed");
	ImGui::Spacing();

	struct SpeedOption { const char* label; uint8_t value; };
	static const SpeedOption speeds[] = {
		{ "1x",        1 },
		{ "2x",        2 },
		{ "4x",        4 },
		{ "8x",        8 },
		{ "16x",      16 },
		{ "32x",      32 },
		{ "Unlimited", 0 },
	};

	for (const auto& s : speeds) {
		if (ImGui::RadioButton(s.label, g_speedValue == s.value)) {
			g_speedValue = s.value;
		}
		ImGui::SameLine();
	}
	ImGui::NewLine();

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::Checkbox("Stopped", &g_speedStopped);
	ImGui::Checkbox("Run in Background", &g_runInBackground);

	bool autoSlow = !g_wantNotAutoSlow;
	if (ImGui::Checkbox("AutoSlow", &autoSlow)) {
		g_wantNotAutoSlow = !autoSlow;
	}
}

/* ── Advanced tab ────────────────────────────────────── */

void ControlOverlay::drawAdvancedTab(UIState currentState,
	UIState& requestedState)
{
	ImGui::Spacing();

	bool isDeveloper = (currentState == UIState::Developer);
	if (ImGui::Button(isDeveloper ? "Exit Developer Mode" : "Developer Mode",
		ImVec2(200, 36)))
	{
		requestedState = isDeveloper ? UIState::Windowed : UIState::Developer;
	}
	if (!isDeveloper) {
		ImGui::SameLine();
		ImGui::TextDisabled("Debug tools");
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::Text("maxivmac");
	ImGui::TextDisabled("A modernisation of the Mini vMac emulator");
	ImGui::TextDisabled("github.com/InvisibleUp/minivmac");
	ImGui::TextDisabled("Copyright 2001-2024 Paul C. Pratt et al.");
	ImGui::TextDisabled("Licensed under GPL v2");
}
