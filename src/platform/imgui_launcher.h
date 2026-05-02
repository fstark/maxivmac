/*
	imgui_launcher.h — Pre-boot Launcher UI

	Displays .mac file entries as clickable cards.
	Replaces the model selector.
*/

#pragma once

#include "config/mac_file.h"
#include <vector>

class Launcher
{
public:
	void init(std::vector<MacFileEntry> entries);

	// Returns the selected entry when a card is clicked, or nullptr.
	const MacFileEntry *draw();

private:
	const MacFileEntry *drawCards();
	std::vector<MacFileEntry> entries_;
	int selectedIndex_ = -1;
};
