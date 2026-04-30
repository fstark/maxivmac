# DEVREMOVE_PLAN.md — Remove Developer Mode (Implementation Plan)

Reference design: [docs/DEVREMOVE.md](DEVREMOVE.md)

---

> **Phases 1–6 completed 2026-04-30** (commits 46308f2..4b38df5).
> Removed developer mode UI, tool framework, debug windows, overlay button,
> and documentation references. Build + verify + debugger smoke tests all pass.

---

## ~~Phase 1 — Delete tool framework files + CMake cleanup~~

## ~~Phase 2 — Strip Developer mode from ImGuiBackend~~

## ~~Phase 3 — Strip Developer from entry points~~

## ~~Phase 4 — Clean overlay~~

## ~~Phase 5 — Documentation sweep~~

## ~~Phase 6 — Verify removal~~

---

## Phase 7 — Debugger enhancements: `info via`

### 7.1 Add InfoVIA function to cmd_info.cpp

File: `src/debugger/cmd_info.cpp`

Add includes after the existing device includes (after L14):

```cpp
#include "devices/via.h"
#include "devices/via2.h"
```

Add static function before `CmdInfo` (around L255):

```cpp
static void InfoVIA(Debugger &dbg)
{
	if (!g_machine)
	{
		dbg.io().write("Machine not initialized.\n");
		return;
	}

	auto dumpVIA = [&](const char *label, VIABase *via)
	{
		if (!via)
		{
			dbg.io().write("%s: not present\n", label);
			return;
		}
		auto &d = via->d_;
		dbg.io().write("%s:\n", label);
		dbg.io().write("  ORA=%02X  ORB=%02X  DDRA=%02X  DDRB=%02X\n",
					   d.ORA, d.ORB, d.DDR_A, d.DDR_B);
		dbg.io().write("  T1C=%08X  T1L=%02X%02X  T2C=%08X  T2L=%02X\n",
					   d.T1C_F, d.T1L_H, d.T1L_L, d.T2C_F, d.T2L_L);
		dbg.io().write("  SR=%02X  ACR=%02X  PCR=%02X  IFR=%02X  IER=%02X\n",
					   d.SR, d.ACR, d.PCR, d.IFR, d.IER);
		dbg.io().write("  T1Active=%d  T2Active=%d\n",
					   via->T1_Active, via->T2_Active);
	};

	dumpVIA("VIA1", g_machine->findDevice<VIA1Device>());
	dumpVIA("VIA2", g_machine->findDevice<VIA2Device>());
}
```

### 7.2 Wire into CmdInfo dispatch

In `CmdInfo()` (around L262–280), add before the `else` fallthrough:

```cpp
	else if (sub == "via")
		InfoVIA(dbg);
```

### 7.3 Update usage string

In `CmdInfo()` (L258), update the error/usage message:

**Before:**
```cpp
		dbg.io().write("Unknown info sub-command '%s'.\n"
					   "  Available: break, reg, trace, traps, globals, types, symbol, insn\n",
					   sub.c_str());
```

**After:**
```cpp
		dbg.io().write("Unknown info sub-command '%s'.\n"
					   "  Available: break, reg, trace, traps, globals, types, symbol, insn, via, scrap, console\n",
					   sub.c_str());
```

### 7.4 Update help text (cmd_help.cpp L73)

After:
```cpp
	dbg.io().write("  info insn       Instruction count\n");
```

Add:
```cpp
	dbg.io().write("  info via        VIA1/VIA2 register dump\n");
```

### 7.5 Update info command helpFull (debugger.cpp L113–114)

**Before:**
```cpp
	{"info", "i", CmdInfo, "Show info about debugger state",
	 "info <break|reg|traps|globals|symbol|insn>\n  Show various debugger information.\n"},
```

**After:**
```cpp
	{"info", "i", CmdInfo, "Show info about debugger state",
	 "info <break|reg|traps|globals|symbol|insn|via|scrap|console>\n  Show various debugger information.\n"},
```

