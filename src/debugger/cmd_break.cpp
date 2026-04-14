/*
	cmd_break.cpp — Breakpoint and watchpoint commands
*/

#include "debugger/debugger.h"
#include "debugger/cmd_parser.h"
#include "debugger/symbols.h"

#include "cpu/trap_counter.h"

#include <cstdio>
#include <cstring>

/* Parse a watchpoint value condition: "if val <op> <value>" */
static bool ParseValCond(const std::vector<Token> &args, int startIdx, bool &hasValCond,
						 uint8_t &valCondOp, uint32_t &valCondValue)
{
	hasValCond = false;
	valCondOp = 0;
	valCondValue = 0;

	/* Look for "if" "val" <op> <value> */
	int i = startIdx;
	auto end = static_cast<int>(args.size());
	while (i < end && args[i].kind != Token::Kind::End)
	{
		if (args[i].kind == Token::Kind::Word && args[i].text == "if")
		{
			++i;
			if (i < end && args[i].kind == Token::Kind::Word && args[i].text == "val")
			{
				++i;
				if (i < end && args[i].kind == Token::Kind::Operator)
				{
					auto &op = args[i].text;
					if (op == "==")
						valCondOp = 0;
					else if (op == "!=")
						valCondOp = 1;
					else if (op == "<")
						valCondOp = 2;
					else if (op == ">")
						valCondOp = 3;
					else if (op == "<=")
						valCondOp = 4;
					else if (op == ">=")
						valCondOp = 5;
					else if (op == "&")
						valCondOp = 6;
					else
					{
						std::printf("Unknown watchpoint operator '%s'\n", op.c_str());
						return false;
					}
					++i;
					if (i < end && args[i].kind == Token::Kind::Number)
					{
						valCondValue = args[i].numValue;
						hasValCond = true;
						return true;
					}
					std::printf("Expected value after operator\n");
					return false;
				}
			}
			std::printf("Expected 'val <op> <value>' after 'if'\n");
			return false;
		}
		++i;
	}
	return true;
}

void CmdBreak(Debugger &dbg, const std::vector<Token> &args)
{
	if (args.empty() || args[0].kind == Token::Kind::End)
	{
		std::printf("Usage: break <location> [if <cond>]\n");
		return;
	}

	uint32_t addr = 0;
	uint16_t trapWord = 0;

	if (args[0].kind == Token::Kind::Number)
	{
		addr = args[0].numValue;
	}
	else if (args[0].kind == Token::Kind::Word)
	{
		if (!SymbolsResolve(args[0].text, addr, trapWord))
		{
			std::printf("Cannot resolve '%s'\n", args[0].text.c_str());
			return;
		}
	}
	else
	{
		std::printf("Expected address or symbol name\n");
		return;
	}

	/* Parse optional condition */
	std::string condition;
	for (size_t i = 1; i < args.size(); ++i)
	{
		if (args[i].kind == Token::Kind::Word && args[i].text == "if")
		{
			/* Capture everything after "if" as the condition */
			for (size_t j = i + 1; j < args.size(); ++j)
			{
				if (args[j].kind == Token::Kind::End) break;
				if (!condition.empty()) condition += ' ';
				condition += args[j].text;
			}
			break;
		}
	}

	uint32_t id = dbg.addBreakpoint(addr, trapWord, condition);

	if (trapWord)
	{
		const char *name = trap_dict_name(trapWord);
		if (name)
			std::printf("Breakpoint %u on trap $%04X (%s)\n", id, trapWord, name);
		else
			std::printf("Breakpoint %u on trap $%04X\n", id, trapWord);
	}
	else
	{
		auto sym = SymbolsAtAddress(addr);
		if (!sym.empty())
			std::printf("Breakpoint %u at $%08X (%.*s)\n", id, addr, static_cast<int>(sym.size()),
						sym.data());
		else
			std::printf("Breakpoint %u at $%08X\n", id, addr);
	}

	if (!condition.empty()) std::printf("  condition: %s\n", condition.c_str());
}

