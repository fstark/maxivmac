/*
	imgui_model_selector.cpp — Pre-boot model selection UI
*/

#include "platform/imgui_model_selector.h"
#include "core/machine_config.h"
#include <imgui.h>
#include <algorithm>

/* ── Model metadata ──────────────────────────────────── */

struct ModelInfo
{
	MacModel model;
	const char *displayName;
	const char *cpu;
	const char *ram;
	const char *resolution;
};

static const ModelInfo kModelTable[] = {
	{MacModel::Twig43, "Twig43", "68000", "128 KB RAM", "512x342"},
	{MacModel::Twiggy, "Twiggy", "68000", "128 KB RAM", "512x342"},
	{MacModel::Mac128K, "Macintosh 128K", "68000", "128 KB RAM", "512x342"},
	{MacModel::Mac512Ke, "Macintosh 512Ke", "68000", "512 KB RAM", "512x342"},
	{MacModel::Plus, "Macintosh Plus", "68000", "4 MB RAM", "512x342"},
	{MacModel::Kanji, "Mac Plus Kanji", "68000", "4 MB RAM", "512x342"},
	{MacModel::SE, "Macintosh SE", "68000", "4 MB RAM", "512x342"},
	{MacModel::SEFDHD, "Macintosh SE FDHD", "68000", "4 MB RAM", "512x342"},
	{MacModel::Classic, "Macintosh Classic", "68000", "4 MB RAM", "512x342"},
	{MacModel::PB100, "PowerBook 100", "68000", "4 MB RAM", "640x400"},
	{MacModel::II, "Macintosh II", "68020", "8 MB RAM", "640x480 color"},
	{MacModel::IIx, "Macintosh IIx", "68030", "8 MB RAM", "640x480 color"},
};
static constexpr int kModelCount = sizeof(kModelTable) / sizeof(kModelTable[0]);

/* Valid RAM options per model (in MB). */
struct RAMOptions
{
	MacModel model;
	float options[6]; // 0-terminated
};

static const RAMOptions kRAMTable[] = {
	{MacModel::Twig43, {0.125f, 0}},	  {MacModel::Twiggy, {0.125f, 0}},
	{MacModel::Mac128K, {0.125f, 0}},	  {MacModel::Mac512Ke, {0.5f, 0}},
	{MacModel::Kanji, {1, 2.5f, 4, 0}},	  {MacModel::Plus, {1, 2.5f, 4, 0}},
	{MacModel::SE, {1, 2.5f, 4, 0}},	  {MacModel::SEFDHD, {1, 2.5f, 4, 0}},
	{MacModel::Classic, {1, 2.5f, 4, 0}}, {MacModel::PB100, {2, 4, 8, 0}},
	{MacModel::II, {1, 2, 4, 5, 8, 0}},	  {MacModel::IIx, {1, 2, 4, 5, 8, 0}},
};

static const float *GetRAMOptions(MacModel model)
{
	for (const auto &r : kRAMTable)
	{
		if (r.model == model) return r.options;
	}
	return nullptr;
}

static float GetDefaultRAM(MacModel model)
{
	MachineConfig mc = MachineConfigForModel(model);
	return (float)mc.ramSize() / (1024.0f * 1024.0f);
}

/* ── ModelSelector ───────────────────────────────────── */

void ModelSelector::init(const std::string &romDir)
{
	buildModelList(romDir);
	diskPaths_.resize(6);
}

void ModelSelector::buildModelList(const std::string &romDir)
{
	models_.clear();
	models_.reserve(kModelCount);

	for (int i = 0; i < kModelCount; ++i)
	{
		ModelEntry e;
		e.model = kModelTable[i].model;
		e.displayName = kModelTable[i].displayName;
		e.cpu = kModelTable[i].cpu;
		e.ram = kModelTable[i].ram;
		e.resolution = kModelTable[i].resolution;
		e.romPath = ResolveRomPath("", e.model, romDir);
		e.romAvailable = !e.romPath.empty();
		models_.push_back(e);
	}
}

