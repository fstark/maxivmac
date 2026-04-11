/*
	imgui_debug_windows.h — Debug tool panels

	ToolPanel implementations for Registers, Disassembly,
	Memory, and VIA debug windows.
*/

#pragma once

#include "platform/imgui_tool.h"

class RegistersTool : public ToolPanel
{
public:
	const char *name() const override { return "Registers"; }
	void draw() override;
};

class DisassemblyTool : public ToolPanel
{
public:
	const char *name() const override { return "Disassembly"; }
	void draw() override;
};

class MemoryTool : public ToolPanel
{
public:
	const char *name() const override { return "Memory"; }
	void draw() override;
};

class VIATool : public ToolPanel
{
public:
	const char *name() const override { return "VIA State"; }
	void draw() override;
};

class TrapsTool : public ToolPanel
{
public:
	const char *name() const override { return "Traps"; }
	void draw() override;
};

class ScrapTool : public ToolPanel
{
public:
	const char *name() const override { return "Scrap"; }
	void draw() override;
};

class ConsoleTool : public ToolPanel
{
public:
	const char *name() const override { return "Guest Console"; }
	void draw() override;

private:
	bool autoScroll = true;
};

/* Register all debug tools with the given registry. */
class ToolRegistry;
void RegisterDebugTools(ToolRegistry &registry);
