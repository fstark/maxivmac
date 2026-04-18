/*
	global_registry.h — Low-memory global definitions from globals.def

	Parsed at startup; replaces the hardcoded kLowMemGlobals[] table.
*/

#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class TypeRegistry; // forward

struct GlobalDef
{
	std::string name;	  // e.g. "MemTop"
	uint32_t addr;		  // low-memory address
	uint16_t size;		  // computed from type (bytes)
	std::string typeName; // type from types.def, e.g. "Ptr", "QHdr", "^Zone"
	uint16_t count;		  // array element count (1 for scalars)
	std::string section;  // include-file section, e.g. "MemoryMgr"
	std::string brief;	  // one-line description
};

class GlobalRegistry
{
public:
	/* Parse globals.def.  Types validated against TypeRegistry.
	   Returns count of loaded globals. */
	int load(const std::filesystem::path &path, const TypeRegistry &types);

	std::span<const GlobalDef> globals() const;
	int count() const;

	/* Lookup helpers (binary search, sorted at load time). */
	const GlobalDef *findByName(std::string_view name) const;
	const GlobalDef *findByAddr(uint32_t addr) const;

	/* Distinct section names in file order. */
	std::span<const std::string> sections() const;

private:
	std::vector<GlobalDef> globals_;
	std::vector<int> byName_; // indices sorted by name
	std::vector<int> byAddr_; // indices sorted by addr
	std::vector<std::string> sections_;
};

GlobalRegistry &g_globalRegistry();
