/*
	cmd_memory.cpp — Memory examination, modification, and search commands
*/

#include "debugger/debugger.h"
#include "debugger/cmd_parser.h"
#include "debugger/symbols.h"
#include "debugger/expr.h"

#include "core/machine.h"
#include "cpu/m68k.h"
#include "cpu/disasm.h"

#include <cstdio>
#include <cstring>

/* ── Build ExprContext from live CPU ────────────────── */

static ExprContext MakeLiveContext()
{
	ExprContext ctx{};
	m68k_getRegs(ctx.dregs, ctx.aregs);
	ctx.pc = m68k_getPC_public();
	ctx.sr = m68k_getSR_public();
	ctx.readLong = [](uint32_t addr) -> uint32_t { return get_vm_long(addr); };
	ctx.readWord = [](uint32_t addr) -> uint16_t { return get_vm_word(addr); };
	ctx.readByte = [](uint32_t addr) -> uint8_t { return get_vm_byte(addr); };
	return ctx;
}

/* ── Parse address from token ──────────────────────── */
[[maybe_unused]]
static bool ParseAddr(const Token &tok, uint32_t &addr)
{
	if (tok.kind == Token::Kind::Number)
	{
		addr = tok.numValue;
		return true;
	}
	if (tok.kind == Token::Kind::Word)
	{
		uint16_t tw;
		if (SymbolsResolve(tok.text, addr, tw)) return true;

		/* Try as expression (register name) */
		auto ctx = MakeLiveContext();
		std::string err;
		if (ExprEval(tok.text, ctx, addr, err)) return true;
	}
	return false;
}

/* ── Format spec parser for x command ──────────────── */

struct FmtSpec
{
	int count = 1;
	char size = 'w';   /* b, w, l */
	char format = 'x'; /* x, d, s, i */
};

static FmtSpec ParseFmtSpec(const std::string &spec)
{
	FmtSpec f;
	int pos = 0;
	int len = static_cast<int>(spec.size());

	/* Leading digits = count */
	if (pos < len && spec[pos] >= '0' && spec[pos] <= '9')
	{
		f.count = 0;
		while (pos < len && spec[pos] >= '0' && spec[pos] <= '9')
		{
			f.count = f.count * 10 + (spec[pos] - '0');
			++pos;
		}
	}

	/* Size letter */
	if (pos < len && (spec[pos] == 'b' || spec[pos] == 'w' || spec[pos] == 'l'))
	{
		f.size = spec[pos++];
	}

	/* Format letter */
	if (pos < len && (spec[pos] == 'x' || spec[pos] == 'd' || spec[pos] == 's' || spec[pos] == 'i'))
	{
		f.format = spec[pos++];
	}

	return f;
}

