# DEVREMOVE_PLAN.md — Remove Developer Mode (Implementation Plan)

Reference design: [docs/DEVREMOVE.md](DEVREMOVE.md)

---

## Phase 1 — Delete tool framework files + CMake cleanup

### 1.1 Delete files

```
rm src/platform/imgui_tool.h
rm src/platform/imgui_tool_registry.h
rm src/platform/imgui_tool_registry.cpp
rm src/platform/imgui_debug_windows.h
rm src/platform/imgui_debug_windows.cpp
rm src/platform/imgui_lomem_tool.h
rm src/platform/imgui_lomem_tool.cpp
```

### 1.2 Remove from CMakeLists.txt

File: `CMakeLists.txt`, lines 135, 138–139.

Delete these three lines from the `IMGUI_SOURCES` list:

```
    src/platform/imgui_debug_windows.cpp      # line 135
    src/platform/imgui_tool_registry.cpp       # line 138
    src/platform/imgui_lomem_tool.cpp          # line 139
```

### Gate

Build will **not** compile yet — expected.  Commit is deferred to end of Phase 3.

---

## Phase 2 — Strip Developer mode from ImGuiBackend

All edits in this phase are to `src/platform/imgui_backend.h` and
`src/platform/imgui_backend.cpp`.

### 2.1 UIState enum (imgui_backend.h L21–28)

**Before:**
```cpp
enum class UIState
{
	ModelSelector,
	Windowed,
	Fullscreen,
	Developer,
};
```

**After:**
```cpp
enum class UIState
{
	ModelSelector,
	Windowed,
	Fullscreen,
};
```

### 2.2 Remove tool-registry include (imgui_backend.h L15)

Delete:
```cpp
#include "platform/imgui_tool_registry.h"
```

### 2.3 Remove enterDeveloper declaration (imgui_backend.h L83)

Delete:
```cpp
	void enterDeveloper();
```

### 2.4 Remove getToolRegistry accessor (imgui_backend.h L88–89)

Delete:
```cpp
	/* Tool registry for developer mode */
	ToolRegistry &getToolRegistry() { return toolRegistry_; }
```

### 2.5 Update savedWin comment (imgui_backend.h L117)

**Before:**
```cpp
	/* Saved window geometry for returning from fullscreen/developer */
```

**After:**
```cpp
	/* Saved window geometry for returning from fullscreen */
```

### 2.6 Remove drawMenuBar & developer draw declarations (imgui_backend.h)

Delete these declarations:

```cpp
	void drawMenuBar();                  // L125
	void drawViewportDeveloper();        // L128
```

### 2.7 Remove toolRegistry_ member (imgui_backend.h L136–137)

Delete:
```cpp
	/* Tool registry for developer mode */
	ToolRegistry toolRegistry_;
```

### 2.8 Remove drawDeveloperState declaration (imgui_backend.h L142)

Delete:
```cpp
	void drawDeveloperState();
```

### 2.9 Remove debug-windows include (imgui_backend.cpp L15)

Delete:
```cpp
#include "platform/imgui_debug_windows.h"
```

### 2.10 Remove Developer from runLoop switch (imgui_backend.cpp L191)

**Before:**
```cpp
			case UIState::Windowed:
			case UIState::Fullscreen:
			case UIState::Developer:
				drawWindowedState();
				break;
```

**After:**
```cpp
			case UIState::Windowed:
			case UIState::Fullscreen:
				drawWindowedState();
				break;
```

### 2.11 Remove menu-bar guard in drawWindowedState (imgui_backend.cpp L239–240)

Delete these two lines:
```cpp
	/* Only show menu bar in Developer state */
	if (uiState_ == UIState::Developer) drawMenuBar();
```

### 2.12 Remove Developer case from overlay state-switch (imgui_backend.cpp)

In `drawWindowedState()`, the overlay state-change switch (around L253–264).

**Before:**
```cpp
			switch (requested)
			{
				case UIState::Windowed:
					enterWindowed();
					break;
				case UIState::Fullscreen:
					enterFullscreen();
					break;
				case UIState::Developer:
					enterDeveloper();
					break;
				default:
					break;
			}
```

**After:**
```cpp
			switch (requested)
			{
				case UIState::Windowed:
					enterWindowed();
					break;
				case UIState::Fullscreen:
					enterFullscreen();
					break;
				default:
					break;
			}
```

