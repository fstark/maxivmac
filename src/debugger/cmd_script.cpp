// Script commands — wait, timeout, fail for automated scripting.
#include "debugger/debugger.h"
#include "debugger/dbg_io.h"
#include "debugger/cmd_parser.h"
#include "debugger/symbols.h"
#include "debugger/bp_screen.h"
#include "debugger/script_keymap.h"

#include "core/machine.h"
#include "core/ict_scheduler.h"
#include "guest/guest_dialog.h"
#include "platform/platform.h"
#include "platform/keycodes.h"
#include "platform/common/event_queue.h"
#include "util/macroman.h"

#include <cinttypes>
#include <cstdlib>

/* Default timeout budget (cycles). ~5 seconds at 8 MHz. */
static ScaledCycleCount s_defaultTimeout = 40'000'000;

void CmdTimeout(Debugger &dbg, const std::vector<Token> &args)
{
	if (args.empty() || !args[0].isNumber())
	{
		dbg.io().write("timeout = %" PRIu64 " cycles\n", s_defaultTimeout);
		dbg.io().write("Usage: timeout <cycles>\n");
		return;
	}
	s_defaultTimeout = args[0].numValue;
	dbg.io().write("timeout = %" PRIu64 " cycles\n", s_defaultTimeout);
}

ScaledCycleCount ScriptDefaultTimeout()
{
	return s_defaultTimeout;
}

void CmdWait(Debugger &dbg, const std::vector<Token> &args)
{
	if (args.empty() || args[0].isEnd())
	{
		dbg.io().write("Usage: wait text \"pattern\" [cycles]\n");
		dbg.io().write("       wait screen \"ref.png\" [cycles] [pct]\n");
		dbg.io().write("       wait for <cycles>\n");
		dbg.io().write("       wait off [cycles]\n");
		dbg.io().write("       wait <addr|symbol> [cycles]\n");
		dbg.io().write("       wait trap <name> [cycles]\n");
		return;
	}

	// Determine cycle budget (0 = no timeout)
	auto parseBudget = [&](size_t argIdx) -> ScaledCycleCount
	{
		if (argIdx < args.size() && args[argIdx].isNumber()) return args[argIdx].numValue;
		return s_defaultTimeout;
	};

	auto computeDeadline = [&](ScaledCycleCount budget) -> ScaledCycleCount
	{
		if (budget == 0) return 0; // infinite
		return g_ict.getCurrent() + budget;
	};

	Debugger::Breakpoint bp;
	bp.enabled = true;
	bp.temporary = true;
	bp.scriptOwned = true;

	if (args[0].isWord("text"))
	{
		if (args.size() < 2 || args[1].isEnd())
		{
			dbg.io().write("Usage: wait text \"pattern\" [cycles]\n");
			return;
		}
		bp.kind = Debugger::Breakpoint::Kind::Text;
		bp.textPattern = args[1].text;
		bp.timeoutAt = computeDeadline(parseBudget(2));
	}
	else if (args[0].isWord("screen"))
	{
		if (args.size() < 2 || args[1].isEnd())
		{
			dbg.io().write("Usage: wait screen \"ref.png\" [cycles] [pct]\n");
			return;
		}
		bp.kind = Debugger::Breakpoint::Kind::Screen;
		if (!bp.screenMatcher.loadReference(args[1].text))
		{
			dbg.io().write("Error: cannot load reference PNG '%s'\n", args[1].text.c_str());
			return;
		}
		bp.timeoutAt = computeDeadline(parseBudget(2));
		// Optional threshold
		if (args.size() >= 4 && args[3].isNumber())
			bp.screenMatcher.threshold = static_cast<float>(args[3].numValue);
	}
	else if (args[0].isWord("off"))
	{
		bp.kind = Debugger::Breakpoint::Kind::PowerOff;
		bp.timeoutAt = computeDeadline(parseBudget(1));
	}
	else if (args[0].isWord("for"))
	{
		if (args.size() < 2 || !args[1].isNumber())
		{
			dbg.io().write("Usage: wait for <cycles>\n");
			return;
		}
		ScaledCycleCount target = g_ict.getCurrent() + args[1].numValue;
		uint32_t id = dbg.setCycleBreak(target, true);
		dbg.io().write("Waiting %'" PRIu64 " cycles (bp %u)...\n", args[1].numValue, id);
		dbg.setRunning();
		return;
	}
	else if (args[0].isWord("trap"))
	{
		if (args.size() < 2 || args[1].isEnd())
		{
			dbg.io().write("Usage: wait trap <name> [cycles]\n");
			return;
		}
		uint32_t addr = 0;
		uint16_t trapWord = 0;
		uint16_t subtrapSel = 0;
		if (!SymbolsResolve(args[1].text, addr, trapWord, subtrapSel))
		{
			dbg.io().write("Cannot resolve '%s'\n", args[1].text.c_str());
			return;
		}
		bp.kind = Debugger::Breakpoint::Kind::Trap;
		bp.trapWord = trapWord;
		bp.subtrapSelector = subtrapSel;
		bp.timeoutAt = computeDeadline(parseBudget(2));
	}
	else
	{
		// Address or symbol
		uint32_t addr = 0;
		uint16_t trapWord = 0;
		uint16_t subtrapSel = 0;
		if (args[0].isNumber())
		{
			addr = args[0].numValue;
		}
		else if (args[0].isWord())
		{
			if (!SymbolsResolve(args[0].text, addr, trapWord, subtrapSel))
			{
				dbg.io().write("Cannot resolve '%s'\n", args[0].text.c_str());
				return;
			}
		}
		if (trapWord != 0)
		{
			bp.kind = Debugger::Breakpoint::Kind::Trap;
			bp.trapWord = trapWord;
			bp.subtrapSelector = subtrapSel;
		}
		else
		{
			bp.kind = Debugger::Breakpoint::Kind::Address;
			bp.address = addr;
		}
		bp.timeoutAt = computeDeadline(parseBudget(1));
	}

	uint32_t id = dbg.addBreakpoint(std::move(bp));
	auto deadline = dbg.breakpoints().back().timeoutAt;
	if (deadline == 0)
		dbg.io().write("Waiting (bp %u, no timeout)...\n", id);
	else
		dbg.io().write("Waiting (bp %u, timeout %" PRIu64 " cycles)...\n", id,
					   deadline - g_ict.getCurrent());
	dbg.setRunning();
}

