/*
	cmd_info.cpp — Info commands: break, reg, traps, globals, symbol, insn, backtrace
*/

#include "debugger/debugger.h"
#include "debugger/cmd_parser.h"
#include "debugger/symbols.h"

#include "core/machine.h"
#include "cpu/m68k.h"
#include "cpu/trap_counter.h"

#include <cstdio>

extern uint32_t g_instructionCount;

/* ── SR flag formatting ─────────────────────────────── */

static void FormatSRFlags(uint16_t sr, char *buf)
{
	buf[0] = (sr & 0x2000) ? 'S' : '-'; /* Supervisor */
	buf[1] = (sr & 0x1000) ? 'M' : '-'; /* Master/Interrupt (68020) */
	buf[2] = (sr & 0x0010) ? 'X' : '-'; /* Extend */
	buf[3] = (sr & 0x0008) ? 'N' : '-'; /* Negative */
	buf[4] = (sr & 0x0004) ? 'Z' : '-'; /* Zero */
	buf[5] = (sr & 0x0002) ? 'V' : '-'; /* Overflow */
	buf[6] = (sr & 0x0001) ? 'C' : '-'; /* Carry */
	buf[7] = '\0';
}

/* ── Info subcommands ───────────────────────────────── */

static void InfoBreak(Debugger &dbg)
{
	auto &bps = dbg.breakpoints();
	auto &wps = dbg.watchpoints();

	if (bps.empty() && wps.empty())
	{
		std::printf("No breakpoints or watchpoints.\n");
		return;
	}

	std::printf("Num  Type        Enb  Address     What\n");
	for (auto &bp : bps)
	{
		const char *enb = bp.enabled ? "y" : "n";
		if (bp.trapWord)
		{
			const char *name = trap_dict_name(bp.trapWord);
			if (name)
				std::printf("%-4u breakpoint  %s    $%04X       trap %s\n", bp.id, enb, bp.trapWord,
							name);
			else
				std::printf("%-4u breakpoint  %s    $%04X       trap\n", bp.id, enb, bp.trapWord);
		}
		else
		{
			auto sym = SymbolsAtAddress(bp.address);
			if (!sym.empty())
				std::printf("%-4u breakpoint  %s    $%08X  %.*s\n", bp.id, enb, bp.address,
							static_cast<int>(sym.size()), sym.data());
			else
				std::printf("%-4u breakpoint  %s    $%08X\n", bp.id, enb, bp.address);
		}
		if (!bp.condition.empty()) std::printf("     condition: %s\n", bp.condition.c_str());
		if (!bp.commands.empty()) std::printf("     %zu auto-command(s)\n", bp.commands.size());
	}

	for (auto &wp : wps)
	{
		const char *enb = wp.enabled ? "y" : "n";
		const char *mode = (wp.mode == 'W') ? "write" : (wp.mode == 'R') ? "read" : "access";
		auto sym = SymbolsAtAddress(wp.address);
		if (!sym.empty())
			std::printf("%-4u watchpoint  %s    $%08X  %s len=%u (%.*s)\n", wp.id, enb, wp.address,
						mode, wp.length, static_cast<int>(sym.size()), sym.data());
		else
			std::printf("%-4u watchpoint  %s    $%08X  %s len=%u\n", wp.id, enb, wp.address, mode,
						wp.length);
	}
}

static void InfoReg()
{
	uint32_t d[8], a[8];
	m68k_getRegs(d, a);
	uint32_t pc = m68k_getPC_public();
	uint16_t sr = m68k_getSR_public();

	std::printf("D0=%08X  D1=%08X  D2=%08X  D3=%08X\n", d[0], d[1], d[2], d[3]);
	std::printf("D4=%08X  D5=%08X  D6=%08X  D7=%08X\n", d[4], d[5], d[6], d[7]);
	std::printf("A0=%08X  A1=%08X  A2=%08X  A3=%08X\n", a[0], a[1], a[2], a[3]);
	std::printf("A4=%08X  A5=%08X  A6=%08X  A7=%08X\n", a[4], a[5], a[6], a[7]);

	char flags[8];
	FormatSRFlags(sr, flags);
	std::printf("PC=%08X  SR=%04X [%s]  USP=%08X  ISP=%08X\n", pc, sr, flags, m68k_getUSP(),
				m68k_getISP());
}

static void InfoTraps(const std::vector<Token> &args)
{
	std::string_view prefix;
	std::string prefixStr;
	if (args.size() > 1 && args[1].kind == Token::Kind::Word)
	{
		prefixStr = args[1].text;
		prefix = prefixStr;
	}

	std::vector<SymbolEntry> results;
	SymbolsSearch(prefix, 't', results, 50);

	std::printf("%-20s  TrapWord\n", "Name");
	for (auto &e : results)
		std::printf("%-20.*s  $%04X\n", static_cast<int>(e.name.size()), e.name.data(), e.trapWord);
	std::printf("(%zu results)\n", results.size());
}

