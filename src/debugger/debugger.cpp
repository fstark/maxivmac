/*
	debugger.cpp — Debugger singleton, state machine, and command loop
*/

#include "debugger/debugger.h"
#include "debugger/dbg_io.h"
#include "debugger/cmd_parser.h"
#include "debugger/symbols.h"
#include "debugger/expr.h"
#include "debugger/script_suspend.h"

#include "core/machine.h"
#include "cpu/m68k.h"
#include "cpu/trap_counter.h"
#include "cpu/trap_tracer.h"
#include "cpu/disasm.h"

#include <algorithm>
#include <bitset>
#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <unordered_map>

/* ── Forward declarations for command handlers ──────── */

void CmdRun(Debugger &dbg, const std::vector<Token> &args);
void CmdContinue(Debugger &dbg, const std::vector<Token> &args);
void CmdStep(Debugger &dbg, const std::vector<Token> &args);
void CmdStepi(Debugger &dbg, const std::vector<Token> &args);
void CmdNext(Debugger &dbg, const std::vector<Token> &args);
void CmdFinish(Debugger &dbg, const std::vector<Token> &args);
void CmdUntil(Debugger &dbg, const std::vector<Token> &args);
void CmdBreak(Debugger &dbg, const std::vector<Token> &args);
void CmdTbreak(Debugger &dbg, const std::vector<Token> &args);
void CmdDelete(Debugger &dbg, const std::vector<Token> &args);
void CmdDisable(Debugger &dbg, const std::vector<Token> &args);
void CmdEnable(Debugger &dbg, const std::vector<Token> &args);
void CmdWatch(Debugger &dbg, const std::vector<Token> &args);
void CmdRwatch(Debugger &dbg, const std::vector<Token> &args);
void CmdAwatch(Debugger &dbg, const std::vector<Token> &args);
void CmdCommands(Debugger &dbg, const std::vector<Token> &args);
void CmdIgnore(Debugger &dbg, const std::vector<Token> &args);
void CmdExamine(Debugger &dbg, const std::vector<Token> &args);
void CmdPrint(Debugger &dbg, const std::vector<Token> &args);
void CmdSet(Debugger &dbg, const std::vector<Token> &args);
void CmdFind(Debugger &dbg, const std::vector<Token> &args);
void CmdDisas(Debugger &dbg, const std::vector<Token> &args);
void CmdTrace(Debugger &dbg, const std::vector<Token> &args);
void CmdInfo(Debugger &dbg, const std::vector<Token> &args);
void CmdLog(Debugger &dbg, const std::vector<Token> &args);
void CmdDiag(Debugger &dbg, const std::vector<Token> &args);
void CmdBacktrace(Debugger &dbg, const std::vector<Token> &args);
void CmdHelp(Debugger &dbg, const std::vector<Token> &args);
void CmdQuit(Debugger &dbg, const std::vector<Token> &args);
void CmdSource(Debugger &dbg, const std::vector<Token> &args);
void CmdDrive(Debugger &dbg, const std::vector<Token> &args);
void CmdShowtext(Debugger &dbg, const std::vector<Token> &args);
void CmdScreenshot(Debugger &dbg, const std::vector<Token> &args);
void CmdTimeout(Debugger &dbg, const std::vector<Token> &args);
void CmdWait(Debugger &dbg, const std::vector<Token> &args);
void CmdFail(Debugger &dbg, const std::vector<Token> &args);

/* ── Command table ──────────────────────────────────── */

