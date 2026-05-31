/*
	imgui_overlay.cpp — Control overlay (Ctrl key)

	Single flat panel with primary controls always visible
	and an advanced section collapsed by default.
*/

#include "platform/imgui_overlay.h"
#include "platform/imgui_backend.h"
#include "platform/emulator_shell.h"
#include "platform/platform.h"
#include "platform/platform_config.h"
#include "core/extn_system.h"
#include <imgui.h>
#include <SDL3/SDL.h>

/* Globals from various platform headers */
extern bool g_requestMacOff;
extern bool g_speedStopped;
extern bool g_runInBackground;
extern bool g_wantNotAutoSlow;

void ControlOverlay::flash(const char *msg, uint32_t ms)
{
	flashMsg_ = msg;
	flashExpiry_ = SDL_GetTicks() + ms;
}

bool ControlOverlay::draw(UIState currentState, EmulatorShell *shell, ImGuiBackend *backend,
						  UIState &requestedState)
{
	hoverHelp_ = nullptr;
	bool stateChanged = false;
	requestedState = currentState;

	/* Full-viewport scrim */
	ImVec2 ds = ImGui::GetIO().DisplaySize;
	ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), ds, IM_COL32(0, 0, 0, 160));

	/* Centered panel — capped so a 16 px border is preserved on all sides */
	const float kBorder = 16.0f;
	float panelW = std::min(480.0f, ds.x - 2.0f * kBorder);
	float panelH = std::min(360.0f, ds.y - 2.0f * kBorder);
	ImVec2 panelPos((ds.x - panelW) * 0.5f, (ds.y - panelH) * 0.5f);
	ImGui::SetNextWindowPos(panelPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(panelW, panelH), ImGuiCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.16f, 0.95f));

	/* NoDecoration includes NoScrollbar; replace it with its other parts so
	   a scrollbar can appear if the panel is shorter than its content. */
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
							 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
							 ImGuiWindowFlags_NoSavedSettings;

	if (ImGui::Begin("##OverlayPanel", nullptr, flags))
	{
		drawPrimaryControls(currentState, shell, backend, requestedState);
		if (requestedState != currentState) stateChanged = true;

		/* Info area: flash → hover help → about */
		ImGui::SeparatorText("Information");
		if (flashMsg_ && SDL_GetTicks() < flashExpiry_)
		{
			ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", flashMsg_);
		}
		else
		{
			flashMsg_ = nullptr;
			if (hoverHelp_)
				ImGui::TextWrapped("%s", hoverHelp_);
			else
				drawAbout();
		}

		/* Bottom row: Interrupt | Reboot | Power Off — pinned to panel bottom */
		{
			const float btnH = 32, sp = 8;
			float sepY = ImGui::GetContentRegionMax().y - btnH
						 - ImGui::GetStyle().ItemSpacing.y - 1.0f;
			ImGui::SetCursorPosY(sepY - ImGui::GetStyle().ItemSpacing.y);
			ImGui::Separator();
			ImGui::SetCursorPosY(ImGui::GetContentRegionMax().y - btnH);
			const float btn3W = (ImGui::GetContentRegionAvail().x - 2 * sp) / 3.0f;
			if (ImGui::Button("Interrupt (N)", ImVec2(btn3W, btnH)))
				backend->executeAction(UIAction::Interrupt);
			if (!hoverHelp_ && ImGui::IsItemHovered())
				hoverHelp_ = "Send a Non-Maskable Interrupt to the Mac's 68000 CPU. If MacsBug or another debugger is installed in the guest, this drops you into it for low-level debugging.";
			ImGui::SameLine(0, sp);
			if (ImGui::Button("Reboot (R)", ImVec2(btn3W, btnH)))
				backend->executeAction(UIAction::Reboot);
			if (!hoverHelp_ && ImGui::IsItemHovered())
				hoverHelp_ = "Restart the emulated Mac, like pressing the programmer's reset button. Mounted disk images are ejected; the Mac reboots from scratch.";
			ImGui::SameLine(0, sp);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
			if (ImGui::Button("Power Off (Q)", ImVec2(btn3W, btnH)))
				backend->executeAction(UIAction::PowerOff);
			ImGui::PopStyleColor();
			if (!hoverHelp_ && ImGui::IsItemHovered())
				hoverHelp_ = "Cut power to the emulated Mac immediately, like pulling the plug. Any unsaved work in the guest will be lost. Use Reboot for a clean restart.";
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
	float avail = ImGui::GetContentRegionAvail().x;
	float btnH = 32, sp = 8;
	float btn2W = (avail - sp) * 0.5f;

	/* Row 1: Insert Disk | Screenshot */
	if (ImGui::Button("Insert Disk (I)", ImVec2(btn2W, btnH)))
		backend->executeAction(UIAction::InsertDisk);
	if (!hoverHelp_ && ImGui::IsItemHovered())
		hoverHelp_ = "Load a disk image into the emulated Mac. The volume appears on the Finder desktop, just as if you inserted a real floppy or mounted a hard disk.";
	ImGui::SameLine(0, sp);
	if (ImGui::Button("Screenshot (S)", ImVec2(btn2W, btnH)))
		backend->executeAction(UIAction::Screenshot);
	if (!hoverHelp_ && ImGui::IsItemHovered())
		hoverHelp_ = "Copy the Mac screen to the clipboard as a PNG (S). Press Shift+S to open a Save dialog and write it to a file on disk.";

	/* Row 2: Fullscreen | Zoom */
	bool isFS = (currentState == UIState::Fullscreen);
	if (ImGui::Button(isFS ? "Windowed (F)" : "Fullscreen (F)", ImVec2(btn2W, btnH)))
		requestedState = isFS ? UIState::Windowed : UIState::Fullscreen;
	if (!hoverHelp_ && ImGui::IsItemHovered())
		hoverHelp_ = "Toggle between fullscreen and windowed display. Fullscreen gives the Mac the whole screen; windowed lets you use host apps alongside the emulator.";
	ImGui::SameLine(0, sp);
	if (ImGui::Button("Zoom (Z)", ImVec2(btn2W, btnH))) backend->executeAction(UIAction::Zoom);
	if (!hoverHelp_ && ImGui::IsItemHovered())
		hoverHelp_ = "Cycle the window through integer scales (1×, 2×, 3×, … up to the largest that fits the display, then back to 1×). Each press snaps to the next size and centres the window.";

	ImGui::SeparatorText("Rendering");

	/* Row 3: Pixel Perfect | Smooth filter */
	{
		bool pixelPerfect = (backend->scalingMode() == ScalingMode::PixelPerfect);
		if (ImGui::Checkbox("Pixel Perfect (P)", &pixelPerfect))
			backend->setScalingMode(pixelPerfect ? ScalingMode::PixelPerfect : ScalingMode::Stretched);
		if (!hoverHelp_ && ImGui::IsItemHovered())
			hoverHelp_ = "Choose how the Mac screen fills the window. Pixel Perfect scales to the largest exact integer multiple (sharp pixels); unchecked stretches to fill all available space.";
	}
	ImGui::SameLine(0, sp * 3);
	{
		bool smooth = (backend->textureFilter() == TextureFilter::Linear);
		if (ImGui::Checkbox("Smooth", &smooth))
			backend->setTextureFilter(smooth ? TextureFilter::Linear : TextureFilter::Nearest);
		if (!hoverHelp_ && ImGui::IsItemHovered())
			hoverHelp_ = "Toggle texture filtering. Smooth (Linear) softens edges for a less pixelated appearance. Uncheck for sharp, blocky pixels true to the original hardware.";
	}
	ImGui::SameLine(0, sp * 3);
	{
		bool crt = (backend->renderStyle() == RenderStyle::CRT);
		if (ImGui::Checkbox("CRT", &crt))
			backend->setRenderStyle(crt ? RenderStyle::CRT : RenderStyle::Plain);
		if (!hoverHelp_ && ImGui::IsItemHovered())
			hoverHelp_ = "Apply a CRT monitor effect: scanline gaps, barrel distortion (curved glass), and corner vignette. Evokes the look of the original Mac's built-in display.";
	}

	ImGui::SeparatorText("Speed");

	/* Speed row */
	{
		bool paused = g_speedStopped;
		if (paused) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
		if (ImGui::SmallButton("Pause")) g_speedStopped = !g_speedStopped;
		if (paused) ImGui::PopStyleColor();
		if (!hoverHelp_ && ImGui::IsItemHovered())
			hoverHelp_ = "Freeze the emulated Mac completely: no CPU cycles run, no sound plays, and the clock stops. Click Pause again to resume exactly where you left off.";
	}
	static constexpr uint8_t kPresets[] = {0, 1, 2, 3, 4, 5, (uint8_t)-1};
	static constexpr const char *kLabels[] = {"1x", "2x", "4x", "8x", "16x", "32x", "Max"};
	for (int i = 0; i < 7; ++i)
	{
		ImGui::SameLine(0, 4);
		bool selected = (!g_speedStopped && g_speedValue == kPresets[i]);
		if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
		if (ImGui::SmallButton(kLabels[i])) { g_speedValue = kPresets[i]; g_speedStopped = false; }
		if (selected) ImGui::PopStyleColor();
		if (!hoverHelp_ && ImGui::IsItemHovered())
			hoverHelp_ = "Set how fast the emulated Mac runs. 1x is authentic vintage speed. Higher values fast-forward the guest; Max runs as fast as your host CPU allows.";
	}

	/* Background behaviour: [Pause][Slow][Fast] */
	ImGui::SameLine(0, 12);
	ImGui::Text("Background:");
	ImGui::SameLine(0, 4);
	{
		static constexpr const char *kBgLabels[] = {"Pause##bg", "Slow##bg", "Fast##bg"};
		static constexpr const char *kBgHelp[] = {
			"Pause the Mac when the window loses focus.",
			"Reduce emulation speed automatically when the window loses focus (AutoSlow).",
			"Keep the Mac running at full speed even when the window loses focus.",
		};
		int bgState = g_runInBackground ? (g_wantNotAutoSlow ? 2 : 1) : 0;
		for (int i = 0; i < 3; ++i)
		{
			if (i > 0) ImGui::SameLine(0, 4);
			bool sel = (bgState == i);
			if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
			if (ImGui::SmallButton(kBgLabels[i]))
			{
				if (i == 0)      { g_runInBackground = false; }
				else if (i == 1) { g_runInBackground = true; g_wantNotAutoSlow = false; }
				else             { g_runInBackground = true; g_wantNotAutoSlow = true; }
			}
			if (sel) ImGui::PopStyleColor();
			if (!hoverHelp_ && ImGui::IsItemHovered())
				hoverHelp_ = kBgHelp[i];
		}
	}

	ImGui::Spacing();
	ImGui::Spacing();

	(void)shell;
}

/* ── Advanced (collapsible) ──────────────────────────── */

void ControlOverlay::drawAdvancedControls(ImGuiBackend * /*backend*/)
{
	// AutoSlow moved to the speed row in drawPrimaryControls.
}

/* ── About ───────────────────────────────────────────── */

static std::string formatSystemVersion(int bcd)
{
	int major = (bcd >> 8) & 0xFF;
	int minor = (bcd >> 4) & 0x0F;
	int patch = bcd & 0x0F;
	char buf[16];
	if (patch)
		snprintf(buf, sizeof(buf), "%d.%d.%d", major, minor, patch);
	else
		snprintf(buf, sizeof(buf), "%d.%d", major, minor);
	return buf;
}

static std::string machineTypeName(int type)
{
	switch (type)
	{
		case -2:
			return "Mac XL";
		case -1:
			return "Mac 64K ROM";
		case 1:
			return "Mac 512Ke";
		case 2:
			return "Mac Plus";
		case 3:
			return "Mac SE";
		case 4:
			return "Mac II";
		case 5:
			return "Mac IIx";
		case 6:
			return "Mac IIcx";
		case 7:
			return "Mac SE/30";
		case 8:
			return "Mac Portable";
		case 9:
			return "Mac IIci";
		case 11:
			return "Mac IIfx";
		default:
		{
			char buf[32];
			snprintf(buf, sizeof(buf), "Mac (type %d)", type);
			return buf;
		}
	}
}

void ControlOverlay::drawAbout()
{
	ImGui::TextDisabled("maxivmac %s", MAXIVMAC_VERSION);

	{
		const auto &info = ExtnSystemInitInfo();
		if (!info.loaded())
		{
			ImGui::TextDisabled("INIT: not loaded");
		}
		else if (info.isStale())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "%s · System %s · INIT %s",
							   machineTypeName(info.machineType()).c_str(),
							   formatSystemVersion(info.systemVersion()).c_str(),
							   std::string(info.version()).c_str());
		}
		else
		{
			ImGui::Text("%s · System %s · INIT %s", machineTypeName(info.machineType()).c_str(),
						formatSystemVersion(info.systemVersion()).c_str(),
						std::string(info.version()).c_str());
		}
	}

	ImGui::TextDisabled("Licensed under GNU GPL v2");
	ImGui::TextLinkOpenURL("github.com/fstark/maxivmac",
						   "https://github.com/fstark/maxivmac");
}
