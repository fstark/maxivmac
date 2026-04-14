/*
	trap_tracer.cpp — Hierarchical A-line trap tracer implementation
*/

#include "cpu/trap_tracer.h"
#include "cpu/trap_counter.h"
#include "core/machine.h"
#include "cpu/m68k.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>

/* ── globals ─────────────────────────────────────────── */

TrapDefs g_trapDefs;
TrapTracer g_tracer(g_trapDefs);

/* ── param size helper ───────────────────────────────── */

static int paramSize(ParamType t)
{
	switch (t)
	{
		case ParamType::Byte:
			return 2; /* pushed as word on 68K stack */
		case ParamType::Word:
			return 2;
		case ParamType::Long:
			return 4;
		case ParamType::Ptr:
			return 4;
		case ParamType::Handle:
			return 4;
		case ParamType::OSType:
			return 4;
		case ParamType::Str255:
			return 4; /* pointer to string */
		case ParamType::OSErr:
			return 2;
		case ParamType::Boolean:
			return 2; /* pushed as word on 68K stack */
		case ParamType::Rect:
			return 8;
		case ParamType::Point:
			return 4;
	}
	return 2;
}

/* ── TrapTracer ──────────────────────────────────────── */

TrapTracer::TrapTracer(TrapDefs &defs) : defs_(defs) {}

void TrapTracer::enable(bool on)
{
	enabled_ = on;
	if (!on)
	{
		depth_ = 0;
		overflowWarned_ = false;
	}
}

void TrapTracer::setMaxDepth(int depth)
{
	if (depth > 0 && depth <= 64) maxDepth_ = depth;
}

void TrapTracer::addFilter(uint16_t trapWord)
{
	if (std::find(filter_.begin(), filter_.end(), trapWord) == filter_.end())
		filter_.push_back(trapWord);
}

void TrapTracer::clearFilter()
{
	filter_.clear();
}

/* ── enter ───────────────────────────────────────────── */

void TrapTracer::enter(uint16_t trapWord)
{
	if (!enabled_) return;

	if (!filter_.empty())
	{
		if (std::find(filter_.begin(), filter_.end(), trapWord) == filter_.end()) return;
	}

	/* Context-switch detection */
	uint16_t appId = readAppId();
	if (depth_ > 0 && appId != lastAppId_)
	{
		flushStack("context switch");
		std::string appName = readAppName();
		fprintf(stdout, "── context switch: appId=%u \"%s\" ──\n", (unsigned)appId,
				appName.c_str());
	}
	lastAppId_ = appId;

	/* Overflow guard */
	if (depth_ >= maxDepth_)
	{
		if (!overflowWarned_)
		{
			fprintf(stdout, "[TRACE] nesting overflow at depth %d, suppressing further nesting\n",
					maxDepth_);
			overflowWarned_ = true;
		}
		return;
	}

	/* Build frame */
	TrapFrame frame{};
	frame.trapWord = trapWord;

	/* After BackupPC() in DoCodeA(), PC points at the A-line word.
	   The return address is PC+2 (the instruction after the trap). */
	frame.callerPC = m68k_getPC_public() + 2;

	uint32_t sp = 0;
	{
		uint32_t dregs[8], aregs[8];
		m68k_getRegs(dregs, aregs);
		sp = aregs[7];
	}
	frame.entryCycle = g_instructionCount;
	frame.sp = sp;
	frame.appId = appId;
	frame.depth = depth_;

	/* Look up definition (masking handles auto-pop bit automatically) */
	const TrapDef *def = defs_.find(trapWord);

	/* Auto-pop Toolbox trap: bit 10 set on a Toolbox trap ($Axxx where bit 11 set).
	   The ROM dispatcher discards the return address, so the ROM routine
	   returns directly to the caller's caller.  Don't push a frame. */
	bool autoPop = (trapWord & 0x0800) && (trapWord & 0x0400);

	/* Handle noreturn traps */
	if (def && def->noreturn)
	{
		emitEntry(frame, *def);
		flushStack(def->name.c_str());
		return;
	}

	if (autoPop)
	{
		const char *name = def ? def->name.c_str() : nullptr;
		emitAutoPop(frame, name);
		return;
	}

	/* Emit entry and push frame */
	if (def)
		emitEntry(frame, *def);
	else
		emitEntry(frame);
	stack_[depth_++] = frame;
}