static CmdEntry s_commands[] = {
	{"run", "r", CmdRun, "Start/resume execution",
	 "run\n  Start execution from current PC.  Same as 'continue' if already started.\n"},
	{"continue", "c", CmdContinue, "Continue execution",
	 "continue\n  Resume execution until next breakpoint or Ctrl-C.\n"},
	{"step", "s", CmdStep, "Step N source-level instructions",
	 "step [N]\n  Execute N instructions (default 1), stepping into calls.\n"},
	{"stepi", "si", CmdStepi, "Step one machine instruction",
	 "stepi [N]\n  Execute N machine instructions (default 1).\n"},
	{"next", "n", CmdNext, "Step over calls",
	 "next [N]\n  Execute N instructions, stepping over JSR/BSR/A-line traps.\n"},
	{"finish", "fin", CmdFinish, "Run until current function/trap returns",
	 "finish\n  Run until the current frame returns (SP >= saved SP and RTS/RTE/RTD).\n"
	 "  Works with A-line trap calls: stops when the trap handler returns to caller.\n"},
	{"until", "u", CmdUntil, "Run until address", "until <addr>\n  Run until PC reaches <addr>.\n"},
	{"break", "b", CmdBreak, "Set breakpoint",
	 "break <location> [if <cond>]\n  Set a breakpoint at address, trap name, or global name.\n"
	 "  Optional 'if <cond>' adds a condition evaluated on hit.\n"},
	{"tbreak", "tb", CmdTbreak, "Set temporary breakpoint",
	 "tbreak <location> [if <cond>]\n  Set a one-shot breakpoint (deleted after first hit).\n"},
	{"delete", "d", CmdDelete, "Delete breakpoint/watchpoint",
	 "delete [id]\n  Delete breakpoint/watchpoint by ID.  No args = delete all.\n"},
	{"disable", "", CmdDisable, "Disable breakpoint/watchpoint",
	 "disable <id>\n  Disable breakpoint or watchpoint without removing it.\n"},
	{"enable", "", CmdEnable, "Enable breakpoint/watchpoint",
	 "enable <id>\n  Re-enable a disabled breakpoint or watchpoint.\n"},
	{"watch", "", CmdWatch, "Set write watchpoint",
	 "watch <addr> [len] [if val <op> <value>]\n  Break on write to address range.\n"},
	{"rwatch", "", CmdRwatch, "Set read watchpoint",
	 "rwatch <addr> [len] [if val <op> <value>]\n  Break on read from address range.\n"},
	{"awatch", "", CmdAwatch, "Set access watchpoint",
	 "awatch <addr> [len] [if val <op> <value>]\n  Break on read or write to address range.\n"},
	{"commands", "", CmdCommands, "Set auto-execute commands on breakpoint",
	 "commands <id>\n  Enter commands to execute when breakpoint <id> fires.\n"
	 "  Type 'end' to finish.  Use 'continue' to auto-resume.\n"},
	{"ignore", "", CmdIgnore, "Skip next N breakpoint hits",
	 "ignore <id> <count>\n  Skip the next <count> hits of breakpoint <id>.\n"},
	{"x", "", CmdExamine, "Examine memory",
	 "x[/FMT] <addr>\n  FMT = [count][size][format]\n"
	 "  size: b(byte) w(word) l(long)  format: x(hex) d(dec) s(string) i(insn)\n"},
	{"print", "p", CmdPrint, "Evaluate and print expression",
	 "print <expr>\n  Evaluate expression and print result as hex.\n"},
	{"set", "", CmdSet, "Set register or memory",
	 "set <target> = <value>\n  target: register name, or *<addr>[.w/.l] for memory.\n"},
	{"find", "", CmdFind, "Search memory",
	 "find <start> <end> <pattern>\n  Search memory for hex bytes, wildcards, or \"string\".\n"},
	{"disas", "", CmdDisas, "Disassemble address range",
	 "disas <start> [<end> | +<len>]\n  Disassemble instructions in range.\n"
	 "  Default range: 64 bytes from start.\n"},
	{"trace", "", CmdTrace, "Control tracing",
	 "trace <traps|insn|io> <on|off|names...>\n  Enable/disable trace output.\n"},
	{"diag", "", CmdDiag, "Control diagnostic traces",
	 "diag [<subsystem> <on|off>]\n  Toggle subsystem traces. 'diag' lists all.\n"
	 "  diag all on/off — toggle everything.\n"},
	{"info", "i", CmdInfo, "Show info about debugger state",
	 "info <break|reg|traps|globals|symbol|insn|via|scrap|console>\n  Show various debugger "
	 "information.\n"},
	{"log", "", CmdLog, "Show guest log / log to file",
	 "log [N]\n  Show last N guest log lines (default 20).\n"
	 "log grep <pattern>\n  Show guest log lines matching pattern.\n"
	 "log file <path>\n  Mirror all debugger output to <path> (append mode).\n"
	 "log file off\n  Stop logging to file.\n"
	 "log file\n  Show current log file status.\n"},
	{"backtrace", "bt", CmdBacktrace, "Heuristic stack backtrace",
	 "backtrace\n  Scan stack for return addresses (heuristic, best-effort).\n"},
	{"help", "h", CmdHelp, "Show help",
	 "help [command]\n  Show command list or detailed help for a command.\n"},
	{"source", "so", CmdSource, "Execute commands from a file",
	 "source <path>\n  Read and execute debugger commands from a file.\n"},
	{"quit", "q", CmdQuit, "Quit emulator", "quit\n  Exit the emulator immediately.\n"},
	{"drive", "", CmdDrive, "Mount/unmount shared drives",
	 "drive mount <path>\n  Mount a host directory as a new shared drive.\n"
	 "drive unmount <slot>\n  Unmount and release shared drive by slot number.\n"
	 "drive list\n  Show all mounted shared drives.\n"},
	{"showtext", "", CmdShowtext, "Toggle text capture display",
	 "showtext [on|off]\n  Show/hide text captured from guest DrawString/ParamText calls.\n"},
	{"screenshot", "", CmdScreenshot, "Save framebuffer to PNG",
	 "screenshot <filename>\n  Save the current framebuffer to a PNG file.\n"},
	{"timeout", "", CmdTimeout, "Set default wait budget",
	 "timeout <cycles>\n  Set the default cycle budget for 'wait' commands.\n"},
	{"wait", "", CmdWait, "Wait for condition",
	 "wait text \"pattern\" [cycles]\n  Resume until text appears (script suspends).\n"
	 "wait screen \"ref.png\" [cycles] [pct]\n  Resume until screen matches reference.\n"
	 "wait off [cycles]\n  Resume until guest power-off.\n"
	 "wait <addr|symbol> [cycles]\n  Resume until address is reached.\n"
	 "wait trap <name> [cycles]\n  Resume until trap fires.\n"},
	{"fail", "", CmdFail, "Abort with error",
	 "fail [\"message\"]\n  Print error, save screenshot, exit with code 1.\n"},
};

