/*
	imgui_debug_windows.cpp — Debug tool panel implementations
*/

#include "platform/imgui_debug_windows.h"
#include "platform/imgui_lomem_tool.h"
#include "platform/imgui_tool_registry.h"

#include <imgui.h>

#include "core/machine.h"
#include "core/machine_obj.h"
#include "cpu/m68k.h"
#include "cpu/disasm.h"
#include "devices/via.h"
#include "devices/via2.h"
#include "cpu/trap_counter.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>

#include "util/macroman.h"

/* ── RegistersTool ─────────────────────────────────── */

void RegistersTool::draw()
{
	if (!ImGui::Begin(name(), &visible))
	{
		ImGui::End();
		return;
	}

	uint32_t d[8], a[8];
	m68k_getRegs(d, a);
	uint32_t pc = m68k_getPC_public();
	uint16_t sr = m68k_getSR_public();

	ImGui::Text("PC  %08X", pc);
	ImGui::Text("SR  %04X", sr);
	ImGui::Separator();

	ImGui::Text("Flags: %c%c%c%c%c  IPL=%d  %c%c", (sr & 0x10) ? 'X' : '-', (sr & 0x08) ? 'N' : '-',
				(sr & 0x04) ? 'Z' : '-', (sr & 0x02) ? 'V' : '-', (sr & 0x01) ? 'C' : '-',
				(sr >> 8) & 7, (sr & 0x2000) ? 'S' : 'U', (sr & 0x8000) ? 'T' : '-');
	ImGui::Separator();

	if (ImGui::BeginTable("regs", 2))
	{
		ImGui::TableSetupColumn("Data");
		ImGui::TableSetupColumn("Address");
		ImGui::TableHeadersRow();
		for (int i = 0; i < 8; ++i)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("D%d  %08X", i, d[i]);
			ImGui::TableNextColumn();
			ImGui::Text("A%d  %08X", i, a[i]);
		}
		ImGui::EndTable();
	}

	ImGui::Separator();
	ImGui::Text("USP %08X", m68k_getUSP());
	ImGui::Text("ISP %08X", m68k_getISP());
	ImGui::Text("MSP %08X", m68k_getMSP());
	ImGui::Text("VBR %08X", m68k_getVBR());

	ImGui::End();
}

/* ── DisassemblyTool ───────────────────────────────── */

void DisassemblyTool::draw()
{
	if (!ImGui::Begin(name(), &visible))
	{
		ImGui::End();
		return;
	}

	static uint32_t disasmAddr = 0;
	static bool followPC = true;

	ImGui::Checkbox("Follow PC", &followPC);
	ImGui::SameLine();
	if (!followPC)
	{
		ImGui::SetNextItemWidth(100);
		ImGui::InputScalar("Address", ImGuiDataType_U32, &disasmAddr, nullptr, nullptr, "%08X",
						   ImGuiInputTextFlags_CharsHexadecimal);
	}

	uint32_t pc = m68k_getPC_public();
	uint32_t addr = followPC ? pc : disasmAddr;

	if (ImGui::BeginChild("disasm_listing", ImVec2(0, 0), ImGuiChildFlags_None,
						  ImGuiWindowFlags_HorizontalScrollbar))
	{
		for (int i = 0; i < 32; ++i)
		{
			uint32_t lineAddr = addr;
			std::string text = Disassemble(addr);

			bool isCurrent = (lineAddr == pc);
			if (isCurrent) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.3f, 1.0f));

			ImGui::Text("%s%08X  %s", isCurrent ? ">" : " ", lineAddr, text.c_str());

			if (isCurrent) ImGui::PopStyleColor();
		}
	}
	ImGui::EndChild();

	ImGui::End();
}

/* ── MemoryTool ────────────────────────────────────── */

