/*
	imgui_launcher.cpp — Pre-boot Launcher UI

	Shows .mac file entries as a card grid. One click boots.
*/

#include "platform/imgui_launcher.h"
#include "core/model_defs.h"
#include <imgui.h>
#include <cstdio>
#include <algorithm>

void Launcher::init(std::vector<MacFileEntry> entries)
{
	entries_ = std::move(entries);
	selectedIndex_ = -1;
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
			const char *title = "Maxi vMac";
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
	float cardH = 120.0f;

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
			/* Icon: colored square with initial letter */
			float iconSize = 32.0f;
			ImGui::SetCursorPosX((cardW - iconSize) * 0.5f);
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
			ImGui::Spacing();

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));

			/* Line 1: Name (centered, bold) */
			float nameW = ImGui::CalcTextSize(e.name.c_str()).x;
			ImGui::SetCursorPosX((cardW - nameW) * 0.5f);
			ImVec2 namePos = ImGui::GetCursorScreenPos();
			ImGui::Text("%s", e.name.c_str());
			ImGui::GetWindowDrawList()->AddText(ImVec2(namePos.x + 1.0f, namePos.y),
												IM_COL32(0, 0, 0, 255), e.name.c_str());

			/* Line 2: Model info from ModelDef */
			const ModelDef *def = ModelDefFor(e.model);
			if (def)
			{
				char line2[64];
				snprintf(line2, sizeof(line2), "%s · %u MB · %ux%u",
						 def->use68020 ? "68020" : "68000",
						 (def->ramASize + def->ramBSize) / (1024 * 1024), def->screen.width,
						 def->screen.height);
				float l2W = ImGui::CalcTextSize(line2).x;
				ImGui::SetCursorPosX((cardW - l2W) * 0.5f);
				ImGui::Text("%s", line2);
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
				const char *reason = e.validationError.c_str();
				float errW = ImGui::CalcTextSize(reason).x;
				ImGui::SetCursorPosX((cardW - errW) * 0.5f);
				ImGui::Text("%s", reason);
				ImGui::PopStyleColor();
			}

			ImGui::PopStyleColor(); /* black text */
		}

		bool hovered = ImGui::IsWindowHovered();
		ImGui::EndChild();

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
