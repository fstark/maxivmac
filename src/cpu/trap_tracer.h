/*
	trap_tracer.h — Hierarchical A-line trap tracer

	Tracks trap entry/exit with nesting, decodes parameters
	using external definitions from TrapDefs.
*/
#pragma once

#include "cpu/trap_defs.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct TrapFrame
{
	uint16_t trapWord;
	uint32_t callerPC;	 /* return address from stack */
	uint32_t entryCycle; /* g_instructionCount at entry */
	uint32_t sp;		 /* SP at entry (for toolbox param read) */
	uint16_t appId;		 /* CurApRefNum at entry time */
	int depth;			 /* nesting depth when pushed */
};

class TrapTracer
{
public:
	explicit TrapTracer(TrapDefs &defs);

	void enter(uint16_t trapWord);
	void checkReturn(uint32_t pc);
	bool active() const { return depth_ > 0; }

	void enable(bool on);
	bool enabled() const { return enabled_; }
	void setMaxDepth(int depth);

	void addFilter(uint16_t trapWord);
	void clearFilter();

public: /* public for testability */
	std::string formatParam(const ParamDef &p, uint32_t rawValue);
	std::string formatOSType(uint32_t raw);
	std::string formatStr255(uint32_t addr);
	std::string formatOSErr(int16_t err);

private:
	void flushStack(const char *reason);
	void emitEntry(const TrapFrame &frame, const TrapDef &def);
	void emitEntry(const TrapFrame &frame);
	void emitAutoPop(const TrapFrame &frame, const char *name);
	void emitExit(const TrapFrame &frame, const TrapDef &def);
	void emitExit(const TrapFrame &frame);
	uint32_t readParamRaw(const ParamDef &p, uint32_t sp, int &stackOffset);
	uint16_t readAppId() const;
	std::string readAppName() const;

	TrapDefs &defs_;
	std::array<TrapFrame, 64> stack_;
	int depth_ = 0;
	bool enabled_ = false;
	int maxDepth_ = 64;
	uint16_t lastAppId_ = 0;
	bool overflowWarned_ = false;
	std::vector<uint16_t> filter_;
};

extern TrapDefs g_trapDefs;
extern TrapTracer g_tracer;