void MemoryTool::draw()
{
	if (!ImGui::Begin(name(), &visible))
	{
		ImGui::End();
		return;
	}

	static uint32_t memAddr = 0;
	static int bytesPerRow = 16;

	ImGui::SetNextItemWidth(100);
	ImGui::InputScalar("Address", ImGuiDataType_U32, &memAddr, nullptr, nullptr, "%08X",
					   ImGuiInputTextFlags_CharsHexadecimal);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(60);
	if (ImGui::BeginCombo("##bpr", std::to_string(bytesPerRow).c_str()))
	{
		for (int n : {8, 16, 32})
		{
			if (ImGui::Selectable(std::to_string(n).c_str(), bytesPerRow == n)) bytesPerRow = n;
		}
		ImGui::EndCombo();
	}

	uint32_t ramSz = g_machine ? g_machine->ramSize() : 0;
	uint8_t *ram = g_ram;

	if (ImGui::BeginChild("mem_hex", ImVec2(0, 0), ImGuiChildFlags_None,
						  ImGuiWindowFlags_HorizontalScrollbar))
	{
		int rows = 32;
		for (int r = 0; r < rows; ++r)
		{
			uint32_t rowAddr = memAddr + (uint32_t)(r * bytesPerRow);
			char line[256];
			int pos = snprintf(line, sizeof(line), "%08X  ", rowAddr);

			char ascii[64];
			int apos = 0;
			for (int b = 0; b < bytesPerRow; ++b)
			{
				uint32_t a = rowAddr + (uint32_t)b;
				if (ram && a < ramSz)
				{
					uint8_t v = ram[a];
					pos += snprintf(line + pos, sizeof(line) - (size_t)pos, "%02X ", v);
					ascii[apos++] = (v >= 0x20 && v < 0x7F) ? (char)v : '.';
				}
				else
				{
					pos += snprintf(line + pos, sizeof(line) - (size_t)pos, "?? ");
					ascii[apos++] = '?';
				}
			}
			ascii[apos] = '\0';
			ImGui::Text("%s %s", line, ascii);
		}
	}
	ImGui::EndChild();

	ImGui::End();
}

/* ── VIATool ───────────────────────────────────────── */

void VIATool::draw()
{
	if (!g_machine) return;
	if (!ImGui::Begin(name(), &visible))
	{
		ImGui::End();
		return;
	}

	auto drawVIA = [](const char *label, VIABase *via)
	{
		if (!via) return;
		if (!ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) return;

		auto &d = via->d_;
		ImGui::Text("ORA  %02X  ORB  %02X", d.ORA, d.ORB);
		ImGui::Text("DDRA %02X  DDRB %02X", d.DDR_A, d.DDR_B);
		ImGui::Text("T1C  %08X  T1L  %02X%02X", d.T1C_F, d.T1L_H, d.T1L_L);
		ImGui::Text("T2C  %08X  T2L  %02X", d.T2C_F, d.T2L_L);
		ImGui::Text("SR   %02X  ACR  %02X  PCR  %02X", d.SR, d.ACR, d.PCR);
		ImGui::Text("IFR  %02X  IER  %02X", d.IFR, d.IER);
		ImGui::Text("T1Active %d  T2Active %d", via->T1_Active, via->T2_Active);
	};

	drawVIA("VIA1", g_machine->findDevice<VIA1Device>());
	drawVIA("VIA2", g_machine->findDevice<VIA2Device>());

	ImGui::End();
}

/* ── TrapsTool ─────────────────────────────────────── */

