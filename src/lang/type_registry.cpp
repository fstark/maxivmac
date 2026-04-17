#include "lang/type_registry.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

/* ── Primitive type system ────────────────────────────── */

enum class PrimitiveKind
{
	Byte,
	SByte,
	BooleanK,
	Word,
	SWord,
	Long,
	SLong,
	Ptr,
	Handle,
	ProcPtr,
	OSType,
	OSErr,
	Fixed,
	Fract,
	Str255,
	Str63,
	Str31,
	PStr,
};

struct PrimitiveInfo
{
	std::string_view name;
	PrimitiveKind kind;
	uint16_t size;
};

static constexpr PrimitiveInfo kPrimitives[] = {
	{"byte", PrimitiveKind::Byte, 1},		 {"sbyte", PrimitiveKind::SByte, 1},
	{"Boolean", PrimitiveKind::BooleanK, 1}, {"word", PrimitiveKind::Word, 2},
	{"sword", PrimitiveKind::SWord, 2},		 {"long", PrimitiveKind::Long, 4},
	{"slong", PrimitiveKind::SLong, 4},		 {"Ptr", PrimitiveKind::Ptr, 4},
	{"Handle", PrimitiveKind::Handle, 4},	 {"ProcPtr", PrimitiveKind::ProcPtr, 4},
	{"OSType", PrimitiveKind::OSType, 4},	 {"OSErr", PrimitiveKind::OSErr, 2},
	{"Fixed", PrimitiveKind::Fixed, 4},		 {"Fract", PrimitiveKind::Fract, 4},
	{"Str255", PrimitiveKind::Str255, 256},	 {"Str63", PrimitiveKind::Str63, 64},
	{"Str31", PrimitiveKind::Str31, 32},	 {"PStr", PrimitiveKind::PStr, 4},
};

[[maybe_unused]]
static const PrimitiveInfo *FindPrimitive(std::string_view name)
{
	for (auto &p : kPrimitives)
	{
		if (p.name == name) return &p;
	}
	return nullptr;
}

/* ── FormatFourCC ─────────────────────────────────────── */

[[maybe_unused]]
static std::string FormatFourCC(uint32_t val)
{
	char buf[12];
	char c[4];
	c[0] = static_cast<char>((val >> 24) & 0xFF);
	c[1] = static_cast<char>((val >> 16) & 0xFF);
	c[2] = static_cast<char>((val >> 8) & 0xFF);
	c[3] = static_cast<char>(val & 0xFF);

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
		std::snprintf(buf, sizeof(buf), "'%c%c%c%c'", c[0], c[1], c[2], c[3]);
	else
		std::snprintf(buf, sizeof(buf), "$%08X", val);
	return buf;
}

/* ── Internal data structures ─────────────────────────── */

struct TypeRegistry::FieldDef
{
	uint16_t offset;
	std::string typeName;
	std::string fieldName;
	uint16_t arrayCount = 1;
};

struct TypeRegistry::StructDef
{
	std::string name;
	std::vector<FieldDef> fields;
};

struct TypeRegistry::UnionDef
{
	std::string name;
	std::vector<std::pair<std::string, std::string>> arms;
};

struct TypeRegistry::TypeEntry
{
	std::string name;
	bool isUnion = false;
	StructDef structDef;
	UnionDef unionDef;
	mutable uint16_t cachedSize = 0;
};

/* ── Parser helpers ───────────────────────────────────── */

static bool BlankOrComment(const std::string &line)
{
	for (char c : line)
	{
		if (c == '#') return true;
		if (!std::isspace(static_cast<unsigned char>(c))) return false;
	}
	return true;
}

static std::string StripComment(const std::string &line)
{
	auto pos = line.find('#');
	if (pos == std::string::npos) return line;
	return line.substr(0, pos);
}

