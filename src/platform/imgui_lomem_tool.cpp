/*
	imgui_lomem_tool.cpp — Low Memory Globals viewer panel

	Sortable, filterable table of Macintosh low-memory globals
	with live values read from emulated RAM. Mark/Clear buttons
	highlight values that changed since the last snapshot.
*/

#include "platform/imgui_lomem_tool.h"
#include "platform/lomem_globals.h"
#include "lang/global_registry.h"
#include "core/machine.h"
#include "platform/common/mac_roman.h"

#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <algorithm>

/* Case-insensitive substring search. */
static bool contains_ci(const char *haystack, const char *needle)
{
	if (!needle[0]) return true;
	for (const char *h = haystack; *h; ++h)
	{
		const char *a = h, *b = needle;
		while (
			*a && *b &&
			((*a ^ *b) == 0 || ((*a ^ *b) == 0x20 && ((*a | 0x20) >= 'a') && ((*a | 0x20) <= 'z'))))
		{
			++a;
			++b;
		}
		if (!*b) return true;
	}
	return false;
}

/* ── Value formatting ─────────────────────────────────── */

static uint8_t rd8(uint32_t a)
{
	return g_ram[a];
}

static uint16_t rd16(uint32_t a)
{
	return (static_cast<uint16_t>(g_ram[a]) << 8) | g_ram[a + 1];
}

static uint32_t rd32(uint32_t a)
{
	return (static_cast<uint32_t>(g_ram[a]) << 24) | (static_cast<uint32_t>(g_ram[a + 1]) << 16) |
		   (static_cast<uint32_t>(g_ram[a + 2]) << 8) | static_cast<uint32_t>(g_ram[a + 3]);
}

