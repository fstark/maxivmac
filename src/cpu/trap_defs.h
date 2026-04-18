/*
	trap_defs.h — External trap definition loader

	Parses assets/traps.def at startup into a lookup table
	mapping trap words to names, conventions, and typed parameters.
*/
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <filesystem>

enum class ParamLoc
{
	Stack,
	D0,
	D1,
	D2,
	D3,
	D4,
	D5,
	D6,
	D7,
	A0,
	A1,
	A2,
	A3,
	A4,
	A5,
	A6,
	A7
};

struct ParamDef
{
	std::string name;
	std::string typeName; /* TypeRegistry type name (e.g. "long", "Str255", "IOParam") */
	ParamLoc loc = ParamLoc::Stack;
	bool isStructPtr = false; /* true when typeName is a ^struct pointer */
};

enum class TrapConvention
{
	OS,
	Toolbox
};

struct StructFieldFilter
{
	std::string paramName;			 /* back-reference to an in/out param name */
	std::vector<std::string> fields; /* field names to display */
};

struct DispatchInfo
{
	ParamDef selectorParam; /* type + location of the selector */
};

struct TrapDef
{
	uint16_t trapWord;
	std::string name;
	TrapConvention convention;
	bool noreturn = false;
	std::vector<ParamDef> paramsIn;
	std::vector<ParamDef> paramsOut;
	std::vector<StructFieldFilter> showIn;	/* entry-time struct field filters */
	std::vector<StructFieldFilter> showOut; /* exit-time struct field filters */
	std::optional<DispatchInfo> dispatch;	/* nullopt for non-dispatch traps */
};

struct SubtrapDef
{
	uint16_t selector; /* selector value (e.g. 0x09 for PBGetCatInfo) */
	TrapDef def;	   /* full definition (name, params, show-in/out) */
};

class TrapDefs
{
public:
	int load(const std::filesystem::path &path);
	int loadErrors(const std::filesystem::path &path);
	const TrapDef *find(uint16_t trapWord) const;
	const char *errorName(int16_t code) const;

	/* Name/search API (replaces trap_counter.cpp's s_dict) */
	int size() const;
	std::pair<uint32_t, std::string_view> entry(int index) const;
	std::string_view nameOf(uint16_t trapWord) const;
	void search(std::string_view prefix,
				std::vector<std::pair<uint32_t, std::string_view>> &results,
				int maxResults = 20) const;

	/* Dispatch subtrap API */
	const SubtrapDef *findSubtrap(uint16_t parentTrapWord, uint16_t selector) const;
	bool isDispatch(uint16_t trapWord) const;
	const DispatchInfo *dispatchInfo(uint16_t trapWord) const;
	std::string_view nameOfSubtrap(uint32_t syntheticKey) const;

	static uint16_t maskTrapWord(uint16_t tw);

private:
	static bool parseHeaderLine(const std::string &line, TrapDef &out);
	static void parseParamLine(const std::string &line, TrapDef &out);

	std::unordered_map<uint16_t, TrapDef> defs_;
	std::unordered_map<int16_t, std::string> errors_;
	std::vector<std::pair<uint32_t, std::string>> sortedNames_;

	/* Key: parent trapWord (masked). Value: selector → SubtrapDef. */
	std::unordered_map<uint16_t, std::unordered_map<uint16_t, SubtrapDef>> subtraps_;
};
