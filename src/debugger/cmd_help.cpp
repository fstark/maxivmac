/*
	cmd_help.cpp — Help and quit commands
*/

#include "debugger/debugger.h"
#include "debugger/dbg_io.h"
#include "debugger/cmd_parser.h"

#include <cstdio>
#include <cstdlib>

void CmdHelp(Debugger &dbg, const std::vector<Token> &args)
{
	if (!args.empty() && args[0].kind == Token::Kind::Word)
	{
		/* Detailed help for a specific command */
		auto *table = dbg.commandTable();
		int n = dbg.commandTableSize();
		auto *entry = DispatchCommand(args[0].text, table, n);
		if (entry)
		{
			dbg.io().write("%.*s", static_cast<int>(entry->helpFull.size()),
						   entry->helpFull.data());
			return;
		}
		return;
	}

	/* General help */
	dbg.io().write("maxivmac debugger commands:\n\n");

	dbg.io().write("Execution:\n");
	dbg.io().write("  run (r)        Start/resume execution\n");
	dbg.io().write("  continue (c)   Continue execution\n");
	dbg.io().write("  step [N] (s)   Step N instructions (into calls)\n");
	dbg.io().write("  stepi [N] (si) Step N machine instructions\n");
	dbg.io().write("  next [N] (n)   Step N instructions (over calls)\n");
	dbg.io().write("  finish (fin)   Run until current function returns\n");
	dbg.io().write("  until <addr>   Run until PC reaches address\n");
	dbg.io().write("\n");

	dbg.io().write("Breakpoints:\n");
	dbg.io().write("  break <loc> [if <cond>] (b)  Set breakpoint\n");
	dbg.io().write("  break #<insn>                Break at instruction number\n");
	dbg.io().write("  watch <addr> [len]           Write watchpoint\n");
	dbg.io().write("  rwatch <addr> [len]          Read watchpoint\n");
	dbg.io().write("  awatch <addr> [len]          Access watchpoint\n");
	dbg.io().write("  delete [id] (d)              Delete breakpoint/watchpoint\n");
	dbg.io().write("  disable <id>                 Disable bp/wp\n");
	dbg.io().write("  enable <id>                  Enable bp/wp\n");
	dbg.io().write("  commands <id>                Set auto-execute commands\n");
	dbg.io().write("\n");

	dbg.io().write("Memory:\n");
	dbg.io().write("  x[/FMT] <addr>  Examine memory (FMT: [count][bwl][xdsi])\n");
	dbg.io().write("  print <expr> (p) Evaluate expression\n");
	dbg.io().write("  set <tgt> = <val> Set register or memory\n");
	dbg.io().write("  find <s> <e> <pat> Search memory\n");
	dbg.io().write("\n");

	dbg.io().write("Tracing:\n");
	dbg.io().write("  trace <traps|insn|io> <on|off|names...>\n");
	dbg.io().write("\n");

	dbg.io().write("Information:\n");
	dbg.io().write("  info break      List breakpoints/watchpoints\n");
	dbg.io().write("  info reg        Show registers\n");
	dbg.io().write("  info traps [p]  Search trap dictionary\n");
	dbg.io().write("  info globals [p] [--section S] Search low-memory globals\n");
	dbg.io().write("  info symbol <a> Reverse symbol lookup\n");
	dbg.io().write("  info insn       Instruction count\n");
	dbg.io().write("  info via        VIA1/VIA2 register dump\n");
	dbg.io().write("  info scrap      Guest clipboard contents\n");
	dbg.io().write("  info console    Guest debug console output\n");
	dbg.io().write("  backtrace (bt)  Heuristic stack trace\n");
	dbg.io().write("\n");

	dbg.io().write("Misc:\n");
	dbg.io().write("  help [cmd] (h)  Show help\n");
	dbg.io().write("  quit (q)        Exit emulator\n");
	dbg.io().write("\nEmpty line repeats last command.\n");
}

void CmdQuit(Debugger &dbg, const std::vector<Token> &)
{
	dbg.io().write("Quitting.\n");
	std::exit(0);
}