void CmdExamine(Debugger &dbg, const std::vector<Token> &args)
{
	(void)dbg;

	FmtSpec fmt;
	int argIdx = 0;

	/* Check for /FMT */
	if (!args.empty() && args[0].kind == Token::Kind::Operator && args[0].text == "/")
	{
		++argIdx;
		if (argIdx < static_cast<int>(args.size()) &&
			(args[argIdx].kind == Token::Kind::Word || args[argIdx].kind == Token::Kind::Number))
		{
			fmt = ParseFmtSpec(args[argIdx].text);
			++argIdx;
		}
	}

	if (argIdx >= static_cast<int>(args.size()) || args[argIdx].kind == Token::Kind::End)
	{
		std::printf("Usage: x[/FMT] <addr>\n");
		return;
	}

	/* Parse address — may be an expression */
	uint32_t addr;
	{
		/* Build expression string from remaining tokens */
		std::string exprStr;
		for (int i = argIdx; i < static_cast<int>(args.size()); ++i)
		{
			if (args[i].kind == Token::Kind::End) break;
			if (!exprStr.empty()) exprStr += ' ';
			exprStr += args[i].text;
		}
		auto ctx = MakeLiveContext();
		std::string err;
		if (!ExprEval(exprStr, ctx, addr, err))
		{
			std::printf("Cannot evaluate address: %s\n", err.c_str());
			return;
		}
	}

	/* Disassembly mode */
	if (fmt.format == 'i')
	{
		uint32_t pc = addr;
		for (int i = 0; i < fmt.count; ++i)
		{
			uint32_t thisPC = pc;
			auto text = Disassemble(pc);
			std::printf("$%08X: %s\n", thisPC, text.c_str());
		}
		return;
	}

	/* String mode */
	if (fmt.format == 's')
	{
		std::printf("$%08X: \"", addr);
		for (int i = 0; i < 256; ++i)
		{
			uint8_t c = get_vm_byte(addr + i);
			if (c == 0) break;
			if (c >= 0x20 && c < 0x7F)
				std::printf("%c", c);
			else
				std::printf("\\x%02X", c);
		}
		std::printf("\"\n");
		return;
	}

	/* Hex/decimal dump */
	int bytesPerUnit = (fmt.size == 'b') ? 1 : (fmt.size == 'l') ? 4 : 2;
	int perRow = (fmt.size == 'b') ? 16 : (fmt.size == 'l') ? 4 : 8;

	for (int i = 0; i < fmt.count; ++i)
	{
		if (i % perRow == 0)
		{
			if (i > 0) std::printf("\n");
			std::printf("$%08X:", addr + i * bytesPerUnit);
		}

		uint32_t val;
		uint32_t a = addr + i * bytesPerUnit;
		if (bytesPerUnit == 1)
			val = get_vm_byte(a);
		else if (bytesPerUnit == 2)
			val = get_vm_word(a);
		else
			val = get_vm_long(a);

		if (fmt.format == 'd')
		{
			std::printf(" %u", val);
		}
		else
		{
			if (bytesPerUnit == 1)
				std::printf(" %02X", val);
			else if (bytesPerUnit == 2)
				std::printf(" %04X", val);
			else
				std::printf(" %08X", val);
		}
	}
	std::printf("\n");
}

void CmdPrint(Debugger &dbg, const std::vector<Token> &args)
{
	(void)dbg;

	if (args.empty() || args[0].kind == Token::Kind::End)
	{
		std::printf("Usage: print <expr>\n");
		return;
	}

	/* Build expression string from all tokens */
	std::string exprStr;
	for (auto &tok : args)
	{
		if (tok.kind == Token::Kind::End) break;
		if (!exprStr.empty()) exprStr += ' ';
		exprStr += tok.text;
	}

	auto ctx = MakeLiveContext();
	uint32_t val;
	std::string err;
	if (ExprEval(exprStr, ctx, val, err))
	{
		std::printf("$%08X (%u)\n", val, val);
	}
	else
	{
		std::printf("Error: %s\n", err.c_str());
	}
}

void CmdSet(Debugger &dbg, const std::vector<Token> &args)
{
	(void)dbg;

	if (args.size() < 3)
	{
		std::printf("Usage: set <target> = <value>\n");
		return;
	}

	/* Find '=' token */
	int eqIdx = -1;
	for (int i = 0; i < static_cast<int>(args.size()); ++i)
	{
		if (args[i].kind == Token::Kind::Operator && args[i].text == "=")
		{
			eqIdx = i;
			break;
		}
	}
	if (eqIdx < 1)
	{
		std::printf("Expected '=' in set command\n");
		return;
	}

	/* Parse value expression */
	std::string valStr;
	for (int i = eqIdx + 1; i < static_cast<int>(args.size()); ++i)
	{
		if (args[i].kind == Token::Kind::End) break;
		if (!valStr.empty()) valStr += ' ';
		valStr += args[i].text;
	}
	auto ctx = MakeLiveContext();
	uint32_t val;
	std::string err;
	if (!ExprEval(valStr, ctx, val, err))
	{
		std::printf("Cannot evaluate value: %s\n", err.c_str());
		return;
	}

	/* Target is memory: *addr or *addr.w or *addr.l */
	if (args[0].kind == Token::Kind::Operator && args[0].text == "*")
	{
		/* Parse address from tokens between * and = */
		std::string addrStr;
		int sizeBytes = 1; /* default byte */
		for (int i = 1; i < eqIdx; ++i)
		{
			if (args[i].kind == Token::Kind::End) break;
			/* Check for .w .l suffix */
			if (args[i].text == ".w")
			{
				sizeBytes = 2;
				continue;
			}
			if (args[i].text == ".l")
			{
				sizeBytes = 4;
				continue;
			}
			if (!addrStr.empty()) addrStr += ' ';
			addrStr += args[i].text;
		}

		uint32_t addr;
		if (!ExprEval(addrStr, ctx, addr, err))
		{
			std::printf("Cannot evaluate address: %s\n", err.c_str());
			return;
		}

		if (sizeBytes == 1)
			put_vm_byte(addr, static_cast<uint8_t>(val));
		else if (sizeBytes == 2)
			put_vm_word(addr, static_cast<uint16_t>(val));
		else
			put_vm_long(addr, val);

		std::printf("$%08X <- $%0*X\n", addr, sizeBytes * 2, val);
		return;
	}

	/* Target is a register */
	if (args[0].kind == Token::Kind::Word)
	{
		auto &name = args[0].text;
		/* Register writes need direct access to the CPU state.
		   For now, we support writing via ExprContext mapping. */
		// The plan says to use m68k_dreg(n) = val etc. but those are
		// macros inside m68k.cpp. We'd need a setter API.
		// For now, print that register writes are not yet supported.
		std::printf("Register write '%s' = $%08X — not yet supported\n", name.c_str(), val);
		return;
	}

	std::printf("Cannot parse target\n");
}

