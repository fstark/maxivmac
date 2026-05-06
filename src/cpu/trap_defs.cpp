/*
	trap_defs.cpp — Parser for the external trap definition file (assets/traps.def)
*/

#include "cpu/trap_defs.h"
#include "lang/type_registry.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

/* ── helpers ──────────────────────────────────────────── */

static std::string StrToLower(std::string_view sv)
{
	std::string s(sv);
	for (auto &c : s)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return s;
}

static bool BlankOrComment(const std::string &line)
{
	for (char c : line)
	{
		if (c == '#') return true;
		if (!std::isspace(static_cast<unsigned char>(c))) return false;
	}
	return true;
}

/* Parse a single param token: "name:type" or "name:type.reg"
   Type may be "^StructName" for pointer-to-struct. */

static bool ParseRegLoc(const std::string &s, ParamLoc &out)
{
	auto low = StrToLower(s);
	if (low == "d0")
	{
		out = ParamLoc::D0;
		return true;
	}
	if (low == "d1")
	{
		out = ParamLoc::D1;
		return true;
	}
	if (low == "d2")
	{
		out = ParamLoc::D2;
		return true;
	}
	if (low == "d3")
	{
		out = ParamLoc::D3;
		return true;
	}
	if (low == "d4")
	{
		out = ParamLoc::D4;
		return true;
	}
	if (low == "d5")
	{
		out = ParamLoc::D5;
		return true;
	}
	if (low == "d6")
	{
		out = ParamLoc::D6;
		return true;
	}
	if (low == "d7")
	{
		out = ParamLoc::D7;
		return true;
	}
	if (low == "a0")
	{
		out = ParamLoc::A0;
		return true;
	}
	if (low == "a1")
	{
		out = ParamLoc::A1;
		return true;
	}
	if (low == "a2")
	{
		out = ParamLoc::A2;
		return true;
	}
	if (low == "a3")
	{
		out = ParamLoc::A3;
		return true;
	}
	if (low == "a4")
	{
		out = ParamLoc::A4;
		return true;
	}
	if (low == "a5")
	{
		out = ParamLoc::A5;
		return true;
	}
	if (low == "a6")
	{
		out = ParamLoc::A6;
		return true;
	}
	if (low == "a7")
	{
		out = ParamLoc::A7;
		return true;
	}
	return false;
}

static bool ParseParam(const std::string &token, ParamDef &out)
{
	auto colon = token.find(':');
	if (colon == std::string::npos || colon == 0) return false;

	out.name = token.substr(0, colon);
	std::string rest = token.substr(colon + 1);

	auto dot = rest.find('.');
	std::string typeStr, regStr;
	if (dot != std::string::npos)
	{
		typeStr = rest.substr(0, dot);
		regStr = rest.substr(dot + 1);
	}
	else
	{
		typeStr = rest;
	}

	/* Check for ^StructName (pointer-to-struct) */
	if (!typeStr.empty() && typeStr[0] == '^')
	{
		out.isStructPtr = true;
		out.typeName = typeStr.substr(1);
		if (out.typeName.empty()) return false;
	}
	else
	{
		out.isStructPtr = false;
		/* "Text" is a semantic alias for Str255 — marks displayable text */
		if (typeStr == "Text")
		{
			out.typeName = "Str255";
			out.isText = true;
		}
		else if (typeStr == "TextBuf")
		{
			out.typeName = "Ptr";
			out.isText = true;
		}
		else if (typeStr == "TextStart")
		{
			out.typeName = "sword";
			out.isTextStart = true;
		}
		else if (typeStr == "TextCount")
		{
			out.typeName = "sword";
			out.isTextCount = true;
		}
		else
		{
			out.typeName = typeStr;
		}
		if (out.typeName.empty()) return false;
		if (!g_typeRegistry().has(out.typeName))
		{
			fprintf(stderr, "trap_defs: unknown type '%s'\n", out.typeName.c_str());
			return false;
		}
	}

	if (!regStr.empty())
	{
		if (!ParseRegLoc(regStr, out.loc)) return false;
	}
	else
	{
		out.loc = ParamLoc::Stack;
	}
	return true;
}

