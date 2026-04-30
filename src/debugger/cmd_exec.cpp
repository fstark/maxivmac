/*
	cmd_exec.cpp — Execution commands: run, continue, step, stepi, next, finish, until
*/

#include "debugger/debugger.h"
#include "debugger/dbg_io.h"
#include "debugger/cmd_parser.h"
#include "debugger/symbols.h"

#include "core/machine.h"
#include "cpu/m68k.h"

#include <cstdio>

void CmdRun(Debugger &dbg, const std::vector<Token> &)
{
	dbg.io().write("[running]\n");
	dbg.setRunning();
}

void CmdContinue(Debugger &dbg, const std::vector<Token> &)
{
	dbg.io().write("[continuing]\n");
	dbg.setRunning();
}

void CmdStep(Debugger &dbg, const std::vector<Token> &args)
{
	uint32_t n = 1;
	if (!args.empty() && args[0].isNumber()) n = args[0].numValue;
	if (n == 0) n = 1;
	dbg.setStepping(n);
}

void CmdStepi(Debugger &dbg, const std::vector<Token> &args)
{
	CmdStep(dbg, args);
}

void CmdNext(Debugger &dbg, const std::vector<Token> &args)
{
	uint32_t n = 1;
	if (!args.empty() && args[0].isNumber()) n = args[0].numValue;
	if (n == 0) n = 1;

	/* Read opcode at current PC */
	uint32_t pc = m68k_getPC_public();
	uint16_t opcode = get_vm_word(pc);

	/* BSR: 0110xxx[xx], JSR: 0100111010xxxxxx, A-line trap: 1010xxxxxxxxxxxx */
	bool isCall = false;
	if ((opcode & 0xFF00) == 0x6100) isCall = true; /* BSR */
	if ((opcode & 0xFFC0) == 0x4E80) isCall = true; /* JSR */
	if ((opcode & 0xF000) == 0xA000) isCall = true; /* A-line trap */

	if (isCall)
	{
		uint32_t d[8], a[8];
		m68k_getRegs(d, a);
		dbg.setNexting(a[7], n); /* saved SP + remaining count */
	}
	else
	{
		dbg.setStepping(n);
	}
}

void CmdFinish(Debugger &dbg, const std::vector<Token> &)
{
	uint32_t d[8], a[8];
	m68k_getRegs(d, a);
	dbg.io().write("[finishing, SP=$%08X]\n", a[7]);
	dbg.setFinishing(a[7]);
}

void CmdUntil(Debugger &dbg, const std::vector<Token> &args)
{
	if (args.empty() || args[0].isEnd())
	{
		dbg.io().write("Usage: until <addr>\n");
		return;
	}

	uint32_t addr = 0;
	if (args[0].isNumber())
	{
		addr = args[0].numValue;
	}
	else
	{
		/* Try symbol lookup */
		uint16_t tw;
		if (!SymbolsResolve(args[0].text, addr, tw))
		{
			dbg.io().write("Cannot resolve '%s'\n", args[0].text.c_str());
			return;
		}
	}

	dbg.io().write("[running until $%08X]\n", addr);
	dbg.setUntil(addr);
}

void CmdSource(Debugger &dbg, const std::vector<Token> &args)
{
	if (args.empty() || args[0].isEnd())
	{
		dbg.io().write("Usage: source <path>\n");
		return;
	}
	SourceFile(dbg, args[0].text);
}
