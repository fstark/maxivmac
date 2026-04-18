/*
	trap_tracer.cpp -- Hierarchical A-line trap tracer implementation
*/

#include "cpu/trap_tracer.h"
#include "cpu/trap_counter.h"
#include "core/machine.h"
#include "cpu/m68k.h"
#include "debugger/dbg_io.h"
#include "lang/type_registry.h"

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <format>

/* -- globals ------------------------------------------- */

TrapDefs g_trapDefs;
TrapTracer g_tracer(g_trapDefs);

/* -- param size helper --------------------------------- */

static int paramSize(const ParamDef &pd)
{
	if (pd.isStructPtr) return 4;
	return g_typeRegistry().stackSize(pd.typeName);
}

/* -- TrapTracer ---------------------------------------- */

TrapTracer::TrapTracer(TrapDefs &defs) : defs_(defs)
{
	allowed_.set(); /* trace all traps by default */
}

void TrapTracer::setIO(DbgIO *io)
{
	io_ = io;
}

void TrapTracer::emitStr(std::string_view s)
{
	if (io_)
		io_->write("%.*s", (int)s.size(), s.data());
	else
		fwrite(s.data(), 1, s.size(), stdout);
}

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

void TrapTracer::addTrap(uint16_t trapWord)
{
	allowed_.set(trapWord);
}

void TrapTracer::removeTrap(uint16_t trapWord)
{
	allowed_.reset(trapWord);
}

void TrapTracer::addAllTraps()
{
	allowed_.set();
}

void TrapTracer::removeAllTraps()
{
	allowed_.reset();
}

void TrapTracer::addSubtrap(uint16_t parentTrapWord, uint16_t selector)
{
	uint32_t synKey =
		(static_cast<uint32_t>(TrapDefs::maskTrapWord(parentTrapWord)) << 16) | selector;
	subtrapAllowed_[synKey] = true;
	/* Ensure parent is allowed so enter() fires */
	allowed_.set(parentTrapWord);
}

void TrapTracer::removeSubtrap(uint16_t parentTrapWord, uint16_t selector)
{
	uint32_t synKey =
		(static_cast<uint32_t>(TrapDefs::maskTrapWord(parentTrapWord)) << 16) | selector;
	subtrapAllowed_.erase(synKey);
}

bool TrapTracer::hasSubtrapFilter() const
{
	return !subtrapAllowed_.empty();
}

uint16_t TrapTracer::readSelector(const DispatchInfo &info)
{
	int stackOff = 0;
	uint32_t raw = readParamRaw(info.selectorParam, 0, stackOff);
	return static_cast<uint16_t>(raw);
}

/* -- enter --------------------------------------------- */

