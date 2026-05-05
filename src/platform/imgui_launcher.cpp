/*
	imgui_launcher.cpp — Pre-boot Launcher UI

	Shows .mac file entries as a card grid. One click boots.
*/

#include "platform/imgui_launcher.h"
#include "platform/platform_config.h"
#include "core/model_defs.h"
#include <imgui.h>
#include <cstdio>
#include <algorithm>

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

Launcher::~Launcher()
{
	for (auto tex : textures_)
		if (tex) glDeleteTextures(1, &tex);
}

void Launcher::init(std::vector<MacFileEntry> entries)
{
	entries_ = std::move(entries);
	selectedIndex_ = -1;
}

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

const MacFileEntry *Launcher::draw()
{
	ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(displaySize);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
							 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
							 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.78f, 0.78f, 0.78f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(40, 30));

	const MacFileEntry *result = nullptr;

	if (ImGui::Begin("##Launcher", nullptr, flags))
	{
		/* Title */
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
		{
			const char *title = "maxivmac";
			float titleW = ImGui::CalcTextSize(title).x;
			ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - titleW) * 0.5f);
			ImGui::Text("%s", title);
		}
		ImGui::PopStyleColor();
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.3f, 0.3f, 0.3f, 1));
		ImGui::Separator();
		ImGui::PopStyleColor();
		ImGui::Spacing();
		ImGui::Spacing();

		if (entries_.empty())
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.3f, 0.3f, 1));
			ImGui::Text("No .mac files found in data/macs/");
			ImGui::PopStyleColor();
		}
		else
		{
			result = drawCards();
		}

		/* Version — bottom right */
		{
			const char *ver = MAXIVMAC_VERSION;
			ImVec2 textSize = ImGui::CalcTextSize(ver);
			ImVec2 winSize = ImGui::GetWindowSize();
			ImVec2 pad = ImGui::GetStyle().WindowPadding;
			ImGui::SetCursorPos(
				ImVec2(winSize.x - pad.x - textSize.x, winSize.y - pad.y - textSize.y));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
			ImGui::TextUnformatted(ver);
			ImGui::PopStyleColor();
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();

	return result;
}

const MacFileEntry *Launcher::drawCards()
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
		const auto &e = entries_[i];
		bool valid = e.romAvailable && e.allDisksAvailable;

		if (col > 0) ImGui::SameLine(0, gap);

		ImGui::PushID(i);

		if (!valid) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.35f);

		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
		ImGui::BeginChild("card", ImVec2(cardW, cardH), ImGuiChildFlags_None,
						  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		{
			/* Icon: texture if available, else colored square with initial */
			float iconSize = 64.0f;
			ImGui::SetCursorPosX((cardW - iconSize) * 0.5f);

			if (i < (int)textures_.size() && textures_[i])
			{
				ImGui::Image((ImTextureID)(uintptr_t)textures_[i], ImVec2(iconSize, iconSize));
			}
			else
			{
				ImVec2 iconPos = ImGui::GetCursorScreenPos();
				ImU32 iconColor = valid ? IM_COL32(100, 149, 237, 255) : IM_COL32(80, 80, 80, 255);
				ImGui::GetWindowDrawList()->AddRectFilled(
					iconPos, ImVec2(iconPos.x + iconSize, iconPos.y + iconSize), iconColor, 6.0f);
				char initial[2] = {e.name.empty() ? '?' : e.name[0], 0};
				ImVec2 letterSize = ImGui::CalcTextSize(initial);
				ImGui::GetWindowDrawList()->AddText(
					ImVec2(iconPos.x + (iconSize - letterSize.x) * 0.5f,
						   iconPos.y + (iconSize - letterSize.y) * 0.5f),
					IM_COL32(255, 255, 255, 255), initial);
				ImGui::Dummy(ImVec2(iconSize, iconSize));
			}
			ImGui::Spacing();

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));

			/* Line 1: Name (centered, bold) */
			float nameW = ImGui::CalcTextSize(e.name.c_str()).x;
			ImGui::SetCursorPosX((cardW - nameW) * 0.5f);
			ImVec2 namePos = ImGui::GetCursorScreenPos();
			ImGui::Text("%s", e.name.c_str());
			ImGui::GetWindowDrawList()->AddText(ImVec2(namePos.x + 1.0f, namePos.y),
												IM_COL32(0, 0, 0, 255), e.name.c_str());

			/* Line 2+3: Model info from ModelDef (two lines) */
			const ModelDef *def = ModelDefFor(e.model);
			if (def)
			{
				char cpuLine[32];
				snprintf(cpuLine, sizeof(cpuLine), "%s · %u MB", def->use68020 ? "68020" : "68000",
						 (def->ramASize + def->ramBSize) / (1024 * 1024));
				float cpuW = ImGui::CalcTextSize(cpuLine).x;
				ImGui::SetCursorPosX((cardW - cpuW) * 0.5f);
				ImGui::Text("%s", cpuLine);

				char resLine[16];
				snprintf(resLine, sizeof(resLine), "%ux%u", def->screen.width, def->screen.height);
				float resW = ImGui::CalcTextSize(resLine).x;
				ImGui::SetCursorPosX((cardW - resW) * 0.5f);
				ImGui::Text("%s", resLine);
			}

			/* Line 3: Boot disk filename or validation error */
			if (valid && !e.disks.empty())
			{
				const char *diskName = e.disks[0].c_str();
				float l3W = ImGui::CalcTextSize(diskName).x;
				ImGui::SetCursorPosX((cardW - l3W) * 0.5f);
				ImGui::Text("%s", diskName);
			}
			else if (!valid)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.2f, 0.2f, 1));
				const char *shortLabel = "unavailable";
				float errW = ImGui::CalcTextSize(shortLabel).x;
				ImGui::SetCursorPosX((cardW - errW) * 0.5f);
				ImGui::Text("%s", shortLabel);
				ImGui::PopStyleColor();
			}

			ImGui::PopStyleColor(); /* black text */
		}

		bool hovered = ImGui::IsWindowHovered();
		ImGui::EndChild();

		if (hovered && !valid && !e.validationError.empty())
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

		if (hovered && valid)
		{
			ImVec2 rMin = ImGui::GetItemRectMin();
			ImVec2 rMax = ImGui::GetItemRectMax();
			ImGui::GetWindowDrawList()->AddRectFilled(rMin, rMax, IM_COL32(255, 255, 255, 50),
													  8.0f);
		}

		ImGui::PopStyleVar(2);	/* ChildRounding, ChildBorderSize */
		ImGui::PopStyleColor(); /* ChildBg */

		if (!valid) ImGui::PopStyleVar(); /* Alpha */

		if (hovered && valid && ImGui::IsMouseClicked(0))
		{
			ImGui::PopID();
			ImGui::End();
			ImGui::PopStyleVar();
			ImGui::PopStyleColor();
			return &entries_[i];
		}

		ImGui::PopID();

		col = (col + 1) % cols;
		if (col == 0)
		{
			ImGui::Spacing();
			float nextOffsetX = (avail - totalW) * 0.5f;
			if (nextOffsetX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + nextOffsetX);
		}
	}

	return nullptr;
}