/* ── TrapDefs implementation ─────────────────────────── */

uint16_t TrapDefs::maskTrapWord(uint16_t tw)
{
	if (tw & 0x0800)
		return 0xA800 | (tw & 0x03FF); /* Toolbox: keep bits 0-9 + bit 11 */
	else
		return 0xA000 | (tw & 0x03FF); /* OS: keep bits 0-9 */
}

bool TrapDefs::parseHeaderLine(const std::string &line, TrapDef &out)
{
	std::istringstream iss(line);
	std::string hexWord, name, conv;
	if (!(iss >> hexWord >> name >> conv))
	{
		fprintf(stderr, "trap_defs: bad header '%s'\n", line.c_str());
		return false;
	}

	char *end = nullptr;
	unsigned long tw = std::strtoul(hexWord.c_str(), &end, 16);
	if (end == hexWord.c_str() || tw > 0xFFFF)
	{
		fprintf(stderr, "trap_defs: bad trap word '%s'\n", hexWord.c_str());
		return false;
	}

	out.trapWord = static_cast<uint16_t>(tw);
	out.name = name;

	auto cl = StrToLower(conv);
	if (cl == "os")
		out.convention = TrapConvention::OS;
	else if (cl == "toolbox")
		out.convention = TrapConvention::Toolbox;
	else
	{
		fprintf(stderr, "trap_defs: bad convention '%s'\n", conv.c_str());
		return false;
	}

	std::string extra;
	if (iss >> extra)
	{
		if (extra.starts_with("dispatch="))
		{
			std::string dispatchSpec = extra.substr(9); /* e.g. "word.D0" */
			ParamDef selectorParam;
			selectorParam.name = "selector";
			if (ParseParam("selector:" + dispatchSpec, selectorParam))
			{
				out.dispatch = DispatchInfo{std::move(selectorParam)};
			}
			else
			{
				fprintf(stderr, "trap_defs: bad dispatch spec '%s'\n", dispatchSpec.c_str());
			}
		}
		else if (StrToLower(extra) == "noreturn")
		{
			out.noreturn = true;
		}
	}
	return true;
}

void TrapDefs::parseParamLine(const std::string &line, TrapDef &out)
{
	std::istringstream iss(line);
	std::string direction;
	iss >> direction;
	auto dir = StrToLower(direction);

	/* show-in / show-out: field filter for StructPtr params */
	if (dir == "show-in" || dir == "show-out")
	{
		std::string paramName;
		if (!(iss >> paramName))
		{
			fprintf(stderr, "trap_defs: %s missing param name\n", direction.c_str());
			return;
		}
		StructFieldFilter filter;
		filter.paramName = paramName;
		std::string field;
		while (iss >> field)
		{
			/* Check for hexdump annotation: fieldName:hexdump(sizeField) */
			auto colon = field.find(':');
			if (colon != std::string::npos)
			{
				auto tag = field.substr(colon + 1);
				auto ptrField = field.substr(0, colon);
				if (tag.starts_with("hexdump(") && tag.back() == ')')
				{
					HexdumpAnnotation hd;
					hd.ptrField = ptrField;
					hd.sizeField = tag.substr(8, tag.size() - 9);
					hd.maxBytes = 16;
					filter.hexdumps.push_back(std::move(hd));
					continue;
				}
			}
			filter.fields.push_back(field);
		}
		/* Validate field names against the struct type at load time */
		const ParamDef *targetParam = nullptr;
		auto &paramList = (dir == "show-in") ? out.paramsIn : out.paramsOut;
		for (auto &pd : paramList)
			if (pd.name == filter.paramName)
			{
				targetParam = &pd;
				break;
			}
		if (targetParam && targetParam->isStructPtr)
		{
			auto &tr = g_typeRegistry();
			if (tr.has(targetParam->typeName))
			{
				auto allFields = tr.fieldNames(targetParam->typeName);
				for (auto &want : filter.fields)
				{
					bool found = false;
					for (auto &fn : allFields)
					{
						std::string_view leaf = fn;
						auto dot = leaf.rfind('.');
						if (dot != std::string_view::npos) leaf = leaf.substr(dot + 1);
						if (leaf == want)
						{
							found = true;
							break;
						}
						/* Match composite struct prefixes (e.g. "ioFlFndrInfo"
						   matches "ioFlFndrInfo.fdType") */
						std::string wantDot = want + ".";
						if (fn.starts_with(wantDot))
						{
							found = true;
							break;
						}
					}
					if (!found)
						fprintf(stderr, "trap_defs: %s field '%s' not found in struct '%s'\n",
								direction.c_str(), want.c_str(), targetParam->typeName.c_str());
				}
			}
		}
		if (dir == "show-in")
			out.showIn.push_back(std::move(filter));
		else
			out.showOut.push_back(std::move(filter));
		return;
	}

	/* "void" marks a confirmed-parameterless trap (no in/out params). */
	if (dir == "void") return;

	bool isIn = (dir == "in");
	bool isOut = (dir == "out");
	if (!isIn && !isOut)
	{
		fprintf(stderr, "trap_defs: bad param direction '%s'\n", direction.c_str());
		return;
	}
	std::string tok;
	while (iss >> tok)
	{
		ParamDef pd;
		if (ParseParam(tok, pd))
		{
			if (isIn)
				out.paramsIn.push_back(std::move(pd));
			else
				out.paramsOut.push_back(std::move(pd));
		}
		else
		{
			fprintf(stderr, "trap_defs: bad param '%s'\n", tok.c_str());
		}
	}
}

