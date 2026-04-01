/*
	imgui_tool.h — ToolPanel base class for developer tools

	Each debug/developer tool implements this interface and
	registers with the ToolRegistry. The framework handles
	menu population and visibility management.
*/

#pragma once

class ToolPanel {
public:
	virtual ~ToolPanel() = default;
	virtual const char* name() const = 0;
	virtual void draw() = 0;
	bool visible = false;
};
