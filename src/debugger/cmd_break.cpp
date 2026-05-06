/*
	cmd_break.cpp — Breakpoint and watchpoint commands
*/

#include "debugger/debugger.h"
#include "debugger/dbg_io.h"
#include "debugger/cmd_parser.h"
#include "debugger/symbols.h"

#include "core/machine.h"
#include "cpu/trap_counter.h"

#include <cinttypes>
#include <cstdio>
#include <cstring>

/* Parse a watchpoint value condition: "if val <op> <value>" */
static bool ParseValCond(DbgIO &io, const std::vector<Token> &args, int startIdx, bool &hasValCond,
						 uint8_t &valCondOp, uint32_t &valCondValue)
{
	hasValCond = false;
	valCondOp = 0;
	valCondValue = 0;

	/* Look for "if" "val" <op> <value> */
	int i = startIdx;
	auto end = static_cast<int>(args.size());
	while (i < end && !args[i].isEnd())
	{
		if (args[i].isWord("if"))
		{
			++i;
			if (i < end && args[i].isWord("val"))
			{
				++i;
				if (i < end && args[i].isOperator())
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
						io.write("Unknown watchpoint operator '%s'\n", op.c_str());
						return false;
					}
					++i;
					if (i < end && args[i].isNumber())
					{
						valCondValue = args[i].numValue;
						hasValCond = true;
						return true;
					}
					io.write("Expected value after operator\n");
					return false;
				}
			}
			io.write("Expected 'val <op> <value>' after 'if'\n");
			return false;
		}
		++i;
	}
	return true;
}

void CmdBreak(Debugger &dbg, const std::vector<Token> &args)
{
	if (args.empty() || args[0].isEnd())
	{
		dbg.io().write("Usage: break <location> [if <cond>]\n");
		dbg.io().write("       break #<insn>   Break at instruction number\n");
		return;
	}

	/* break #N or break #-N — instruction-count breakpoint */
	if (args[0].isOperator("#") && args.size() >= 2)
	{
		bool negative = false;
		size_t numIdx = 1;

		if (args[1].isOperator("-") && args.size() >= 3 && args[2].isNumber())
		{
			negative = true;
			numIdx = 2;
		}
		else if (!args[1].isNumber())
		{
			dbg.io().write("Usage: break #<N> or break #-<N>\n");
			return;
		}

		uint32_t n = args[numIdx].numValue;
		if (negative)
		{
			if (n > g_instructionCount)
			{
				dbg.io().write("Error: offset %u exceeds current insn count %" PRIu64 "\n", n,
							   g_instructionCount);
				return;
			}
			n = static_cast<uint32_t>(g_instructionCount - n);
			dbg.io().write("(resolved to instruction #%u)\n", n);
		}
		uint32_t id = dbg.setInsnBreak(n);
		dbg.io().write("Breakpoint %u at instruction #%u\n", id, n);
		return;
	}

	uint32_t addr = 0;
	uint16_t trapWord = 0;
	uint16_t subtrapSelector = 0;

	if (args[0].isNumber())
	{
		addr = args[0].numValue;
	}
	else if (args[0].isWord())
	{
		if (!SymbolsResolve(args[0].text, addr, trapWord, subtrapSelector))
		{
			dbg.io().write("Cannot resolve '%s'\n", args[0].text.c_str());
			return;
		}
	}
	else
	{
		dbg.io().write("Expected address or symbol name\n");
		return;
	}

	/* Parse optional condition */
	std::string condition;
	for (size_t i = 1; i < args.size(); ++i)
	{
		if (args[i].isWord("if"))
		{
			/* Capture everything after "if" as the condition */
			for (size_t j = i + 1; j < args.size(); ++j)
			{
				if (args[j].isEnd()) break;
				if (!condition.empty()) condition += ' ';
				condition += args[j].text;
			}
			break;
		}
	}

	uint32_t id = dbg.addBreakpoint(addr, trapWord, subtrapSelector, condition);

	if (subtrapSelector != 0)
	{
		dbg.io().write("Breakpoint %u on subtrap %s ($%04X sel $%02X)\n", id,
					   SymbolsSubtrapName(trapWord, subtrapSelector), trapWord, subtrapSelector);
	}
	else if (trapWord)
	{
		dbg.io().write("Breakpoint %u on trap %s ($%04X)\n", id, SymbolsTrapName(trapWord),
					   trapWord);
	}
	else
	{
		auto sym = SymbolsAtAddress(addr);
		if (!sym.empty())
			dbg.io().write("Breakpoint %u at $%08X (%.*s)\n", id, addr,
						   static_cast<int>(sym.size()), sym.data());
		else
			dbg.io().write("Breakpoint %u at $%08X\n", id, addr);
	}

	if (!condition.empty()) dbg.io().write("  condition: %s\n", condition.c_str());
}