void TrapTracer::enter(uint16_t trapWord)
{
	if (!enabled_) return;

	if (!allowed_.test(trapWord)) return;

	/* Context-switch detection */
	uint16_t appId = readAppId();
	if (depth_ > 0 && appId != lastAppId_)
	{
		flushStack("context switch");
		std::string appName = readAppName();
		emitStr(std::format("-- context switch: appId={} \"{}\" --\n", appId, appName));
	}
	lastAppId_ = appId;

	/* Overflow guard */
	if (depth_ >= maxDepth_)
	{
		if (!overflowWarned_)
		{
			emitStr(std::format(
				"[TRACE] nesting overflow at depth {}, suppressing further nesting\n", maxDepth_));
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

	/* Dispatch trap resolution: read selector and resolve subtrap */
	const TrapDef *effectiveDef = def;
	uint16_t subtrapSel = 0;
	if (def && def->dispatch)
	{
		subtrapSel = readSelector(*def->dispatch);
		auto *sub = defs_.findSubtrap(trapWord, subtrapSel);
		if (sub) effectiveDef = &sub->def;

		/* Check subtrap filter */
		if (!subtrapAllowed_.empty())
		{
			uint32_t synKey =
				(static_cast<uint32_t>(TrapDefs::maskTrapWord(trapWord)) << 16) | subtrapSel;
			if (subtrapAllowed_.find(synKey) == subtrapAllowed_.end()) return;
		}
	}
	frame.subtrapSelector = subtrapSel;

	/* Auto-pop Toolbox trap: bit 10 set on a Toolbox trap ($Axxx where bit 11 set).
	   The ROM dispatcher discards the return address, so the ROM routine
	   returns directly to the caller's caller.  Don't push a frame. */
	bool autoPop = (trapWord & 0x0800) && (trapWord & 0x0400);

	/* Handle noreturn traps */
	if (effectiveDef && effectiveDef->noreturn)
	{
		emitEntry(frame, *effectiveDef);
		flushStack(effectiveDef->name.c_str());
		return;
	}

	if (autoPop)
	{
		const char *name = effectiveDef ? effectiveDef->name.c_str() : nullptr;
		emitAutoPop(frame, name);
		return;
	}

	/* Emit entry and push frame */
	if (effectiveDef)
	{
		/* Save StructPtr addresses for show-out at exit */
		if (!effectiveDef->showOut.empty())
		{
			int totalStack = 0;
			for (const auto &pd : effectiveDef->paramsIn)
				if (pd.loc == ParamLoc::Stack) totalStack += paramSize(pd);
			int stackOff = totalStack;
			for (const auto &pd : effectiveDef->paramsIn)
			{
				if (pd.loc == ParamLoc::Stack) stackOff -= paramSize(pd);
				if (pd.isStructPtr && frame.nStructAddrs < TrapFrame::kMaxStructAddrs)
				{
					uint32_t raw = readParamRaw(pd, frame.sp, stackOff);
					auto &sa = frame.structAddrs[frame.nStructAddrs++];
					sa.paramName = nullptr; /* will be set below */
					sa.addr = raw;
					/* Store param name -- it lives in the TrapDef which outlives the frame.
					   We look it up by index at exit. */
				}
			}
			/* Store param name pointers separately -- we need them stable.
			   Re-scan to set them (TrapDef strings are stable in the map). */
			int idx = 0;
			for (const auto &pd : effectiveDef->paramsIn)
			{
				if (pd.isStructPtr && idx < frame.nStructAddrs)
					frame.structAddrs[idx++].paramName = pd.name.c_str();
			}
		}

		emitEntry(frame, *effectiveDef);
	}
	else
		emitEntry(frame);
	stack_[depth_++] = frame;
}

/* -- checkReturn --------------------------------------- */

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
			emitStr(std::format("{:>{}} X {} [missed return]\n", "", orphan.depth * 2, name));
		}
	}

	/* Pop the matched frame */
	depth_ = match;
	const TrapFrame &frame = stack_[match];
	const TrapDef *def = defs_.find(frame.trapWord);
	/* Resolve subtrap for exit formatting */
	const TrapDef *exitDef = def;
	if (frame.subtrapSelector != 0)
	{
		auto *sub = defs_.findSubtrap(frame.trapWord, frame.subtrapSelector);
		if (sub) exitDef = &sub->def;
	}
	if (exitDef)
		emitExit(frame, *exitDef);
	else
		emitExit(frame);
	overflowWarned_ = false;
}

/* -- emit helpers -------------------------------------- */

static std::string unknownTrapName(uint16_t trapWord)
{
	const char *dictName = trap_dict_name(trapWord);
	if (dictName) return std::string(dictName) + "?";
	char buf[16];
	snprintf(buf, sizeof(buf), "$%04X?", trapWord);
	return buf;
}

/* Find a show-in or show-out filter for a given param name. */
static const StructFieldFilter *findFilter(const std::vector<StructFieldFilter> &filters,
										   std::string_view paramName)
{
	for (auto &f : filters)
		if (f.paramName == paramName) return &f;
	return nullptr;
}

/* Format a StructPtr -- returns just "$ADDR" for inline display.
   The caller handles the indented field dump via formatStructDump(). */
std::string TrapTracer::formatStructPtr(const ParamDef & /*p*/, uint32_t rawValue,
										const StructFieldFilter * /*filter*/)
{
	char buf[64];
	snprintf(buf, sizeof(buf), "$%08X", rawValue);
	return buf;
}

/* Produce an indented field dump for a struct at the given address.
   Each line is formatted as: <pad><paramName>^.<fieldName>:  <value>\n
   If filter is non-null, only matching fields are included. */
std::string TrapTracer::formatStructDump(const ParamDef &p, uint32_t rawValue,
										 const StructFieldFilter *filter, std::string_view pad)
{
	return formatStructDumpFor(p.typeName, p.name, rawValue, filter, pad);
}