/* ── checkReturn ─────────────────────────────────────── */

void TrapTracer::checkReturn(uint32_t pc)
{
	if (depth_ <= 0) return;

	/* Scan stack for a frame whose callerPC matches */
	int match = -1;
	for (int i = depth_ - 1; i >= 0; --i)
	{
		if (pc == stack_[i].callerPC)
		{
			match = i;
			break;
		}
	}
	if (match < 0) return;

	/* Flush orphaned frames above the match */
	if (match < depth_ - 1)
	{
		for (int j = depth_ - 1; j > match; --j)
		{
			const TrapFrame &orphan = stack_[j];
			const char *name = nullptr;
			const TrapDef *d = defs_.find(orphan.trapWord);
			if (d) name = d->name.c_str();
			if (!name) name = trap_dict_name(orphan.trapWord);
			char nameBuf[16];
			if (!name)
			{
				snprintf(nameBuf, sizeof(nameBuf), "$%04X", orphan.trapWord);
				name = nameBuf;
			}
			fprintf(stdout, "%*s⊘ %s [missed return]\n", orphan.depth * 2, "", name);
		}
	}

	/* Pop the matched frame */
	depth_ = match;
	const TrapFrame &frame = stack_[match];
	const TrapDef *def = defs_.find(frame.trapWord);
	if (def)
		emitExit(frame, *def);
	else
		emitExit(frame);
	overflowWarned_ = false;
}

/* ── emit helpers ────────────────────────────────────── */

static std::string unknownTrapName(uint16_t trapWord)
{
	const char *dictName = trap_dict_name(trapWord);
	if (dictName) return std::string(dictName) + "?";
	char buf[16];
	snprintf(buf, sizeof(buf), "$%04X?", trapWord);
	return buf;
}

void TrapTracer::emitEntry(const TrapFrame &frame, const TrapDef &def)
{
	int indent = frame.depth * 2;

	/* Parameters — Pascal convention: first declared param is deepest on stack.
	   Compute total stack size, then read each param from the bottom up. */
	std::string params;
	int totalStack = 0;
	for (const auto &pd : def.paramsIn)
		if (pd.loc == ParamLoc::Stack) totalStack += paramSize(pd.type);
	int stackOff = totalStack;
	for (size_t i = 0; i < def.paramsIn.size(); ++i)
	{
		if (i > 0) params += ", ";
		const ParamDef &pd = def.paramsIn[i];
		if (pd.loc == ParamLoc::Stack) stackOff -= paramSize(pd.type);
		uint32_t raw = readParamRaw(pd, frame.sp, stackOff);
		params += pd.name;
		params += ':';
		params += formatParam(pd, raw);
	}

	fprintf(stdout, "%*s→ %u [%u] %s(%s) [caller:$%06X]\n", indent, "", (unsigned)frame.entryCycle,
			(unsigned)frame.appId, def.name.c_str(), params.c_str(), (unsigned)frame.callerPC);
}

void TrapTracer::emitEntry(const TrapFrame &frame)
{
	std::string name = unknownTrapName(frame.trapWord);

	fprintf(stdout, "%*s→ %u [%u] %s() [caller:$%06X]\n", frame.depth * 2, "",
			(unsigned)frame.entryCycle, (unsigned)frame.appId, name.c_str(),
			(unsigned)frame.callerPC);
}

void TrapTracer::emitAutoPop(const TrapFrame &frame, const char *name)
{
	std::string nameBuf;
	if (!name)
	{
		nameBuf = unknownTrapName(frame.trapWord);
		name = nameBuf.c_str();
	}

	fprintf(stdout, "%*s= %u [%u] %s [auto-pop $%04X]\n", frame.depth * 2, "",
			(unsigned)frame.entryCycle, (unsigned)frame.appId, name, (unsigned)frame.trapWord);
}

