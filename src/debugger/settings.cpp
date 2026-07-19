/*
	settings.cpp — Debugger settings registry
*/

#include "debugger/settings.h"

#include <algorithm>
#include <cinttypes>
#include <unordered_map>
#include <variant>

/* ── Setting definitions ────────────────────────────── */

static const std::vector<SettingDef> s_defs = {
	{"default-timeout", SettingType::UInt64, "Default cycle budget for 'wait' commands (0 = infinite)"},
	{"confirm", SettingType::Bool, "Require y/n confirmation for destructive commands"},
};

/* ── Setting storage ────────────────────────────────── */

using SettingValue = std::variant<bool, uint64_t, std::string>;

static std::unordered_map<std::string, SettingValue> s_values;

void SettingsInit()
{
	s_values["default-timeout"] = uint64_t{40'000'000};
	s_values["confirm"] = false;
}

/* ── Getters / setters ──────────────────────────────── */

static const SettingDef *FindDef(std::string_view name)
{
	for (auto &d : s_defs)
		if (d.name == name) return &d;
	return nullptr;
}

bool SettingGetBool(std::string_view name, bool &out)
{
	auto *def = FindDef(name);
	if (!def || def->type != SettingType::Bool) return false;
	auto it = s_values.find(std::string(name));
	if (it == s_values.end()) return false;
	out = std::get<bool>(it->second);
	return true;
}

bool SettingSetBool(std::string_view name, bool value)
{
	auto *def = FindDef(name);
	if (!def || def->type != SettingType::Bool) return false;
	s_values[std::string(name)] = value;
	return true;
}

bool SettingGetUInt64(std::string_view name, uint64_t &out)
{
	auto *def = FindDef(name);
	if (!def || def->type != SettingType::UInt64) return false;
	auto it = s_values.find(std::string(name));
	if (it == s_values.end()) return false;
	out = std::get<uint64_t>(it->second);
	return true;
}

bool SettingSetUInt64(std::string_view name, uint64_t value)
{
	auto *def = FindDef(name);
	if (!def || def->type != SettingType::UInt64) return false;
	s_values[std::string(name)] = value;
	return true;
}

bool SettingGetString(std::string_view name, std::string &out)
{
	auto *def = FindDef(name);
	if (!def || def->type != SettingType::String) return false;
	auto it = s_values.find(std::string(name));
	if (it == s_values.end()) return false;
	out = std::get<std::string>(it->second);
	return true;
}

bool SettingFormat(std::string_view name, std::string &out)
{
	auto *def = FindDef(name);
	if (!def) return false;
	auto it = s_values.find(std::string(name));
	if (it == s_values.end()) return false;

	switch (def->type)
	{
	case SettingType::Bool:
		out = std::get<bool>(it->second) ? "on" : "off";
		return true;
	case SettingType::UInt64:
	{
		char buf[64];
		std::snprintf(buf, sizeof(buf), "%" PRIu64, std::get<uint64_t>(it->second));
		out = buf;
		return true;
	}
	case SettingType::String:
		out = "\"" + std::get<std::string>(it->second) + "\"";
		return true;
	}
	return false;
}

const std::vector<SettingDef> &SettingsList()
{
	return s_defs;
}
