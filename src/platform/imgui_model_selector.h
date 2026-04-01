/*
	imgui_model_selector.h — Pre-boot model selection UI

	Displays a grid of Mac models, lets the user pick one,
	configure RAM/disks, and boot.
*/

#pragma once

#include "core/config_loader.h"
#include <string>
#include <vector>

class EmulatorShell;
class ImGuiBackend;

/* Result returned when the user clicks Boot. */
struct ModelSelectorResult {
	bool        accepted = false;  // true = user clicked Boot
	LaunchConfig config;           // populated with user choices
};

/* Persistent state for the model selector UI. */
class ModelSelector {
public:
	void init(const std::string& romDir);
	ModelSelectorResult draw();

private:
	/* Per-model metadata. */
	struct ModelEntry {
		MacModel    model;
		const char* displayName;
		const char* description;
		bool        featured;       // shown in main grid
		bool        romAvailable;
		std::string romPath;        // resolved path (empty if missing)
	};

	void buildModelList(const std::string& romDir);
	void drawModelGrid();
	void drawConfigPanel();

	std::vector<ModelEntry> models_;
	int  selectedIndex_ = -1;       // -1 = grid view, >= 0 = config view
	bool showAllModels_ = false;

	/* Config panel state */
	int         ramChoice_ = 0;
	int         speedChoice_ = 0;
	int         configTab_ = 0;     // 0 = Machine, 1 = Disks
	bool        bootClicked_ = false;
	std::vector<std::string> diskPaths_;
};
