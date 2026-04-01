/*
	imgui_model_selector.cpp — Pre-boot model selection UI
*/

#include "platform/imgui_model_selector.h"
#include "core/machine_config.h"
#include <imgui.h>
#include <algorithm>

/* ── Model metadata ──────────────────────────────────── */

struct ModelInfo {
	MacModel    model;
	const char* displayName;
	const char* description;
	bool        featured;
};

static const ModelInfo kModelTable[] = {
	{ MacModel::Mac128K,  "Macintosh 128K",  "68000, 128 KB RAM, 512\xc3\x97" "342",        true  },
	{ MacModel::Plus,     "Macintosh Plus",   "68000, 4 MB RAM, 512\xc3\x97" "342",          true  },
	{ MacModel::SE,       "Macintosh SE",     "68000, 4 MB RAM, ADB, 512\xc3\x97" "342",     true  },
	{ MacModel::II,       "Macintosh II",     "68020, 8 MB RAM, 640\xc3\x97" "480 color",    true  },
	{ MacModel::Twig43,   "Twig43",           "Prototype, 68000, 128 KB",                     false },
	{ MacModel::Twiggy,   "Twiggy",           "Prototype, 68000, 128 KB",                     false },
	{ MacModel::Mac512Ke, "Macintosh 512Ke",  "68000, 512 KB RAM, 512\xc3\x97" "342",        false },
	{ MacModel::Kanji,    "Mac Plus Kanji",   "68000, 4 MB RAM, Japanese ROM",                false },
	{ MacModel::SEFDHD,   "Macintosh SE FDHD","68000, 4 MB RAM, SuperDrive",                  false },
	{ MacModel::Classic,  "Macintosh Classic", "68000, 4 MB RAM, System in ROM",              false },
	{ MacModel::PB100,    "PowerBook 100",    "68000, 4 MB RAM, 640\xc3\x97" "400",          false },
	{ MacModel::IIx,      "Macintosh IIx",    "68030, 8 MB RAM, 640\xc3\x97" "480 color",    false },
};
static constexpr int kModelCount = sizeof(kModelTable) / sizeof(kModelTable[0]);

/* Valid RAM options per model (in MB). */
struct RAMOptions {
	MacModel model;
	float    options[6];  // 0-terminated
};

static const RAMOptions kRAMTable[] = {
	{ MacModel::Twig43,    { 0.125f, 0 } },
	{ MacModel::Twiggy,    { 0.125f, 0 } },
	{ MacModel::Mac128K,   { 0.125f, 0 } },
	{ MacModel::Mac512Ke,  { 0.5f, 0 } },
	{ MacModel::Kanji,     { 1, 2.5f, 4, 0 } },
	{ MacModel::Plus,      { 1, 2.5f, 4, 0 } },
	{ MacModel::SE,        { 1, 2.5f, 4, 0 } },
	{ MacModel::SEFDHD,    { 1, 2.5f, 4, 0 } },
	{ MacModel::Classic,   { 1, 2.5f, 4, 0 } },
	{ MacModel::PB100,     { 2, 4, 8, 0 } },
	{ MacModel::II,        { 1, 2, 4, 5, 8, 0 } },
	{ MacModel::IIx,       { 1, 2, 4, 5, 8, 0 } },
};

