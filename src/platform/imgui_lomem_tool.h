/*
	imgui_lomem_tool.h — Low Memory Globals viewer panel
*/

#pragma once

#include "platform/imgui_tool.h"
#include <cstdint>

class LowMemTool : public ToolPanel
{
public:
	const char *name() const override { return "Low Memory Globals"; }
	void draw() override;

private:
	char filterBuf_[64] = {};
	int sectionFilter_ = 0; /* 0 = All */
	bool snapshotValid_ = false;
	uint8_t snapshot_[4096] = {};
};
