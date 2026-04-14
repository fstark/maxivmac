/*
	cmd_trace.cpp — Trace control commands
*/

#include "debugger/debugger.h"
#include "debugger/dbg_io.h"
#include "debugger/cmd_parser.h"
#include "debugger/symbols.h"

#include <cstdio>

void CmdTrace(Debugger &dbg, const std::vector<Token> &args)
{
	if (args.size() < 2 || args[0].kind == Token::Kind::End || args[1].kind == Token::Kind::End)
	{
		dbg.io().write("Usage: trace <traps|insn|io> <on|off|names...>\n");
		return;
	}

	auto &target = args[0].text;
	auto &action = args[1].text;

	if (target == "traps")
	{
		if (action == "on")
		{
			dbg.clearTrapFilter();
			dbg.setTraceTraps(true);
			dbg.io().write("Trap tracing enabled (all traps)\n");
		}
		else if (action == "off")
		{
			dbg.setTraceTraps(false);
			dbg.clearTrapFilter();
			dbg.io().write("Trap tracing disabled\n");
		}
		else
		{
			/* Named trap filter */
			dbg.clearTrapFilter();
			for (size_t i = 1; i < args.size(); ++i)
			{
				if (args[i].kind == Token::Kind::End) break;
				uint32_t addr;
				uint16_t tw;
				if (SymbolsResolve(args[i].text, addr, tw) && tw != 0)
				{
					dbg.addTrapFilter(tw);
					dbg.io().write("  filter: %s ($%04X)\n", args[i].text.c_str(), tw);
				}
				else
				{
					dbg.io().write("  unknown trap: %s\n", args[i].text.c_str());
				}
			}
			dbg.setTraceTraps(true);
			dbg.io().write("Trap tracing enabled (filtered)\n");
		}
	}
	else if (target == "insn")
	{
		if (action == "on")
		{
			dbg.setTraceInsn(true);
			dbg.io().write("Instruction tracing enabled\n");
		}
		else if (action == "off")
		{
			dbg.setTraceInsn(false);
			dbg.io().write("Instruction tracing disabled\n");
		}
		else
		{
			dbg.io().write("Usage: trace insn <on|off>\n");
		}
	}
	else if (target == "io")
	{
		if (action == "on")
		{
			dbg.setTraceIO(true);
			dbg.io().write("I/O tracing enabled\n");
		}
		else if (action == "off")
		{
			dbg.setTraceIO(false);
			dbg.io().write("I/O tracing disabled\n");
		}
		else
		{
			dbg.io().write("Usage: trace io <on|off>\n");
		}
	}
	else
	{
		dbg.io().write("Unknown trace target '%s'. Use: traps, insn, io\n", target.c_str());
	}
}