ModelSelectorResult ModelSelector::draw()
{
	ModelSelectorResult result;

	ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(displaySize);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
							 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
							 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.78f, 0.78f, 0.78f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(40, 30));
	if (ImGui::Begin("##ModelSelector", nullptr, flags))
	{
		if (selectedIndex_ < 0)
		{
			drawModelGrid();
		}
		else
		{
			drawConfigPanel();
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();

	/* Check if Boot was clicked (flag set by drawConfigPanel) */
	if (bootClicked_ && selectedIndex_ >= 0 && selectedIndex_ < (int)models_.size())
	{
		const ModelEntry &entry = models_[selectedIndex_];
		result.accepted = true;
		result.config.model = entry.model;
		result.config.modelExplicit = true;
		result.config.romPath = entry.romPath;

		/* RAM */
		const float *ramOpts = GetRAMOptions(entry.model);
		if (ramOpts && ramOpts[ramChoice_] > 0)
		{
			result.config.ramMB = (uint32_t)(ramOpts[ramChoice_] + 0.01f);
			/* Handle fractional MB (e.g. 2.5 MB = 2560 KB) */
			if (ramOpts[ramChoice_] < 1.0f)
			{
				result.config.ramMB = 0; // will set via raw bytes later
			}
		}

		/* Speed: 0=1x, 1=2x, 2=4x, 3=8x, 4=unlimited(0) */
		static const int speedMap[] = {1, 2, 4, 8, 0};
		result.config.speed = speedMap[speedChoice_];

		/* Disks */
		for (const auto &dp : diskPaths_)
		{
			if (!dp.empty()) result.config.diskPaths.push_back(dp);
		}

		bootClicked_ = false;
	}

	return result;
}

/* ── Grid view ───────────────────────────────────────── */

void ModelSelector::drawModelGrid()
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
	for (int i = 0; i < (int)models_.size(); ++i)
	{
		auto &entry = models_[i];

		if (col > 0) ImGui::SameLine(0, gap);

		ImGui::PushID(i);

		bool disabled = !entry.romAvailable;
		if (disabled) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.35f);

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
			ImU32 iconColor = disabled ? IM_COL32(80, 80, 80, 255) : IM_COL32(100, 149, 237, 255);
			ImGui::GetWindowDrawList()->AddRectFilled(
				iconPos, ImVec2(iconPos.x + iconSize, iconPos.y + iconSize), iconColor, 6.0f);
			char initial[2] = {entry.displayName[0], 0};
			ImVec2 letterSize = ImGui::CalcTextSize(initial);
			ImGui::GetWindowDrawList()->AddText(
				ImVec2(iconPos.x + (iconSize - letterSize.x) * 0.5f,
					   iconPos.y + (iconSize - letterSize.y) * 0.5f),
				IM_COL32(255, 255, 255, 255), initial);
			ImGui::Dummy(ImVec2(iconSize, iconSize));
			ImGui::Spacing();

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));

			/* Line 1: Model name (centered, bold) */
			float nameW = ImGui::CalcTextSize(entry.displayName).x;
			ImGui::SetCursorPosX((cardW - nameW) * 0.5f);
			ImVec2 namePos = ImGui::GetCursorScreenPos();
			ImGui::Text("%s", entry.displayName);
			/* Simulate bold: overdraw offset 1px right */
			ImGui::GetWindowDrawList()->AddText(ImVec2(namePos.x + 1.0f, namePos.y),
												IM_COL32(0, 0, 0, 255), entry.displayName);

			/* Line 2: CPU + RAM (centered, black) */
			char line2[64];
			snprintf(line2, sizeof(line2), "%s, %s", entry.cpu, entry.ram);
			float l2W = ImGui::CalcTextSize(line2).x;
			ImGui::SetCursorPosX((cardW - l2W) * 0.5f);
			ImGui::Text("%s", line2);

			/* Line 3: Resolution (centered, black) */
			float l3W = ImGui::CalcTextSize(entry.resolution).x;
			ImGui::SetCursorPosX((cardW - l3W) * 0.5f);
			ImGui::Text("%s", entry.resolution);

			ImGui::PopStyleColor(); /* black text */
		}

		bool hovered = ImGui::IsWindowHovered();
		ImGui::EndChild();

		if (hovered && !disabled)
		{
			ImVec2 rMin = ImGui::GetItemRectMin();
			ImVec2 rMax = ImGui::GetItemRectMax();
			ImGui::GetWindowDrawList()->AddRectFilled(rMin, rMax, IM_COL32(255, 255, 255, 50),
													  8.0f);
		}

		ImGui::PopStyleVar(2);	/* ChildRounding, ChildBorderSize */
		ImGui::PopStyleColor(); /* ChildBg */

		if (hovered && !disabled && ImGui::IsMouseClicked(0))
		{
			selectedIndex_ = i;
			const float *opts = GetRAMOptions(entry.model);
			float defRam = GetDefaultRAM(entry.model);
			ramChoice_ = 0;
			if (opts)
			{
				for (int j = 0; opts[j] > 0; ++j)
				{
					if (opts[j] >= defRam - 0.01f)
					{
						ramChoice_ = j;
						break;
					}
				}
			}
			speedChoice_ = 0;
			configTab_ = 0;
		}

		if (disabled) ImGui::PopStyleVar(); /* Alpha */

		ImGui::PopID();

		++col;
		if (col >= cols)
		{
			col = 0;
			ImGui::Spacing();
			if (offsetX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
		}
	}

	/* Hint for missing ROMs */
	ImGui::Spacing();
	ImGui::Spacing();
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.25f, 0.25f, 0.25f, 1));
		const char *hint = "Greyed-out models have no ROM file in roms/";
		float hintW = ImGui::CalcTextSize(hint).x;
		ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - hintW) * 0.5f);
		ImGui::Text("%s", hint);
		ImGui::PopStyleColor();
	}
}