static void formatGlobalValue(const GlobalDef &gd, char *buf, int bufSize)
{
	if (!g_ram)
	{
		snprintf(buf, bufSize, "\xe2\x80\x94"); /* — */
		return;
	}

	uint32_t a = gd.addr;
	std::string_view tn = gd.typeName;

	/* Pointer/Handle to struct */
	if (tn.size() > 2 && tn[0] == '^' && tn[1] == '^')
	{
		snprintf(buf, bufSize, "$%08X (H)", rd32(a));
		return;
	}
	if (tn.size() > 1 && tn[0] == '^')
	{
		snprintf(buf, bufSize, "$%08X", rd32(a));
		return;
	}

	/* Strip array suffix for base type check */
	std::string_view baseType = tn;
	auto bracket = tn.find('[');
	if (bracket != std::string_view::npos) baseType = tn.substr(0, bracket);

	if (baseType == "byte" || baseType == "Boolean")
	{
		if (gd.count > 1)
		{
			/* byte array */
			int pos = 0;
			int show = gd.size > 16 ? 16 : gd.size;
			for (int i = 0; i < show && pos + 4 < bufSize; i++)
			{
				if (i > 0) buf[pos++] = ' ';
				pos += snprintf(buf + pos, bufSize - pos, "%02X", rd8(a + i));
			}
			if (gd.size > 16 && pos + 4 < bufSize)
			{
				buf[pos++] = ' ';
				buf[pos++] = '\xE2';
				buf[pos++] = '\x80';
				buf[pos++] = '\xA6'; /* … */
			}
			buf[pos] = '\0';
		}
		else
		{
			snprintf(buf, bufSize, "$%02X", rd8(a));
		}
	}
	else if (baseType == "sbyte")
	{
		snprintf(buf, bufSize, "%d", static_cast<int8_t>(rd8(a)));
	}
	else if (baseType == "word")
	{
		snprintf(buf, bufSize, "$%04X", rd16(a));
	}
	else if (baseType == "sword" || baseType == "OSErr")
	{
		snprintf(buf, bufSize, "%d", static_cast<int16_t>(rd16(a)));
	}
	else if (baseType == "long")
	{
		snprintf(buf, bufSize, "$%08X", rd32(a));
	}
	else if (baseType == "slong")
	{
		snprintf(buf, bufSize, "%d", static_cast<int32_t>(rd32(a)));
	}
	else if (baseType == "Ptr" || baseType == "ProcPtr")
	{
		if (gd.count > 1)
		{
			/* Array of pointers */
			int pos = 0;
			int show = gd.count > 4 ? 4 : gd.count;
			for (int i = 0; i < show && pos + 12 < bufSize; i++)
			{
				if (i > 0) pos += snprintf(buf + pos, bufSize - pos, " ");
				pos += snprintf(buf + pos, bufSize - pos, "$%08X", rd32(a + i * 4));
			}
			if (gd.count > 4 && pos + 4 < bufSize)
			{
				buf[pos++] = ' ';
				buf[pos++] = '\xE2';
				buf[pos++] = '\x80';
				buf[pos++] = '\xA6';
			}
			buf[pos] = '\0';
		}
		else
		{
			snprintf(buf, bufSize, "$%08X", rd32(a));
		}
	}
	else if (baseType == "Handle")
	{
		if (gd.count > 1)
		{
			int pos = 0;
			int show = gd.count > 4 ? 4 : gd.count;
			for (int i = 0; i < show && pos + 16 < bufSize; i++)
			{
				if (i > 0) pos += snprintf(buf + pos, bufSize - pos, " ");
				pos += snprintf(buf + pos, bufSize - pos, "$%08X", rd32(a + i * 4));
			}
			if (gd.count > 4 && pos + 4 < bufSize)
			{
				buf[pos++] = ' ';
				buf[pos++] = '\xE2';
				buf[pos++] = '\x80';
				buf[pos++] = '\xA6';
			}
			buf[pos] = '\0';
		}
		else
		{
			snprintf(buf, bufSize, "$%08X (H)", rd32(a));
		}
	}
	else if (baseType == "OSType")
	{
		uint8_t c[4];
		for (int i = 0; i < 4; i++)
			c[i] = rd8(a + i);
		bool printable = true;
		for (int i = 0; i < 4; i++)
		{
			if (c[i] < 0x20 || c[i] > 0x7E)
			{
				printable = false;
				break;
			}
		}
		if (printable)
			snprintf(buf, bufSize, "'%c%c%c%c'", c[0], c[1], c[2], c[3]);
		else
			snprintf(buf, bufSize, "$%08X", rd32(a));
	}
	else if (baseType == "Str31" || baseType == "Str63" || baseType == "Str255")
	{
		uint8_t len = rd8(a);
		if (len > 31) len = 31;
		uint8_t raw[32];
		for (int i = 0; i < len; i++)
			raw[i] = rd8(a + 1 + i);
		uint32_t uLen = MacRoman2UniCodeSize(raw, len);
		if (static_cast<int>(uLen) + 3 > bufSize) uLen = bufSize - 3;
		buf[0] = '"';
		MacRoman2UniCodeData(raw, len, buf + 1);
		buf[1 + uLen] = '"';
		buf[2 + uLen] = '\0';
	}
	else if (baseType == "Rect")
	{
		auto top = static_cast<int16_t>(rd16(a));
		auto left = static_cast<int16_t>(rd16(a + 2));
		auto bottom = static_cast<int16_t>(rd16(a + 4));
		auto right = static_cast<int16_t>(rd16(a + 6));
		snprintf(buf, bufSize, "(%d,%d)-(%d,%d)", top, left, bottom, right);
	}
	else if (baseType == "Pattern")
	{
		int pos = 0;
		for (int i = 0; i < 8 && pos + 4 < bufSize; i++)
		{
			if (i > 0) buf[pos++] = ' ';
			pos += snprintf(buf + pos, bufSize - pos, "%02X", rd8(a + i));
		}
		buf[pos] = '\0';
	}
	else
	{
		/* Structs, QHdr, etc. — show raw hex */
		int pos = 0;
		int show = gd.size > 16 ? 16 : gd.size;
		for (int i = 0; i < show && pos + 4 < bufSize; i++)
		{
			if (i > 0) buf[pos++] = ' ';
			pos += snprintf(buf + pos, bufSize - pos, "%02X", rd8(a + i));
		}
		if (gd.size > 16 && pos + 4 < bufSize)
		{
			buf[pos++] = ' ';
			buf[pos++] = '\xE2';
			buf[pos++] = '\x80';
			buf[pos++] = '\xA6';
		}
		buf[pos] = '\0';
	}
}

/* ── Draw ─────────────────────────────────────────────── */

