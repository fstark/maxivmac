/*
	imgui_launcher.cpp — Pre-boot Launcher UI

	Shows .mac file entries as a card grid. One click boots.
*/

#include "platform/imgui_launcher.h"
#include "platform/platform_config.h"
#include "core/model_defs.h"
#include <imgui.h>
#include <cstdio>

#define GL_SILENCE_DEPRECATION
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
#pragma GCC diagnostic pop

/* Draw text horizontally centered within the given width. */
static void drawCenteredText(const char *text, float width)
{
	float textW = ImGui::CalcTextSize(text).x;
	ImGui::SetCursorPosX((width - textW) * 0.5f);
	ImGui::Text("%s", text);
}

Launcher::~Launcher()
{
	for (auto tex : textures_)
		if (tex) glDeleteTextures(1, &tex);
}

/* Store parsed .mac entries for display. */
void Launcher::init(std::vector<MacFileEntry> entries)
{
	entries_ = std::move(entries);
}

/* Load PNG icon textures for entries that have an icon path.
   Must be called after the GL context is ready. */
void Launcher::loadTextures()
{
	textures_.resize(entries_.size(), 0);
	for (size_t i = 0; i < entries_.size(); ++i)
	{
		const auto &path = entries_[i].iconPath;
		if (path.empty()) continue;

		int w, h, channels;
		unsigned char *pixels = stbi_load(path.c_str(), &w, &h, &channels, 4);
		if (!pixels) continue;

		GLuint tex = 0;
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		stbi_image_free(pixels);

		textures_[i] = tex;
	}
}

/* Main draw entry point. Renders the full-screen launcher window
   with title, card grid, version badge, and info popup. */
void Launcher::draw()
{
	selectedMac_ = nullptr;

	/* Draw the info popup first so its dismiss check runs before
	   drawInfoButton() can set displayedMac_ on the same frame. */
	drawInfoPopup();

	ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(displaySize);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
							 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
							 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.78f, 0.78f, 0.78f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(40, 30));

	if (ImGui::Begin("##Launcher", nullptr, flags))
	{
		drawTitle();

		if (entries_.empty())
			drawEmptyState();
		else
			drawCards();

		drawVersion();
	}
	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();
}

/* Draw the centered "maxivmac" title and separator. */
void Launcher::drawTitle()
{
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
	drawCenteredText("maxivmac", ImGui::GetContentRegionAvail().x);
	ImGui::PopStyleColor();
	ImGui::Spacing();
	ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.3f, 0.3f, 0.3f, 1));
	ImGui::Separator();
	ImGui::PopStyleColor();
	ImGui::Spacing();
	ImGui::Spacing();
}

/* Draw the version string in the bottom-right corner. */
void Launcher::drawVersion()
{
	const char *ver = MAXIVMAC_VERSION;
	ImVec2 textSize = ImGui::CalcTextSize(ver);
	ImVec2 winSize = ImGui::GetWindowSize();
	ImVec2 pad = ImGui::GetStyle().WindowPadding;
	ImGui::SetCursorPos(ImVec2(winSize.x - pad.x - textSize.x, winSize.y - pad.y - textSize.y));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
	ImGui::TextUnformatted(ver);
	ImGui::PopStyleColor();
}

/* Show a message when no .mac files are found. */
void Launcher::drawEmptyState()
{
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.3f, 0.3f, 1));
	ImGui::Text("No .mac files found in data/macs/");
	ImGui::PopStyleColor();
}

