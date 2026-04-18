/*
	global_registry.cpp — Parser for assets/globals.def

	Reads the low-memory globals definition file and builds sorted
	indices for fast name and address lookup.
*/

#include "lang/global_registry.h"
#include "lang/type_registry.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>

/* ── Helpers ──────────────────────────────────────────── */

/* Case-insensitive compare for name lookup. */
static int CICompare(std::string_view a, std::string_view b)
{
	auto len = std::min(a.size(), b.size());
	for (size_t i = 0; i < len; ++i)
	{
		int ca = std::tolower(static_cast<unsigned char>(a[i]));
		int cb = std::tolower(static_cast<unsigned char>(b[i]));
		if (ca != cb) return ca - cb;
	}
	if (a.size() < b.size()) return -1;
	if (a.size() > b.size()) return 1;
	return 0;
}

/* ── Singleton ────────────────────────────────────────── */

GlobalRegistry &g_globalRegistry()
{
	static GlobalRegistry s_instance;
	return s_instance;
}

/* ── Parser ───────────────────────────────────────────── */

int GlobalRegistry::load(const std::filesystem::path &path, const TypeRegistry &types)
{
	std::ifstream f(path);
	if (!f.is_open()) return 0;

	std::string fileName = path.filename().string();
	std::string currentSection;
	int lineNo = 0;
	std::string line;

	while (std::getline(f, line))
	{
		++lineNo;

		/* Skip blank lines */
		{
			bool blank = true;
			for (char c : line)
			{
				if (!std::isspace(static_cast<unsigned char>(c)))
				{
					blank = false;
					break;
				}
			}
			if (blank) continue;
		}

		/* Skip #? commented-out globals */
		{
			auto pos = line.find_first_not_of(" \t");
			if (pos != std::string::npos && line[pos] == '#')
			{
				/* Check for section header: # ── SectionName ── */
				if (line.size() > pos + 1 && line[pos + 1] != '?')
				{
					/* Look for ── delimiters */
					auto dash1 = line.find("\xe2\x94\x80\xe2\x94\x80", pos); /* ── in UTF-8 */
					if (dash1 != std::string::npos)
					{
						auto nameStart = dash1 + 6; /* skip "── " */
						/* Skip spaces after first delimiter */
						while (nameStart < line.size() && line[nameStart] == ' ')
							++nameStart;
						/* Find second delimiter */
						auto dash2 = line.find("\xe2\x94\x80\xe2\x94\x80", nameStart);
						if (dash2 != std::string::npos)
						{
							auto nameEnd = dash2;
							while (nameEnd > nameStart && line[nameEnd - 1] == ' ')
								--nameEnd;
							currentSection = line.substr(nameStart, nameEnd - nameStart);
							/* Track unique sections */
							bool found = false;
							for (auto &s : sections_)
							{
								if (s == currentSection)
								{
									found = true;
									break;
								}
							}
							if (!found) sections_.push_back(currentSection);
						}
					}
				}
				continue; /* skip all comment lines */
			}
		}

		/* Parse data line: <hex_addr> <type> <name> "<description>" */
		std::istringstream iss(line);
		std::string addrStr, typeStr, nameStr;
		if (!(iss >> addrStr >> typeStr >> nameStr))
		{
			std::fprintf(stderr, "%s:%d: bad globals line\n", fileName.c_str(), lineNo);
			continue;
		}

		/* Parse address */
		uint32_t addr = 0;
		if (addrStr.size() > 2 && addrStr[0] == '0' && (addrStr[1] == 'x' || addrStr[1] == 'X'))
		{
			addr = static_cast<uint32_t>(std::strtoul(addrStr.c_str(), nullptr, 16));
		}
		else
		{
			std::fprintf(stderr, "%s:%d: bad address '%s'\n", fileName.c_str(), lineNo,
						 addrStr.c_str());
			continue;
		}

		/* Parse description (everything inside quotes) */
		std::string brief;
		auto qpos = line.find('"');
		if (qpos != std::string::npos)
		{
			auto qend = line.rfind('"');
			if (qend > qpos) brief = line.substr(qpos + 1, qend - qpos - 1);
		}

		/* Parse type: handle ^/^^ prefixes and array [N] suffix */
		uint16_t count = 1;
		uint16_t size = 0;

		if (typeStr.size() > 2 && typeStr[0] == '^' && typeStr[1] == '^')
		{
			/* Handle-to-struct: ^^StructName */
			size = 4;
			/* Validate struct exists */
			std::string_view structName(typeStr.data() + 2, typeStr.size() - 2);
			if (!types.has(structName))
			{
				std::fprintf(stderr, "%s:%d: unknown struct '%.*s' in type '%s'\n",
							 fileName.c_str(), lineNo, (int)structName.size(), structName.data(),
							 typeStr.c_str());
			}
		}
		else if (typeStr.size() > 1 && typeStr[0] == '^')
		{
			/* Pointer-to-struct: ^StructName */
			size = 4;
			/* Validate struct exists */
			std::string_view structName(typeStr.data() + 1, typeStr.size() - 1);
			if (!types.has(structName))
			{
				std::fprintf(stderr, "%s:%d: unknown struct '%.*s' in type '%s'\n",
							 fileName.c_str(), lineNo, (int)structName.size(), structName.data(),
							 typeStr.c_str());
			}
		}
		else
		{
			/* Split on '[' for array types */
			std::string baseType = typeStr;
			auto bracket = typeStr.find('[');
			if (bracket != std::string::npos)
			{
				auto end = typeStr.find(']', bracket);
				if (end != std::string::npos)
				{
					int n = std::atoi(typeStr.c_str() + bracket + 1);
					if (n > 0) count = static_cast<uint16_t>(n);
				}
				baseType = typeStr.substr(0, bracket);
			}

			uint16_t elemSize = types.sizeOf(baseType);
			if (elemSize == 0)
			{
				std::fprintf(stderr, "%s:%d: unknown type '%s'\n", fileName.c_str(), lineNo,
							 baseType.c_str());
				continue;
			}
			size = elemSize * count;
		}

		GlobalDef gd;
		gd.name = std::move(nameStr);
		gd.addr = addr;
		gd.size = size;
		gd.typeName = std::move(typeStr);
		gd.count = count;
		gd.section = currentSection;
		gd.brief = std::move(brief);
		globals_.push_back(std::move(gd));
	}

	/* Build sorted indices */
	int n = static_cast<int>(globals_.size());

	byName_.resize(n);
	for (int i = 0; i < n; ++i)
		byName_[i] = i;
	std::sort(byName_.begin(), byName_.end(),
			  [this](int a, int b) { return CICompare(globals_[a].name, globals_[b].name) < 0; });

	byAddr_.resize(n);
	for (int i = 0; i < n; ++i)
		byAddr_[i] = i;
	std::sort(byAddr_.begin(), byAddr_.end(),
			  [this](int a, int b) { return globals_[a].addr < globals_[b].addr; });

	return n;
}

/* ── Accessors ────────────────────────────────────────── */

std::span<const GlobalDef> GlobalRegistry::globals() const
{
	return globals_;
}

int GlobalRegistry::count() const
{
	return static_cast<int>(globals_.size());
}

std::span<const std::string> GlobalRegistry::sections() const
{
	return sections_;
}

const GlobalDef *GlobalRegistry::findByName(std::string_view name) const
{
	auto it =
		std::lower_bound(byName_.begin(), byName_.end(), name, [this](int idx, std::string_view n)
						 { return CICompare(globals_[idx].name, n) < 0; });
	if (it != byName_.end() && CICompare(globals_[*it].name, name) == 0) return &globals_[*it];
	return nullptr;
}

const GlobalDef *GlobalRegistry::findByAddr(uint32_t addr) const
{
	auto it = std::lower_bound(byAddr_.begin(), byAddr_.end(), addr,
							   [this](int idx, uint32_t a) { return globals_[idx].addr < a; });
	if (it != byAddr_.end() && globals_[*it].addr == addr) return &globals_[*it];
	return nullptr;
}