static constexpr int kNumCommands = sizeof(s_commands) / sizeof(s_commands[0]);

/* ── SIGINT handling ────────────────────────────────── */

static volatile sig_atomic_t s_interrupted = 0;

static void SignalHandler(int)
{
	s_interrupted = 1;
}

/* ── Debugger::Impl ─────────────────────────────────── */

enum class DbgState
{
	Stopped,
	Running,
	Stepping
};

struct Debugger::Impl
{
	DbgState state = DbgState::Stopped;
	uint32_t stepsRemaining = 0;

	/* Next/finish/until state */
	bool finishing = false;
	uint32_t finishSP = 0;
	bool nexting = false;
	uint32_t nextSP = 0;
	uint32_t nextRemaining = 0; /* steps remaining after a next-over-call */
	uint32_t untilAddr = 0;

	/* Breakpoints */
	uint32_t nextBpId = 1;
	std::vector<Debugger::Breakpoint> breakpoints;
	std::vector<Debugger::Watchpoint> watchpoints;

	/* Instruction-count breakpoint (0 = disabled) */
	uint32_t insnBreakId = 0;
	InstructionCount insnBreakCount = 0;

	/* Trace flags */
	bool trTraps = false;
	bool trInsn = false;
	bool trIO = false;
	std::bitset<65536> trapFilter;

	/* Last command (for empty-line repeat) */
	std::string lastLine;

	/* Pending script (scripting suspension/resumption) */
	PendingScript pendingScript;

	/* I/O transport */
	std::unique_ptr<DbgIO> io;
	bool clientConnected = false;
};

/* ── Singleton ──────────────────────────────────────── */

bool g_debuggerActive = false;
bool g_watchpointActive = false;
static Debugger *s_instance = nullptr;

void dbg_printf(const char *fmt, ...)
{
	std::va_list ap;
	va_start(ap, fmt);
	if (s_instance)
		s_instance->io().vwrite(fmt, ap);
	else
		std::vprintf(fmt, ap);
	va_end(ap);
}

Debugger::Debugger()
{
	impl_ = new Impl();
}

Debugger *Debugger::instance()
{
	return s_instance;
}

void Debugger::create()
{
	create(nullptr);
}

void Debugger::create(DbgIO *io)
{
	if (s_instance) return;
	s_instance = new Debugger();

	if (io)
		s_instance->impl_->io.reset(io);
	else
		s_instance->impl_->io = CreateStdioIO();

	SymbolsInit();

	std::signal(SIGINT, SignalHandler);

	auto &out = *s_instance->impl_->io;
	out.write("maxivmac debugger — type 'help' for commands\n");
}

/* ── I/O accessor ───────────────────────────────────── */

DbgIO &Debugger::io()
{
	return *impl_->io;
}

/* ── State queries ──────────────────────────────────── */

bool Debugger::isRunning() const
{
	return impl_->state == DbgState::Running;
}

bool Debugger::isStopped() const
{
	return impl_->state == DbgState::Stopped;
}