### Gate — build + test

```bash
cmake --preset macos && cmake --build bld/macos
```

Quick manual test:
```bash
./bld/macos/maxivmac --debugger --headless --model=MacII --rom MacII.ROM <<< 'info via
quit'
```

Expect output containing `VIA1:`.

```bash
git add -A && git commit -m "debugger: add 'info via' command"
```

---

## Phase 8 — Debugger enhancements: `info scrap`

### 8.1 Add InfoScrap function to cmd_info.cpp

Add include (after the new via includes):
```cpp
#include "util/macroman.h"
```

Add static function:

```cpp
static void InfoScrap(Debugger &dbg)
{
	uint32_t scrapSize   = get_vm_long(0x0960);
	uint32_t scrapHandle = get_vm_long(0x0964);
	int16_t  scrapCount  = static_cast<int16_t>(get_vm_word(0x0968));
	int16_t  scrapState  = static_cast<int16_t>(get_vm_word(0x096A));

	dbg.io().write("ScrapSize=%u  ScrapHandle=$%08X  ScrapCount=%d  ScrapState=%d",
				   scrapSize, scrapHandle, scrapCount, scrapState);
	if (scrapState > 0)       dbg.io().write(" (in memory)\n");
	else if (scrapState == 0) dbg.io().write(" (on disk)\n");
	else                      dbg.io().write(" (uninitialized)\n");

	if (scrapState <= 0 || scrapHandle == 0) return;

	uint32_t masterPtr = get_vm_long(scrapHandle);
	if (masterPtr == 0)
	{
		dbg.io().write("Master pointer is NULL (purged?)\n");
		return;
	}

	uint32_t ramSz = g_machine ? g_machine->ramSize() : 0;
	uint32_t offset = 0;
	int entryIdx = 0;

	while (offset + 8 <= scrapSize)
	{
		uint32_t entryAddr = masterPtr + offset;
		if (entryAddr + 8 > ramSz) break;

		char type[5];
		for (int i = 0; i < 4; i++)
			type[i] = static_cast<char>(get_vm_byte(entryAddr + static_cast<uint32_t>(i)));
		type[4] = '\0';

		uint32_t entryLen = get_vm_long(entryAddr + 4);
		if (entryLen > scrapSize - offset - 8) break;

		uint32_t dataAddr = entryAddr + 8;
		dbg.io().write("Entry %d: '%s' %u bytes @$%08X\n",
					   entryIdx, type, entryLen, dataAddr);

		if (entryLen > 0 && memcmp(type, "TEXT", 4) == 0)
		{
			uint32_t previewLen = (entryLen < 4096) ? entryLen : 4096;
			std::vector<uint8_t> buf(previewLen);
			for (uint32_t i = 0; i < previewLen; i++)
				buf[i] = get_vm_byte(dataAddr + i);
			std::string display = UTF8FromMacRoman({buf.data(), previewLen});
			for (auto &c : display)
				if (c == '\r') c = '\n';
			if (entryLen > previewLen) display += "...";
			dbg.io().write("  %s\n", display.c_str());
		}
		else if (entryLen > 0)
		{
			uint32_t dumpLen = (entryLen < 128) ? entryLen : 128;
			for (uint32_t row = 0; row < dumpLen; row += 16)
			{
				dbg.io().write("  %04X  ", row);
				uint32_t cols = (dumpLen - row < 16) ? dumpLen - row : 16;
				for (uint32_t c = 0; c < cols; c++)
					dbg.io().write("%02X ", get_vm_byte(dataAddr + row + c));
				dbg.io().write("\n");
			}
		}

		offset += 8 + entryLen;
		if (offset & 1) offset++;
		entryIdx++;
	}
}
```

### 8.2 Wire into CmdInfo dispatch

Add after the `via` dispatch:
```cpp
	else if (sub == "scrap")
		InfoScrap(dbg);
```

### 8.3 Update help text (cmd_help.cpp)

