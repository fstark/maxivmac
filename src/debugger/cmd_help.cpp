/*
	cmd_help.cpp — Help and quit commands
*/

#include "debugger/debugger.h"
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
			std::printf("%.*s", static_cast<int>(entry->helpFull.size()), entry->helpFull.data());
			return;
		}
		return;
	}

	/* General help */
	std::printf("maxivmac debugger commands:\n\n");

	std::printf("Execution:\n");
	std::printf("  run (r)        Start/resume execution\n");
	std::printf("  continue (c)   Continue execution\n");
	std::printf("  step [N] (s)   Step N instructions (into calls)\n");
	std::printf("  stepi [N] (si) Step N machine instructions\n");
	std::printf("  next [N] (n)   Step N instructions (over calls)\n");
	std::printf("  finish (fin)   Run until current function returns\n");
	std::printf("  until <addr>   Run until PC reaches address\n");
	std::printf("\n");

	std::printf("Breakpoints:\n");
	std::printf("  break <loc> [if <cond>] (b)  Set breakpoint\n");
	std::printf("  watch <addr> [len]           Write watchpoint\n");
	std::printf("  rwatch <addr> [len]          Read watchpoint\n");
	std::printf("  awatch <addr> [len]          Access watchpoint\n");
	std::printf("  delete [id] (d)              Delete breakpoint/watchpoint\n");
	std::printf("  disable <id>                 Disable bp/wp\n");
	std::printf("  enable <id>                  Enable bp/wp\n");
	std::printf("  commands <id>                Set auto-execute commands\n");
	std::printf("\n");

	std::printf("Memory:\n");
	std::printf("  x[/FMT] <addr>  Examine memory (FMT: [count][bwl][xdsi])\n");
	std::printf("  print <expr> (p) Evaluate expression\n");
	std::printf("  set <tgt> = <val> Set register or memory\n");
	std::printf("  find <s> <e> <pat> Search memory\n");
	std::printf("\n");

	std::printf("Tracing:\n");
	std::printf("  trace <traps|insn|io> <on|off|names...>\n");
	std::printf("\n");

	std::printf("Information:\n");
	std::printf("  info break      List breakpoints/watchpoints\n");
	std::printf("  info reg        Show registers\n");
	std::printf("  info traps [p]  Search trap dictionary\n");
	std::printf("  info globals [p] Search low-memory globals\n");
	std::printf("  info symbol <a> Reverse symbol lookup\n");
	std::printf("  info insn       Instruction count\n");
	std::printf("  backtrace (bt)  Heuristic stack trace\n");
	std::printf("\n");

	std::printf("Misc:\n");
	std::printf("  help [cmd] (h)  Show help\n");
	std::printf("  quit (q)        Exit emulator\n");
	std::printf("\nEmpty line repeats last command.\n");
}

void CmdQuit(Debugger &, const std::vector<Token> &)
{
	std::printf("Quitting.\n");
	std::exit(0);
}