### 2.13 Remove toolRegistry draw call (imgui_backend.cpp L269–270)

Delete:
```cpp
	/* Debug tools only in Developer mode */
	if (uiState_ == UIState::Developer) toolRegistry_.drawAllVisible();
```

### 2.14 Delete drawDeveloperState method (imgui_backend.cpp L293–299)

Delete the entire method:
```cpp
void ImGuiBackend::drawDeveloperState()
{
	...
}
```

### 2.15 Delete RegisterDebugTools call in bootFromSelector (imgui_backend.cpp L344)

Delete:
```cpp
	/* Register debug tools now that the machine is initialized */
	RegisterDebugTools(toolRegistry_);
```

### 2.16 Delete enterDeveloper method (imgui_backend.cpp L378–397)

Delete the entire method:
```cpp
void ImGuiBackend::enterDeveloper()
{
	...
}
```

### 2.17 Update imGuiConsumedEvent comment (imgui_backend.cpp L459–463)

**Before:**
```cpp
			/* Forward to the emulator only when the emulator viewport
			   is the topmost hovered window (from the previous frame).
			   In Windowed/Fullscreen the viewport fills the window so
			   this is ~always true; in Developer mode, debug tool
			   windows on top correctly block the hover. */
```

**After:**
```cpp
			/* Forward to the emulator only when the emulator viewport
			   is hovered (from the previous frame).  In Windowed the
			   viewport fills the window, so this is always true.
			   The overlay is the only UI that can appear on top. */
```

### 2.18 Delete drawMenuBar method (imgui_backend.cpp L634–653)

Delete the entire method:
```cpp
void ImGuiBackend::drawMenuBar()
{
	...
}
```

### 2.19 Delete drawViewportDeveloper method (imgui_backend.cpp L734–744)

Delete the entire method:
```cpp
void ImGuiBackend::drawViewportDeveloper()
{
	...
}
```

### 2.20 Remove Developer case from drawEmulatorViewport (imgui_backend.cpp L755–759)

**Before:**
```cpp
	switch (uiState_)
	{
		case UIState::Fullscreen:
			drawViewportFullscreen();
			break;
		case UIState::Developer:
			drawViewportDeveloper();
			break;
		default:
			drawViewportWindowed();
			break;
	}
```

**After:**
```cpp
	switch (uiState_)
	{
		case UIState::Fullscreen:
			drawViewportFullscreen();
			break;
		default:
			drawViewportWindowed();
			break;
	}
```

### 2.21 Remove Developer sizing in createWindow (imgui_backend.cpp L770–781)

**Before:**
```cpp
	/* In Windowed mode the window should be exactly the emulator's
	   screen size so it feels like the original Mac.  In Developer
	   mode we add extra space for debug panels. */
	int winW, winH;
	if (uiState_ == UIState::Developer)
	{
		winW = width + 200;
		winH = height + 200;
	}
	else
	{
		winW = width;
		winH = height;
	}
```

**After:**
```cpp
	int winW = width;
	int winH = height;
```

### 2.22 Update onResolutionChanged comment (imgui_backend.cpp L996–998)

**Before:**
```cpp
	/* Resize SDL window only in Windowed mode.  Fullscreen and
	   Developer modes handle the new resolution automatically
	   via aspect-ratio scaling / ImGui auto-resize. */
```

**After:**
```cpp
	/* Resize SDL window only in Windowed mode.  Fullscreen handles
	   the new resolution automatically via aspect-ratio scaling. */
```

### Gate

Build: `cmake --preset macos && cmake --build bld/macos`
— expect errors from `app_main.cpp` / `imgui_main.cpp` (fixed in Phase 3).

---

## Phase 3 — Strip Developer from entry points

### 3.1 app_main.cpp (L10, L47)

Delete the include:
```cpp
#include "platform/imgui_debug_windows.h"    // L10
```

Delete the `RegisterDebugTools` call:
```cpp
		RegisterDebugTools(imguiBackend.getToolRegistry());   // L47
```

### 3.2 imgui_main.cpp (L11, L32)

Delete the include:
```cpp
#include "platform/imgui_debug_windows.h"    // L11
```

Delete the `RegisterDebugTools` call:
```cpp
		RegisterDebugTools(backend.getToolRegistry());   // L32
```

