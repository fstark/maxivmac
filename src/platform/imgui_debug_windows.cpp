/*
	imgui_debug_windows.cpp — ImGui debug panels

	Register viewer, disassembly listing, memory hex viewer,
	and VIA state inspector.
*/

#include "platform/imgui_debug_windows.h"

#include <imgui.h>

#include "core/machine.h"
#include "core/machine_obj.h"
#include "cpu/m68k.h"
#include "cpu/disasm.h"
#include "devices/via.h"
#include "devices/via2.h"

#include <cstdio>
#include <cstring>
#include <string>

/* ── visibility toggles (accessible from menu) ─────── */

bool g_showRegisters   = false;
bool g_showDisassembly = false;
bool g_showMemory      = false;
bool g_showVIA         = false;

/* ── Register Window ───────────────────────────────── */

static void DrawRegisterWindow()
{
	if (!g_showRegisters) return;
	if (!ImGui::Begin("Registers", &g_showRegisters)) {
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

	/* Flags */
	ImGui::Text("Flags: %c%c%c%c%c  IPL=%d  %c%c",
		(sr & 0x10) ? 'X' : '-',
		(sr & 0x08) ? 'N' : '-',
		(sr & 0x04) ? 'Z' : '-',
		(sr & 0x02) ? 'V' : '-',
		(sr & 0x01) ? 'C' : '-',
		(sr >> 8) & 7,
		(sr & 0x2000) ? 'S' : 'U',
		(sr & 0x8000) ? 'T' : '-');
	ImGui::Separator();

	/* Data and address registers side by side */
	if (ImGui::BeginTable("regs", 2)) {
		ImGui::TableSetupColumn("Data");
		ImGui::TableSetupColumn("Address");
		ImGui::TableHeadersRow();
		for (int i = 0; i < 8; ++i) {
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

/* ── Disassembly Window ────────────────────────────── */

static void DrawDisassemblyWindow()
{
	if (!g_showDisassembly) return;
	if (!ImGui::Begin("Disassembly", &g_showDisassembly)) {
		ImGui::End();
		return;
	}

	static uint32_t disasmAddr = 0;
	static bool followPC = true;

	ImGui::Checkbox("Follow PC", &followPC);
	ImGui::SameLine();
	if (!followPC) {
		ImGui::SetNextItemWidth(100);
		ImGui::InputScalar("Address", ImGuiDataType_U32,
			&disasmAddr, nullptr, nullptr, "%08X",
			ImGuiInputTextFlags_CharsHexadecimal);
	}

	uint32_t pc = m68k_getPC_public();
	uint32_t addr = followPC ? pc : disasmAddr;

	/* Show ~32 lines of disassembly */
	if (ImGui::BeginChild("disasm_listing", ImVec2(0, 0), ImGuiChildFlags_None,
		ImGuiWindowFlags_HorizontalScrollbar))
	{
		for (int i = 0; i < 32; ++i) {
			uint32_t lineAddr = addr;
			std::string text = Disassemble(addr);

			bool isCurrent = (lineAddr == pc);
			if (isCurrent)
				ImGui::PushStyleColor(ImGuiCol_Text,
					ImVec4(1.0f, 1.0f, 0.3f, 1.0f));

			ImGui::Text("%s%08X  %s",
				isCurrent ? ">" : " ",
				lineAddr, text.c_str());

			if (isCurrent)
				ImGui::PopStyleColor();
		}
	}
	ImGui::EndChild();

	ImGui::End();
}

/* ── Memory Viewer ─────────────────────────────────── */

static void DrawMemoryWindow()
{
	if (!g_showMemory) return;
	if (!ImGui::Begin("Memory", &g_showMemory)) {
		ImGui::End();
		return;
	}

	static uint32_t memAddr = 0;
	static int bytesPerRow = 16;

	ImGui::SetNextItemWidth(100);
	ImGui::InputScalar("Address", ImGuiDataType_U32,
		&memAddr, nullptr, nullptr, "%08X",
		ImGuiInputTextFlags_CharsHexadecimal);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(60);
	if (ImGui::BeginCombo("##bpr", std::to_string(bytesPerRow).c_str())) {
		for (int n : {8, 16, 32}) {
			if (ImGui::Selectable(std::to_string(n).c_str(), bytesPerRow == n))
				bytesPerRow = n;
		}
		ImGui::EndCombo();
	}

	uint32_t ramSz = g_machine ? g_machine->ramSize() : 0;
	uint8_t *ram = g_ram;

	if (ImGui::BeginChild("mem_hex", ImVec2(0, 0), ImGuiChildFlags_None,
		ImGuiWindowFlags_HorizontalScrollbar))
	{
		/* Show enough rows to fill the window */
		int rows = 32;
		for (int r = 0; r < rows; ++r) {
			uint32_t rowAddr = memAddr + (uint32_t)(r * bytesPerRow);
			char line[256];
			int pos = snprintf(line, sizeof(line), "%08X  ", rowAddr);

			char ascii[64];
			int apos = 0;
			for (int b = 0; b < bytesPerRow; ++b) {
				uint32_t a = rowAddr + (uint32_t)b;
				if (ram && a < ramSz) {
					uint8_t v = ram[a];
					pos += snprintf(line + pos, sizeof(line) - (size_t)pos,
						"%02X ", v);
					ascii[apos++] = (v >= 0x20 && v < 0x7F) ? (char)v : '.';
				} else {
					pos += snprintf(line + pos, sizeof(line) - (size_t)pos,
						"?? ");
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

/* ── VIA State Window ──────────────────────────────── */

static void DrawVIAWindow()
{
	if (!g_showVIA) return;
	if (!g_machine) return;
	if (!ImGui::Begin("VIA State", &g_showVIA)) {
		ImGui::End();
		return;
	}

	auto drawVIA = [](const char* label, VIABase* via) {
		if (!via) return;
		if (!ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen))
			return;

		auto& d = via->d_;
		ImGui::Text("ORA  %02X  ORB  %02X", d.ORA, d.ORB);
		ImGui::Text("DDRA %02X  DDRB %02X", d.DDR_A, d.DDR_B);
		ImGui::Text("T1C  %08X  T1L  %02X%02X",
			d.T1C_F, d.T1L_H, d.T1L_L);
		ImGui::Text("T2C  %08X  T2L  %02X",
			d.T2C_F, d.T2L_L);
		ImGui::Text("SR   %02X  ACR  %02X  PCR  %02X",
			d.SR, d.ACR, d.PCR);
		ImGui::Text("IFR  %02X  IER  %02X", d.IFR, d.IER);
		ImGui::Text("T1Active %d  T2Active %d",
			via->T1_Active, via->T2_Active);
	};

	drawVIA("VIA1", g_machine->findDevice<VIA1Device>());
	drawVIA("VIA2", g_machine->findDevice<VIA2Device>());

	ImGui::End();
}

/* ── Main entry point ──────────────────────────────── */

void DrawDebugWindows()
{
	DrawRegisterWindow();
	DrawDisassemblyWindow();
	DrawMemoryWindow();
	DrawVIAWindow();
}