void CmdFail(Debugger &dbg, const std::vector<Token> &args)
{
	std::string msg = "script failed";
	if (!args.empty() && !args[0].isEnd()) msg = args[0].text;

	dbg.io().write("FAIL: %s\n", msg.c_str());

	// Save a failure screenshot
	SaveScreenshot("fail-screenshot.png");

	std::exit(1);
}

void CheckScriptTimeouts()
{
	auto *dbg = Debugger::instance();
	if (!dbg) return;

	auto &bps = dbg->breakpoints();
	ScaledCycleCount now = g_ict.getCurrent();

	for (size_t i = 0; i < bps.size(); ++i)
	{
		auto &bp = bps[i];
		if (!bp.scriptOwned) continue;
		if (!bp.enabled) continue;
		if (bp.timeoutAt == 0) continue;
		if (now < bp.timeoutAt) continue;

		// Timeout fired!
		dbg->io().write("TIMEOUT: wait (bp %u) expired after %" PRIu64 " cycles\n", bp.id,
						bp.timeoutAt);

		SaveScreenshot("timeout-screenshot.png");

		uint32_t bpId = bp.id;
		dbg->deleteById(bpId);

		// In headless/scripting mode, exit with error
		std::exit(1);
	}
}

void CmdType(Debugger &dbg, const std::vector<Token> &args)
{
	if (args.empty() || args[0].isEnd())
	{
		dbg.io().write("Usage: type \"text\"\n");
		return;
	}

	// Convert UTF-8 argument to MacRoman for key lookup
	std::string macRoman = MacRomanFromUTF8(args[0].text);
	ScaledCycleCount t = g_ict.getCurrent();

	for (uint8_t ch : macRoman)
	{
		auto [keycode, needShift] = CharToMacKey(ch);
		if (keycode == 0xFF) continue; // unmappable

		if (needShift) EventQ_Push({t, EvtQElKind::Key, {.press = {MKC_Shift, true}}});
		EventQ_Push({t, EvtQElKind::Key, {.press = {keycode, true}}});
		EventQ_Push({t + 80000, EvtQElKind::Key, {.press = {keycode, false}}});
		if (needShift) EventQ_Push({t + 80000, EvtQElKind::Key, {.press = {MKC_Shift, false}}});
		t += 160000; // ~20ms inter-key gap at 8 MHz
	}

	dbg.io().write("type \"%s\"\n", args[0].text.c_str());
}

