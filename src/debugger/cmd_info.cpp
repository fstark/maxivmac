/*
	cmd_info.cpp — Info commands: break, reg, traps, globals, symbol, insn, backtrace
*/

#include "debugger/debugger.h"
#include "debugger/dbg_io.h"
#include "debugger/cmd_parser.h"
#include "debugger/symbols.h"

#include "core/extn_clip.h"
#include "core/machine.h"
#include "core/machine_obj.h"
#include "cpu/m68k.h"
#include "cpu/trap_counter.h"

#include "devices/via.h"
#include "devices/via2.h"

#include "lang/type_registry.h"
#include "lang/global_registry.h"

#include "util/macroman.h"

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
	auto &io = dbg.io();

	if (bps.empty() && wps.empty() && dbg.insnBreakCount() == 0)
	{
		io.write("No breakpoints or watchpoints.\n");
		return;
	}

	io.write("Num  Type        Enb  Address     What\n");

	/* Instruction-count breakpoint (if set) */
	if (dbg.insnBreakCount() != 0)
	{
		dbg.io().write("%-4u insn-break  y    -           at instruction #%u\n", dbg.insnBreakId(),
					   dbg.insnBreakCount());
	}

	for (auto &bp : bps)
	{
		const char *enb = bp.enabled ? "y" : "n";
		if (bp.trapWord)
		{
			dbg.io().write("%-4u breakpoint  %s    $%04X       trap %s\n", bp.id, enb, bp.trapWord,
						   SymbolsTrapName(bp.trapWord));
		}
		else
		{
			auto sym = SymbolsAtAddress(bp.address);
			if (!sym.empty())
				dbg.io().write("%-4u breakpoint  %s    $%08X  %.*s\n", bp.id, enb, bp.address,
							   static_cast<int>(sym.size()), sym.data());
			else
				dbg.io().write("%-4u breakpoint  %s    $%08X\n", bp.id, enb, bp.address);
		}
		if (!bp.condition.empty()) dbg.io().write("     condition: %s\n", bp.condition.c_str());
		if (!bp.commands.empty()) dbg.io().write("     %zu auto-command(s)\n", bp.commands.size());
		if (bp.ignoreCount > 0) dbg.io().write("     ignore next: %u\n", bp.ignoreCount);
	}

	for (auto &wp : wps)
	{
		const char *enb = wp.enabled ? "y" : "n";
		const char *mode = (wp.mode == 'W') ? "write" : (wp.mode == 'R') ? "read" : "access";
		auto sym = SymbolsAtAddress(wp.address);
		if (!sym.empty())
			dbg.io().write("%-4u watchpoint  %s    $%08X  %s len=%u (%.*s)\n", wp.id, enb,
						   wp.address, mode, wp.length, static_cast<int>(sym.size()), sym.data());
		else
			dbg.io().write("%-4u watchpoint  %s    $%08X  %s len=%u\n", wp.id, enb, wp.address,
						   mode, wp.length);
	}
}

static void InfoReg(Debugger &dbg)
{
	uint32_t d[8], a[8];
	m68k_getRegs(d, a);
	uint32_t pc = m68k_getPC_public();
	uint16_t sr = m68k_getSR_public();

	dbg.io().write("D0=%08X  D1=%08X  D2=%08X  D3=%08X\n", d[0], d[1], d[2], d[3]);
	dbg.io().write("D4=%08X  D5=%08X  D6=%08X  D7=%08X\n", d[4], d[5], d[6], d[7]);
	dbg.io().write("A0=%08X  A1=%08X  A2=%08X  A3=%08X\n", a[0], a[1], a[2], a[3]);
	dbg.io().write("A4=%08X  A5=%08X  A6=%08X  A7=%08X\n", a[4], a[5], a[6], a[7]);

	char flags[8];
	FormatSRFlags(sr, flags);
	dbg.io().write("PC=%08X  SR=%04X [%s]  USP=%08X  ISP=%08X\n", pc, sr, flags, m68k_getUSP(),
				   m68k_getISP());
}

static void InfoTraps(Debugger &dbg, const std::vector<Token> &args)
{
	std::string_view prefix;
	std::string prefixStr;
	if (args.size() > 1 && args[1].isWord())
	{
		prefixStr = args[1].text;
		prefix = prefixStr;
	}

	std::vector<SymbolEntry> results;
	SymbolsSearch(prefix, 't', results, 50);

	dbg.io().write("%-20s  TrapWord\n", "Name");
	for (auto &e : results)
		dbg.io().write("%-20.*s  $%04X\n", static_cast<int>(e.name.size()), e.name.data(),
					   e.trapWord);
	dbg.io().write("(%zu results)\n", results.size());
}

