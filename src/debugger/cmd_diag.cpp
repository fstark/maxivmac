/*
	cmd_diag.cpp — Debugger command: diag subsystem on/off
*/

#include "core/diag.h"
#include "debugger/debugger.h"
#include "debugger/dbg_io.h"
#include "debugger/cmd_parser.h"

void CmdDiag(Debugger &dbg, const std::vector<Token> &args)
{
	if (args.empty() || args[0].isEnd())
	{
		/* No arguments: list all subsystems and their state */
		for (size_t i = 0; i < DiagConfig::count(); ++i)
		{
			auto s = static_cast<DiagSubsystem>(i);
			dbg.io().write("  %-10s %s\n", DiagConfig::tag(s), Diag().isEnabled(s) ? "on" : "off");
		}
		return;
	}

	auto &target = args[0].text;

	if (target == "all")
	{
		if (args.size() < 2 || args[1].isEnd())
		{
			dbg.io().write("Usage: diag all <on|off>\n");
			return;
		}
		bool on = (args[1].text == "on");
		Diag().setAll(on);
		dbg.io().write("All diagnostics %s\n", on ? "enabled" : "disabled");
		return;
	}

	DiagSubsystem s;
	if (!DiagConfig::fromName(target.c_str(), s))
	{
		dbg.io().write("Unknown subsystem '%s'. Known:", target.c_str());
		for (size_t i = 0; i < DiagConfig::count(); ++i)
		{
			dbg.io().write(" %s", DiagConfig::tag(static_cast<DiagSubsystem>(i)));
		}
		dbg.io().write("\n");
		return;
	}

	if (args.size() < 2 || args[1].isEnd())
	{
		dbg.io().write("%s is %s\n", DiagConfig::tag(s), Diag().isEnabled(s) ? "on" : "off");
		return;
	}

	bool on = (args[1].text == "on");
	Diag().set(s, on);
	dbg.io().write("%s %s\n", DiagConfig::tag(s), on ? "enabled" : "disabled");
}