void Debugger::stop(std::string_view reason)
{
	impl_->state = DbgState::Stopped;
	impl_->finishing = false;
	impl_->nexting = false;
	impl_->nextRemaining = 0;
	impl_->untilAddr = 0;
	if (!reason.empty())
		impl_->io->write("[%.*s]\n", static_cast<int>(reason.size()), reason.data());
}

void Debugger::setRunning()
{
	impl_->state = DbgState::Running;
}

void Debugger::setStepping(uint32_t n)
{
	impl_->stepsRemaining = n;
	impl_->state = DbgState::Stepping;
}

void Debugger::setFinishing(uint32_t savedSP)
{
	impl_->finishing = true;
	impl_->finishSP = savedSP;
	impl_->state = DbgState::Running;
}

void Debugger::setNexting(uint32_t savedSP, uint32_t count)
{
	impl_->nexting = true;
	impl_->nextSP = savedSP;
	impl_->nextRemaining = count;
	impl_->state = DbgState::Running;
}

void Debugger::setUntil(uint32_t addr)
{
	impl_->untilAddr = addr;
	impl_->state = DbgState::Running;
}

uint32_t Debugger::setInsnBreak(InstructionCount insnNumber)
{
	impl_->insnBreakId = impl_->nextBpId++;
	impl_->insnBreakCount = insnNumber;
	return impl_->insnBreakId;
}

uint32_t Debugger::insnBreakId() const
{
	return impl_->insnBreakId;
}
InstructionCount Debugger::insnBreakCount() const
{
	return impl_->insnBreakCount;
}

/* ── Breakpoint/watchpoint management ───────────────── */

static void RecalcWatchpointFlag(const std::vector<Debugger::Watchpoint> &watchpoints)
{
	bool any = false;
	for (auto &wp : watchpoints)
		if (wp.enabled)
		{
			any = true;
			break;
		}
	g_watchpointActive = any;
	if (any) m68k_InvalidateMATC();
}

uint32_t Debugger::addBreakpoint(uint32_t addr, uint16_t trapWord, const std::string &condition)
{
	return addBreakpoint(addr, trapWord, 0, condition);
}

uint32_t Debugger::addBreakpoint(uint32_t addr, uint16_t trapWord, uint16_t subtrapSelector,
								 const std::string &condition)
{
	Breakpoint bp;
	bp.id = impl_->nextBpId++;
	bp.enabled = true;
	bp.address = addr;
	bp.trapWord = trapWord;
	bp.subtrapSelector = subtrapSelector;
	bp.condition = condition;
	bp.kind = (addr != 0) ? Breakpoint::Kind::Address : Breakpoint::Kind::Trap;
	impl_->breakpoints.push_back(std::move(bp));
	return impl_->breakpoints.back().id;
}

uint32_t Debugger::addBreakpoint(Breakpoint bp)
{
	bp.id = impl_->nextBpId++;
	impl_->breakpoints.push_back(std::move(bp));
	return impl_->breakpoints.back().id;
}

uint32_t Debugger::addWatchpoint(uint32_t addr, uint32_t len, char mode, bool hasValCond,
								 uint8_t valCondOp, uint32_t valCondValue)
{
	Watchpoint wp;
	wp.id = impl_->nextBpId++;
	wp.enabled = true;
	wp.address = addr;
	wp.length = len;
	wp.mode = mode;
	wp.hasValCond = hasValCond;
	wp.valCondOp = valCondOp;
	wp.valCondValue = valCondValue;
	impl_->watchpoints.push_back(std::move(wp));
	RecalcWatchpointFlag(impl_->watchpoints);
	return impl_->watchpoints.back().id;
}

bool Debugger::deleteById(uint32_t id)
{
	if (impl_->insnBreakId == id && impl_->insnBreakCount != 0)
	{
		impl_->insnBreakId = 0;
		impl_->insnBreakCount = 0;
		return true;
	}
	for (auto it = impl_->breakpoints.begin(); it != impl_->breakpoints.end(); ++it)
	{
		if (it->id == id)
		{
			impl_->breakpoints.erase(it);
			return true;
		}
	}
	for (auto it = impl_->watchpoints.begin(); it != impl_->watchpoints.end(); ++it)
	{
		if (it->id == id)
		{
			impl_->watchpoints.erase(it);
			RecalcWatchpointFlag(impl_->watchpoints);
			return true;
		}
	}
	return false;
}

