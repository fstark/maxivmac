/*
	imgui_lomem_tool.cpp — Low Memory Globals viewer panel

	Sortable, filterable table of Macintosh low-memory globals
	with live values read from emulated RAM. Mark/Clear buttons
	highlight values that changed since the last snapshot.
*/

#include "platform/imgui_lomem_tool.h"
#include "platform/lomem_globals.h"
#include "core/machine.h"

#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <algorithm>

/* Case-insensitive substring search. */
static bool contains_ci(const char *haystack, const char *needle)
{
	if (!needle[0]) return true;
	for (const char *h = haystack; *h; ++h) {
		const char *a = h, *b = needle;
		while (*a && *b && ((*a ^ *b) == 0 ||
			   ((*a ^ *b) == 0x20 && ((*a | 0x20) >= 'a') && ((*a | 0x20) <= 'z'))))
		{
			++a; ++b;
		}
		if (!*b) return true;
	}
	return false;
}

void LowMemTool::draw()
{
	if (!ImGui::Begin(name(), &visible)) {
		ImGui::End();
		return;
	}

	if (!g_ram) {
		ImGui::Text("No machine loaded");
		ImGui::End();
		return;
	}

	/* ── Controls ── */
	ImGui::SetNextItemWidth(200);
	ImGui::InputText("Filter", filterBuf_, sizeof(filterBuf_));

	ImGui::SameLine();
	ImGui::SetNextItemWidth(120);
	if (ImGui::BeginCombo("Category", categoryFilter_ == 0 ? "All" : kLMCategoryLabels[categoryFilter_ - 1])) {
		if (ImGui::Selectable("All", categoryFilter_ == 0))
			categoryFilter_ = 0;
		for (int i = 0; i < LM_CAT_COUNT; i++) {
			if (ImGui::Selectable(kLMCategoryLabels[i], categoryFilter_ == i + 1))
				categoryFilter_ = i + 1;
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	if (ImGui::Button("Mark")) {
		lomem_snapshot_take(snapshot_);
		snapshotValid_ = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Clear")) {
		snapshotValid_ = false;
	}

	ImGui::Separator();

	/* ── Table ── */
	const ImGuiTableFlags flags =
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable;

	if (ImGui::BeginTable("lomem", 5, flags)) {
		ImGui::TableSetupColumn("Name",  ImGuiTableColumnFlags_DefaultSort, 0.0f, 0);
		ImGui::TableSetupColumn("Addr",  ImGuiTableColumnFlags_None | ImGuiTableColumnFlags_WidthFixed, 70.0f, 1);
		ImGui::TableSetupColumn("Type",  ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed, 50.0f, 2);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_NoSort, 0.0f, 3);
		ImGui::TableSetupColumn("Brief", ImGuiTableColumnFlags_NoSort, 0.0f, 4);
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableHeadersRow();

		/* Build filtered index. */
		static int sortedIdx[256];
		int count = 0;
		for (int i = 0; i < kLowMemCount && count < 256; i++) {
			const LMGlobal &e = kLowMemGlobals[i];
			if (categoryFilter_ > 0 && (int)e.category != categoryFilter_ - 1)
				continue;
			if (filterBuf_[0] && !contains_ci(e.name, filterBuf_) && !contains_ci(e.brief, filterBuf_))
				continue;
			sortedIdx[count++] = i;
		}

		/* Sort. */
		if (ImGuiTableSortSpecs *specs = ImGui::TableGetSortSpecs()) {
			if (specs->SpecsCount > 0) {
				auto &s = specs->Specs[0];
				bool asc = (s.SortDirection == ImGuiSortDirection_Ascending);
				std::sort(sortedIdx, sortedIdx + count,
					[&s, asc](int ai, int bi) {
						const LMGlobal &a = kLowMemGlobals[ai];
						const LMGlobal &b = kLowMemGlobals[bi];
						int cmp = 0;
						switch (s.ColumnUserID) {
						case 0: cmp = strcmp(a.name, b.name); break;
						case 1: cmp = (a.addr < b.addr) ? -1 : (a.addr > b.addr) ? 1 : 0; break;
						}
						return asc ? (cmp < 0) : (cmp > 0);
					});
			}
		}

		/* Rows. */
		for (int n = 0; n < count; n++) {
			const LMGlobal &e = kLowMemGlobals[sortedIdx[n]];
			ImGui::TableNextRow();

			/* Name */
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(e.name);

			/* Addr */
			ImGui::TableNextColumn();
			ImGui::Text("$%04X", e.addr);

			/* Type */
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(lomem_type_label(e.type));

			/* Value */
			ImGui::TableNextColumn();
			bool changed = snapshotValid_ && lomem_snapshot_changed(snapshot_, e.addr, e.size);
			if (changed)
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
			char vbuf[128];
			lomem_format_value(&e, vbuf, sizeof(vbuf));
			ImGui::TextUnformatted(vbuf);
			if (changed)
				ImGui::PopStyleColor();

			/* Brief */
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(e.brief);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", e.brief);
		}

		ImGui::EndTable();
	}

	ImGui::End();
}