int TrapDefs::load(const std::filesystem::path &path)
{
	std::ifstream f(path);
	if (!f.is_open()) return 0;

	int count = 0;
	std::string line;
	TrapDef current{};
	bool inEntry = false;

	/* Subtrap parsing state */
	bool inSubtrap = false;
	uint16_t currentSubSelector = 0;
	SubtrapDef currentSub{};
	uint16_t currentParentMasked = 0;

	auto flushSubtrap = [&]()
	{
		if (inSubtrap)
		{
			subtraps_[currentParentMasked][currentSubSelector] = std::move(currentSub);
			inSubtrap = false;
		}
	};

	auto flushEntry = [&]()
	{
		flushSubtrap();
		if (inEntry)
		{
			defs_[maskTrapWord(current.trapWord)] = std::move(current);
			current = TrapDef{};
			inEntry = false;
			++count;
		}
	};

	while (std::getline(f, line))
	{
		if (BlankOrComment(line))
		{
			flushEntry();
			continue;
		}

		bool isIndented = std::isspace(static_cast<unsigned char>(line[0]));

		if (isIndented && inEntry)
		{
			/* Check for subtrap directive */
			std::string_view sv(line);
			auto pos = sv.find_first_not_of(" \t");
			if (pos != std::string_view::npos)
			{
				std::string_view trimmed = sv.substr(pos);
				if (trimmed.starts_with("subtrap "))
				{
					flushSubtrap();

					std::istringstream iss{std::string(trimmed)};
					std::string directive, hexSel, name;
					if (!(iss >> directive >> hexSel >> name))
					{
						fprintf(stderr, "trap_defs: bad subtrap line '%s'\n", line.c_str());
						continue;
					}
					char *end = nullptr;
					unsigned long sel = std::strtoul(hexSel.c_str(), &end, 16);

					inSubtrap = true;
					currentSubSelector = static_cast<uint16_t>(sel);
					currentParentMasked = maskTrapWord(current.trapWord);
					currentSub = SubtrapDef{};
					currentSub.selector = currentSubSelector;
					currentSub.def.trapWord = current.trapWord;
					currentSub.def.name = name;
					currentSub.def.convention = current.convention;
					continue;
				}
			}

			if (inSubtrap)
				parseParamLine(line, currentSub.def);
			else
				parseParamLine(line, current);
		}
		else
		{
			flushEntry();
			inEntry = parseHeaderLine(line, current);
		}
	}

	flushEntry();

	/* Build sorted name index for name/search API (includes subtraps) */
	sortedNames_.clear();
	sortedNames_.reserve(defs_.size() + 64);
	for (auto &[tw, def] : defs_)
		sortedNames_.push_back({static_cast<uint32_t>(def.trapWord), def.name});
	for (auto &[parentTw, selMap] : subtraps_)
	{
		for (auto &[sel, sub] : selMap)
		{
			uint32_t synKey = (static_cast<uint32_t>(parentTw) << 16) | sel;
			sortedNames_.push_back({synKey, sub.def.name});
		}
	}
	std::sort(sortedNames_.begin(), sortedNames_.end(),
			  [](auto &a, auto &b) { return a.second < b.second; });

	return count;
}