bool Debugger::enableById(uint32_t id, bool enable)
{
	for (auto &bp : impl_->breakpoints)
	{
		if (bp.id == id)
		{
			bp.enabled = enable;
			return true;
		}
	}
	for (auto &wp : impl_->watchpoints)
	{
		if (wp.id == id)
		{
			wp.enabled = enable;
			RecalcWatchpointFlag(impl_->watchpoints);
			return true;
		}
	}
	return false;
}

const Debugger::Breakpoint *Debugger::lookupByAddr(uint32_t addr) const
{
	for (auto &bp : impl_->breakpoints)
	{
		if (bp.enabled && bp.address == addr && bp.address != 0) return &bp;
	}
	return nullptr;
}

const Debugger::Breakpoint *Debugger::lookupByTrap(uint16_t trapWord) const
{
	for (auto &bp : impl_->breakpoints)
	{
		if (bp.enabled && bp.trapWord == trapWord && bp.trapWord != 0 && bp.subtrapSelector == 0)
			return &bp;
	}
	return nullptr;
}

const Debugger::Breakpoint *Debugger::lookupBySubtrap(uint16_t trapWord, uint16_t selector) const
{
	for (auto &bp : impl_->breakpoints)
	{
		if (bp.enabled && bp.trapWord == trapWord && bp.subtrapSelector == selector &&
			bp.subtrapSelector != 0)
			return &bp;
	}
	return nullptr;
}

const std::vector<Debugger::Breakpoint> &Debugger::breakpoints() const
{
	return impl_->breakpoints;
}

std::vector<Debugger::Breakpoint> &Debugger::breakpoints()
{
	return impl_->breakpoints;
}

const std::vector<Debugger::Watchpoint> &Debugger::watchpoints() const
{
	return impl_->watchpoints;
}

/* ── Trace flags ────────────────────────────────────── */

bool Debugger::traceTraps() const
{
	return impl_->trTraps;
}
bool Debugger::traceInsn() const
{
	return impl_->trInsn;
}
bool Debugger::traceIO() const
{
	return impl_->trIO;
}

void Debugger::setTraceTraps(bool on)
{
	impl_->trTraps = on;
	g_tracer.setIO(on ? impl_->io.get() : nullptr);
	g_tracer.enable(on);
}

void Debugger::setTraceInsn(bool on)
{
	impl_->trInsn = on;
}
void Debugger::setTraceIO(bool on)
{
	impl_->trIO = on;
}

bool Debugger::trapInFilter(uint16_t tw) const
{
	return impl_->trapFilter.test(tw);
}

void Debugger::addTrap(uint16_t tw)
{
	impl_->trapFilter.set(tw);
	g_tracer.addTrap(tw);
}

void Debugger::removeTrap(uint16_t tw)
{
	impl_->trapFilter.reset(tw);
	g_tracer.removeTrap(tw);
}

void Debugger::addAllTraps()
{
	impl_->trapFilter.set();
	g_tracer.addAllTraps();
}

void Debugger::removeAllTraps()
{
	impl_->trapFilter.reset();
	g_tracer.removeAllTraps();
}

void Debugger::addSubtrap(uint16_t trapWord, uint16_t selector)
{
	impl_->trapFilter.set(trapWord);
	g_tracer.addSubtrap(trapWord, selector);
}

void Debugger::removeSubtrap(uint16_t trapWord, uint16_t selector)
{
	g_tracer.removeSubtrap(trapWord, selector);
}

std::vector<uint16_t> Debugger::trapsEnabled() const
{
	std::vector<uint16_t> result;
	for (int i = 0, n = trap_dict_size(); i < n; ++i)
	{
		uint16_t tw = trap_dict_entry(i).trapWord;
		if (impl_->trapFilter.test(tw)) result.push_back(tw);
	}
	return result;
}

std::vector<uint16_t> Debugger::trapsDisabled() const
{
	std::vector<uint16_t> result;
	for (int i = 0, n = trap_dict_size(); i < n; ++i)
	{
		uint16_t tw = trap_dict_entry(i).trapWord;
		if (!impl_->trapFilter.test(tw)) result.push_back(tw);
	}
	return result;
}

/* ── Command table access ───────────────────────────── */

CmdEntry *Debugger::commandTable()
{
	return s_commands;
}

int Debugger::commandTableSize() const
{
	return kNumCommands;
}

/* ── Command execution for breakpoint auto-commands ─── */

