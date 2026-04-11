/*
	imgui_tool_registry.h — Tool registration and auto-menu
*/

#pragma once

#include "platform/imgui_tool.h"
#include <memory>
#include <vector>

class ToolRegistry
{
public:
	void registerTool(std::unique_ptr<ToolPanel> tool);
	void drawAllVisible();
	void drawToolMenu(); /* ImGui menu items for each tool */
	ToolPanel *findByName(const char *name);

private:
	std::vector<std::unique_ptr<ToolPanel>> tools_;
};