std::string TrapTracer::formatStructDumpFor(std::string_view structName, std::string_view paramName,
											uint32_t addr, const StructFieldFilter *filter,
											std::string_view pad)
{
	auto &tr = g_typeRegistry();
	if (addr == 0 || !tr.has(structName)) return {};

	auto fields = tr.read(structName, addr);
	if (fields.empty()) return {};

	/* Filter fields if requested */
	std::vector<const FieldValue *> keep;
	for (auto &f : fields)
	{
		if (filter && !filter->fields.empty())
		{
			std::string_view leaf = f.name;
			auto dot = leaf.rfind('.');
			if (dot != std::string_view::npos) leaf = leaf.substr(dot + 1);
			bool match = false;
			for (auto &want : filter->fields)
				if (leaf == want)
				{
					match = true;
					break;
				}
			if (!match) continue;
		}
		keep.push_back(&f);
	}
	if (keep.empty()) return {};

	/* Measure the longest prefixed name for column alignment */
	std::string prefix;
	prefix += paramName;
	prefix += "^.";
	size_t maxLen = 0;
	for (auto *f : keep)
	{
		size_t len = prefix.size() + f->name.size();
		if (len > maxLen) maxLen = len;
	}
	if (maxLen > 40) maxLen = 40;

	/* Format each field */
	std::string result;
	for (auto *f : keep)
	{
		result += pad;
		std::string label = prefix + f->name + ":";
		result += label;
		size_t padTo = maxLen + 3; /* label + ":" + at least 1 space */
		if (label.size() < padTo)
			result.append(padTo - label.size(), ' ');
		else
			result += ' ';
		result += f->display;
		result += '\n';
	}
	return result;
}

void TrapTracer::emitEntry(const TrapFrame &frame, const TrapDef &def)
{
	int indent = frame.depth * 2;

	/* Build header: "-> CYCLE [APP] TrapName(" */
	std::string hdr =
		std::format("{:>{}}-> {} [{}] {}(", "", indent, frame.entryCycle, frame.appId, def.name);

	/* Padding string for continuation lines -- aligns under the '(' */
	std::string pad(hdr.size(), ' ');

	/* Build inline params and collect StructPtr dumps */
	std::string params;
	std::string structDumps;

	int totalStack = 0;
	for (const auto &pd : def.paramsIn)
		if (pd.loc == ParamLoc::Stack) totalStack += paramSize(pd);
	int stackOff = totalStack;

	for (size_t i = 0; i < def.paramsIn.size(); ++i)
	{
		if (i > 0) params += ", ";
		const ParamDef &pd = def.paramsIn[i];
		if (pd.loc == ParamLoc::Stack) stackOff -= paramSize(pd);
		uint32_t raw = readParamRaw(pd, frame.sp, stackOff);
		params += pd.name;
		params += ':';

		if (pd.isStructPtr)
		{
			auto *filter = findFilter(def.showIn, pd.name);
			params += formatStructPtr(pd, raw, filter);
			std::string dump = formatStructDump(pd, raw, filter, pad);
			if (!dump.empty()) structDumps += dump;
		}
		else
		{
			params += formatParam(pd, raw);
		}
	}

	if (structDumps.empty())
	{
		/* Simple case: everything fits on one line */
		emitStr(std::format("{}{}) [caller:${:06X}]\n", hdr, params, frame.callerPC));
	}
	else
	{
		/* Multi-line: header + inline params, then struct fields, then closing */
		emitStr(std::format("{}{}\n{}{}) [caller:${:06X}]\n", hdr, params, structDumps, pad,
							frame.callerPC));
	}
}

void TrapTracer::emitEntry(const TrapFrame &frame)
{
	std::string name = unknownTrapName(frame.trapWord);

	emitStr(std::format("{:>{}}-> {} [{}] {}() [caller:${:06X}]\n", "", frame.depth * 2,
						frame.entryCycle, frame.appId, name, frame.callerPC));
}

void TrapTracer::emitAutoPop(const TrapFrame &frame, const char *name)
{
	std::string nameBuf;
	if (!name)
	{
		nameBuf = unknownTrapName(frame.trapWord);
		name = nameBuf.c_str();
	}

	emitStr(std::format("{:>{}}= {} [{}] {} [auto-pop ${:04X}]\n", "", frame.depth * 2,
						frame.entryCycle, frame.appId, name, frame.trapWord));
}