/* Lay out the responsive card grid and draw each entry. */
void Launcher::drawCards()
{
	float avail = ImGui::GetContentRegionAvail().x;
	int cols = 4;
	float gap = 16.0f;
	float cardW = (avail - (cols - 1) * gap) / cols;
	if (cardW < 120.0f)
	{
		cols = 3;
		cardW = (avail - (cols - 1) * gap) / cols;
	}
	float cardH = 135.0f;

	float totalW = cols * cardW + (cols - 1) * gap;
	float offsetX = (avail - totalW) * 0.5f;
	if (offsetX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

	int col = 0;
	for (int i = 0; i < (int)entries_.size(); ++i)
	{
		if (col > 0) ImGui::SameLine(0, gap);

		ImGui::PushID(i);
		drawCard(i, cardW, cardH);
		ImGui::PopID();

		col = (col + 1) % cols;
		if (col == 0)
		{
			ImGui::Spacing();
			if (offsetX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
		}
	}
}

/* Draw a single card: icon, text, hover effects, info button.
   Sets selectedMac_ if the card is clicked to boot. */
void Launcher::drawCard(int index, float cardW, float cardH)
{
	const auto &e = entries_[index];
	bool valid = e.romAvailable && e.allDisksAvailable;

	if (!valid) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.35f);

	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
	ImGui::BeginChild("card", ImVec2(cardW, cardH), ImGuiChildFlags_None,
					  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	{
		drawCardIcon(index, cardW, valid);
		ImGui::Spacing();
		drawCardText(e, cardW, valid);
	}

	bool hovered = ImGui::IsWindowHovered();
	ImGui::EndChild();

	bool infoClicked = hovered && drawInfoButton(index, cardW);

	if (hovered) drawCardHover(e, valid);

	ImGui::PopStyleVar(2);	/* ChildRounding, ChildBorderSize */
	ImGui::PopStyleColor(); /* ChildBg */

	if (!valid) ImGui::PopStyleVar(); /* Alpha */

	if (hovered && valid && !infoClicked && ImGui::IsMouseClicked(0))
		selectedMac_ = &entries_[index];
}

/* Draw hover effects: highlight for valid cards, error tooltip for invalid ones. */
void Launcher::drawCardHover(const MacFileEntry &e, bool valid)
{
	if (valid)
	{
		ImVec2 rMin = ImGui::GetItemRectMin();
		ImVec2 rMax = ImGui::GetItemRectMax();
		ImGui::GetWindowDrawList()->AddRectFilled(rMin, rMax, IM_COL32(255, 255, 255, 50), 8.0f);
	}
	else if (!e.validationError.empty())
	{
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.85f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
		ImGui::SetItemTooltip("%s", e.validationError.c_str());
		ImGui::PopStyleVar(3);
		ImGui::PopStyleColor(2);
	}
}

/* Draw the card icon: PNG texture if available, else a colored square with
   the first letter of the machine name. */
void Launcher::drawCardIcon(int index, float cardW, bool valid)
{
	float iconSize = 64.0f;
	ImGui::SetCursorPosX((cardW - iconSize) * 0.5f);

	if (index < (int)textures_.size() && textures_[index])
	{
		ImGui::Image((ImTextureID)(uintptr_t)textures_[index], ImVec2(iconSize, iconSize));
	}
	else
	{
		const auto &e = entries_[index];
		ImVec2 iconPos = ImGui::GetCursorScreenPos();
		ImU32 iconColor = valid ? IM_COL32(100, 149, 237, 255) : IM_COL32(80, 80, 80, 255);
		ImGui::GetWindowDrawList()->AddRectFilled(
			iconPos, ImVec2(iconPos.x + iconSize, iconPos.y + iconSize), iconColor, 6.0f);
		char initial[2] = {e.name.empty() ? '?' : e.name[0], 0};
		ImVec2 letterSize = ImGui::CalcTextSize(initial);
		ImGui::GetWindowDrawList()->AddText(ImVec2(iconPos.x + (iconSize - letterSize.x) * 0.5f,
												   iconPos.y + (iconSize - letterSize.y) * 0.5f),
											IM_COL32(255, 255, 255, 255), initial);
		ImGui::Dummy(ImVec2(iconSize, iconSize));
	}
}

/* Draw the card text lines: name (bold), CPU · RAM, resolution, boot disk
   or "unavailable" status. */
void Launcher::drawCardText(const MacFileEntry &e, float cardW, bool valid)
{
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));

	/* Name (centered, faux-bold via double draw) */
	float nameW = ImGui::CalcTextSize(e.name.c_str()).x;
	ImGui::SetCursorPosX((cardW - nameW) * 0.5f);
	ImVec2 namePos = ImGui::GetCursorScreenPos();
	ImGui::Text("%s", e.name.c_str());
	ImGui::GetWindowDrawList()->AddText(ImVec2(namePos.x + 1.0f, namePos.y), IM_COL32(0, 0, 0, 255),
										e.name.c_str());

	/* CPU · RAM and resolution from ModelDef */
	const ModelDef *def = ModelDefFor(e.model);
	if (def)
	{
		char cpuLine[32];
		snprintf(cpuLine, sizeof(cpuLine), "%s · %u MB", def->use68020 ? "68020" : "68000",
				 (def->ramASize + def->ramBSize) / (1024 * 1024));
		drawCenteredText(cpuLine, cardW);

		char resLine[16];
		snprintf(resLine, sizeof(resLine), "%ux%u", def->screen.width, def->screen.height);
		drawCenteredText(resLine, cardW);
	}

	/* Boot disk filename or validation error */
	if (valid && !e.disks.empty())
	{
		drawCenteredText(e.disks[0].c_str(), cardW);
	}
	else if (!valid)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.2f, 0.2f, 1));
		drawCenteredText("unavailable", cardW);
		ImGui::PopStyleColor();
	}

	ImGui::PopStyleColor(); /* black text */
}