static bool ParseFieldLine(const std::string &raw, uint16_t &outOffset, std::string &outType,
						   std::string &outField, uint16_t &outArray, const std::string &file,
						   int lineNo)
{
	std::string line = StripComment(raw);
	std::istringstream iss(line);
	int offset;
	std::string typeName;
	std::string fieldName;
	if (!(iss >> offset >> typeName >> fieldName)) return false;

	if (offset < 0 || offset > 65535)
	{
		std::fprintf(stderr, "%s:%d: offset %d out of range\n", file.c_str(), lineNo, offset);
		return false;
	}

	outOffset = static_cast<uint16_t>(offset);
	outArray = 1;

	/* Check for array suffix on the type: type[N] */
	auto bracket = typeName.find('[');
	if (bracket != std::string::npos)
	{
		auto end = typeName.find(']', bracket);
		if (end != std::string::npos)
		{
			int n = std::atoi(typeName.c_str() + bracket + 1);
			if (n > 0) outArray = static_cast<uint16_t>(n);
			typeName = typeName.substr(0, bracket);
		}
	}

	/* Check for array suffix on the field name: name[N] */
	bracket = fieldName.find('[');
	if (bracket != std::string::npos)
	{
		auto end = fieldName.find(']', bracket);
		if (end != std::string::npos)
		{
			int n = std::atoi(fieldName.c_str() + bracket + 1);
			if (n > 0) outArray = static_cast<uint16_t>(n);
			fieldName = fieldName.substr(0, bracket);
		}
	}

	outType = std::move(typeName);
	outField = std::move(fieldName);
	return true;
}

/* ── Singleton ────────────────────────────────────────── */

TypeRegistry &g_typeRegistry()
{
	static TypeRegistry s_instance;
	return s_instance;
}

/* ── Special members (TypeEntry must be complete) ─────── */

TypeRegistry::TypeRegistry() = default;
TypeRegistry::~TypeRegistry() = default;
TypeRegistry::TypeRegistry(TypeRegistry &&) noexcept = default;
TypeRegistry &TypeRegistry::operator=(TypeRegistry &&) noexcept = default;

/* ── Public method stubs ──────────────────────────────── */

void TypeRegistry::init(MemReader reader)
{
	mem_ = reader;
}

int TypeRegistry::load(const std::filesystem::path &path)
{
	std::ifstream f(path);
	if (!f.is_open()) return 0;

	std::string fileName = path.filename().string();
	int count = 0;
	int lineNo = 0;
	std::string line;

	enum class State
	{
		Idle,
		Struct,
		Union
	};
	State state = State::Idle;
	StructDef currentStruct;
	UnionDef currentUnion;

	auto finishStruct = [&]()
	{
		TypeEntry te;
		te.name = currentStruct.name;
		te.isUnion = false;
		te.structDef = std::move(currentStruct);

		/* Check for duplicate, overwrite */
		bool replaced = false;
		for (auto &existing : types_)
		{
			if (existing.name == te.name)
			{
				std::fprintf(stderr, "%s:%d: duplicate type '%s', overwriting\n", fileName.c_str(),
							 lineNo, te.name.c_str());
				existing = std::move(te);
				replaced = true;
				break;
			}
		}
		if (!replaced) types_.push_back(std::move(te));
		++count;
		currentStruct = StructDef{};
	};

	auto finishUnion = [&]()
	{
		TypeEntry te;
		te.name = currentUnion.name;
		te.isUnion = true;
		te.unionDef = std::move(currentUnion);

		bool replaced = false;
		for (auto &existing : types_)
		{
			if (existing.name == te.name)
			{
				std::fprintf(stderr, "%s:%d: duplicate type '%s', overwriting\n", fileName.c_str(),
							 lineNo, te.name.c_str());
				existing = std::move(te);
				replaced = true;
				break;
			}
		}
		if (!replaced) types_.push_back(std::move(te));
		++count;
		currentUnion = UnionDef{};
	};

	while (std::getline(f, line))
	{
		++lineNo;
		if (BlankOrComment(line)) continue;

		if (state == State::Idle)
		{
			std::istringstream iss(line);
			std::string keyword, name, brace;
			iss >> keyword >> name >> brace;

			if (keyword == "struct" && brace == "{")
			{
				currentStruct = StructDef{};
				currentStruct.name = name;
				state = State::Struct;
			}
			else if (keyword == "union" && brace == "{")
			{
				currentUnion = UnionDef{};
				currentUnion.name = name;
				state = State::Union;
			}
			else
			{
				std::fprintf(stderr, "%s:%d: unexpected line in idle state\n", fileName.c_str(),
							 lineNo);
			}
		}
		else if (state == State::Struct)
		{
			std::string trimmed = line;
			/* Trim leading whitespace */
			auto start = trimmed.find_first_not_of(" \t");
			if (start != std::string::npos) trimmed = trimmed.substr(start);

			if (trimmed[0] == '}')
			{
				finishStruct();
				state = State::Idle;
				continue;
			}

			uint16_t offset;
			std::string typeName, fieldName;
			uint16_t arrayCount;
			if (!ParseFieldLine(line, offset, typeName, fieldName, arrayCount, fileName, lineNo))
			{
				std::fprintf(stderr, "%s:%d: bad field line\n", fileName.c_str(), lineNo);
				continue;
			}

			/* Validate non-decreasing offsets */
			if (!currentStruct.fields.empty())
			{
				auto &prev = currentStruct.fields.back();
				const auto *prevPrim = FindPrimitive(prev.typeName);
				uint16_t prevSize = prevPrim ? prevPrim->size : 0;
				if (!prevPrim)
				{
					/* Check user-defined type */
					const auto *prevType = findType(prev.typeName);
					if (prevType) prevSize = computeSize(*prevType);
				}
				uint16_t prevEnd = prev.offset + prevSize * prev.arrayCount;
				if (offset < prevEnd)
				{
					std::fprintf(stderr,
								 "%s:%d: overlapping field at offset %d (previous ends at %d)\n",
								 fileName.c_str(), lineNo, offset, prevEnd);
					continue;
				}
			}

			FieldDef fd;
			fd.offset = offset;
			fd.typeName = std::move(typeName);
			fd.fieldName = std::move(fieldName);
			fd.arrayCount = arrayCount;
			currentStruct.fields.push_back(std::move(fd));
		}
		else if (state == State::Union)
		{
			std::string trimmed = line;
			auto start = trimmed.find_first_not_of(" \t");
			if (start != std::string::npos) trimmed = trimmed.substr(start);

			if (trimmed[0] == '}')
			{
				finishUnion();
				state = State::Idle;
				continue;
			}

			std::string stripped = StripComment(line);
			std::istringstream iss(stripped);
			std::string tag, typeName;
			if (!(iss >> tag >> typeName))
			{
				std::fprintf(stderr, "%s:%d: bad union arm line\n", fileName.c_str(), lineNo);
				continue;
			}
			currentUnion.arms.emplace_back(std::move(tag), std::move(typeName));
		}
	}

	/* Missing closing brace */
	if (state != State::Idle)
	{
		std::fprintf(stderr, "%s:%d: missing '}' at end of file\n", fileName.c_str(), lineNo);
	}

	return count;
}

