/*
	imgui_tool_registry.cpp — Tool registration and drawing
*/

#include "platform/imgui_tool_registry.h"
#include <imgui.h>
#include <cstring>

void ToolRegistry::registerTool(std::unique_ptr<ToolPanel> tool)
{
	tools_.push_back(std::move(tool));
}

void ToolRegistry::drawAllVisible()
{
	for (auto& tool : tools_) {
		if (tool->visible)
			tool->draw();
	}
}

void ToolRegistry::drawToolMenu()
{
	for (auto& tool : tools_) {
		ImGui::MenuItem(tool->name(), nullptr, &tool->visible);
	}
}

ToolPanel* ToolRegistry::findByName(const char* name)
{
	for (auto& tool : tools_) {
		if (strcmp(tool->name(), name) == 0)
			return tool.get();
	}
	return nullptr;
}