void CmdKey(Debugger &dbg, const std::vector<Token> &args)
{
	if (args.empty() || args[0].isEnd())
	{
		dbg.io().write("Usage: key <keyspec>  (e.g. cmd-S, return, cmd-shift-N)\n");
		return;
	}

	// Reconstruct keyspec from tokens (tokenizer splits on '-')
	std::string spec;
	for (const auto &tok : args)
	{
		if (tok.isEnd()) break;
		spec += tok.text;
	}

	KeySpec ks = ParseKeySpec(spec);
	if (ks.keycode == 0xFF)
	{
		dbg.io().write("Error: cannot resolve key '%s'\n", spec.c_str());
		return;
	}

	ScaledCycleCount t = g_ict.getCurrent();

	// Press modifiers
	if (ks.modifiers & kModCmd) EventQ_Push({t, EvtQElKind::Key, {.press = {MKC_Command, true}}});
	if (ks.modifiers & kModShift) EventQ_Push({t, EvtQElKind::Key, {.press = {MKC_Shift, true}}});
	if (ks.modifiers & kModOption) EventQ_Push({t, EvtQElKind::Key, {.press = {MKC_Option, true}}});
	if (ks.modifiers & kModCtrl) EventQ_Push({t, EvtQElKind::Key, {.press = {MKC_Control, true}}});

	// Key down/up
	EventQ_Push({t + 10000, EvtQElKind::Key, {.press = {ks.keycode, true}}});
	EventQ_Push({t + 90000, EvtQElKind::Key, {.press = {ks.keycode, false}}});

	// Release modifiers
	ScaledCycleCount release = t + 100000;
	if (ks.modifiers & kModCtrl)
		EventQ_Push({release, EvtQElKind::Key, {.press = {MKC_Control, false}}});
	if (ks.modifiers & kModOption)
		EventQ_Push({release, EvtQElKind::Key, {.press = {MKC_Option, false}}});
	if (ks.modifiers & kModShift)
		EventQ_Push({release, EvtQElKind::Key, {.press = {MKC_Shift, false}}});
	if (ks.modifiers & kModCmd)
		EventQ_Push({release, EvtQElKind::Key, {.press = {MKC_Command, false}}});

	dbg.io().write("Key: %s\n", spec.c_str());
}

void CmdClearKeys(Debugger &dbg, const std::vector<Token> &)
{
	EventQ_ClearFutureKeys();
	dbg.io().write("Cleared pending key events\n");
}

/* ── Guest commands ────────────────────────────────── */

#include "core/extn_extfs.h"

void CmdLaunch(Debugger &dbg, const std::vector<Token> &args)
{
	if (args.empty())
	{
		dbg.io().write("Usage: launch \"path\"\n");
		return;
	}
	auto path = MacRomanFromUTF8(args[0].text);
	ExtFS_QueueGuestCmd(1, path);
	dbg.io().write("Queued launch: %s\n", args[0].text.c_str());
}