int TypeRegistry::loadErrors(const std::filesystem::path &path)
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

bool TypeRegistry::has(std::string_view typeName) const
{
	return findType(typeName) != nullptr || FindPrimitive(typeName) != nullptr;
}

uint16_t TypeRegistry::sizeOf(std::string_view typeName) const
{
	const auto *te = findType(typeName);
	if (te) return computeSize(*te);
	const auto *prim = FindPrimitive(typeName);
	if (prim) return prim->size;
	return 0;
}

std::vector<FieldValue> TypeRegistry::read(std::string_view typeName, uint32_t addr,
										   std::string_view variant) const
{
	const auto *te = findType(typeName);
	if (!te) return {};

	const StructDef *sd = nullptr;

	if (te->isUnion)
	{
		/* Find matching variant arm */
		std::string_view armType;
		for (auto &[tag, tn] : te->unionDef.arms)
		{
			if (tag == variant || variant.empty())
			{
				armType = tn;
				break;
			}
		}
		if (armType.empty()) return {};
		const auto *armEntry = findType(armType);
		if (!armEntry || armEntry->isUnion) return {};
		sd = &armEntry->structDef;
	}
	else
	{
		sd = &te->structDef;
	}

	std::vector<FieldValue> out;
	readStruct(*sd, addr, addr, "", out);
	return out;
}

std::string TypeRegistry::format(std::string_view typeName, uint32_t addr,
								 std::string_view variant) const
{
	auto fields = read(typeName, addr, variant);
	if (fields.empty()) return {};

	size_t maxNameLen = 0;
	for (auto &f : fields)
	{
		if (f.name.size() > maxNameLen) maxNameLen = f.name.size();
	}
	if (maxNameLen > 30) maxNameLen = 30;

	std::string result;
	for (auto &f : fields)
	{
		result += "  ";
		std::string label = f.name + ":";
		result += label;
		size_t padTo = maxNameLen + 3; /* name + ":" + at least 1 space */
		if (label.size() < padTo)
			result.append(padTo - label.size(), ' ');
		else
			result += ' ';
		result += f.display;
		result += '\n';
	}
	return result;
}

