/*
	expr.cpp — Expression evaluator implementation

	Recursive-descent parser supporting:
	  - Register names: d0-d7, a0-a7, sp, pc, sr
	  - Hex literals: $xx, 0xXX
	  - Decimal literals
	  - Symbol names (via SymbolsResolve)
	  - Dereference: (expr) reads a long at the address
	  - Arithmetic: + and - with left-to-right evaluation
	  - Conditions: value op value, joined by &&
*/

#include "debugger/expr.h"
#include "debugger/symbols.h"

#include <cctype>
#include <cstring>

static void SkipSpaces(std::string_view text, int &pos)
{
	while (pos < static_cast<int>(text.size()) && text[pos] == ' ')
		++pos;
}

static bool IsWordChar(char c)
{
	return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

static std::string_view ReadWord(std::string_view text, int &pos)
{
	int start = pos;
	while (pos < static_cast<int>(text.size()) && IsWordChar(text[pos]))
		++pos;
	return text.substr(start, pos - start);
}

static bool ParseHex(std::string_view hex, uint32_t &outVal)
{
	if (hex.empty()) return false;
	uint32_t val = 0;
	for (char c : hex)
	{
		uint32_t digit;
		if (c >= '0' && c <= '9')
			digit = c - '0';
		else if (c >= 'a' && c <= 'f')
			digit = 10 + (c - 'a');
		else if (c >= 'A' && c <= 'F')
			digit = 10 + (c - 'A');
		else
			return false;
		val = (val << 4) | digit;
	}
	outVal = val;
	return true;
}

static bool ParseDecimal(std::string_view dec, uint32_t &outVal)
{
	if (dec.empty()) return false;
	uint32_t val = 0;
	for (char c : dec)
	{
		if (c < '0' || c > '9') return false;
		val = val * 10 + (c - '0');
	}
	outVal = val;
	return true;
}

static bool CIEqual(std::string_view a, std::string_view b)
{
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); ++i)
	{
		if (std::tolower(static_cast<unsigned char>(a[i])) !=
			std::tolower(static_cast<unsigned char>(b[i])))
			return false;
	}
	return true;
}

bool ExprParseValue(std::string_view text, int &pos, const ExprContext &ctx, uint32_t &outVal,
					std::string &outErr)
{
	SkipSpaces(text, pos);

	if (pos >= static_cast<int>(text.size()))
	{
		outErr = "unexpected end of expression";
		return false;
	}

	/* Parenthesized dereference: (expr) reads a long */
	if (text[pos] == '(')
	{
		++pos;
		uint32_t addr;
		if (!ExprParseValue(text, pos, ctx, addr, outErr)) return false;

		/* Handle arithmetic inside parens */
		SkipSpaces(text, pos);
		while (pos < static_cast<int>(text.size()) && (text[pos] == '+' || text[pos] == '-'))
		{
			char op = text[pos++];
			uint32_t rhs;
			if (!ExprParseValue(text, pos, ctx, rhs, outErr)) return false;
			if (op == '+')
				addr += rhs;
			else
				addr -= rhs;
			SkipSpaces(text, pos);
		}

		SkipSpaces(text, pos);
		if (pos >= static_cast<int>(text.size()) || text[pos] != ')')
		{
			outErr = "expected ')'";
			return false;
		}
		++pos;
		if (ctx.readLong)
			outVal = ctx.readLong(addr);
		else
			outVal = 0;
		return true;
	}

	/* $ hex literal */
	if (text[pos] == '$')
	{
		++pos;
		int start = pos;
		while (pos < static_cast<int>(text.size()) &&
			   std::isxdigit(static_cast<unsigned char>(text[pos])))
			++pos;
		if (pos == start)
		{
			outErr = "expected hex digits after '$'";
			return false;
		}
		return ParseHex(text.substr(start, pos - start), outVal);
	}

	/* 0x hex literal */
	if (pos + 1 < static_cast<int>(text.size()) && text[pos] == '0' &&
		(text[pos + 1] == 'x' || text[pos + 1] == 'X'))
	{
		pos += 2;
		int start = pos;
		while (pos < static_cast<int>(text.size()) &&
			   std::isxdigit(static_cast<unsigned char>(text[pos])))
			++pos;
		if (pos == start)
		{
			outErr = "expected hex digits after '0x'";
			return false;
		}
		return ParseHex(text.substr(start, pos - start), outVal);
	}

	/* Pure decimal */
	if (std::isdigit(static_cast<unsigned char>(text[pos])))
	{
		int start = pos;
		while (pos < static_cast<int>(text.size()) &&
			   std::isdigit(static_cast<unsigned char>(text[pos])))
			++pos;
		return ParseDecimal(text.substr(start, pos - start), outVal);
	}

	/* Word: register name or symbol */
	if (std::isalpha(static_cast<unsigned char>(text[pos])) || text[pos] == '_')
	{
		auto word = ReadWord(text, pos);

		/* Register names */
		if (word.size() == 2 && (word[0] == 'd' || word[0] == 'D') && word[1] >= '0' &&
			word[1] <= '7')
		{
			outVal = ctx.dregs[word[1] - '0'];
			return true;
		}
		if (word.size() == 2 && (word[0] == 'a' || word[0] == 'A') && word[1] >= '0' &&
			word[1] <= '7')
		{
			outVal = ctx.aregs[word[1] - '0'];
			return true;
		}
		if (CIEqual(word, "pc"))
		{
			outVal = ctx.pc;
			return true;
		}
		if (CIEqual(word, "sr"))
		{
			outVal = ctx.sr;
			return true;
		}
		if (CIEqual(word, "sp"))
		{
			outVal = ctx.aregs[7];
			return true;
		}

		/* Symbol lookup (trap or low-memory global) */
		uint32_t addr;
		uint16_t trapWord;
		if (SymbolsResolve(word, addr, trapWord))
		{
			outVal = (trapWord != 0) ? trapWord : addr;
			return true;
		}

		outErr = "unknown symbol '";
		outErr += word;
		outErr += "'";
		return false;
	}

	outErr = "unexpected character '";
	outErr += text[pos];
	outErr += "'";
	return false;
}