static void InfoGlobals(Debugger &dbg, const std::vector<Token> &args)
{
	std::string_view prefix;
	std::string prefixStr;
	std::string sectionFilter;

	/* Parse args: info globals [prefix] [--section NAME] */
	for (size_t i = 1; i < args.size(); ++i)
	{
		if (args[i].isEnd()) break;
		if (args[i].isOperator("--") && i + 1 < args.size() && args[i + 1].isWord("section") &&
			i + 2 < args.size() && !args[i + 2].isEnd())
		{
			sectionFilter = args[i + 2].text;
			i += 2; /* skip section, NAME */
			continue;
		}
		if (args[i].isWord() && prefixStr.empty())
		{
			prefixStr = args[i].text;
			prefix = prefixStr;
		}
	}

	if (!sectionFilter.empty())
	{
		/* Section-filtered listing from GlobalRegistry */
		auto &reg = g_globalRegistry();
		dbg.io().write("%-20s  Address   Size\n", "Name");
		int count = 0;
		for (auto &gd : reg.globals())
		{
			if (gd.section != sectionFilter) continue;
			if (!prefix.empty() &&
				!(gd.name.size() >= prefix.size() &&
				  strncasecmp(gd.name.data(), prefix.data(), prefix.size()) == 0))
				continue;
			dbg.io().write("%-20.*s  $%04X     %u\n", static_cast<int>(gd.name.size()),
						   gd.name.data(), gd.addr, gd.size);
			if (++count >= 50) break;
		}
		dbg.io().write("(%d results, section=%s)\n", count, sectionFilter.c_str());
	}
	else
	{
		/* Original prefix-search behavior */
		std::vector<SymbolEntry> results;
		SymbolsSearch(prefix, 'g', results, 50);
		dbg.io().write("%-20s  Address   Size\n", "Name");
		for (auto &e : results)
			dbg.io().write("%-20.*s  $%04X     %u\n", static_cast<int>(e.name.size()),
						   e.name.data(), e.address, e.size);
		dbg.io().write("(%zu results)\n", results.size());
	}
}

static void InfoSymbol(Debugger &dbg, const std::vector<Token> &args)
{
	if (args.size() < 2 || args[1].isEnd())
	{
		dbg.io().write("Usage: info symbol <addr>\n");
		return;
	}

	uint32_t addr = 0;
	if (args[1].isNumber())
		addr = args[1].numValue;
	else
	{
		dbg.io().write("Expected address\n");
		return;
	}

	auto name = SymbolsAtAddress(addr);
	if (!name.empty())
		dbg.io().write("$%08X = %.*s\n", addr, static_cast<int>(name.size()), name.data());
	else
		dbg.io().write("No symbol at $%08X\n", addr);
}

static void InfoInsn(Debugger &dbg)
{
	dbg.io().write("Instructions executed: %u\n", g_instructionCount);
}

static void InfoVIA(Debugger &dbg)
{
	if (!g_machine)
	{
		dbg.io().write("Machine not initialized.\n");
		return;
	}

	auto dumpVIA = [&](const char *label, VIABase *via)
	{
		if (!via)
		{
			dbg.io().write("%s: not present\n", label);
			return;
		}
		auto &d = via->d_;
		dbg.io().write("%s:\n", label);
		dbg.io().write("  ORA=%02X  ORB=%02X  DDRA=%02X  DDRB=%02X\n", d.ORA, d.ORB, d.DDR_A,
					   d.DDR_B);
		dbg.io().write("  T1C=%08X  T1L=%02X%02X  T2C=%08X  T2L=%02X\n", d.T1C_F, d.T1L_H, d.T1L_L,
					   d.T2C_F, d.T2L_L);
		dbg.io().write("  SR=%02X  ACR=%02X  PCR=%02X  IFR=%02X  IER=%02X\n", d.SR, d.ACR, d.PCR,
					   d.IFR, d.IER);
		dbg.io().write("  T1Active=%d  T2Active=%d\n", via->T1_Active, via->T2_Active);
	};

	dumpVIA("VIA1", g_machine->findDevice<VIA1Device>());
	dumpVIA("VIA2", g_machine->findDevice<VIA2Device>());
}