### Gate — build + test

```bash
cmake --preset macos && cmake --build bld/macos
./selftest.sh                      # emulation regression check
```

Both must pass.

```bash
git add -A && git commit -m "devmode: remove developer mode UI, tool framework, and debug windows"
```

---

## Phase 4 — Clean overlay

### 4.1 Simplify drawAdvancedTab (imgui_overlay.cpp L240–257)

The `drawAdvancedTab` method currently toggles developer mode.  After
removal it only shows the About block.

**Before (L240–257):**
```cpp
void ControlOverlay::drawAdvancedTab(UIState currentState, UIState &requestedState)
{
	ImGui::Spacing();

	bool isDeveloper = (currentState == UIState::Developer);
	if (ImGui::Button(isDeveloper ? "Exit Developer Mode" : "Developer Mode", ImVec2(200, 36)))
	{
		requestedState = isDeveloper ? UIState::Windowed : UIState::Developer;
	}
	if (!isDeveloper)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("Debug tools");
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::Text("maxivmac");
	ImGui::TextDisabled("A modernisation of the Mini vMac emulator");
	ImGui::TextDisabled("github.com/InvisibleUp/minivmac");
	ImGui::TextDisabled("Copyright 2001-2024 Paul C. Pratt et al.");
	ImGui::TextDisabled("Licensed under GPL v2");
}
```

**After:**
```cpp
void ControlOverlay::drawAdvancedTab()
{
	ImGui::Spacing();

	ImGui::Text("maxivmac");
	ImGui::TextDisabled("A modernisation of the Mini vMac emulator");
	ImGui::TextDisabled("github.com/InvisibleUp/minivmac");
	ImGui::TextDisabled("Copyright 2001-2024 Paul C. Pratt et al.");
	ImGui::TextDisabled("Licensed under GPL v2");
}
```

### 4.2 Update call site in draw() (imgui_overlay.cpp L81–84)

**Before:**
```cpp
			if (ImGui::BeginTabItem("Advanced"))
			{
				drawAdvancedTab(currentState, requestedState);
				if (requestedState != currentState) stateChanged = true;
				ImGui::EndTabItem();
			}
```

**After:**
```cpp
			if (ImGui::BeginTabItem("Advanced"))
			{
				drawAdvancedTab();
				ImGui::EndTabItem();
			}
```

### 4.3 Update imgui_overlay.h declaration (L27)

**Before:**
```cpp
	void drawAdvancedTab(UIState currentState, UIState &requestedState);
```

**After:**
```cpp
	void drawAdvancedTab();
```

### Gate — build + test

```bash
cmake --preset macos && cmake --build bld/macos
```

Verify: Ctrl overlay → Advanced tab shows only the About block, no
"Developer Mode" button.

```bash
git add -A && git commit -m "devmode: remove developer mode button from control overlay"
```

---

## Phase 5 — Documentation sweep

### 5.1 docs/TODO.md

Delete line 1:
```
* Remove Developer Mode UI completely
```

### 5.2 docs/BUGS.md

Delete lines 1–2:
```
* [DONE] in developer mode: Clear console does not work
* [DONE] in developer mode: Events are sent to the emulator even if another window is on top, making it impossible to work on top of the guest
```

### 5.3 docs/done/UI.md

Add at the very top (line 1), before the existing `# Plan:` heading:

```markdown
> **Note (2026-04):** Developer mode was removed. See [DEVREMOVE.md](../DEVREMOVE.md).
> This document is kept as historical reference.

```

### 5.4 docs/done/UI_PLAN.md

Add at the top (line 1):

```markdown
> **Note (2026-04):** Developer mode was removed. See [DEVREMOVE.md](../DEVREMOVE.md).
> Phase 5 (Developer Mode + DockSpace) and Phase 6 (Tool Framework) are no longer applicable.

```

### 5.5 docs/BUILDING.md (L28)

**Before:**
```
The binary is at `bld/macos/maxivmac`. The ImGui backend provides a graphical UI with debug tools. Place a Mac ROM file in the working directory and pass a System disk image on the command line to boot.
```

**After:**
```
The binary is at `bld/macos/maxivmac`. The ImGui backend provides a graphical UI with a model selector and control overlay. Use `--debugger` for the interactive command-line debugger. Place a Mac ROM file in the working directory and pass a System disk image on the command line to boot.
```

