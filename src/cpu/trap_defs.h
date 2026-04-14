/*
	trap_defs.h — External trap definition loader

	Parses assets/traps.def at startup into a lookup table
	mapping trap words to names, conventions, and typed parameters.
*/
#pragma once

#include <cstdint>
#include <string>
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

enum class ParamType
{
	Byte,
	Word,
	Long,
	Ptr,
	Handle,
	OSType,
	Str255,
	OSErr,
	Boolean,
	Rect,
	Point
};

struct ParamDef
{
	std::string name;
	ParamType type;
	ParamLoc loc;
};

enum class TrapConvention
{
	OS,
	Toolbox
};

struct TrapDef
{
	uint16_t trapWord;
	std::string name;
	TrapConvention convention;
	bool noreturn = false;
	std::vector<ParamDef> paramsIn;
	std::vector<ParamDef> paramsOut;
};

class TrapDefs
{
public:
	int load(const std::filesystem::path &path);
	int loadErrors(const std::filesystem::path &path);
	const TrapDef *find(uint16_t trapWord) const;
	const char *errorName(int16_t code) const;

private:
	static uint16_t maskTrapWord(uint16_t tw);
	static bool parseHeaderLine(const std::string &line, TrapDef &out);
	static void parseParamLine(const std::string &line, TrapDef &out);

	std::unordered_map<uint16_t, TrapDef> defs_;
	std::unordered_map<int16_t, std::string> errors_;
};