static void InfoScrap(Debugger &dbg)
{
	uint32_t scrapSize = get_vm_long(0x0960);
	uint32_t scrapHandle = get_vm_long(0x0964);
	int16_t scrapCount = static_cast<int16_t>(get_vm_word(0x0968));
	int16_t scrapState = static_cast<int16_t>(get_vm_word(0x096A));

	dbg.io().write("ScrapSize=%u  ScrapHandle=$%08X  ScrapCount=%d  ScrapState=%d", scrapSize,
				   scrapHandle, scrapCount, scrapState);
	if (scrapState > 0)
		dbg.io().write(" (in memory)\n");
	else if (scrapState == 0)
		dbg.io().write(" (on disk)\n");
	else
		dbg.io().write(" (uninitialized)\n");

	if (scrapState <= 0 || scrapHandle == 0) return;

	uint32_t masterPtr = get_vm_long(scrapHandle);
	if (masterPtr == 0)
	{
		dbg.io().write("Master pointer is NULL (purged?)\n");
		return;
	}

	uint32_t ramSz = g_machine ? g_machine->ramSize() : 0;
	uint32_t offset = 0;
	int entryIdx = 0;

	while (offset + 8 <= scrapSize)
	{
		uint32_t entryAddr = masterPtr + offset;
		if (entryAddr + 8 > ramSz) break;

		char type[5];
		for (int i = 0; i < 4; i++)
			type[i] = static_cast<char>(get_vm_byte(entryAddr + static_cast<uint32_t>(i)));
		type[4] = '\0';

		uint32_t entryLen = get_vm_long(entryAddr + 4);
		if (entryLen > scrapSize - offset - 8) break;

		uint32_t dataAddr = entryAddr + 8;
		dbg.io().write("Entry %d: '%s' %u bytes @$%08X\n", entryIdx, type, entryLen, dataAddr);

		if (entryLen > 0 && memcmp(type, "TEXT", 4) == 0)
		{
			uint32_t previewLen = (entryLen < 4096) ? entryLen : 4096;
			std::vector<uint8_t> buf(previewLen);
			for (uint32_t i = 0; i < previewLen; i++)
				buf[i] = get_vm_byte(dataAddr + i);
			std::string display = UTF8FromMacRoman({buf.data(), previewLen});
			for (auto &c : display)
				if (c == '\r') c = '\n';
			if (entryLen > previewLen) display += "...";
			dbg.io().write("  %s\n", display.c_str());
		}
		else if (entryLen > 0)
		{
			uint32_t dumpLen = (entryLen < 128) ? entryLen : 128;
			for (uint32_t row = 0; row < dumpLen; row += 16)
			{
				dbg.io().write("  %04X  ", row);
				uint32_t cols = (dumpLen - row < 16) ? dumpLen - row : 16;
				for (uint32_t c = 0; c < cols; c++)
					dbg.io().write("%02X ", get_vm_byte(dataAddr + row + c));
				dbg.io().write("\n");
			}
		}

		offset += 8 + entryLen;
		if (offset & 1) offset++;
		entryIdx++;
	}
}

static void InfoConsole(Debugger &dbg, const std::vector<Token> &args)
{
	/* info console clear — clear the buffer */
	if (args.size() > 1 && args[1].isWord("clear"))
	{
		ExtnDbgConsoleClear();
		dbg.io().write("Console cleared.\n");
		return;
	}

	const auto &lines = extnDbgConsoleLines();
	if (lines.empty())
	{
		dbg.io().write("[guest console — empty]\n");
		return;
	}

	dbg.io().write("[guest console — %zu line(s)]\n", lines.size());
	for (auto &line : lines)
		dbg.io().write("%s\n", line.c_str());
}

static void InfoTypes(Debugger &dbg, const std::vector<Token> &args)
{
	std::string_view prefix;
	std::string prefixStr;
	if (args.size() > 1 && args[1].isWord())
	{

		prefixStr = args[1].text;
		prefix = prefixStr;
	}

	auto all = g_typeRegistry().typeNames();

	dbg.io().write("%-24s  Kind     Size\n", "Name");
	int count = 0;
	for (auto &ti : all)
	{
		if (!prefix.empty() &&
			!(ti.name.size() >= prefix.size() && ti.name.substr(0, prefix.size()) == prefix))
			continue;
		dbg.io().write("%-24.*s  %-6s   %u\n", static_cast<int>(ti.name.size()), ti.name.data(),
					   ti.isUnion ? "union" : "struct", ti.size);
		++count;
	}
	dbg.io().write("(%d results)\n", count);
}

static void WriteTrapList(DbgIO &io, const char *prefix, const std::vector<uint16_t> &traps)
{
	io.write(" ");
	for (uint16_t tw : traps)
		io.write(" %s%s", prefix, SymbolsTrapName(tw));
	io.write("\n");
}