void CmdFind(Debugger &dbg, const std::vector<Token> &args)
{
	(void)dbg;

	if (args.size() < 3)
	{
		std::printf("Usage: find <start> <end> <pattern>\n");
		return;
	}

	uint32_t start = 0, end = 0;
	if (args[0].kind == Token::Kind::Number)
		start = args[0].numValue;
	else
	{
		std::printf("Expected start address\n");
		return;
	}
	if (args[1].kind == Token::Kind::Number)
		end = args[1].numValue;
	else
	{
		std::printf("Expected end address\n");
		return;
	}

	/* Build pattern from remaining tokens */
	std::vector<int> pattern; /* -1 = wildcard, 0-255 = byte */

	for (int i = 2; i < static_cast<int>(args.size()); ++i)
	{
		if (args[i].kind == Token::Kind::End) break;
		auto &text = args[i].text;

		/* Check for quoted string */
		if (text.size() >= 2 && text[0] == '"' && text.back() == '"')
		{
			for (size_t j = 1; j + 1 < text.size(); ++j)
				pattern.push_back(static_cast<uint8_t>(text[j]));
			continue;
		}

		/* ?? wildcard */
		if (text == "??")
		{
			pattern.push_back(-1);
			continue;
		}

		/* Hex byte pair */
		uint32_t val;
		if (ParseNumber(text, val))
		{
			if (val <= 0xFF)
			{
				pattern.push_back(static_cast<int>(val));
			}
			else
			{
				/* Multi-byte number: push bytes big-endian */
				if (val > 0xFFFF)
				{
					pattern.push_back(static_cast<int>((val >> 24) & 0xFF));
					pattern.push_back(static_cast<int>((val >> 16) & 0xFF));
				}
				if (val > 0xFF)
				{
					pattern.push_back(static_cast<int>((val >> 8) & 0xFF));
				}
				pattern.push_back(static_cast<int>(val & 0xFF));
			}
			continue;
		}

		std::printf("Cannot parse pattern element '%s'\n", text.c_str());
		return;
	}

	if (pattern.empty())
	{
		std::printf("No pattern specified\n");
		return;
	}

	int found = 0;
	int maxFound = 64;
	uint32_t patLen = static_cast<uint32_t>(pattern.size());

	for (uint32_t addr = start; addr + patLen <= end && found < maxFound; ++addr)
	{
		bool match = true;
		for (uint32_t j = 0; j < patLen; ++j)
		{
			if (pattern[j] == -1) continue; /* wildcard */
			if (get_vm_byte(addr + j) != static_cast<uint8_t>(pattern[j]))
			{
				match = false;
				break;
			}
		}
		if (match)
		{
			std::printf("$%08X:", addr);
			for (uint32_t j = 0; j < patLen && j < 16; ++j)
				std::printf(" %02X", get_vm_byte(addr + j));
			std::printf("\n");
			++found;
		}
	}

	if (found == 0)
		std::printf("Pattern not found\n");
	else
		std::printf("%d match%s\n", found, found == 1 ? "" : "es");
}