### 5.6 docs/specs/MOUSE.md — remove Developer section

Delete the "Developer" subsection (lines 67–80 approximately):

```markdown
### Developer

The guest image is displayed at native resolution inside a resizable
ImGui window.  Other ImGui windows (debug tools, menus) may overlap
the guest viewport.

Cursor visibility depends on what is topmost under the pointer:

- **Guest viewport topmost** — the host cursor is hidden and the
  guest receives both position and click events, exactly as in
  windowed mode.
- **ImGui window on top of the guest** — the host cursor is shown
  (the user is interacting with host UI).  The guest still receives
  position updates so its cursor tracks the host pointer, but clicks
```

(Delete everything from `### Developer` up to the next section or EOF.)

Also update the intro mention at line 30:
```
   borders in fullscreen, or at a fixed origin in developer mode).
```
→
```
   borders in fullscreen).
```

### 5.7 docs/PLATFORM_ARCH.md (L86)

Replace:
```
│                → debug windows → Render → Present      │
```
With:
```
│                → overlay → Render → Present             │
```

Also update L46:
```
2. Issue all draw commands (menus, debug windows, emulator viewport)
```
→
```
2. Issue all draw commands (overlay, emulator viewport)
```

And L53:
```
SDL renders only when the emulated screen has changed (dirty-rect push). ImGui must render **every host frame** — menus, hover states, and debug windows update continuously even when the emulated screen is static.
```
→
```
SDL renders only when the emulated screen has changed (dirty-rect push). ImGui must render **every host frame** — the overlay and model selector update continuously even when the emulated screen is static.
```

### 5.8 docs/UI_OVERLAY_PLAN.md (L25, L45)

L25 — delete or rephrase:
```
- Copy Options — developer-only; build system documents this
```

L45 — rephrase:
```
| **Abnormal trap** | `osglu_common.cpp` — emulation anomaly | Info | Developer-oriented, low priority |
```
→
```
| **Abnormal trap** | `osglu_common.cpp` — emulation anomaly | Info | Low priority |
```

### 5.9 docs/MACROMAN_UNIFY_DESIGN.md (L33, L202)

At L33 add `[removed]` note:
```
    imgui_debug_windows.cpp ← rewritten to use util/macroman.h
```
→
```
    imgui_debug_windows.cpp ← [removed — developer mode deleted]
```

At L202 add note:
```
### 3.4 — imgui_debug_windows.cpp (lines 404, 406)
```
→
```
### 3.4 — imgui_debug_windows.cpp [removed — developer mode deleted]
```

Same for `imgui_lomem_tool.cpp` at L32:
```
    imgui_lomem_tool.cpp ← rewritten to use util/macroman.h
```
→
```
    imgui_lomem_tool.cpp ← [removed — developer mode deleted]
```

### 5.10 docs/features/LOW_MEMORY_DESIGN.md

Add at line 1:
```markdown
> **Note (2026-04):** The ImGui LowMemTool was removed with developer mode.
> The `info globals` debugger command provides equivalent functionality.
> This document is kept as historical reference.

```

### 5.11 docs/features/LOW_MEMORY_PLAN.md

Add at line 1:
```markdown
> **Note (2026-04):** The ImGui LowMemTool was removed with developer mode.
> This document is kept as historical reference.

```

### Gate

```bash
git add -A && git commit -m "devmode: documentation sweep — remove developer mode references"
```

---

## Phase 6 — Verify removal

Manual and automated checks for the code removed in Phases 1–5.

```bash
# 1. Clean build
cmake --preset macos && cmake --build bld/macos

# 2. Launch model selector (no --model)
./bld/macos/maxivmac
#    → verify model selector appears, pick a model, boot works

# 3. Launch with --model
./bld/macos/maxivmac --model=MacII disk.hfs
#    → verify boots into Windowed

# 4. Ctrl overlay
#    → verify no "Developer Mode" button in Advanced tab
#    → verify Fullscreen toggle works
#    → verify speed controls work

# 5. Selftest
./selftest.sh

# 6. Debugger
./bld/macos/maxivmac --debugger --headless --model=MacII --rom MacII.ROM <<< 'info reg
quit'
#    → verify debugger still works

# 7. Debugger smoke tests
bash test/debugger_smoke.sh
```

No commit — this is verification only.

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
