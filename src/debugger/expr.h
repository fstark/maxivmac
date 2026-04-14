/*
	expr.h — Expression evaluator for debugger

	Recursive-descent parser for value and condition expressions
	used by print, breakpoint conditions, and other commands.
*/
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

// Register and memory read context — caller supplies this to decouple from CPU.
struct ExprContext
{
	uint32_t dregs[8]; // D0-D7
	uint32_t aregs[8]; // A0-A7
	uint32_t pc;
	uint16_t sr;
	uint32_t (*readLong)(uint32_t addr);
	uint16_t (*readWord)(uint32_t addr);
	uint8_t (*readByte)(uint32_t addr);
};

// Evaluate a value expression (for `print`).
// Returns true on success, sets outVal.  On error, sets outErr.
bool ExprEval(std::string_view text, const ExprContext &ctx, uint32_t &outVal, std::string &outErr);

// Evaluate a condition expression (for breakpoint conditions).
// Returns true if the condition is satisfied.
// On parse error, sets outErr and returns false.
bool ExprCheck(std::string_view text, const ExprContext &ctx, std::string &outErr);

// Parse a single value (register, hex, decimal, global name).
// Advances *pos past the consumed token.
// Returns true on success.
bool ExprParseValue(std::string_view text, int &pos, const ExprContext &ctx, uint32_t &outVal,
					std::string &outErr);