void TrapsTool::draw()
{
	if (!ImGui::Begin(name(), &visible))
	{
		ImGui::End();
		return;
	}

	/* Lazy-init: load defaults on first open */
	static bool inited = false;
	if (!inited)
	{
		trap_watch_load_defaults();
		inited = true;
	}

	/* ── Add trap control ─────────────────────────── */
	static char searchBuf[64] = "";
	static std::vector<TrapInfo> suggestions;
	static int selectedSuggestion = -1;

	ImGui::Text("Add trap:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(200);
	bool inputChanged = ImGui::InputText("##trap_search", searchBuf, sizeof(searchBuf));
	if (inputChanged)
	{
		trap_dict_search(searchBuf, suggestions, 10);
		selectedSuggestion = -1;
	}

	if (searchBuf[0] && !suggestions.empty())
	{
		ImGui::SetNextWindowSizeConstraints(ImVec2(200, 0), ImVec2(400, 200));
		if (ImGui::BeginChild("##suggestions", ImVec2(400, 0),
							  ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border))
		{
			for (int i = 0; i < (int)suggestions.size(); ++i)
			{
				char label[80];
				snprintf(label, sizeof(label), "%s ($%04X)", suggestions[i].name,
						 suggestions[i].trapWord);
				if (ImGui::Selectable(label, i == selectedSuggestion))
				{
					trap_watch_add(suggestions[i].trapWord);
					searchBuf[0] = '\0';
					suggestions.clear();
					selectedSuggestion = -1;
				}
			}
		}
		ImGui::EndChild();
	}

	ImGui::SameLine();
	if (ImGui::Button("Reset Counts"))
	{
		trap_counter_reset();
	}
	ImGui::SameLine();
	if (ImGui::Button("Defaults"))
	{
		trap_watch_load_defaults();
	}

	ImGui::Separator();

	/* ── Watchlist table ──────────────────────────── */
	auto entries = trap_watch_snapshot();

	if (ImGui::BeginTable("traps", 5,
						  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
							  ImGuiTableFlags_Sortable | ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn(
			"##del", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed, 24.0f, 0);
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort, 0.0f, 1);
		ImGui::TableSetupColumn("Trap", ImGuiTableColumnFlags_None, 0.0f, 2);
		ImGui::TableSetupColumn("Handler", ImGuiTableColumnFlags_None, 0.0f, 3);
		ImGui::TableSetupColumn(
			"Count", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending,
			0.0f, 4);
		ImGui::TableHeadersRow();

		/* Sort if user clicks a column header */
		if (ImGuiTableSortSpecs *specs = ImGui::TableGetSortSpecs())
		{
			if (specs->SpecsCount > 0)
			{
				auto &s = specs->Specs[0];
				bool asc = (s.SortDirection == ImGuiSortDirection_Ascending);
				std::sort(entries.begin(), entries.end(),
						  [&s, asc](const WatchEntry &a, const WatchEntry &b)
						  {
							  int cmp = 0;
							  switch (s.ColumnUserID)
							  {
								  case 1:
									  cmp = strcmp(a.name, b.name);
									  break;
								  case 2:
									  cmp = (int)a.trapWord - (int)b.trapWord;
									  break;
								  case 3:
									  cmp = (int)a.trapWord - (int)b.trapWord;
									  break;
								  case 4:
									  cmp = (a.count < b.count) ? -1 : (a.count > b.count) ? 1 : 0;
									  break;
							  }
							  return asc ? (cmp < 0) : (cmp > 0);
						  });
			}
		}

		uint16_t toRemove = 0;
		for (int i = 0; i < (int)entries.size(); ++i)
		{
			ImGui::TableNextRow();

			/* Delete button */
			ImGui::TableNextColumn();
			ImGui::PushID(i);
			if (ImGui::SmallButton("X"))
			{
				toRemove = entries[i].trapWord;
			}
			ImGui::PopID();

			/* Name */
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(entries[i].name);

			/* Trap word */
			ImGui::TableNextColumn();
			ImGui::Text("$%04X", entries[i].trapWord);

			/* Handler address */
			ImGui::TableNextColumn();
			uint16_t w = entries[i].trapWord;
			uint32_t slot;
			if (w & 0x0800)
				slot = 0x0E00 + 4 * (w & 0x03FF);
			else
				slot = 0x0400 + 4 * (w & 0x00FF);
			uint32_t handler = get_vm_long(slot);
			ImGui::Text("$%08X", handler);

			/* Count */
			ImGui::TableNextColumn();
			if (entries[i].count > 0)
				ImGui::Text("%u", entries[i].count);
			else
				ImGui::TextDisabled("0");
		}
		ImGui::EndTable();

		if (toRemove) trap_watch_remove(toRemove);
	}

	ImGui::End();
}

/* ── ScrapTool ─────────────────────────────────────── */

/*
	Low-memory scrap globals:
	  $0960 ScrapSize   (long)  — byte count of scrap data
	  $0964 ScrapHandle (long)  — handle to scrap in memory
	  $0968 ScrapCount  (word)  — bumped by ZeroScrap
	  $096A ScrapState  (word)  — >0 in memory, 0 on disk, <0 uninit

	Scrap data format (sequential entries):
	  4 bytes  type   (e.g. 'TEXT', 'PICT')
	  4 bytes  length  of following data
	  n bytes  data   (padded to even)
*/

static std::string macRomanToDisplay(const uint8_t *data, uint32_t len, uint32_t maxChars)
{
	uint32_t n = (len < maxChars) ? len : maxChars;
	std::string out = UTF8FromMacRoman({data, n});
	/* CR (0x0D) -> newline for display */
	for (auto &c : out)
	{
		if (c == '\r') c = '\n';
	}
	if (len > maxChars)
	{
		out += "...";
	}
	return out;
}

void ScrapTool::draw()
{
	if (!ImGui::Begin(name(), &visible))
	{
		ImGui::End();
		return;
	}

	/* Read low-memory globals */
	uint32_t scrapSize = get_vm_long(0x0960);
	uint32_t scrapHandle = get_vm_long(0x0964);
	int16_t scrapCount = (int16_t)get_vm_word(0x0968);
	int16_t scrapState = (int16_t)get_vm_word(0x096A);

	ImGui::Text("ScrapSize   %u ($%X)", scrapSize, scrapSize);
	ImGui::Text("ScrapHandle $%08X", scrapHandle);
	ImGui::Text("ScrapCount  %d", scrapCount);
	ImGui::Text("ScrapState  %d  %s", scrapState,
				scrapState > 0	  ? "(in memory)"
				: scrapState == 0 ? "(on disk)"
								  : "(uninitialized)");
	ImGui::Separator();

	if (scrapState < 0)
	{
		ImGui::TextDisabled("Scrap not initialized");
		ImGui::End();
		return;
	}

	if (scrapState == 0)
	{
		ImGui::TextDisabled("Scrap on disk — not readable from RAM");
		ImGui::End();
		return;
	}

	if (scrapHandle == 0)
	{
		ImGui::TextDisabled("ScrapHandle is NULL");
		ImGui::End();
		return;
	}

	/* Dereference handle: handle -> master pointer -> data */
	uint32_t masterPtr = get_vm_long(scrapHandle);
	if (masterPtr == 0)
	{
		ImGui::TextDisabled("Master pointer is NULL (purged?)");
		ImGui::End();
		return;
	}

	ImGui::Text("Scrap data at $%08X", masterPtr);
	ImGui::Separator();

	/* Walk scrap entries */
	uint32_t offset = 0;
	int entryIdx = 0;
	uint32_t ramSz = g_machine ? g_machine->ramSize() : 0;

	while (offset + 8 <= scrapSize)
	{
		uint32_t entryAddr = masterPtr + offset;

		/* Bounds check */
		if (entryAddr + 8 > ramSz)
		{
			ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1),
							   "Entry %d: address $%08X out of RAM bounds", entryIdx, entryAddr);
			break;
		}

		/* Read type (4 chars) and length */
		char type[5];
		for (int i = 0; i < 4; i++)
			type[i] = (char)get_vm_byte(entryAddr + (uint32_t)i);
		type[4] = '\0';

		uint32_t entryLen = get_vm_long(entryAddr + 4);

		/* Sanity check */
		if (entryLen > scrapSize - offset - 8)
		{
			ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1),
							   "Entry %d '%s': length %u exceeds remaining scrap (%u)", entryIdx,
							   type, entryLen, scrapSize - offset - 8);
			break;
		}

		uint32_t dataAddr = entryAddr + 8;

		char header[64];
		snprintf(header, sizeof(header), "Entry %d: '%s'  %u bytes  @$%08X", entryIdx, type,
				 entryLen, dataAddr);

		if (ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (entryLen == 0)
			{
				ImGui::TextDisabled("  (empty)");
			}
			else if (memcmp(type, "TEXT", 4) == 0)
			{
				/* Decode TEXT — read bytes from guest, convert MacRoman */
				uint32_t previewLen = (entryLen < 4096) ? entryLen : 4096;
				std::vector<uint8_t> buf(previewLen);
				for (uint32_t i = 0; i < previewLen; i++)
					buf[i] = get_vm_byte(dataAddr + i);

				std::string display = macRomanToDisplay(buf.data(), entryLen, previewLen);

				ImGui::Text("  Length: %u chars", entryLen);
				ImGui::Indent();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 1.0f, 0.5f, 1.0f));
				ImGui::TextWrapped("%s", display.c_str());
				ImGui::PopStyleColor();
				ImGui::Unindent();
			}
			else
			{
				/* Non-TEXT: hex dump first 128 bytes */
				uint32_t dumpLen = (entryLen < 128) ? entryLen : 128;
				ImGui::Text("  Hex (%u of %u bytes):", dumpLen, entryLen);
				ImGui::Indent();

				for (uint32_t row = 0; row < dumpLen; row += 16)
				{
					char line[80];
					int pos = snprintf(line, sizeof(line), "  %04X  ", row);
					char ascii[17];
					uint32_t cols = (dumpLen - row < 16) ? dumpLen - row : 16;

					for (uint32_t c = 0; c < cols; c++)
					{
						uint8_t v = get_vm_byte(dataAddr + row + c);
						pos += snprintf(line + pos, sizeof(line) - (size_t)pos, "%02X ", v);
						ascii[c] = (v >= 0x20 && v < 0x7F) ? (char)v : '.';
					}
					ascii[cols] = '\0';
					/* Pad if short row */
					for (uint32_t c = cols; c < 16; c++)
						pos += snprintf(line + pos, sizeof(line) - (size_t)pos, "   ");
					ImGui::Text("%s %s", line, ascii);
				}

				ImGui::Unindent();
			}
		}

		/* Advance: 8 byte header + data padded to even */
		offset += 8 + entryLen;
		if (offset & 1) offset++;
		entryIdx++;
	}

	if (entryIdx == 0 && scrapSize > 0)
	{
		ImGui::TextDisabled("No scrap entries found (size=%u)", scrapSize);
	}

	ImGui::End();
}

