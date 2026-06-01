/*
	bp_power.cpp — Power-off breakpoint handling
*/
#include "debugger/bp_power.h"
#include "debugger/debugger.h"
#include "debugger/dbg_io.h"

void CheckPowerOffBreakpoints()
{
	auto *dbg = Debugger::instance();
	if (!dbg) return;

	auto &bps = dbg->breakpoints();
	for (size_t i = 0; i < bps.size(); ++i)
	{
		auto &bp = bps[i];
		if (bp.kind != Debugger::Breakpoint::Kind::PowerOff) continue;
		if (!bp.enabled) continue;

		dbg->io().write("Breakpoint %u: power-off detected\n", bp.id);

		bool wasTemporary = bp.temporary;
		bool wasScriptOwned = bp.scriptOwned;
		uint32_t bpId = bp.id;

		if (!bp.commands.empty()) dbg->executeCommands(bp.commands);
		if (wasTemporary) dbg->deleteById(bpId);

		dbg->stop("");

		if (wasScriptOwned) dbg->tryResumeScript(nullptr);
		return;
	}
}