/* Parse an additive expression: value ((+|-) value)* */
static bool ParseAdditiveExpr(std::string_view text, int &pos, const ExprContext &ctx,
							  uint32_t &outVal, std::string &outErr)
{
	if (!ExprParseValue(text, pos, ctx, outVal, outErr)) return false;

	SkipSpaces(text, pos);
	while (pos < static_cast<int>(text.size()) && (text[pos] == '+' || text[pos] == '-'))
	{
		char op = text[pos++];
		uint32_t rhs;
		if (!ExprParseValue(text, pos, ctx, rhs, outErr)) return false;
		if (op == '+')
			outVal += rhs;
		else
			outVal -= rhs;
		SkipSpaces(text, pos);
	}
	return true;
}

bool ExprEval(std::string_view text, const ExprContext &ctx, uint32_t &outVal, std::string &outErr)
{
	int pos = 0;
	if (!ParseAdditiveExpr(text, pos, ctx, outVal, outErr)) return false;
	SkipSpaces(text, pos);
	if (pos < static_cast<int>(text.size()))
	{
		outErr = "unexpected trailing text: '";
		outErr += text.substr(pos);
		outErr += "'";
		return false;
	}
	return true;
}

/* Parse a comparison operator. Returns true if found. */
static bool ParseCompOp(std::string_view text, int &pos, int &op)
{
	SkipSpaces(text, pos);
	if (pos >= static_cast<int>(text.size())) return false;

	if (pos + 1 < static_cast<int>(text.size()))
	{
		char c0 = text[pos], c1 = text[pos + 1];
		if (c0 == '=' && c1 == '=')
		{
			pos += 2;
			op = 0;
			return true;
		}
		if (c0 == '!' && c1 == '=')
		{
			pos += 2;
			op = 1;
			return true;
		}
		if (c0 == '<' && c1 == '=')
		{
			pos += 2;
			op = 4;
			return true;
		}
		if (c0 == '>' && c1 == '=')
		{
			pos += 2;
			op = 5;
			return true;
		}
	}
	if (text[pos] == '<')
	{
		pos += 1;
		op = 2;
		return true;
	}
	if (text[pos] == '>')
	{
		pos += 1;
		op = 3;
		return true;
	}
	if (text[pos] == '&')
	{
		/* Single & is bitwise AND test (true if nonzero) */
		if (pos + 1 < static_cast<int>(text.size()) && text[pos + 1] == '&')
			return false; /* && is the conjunction, not a comparison op */
		pos += 1;
		op = 6;
		return true;
	}
	return false;
}

static bool EvalComparison(uint32_t lhs, int op, uint32_t rhs)
{
	switch (op)
	{
		case 0:
			return lhs == rhs;
		case 1:
			return lhs != rhs;
		case 2:
			return lhs < rhs;
		case 3:
			return lhs > rhs;
		case 4:
			return lhs <= rhs;
		case 5:
			return lhs >= rhs;
		case 6:
			return (lhs & rhs) != 0;
		default:
			return false;
	}
}

bool ExprCheck(std::string_view text, const ExprContext &ctx, std::string &outErr)
{
	int pos = 0;

	for (;;)
	{
		/* Parse lhs */
		uint32_t lhs;
		if (!ParseAdditiveExpr(text, pos, ctx, lhs, outErr)) return false;

		/* Parse operator */
		int op;
		if (!ParseCompOp(text, pos, op))
		{
			outErr = "expected comparison operator";
			return false;
		}

		/* Parse rhs */
		uint32_t rhs;
		if (!ParseAdditiveExpr(text, pos, ctx, rhs, outErr)) return false;

		if (!EvalComparison(lhs, op, rhs)) return false;

		/* Check for && conjunction */
		SkipSpaces(text, pos);
		if (pos + 1 < static_cast<int>(text.size()) && text[pos] == '&' && text[pos + 1] == '&')
		{
			pos += 2;
			continue;
		}

		/* End of expression or trailing text */
		SkipSpaces(text, pos);
		if (pos < static_cast<int>(text.size()))
		{
			outErr = "unexpected trailing text in condition";
			return false;
		}
		return true;
	}
}
