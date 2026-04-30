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

	bool isWord() const { return kind == Kind::Word; }
	bool isWord(std::string_view s) const { return kind == Kind::Word && text == s; }
	bool isNumber() const { return kind == Kind::Number; }
	bool isOperator() const { return kind == Kind::Operator; }
	bool isOperator(std::string_view s) const { return kind == Kind::Operator && text == s; }
	bool isEnd() const { return kind == Kind::End; }
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

class DbgIO; // forward

// Find the CmdEntry matching the first token (prefix match).
// Returns nullptr if no match or ambiguous (error printed via io).
const CmdEntry *DispatchCommand(std::string_view input, const CmdEntry *table, int tableSize,
								DbgIO *io = nullptr);

/* ── Format spec for the x (examine) command ───────── */

struct FmtSpec
{
	int count = 1;
	char size = 'w';   /* b, w, l */
	char format = 'x'; /* x, d, s, i, t */
};

FmtSpec ParseFmtSpec(const std::string &spec);