std::string TypeRegistry::formatFiltered(std::string_view typeName, uint32_t addr,
										 const std::vector<std::string> &fields,
										 std::string_view variant) const
{
	auto allFields = read(typeName, addr, variant);
	if (allFields.empty()) return {};

	/* Keep only fields whose suffix (after the last '.') matches the filter list. */
	std::vector<FieldValue *> keep;
	for (auto &f : allFields)
	{
		/* Nested fields have prefixed names like "header.ioResult".
		   Match against the leaf name. */
		std::string_view leaf = f.name;
		auto dot = leaf.rfind('.');
		if (dot != std::string_view::npos) leaf = leaf.substr(dot + 1);

		for (auto &want : fields)
		{
			if (leaf == want)
			{
				keep.push_back(&f);
				break;
			}
		}
	}
	if (keep.empty()) return {};

	size_t maxNameLen = 0;
	for (auto *f : keep)
	{
		if (f->name.size() > maxNameLen) maxNameLen = f->name.size();
	}
	if (maxNameLen > 30) maxNameLen = 30;

	std::string result;
	for (auto *f : keep)
	{
		result += "  ";
		std::string label = f->name + ":";
		result += label;
		size_t padTo = maxNameLen + 3;
		if (label.size() < padTo)
			result.append(padTo - label.size(), ' ');
		else
			result += ' ';
		result += f->display;
		result += '\n';
	}
	return result;
}

std::string TypeRegistry::readField(std::string_view typeName, uint32_t addr,
									std::string_view fieldPath, std::string_view variant) const
{
	auto fields = read(typeName, addr, variant);
	for (auto &f : fields)
	{
		if (f.name == fieldPath) return f.display;
	}
	return {};
}

std::vector<TypeRegistry::TypeInfo> TypeRegistry::typeNames() const
{
	std::vector<TypeInfo> out;
	out.reserve(types_.size());
	for (auto &te : types_)
		out.push_back({te.name, te.isUnion, computeSize(te)});
	return out;
}

const TypeRegistry::TypeEntry *TypeRegistry::findType(std::string_view name) const
{
	for (auto &te : types_)
	{
		if (te.name == name) return &te;
	}
	return nullptr;
}

void TypeRegistry::readStruct(const StructDef &sd, uint32_t baseAddr, uint32_t origBase,
							  std::string_view prefix, std::vector<FieldValue> &out) const
{
	for (auto &field : sd.fields)
	{
		const auto *prim = FindPrimitive(field.typeName);
		uint16_t elemSize = 0;
		if (prim)
		{
			elemSize = prim->size;
		}
		else
		{
			const auto *inner = findType(field.typeName);
			if (inner) elemSize = computeSize(*inner);
		}

		for (uint16_t i = 0; i < field.arrayCount; ++i)
		{
			uint32_t fieldAddr = baseAddr + field.offset + i * elemSize;
			std::string name(prefix);
			name += field.fieldName;
			if (field.arrayCount > 1)
			{
				name += '[';
				name += std::to_string(i);
				name += ']';
			}

			if (prim)
			{
				std::string display = formatPrimitive(field.typeName, fieldAddr);
				out.push_back({name, fieldAddr - origBase, prim->size, std::move(display)});
			}
			else
			{
				const auto *inner = findType(field.typeName);
				if (inner && !inner->isUnion)
				{
					readStruct(inner->structDef, fieldAddr, origBase, name + ".", out);
				}
			}
		}
	}
}

