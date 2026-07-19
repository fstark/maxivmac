/*
	cmd_help.cpp — Help and quit commands
*/

#include "debugger/debugger.h"
#include "debugger/dbg_io.h"
#include "debugger/cmd_parser.h"

#include <cstdio>
#include <cstdlib>
#include <string_view>

void CmdHelp(Debugger &dbg, const std::vector<Token> &args)
{
	if (!args.empty() && args[0].isWord())
	{
		/* Detailed help for a specific command */
		auto *table = dbg.commandTable();
		int n = dbg.commandTableSize();
		auto *entry = DispatchCommand(args[0].text, table, n);
		if (entry)
		{
			dbg.io().write("%.*s", static_cast<int>(entry->helpFull.size()),
						   entry->helpFull.data());
			return;
		}
		return;
	}

	/* Auto-generated help grouped by category */
	dbg.io().write("maxivmac debugger commands:\n\n");

	static constexpr std::string_view categories[] = {
		"Execution", "Breakpoints", "Memory", "Tracing",
		"Information", "Scripting", "Guest", "Other"
	};

	auto *table = dbg.commandTable();
	int n = dbg.commandTableSize();

	for (auto cat : categories)
	{
		bool headerShown = false;
		for (int i = 0; i < n; ++i)
		{
			auto &cmd = table[i];
			if (cmd.category != cat) continue;
			if (!headerShown)
			{
				dbg.io().write("%.*s:\n", static_cast<int>(cat.size()), cat.data());
				headerShown = true;
			}
			if (cmd.shortcut.empty())
				dbg.io().write("  %-14.*s %.*s\n",
							   static_cast<int>(cmd.name.size()), cmd.name.data(),
							   static_cast<int>(cmd.helpBrief.size()), cmd.helpBrief.data());
			else
				dbg.io().write("  %-14.*s %.*s\n",
							   // Format: "name (shortcut)"
							   static_cast<int>(cmd.name.size() + 3 + cmd.shortcut.size()),
							   (std::string(cmd.name) + " (" + std::string(cmd.shortcut) + ")").c_str(),
							   static_cast<int>(cmd.helpBrief.size()), cmd.helpBrief.data());
		}
		if (headerShown) dbg.io().write("\n");
	}

	dbg.io().write("Empty line repeats last command. 'help <cmd>' for details.\n");
}

void CmdQuit(Debugger &dbg, const std::vector<Token> &)
{
	dbg.io().write("Quitting.\n");
	std::exit(0);
}

void CmdExit(Debugger &dbg, const std::vector<Token> &args)
{
	int code = 0;
	if (!args.empty() && args[0].isNumber())
		code = static_cast<int>(args[0].numValue);
	dbg.io().write("Exiting with code %d\n", code);
	std::exit(code);
}