static const float* GetRAMOptions(MacModel model)
{
	for (const auto& r : kRAMTable) {
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

void ModelSelector::init(const std::string& romDir)
{
	buildModelList(romDir);
	diskPaths_.resize(6);
}

void ModelSelector::buildModelList(const std::string& romDir)
{
	models_.clear();
	models_.reserve(kModelCount);

	for (int i = 0; i < kModelCount; ++i) {
		ModelEntry e;
		e.model       = kModelTable[i].model;
		e.displayName = kModelTable[i].displayName;
		e.description = kModelTable[i].description;
		e.featured    = kModelTable[i].featured;
		e.romPath     = ResolveRomPath("", e.model, romDir);
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
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoSavedSettings;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(40, 30));
	if (ImGui::Begin("##ModelSelector", nullptr, flags)) {
		if (selectedIndex_ < 0) {
			drawModelGrid();
		} else {
			drawConfigPanel();
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();

	/* Check if Boot was clicked (flag set by drawConfigPanel) */
	if (bootClicked_ && selectedIndex_ >= 0 && selectedIndex_ < (int)models_.size()) {
		const ModelEntry& entry = models_[selectedIndex_];
		result.accepted = true;
		result.config.model = entry.model;
		result.config.modelExplicit = true;
		result.config.romPath = entry.romPath;

		/* RAM */
		const float* ramOpts = GetRAMOptions(entry.model);
		if (ramOpts && ramOpts[ramChoice_] > 0) {
			result.config.ramMB = (uint32_t)(ramOpts[ramChoice_] + 0.01f);
			/* Handle fractional MB (e.g. 2.5 MB = 2560 KB) */
			if (ramOpts[ramChoice_] < 1.0f) {
				result.config.ramMB = 0; // will set via raw bytes later
			}
		}

		/* Speed: 0=1x, 1=2x, 2=4x, 3=8x, 4=unlimited(0) */
		static const int speedMap[] = { 1, 2, 4, 8, 0 };
		result.config.speed = speedMap[speedChoice_];

		/* Disks */
		for (const auto& dp : diskPaths_) {
			if (!dp.empty())
				result.config.diskPaths.push_back(dp);
		}

		bootClicked_ = false;
	}

	return result;
}

/* ── Grid view ───────────────────────────────────────── */

void ModelSelector::drawModelGrid()
{
	/* Title */
	ImGui::PushFont(nullptr); /* use default for now */
	{
		const char* title = "Maxi vMac";
		float titleW = ImGui::CalcTextSize(title).x;
		ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - titleW) * 0.5f);
		ImGui::Text("%s", title);
	}
	ImGui::PopFont();
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();
	ImGui::Spacing();

	/* Determine which models to show */
	float avail = ImGui::GetContentRegionAvail().x;
	int cols = showAllModels_ ? 4 : 2;
	float cardW = (avail - (cols - 1) * 16.0f) / cols;
	if (cardW < 140.0f) { cols = 2; cardW = (avail - 16.0f) / 2; }
	float cardH = showAllModels_ ? 80.0f : 120.0f;

	/* Center the grid */
	float totalW = cols * cardW + (cols - 1) * 16.0f;
	float offsetX = (avail - totalW) * 0.5f;
	if (offsetX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

	int col = 0;
	int modelIdx = 0;
	for (auto& entry : models_) {
		if (!showAllModels_ && !entry.featured) {
			++modelIdx;
			continue;
		}

		if (col > 0)
			ImGui::SameLine(0, 16.0f);

		ImGui::PushID(modelIdx);

		/* Card styling */
		bool disabled = !entry.romAvailable;
		if (disabled) {
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.35f);
		}

		/* Draw card as a selectable child window */
		ImVec2 cardSize(cardW, cardH);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
		ImGui::BeginChild("card", cardSize, ImGuiChildFlags_Borders);
		{
			ImGui::Spacing();

			/* Placeholder icon: colored square */
			ImVec2 iconPos = ImGui::GetCursorScreenPos();
			float iconSize = showAllModels_ ? 32.0f : 48.0f;
			ImGui::SetCursorPosX((cardW - iconSize) * 0.5f);
			ImU32 iconColor = disabled
				? IM_COL32(80, 80, 80, 255)
				: (entry.featured
					? IM_COL32(100, 149, 237, 255)
					: IM_COL32(120, 120, 140, 255));
			iconPos = ImGui::GetCursorScreenPos();
			ImGui::GetWindowDrawList()->AddRectFilled(
				iconPos,
				ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
				iconColor, 6.0f);
			/* Initial letter */
			char initial[2] = { entry.displayName[0], 0 };
			if (entry.displayName[0] == 'M' && entry.displayName[10] != 0)
				initial[0] = entry.displayName[10]; // Use distinguishing char
			ImVec2 letterSize = ImGui::CalcTextSize(initial);
			ImGui::GetWindowDrawList()->AddText(
				ImVec2(iconPos.x + (iconSize - letterSize.x) * 0.5f,
				       iconPos.y + (iconSize - letterSize.y) * 0.5f),
				IM_COL32(255, 255, 255, 255), initial);
			ImGui::Dummy(ImVec2(iconSize, iconSize));
			ImGui::Spacing();

			/* Name (centered) */
			float nameW = ImGui::CalcTextSize(entry.displayName).x;
			ImGui::SetCursorPosX((cardW - nameW) * 0.5f);
			ImGui::Text("%s", entry.displayName);

			/* Description (centered, smaller) */
			if (!showAllModels_) {
				float descW = ImGui::CalcTextSize(entry.description).x;
				ImGui::SetCursorPosX((cardW - descW) * 0.5f);
				ImGui::TextDisabled("%s", entry.description);
			}
		}

		/* Detect click on the child window */
		bool hovered = ImGui::IsWindowHovered();
		ImGui::EndChild();
		ImGui::PopStyleVar(); /* ChildRounding */
		ImGui::PopStyleColor(); /* ChildBg */

		if (hovered && !disabled && ImGui::IsMouseClicked(0)) {
			selectedIndex_ = modelIdx;
			/* Set default RAM */
			const float* opts = GetRAMOptions(entry.model);
			float defRam = GetDefaultRAM(entry.model);
			ramChoice_ = 0;
			if (opts) {
				for (int j = 0; opts[j] > 0; ++j) {
					if (opts[j] >= defRam - 0.01f) { ramChoice_ = j; break; }
				}
			}
			speedChoice_ = 0;
			configTab_ = 0;
		}

		if (disabled) {
			ImGui::PopStyleVar(); /* Alpha */
		}

		ImGui::PopID();

		++col;
		if (col >= cols) {
			col = 0;
			ImGui::Spacing();
			/* Re-center for next row */
			if (offsetX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
		}
		++modelIdx;
	}

	/* "More Models..." / "Fewer Models" button */
	ImGui::Spacing();
	ImGui::Spacing();
	{
		const char* btnText = showAllModels_ ? "Show Featured" : "More Models...";
		float btnW = ImGui::CalcTextSize(btnText).x + 40;
		ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - btnW) * 0.5f);
		if (ImGui::Button(btnText, ImVec2(btnW, 0))) {
			showAllModels_ = !showAllModels_;
		}
	}

	/* Hint for missing ROMs */
	ImGui::Spacing();
	{
		const char* hint = "Greyed-out models have no ROM file in roms/";
		float hintW = ImGui::CalcTextSize(hint).x;
		ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - hintW) * 0.5f);
		ImGui::TextDisabled("%s", hint);
	}
}