void TrapTracer::emitExit(const TrapFrame &frame, const TrapDef &def)
{
	int indent = frame.depth * 2;

	/* Toolbox (Pascal) stack layout at entry:
		 frame.sp -> [input params...]   inStackSize bytes
					[result space...]    outStackSize bytes
	   The trap consumes inputs and writes results into the result space,
	   so output values live at frame.sp + inStackSize. */
	int inStackSize = 0;
	for (const auto &pd : def.paramsIn)
		if (pd.loc == ParamLoc::Stack) inStackSize += paramSize(pd);
	uint32_t outBase = frame.sp + inStackSize;

	std::string outParams;
	int totalStack = 0;
	for (const auto &pd : def.paramsOut)
		if (pd.loc == ParamLoc::Stack) totalStack += paramSize(pd);
	int stackOff = totalStack;
	for (size_t i = 0; i < def.paramsOut.size(); ++i)
	{
		if (i > 0) outParams += " ";
		const ParamDef &pd = def.paramsOut[i];
		if (pd.loc == ParamLoc::Stack) stackOff -= paramSize(pd);
		uint32_t raw = readParamRaw(pd, outBase, stackOff);
		outParams += pd.name;
		outParams += ':';
		if (pd.isStructPtr)
		{
			auto *filter = findFilter(def.showOut, pd.name);
			outParams += formatStructPtr(pd, raw, filter);
		}
		else
		{
			outParams += formatParam(pd, raw);
		}
	}

	uint32_t delta = g_instructionCount - frame.entryCycle;

	/* Build the exit header to measure its width for alignment */
	std::string hdr;
	if (!outParams.empty())
		hdr = std::format("{:>{}}<- {} [{}] {} -> {}  (+{} cycles)", "", indent, g_instructionCount,
						  frame.appId, def.name, outParams, delta);
	else
		hdr = std::format("{:>{}}<- {} [{}] {}  (+{} cycles)", "", indent, g_instructionCount,
						  frame.appId, def.name, delta);

	/* show-out: dump filtered struct fields from saved addresses */
	std::string showOutDump;
	if (!def.showOut.empty())
	{
		/* Compute pad width to align with the entry header position */
		size_t padWidth = hdr.size();
		/* Cap at a reasonable width to avoid huge indents */
		if (padWidth > 60) padWidth = indent + 20;
		std::string pad(padWidth, ' ');

		for (auto &filter : def.showOut)
		{
			uint32_t addr = 0;
			std::string structName;
			for (int i = 0; i < frame.nStructAddrs; ++i)
			{
				if (frame.structAddrs[i].paramName &&
					filter.paramName == frame.structAddrs[i].paramName)
				{
					addr = frame.structAddrs[i].addr;
					break;
				}
			}
			for (auto &pd : def.paramsIn)
			{
				if (pd.name == filter.paramName && pd.isStructPtr)
				{
					structName = pd.typeName;
					break;
				}
			}
			if (addr == 0 || structName.empty()) continue;

			showOutDump += formatStructDumpFor(structName, filter.paramName, addr, &filter, pad);
		}
	}

	if (showOutDump.empty())
		emitStr(hdr + "\n");
	else
		emitStr(hdr + "\n" + showOutDump);
}

void TrapTracer::emitExit(const TrapFrame &frame)
{
	std::string name = unknownTrapName(frame.trapWord);
	uint32_t delta = g_instructionCount - frame.entryCycle;

	emitStr(std::format("{:>{}}<- {} [{}] {}  (+{} cycles)\n", "", frame.depth * 2,
						g_instructionCount, frame.appId, name, delta));
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
		emitStr(std::format("{:>{}}X {} [abandoned -- {}]\n", "", indent, name, reason));
	}
	depth_ = 0;
	overflowWarned_ = false;
}

/* -- parameter reading --------------------------------- */

uint32_t TrapTracer::readParamRaw(const ParamDef &p, uint32_t sp, int &stackOffset)
{
	if (p.loc != ParamLoc::Stack)
	{
		/* Register-based (OS traps) */
		int idx = static_cast<int>(p.loc) - 1; /* D0=1 -> index 0 */
		uint32_t dregs[8], aregs[8];
		m68k_getRegs(dregs, aregs);
		if (idx < 8)
			return dregs[idx];
		else
			return aregs[idx - 8];
	}

	/* Stack-based (Toolbox traps) -- Pascal convention, offset pre-computed by caller */
	uint32_t addr = sp + stackOffset;
	int sz = paramSize(p);

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

std::string TrapTracer::formatParam(const ParamDef &p, uint32_t rawValue)
{
	if (p.isStructPtr) return formatStructPtr(p, rawValue, nullptr);

	/* Str255/Str63/Str31 in params are pointers — format as PStr */
	auto &tr = g_typeRegistry();
	std::string_view effective = p.typeName;
	if (effective == "Str255" || effective == "Str63" || effective == "Str31") effective = "PStr";

	std::string s = tr.formatValue(effective, rawValue);
	if (!s.empty()) return s;

	/* fallback for unknown types */
	char buf[16];
	snprintf(buf, sizeof(buf), "$%08X", rawValue);
	return buf;
}

/* -- low-memory readers -------------------------------- */

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