std::string TypeRegistry::formatPrimitive(std::string_view typeName, uint32_t addr) const
{
	const auto *prim = FindPrimitive(typeName);
	if (!prim) return {};

	char buf[280];

	switch (prim->kind)
	{
		case PrimitiveKind::Byte:
			std::snprintf(buf, sizeof(buf), "$%02X", mem_.readByte(addr));
			break;
		case PrimitiveKind::SByte:
			std::snprintf(buf, sizeof(buf), "%d", static_cast<int8_t>(mem_.readByte(addr)));
			break;
		case PrimitiveKind::BooleanK:
			return mem_.readByte(addr) ? "true" : "false";
		case PrimitiveKind::Word:
			std::snprintf(buf, sizeof(buf), "$%04X", mem_.readWord(addr));
			break;
		case PrimitiveKind::SWord:
			std::snprintf(buf, sizeof(buf), "%d", static_cast<int16_t>(mem_.readWord(addr)));
			break;
		case PrimitiveKind::Long:
			std::snprintf(buf, sizeof(buf), "$%08X", mem_.readLong(addr));
			break;
		case PrimitiveKind::SLong:
			std::snprintf(buf, sizeof(buf), "%d", static_cast<int32_t>(mem_.readLong(addr)));
			break;
		case PrimitiveKind::Ptr:
		case PrimitiveKind::Handle:
		case PrimitiveKind::ProcPtr:
			std::snprintf(buf, sizeof(buf), "$%08X", mem_.readLong(addr));
			break;
		case PrimitiveKind::OSType:
			return FormatFourCC(mem_.readLong(addr));
		case PrimitiveKind::OSErr:
		{
			auto val = static_cast<int16_t>(mem_.readWord(addr));
			auto it = errors_.find(val);
			if (it != errors_.end())
				std::snprintf(buf, sizeof(buf), "%d %s", val, it->second.c_str());
			else
				std::snprintf(buf, sizeof(buf), "%d", val);
			break;
		}
		case PrimitiveKind::Fixed:
		{
			auto raw = static_cast<int32_t>(mem_.readLong(addr));
			int intPart = raw >> 16;
			int fracPart = ((raw & 0xFFFF) * 10000) >> 16;
			if (fracPart < 0) fracPart = -fracPart;
			std::snprintf(buf, sizeof(buf), "%d.%04d", intPart, fracPart);
			break;
		}
		case PrimitiveKind::Fract:
		{
			auto raw = static_cast<int32_t>(mem_.readLong(addr));
			int intPart = raw >> 30;
			int fracPart = ((raw & 0x3FFFFFFF) * 10000) >> 30;
			if (fracPart < 0) fracPart = -fracPart;
			std::snprintf(buf, sizeof(buf), "%d.%04d", intPart, fracPart);
			break;
		}
		case PrimitiveKind::PStr:
		{
			uint32_t ptr = mem_.readLong(addr);
			if (ptr == 0) return "$00000000";
			uint8_t len = mem_.readByte(ptr);
			if (len > 255) len = 255;
			std::string result;
			char hdr[16];
			std::snprintf(hdr, sizeof(hdr), "$%08X \"", ptr);
			result = hdr;
			for (uint8_t i = 0; i < len; ++i)
			{
				uint8_t c = mem_.readByte(ptr + 1 + i);
				if (c >= 0x20 && c < 0x7F)
					result += static_cast<char>(c);
				else
				{
					char esc[5];
					std::snprintf(esc, sizeof(esc), "\\x%02X", c);
					result += esc;
				}
			}
			result += '"';
			return result;
		}
		case PrimitiveKind::Str255:
		case PrimitiveKind::Str63:
		case PrimitiveKind::Str31:
		{
			uint8_t len = mem_.readByte(addr);
			uint8_t maxLen = (prim->kind == PrimitiveKind::Str255)	? 255
							 : (prim->kind == PrimitiveKind::Str63) ? 63
																	: 31;
			if (len > maxLen) len = maxLen;
			std::string result = "\"";
			for (uint8_t i = 0; i < len; ++i)
			{
				uint8_t c = mem_.readByte(addr + 1 + i);
				if (c >= 0x20 && c < 0x7F)
					result += static_cast<char>(c);
				else
				{
					char esc[5];
					std::snprintf(esc, sizeof(esc), "\\x%02X", c);
					result += esc;
				}
			}
			result += '"';
			return result;
		}
	}

	return buf;
}

uint16_t TypeRegistry::computeSize(const TypeEntry &te) const
{
	if (te.cachedSize > 0) return te.cachedSize;

	if (te.isUnion)
	{
		uint16_t maxSize = 0;
		for (auto &[tag, tn] : te.unionDef.arms)
		{
			const auto *arm = findType(tn);
			if (arm)
			{
				uint16_t s = computeSize(*arm);
				if (s > maxSize) maxSize = s;
			}
		}
		te.cachedSize = maxSize;
	}
	else
	{
		if (te.structDef.fields.empty())
		{
			te.cachedSize = 0;
		}
		else
		{
			auto &last = te.structDef.fields.back();
			const auto *prim = FindPrimitive(last.typeName);
			uint16_t elemSize = 0;
			if (prim)
			{
				elemSize = prim->size;
			}
			else
			{
				const auto *inner = findType(last.typeName);
				if (inner) elemSize = computeSize(*inner);
			}
			te.cachedSize = last.offset + elemSize * last.arrayCount;
		}
	}
	return te.cachedSize;
}