void Debugger::executeCommands(const std::vector<std::string> &cmds)
{
	for (size_t i = 0; i < cmds.size(); ++i)
	{
		auto &line = cmds[i];
		if (auto p = line.find_first_not_of(" \t"); p != std::string::npos && line[p] == '#')
			continue;

		auto tokens = Tokenize(line);
		if (tokens.empty() || tokens[0].isEnd()) continue;

		auto *entry = DispatchCommand(tokens[0].text, s_commands, kNumCommands, impl_->io.get());
		if (entry)
		{
			std::vector<Token> args(tokens.begin() + 1, tokens.end());
			entry->handler(*this, args);
			if (impl_->state != DbgState::Stopped)
			{
				// Script suspended — save remaining lines for resumption
				impl_->pendingScript.lines = cmds;
				impl_->pendingScript.nextLine = static_cast<int>(i + 1);
				return;
			}
		}
	}
	// All lines executed — clear any pending state
	impl_->pendingScript.clear();
}

bool Debugger::tryResumeScript(const Breakpoint *firedBp)
{
	if (impl_->pendingScript.exhausted()) return false;

	if (firedBp && firedBp->scriptOwned)
	{
		// Condition met — resume the pending script from where it left off
		auto &ps = impl_->pendingScript;
		std::vector<std::string> remaining(ps.lines.begin() + ps.nextLine, ps.lines.end());
		ps.clear();
		executeCommands(remaining);
		return (impl_->state != DbgState::Stopped);
	}
	// Non-scriptOwned breakpoint → drop to interactive prompt
	// (pending script remains for potential future resumption)
	return false;
}

/* ── Source file ─────────────────────────────────────── */

bool SourceFile(Debugger &dbg, std::string_view path)
{
	std::string p(path);
	std::FILE *f = std::fopen(p.c_str(), "r");
	if (!f)
	{
		dbg.io().write("source: cannot open '%s'\n", p.c_str());
		return false;
	}

	std::vector<std::string> cmds;
	char buf[1024];
	while (std::fgets(buf, sizeof(buf), f))
	{
		size_t len = std::strlen(buf);
		if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
		cmds.emplace_back(buf);
	}
	std::fclose(f);

	dbg.executeCommands(cmds);
	return true;
}

/* ── Build ExprContext from live CPU state ───────────── */

static ExprContext BuildExprContext()
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

/* ── Command loop ───────────────────────────────────── */

void Debugger::commandLoop()
{
	char buf[1024];
	auto &out = *impl_->io;

	/* For socket mode: ensure a client is connected */
	if (out.isSocket() && !impl_->clientConnected)
	{
		if (!out.acceptClient()) return;
		impl_->clientConnected = true;
	}

	while (impl_->state == DbgState::Stopped)
	{
		/* Show PC and current instruction */
		{
			uint32_t pc = m68k_getPC_public();
			uint16_t opcode = get_vm_word(pc);
			uint32_t disasmPC = pc;
			auto disasm = Disassemble(disasmPC);
			out.write("$%08X: %04X  %s\n", pc, opcode, disasm.c_str());
		}

		out.write("(dbg) ");
		out.endResponse();
		out.flush();

		if (!out.readLine(buf, sizeof(buf)))
		{
			if (out.isSocket())
			{
				/* Client disconnected — wait for new client */
				out.closeClient();
				impl_->clientConnected = false;
				if (!out.acceptClient()) return;
				impl_->clientConnected = true;
				continue;
			}
			out.write("\n");
			std::exit(0);
		}

		/* Strip trailing newline */
		size_t len = std::strlen(buf);
		if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';

		std::string_view line(buf);

		/* Empty line repeats last command */
		if (line.empty())
		{
			line = impl_->lastLine;
			if (line.empty()) continue;
		}
		else
		{
			impl_->lastLine = std::string(line);
		}

		auto tokens = Tokenize(line);
		if (tokens.empty() || tokens[0].isEnd()) continue;

		auto *entry = DispatchCommand(tokens[0].text, s_commands, kNumCommands, impl_->io.get());
		if (entry)
		{
			std::vector<Token> args(tokens.begin() + 1, tokens.end());
			entry->handler(*this, args);
		}
	}
}

/* ── CPU hooks ──────────────────────────────────────── */