/* ── Config panel ────────────────────────────────────── */

void ModelSelector::drawConfigPanel()
{
	if (selectedIndex_ < 0 || selectedIndex_ >= (int)models_.size())
		return;

	const ModelEntry& entry = models_[selectedIndex_];

	/* Back button */
	if (ImGui::Button("<  Back")) {
		selectedIndex_ = -1;
		return;
	}
	ImGui::SameLine();
	ImGui::Text("%s", entry.displayName);
	ImGui::Separator();
	ImGui::Spacing();

	/* Tabs: Machine | Disks */
	if (ImGui::BeginTabBar("ConfigTabs")) {
		if (ImGui::BeginTabItem("Machine")) {
			configTab_ = 0;
			ImGui::Spacing();

			/* RAM */
			const float* ramOpts = GetRAMOptions(entry.model);
			if (ramOpts) {
				ImGui::Text("RAM");
				ImGui::SameLine(100);
				if (ImGui::BeginCombo("##RAM", nullptr, ImGuiComboFlags_NoPreview)) {
					for (int j = 0; ramOpts[j] > 0; ++j) {
						char label[32];
						if (ramOpts[j] < 1.0f)
							snprintf(label, sizeof(label), "%d KB", (int)(ramOpts[j] * 1024));
						else if (ramOpts[j] == (int)ramOpts[j])
							snprintf(label, sizeof(label), "%d MB", (int)ramOpts[j]);
						else
							snprintf(label, sizeof(label), "%.1f MB", ramOpts[j]);
						if (ImGui::Selectable(label, ramChoice_ == j))
							ramChoice_ = j;
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
			const char* speedLabels[] = { "1x", "2x", "4x", "8x", "Unlimited" };
			if (ImGui::BeginCombo("##Speed", speedLabels[speedChoice_])) {
				for (int j = 0; j < 5; ++j) {
					if (ImGui::Selectable(speedLabels[j], speedChoice_ == j))
						speedChoice_ = j;
				}
				ImGui::EndCombo();
			}

			/* ROM path (read-only) */
			ImGui::Spacing();
			ImGui::TextDisabled("ROM: %s", entry.romPath.c_str());

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Disks")) {
			configTab_ = 1;
			ImGui::Spacing();
			ImGui::Text("Mount disk images at boot (drag & drop supported):");
			ImGui::Spacing();

			for (int slot = 0; slot < 6; ++slot) {
				ImGui::PushID(slot);
				char label[32];
				snprintf(label, sizeof(label), "Drive %d", slot + 1);

				if (diskPaths_[slot].empty()) {
					ImGui::TextDisabled("%s: (empty)", label);
					ImGui::SameLine();
					if (ImGui::SmallButton("Browse...")) {
						/* TODO: native file dialog */
					}
				} else {
					/* Show filename only */
					const char* path = diskPaths_[slot].c_str();
					const char* name = strrchr(path, '/');
					if (!name) name = strrchr(path, '\\');
					if (!name) name = path; else ++name;

					ImGui::Text("%s: %s", label, name);
					ImGui::SameLine();
					if (ImGui::SmallButton("x")) {
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
	if (ImGui::Button("Boot", ImVec2(bootW, 36))) {
		bootClicked_ = true;
	}
	ImGui::PopStyleColor(2);
}