void CmdExitToShell(Debugger &, const std::vector<Token> &)
{
	ExtFS_QueueGuestCmd(2);
}

void CmdShutdown(Debugger &, const std::vector<Token> &)
{
	ExtFS_QueueGuestCmd(3);
}

void CmdClick(Debugger &dbg, const std::vector<Token> &args)
{
	if (args.empty() || args[0].isEnd())
	{
		dbg.io().write("Usage: click button \"title\"\n");
		dbg.io().write("       click <x> <y>\n");
		return;
	}

	int16_t clickH, clickV;

	if (args[0].isWord("button"))
	{
		if (args.size() < 2 || args[1].isEnd())
		{
			dbg.io().write("Usage: click button \"title\"\n");
			return;
		}
		auto dlg = guest::readFrontDialog();
		if (!dlg)
		{
			dbg.io().write("Error: front window is not a dialog\n");
			return;
		}
		auto *btn = guest::findButton(*dlg, args[1].text);
		if (!btn)
		{
			dbg.io().write("Error: no button matching \"%s\"\n", args[1].text.c_str());
			return;
		}
		auto pt = guest::itemCenter(*dlg, *btn);
		clickH = pt.h;
		clickV = pt.v;
		dbg.io().write("click button \"%s\" at (%d,%d)\n", btn->text.c_str(), clickH, clickV);
	}
	else if (args[0].isNumber() && args.size() >= 2 && args[1].isNumber())
	{
		clickH = static_cast<int16_t>(args[0].numValue);
		clickV = static_cast<int16_t>(args[1].numValue);
		dbg.io().write("click at (%d,%d)\n", clickH, clickV);
	}
	else
	{
		dbg.io().write("Usage: click button \"title\"\n");
		dbg.io().write("       click <x> <y>\n");
		return;
	}

	// Inject mouse position + button down/up
	ScaledCycleCount t = g_ict.getCurrent();
	EventQ_Push({t, EvtQElKind::MousePos, {.pos = {clickH, clickV}}});
	EventQ_Push({t + 40000, EvtQElKind::MouseButton, {.press = {0, true}}});
	EventQ_Push({t + 120000, EvtQElKind::MouseButton, {.press = {0, false}}});
}

static const char *itemTypeName(guest::DialogItemType t)
{
	switch (t)
	{
		case guest::DialogItemType::Button:
			return "button";
		case guest::DialogItemType::CheckBox:
			return "checkbox";
		case guest::DialogItemType::RadioButton:
			return "radio";
		case guest::DialogItemType::ResControl:
			return "control";
		case guest::DialogItemType::StaticText:
			return "static";
		case guest::DialogItemType::EditText:
			return "edit";
		case guest::DialogItemType::Icon:
			return "icon";
		case guest::DialogItemType::Picture:
			return "picture";
		case guest::DialogItemType::UserItem:
			return "user";
	}
	return "?";
}

void CmdDialog(Debugger &dbg, const std::vector<Token> &)
{
	auto dlg = guest::readFrontDialog();
	if (!dlg)
	{
		dbg.io().write("Front window is not a dialog (or no windows open)\n");
		return;
	}

	dbg.io().write("Dialog @ $%08X  origin=(%d,%d)  portRect=(%d,%d,%d,%d)  default=%d\n",
				   dlg->windowPtr, dlg->origin.h, dlg->origin.v, dlg->portRect.top,
				   dlg->portRect.left, dlg->portRect.bottom, dlg->portRect.right, dlg->defaultItem);
	dbg.io().write("  %zu items:\n", dlg->items.size());

	for (const auto &item : dlg->items)
	{
		dbg.io().write("  #%d  %-8s  %s  (%d,%d,%d,%d)", item.index, itemTypeName(item.type),
					   item.enabled ? "en" : "DIS", item.bounds.top, item.bounds.left,
					   item.bounds.bottom, item.bounds.right);
		if (!item.text.empty()) dbg.io().write("  \"%s\"", item.text.c_str());
		dbg.io().write("\n");
	}
}