bool Debugger::instructionHook(uint32_t pc)
{
	/* SIGINT check */
	if (s_interrupted)
	{
		s_interrupted = 0;
		stop("interrupted");
		commandLoop();
		return true;
	}

	/* Already stopped → enter command loop */
	if (impl_->state == DbgState::Stopped)
	{
		commandLoop();
		return true;
	}

	/* Stepping mode */
	if (impl_->state == DbgState::Stepping)
	{
		if (--impl_->stepsRemaining == 0)
		{
			stop("step completed");
			commandLoop();
			return true;
		}
		/* Show each intermediate instruction */
		{
			uint16_t opcode = get_vm_word(pc);
			uint32_t disasmPC = pc;
			auto disasm = Disassemble(disasmPC);
			impl_->io->write("$%08X: %04X  %s\n", pc, opcode, disasm.c_str());
		}
	}

	/* Until check */
	if (impl_->untilAddr != 0 && pc == impl_->untilAddr)
	{
		stop("until reached");
		commandLoop();
		return true;
	}

	/* Finish check: SP >= saved SP and RTS/RTD/RTE */
	if (impl_->finishing)
	{
		uint32_t d[8], a[8];
		m68k_getRegs(d, a);
		uint32_t sp = a[7];
		if (sp >= impl_->finishSP)
		{
			uint16_t opcode = get_vm_word(pc);
			if (opcode == 0x4E75 || opcode == 0x4E74 || opcode == 0x4E73)
			{
				stop("finish completed");
				commandLoop();
				return true;
			}
		}
	}

	/* Next check: SP >= saved SP */
	if (impl_->nexting)
	{
		uint32_t d[8], a[8];
		m68k_getRegs(d, a);
		uint32_t sp = a[7];
		if (sp >= impl_->nextSP)
		{
			impl_->nexting = false;
			/* If there are remaining steps from a 'next N', keep stepping */
			if (impl_->nextRemaining > 1)
			{
				uint16_t opcode = get_vm_word(pc);
				uint32_t disasmPC = pc;
				auto disasm = Disassemble(disasmPC);
				impl_->io->write("$%08X: %04X  %s\n", pc, opcode, disasm.c_str());
				impl_->nextRemaining--;
				/* Check if this instruction is also a call */
				bool isCall = false;
				if ((opcode & 0xFF00) == 0x6100) isCall = true;
				if ((opcode & 0xFFC0) == 0x4E80) isCall = true;
				if ((opcode & 0xF000) == 0xA000) isCall = true;
				if (isCall)
				{
					impl_->nexting = true;
					impl_->nextSP = sp;
				}
				else
				{
					setStepping(impl_->nextRemaining);
					impl_->nextRemaining = 0;
				}
				return true;
			}
			stop("next completed");
			commandLoop();
			return true;
		}
	}

	/* Instruction-count breakpoint */
	if (impl_->insnBreakCount != 0 && g_instructionCount >= impl_->insnBreakCount)
	{
		uint32_t id = impl_->insnBreakId;
		InstructionCount target = impl_->insnBreakCount;
		impl_->insnBreakCount = 0; /* one-shot */
		impl_->io->write("Breakpoint %u at instruction #%" PRIu64 "\n", id, target);
		stop("");
		commandLoop();
		return true;
	}

	/* Breakpoint check */
	auto *bp = lookupByAddr(pc);
	if (bp)
	{
		/* Evaluate condition if present */
		if (!bp->condition.empty())
		{
			auto ctx = BuildExprContext();
			std::string err;
			if (!ExprCheck(bp->condition, ctx, err))
			{
				return false; /* condition not met */
			}
		}

		/* Ignore count — decrement and skip if still positive */
		auto *mutableBp = const_cast<Breakpoint *>(bp);
		if (mutableBp->ignoreCount > 0)
		{
			--mutableBp->ignoreCount;
			return false;
		}

		impl_->io->write("Breakpoint %u at $%08X\n", bp->id, pc);

		/* Auto-execute commands */
		if (!bp->commands.empty())
		{
			executeCommands(bp->commands);
			if (impl_->state != DbgState::Stopped) return true;
		}

		/* Temporary breakpoints are one-shot — delete after firing */
		bool wasTemporary = bp->temporary;
		bool wasScriptOwned = bp->scriptOwned;
		uint32_t bpId = bp->id;
		if (wasTemporary) deleteById(bpId);

		stop("");

		// If a scriptOwned BP fired, try resuming the pending script
		if (wasScriptOwned && tryResumeScript(nullptr)) return true;

		commandLoop();
		return true;
	}

	/* Instruction tracing */
	if (impl_->trInsn)
	{
		uint16_t opcode = get_vm_word(pc);
		uint32_t disasmPC = pc;
		auto disasm = Disassemble(disasmPC);
		uint32_t d[8], a[8];
		m68k_getRegs(d, a);
		impl_->io->write("%" PRIu64 " %08X: %04X  %-30s D=%08X %08X %08X %08X %08X %08X %08X %08X "
						 "A=%08X %08X %08X %08X %08X %08X %08X %08X\n",
						 g_instructionCount, pc, opcode, disasm.c_str(), d[0], d[1], d[2], d[3],
						 d[4], d[5], d[6], d[7], a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
	}

	return false;
}

bool Debugger::trapHook(uint16_t trapWord)
{
	/* Trap breakpoint check */
	auto *bp = lookupByTrap(trapWord);

	/* Check for subtrap breakpoints on dispatch traps */
	if (!bp && g_trapDefs.isDispatch(trapWord))
	{
		auto *info = g_trapDefs.dispatchInfo(trapWord);
		if (info)
		{
			/* Read selector from current CPU state */
			int idx = static_cast<int>(info->selectorParam.loc) - 1;
			uint32_t dregs[8], aregs[8];
			m68k_getRegs(dregs, aregs);
			uint16_t selector = 0;
			if (idx < 8)
				selector = static_cast<uint16_t>(dregs[idx]);
			else
				selector = static_cast<uint16_t>(aregs[idx - 8]);
			bp = lookupBySubtrap(trapWord, selector);
		}
	}

	if (bp)
	{
		/* Evaluate condition if present */
		if (!bp->condition.empty())
		{
			auto ctx = BuildExprContext();
			std::string err;
			if (!ExprCheck(bp->condition, ctx, err))
			{
				return false;
			}
		}

		/* Ignore count — decrement and skip if still positive */
		auto *mutableBp = const_cast<Breakpoint *>(bp);
		if (mutableBp->ignoreCount > 0)
		{
			--mutableBp->ignoreCount;
			return false;
		}

		if (bp->subtrapSelector != 0)
			impl_->io->write("Breakpoint %u on subtrap %s ($%04X sel $%02X)\n", bp->id,
							 SymbolsSubtrapName(bp->trapWord, bp->subtrapSelector), trapWord,
							 bp->subtrapSelector);
		else
			impl_->io->write("Breakpoint %u on trap %s ($%04X)\n", bp->id,
							 SymbolsTrapName(trapWord), trapWord);

		if (!bp->commands.empty())
		{
			executeCommands(bp->commands);
			if (impl_->state != DbgState::Stopped) return true;
		}

		/* Temporary breakpoints are one-shot — delete after firing */
		bool wasTemporary = bp->temporary;
		bool wasScriptOwned = bp->scriptOwned;
		uint32_t bpId = bp->id;
		if (wasTemporary) deleteById(bpId);

		stop("");

		// If a scriptOwned BP fired, try resuming the pending script
		if (wasScriptOwned && tryResumeScript(nullptr)) return true;

		return true; /* instructionHook will enter command loop */
	}

	return false;
}

bool Debugger::memoryHook(uint32_t addr, uint32_t size, char direction, uint32_t oldVal,
						  uint32_t newVal)
{
	for (auto &wp : impl_->watchpoints)
	{
		if (!wp.enabled) continue;

		/* Check direction match */
		if (wp.mode == 'W' && direction != 'W') continue;
		if (wp.mode == 'R' && direction != 'R') continue;
		/* 'A' matches both */

		/* Check range overlap */
		uint32_t wpEnd = wp.address + wp.length;
		uint32_t accEnd = addr + size;
		if (addr >= wpEnd || accEnd <= wp.address) continue;

		/* Check value condition */
		if (wp.hasValCond)
		{
			uint32_t val = (direction == 'W') ? newVal : oldVal;
			bool match = false;
			switch (wp.valCondOp)
			{
				case 0:
					match = (val == wp.valCondValue);
					break;
				case 1:
					match = (val != wp.valCondValue);
					break;
				case 2:
					match = (val < wp.valCondValue);
					break;
				case 3:
					match = (val > wp.valCondValue);
					break;
				case 4:
					match = (val <= wp.valCondValue);
					break;
				case 5:
					match = (val >= wp.valCondValue);
					break;
				case 6:
					match = (val & wp.valCondValue) != 0;
					break;
			}
			if (!match) continue;
		}

		const char *dirStr = (direction == 'W') ? "write" : "read";
		impl_->io->write("Watchpoint %u hit: %s at $%08X (old=$%08X new=$%08X)\n", wp.id, dirStr,
						 addr, oldVal, newVal);
		stop("");
		return true;
	}
	return false;
}