/* Draw a small '?' icon in the top-right of the last card rect.
   Returns true if clicked, opening the info popup for that entry. */
bool Launcher::drawInfoButton(int cardIndex, float cardW)
{
	ImVec2 cardMin = ImGui::GetItemRectMin();
	float btnSize = 14.0f;
	float margin = 4.0f;
	ImVec2 btnPos(cardMin.x + cardW - btnSize - margin, cardMin.y + margin);
	ImVec2 btnEnd(btnPos.x + btnSize, btnPos.y + btnSize);

	ImVec2 mouse = ImGui::GetMousePos();
	bool btnHovered =
		mouse.x >= btnPos.x && mouse.x <= btnEnd.x && mouse.y >= btnPos.y && mouse.y <= btnEnd.y;

	ImU32 btnBg = btnHovered ? IM_COL32(0, 0, 0, 40) : IM_COL32(0, 0, 0, 0);
	ImU32 btnFg = btnHovered ? IM_COL32(0, 0, 0, 220) : IM_COL32(0, 0, 0, 160);

	auto *dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(btnPos, btnEnd, btnBg, 4.0f);
	const char *label = "?";
	ImVec2 labelSize = ImGui::CalcTextSize(label);
	dl->AddText(ImVec2(btnPos.x + (btnSize - labelSize.x) * 0.5f,
					   btnPos.y + (btnSize - labelSize.y) * 0.5f),
				btnFg, label);

	if (btnHovered && ImGui::IsMouseClicked(0))
	{
		displayedMac_ = &entries_[cardIndex];
		return true;
	}
	return false;
}

/* Show a centered panel with the raw .mac file contents.
   Opened by clicking '?', dismissed by clicking X or outside. */
void Launcher::drawInfoPopup()
{
	if (!displayedMac_) return;

	const auto &e = *displayedMac_;

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(400, 0));
	ImGui::SetNextWindowFocus();

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 12));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
							 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
							 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoNav |
							 ImGuiWindowFlags_NoScrollbar;

	bool closeRequested = false;
	ImGui::Begin("##MacInfo", nullptr, flags);

	/* Title */
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
	ImGui::TextUnformatted(e.name.c_str());
	ImGui::PopStyleColor();

	/* Close button */
	{
		ImVec2 winSize = ImGui::GetWindowSize();
		ImVec2 pad = ImGui::GetStyle().WindowPadding;
		ImGui::SameLine(winSize.x - pad.x - ImGui::CalcTextSize("X").x);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0.1f));
		if (ImGui::SmallButton("X")) closeRequested = true;
		ImGui::PopStyleColor(3);
	}

	ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.6f, 0.6f, 0.6f, 1));
	ImGui::Separator();
	ImGui::PopStyleColor();
	ImGui::Spacing();

	/* .mac file contents */
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.1f, 0.1f, 1));
	ImGui::TextUnformatted(e.rawContent.c_str());
	ImGui::PopStyleColor();

	ImGui::Spacing();
	ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.6f, 0.6f, 0.6f, 1));
	ImGui::Separator();
	ImGui::PopStyleColor();
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.45f, 1));
	ImGui::TextUnformatted(e.filePath.c_str());
	ImGui::PopStyleColor();

	bool windowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
	ImGui::End();
	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(2);

	/* Dismiss on click outside or X button */
	if (closeRequested || (!windowHovered && ImGui::IsMouseClicked(0))) displayedMac_ = nullptr;
}