/* ── ConsoleTool ───────────────────────────────────── */

#include "core/extn_clip.h"

void ConsoleTool::draw()
{
	if (!ImGui::Begin(name(), &visible))
	{
		ImGui::End();
		return;
	}

	ImGui::Checkbox("Auto-scroll", &autoScroll);
	ImGui::SameLine();
	if (ImGui::Button("Clear"))
	{
		ExtnDbgConsoleClear();
	}
	ImGui::Separator();

	ImGui::BeginChild("ConsoleScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

	const auto &lines = extnDbgConsoleLines();
	ImGuiListClipper clipper;
	clipper.Begin(static_cast<int>(lines.size()));
	while (clipper.Step())
	{
		for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
		{
			ImGui::TextUnformatted(lines[i].c_str());
		}
	}

	if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);

	ImGui::EndChild();
	ImGui::End();
}

/* ── Registration helper ───────────────────────────── */

void RegisterDebugTools(ToolRegistry &registry)
{
	registry.registerTool(std::make_unique<RegistersTool>());
	registry.registerTool(std::make_unique<DisassemblyTool>());
	registry.registerTool(std::make_unique<MemoryTool>());
	registry.registerTool(std::make_unique<VIATool>());
	registry.registerTool(std::make_unique<TrapsTool>());
	registry.registerTool(std::make_unique<ScrapTool>());
	registry.registerTool(std::make_unique<ConsoleTool>());
	registry.registerTool(std::make_unique<LowMemTool>());
}