static void InfoGlobals(const std::vector<Token> &args)
{
	std::string_view prefix;
	std::string prefixStr;
	if (args.size() > 1 && args[1].kind == Token::Kind::Word)
	{
		prefixStr = args[1].text;
		prefix = prefixStr;
	}

	std::vector<SymbolEntry> results;
	SymbolsSearch(prefix, 'g', results, 50);

	std::printf("%-20s  Address   Size\n", "Name");
	for (auto &e : results)
		std::printf("%-20.*s  $%04X     %u\n", static_cast<int>(e.name.size()), e.name.data(),
					e.address, e.size);
	std::printf("(%zu results)\n", results.size());
}

static void InfoSymbol(const std::vector<Token> &args)
{
	if (args.size() < 2 || args[1].kind == Token::Kind::End)
	{
		std::printf("Usage: info symbol <addr>\n");
		return;
	}

	uint32_t addr = 0;
	if (args[1].kind == Token::Kind::Number)
		addr = args[1].numValue;
	else
	{
		std::printf("Expected address\n");
		return;
	}

	auto name = SymbolsAtAddress(addr);
	if (!name.empty())
		std::printf("$%08X = %.*s\n", addr, static_cast<int>(name.size()), name.data());
	else
		std::printf("No symbol at $%08X\n", addr);
}

static void InfoInsn()
{
	std::printf("Instructions executed: %u\n", g_instructionCount);
}

void CmdInfo(Debugger &dbg, const std::vector<Token> &args)
{
	if (args.empty() || args[0].kind == Token::Kind::End)
	{
		std::printf("Usage: info <break|reg|traps|globals|symbol|insn>\n");
		return;
	}

	auto &sub = args[0].text;
	if (sub == "break" || sub == "b")
		InfoBreak(dbg);
	else if (sub == "reg" || sub == "r")
		InfoReg();
	else if (sub == "traps")
		InfoTraps(args);
	else if (sub == "globals")
		InfoGlobals(args);
	else if (sub == "symbol")
		InfoSymbol(args);
	else if (sub == "insn")
		InfoInsn();
	else
		std::printf("Unknown info sub-command '%s'.\n"
					"  Available: break, reg, traps, globals, symbol, insn\n",
					sub.c_str());
}

void CmdBacktrace(Debugger &dbg, const std::vector<Token> &)
{
	(void)dbg;

	uint32_t d[8], a[8];
	m68k_getRegs(d, a);
	uint32_t sp = a[7];
	uint32_t pc = m68k_getPC_public();

	std::printf("#0  $%08X", pc);
	auto sym = SymbolsAtAddress(pc);
	if (!sym.empty()) std::printf(" (%.*s)", static_cast<int>(sym.size()), sym.data());
	std::printf("\n");

	/* Heuristic: scan stack for return addresses in ROM/RAM range */
	int frame = 1;
	for (uint32_t offset = 0; offset < 0x1000 && frame < 20; offset += 2)
	{
		uint32_t val = get_vm_long(sp + offset);

		/* Check if this looks like a plausible address:
		   - ROM range ($00400000-$004FFFFF) or ($00000000-$00FFFFFF) typical RAM */
		bool plausible = (val >= 0x1000 && val < 0x01000000) ||	  /* RAM */
						 (val >= 0x00400000 && val < 0x00500000); /* ROM */

		if (!plausible) continue;

		/* Check if preceding instruction at val-2 or val-4 is JSR/BSR */
		uint16_t prev2 = get_vm_word(val - 2);
		uint16_t prev4 = get_vm_word(val - 4);
		uint16_t prev6 = get_vm_word(val - 6);

		bool isReturn = false;
		if ((prev6 & 0xFFC0) == 0x4E80) isReturn = true; /* JSR (6-byte) */
		if ((prev4 & 0xFFC0) == 0x4E80) isReturn = true; /* JSR (4-byte) */
		if ((prev4 & 0xFF00) == 0x6100) isReturn = true; /* BSR (4-byte) */
		if ((prev2 & 0xFF00) == 0x6100) isReturn = true; /* BSR.S */
		if ((prev2 & 0xF000) == 0xA000) isReturn = true; /* A-line trap */

		if (isReturn)
		{
			std::printf("#%-2d $%08X", frame, val);
			auto name = SymbolsAtAddress(val);
			if (!name.empty()) std::printf(" (%.*s)", static_cast<int>(name.size()), name.data());
			std::printf("  [SP+$%X]\n", offset);
			++frame;
		}
	}

	if (frame == 1) std::printf("  (no return addresses found on stack)\n");
}