static void DoWatch(Debugger &dbg, const std::vector<Token> &args, char mode)
{
	if (args.empty() || args[0].kind == Token::Kind::End)
	{
		std::printf("Usage: %cwatch <addr> [len] [if val <op> <value>]\n",
					mode == 'R' ? 'r' : (mode == 'A' ? 'a' : ' '));
		return;
	}

	uint32_t addr = 0;
	uint32_t len = 1;
	int nextArg = 1;

	if (args[0].kind == Token::Kind::Number)
	{
		addr = args[0].numValue;
	}
	else if (args[0].kind == Token::Kind::Word)
	{
		uint16_t tw;
		if (!SymbolsResolve(args[0].text, addr, tw))
		{
			std::printf("Cannot resolve '%s'\n", args[0].text.c_str());
			return;
		}
		/* Auto-detect size from global */
		uint16_t sz = SymbolsSizeAt(addr);
		if (sz > 0) len = sz;
	}

	/* Optional explicit length */
	if (nextArg < static_cast<int>(args.size()) && args[nextArg].kind == Token::Kind::Number)
	{
		len = args[nextArg].numValue;
		++nextArg;
	}

	bool hasValCond;
	uint8_t valCondOp;
	uint32_t valCondValue;
	if (!ParseValCond(args, nextArg, hasValCond, valCondOp, valCondValue)) return;

	uint32_t id = dbg.addWatchpoint(addr, len, mode, hasValCond, valCondOp, valCondValue);

	const char *modeStr = (mode == 'W') ? "write" : (mode == 'R') ? "read" : "access";
	auto sym = SymbolsAtAddress(addr);
	if (!sym.empty())
		std::printf("Watchpoint %u: %s $%08X-%08X (%.*s)\n", id, modeStr, addr, addr + len,
					static_cast<int>(sym.size()), sym.data());
	else
		std::printf("Watchpoint %u: %s $%08X-%08X\n", id, modeStr, addr, addr + len);
}

void CmdWatch(Debugger &dbg, const std::vector<Token> &args)
{
	DoWatch(dbg, args, 'W');
}

void CmdRwatch(Debugger &dbg, const std::vector<Token> &args)
{
	DoWatch(dbg, args, 'R');
}

void CmdAwatch(Debugger &dbg, const std::vector<Token> &args)
{
	DoWatch(dbg, args, 'A');
}

void CmdDelete(Debugger &dbg, const std::vector<Token> &args)
{
	if (args.empty() || args[0].kind == Token::Kind::End)
	{
		/* Delete all */
		int count = 0;
		while (!dbg.breakpoints().empty())
		{
			dbg.deleteById(dbg.breakpoints().front().id);
			++count;
		}
		while (!dbg.watchpoints().empty())
		{
			dbg.deleteById(dbg.watchpoints().front().id);
			++count;
		}
		std::printf("Deleted %d breakpoints/watchpoints\n", count);
		return;
	}

	if (args[0].kind == Token::Kind::Number)
	{
		if (dbg.deleteById(args[0].numValue))
			std::printf("Deleted %u\n", args[0].numValue);
		else
			std::printf("No breakpoint/watchpoint with id %u\n", args[0].numValue);
	}
}

void CmdDisable(Debugger &dbg, const std::vector<Token> &args)
{
	if (args.empty() || args[0].kind != Token::Kind::Number)
	{
		std::printf("Usage: disable <id>\n");
		return;
	}
	if (dbg.enableById(args[0].numValue, false))
		std::printf("Disabled %u\n", args[0].numValue);
	else
		std::printf("No breakpoint/watchpoint with id %u\n", args[0].numValue);
}

void CmdEnable(Debugger &dbg, const std::vector<Token> &args)
{
	if (args.empty() || args[0].kind != Token::Kind::Number)
	{
		std::printf("Usage: enable <id>\n");
		return;
	}
	if (dbg.enableById(args[0].numValue, true))
		std::printf("Enabled %u\n", args[0].numValue);
	else
		std::printf("No breakpoint/watchpoint with id %u\n", args[0].numValue);
}

void CmdCommands(Debugger &dbg, const std::vector<Token> &args)
{
	if (args.empty() || args[0].kind != Token::Kind::Number)
	{
		std::printf("Usage: commands <id>\n");
		return;
	}

	uint32_t id = args[0].numValue;

	/* Find the breakpoint */
	bool found = false;
	for (auto &bp : const_cast<std::vector<Debugger::Breakpoint> &>(dbg.breakpoints()))
	{
		if (bp.id == id)
		{
			bp.commands.clear();
			std::printf("Enter commands for breakpoint %u, one per line.\n", id);
			std::printf("Type 'end' to finish.\n");

			char buf[1024];
			while (std::fgets(buf, sizeof(buf), stdin))
			{
				size_t len = std::strlen(buf);
				if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
				if (std::strcmp(buf, "end") == 0) break;
				bp.commands.push_back(buf);
			}
			found = true;
			break;
		}
	}

	if (!found) std::printf("No breakpoint with id %u\n", id);
}