After the `info via` line added in Phase 7:
```cpp
	dbg.io().write("  info scrap      Guest clipboard contents\n");
```

### Gate — build + test

```bash
cmake --preset macos && cmake --build bld/macos
./bld/macos/maxivmac --debugger --headless --model=MacII --rom MacII.ROM <<< 'info scrap
quit'
```

Expect output containing `ScrapState`.

```bash
git add -A && git commit -m "debugger: add 'info scrap' command"
```

---

## Phase 9 — Debugger enhancements: `info globals --section`

### 9.1 Extend InfoGlobals in cmd_info.cpp

The current `InfoGlobals` (around L160) does a prefix search.  We
extend it to also accept `--section <name>`.

Add include:
```cpp
#include "lang/global_registry.h"
```

**Replace the current `InfoGlobals` with:**

```cpp
static void InfoGlobals(Debugger &dbg, const std::vector<Token> &args)
{
	std::string_view prefix;
	std::string prefixStr;
	std::string sectionFilter;

	/* Parse args: info globals [prefix] [--section NAME] */
	for (size_t i = 1; i < args.size(); ++i)
	{
		if (args[i].kind == Token::Kind::End) break;
		if (args[i].text == "--section" && i + 1 < args.size() &&
			args[i + 1].kind != Token::Kind::End)
		{
			sectionFilter = args[i + 1].text;
			++i; /* skip the section name */
			continue;
		}
		if (args[i].kind == Token::Kind::Word && prefixStr.empty())
		{
			prefixStr = args[i].text;
			prefix = prefixStr;
		}
	}

	if (!sectionFilter.empty())
	{
		/* Section-filtered listing from GlobalRegistry */
		auto &reg = g_globalRegistry();
		dbg.io().write("%-20s  Address   Size\n", "Name");
		int count = 0;
		for (auto &gd : reg.globals())
		{
			if (gd.section != sectionFilter) continue;
			if (!prefix.empty() && !CaseInsensitiveStartsWith(gd.name, prefix))
				continue;
			dbg.io().write("%-20.*s  $%04X     %u\n",
						   static_cast<int>(gd.name.size()), gd.name.data(),
						   gd.addr, gd.size);
			if (++count >= 50) break;
		}
		dbg.io().write("(%d results, section=%s)\n", count, sectionFilter.c_str());
	}
	else
	{
		/* Original prefix-search behavior */
		std::vector<SymbolEntry> results;
		SymbolsSearch(prefix, 'g', results, 50);
		dbg.io().write("%-20s  Address   Size\n", "Name");
		for (auto &e : results)
			dbg.io().write("%-20.*s  $%04X     %u\n",
						   static_cast<int>(e.name.size()), e.name.data(),
						   e.address, e.size);
		dbg.io().write("(%zu results)\n", results.size());
	}
}
```

Note: `CaseInsensitiveStartsWith` is already defined in `symbols.cpp`
(used by `SymbolsSearch`).  If it's `static`, make it accessible
(move to `symbols.h` or duplicate a simple `strncasecmp` wrapper in
`cmd_info.cpp`).

### 9.2 Update help text (cmd_help.cpp L73)

**Before:**
```cpp
	dbg.io().write("  info globals [p] Search low-memory globals\n");
```

**After:**
```cpp
	dbg.io().write("  info globals [p] [--section S] Search low-memory globals\n");
```

### Gate — build + test

```bash
cmake --preset macos && cmake --build bld/macos
./bld/macos/maxivmac --debugger --headless --model=MacII --rom MacII.ROM <<< 'info globals --section MemoryMgr
quit'
```

Expect output containing `MemTop`.

```bash
git add -A && git commit -m "debugger: add 'info globals --section' filter"
```

---

## Phase 10 — Debugger enhancements: `info console`

### 10.1 Add InfoConsole function to cmd_info.cpp

Include is already present (`core/extn_clip.h` — L10).

Add static function:

```cpp
static void InfoConsole(Debugger &dbg, const std::vector<Token> &args)
{
	/* info console clear — clear the buffer */
	if (args.size() > 1 && args[1].kind == Token::Kind::Word &&
		args[1].text == "clear")
	{
		ExtnDbgConsoleClear();
		dbg.io().write("Console cleared.\n");
		return;
	}

	const auto &lines = extnDbgConsoleLines();
	if (lines.empty())
	{
		dbg.io().write("[guest console — empty]\n");
		return;
	}

	dbg.io().write("[guest console — %zu line(s)]\n",
				   lines.size());
	for (auto &line : lines)
		dbg.io().write("%s\n", line.c_str());
}
```

### 10.2 Wire into CmdInfo dispatch

Add after the `scrap` dispatch:
```cpp
	else if (sub == "console")
		InfoConsole(dbg, args);
```

### 10.3 Update help text (cmd_help.cpp)

After the `info scrap` line:
```cpp
	dbg.io().write("  info console    Guest debug console output\n");
```

### Gate — build + test

```bash
cmake --preset macos && cmake --build bld/macos
./bld/macos/maxivmac --debugger --headless --model=MacII --rom MacII.ROM <<< 'info console
quit'
```

Expect output containing `console`.

```bash
git add -A && git commit -m "debugger: add 'info console' command"
```

---

## Phase 11 — Debugger smoke tests

### 11.1 Add tests to test/debugger_smoke.sh

Insert before the `echo "Results:..."` line (around line 141):

```bash
# Test 22: info via
check "info via" "info via
quit" "VIA1:"

# Test 23: info scrap (may be uninitialized at boot, but command must not crash)
check "info scrap" "info scrap
quit" "ScrapState"

# Test 24: info globals --section
check "info globals --section" "info globals --section MemoryMgr
quit" "MemTop"

# Test 25: info console (buffer may be empty at boot)
check "info console" "info console
quit" "console"

# Test 26: help mentions info via
check "help mentions via" "help
quit" "info via"

# Test 27: help mentions info scrap
check "help mentions scrap" "help
quit" "info scrap"

# Test 28: help mentions info console
check "help mentions console" "help
quit" "info console"
```

### Gate — run tests

```bash
bash test/debugger_smoke.sh
```

All 28 tests must pass (21 existing + 7 new).

```bash
git add -A && git commit -m "debugger: add smoke tests for info via/scrap/console/globals --section"
```

---

## Phase 12 — Final verification

```bash
# 1. Full clean build
rm -rf bld/macos && cmake --preset macos && cmake --build bld/macos

# 2. All debugger smoke tests
bash test/debugger_smoke.sh

# 3. Selftest (emulation regression)
./selftest.sh

# 4. Manual check: launch GUI
./bld/macos/maxivmac --model=MacII disk.hfs
#    → Ctrl overlay → no Developer Mode button
#    → Fullscreen works
#    → Model selector works (relaunch without --model)

# 5. Manual check: new debugger commands with booted guest
./bld/macos/maxivmac --debugger --model=MacII disk.hfs
#    (dbg) info via           → shows VIA1/VIA2 registers
#    (dbg) info scrap         → shows scrap state
#    (dbg) info globals --section FileMgr → shows FCBSPtr etc.
#    (dbg) info console       → shows console buffer
#    (dbg) quit
```

No commit — verification only.

---

## Commit Summary

| Commit | Phase | Message |
|--------|-------|---------|
| 1 | 1–3 | `devmode: remove developer mode UI, tool framework, and debug windows` |
| 2 | 4 | `devmode: remove developer mode button from control overlay` |
| 3 | 5 | `devmode: documentation sweep — remove developer mode references` |
| 4 | 7 | `debugger: add 'info via' command` |
| 5 | 8 | `debugger: add 'info scrap' command` |
| 6 | 9 | `debugger: add 'info globals --section' filter` |
| 7 | 10 | `debugger: add 'info console' command` |
| 8 | 11 | `debugger: add smoke tests for info via/scrap/console/globals --section` |

Total: 8 commits, each with a build+test gate.
