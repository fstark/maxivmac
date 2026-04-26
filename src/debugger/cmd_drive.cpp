/*
	cmd_drive.cpp — Debugger command: mount/unmount/list shared drives
*/

#include "debugger/debugger.h"
#include "debugger/dbg_io.h"
#include "debugger/cmd_parser.h"
#include "core/extn_extfs.h"

#include <cstdlib>

void CmdDrive(Debugger &dbg, const std::vector<Token> &args)
{
	auto &io = dbg.io();

	if (args.empty())
	{
		io.write("usage: drive mount <path> | unmount <slot> | list\n");
		return;
	}

	if (args[0].text == "mount" && args.size() >= 2)
	{
		int slot = ExtFSMountDrive(args[1].text);
		if (slot < 0)
			io.write("mount failed: path invalid or no free slot\n");
		else
			io.write("mounted slot %d\n", slot);
	}
	else if (args[0].text == "unmount" && args.size() >= 2)
	{
		int slot = std::atoi(args[1].text.c_str());
		if (!ExtFSUnmountDrive(slot))
			io.write("slot %d is not mounted\n", slot);
		else
			io.write("unmounted slot %d\n", slot);
	}
	else if (args[0].text == "list")
	{
		auto printLine = [](void *ctx, const char *line)
		{ static_cast<DbgIO *>(ctx)->write("%s\n", line); };
		ExtFSDriveList(printLine, &io);
	}
	else
	{
		io.write("usage: drive mount <path> | unmount <slot> | list\n");
	}
}
