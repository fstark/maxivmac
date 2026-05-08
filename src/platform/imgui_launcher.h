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

	// Draw the launcher UI. Check selectedMac() afterwards.
	void draw();

	// Entry selected by the user this frame, or nullptr.
	const MacFileEntry *selectedMac() const { return selectedMac_; }

private:
	void drawTitle();
	void drawVersion();
	void drawEmptyState();
	void drawCards();
	void drawCard(int index, float cardW, float cardH);
	void drawCardIcon(int index, float cardW, bool valid);
	void drawCardText(const MacFileEntry &e, float cardW, bool valid);
	void drawCardHover(const MacFileEntry &e, bool valid);
	bool drawInfoButton(int cardIndex, float cardW);
	void drawInfoPopup();
	std::vector<MacFileEntry> entries_;
	std::vector<uint32_t> textures_;			 // GL texture IDs per entry (0 = none)
	const MacFileEntry *selectedMac_ = nullptr;	 // set on boot click
	const MacFileEntry *displayedMac_ = nullptr; // info panel target
};
