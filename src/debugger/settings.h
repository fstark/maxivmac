/*
	settings.h — Debugger settings registry

	General-purpose settings for the debugger, managed via
	`set <name> <value>` and `show [<name>]` commands.
*/
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

enum class SettingType
{
	Bool,
	UInt64,
	String
};

struct SettingDef
{
	std::string_view name;
	SettingType type;
	std::string_view help;
};

// Initialize settings with default values.
void SettingsInit();

// Get/set by name.  Returns false if name unknown or type mismatch.
bool SettingGetBool(std::string_view name, bool &out);
bool SettingSetBool(std::string_view name, bool value);
bool SettingGetUInt64(std::string_view name, uint64_t &out);
bool SettingSetUInt64(std::string_view name, uint64_t value);
bool SettingGetString(std::string_view name, std::string &out);

// Format a setting's current value as a human-readable string.
bool SettingFormat(std::string_view name, std::string &out);

// List all settings (for `show` with no args).
const std::vector<SettingDef> &SettingsList();