int TrapDefs::loadErrors(const std::filesystem::path &path)
{
	std::ifstream f(path);
	if (!f.is_open()) return 0;

	int count = 0;
	std::string line;
	while (std::getline(f, line))
	{
		if (BlankOrComment(line)) continue;

		std::istringstream iss(line);
		int code;
		std::string name;
		if (!(iss >> code >> name)) continue;

		errors_[static_cast<int16_t>(code)] = name;
		++count;
	}
	return count;
}

const TrapDef *TrapDefs::find(uint16_t trapWord) const
{
	uint16_t key = maskTrapWord(trapWord);
	auto it = defs_.find(key);
	if (it != defs_.end()) return &it->second;

	/* Fallback: OS trap with bit 9 set but no dedicated entry?
	   Try again with bit 9 cleared (e.g. $A207 → $A007 GetVolInfo). */
	if (!(key & 0x0800) && (key & 0x0200))
	{
		uint16_t base = key & ~0x0200;
		it = defs_.find(base);
		if (it != defs_.end()) return &it->second;
	}
	return nullptr;
}

const SubtrapDef *TrapDefs::findSubtrap(uint16_t parentTrapWord, uint16_t selector) const
{
	uint16_t key = maskTrapWord(parentTrapWord);
	auto it = subtraps_.find(key);
	if (it == subtraps_.end()) return nullptr;
	auto sit = it->second.find(selector);
	if (sit == it->second.end()) return nullptr;
	return &sit->second;
}

const char *TrapDefs::errorName(int16_t code) const
{
	auto it = errors_.find(code);
	if (it != errors_.end()) return it->second.c_str();
	return nullptr;
}

/* ── Name/search API ──────────────────────────────────── */

int TrapDefs::size() const
{
	return static_cast<int>(sortedNames_.size());
}

std::pair<uint32_t, std::string_view> TrapDefs::entry(int index) const
{
	return {sortedNames_[index].first, sortedNames_[index].second};
}

std::string_view TrapDefs::nameOf(uint16_t trapWord) const
{
	const TrapDef *def = find(trapWord);
	if (def) return def->name;
	return {};
}

static inline bool ciStartsWith(std::string_view str, std::string_view prefix)
{
	if (str.size() < prefix.size()) return false;
	for (size_t i = 0; i < prefix.size(); ++i)
	{
		if (std::tolower(static_cast<unsigned char>(str[i])) !=
			std::tolower(static_cast<unsigned char>(prefix[i])))
			return false;
	}
	return true;
}

void TrapDefs::search(std::string_view prefix,
					  std::vector<std::pair<uint32_t, std::string_view>> &results,
					  int maxResults) const
{
	results.clear();
	if (prefix.empty()) return;
	for (auto &[tw, name] : sortedNames_)
	{
		if (ciStartsWith(name, prefix))
		{
			results.push_back({tw, name});
			if (static_cast<int>(results.size()) >= maxResults) break;
		}
	}
}

bool TrapDefs::isDispatch(uint16_t trapWord) const
{
	auto *def = find(trapWord);
	return def && def->dispatch.has_value();
}

const DispatchInfo *TrapDefs::dispatchInfo(uint16_t trapWord) const
{
	auto *def = find(trapWord);
	if (!def || !def->dispatch) return nullptr;
	return &*def->dispatch;
}

std::string_view TrapDefs::nameOfSubtrap(uint32_t syntheticKey) const
{
	uint16_t parent = static_cast<uint16_t>(syntheticKey >> 16);
	uint16_t sel = static_cast<uint16_t>(syntheticKey & 0xFFFF);
	auto *sub = findSubtrap(parent, sel);
	if (sub) return sub->def.name;
	return {};
}
