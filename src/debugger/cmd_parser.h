/*
	cmd_parser.h — Tokenizer and command dispatch for debugger
*/
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct Token
{
	enum class Kind
	{
		Word,
		Number,
		Operator,
		End
	};
	Kind kind;
	std::string text;
	uint32_t numValue; // valid when kind == Number
};

// Tokenize a command line into tokens.
std::vector<Token> Tokenize(std::string_view line);

// Try to parse a token as a hex/decimal number.  Returns true on success.
bool ParseNumber(std::string_view text, uint32_t &outVal);

class Debugger; // forward

struct CmdEntry
{
	std::string_view name;
	std::string_view shortcut; // empty if none
	void (*handler)(Debugger &dbg, const std::vector<Token> &args);
	std::string_view helpBrief;
	std::string_view helpFull;
};

// Find the CmdEntry matching the first token (prefix match).
// Returns nullptr if no match or ambiguous.
const CmdEntry *DispatchCommand(std::string_view input, const CmdEntry *table, int tableSize);