static void DoWatch(Debugger &dbg, const std::vector<Token> &args, char mode)
{
	if (args.empty() || args[0].isEnd())
	{
		dbg.io().write("Usage: %cwatch <addr> [len] [if val <op> <value>]\n",
					   mode == 'R' ? 'r' : (mode == 'A' ? 'a' : ' '));
		return;
	}

	uint32_t addr = 0;
	uint32_t len = 1;
	int nextArg = 1;

	if (args[0].isNumber())
	{
		addr = args[0].numValue;
	}
	else if (args[0].isWord())
	{
		uint16_t tw;
		if (!SymbolsResolve(args[0].text, addr, tw))
		{
			dbg.io().write("Cannot resolve '%s'\n", args[0].text.c_str());
			return;
		}
		/* Auto-detect size from global */
		uint16_t sz = SymbolsSizeAt(addr);
		if (sz > 0) len = sz;
	}

	/* Optional explicit length */
	if (nextArg < static_cast<int>(args.size()) && args[nextArg].isNumber())
	{
		len = args[nextArg].numValue;
		++nextArg;
	}

	bool hasValCond;
	uint8_t valCondOp;
	uint32_t valCondValue;
	if (!ParseValCond(dbg.io(), args, nextArg, hasValCond, valCondOp, valCondValue)) return;

	uint32_t id = dbg.addWatchpoint(addr, len, mode, hasValCond, valCondOp, valCondValue);

	const char *modeStr = (mode == 'W') ? "write" : (mode == 'R') ? "read" : "access";
	auto sym = SymbolsAtAddress(addr);
	if (!sym.empty())
		dbg.io().write("Watchpoint %u: %s $%08X-%08X (%.*s)\n", id, modeStr, addr, addr + len,
					   static_cast<int>(sym.size()), sym.data());
	else
		dbg.io().write("Watchpoint %u: %s $%08X-%08X\n", id, modeStr, addr, addr + len);
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
	if (args.empty() || args[0].isEnd())
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
		dbg.io().write("Deleted %d breakpoints/watchpoints\n", count);
		return;
	}

	if (args[0].isNumber())
	{
		if (dbg.deleteById(args[0].numValue))
			dbg.io().write("Deleted %u\n", args[0].numValue);
		else
			dbg.io().write("No breakpoint/watchpoint with id %u\n", args[0].numValue);
	}
}

void CmdDisable(Debugger &dbg, const std::vector<Token> &args)
{
	if (args.empty() || !args[0].isNumber())
	{
		dbg.io().write("Usage: disable <id>\n");
		return;
	}
	if (dbg.enableById(args[0].numValue, false))
		dbg.io().write("Disabled %u\n", args[0].numValue);
	else
		dbg.io().write("No breakpoint/watchpoint with id %u\n", args[0].numValue);
}

void CmdEnable(Debugger &dbg, const std::vector<Token> &args)
{
	if (args.empty() || !args[0].isNumber())
	{
		dbg.io().write("Usage: enable <id>\n");
		return;
	}
	if (dbg.enableById(args[0].numValue, true))
		dbg.io().write("Enabled %u\n", args[0].numValue);
	else
		dbg.io().write("No breakpoint/watchpoint with id %u\n", args[0].numValue);
}

void CmdCommands(Debugger &dbg, const std::vector<Token> &args)
{
	if (args.empty() || !args[0].isNumber())
	{
		dbg.io().write("Usage: commands <id>\n");
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
			dbg.io().write("Enter commands for breakpoint %u, one per line.\n", id);
			dbg.io().write("Type 'end' to finish.\n");

			char buf[1024];
			while (dbg.io().readLine(buf, sizeof(buf)))
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

	if (!found) dbg.io().write("No breakpoint with id %u\n", id);
}

void CmdIgnore(Debugger &dbg, const std::vector<Token> &args)
{
	if (args.size() < 2 || !args[0].isNumber() || !args[1].isNumber())
	{
		dbg.io().write("Usage: ignore <breakpoint-id> <count>\n");
		return;
	}

	uint32_t id = args[0].numValue;
	uint32_t count = args[1].numValue;

	for (auto &bp : dbg.breakpoints())
	{
		if (bp.id == id)
		{
			bp.ignoreCount = count;
			dbg.io().write("Will ignore next %u crossings of breakpoint %u.\n", count, id);
			return;
		}
	}
	dbg.io().write("No breakpoint %u.\n", id);
}

void CmdTbreak(Debugger &dbg, const std::vector<Token> &args)
{
	// Create a regular breakpoint, then mark it temporary
	CmdBreak(dbg, args);
	// The breakpoint just created is the last one in the vector
	auto &bps = dbg.breakpoints();
	if (!bps.empty()) bps.back().temporary = true;
}
