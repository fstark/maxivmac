/*
	debugger.h — Public debugger interface

	The only header included outside src/debugger/.
*/
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/* InstructionCount = uint64_t; canonical definition in core/machine.h */
using InstructionCount = uint64_t;

class DbgIO;

class Debugger
{
public:
	static Debugger *instance();
	static void create();
	static void create(DbgIO *io); // takes ownership

	// I/O accessor (used by cmd_*.cpp to write output)
	DbgIO &io();

	// CPU hooks
	bool instructionHook(uint32_t pc);
	bool trapHook(uint16_t trapWord);
	bool memoryHook(uint32_t addr, uint32_t size, char direction, uint32_t oldVal, uint32_t newVal);

	bool isRunning() const;
	bool isStopped() const;
	void stop(std::string_view reason);

	// State control (used by cmd_exec.cpp)
	void setRunning();
	void setStepping(uint32_t n);
	void setFinishing(uint32_t savedSP);
	void setNexting(uint32_t savedSP, uint32_t count = 1);
	void setUntil(uint32_t addr);

	// Instruction-count breakpoint
	uint32_t setInsnBreak(InstructionCount insnNumber);
	uint32_t insnBreakId() const;
	InstructionCount insnBreakCount() const;

	// Breakpoint/watchpoint management (used by cmd_break.cpp)
	struct Breakpoint
	{
		uint32_t id;
		bool enabled;
		uint32_t address;				   // PC address (0 for trap-only breakpoints)
		uint16_t trapWord;				   // non-zero for trap breakpoints
		uint16_t subtrapSelector = 0;	   // non-zero for subtrap breakpoints
		std::string condition;			   // raw condition text
		std::vector<std::string> commands; // auto-execute on hit
		uint32_t ignoreCount = 0;		   // remaining hits to skip
	};

	struct Watchpoint
	{
		uint32_t id;
		bool enabled;
		uint32_t address;
		uint32_t length;
		char mode; // 'W', 'R', 'A'
		bool hasValCond;
		uint8_t valCondOp; // 0=eq,1=ne,2=lt,3=gt,4=le,5=ge,6=and
		uint32_t valCondValue;
	};

	uint32_t addBreakpoint(uint32_t addr, uint16_t trapWord, const std::string &condition);
	uint32_t addBreakpoint(uint32_t addr, uint16_t trapWord, uint16_t subtrapSelector,
						   const std::string &condition);
	uint32_t addWatchpoint(uint32_t addr, uint32_t len, char mode, bool hasValCond,
						   uint8_t valCondOp, uint32_t valCondValue);
	bool deleteById(uint32_t id);
	bool enableById(uint32_t id, bool enable);
	const Breakpoint *lookupByAddr(uint32_t addr) const;
	const Breakpoint *lookupByTrap(uint16_t trapWord) const;
	const Breakpoint *lookupBySubtrap(uint16_t trapWord, uint16_t selector) const;
	const std::vector<Breakpoint> &breakpoints() const;
	std::vector<Breakpoint> &breakpoints();
	const std::vector<Watchpoint> &watchpoints() const;

	// Trace flags (used by cmd_trace.cpp)
	bool traceTraps() const;
	bool traceInsn() const;
	bool traceIO() const;
	void setTraceTraps(bool on);
	void setTraceInsn(bool on);
	void setTraceIO(bool on);
	bool trapInFilter(uint16_t tw) const;
	void addTrap(uint16_t tw);
	void removeTrap(uint16_t tw);
	void addAllTraps();
	void removeAllTraps();
	void addSubtrap(uint16_t trapWord, uint16_t selector);
	void removeSubtrap(uint16_t trapWord, uint16_t selector);
	std::vector<uint16_t> trapsEnabled() const;
	std::vector<uint16_t> trapsDisabled() const;

	// Breakpoint commands (used by cmd_break.cpp)
	void executeCommands(const std::vector<std::string> &cmds);

	// Command table access (used by cmd_help.cpp)
	struct CmdEntry *commandTable();
	int commandTableSize() const;

private:
	Debugger();
	void commandLoop();
	struct Impl;
	Impl *impl_;
};

extern bool g_debuggerActive;
extern bool g_watchpointActive;

/* Printf that routes to the debugger log file when active,
   otherwise falls through to stdout. */
void dbg_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// Load and execute every line of a .dbg script file.
bool SourceFile(Debugger &dbg, std::string_view path);