void TrapTracer::emitExit(const TrapFrame &frame, const TrapDef &def)
{
	int indent = frame.depth * 2;

	std::string outParams;
	int totalStack = 0;
	for (const auto &pd : def.paramsOut)
		if (pd.loc == ParamLoc::Stack) totalStack += paramSize(pd.type);
	int stackOff = totalStack;
	for (size_t i = 0; i < def.paramsOut.size(); ++i)
	{
		if (i > 0) outParams += " ";
		const ParamDef &pd = def.paramsOut[i];
		if (pd.loc == ParamLoc::Stack) stackOff -= paramSize(pd.type);
		uint32_t raw = readParamRaw(pd, frame.sp, stackOff);
		outParams += pd.name;
		outParams += ':';
		outParams += formatParam(pd, raw);
	}

	uint32_t delta = g_instructionCount - frame.entryCycle;

	if (!outParams.empty())
	{
		fprintf(stdout, "%*s← %u [%u] %s → %s  (+%u cycles)\n", indent, "",
				(unsigned)g_instructionCount, (unsigned)frame.appId, def.name.c_str(),
				outParams.c_str(), (unsigned)delta);
	}
	else
	{
		fprintf(stdout, "%*s← %u [%u] %s  (+%u cycles)\n", indent, "", (unsigned)g_instructionCount,
				(unsigned)frame.appId, def.name.c_str(), (unsigned)delta);
	}
}

void TrapTracer::emitExit(const TrapFrame &frame)
{
	std::string name = unknownTrapName(frame.trapWord);
	uint32_t delta = g_instructionCount - frame.entryCycle;

	fprintf(stdout, "%*s← %u [%u] %s  (+%u cycles)\n", frame.depth * 2, "",
			(unsigned)g_instructionCount, (unsigned)frame.appId, name.c_str(), (unsigned)delta);
}

void TrapTracer::flushStack(const char *reason)
{
	for (int i = depth_ - 1; i >= 0; --i)
	{
		const TrapFrame &frame = stack_[i];
		const TrapDef *def = defs_.find(frame.trapWord);
		const char *name = def ? def->name.c_str() : trap_dict_name(frame.trapWord);
		char nameBuf[16];
		if (!name)
		{
			snprintf(nameBuf, sizeof(nameBuf), "$%04X", frame.trapWord);
			name = nameBuf;
		}
		int indent = frame.depth * 2;
		fprintf(stdout, "%*s⊘ %s [abandoned — %s]\n", indent, "", name, reason);
	}
	depth_ = 0;
	overflowWarned_ = false;
}

/* ── parameter reading ───────────────────────────────── */

uint32_t TrapTracer::readParamRaw(const ParamDef &p, uint32_t sp, int &stackOffset)
{
	if (p.loc != ParamLoc::Stack)
	{
		/* Register-based (OS traps) */
		int idx = static_cast<int>(p.loc) - 1; /* D0=1 → index 0 */
		uint32_t dregs[8], aregs[8];
		m68k_getRegs(dregs, aregs);
		if (idx < 8)
			return dregs[idx];
		else
			return aregs[idx - 8];
	}

	/* Stack-based (Toolbox traps) — Pascal convention, offset pre-computed by caller */
	uint32_t addr = sp + stackOffset;
	int sz = paramSize(p.type);

	switch (sz)
	{
		case 2:
			return get_vm_word(addr);
		case 4:
			return get_vm_long(addr);
		case 8:
			return get_vm_long(addr); /* Rect: return address of first word */
		default:
			return get_vm_word(addr);
	}
}

/* ── type-specific formatters ────────────────────────── */