void LowMemTool::draw()
{
	if (!ImGui::Begin(name(), &visible))
	{
		ImGui::End();
		return;
	}

	if (!g_ram)
	{
		ImGui::Text("No machine loaded");
		ImGui::End();
		return;
	}

	auto &reg = g_globalRegistry();
	auto allGlobals = reg.globals();
	auto sections = reg.sections();

	/* ── Controls ── */
	ImGui::SetNextItemWidth(200);
	ImGui::InputText("Filter", filterBuf_, sizeof(filterBuf_));

	ImGui::SameLine();
	ImGui::SetNextItemWidth(140);
	const char *sectionLabel = sectionFilter_ == 0 ? "All" : sections[sectionFilter_ - 1].c_str();
	if (ImGui::BeginCombo("Section", sectionLabel))
	{
		if (ImGui::Selectable("All", sectionFilter_ == 0)) sectionFilter_ = 0;
		for (int i = 0; i < static_cast<int>(sections.size()); i++)
		{
			if (ImGui::Selectable(sections[i].c_str(), sectionFilter_ == i + 1))
				sectionFilter_ = i + 1;
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	if (ImGui::Button("Mark"))
	{
		Lomem_SnapshotTake(snapshot_);
		snapshotValid_ = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Clear"))
	{
		snapshotValid_ = false;
	}

	ImGui::Separator();

	/* ── Table ── */
	const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
								  ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY |
								  ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable;

	if (ImGui::BeginTable("lomem", 5, flags))
	{
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort, 0.0f, 0);
		ImGui::TableSetupColumn(
			"Addr", ImGuiTableColumnFlags_None | ImGuiTableColumnFlags_WidthFixed, 70.0f, 1);
		ImGui::TableSetupColumn(
			"Type", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed, 70.0f, 2);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_NoSort, 0.0f, 3);
		ImGui::TableSetupColumn("Brief", ImGuiTableColumnFlags_NoSort, 0.0f, 4);
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableHeadersRow();

		/* Build filtered index. */
		int totalGlobals = static_cast<int>(allGlobals.size());
		static std::vector<int> sortedIdx;
		sortedIdx.clear();
		sortedIdx.reserve(totalGlobals);

		for (int i = 0; i < totalGlobals; i++)
		{
			const GlobalDef &e = allGlobals[i];
			if (sectionFilter_ > 0)
			{
				int si = sectionFilter_ - 1;
				if (si < static_cast<int>(sections.size()) && e.section != sections[si]) continue;
			}
			if (filterBuf_[0] && !contains_ci(e.name.c_str(), filterBuf_) &&
				!contains_ci(e.brief.c_str(), filterBuf_))
				continue;
			sortedIdx.push_back(i);
		}

		/* Sort. */
		if (ImGuiTableSortSpecs *specs = ImGui::TableGetSortSpecs())
		{
			if (specs->SpecsCount > 0)
			{
				auto &s = specs->Specs[0];
				bool asc = (s.SortDirection == ImGuiSortDirection_Ascending);
				std::sort(sortedIdx.begin(), sortedIdx.end(),
						  [&allGlobals, &s, asc](int ai, int bi)
						  {
							  const GlobalDef &a = allGlobals[ai];
							  const GlobalDef &b = allGlobals[bi];
							  int cmp = 0;
							  switch (s.ColumnUserID)
							  {
								  case 0:
									  cmp = a.name.compare(b.name);
									  break;
								  case 1:
									  cmp = (a.addr < b.addr) ? -1 : (a.addr > b.addr) ? 1 : 0;
									  break;
							  }
							  return asc ? (cmp < 0) : (cmp > 0);
						  });
			}
		}

		/* Rows. */
		int count = static_cast<int>(sortedIdx.size());
		for (int n = 0; n < count; n++)
		{
			const GlobalDef &e = allGlobals[sortedIdx[n]];
			ImGui::TableNextRow();

			/* Name */
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(e.name.c_str());

			/* Addr */
			ImGui::TableNextColumn();
			ImGui::Text("$%04X", e.addr);

			/* Type */
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(e.typeName.c_str());

			/* Value */
			ImGui::TableNextColumn();
			bool changed = snapshotValid_ && lomem_snapshot_changed(snapshot_, e.addr, e.size);
			if (changed) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
			char vbuf[128];
			formatGlobalValue(e, vbuf, sizeof(vbuf));
			ImGui::TextUnformatted(vbuf);
			if (changed) ImGui::PopStyleColor();

			/* Brief */
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(e.brief.c_str());
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", e.brief.c_str());
		}

		ImGui::EndTable();
	}

	ImGui::End();
}