/* ── Config panel ────────────────────────────────────── */

void ModelSelector::drawConfigPanel()
{
	if (selectedIndex_ < 0 || selectedIndex_ >= (int)models_.size()) return;

	const ModelEntry &entry = models_[selectedIndex_];

	/* Back button */
	if (ImGui::Button("<  Back"))
	{
		selectedIndex_ = -1;
		return;
	}
	ImGui::SameLine();
	ImGui::Text("%s", entry.displayName);
	ImGui::Separator();
	ImGui::Spacing();

	/* Tabs: Machine | Disks */
	if (ImGui::BeginTabBar("ConfigTabs"))
	{
		if (ImGui::BeginTabItem("Machine"))
		{
			configTab_ = 0;
			ImGui::Spacing();

			/* RAM */
			const float *ramOpts = GetRAMOptions(entry.model);
			if (ramOpts)
			{
				ImGui::Text("RAM");
				ImGui::SameLine(100);
				if (ImGui::BeginCombo("##RAM", nullptr, ImGuiComboFlags_NoPreview))
				{
					for (int j = 0; ramOpts[j] > 0; ++j)
					{
						char label[32];
						if (ramOpts[j] < 1.0f)
							snprintf(label, sizeof(label), "%d KB", (int)(ramOpts[j] * 1024));
						else if (ramOpts[j] == (int)ramOpts[j])
							snprintf(label, sizeof(label), "%d MB", (int)ramOpts[j]);
						else
							snprintf(label, sizeof(label), "%.1f MB", ramOpts[j]);
						if (ImGui::Selectable(label, ramChoice_ == j)) ramChoice_ = j;
					}
					ImGui::EndCombo();
				}
				/* Show current selection inline */
				ImGui::SameLine();
				{
					char cur[32];
					float val = ramOpts[ramChoice_];
					if (val < 1.0f)
						snprintf(cur, sizeof(cur), "%d KB", (int)(val * 1024));
					else if (val == (int)val)
						snprintf(cur, sizeof(cur), "%d MB", (int)val);
					else
						snprintf(cur, sizeof(cur), "%.1f MB", val);
					ImGui::Text("%s", cur);
				}
			}

			/* Speed */
			ImGui::Text("Speed");
			ImGui::SameLine(100);
			const char *speedLabels[] = {"1x", "2x", "4x", "8x", "Unlimited"};
			if (ImGui::BeginCombo("##Speed", speedLabels[speedChoice_]))
			{
				for (int j = 0; j < 5; ++j)
				{
					if (ImGui::Selectable(speedLabels[j], speedChoice_ == j)) speedChoice_ = j;
				}
				ImGui::EndCombo();
			}

			/* ROM path (read-only) */
			ImGui::Spacing();
			ImGui::TextDisabled("ROM: %s", entry.romPath.c_str());

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Disks"))
		{
			configTab_ = 1;
			ImGui::Spacing();
			ImGui::Text("Mount disk images at boot (drag & drop supported):");
			ImGui::Spacing();

			for (int slot = 0; slot < 6; ++slot)
			{
				ImGui::PushID(slot);
				char label[32];
				snprintf(label, sizeof(label), "Drive %d", slot + 1);

				if (diskPaths_[slot].empty())
				{
					ImGui::TextDisabled("%s: (empty)", label);
					ImGui::SameLine();
					if (ImGui::SmallButton("Browse..."))
					{
						/* TODO: native file dialog */
					}
				}
				else
				{
					/* Show filename only */
					const char *path = diskPaths_[slot].c_str();
					const char *name = strrchr(path, '/');
					if (!name) name = strrchr(path, '\\');
					if (!name)
						name = path;
					else
						++name;

					ImGui::Text("%s: %s", label, name);
					ImGui::SameLine();
					if (ImGui::SmallButton("x"))
					{
						diskPaths_[slot].clear();
					}
				}
				ImGui::PopID();
			}
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	/* Boot button */
	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	float bootW = 120;
	ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - bootW) * 0.5f);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.3f, 1.0f));
	if (ImGui::Button("Boot", ImVec2(bootW, 36)))
	{
		bootClicked_ = true;
	}
	ImGui::PopStyleColor(2);
}