std::string TrapTracer::formatOSType(uint32_t raw)
{
	char buf[7];
	buf[0] = '\'';
	for (int i = 0; i < 4; ++i)
	{
		uint8_t ch = (raw >> (24 - i * 8)) & 0xFF;
		buf[1 + i] = (ch >= 0x20 && ch < 0x7F) ? static_cast<char>(ch) : '.';
	}
	buf[5] = '\'';
	buf[6] = '\0';
	return buf;
}

std::string TrapTracer::formatStr255(uint32_t addr)
{
	if (addr == 0) return "\"\"";

	uint8_t len = static_cast<uint8_t>(get_vm_byte(addr));
	if (len > 63) len = 63; /* cap for display */

	std::string s = "\"";
	for (int i = 0; i < len; ++i)
	{
		char ch = static_cast<char>(get_vm_byte(addr + 1 + i));
		if (ch >= 0x20 && ch < 0x7F)
			s += ch;
		else
			s += '.';
	}
	s += '"';
	return s;
}

std::string TrapTracer::formatOSErr(int16_t err)
{
	const char *name = defs_.errorName(err);
	char buf[64];
	if (name)
		snprintf(buf, sizeof(buf), "%d %s", (int)err, name);
	else
		snprintf(buf, sizeof(buf), "%d", (int)err);
	return buf;
}

std::string TrapTracer::formatParam(const ParamDef &p, uint32_t rawValue)
{
	char buf[64];
	switch (p.type)
	{
		case ParamType::Byte:
			snprintf(buf, sizeof(buf), "$%02X", rawValue & 0xFF);
			return buf;
		case ParamType::Word:
			snprintf(buf, sizeof(buf), "%d", static_cast<int16_t>(rawValue & 0xFFFF));
			return buf;
		case ParamType::Long:
			snprintf(buf, sizeof(buf), "$%08X", rawValue);
			return buf;
		case ParamType::Ptr:
			snprintf(buf, sizeof(buf), "$%08X", rawValue);
			return buf;
		case ParamType::Handle:
		{
			uint32_t deref = 0;
			if (rawValue != 0) deref = get_vm_long(rawValue);
			snprintf(buf, sizeof(buf), "$%08X→$%08X", rawValue, deref);
			return buf;
		}
		case ParamType::OSType:
			return formatOSType(rawValue);
		case ParamType::Str255:
			return formatStr255(rawValue);
		case ParamType::OSErr:
			return formatOSErr(static_cast<int16_t>(rawValue));
		case ParamType::Boolean:
			return (rawValue & 0xFF) ? "true" : "false";
		case ParamType::Rect:
		{
			/* rawValue is address of rect; read 4 words */
			int16_t t = static_cast<int16_t>(get_vm_word(rawValue));
			int16_t l = static_cast<int16_t>(get_vm_word(rawValue + 2));
			int16_t b = static_cast<int16_t>(get_vm_word(rawValue + 4));
			int16_t r = static_cast<int16_t>(get_vm_word(rawValue + 6));
			snprintf(buf, sizeof(buf), "{%d,%d,%d,%d}", t, l, b, r);
			return buf;
		}
		case ParamType::Point:
		{
			int16_t v = static_cast<int16_t>((rawValue >> 16) & 0xFFFF);
			int16_t h = static_cast<int16_t>(rawValue & 0xFFFF);
			snprintf(buf, sizeof(buf), "{%d,%d}", v, h);
			return buf;
		}
	}
	snprintf(buf, sizeof(buf), "$%08X", rawValue);
	return buf;
}

/* ── low-memory readers ──────────────────────────────── */

uint16_t TrapTracer::readAppId() const
{
	return get_vm_word(0x0900); /* CurApRefNum */
}

std::string TrapTracer::readAppName() const
{
	uint8_t len = static_cast<uint8_t>(get_vm_byte(0x0910)); /* CurApName */
	if (len > 31) len = 31;
	std::string s(len, '\0');
	for (int i = 0; i < len; ++i)
		s[i] = static_cast<char>(get_vm_byte(0x0911 + i));
	return s;
}
