/*
	imgui_launcher.h — Pre-boot Launcher UI

	Displays .mac file entries as clickable cards.
	Replaces the model selector.
*/

#pragma once

#include "config/mac_file.h"
#include <cstdint>
#include <vector>

class Launcher
{
public:
	~Launcher();

	void init(std::vector<MacFileEntry> entries);

	// Call after GL context is ready to load icon textures.
	void loadTextures();

	// Returns the selected entry when a card is clicked, or nullptr.
	const MacFileEntry *draw();

private:
	const MacFileEntry *drawCards();
	std::vector<MacFileEntry> entries_;
	std::vector<uint32_t> textures_; // GL texture IDs per entry (0 = none)
	int selectedIndex_ = -1;
};