static void InfoTrace(Debugger &dbg)
{
	auto &io = dbg.io();

	if (dbg.traceTraps())
	{
		auto enabled = dbg.trapsEnabled();
		auto disabled = dbg.trapsDisabled();

		if (disabled.empty())
		{
			io.write("Trap tracing: on\n");
		}
		else if (enabled.empty())
		{
			io.write("Trap tracing: off\n");
		}
		else if (enabled.size() <= disabled.size())
		{
			io.write("Trap tracing:\n");
			WriteTrapList(io, "+", enabled);
		}
		else
		{
			io.write("Trap tracing:\n");
			WriteTrapList(io, "-", disabled);
		}
	}
	else
	{
		io.write("Trap tracing: off\n");
	}

	io.write("Insn tracing: %s\n", dbg.traceInsn() ? "on" : "off");
	io.write("I/O tracing:  %s\n", dbg.traceIO() ? "on" : "off");
}

void CmdInfo(Debugger &dbg, const std::vector<Token> &args)
{
	if (args.empty() || args[0].isEnd())
	{
		dbg.io().write("Usage: info <break|reg|trace|traps|globals|types|symbol|insn>\n");
		return;
	}

	auto &sub = args[0].text;
	if (sub == "break" || sub == "b")
		InfoBreak(dbg);
	else if (sub == "reg" || sub == "r")
		InfoReg(dbg);
	else if (sub == "traps")
		InfoTraps(dbg, args);
	else if (sub == "globals")
		InfoGlobals(dbg, args);
	else if (sub == "symbol")
		InfoSymbol(dbg, args);
	else if (sub == "insn")
		InfoInsn(dbg);
	else if (sub == "trace")
		InfoTrace(dbg);
	else if (sub == "types")
		InfoTypes(dbg, args);
	else if (sub == "via")
		InfoVIA(dbg);
	else if (sub == "scrap")
		InfoScrap(dbg);
	else if (sub == "console")
		InfoConsole(dbg, args);
	else
		dbg.io().write("Unknown info sub-command '%s'.\n"
					   "  Available: break, reg, trace, traps, globals, types, symbol, insn, via, "
					   "scrap, console\n",
					   sub.c_str());
}

void CmdBacktrace(Debugger &dbg, const std::vector<Token> &)
{

	uint32_t d[8], a[8];
	m68k_getRegs(d, a);
	uint32_t sp = a[7];
	uint32_t pc = m68k_getPC_public();

	dbg.io().write("#0  $%08X", pc);
	auto sym = SymbolsAtAddress(pc);
	if (!sym.empty()) dbg.io().write(" (%.*s)", static_cast<int>(sym.size()), sym.data());
	dbg.io().write("\n");

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
			dbg.io().write("#%-2d $%08X", frame, val);
			auto name = SymbolsAtAddress(val);
			if (!name.empty())
				dbg.io().write(" (%.*s)", static_cast<int>(name.size()), name.data());
			dbg.io().write("  [SP+$%X]\n", offset);
			++frame;
		}
	}

	if (frame == 1) dbg.io().write("  (no return addresses found on stack)\n");
}

void CmdLog(Debugger &dbg, const std::vector<Token> &args)
{
	/* log file <path> — start logging to file */
	if (!args.empty() && args[0].isWord("file"))
	{
		if (args.size() < 2 || args[1].isEnd())
		{
			if (dbg.io().hasLogFile())
				dbg.io().write("logging to %s\n", dbg.io().logFilePath().c_str());
			else
				dbg.io().write("no log file active\n");
			return;
		}
		if (args[1].isWord("off"))
		{
			dbg.io().closeLogFile();
			dbg.io().write("log file closed\n");
			return;
		}
		std::string path(args[1].text);
		if (dbg.io().setLogFile(path.c_str()))
			dbg.io().write("logging to %s\n", path.c_str());
		else
			dbg.io().write("cannot open '%s' for writing\n", path.c_str());
		return;
	}

	const auto &lines = extnDbgConsoleLines();

	if (lines.empty())
	{
		dbg.io().write("(no guest log lines)\n");
		return;
	}

	/* log grep <pattern> */
	if (!args.empty() && args[0].isWord("grep") && args.size() >= 2 && !args[1].isEnd())
	{
		std::string_view pattern = args[1].text;
		int count = 0;
		for (size_t i = 0; i < lines.size(); ++i)
		{
			if (lines[i].find(pattern) != std::string::npos)
			{
				dbg.io().write("[%zu] %s\n", i, lines[i].c_str());
				++count;
			}
		}
		if (count == 0) dbg.io().write("(no matching lines)\n");
		return;
	}

	/* log [N] — show last N lines (default 20) */
	int count = 20;
	if (!args.empty() && args[0].isNumber()) count = static_cast<int>(args[0].numValue);

	size_t start = (lines.size() > static_cast<size_t>(count)) ? (lines.size() - count) : 0;
	for (size_t i = start; i < lines.size(); ++i)
		dbg.io().write("[%zu] %s\n", i, lines[i].c_str());
}
